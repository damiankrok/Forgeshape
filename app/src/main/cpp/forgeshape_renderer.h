// ForgeShape native Vulkan renderer.
//
// Owns exactly what is needed to present the scene snapshot it is handed -- the
// bodies' derived render meshes, the grid, the gizmo and the sketch overlay --
// into an Android Surface, and no geometry truth. Deliberately NOT a generic
// engine: one device, one swapchain, a fixed set of pipelines.
#pragma once

// Required before <vulkan/vulkan.h> to expose VkAndroidSurfaceCreateInfoKHR
// and vkCreateAndroidSurfaceKHR.
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_display.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_grid.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_render_recovery.h"
#include "forgeshape_scene.h"
#include "forgeshape_selection_outline.h"
#include "forgeshape_selection_pulse.h"
#include "forgeshape_sketch_overlay.h"

namespace forgeshape {

// Everything the GPU holds for ONE Construction Body.
//
// Before Stage 017 these were flat Renderer members, because there was exactly
// one object. Making them per body is what keeps bodies independent: each has
// its own device-local buffers, its own derived-geometry cache and its own
// record of which revision and shading it currently holds, so a rebuild or an
// upload for one body cannot be triggered by, or disturb, another.
struct BodyRenderResources {
    // Steady-state mesh geometry: DEVICE_LOCAL, written only through the
    // renderer's shared host-visible staging buffer. Device-scoped, so a
    // Surface swap never touches them and the uploaded revision survives
    // home/resume.
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkDeviceSize vertexCapacityBytes = 0;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    VkDeviceSize indexCapacityBytes = 0;
    uint32_t indexCount = 0;

    // The render-only derived geometry (positions + NORMALS + colour) the
    // buffers above actually hold. The authoritative RuntimeMesh in this body's
    // MeshStore is NOT what gets uploaded: this is derived from it, and its
    // vertex count generally differs because a hard edge needs one render
    // vertex per crease group. Picking still runs on the source.
    RenderMeshCache renderMesh;

    // Which authoritative revision, and which surface shading, this body's
    // buffers currently hold. Both must match for an upload to be skipped.
    MeshRevision uploadedRevision = kNoMeshRevision;
    SurfaceShading uploadedShading = kDefaultSurfaceShading;
    MeshRevision failedRevision = kNoMeshRevision;  // do not retry in a loop

    // How this body's selection is currently being DRAWN — the acknowledgement
    // pulse and the resting tint that follows it.
    //
    // It lives here, beside the GPU state, because it is per body and keyed by
    // the same stable ObjectId, so body A's pulse cannot be interrupted or
    // restarted by anything that happens to body B. It is presentation and
    // nothing else: no revision, no buffer, no upload and no rebuild can be
    // caused by it, and the pulse advancing changes exactly one float in the
    // push constants that were going to be written for this draw anyway.
    SelectionPulseState selectionPulse;
    float selectionAlpha = 0.0f;  // this frame's answer, written by syncScene
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Installs the camera state used by the next recorded frame. The renderer
    // consumes the snapshot verbatim; it never interprets input or derives
    // camera pose itself.
    void setCamera(const CameraSnapshot& camera) { camera_ = camera; }

    // Installs the immutable scene snapshot the next frame draws.
    //
    // Each item carries its own ObjectId, its own published mesh, its own
    // DERIVED model transform and its own selection flag. The renderer consumes
    // all of it verbatim: it owns no position, no rotation, no Euler
    // convention, no geometry and no selection identity, and it never builds a
    // model matrix or decides what is selected.
    //
    // The snapshot holds `shared_ptr`s to published revisions, so a mesh it
    // names stays alive for as long as the renderer holds it even if newer
    // revisions are published meanwhile.
    void setScene(SceneSnapshot scene) { scene_ = std::move(scene); }

    // Installs the display settings used by the next recorded frame, consumed
    // verbatim exactly like the camera snapshot. The renderer owns no
    // presentation preference: DisplaySettingsStore does, and the viewport
    // thread pushes a snapshot in before each frame.
    //
    // Only the Smooth/Faceted choice can cause any work beyond a uniform
    // change, and even that is confined to the render-only derived mesh.
    void setDisplaySettings(const ViewportDisplaySettings& settings) { display_ = settings; }

    // Installs the Construction gizmo state the next frame draws, consumed
    // verbatim exactly like the camera and the display settings.
    //
    // A GizmoSnapshot carries a pivot, a scale, a mode and which handle is
    // held — and deliberately no ObjectId, no dimension and no primitive
    // parameter. The renderer cannot learn WHICH body it is drawing a handle
    // for, which is what keeps a tool overlay from becoming a second route by
    // which the render layer knows about identity.
    void setGizmo(const GizmoSnapshot& gizmo) { gizmo_ = gizmo; }

    // Installs the sketch overlay the next frame draws (`CAD-R0-A1A2`): a
    // world-space line list with a revision, consumed verbatim like the gizmo.
    // Null or empty means no sketch is in progress and nothing is drawn. The
    // renderer re-uploads only when the revision changes, so a frame with no
    // sketch change costs no transfer -- and it cannot learn a sketch
    // coordinate from it, because the vertices are already world positions.
    void setSketchOverlay(SketchOverlayPtr overlay) { sketchOverlay_ = std::move(overlay); }

    // Device-independent setup: Vulkan instance only.
    bool createInstance();
    void destroyInstance();

    // Binds the renderer to a live ANativeWindow. Creates (on first call) the
    // device, buffers and pipeline layout, then the surface-dependent objects.
    bool attachSurface(ANativeWindow* window);

    // Releases every object that references the ANativeWindow. Safe to call
    // when no surface is attached.
    void detachSurface();

    bool hasSurface() const { return surface_ != VK_NULL_HANDLE; }

    void requestResize() { needsSwapchainRebuild_ = true; }

    // Renders and presents one frame.
    //
    // Returns false only when the renderer has STOPPED for good — see
    // `lifecycle()`. A lost device does not return false: it is recovered from
    // in place, and the frame loop keeps its ownership of the surface
    // throughout.
    bool drawFrame();

    // Where the renderer stands. Read by JNI so the product can tell the user
    // the truth about the viewport, and so a lost device can force an immediate
    // project checkpoint rather than hoping one is due.
    RendererLifecycle lifecycle() const { return recovery_.state(); }
    int deviceRebuildAttempts() const { return recovery_.deviceRebuildAttempts(); }
    int deviceRebuildsCompleted() const { return recovery_.deviceRebuildsCompleted(); }
    // Images actually handed to the presentation engine by this renderer. A
    // diagnostic count and nothing else: it lets a test wait for frames that
    // really reached the display instead of guessing a delay, which on a
    // software rasteriser with a four-image FIFO swapchain is seconds.
    uint64_t framesPresented() const { return frameIndex_; }

#ifndef NDEBUG
    // DEBUG-ONLY: makes the NEXT frame behave exactly as though the device had
    // been lost, without asking the driver to lose one.
    //
    // Deliberately an injection rather than a real fault. Provoking a genuine
    // `VK_ERROR_DEVICE_LOST` means destabilising the GPU of the authoritative
    // emulator, which the repository forbids and which would make the test
    // depend on driver behaviour rather than on ForgeShape's. What is under
    // test is what ForgeShape DOES about a lost device, and this reaches every
    // line of that.
    void injectDeviceLossForTest() { injectDeviceLossOnce_ = true; }
#endif

    // Bounded selection-outline diagnostics (`SEL-OUT-R1` §12/§13). Plain
    // counters written by the render thread and read by JNI, exactly as the
    // device-rebuild count is: they exist so "no GPU allocation leak over
    // repeated selection switches" and "no body upload on a selection change"
    // can be ASSERTED from a test rather than inferred from a screenshot.
    //
    // They describe the renderer's own derived resources and carry no identity:
    // there is no ObjectId here, no geometry, no dimension and nothing a
    // diagnostic could leak a model through.
    struct SelectionOutlineStats {
        // How many times the extent-sized mask + depth images have been
        // ALLOCATED for the life of the process. It must move only when the
        // swapchain extent changes or the device is rebuilt, and never once per
        // selection switch — which is the whole point of measuring it.
        uint64_t maskAllocations = 0;
        // Frames in which the mask pass and the composite draw were actually
        // recorded. Zero while nothing is selected or the toggle is off.
        uint64_t maskPassFrames = 0;
        uint64_t compositeDraws = 0;
        // The mask's current extent, so a test can prove it follows the render
        // extent rather than being a fixed-size buffer that is stretched.
        uint32_t maskWidth = 0;
        uint32_t maskHeight = 0;
        // The band's half-width in screen pixels the last recorded composite
        // used. Reported so the visual evidence can quote a measured value.
        float widthPixels = 0.0f;
    };
    SelectionOutlineStats selectionOutlineStats() const;

private:
    bool pickPhysicalDeviceAndQueues();
    bool createLogicalDevice();
    bool createShaderModules();
    bool createSyncObjects();
    bool createCommandPool();

    bool createSwapchainDependents();
    void destroySwapchainDependents();

    // Destroys every DEVICE-scoped object and the device itself, leaving the
    // instance alone.
    //
    // Extracted from destroyInstance, which is still its main caller: process
    // teardown destroys the device and then the instance, and device-loss
    // recovery destroys the device and then builds a new one under the same
    // instance. Having one implementation of "everything that hangs off the
    // device" is what makes the second path safe to add — a recovery that
    // forgot one pipeline would leak it on every rebuild.
    void destroyDeviceScopedResources();

    // Rebuilds the device and everything on it, then re-attaches the surface.
    //
    // The CPU project is not consulted and not touched: what is rebuilt is the
    // GPU's copy of derived data, and `syncScene` re-uploads it from the
    // published `RuntimeMesh` revisions on the next frame because
    // destroyMeshResources cleared the per-body upload record.
    //
    // The ANativeWindow reference is acquired before the teardown and handed to
    // attachSurface afterwards, so the renderer never has to ask the Android
    // layer for a surface it already owns.
    bool rebuildDeviceAfterLoss();

    // Classifies one Vulkan result and acts on it. Returns false only when the
    // renderer has entered its terminal state.
    bool handleFrameResult(int vkResultCode, const char* where);

    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createFramebuffers();
    bool createPipeline();
    bool createGridPipeline();
    bool createCommandBuffers();

    bool recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);

    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VkBuffer* outBuffer,
                      VkDeviceMemory* outMemory);
    bool findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties,
                        uint32_t* outIndex) const;
    VkFormat selectDepthFormat() const;

    // --- dynamic mesh upload (renderer-owned, render thread only) ------------
    //
    // Every Vulkan buffer create/copy/destroy for mesh geometry happens here,
    // on the render thread. The CPU mesh is published by someone else; this
    // side only observes the newest revision and mirrors it onto the GPU.
    bool createMeshUploadObjects();
    void destroyMeshResources();

    // --- MatCap sampling resources (renderer-owned, render thread only) ------
    //
    // One of the two sampled images in ForgeShape, and therefore one of the two
    // reasons a descriptor set exists at all — the selection outline's mask is
    // the other, and everything else the pipelines need still travels as push
    // constants. The image content is generated on the CPU by
    // forgeshape_matcap.cpp; this side only uploads and binds it.
    //
    // Device-scoped, like the mesh buffers: created once with the device and
    // untouched by a Surface swap, so a HOME/resume does not regenerate or
    // re-upload the MatCap.
    bool createDescriptorResources();
    bool createMatCapResources();
    void destroyMatCapResources();

    // --- world reference grid (renderer-owned, render thread only) -----------
    //
    // The grid's vertices are a compile-time constant of forgeshape_grid.h, so
    // they are generated and uploaded EXACTLY ONCE, with the device, and never
    // again: there is no parameter a user can move, no revision to follow and
    // nothing for a camera move or a theme switch to invalidate. Showing or
    // hiding the grid therefore decides only whether one draw call is recorded.
    //
    // Device-scoped like the mesh buffers and the MatCap, so a HOME/resume does
    // not regenerate it. It deliberately does NOT go through
    // BodyRenderResources: the grid has no ObjectId, and giving it one would be
    // the first step towards it becoming a scene object, which it must never be.
    bool createGridResources();
    void destroyGridResources();
    // Records the grid's push constants and its one line draw. Called after
    // every body, so the model has already written depth and the grid — which
    // is depth-tested, depth-biased away and does not write depth — can never
    // punch through it.
    void recordGridDraw(VkCommandBuffer cmd);

    // --- Construction Move / Rotate gizmo (renderer-owned, render thread) ----
    //
    // Same shape as the grid, and for the same reasons: the gizmo geometry is a
    // compile-time constant of forgeshape_gizmo.h authored in a canonical
    // reference-unit space, so it is generated and uploaded EXACTLY ONCE with
    // the device. Where the gizmo IS and how large it is on screen are a matrix
    // this frame, never a buffer rewrite — so a drag, an orbit and a dolly all
    // re-upload nothing at all.
    //
    // It has no ObjectId and never goes through BodyRenderResources: a handle is
    // a tool, not a Construction Body, and it must never become one.
    bool createGizmoResources();
    void destroyGizmoResources();
    bool createGizmoPipeline();
    // Re-uploads the canonical list when — and only when — the stroke weight
    // preference (UI-PREF-R1 F) differs from the list the device holds. The
    // one buffer is sized to the widest weight, so this is a copy and never a
    // reallocation; a visual-size change touches nothing here, being a matrix.
    bool syncGizmoGeometry();
    bool uploadGizmoGeometry(GizmoStrokeWeight weight);
    // Records the gizmo last of all, on its own pipeline, with depth testing
    // OFF so a handle is reachable even where it lies inside the body it moves.
    // It writes no depth either, so it leaves the buffer exactly as the bodies
    // and the grid left it and nothing drawn after it could be occluded by it.
    void recordGizmoDraw(VkCommandBuffer cmd);

    // --- Sketch overlay (renderer-owned, render thread) ---------------------
    //
    // Drawn through the gizmo's own line pipeline and shaders -- a sketch is a
    // tool overlay on the gizmo's exact terms: no normal, no light, no
    // descriptor set, depth test off. What differs is that its geometry
    // CHANGES while the user draws, so it has a revision-gated upload of its
    // own rather than a one-time device-creation upload. The upload waits on
    // the renderer's frame fences before writing, exactly as a mesh upload
    // does, so no in-flight frame can be reading the buffer it replaces.
    bool syncSketchOverlay();
    void destroySketchOverlayResources();
    void recordSketchOverlayDraw(VkCommandBuffer cmd);

    // --- Selection outline (renderer-owned, render thread) -------------------
    //
    // `SEL-OUT-R1` / UI-OWNER-10. A TRUE silhouette of the selected body,
    // derived from the geometry the GPU already holds for that body's own draw,
    // in two steps:
    //
    //   1. a MASK pass, recorded before the main pass, that rasterises every
    //      body in the scene through a position-only pipeline into a
    //      single-channel image with its own depth attachment. Unselected
    //      bodies write 0 and their depth; the selected body writes 1. What
    //      survives is therefore exactly the part of the selected body that is
    //      VISIBLE — occlusion is resolved by the depth test, not by a rule;
    //
    //   2. a COMPOSITE draw, recorded inside the main pass after the bodies and
    //      the grid and before the gizmo, that samples the mask through a
    //      full-screen triangle and paints the band around its silhouette.
    //
    // WHY THIS AND NOT A CHEAPER SHAPE. A normal-extruded shell was the other
    // candidate and is wrong for this product: the render mesh splits normals
    // at every hard edge (that is what RenderMeshCache exists for), and a
    // Faceted box therefore has no shared corner normal to extrude along, so
    // the shell opens a gap at every corner of the primitive the product is
    // most often used on. A screen-space mask has no such failure mode and is
    // representation-neutral for free.
    //
    // WHAT IT DELIBERATELY DOES NOT DO. It builds no geometry, extracts no
    // edges on the CPU, holds no ObjectId, keeps no per-body state and reads
    // nothing back. A selection change costs one bool per scene item — which
    // the snapshot already carried — and re-uploads nothing, because the
    // buffers it rasterises are the ones the body was already going to be drawn
    // from.
    //
    // Device-scoped: the shaders, the sampler, the descriptor set layout, the
    // pool, the one set and both pipeline layouts. A Surface swap keeps them.
    bool createOutlineDeviceResources();
    void destroyOutlineDeviceResources();

    // Swapchain-scoped: everything whose size is the render extent — the mask
    // image, the mask pass's own depth image, the render pass, the framebuffer
    // and both pipelines. Recreated only when the extent changes, which is what
    // keeps `maskAllocations` flat across a selection loop.
    //
    // The mask pass gets its OWN depth image rather than sharing the main
    // pass's. Sharing would be smaller by one allocation and would put a
    // write-after-write hazard between the mask pass's depth writes and the
    // main pass's depth CLEAR, which the main render pass's existing external
    // dependency does not cover. Widening a dependency in the one render pass
    // every frame already depends on, to save an image that is freed with the
    // swapchain, is the wrong trade.
    bool createOutlineSwapchainResources();
    void destroyOutlineSwapchainResources();
    bool createOutlineMaskPipeline();
    bool createOutlineCompositePipeline();

    // Whether this frame draws an outline at all. False when the toggle is off,
    // when nothing is selected, when the selected body has no uploaded
    // geometry yet, or when any outline resource is missing — so a frame that
    // cannot draw a correct outline draws none rather than a stale one.
    bool selectionOutlineActive() const;
    // Records the whole mask pass. Called before the main render pass begins,
    // and only when selectionOutlineActive().
    void recordOutlineMaskPass(VkCommandBuffer cmd);
    // Records the composite draw INSIDE the main pass, after the grid and
    // before the gizmo: over the model and the floor, under the instrument.
    void recordSelectionOutlineDraw(VkCommandBuffer cmd);
    // Waits on the renderer's own frame fences (never vkDeviceWaitIdle /
    // vkQueueWaitIdle) so no in-flight frame can still reference the mesh
    // buffers that are about to be overwritten or destroyed.
    // Waits until no in-flight frame still references the mesh buffers.
    //
    // `timeoutNanoseconds` exists for the ONE caller that can afford to be told
    // "not yet": `releaseBodiesAbsentFromScene` is opportunistic and retried on
    // every frame, so it must never stake the render thread on a fence that may
    // never signal. A failed `vkQueueSubmit` leaves that frame's fence reset and
    // nothing signals it afterwards, and an unbounded wait there would hang the
    // render thread -- which `surfaceDestroyed` then blocks on, so the Activity
    // never tears down.
    //
    // A grow, which MUST complete before it reallocates, keeps the default and
    // is bit-for-bit unaffected: with no deadline `VK_TIMEOUT` cannot occur.
    bool waitForMeshBuffersIdle(uint64_t timeoutNanoseconds = UINT64_MAX);
    bool ensureStagingCapacity(VkDeviceSize bytes);
    // Reuses the existing device-local allocation when it is already big
    // enough; grows (and retires the old one) only when it is not.
    bool ensureMeshCapacity(BodyRenderResources& body, VkDeviceSize vertexBytes,
                            VkDeviceSize indexBytes, bool* outGrew);
    // Uploads DERIVED render geometry into one body's buffers. `sourceRevision`
    // is carried through for diagnostics only: it identifies which
    // authoritative revision this render data was derived from, and is what the
    // uploaded-revision check compares.
    bool uploadRenderMesh(BodyRenderResources& body, ObjectId objectId, const RenderMeshData& mesh,
                          MeshRevision sourceRevision);
    // Called once per frame, before recording. For EACH body in the scene,
    // rebuilds the render-only derived mesh and uploads it ONLY when that
    // body's source revision or the surface shading actually changed; on every
    // other frame each body costs two integer comparisons.
    //
    // Per-body state is what makes "editing A does not rebuild or re-upload B"
    // structural rather than a promise: B's cached revision still matches its
    // own published revision, so B's branch returns before touching anything.
    // Frees the GPU copy held for any body the current scene no longer names.
    //
    // Called at the top of syncScene. Until `UI-OWNER-45` this was not needed:
    // a body could only leave the scene by having its creation undone, the
    // history holds at most `kConstructionHistoryCapacity` of those, and each
    // was a handful of kilobytes. Delete removes that bound -- add and delete
    // in a loop and every cycle mints a FRESH ObjectId, because the allocator
    // is deliberately monotonic, so the map would grow one entry per cycle
    // forever. Entries are small, but each holds two VkDeviceMemory
    // allocations, and `maxMemoryAllocationCount` is a hard device limit
    // commonly around 4096.
    //
    // Undoing the delete costs one re-upload and nothing else: the body comes
    // back with the same ObjectId and the same published revision, and a fresh
    // resource record starts at kNoMeshRevision, so syncBody's gate misses and
    // uploads it again. That is exactly the path a body already takes the first
    // time it is drawn.
    //
    // Waits for every in-flight frame before destroying anything, and only when
    // there is something to destroy, so a steady frame pays one map walk.
    void releaseBodiesAbsentFromScene();

    void syncScene();
    // One body's half of syncScene: the per-body gate, then rebuild + upload.
    void syncBody(const SceneDrawItem& item);
    // Advances every body's selection presentation by one frame. Runs beside
    // syncScene and deliberately outside its revision gate: a pulse must keep
    // decaying on frames where no geometry changed, which is all of them.
    void advanceSelectionFeedback(double deltaSeconds);
    // Seconds since the previous frame, clamped. The renderer owns the only
    // clock in ForgeShape's presentation path.
    double consumeFrameDeltaSeconds();
    // Records one body's push constants and its indexed draw.
    void recordBodyDraw(VkCommandBuffer cmd, const SceneDrawItem& item);
    BodyRenderResources& resourcesFor(ObjectId objectId);
    void destroyBodyResources(BodyRenderResources& body);

    // Instance-level
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    uint32_t apiVersion_ = 0;

    // Device-level (persistent across surfaces)
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = UINT32_MAX;
    uint32_t presentQueueFamily_ = UINT32_MAX;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;

    // Bounded, reused staging: one HOST_VISIBLE buffer that carries the vertex
    // block followed by the index block. Grown only when it is too small.
    //
    // Deliberately SHARED across bodies rather than duplicated per body: it is
    // transient scratch used inside one upload and waited on before the next,
    // so one copy is both correct and the smaller footprint.
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
    VkDeviceSize stagingCapacityBytes_ = 0;
    VkCommandBuffer uploadCommandBuffer_ = VK_NULL_HANDLE;
    VkFence uploadFence_ = VK_NULL_HANDLE;

    uint64_t meshBufferGrowCount_ = 0;

    // One entry per Construction Body that has been drawn, keyed by its stable
    // ObjectId — never by scene index, which would silently rebind a body's GPU
    // buffers to a different body if the collection were ever reordered.
    std::unordered_map<ObjectId, BodyRenderResources> bodies_;

    // The scene this frame draws. Immutable for the duration of the frame.
    SceneSnapshot scene_;

    // The MatCap image, its sampler, and the descriptor set that binds them.
    // Device-scoped: a Surface swap does not touch them. The selection
    // outline's mask has its OWN layout, pool, set and sampler below, because
    // this set is written once for the life of the device and that one is
    // rewritten whenever the render extent changes.
    VkImage matcapImage_ = VK_NULL_HANDLE;
    VkDeviceMemory matcapMemory_ = VK_NULL_HANDLE;
    VkImageView matcapImageView_ = VK_NULL_HANDLE;
    VkSampler matcapSampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;

    VkShaderModule vertShader_ = VK_NULL_HANDLE;
    VkShaderModule fragShader_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;

    // The grid's own shaders and layout. A SEPARATE pipeline layout with no
    // descriptor set, rather than a reuse of pipelineLayout_, because the grid
    // genuinely needs no sampler: saying so in the layout is what keeps a
    // future reader from believing the grid consults the MatCap.
    VkShaderModule gridVertShader_ = VK_NULL_HANDLE;
    VkShaderModule gridFragShader_ = VK_NULL_HANDLE;
    VkPipelineLayout gridPipelineLayout_ = VK_NULL_HANDLE;

    // Device-local, written once at device creation and never again. Sized from
    // kGridVertexCount, which is a compile-time constant.
    VkBuffer gridVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory gridVertexMemory_ = VK_NULL_HANDLE;
    uint32_t gridVertexCount_ = 0;

    // The gizmo's own shaders, layout and buffer. A separate layout with no
    // descriptor set, exactly like the grid's: a handle consults no sampler,
    // takes no light and ignores the shading model, and saying so in the layout
    // is what keeps a future reader from believing otherwise.
    VkShaderModule gizmoVertShader_ = VK_NULL_HANDLE;
    VkShaderModule gizmoFragShader_ = VK_NULL_HANDLE;
    VkPipelineLayout gizmoPipelineLayout_ = VK_NULL_HANDLE;
    VkBuffer gizmoVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory gizmoVertexMemory_ = VK_NULL_HANDLE;
    uint32_t gizmoVertexCount_ = 0;
    // Which weight's list the device holds, so the draw reads the ranges of
    // what was actually uploaded and a change is noticed once per frame.
    GizmoStrokeWeight gizmoUploadedWeight_ = kDefaultGizmoStrokeWeight;
    bool gizmoUploadedOnce_ = false;

    // The selection outline's own shaders, layouts and sampler. Device-scoped,
    // like the MatCap's: a Surface swap does not touch them.
    //
    // A SEPARATE descriptor set layout, pool and set from the MatCap's, rather
    // than a second binding added to the existing one. The MatCap set is
    // written once for the life of the device and shared by every body draw;
    // this one is rewritten whenever the swapchain extent changes, because the
    // image view it names is extent-sized. Folding them together would mean
    // re-writing the MatCap binding on every rotation for no reason.
    VkShaderModule outlineMaskVertShader_ = VK_NULL_HANDLE;
    VkShaderModule outlineMaskFragShader_ = VK_NULL_HANDLE;
    VkShaderModule outlineVertShader_ = VK_NULL_HANDLE;
    VkShaderModule outlineFragShader_ = VK_NULL_HANDLE;
    VkPipelineLayout outlineMaskPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout outlinePipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout outlineSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool outlinePool_ = VK_NULL_HANDLE;
    VkDescriptorSet outlineSet_ = VK_NULL_HANDLE;
    VkSampler outlineSampler_ = VK_NULL_HANDLE;

    // The selection outline's extent-sized resources. Swapchain-scoped: created
    // with the swapchain, destroyed with it, and never reallocated for a
    // selection change.
    VkImage outlineMaskImage_ = VK_NULL_HANDLE;
    VkDeviceMemory outlineMaskMemory_ = VK_NULL_HANDLE;
    VkImageView outlineMaskView_ = VK_NULL_HANDLE;
    VkImage outlineMaskDepthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory outlineMaskDepthMemory_ = VK_NULL_HANDLE;
    VkImageView outlineMaskDepthView_ = VK_NULL_HANDLE;
    VkRenderPass outlineMaskPass_ = VK_NULL_HANDLE;
    VkFramebuffer outlineMaskFramebuffer_ = VK_NULL_HANDLE;
    VkPipeline outlineMaskPipeline_ = VK_NULL_HANDLE;
    VkPipeline outlinePipeline_ = VK_NULL_HANDLE;
    VkExtent2D outlineMaskExtent_{0, 0};

    // Bounded diagnostics. See SelectionOutlineStats.
    uint64_t outlineMaskAllocations_ = 0;
    uint64_t outlineMaskPassFrames_ = 0;
    uint64_t outlineCompositeDraws_ = 0;
    float outlineWidthPixels_ = 0.0f;

    // Surface-dependent
    ANativeWindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{0, 0};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> renderFinished_;   // one per swapchain image
    std::vector<VkFence> imagesInFlight_;

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    // Swapchain-dependent exactly like pipeline_, because it bakes the viewport
    // and the render pass in the same way.
    VkPipeline gridPipeline_ = VK_NULL_HANDLE;
    // Swapchain-dependent for the same reason gridPipeline_ is.
    VkPipeline gizmoPipeline_ = VK_NULL_HANDLE;

    // Frames in flight
    static constexpr uint32_t kMaxFramesInFlight = 2;
    VkSemaphore imageAvailable_[kMaxFramesInFlight]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkFence inFlightFences_[kMaxFramesInFlight]{VK_NULL_HANDLE, VK_NULL_HANDLE};
    uint32_t currentFrame_ = 0;

    // Camera pose/projection, produced by CameraController and pushed in by the
    // viewport thread before each frame.
    CameraSnapshot camera_{};

    // Placement and selection are per body and arrive with `scene_`; there is
    // deliberately no renderer-wide model matrix or selection flag. A single
    // global "something is selected" bool would tint EVERY body at once as soon
    // as any one of them was picked.

    // Purely presentation: which shading model to evaluate and whether the
    // derived normals are smoothed or faceted. Owned by DisplaySettingsStore
    // and pushed in per frame; the defaults here only cover the frames before
    // the first snapshot arrives.
    ViewportDisplaySettings display_{};

    // Where the Construction gizmo is, how big it is on screen and which handle
    // is held. Invisible by default, so a frame recorded before anything pushes
    // one draws no handles rather than handles at the origin.
    GizmoSnapshot gizmo_{};

    // The sketch overlay pushed in for this frame, and what the device holds.
    // The buffer is device-local and grows on demand; a revision already
    // uploaded is drawn again with no transfer.
    SketchOverlayPtr sketchOverlay_;
    VkBuffer sketchVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sketchVertexMemory_ = VK_NULL_HANDLE;
    VkDeviceSize sketchVertexCapacityBytes_ = 0;
    uint32_t sketchVertexCount_ = 0;
    uint64_t sketchUploadedRevision_ = 0;
    bool sketchUploadedOnce_ = false;
    std::vector<SketchOverlayRange> sketchRanges_;

    // True when this swapchain deliberately declared a pre-transform the surface
    // does not currently use (the identity-pre-transform orientation
    // convention), which makes VK_SUBOPTIMAL_KHR the expected steady state
    // rather than a rebuild request. Set by createSwapchain().
    bool expectSuboptimal_ = false;

    bool needsSwapchainRebuild_ = false;

    // The device-loss state machine. Render thread only, like every member here.
    RenderRecoveryPolicy recovery_;
#ifndef NDEBUG
    bool injectDeviceLossOnce_ = false;
#endif
    bool presentedThisSession_ = false;
    uint64_t frameIndex_ = 0;

    // When the previous frame measured its delta. Zero until the first frame,
    // which therefore advances no animation at all rather than by however long
    // the process had been alive.
    std::chrono::steady_clock::time_point lastFrameTime_{};
    bool haveFrameTime_ = false;
};

}  // namespace forgeshape
