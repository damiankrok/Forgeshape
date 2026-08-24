// ForgeShape native Vulkan renderer (Stage 003 viewport foundation).
//
// Owns exactly what is needed to present one indexed cube with perspective and
// depth into an Android Surface. This is deliberately NOT a generic engine.
#pragma once

// Required before <vulkan/vulkan.h> to expose VkAndroidSurfaceCreateInfoKHR
// and vkCreateAndroidSurfaceKHR.
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#include <android/native_window.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_display.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_scene.h"

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

    // Renders and presents one frame. Returns false on unrecoverable failure.
    bool drawFrame();

private:
    bool pickPhysicalDeviceAndQueues();
    bool createLogicalDevice();
    bool createShaderModules();
    bool createSyncObjects();
    bool createCommandPool();

    bool createSwapchainDependents();
    void destroySwapchainDependents();

    bool createSwapchain();
    bool createDepthResources();
    bool createRenderPass();
    bool createFramebuffers();
    bool createPipeline();
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
    // The only sampled image in ForgeShape, and therefore the only reason a
    // descriptor set exists at all: everything else the pipeline needs still
    // travels as push constants. The image content is generated on the CPU by
    // forgeshape_matcap.cpp; this side only uploads and binds it.
    //
    // Device-scoped, like the mesh buffers: created once with the device and
    // untouched by a Surface swap, so a HOME/resume does not regenerate or
    // re-upload the MatCap.
    bool createDescriptorResources();
    bool createMatCapResources();
    void destroyMatCapResources();
    // Waits on the renderer's own frame fences (never vkDeviceWaitIdle /
    // vkQueueWaitIdle) so no in-flight frame can still reference the mesh
    // buffers that are about to be overwritten or destroyed.
    bool waitForMeshBuffersIdle();
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
    void syncScene();
    // One body's half of syncScene: the per-body gate, then rebuild + upload.
    void syncBody(const SceneDrawItem& item);
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

    // The one sampled image, its sampler, and the single descriptor set that
    // binds them. Device-scoped: a Surface swap does not touch them.
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

    // True when this swapchain deliberately declared a pre-transform the surface
    // does not currently use (the identity-pre-transform orientation
    // convention), which makes VK_SUBOPTIMAL_KHR the expected steady state
    // rather than a rebuild request. Set by createSwapchain().
    bool expectSuboptimal_ = false;

    bool needsSwapchainRebuild_ = false;
    bool presentedThisSession_ = false;
    uint64_t frameIndex_ = 0;
};

}  // namespace forgeshape
