// ForgeShape viewport display settings — how the object is DRAWN, never what it
// is.
//
// Platform-independent: no JNI, no Android, no Vulkan and no UI type appears
// here.
//
// This is presentation state, and the distinction is load-bearing. Nothing in
// this file is truth about:
//
//   * an exact dimension or any other Construction parameter;
//   * Construction history or a PrimitiveKind;
//   * a sculpt deformation or a SculptRevision;
//   * picking topology or an ObjectId;
//   * a MeshRevision.
//
// Changing any value here mints no revision, publishes no mesh, moves no vertex
// of the authoritative RuntimeMesh and cannot be observed by picking. The most
// a change can do is cause the RENDER-ONLY derived geometry to be rebuilt (see
// RenderMeshCache), and only the Smooth/Faceted choice does even that —
// Studio Solid and MatCap are a fragment-stage uniform and touch no geometry at
// all.
//
// Ownership: NATIVE owns these values, exactly as it owns the product mode, the
// active tool and the camera. The Android UI may request a change and may read
// the current value back to render its own controls, but it does not hold them.
// That is what makes the settings survive a HOME/resume for free: the store is
// process-scoped, so it outlives every Surface and every Activity instance and
// is reset only when the process dies.
#pragma once

#include <atomic>

#include "forgeshape_render_mesh.h"

namespace forgeshape {

// Which shading model the fragment stage evaluates.
//
// A closed enum and a switch, deliberately — the same rule the sculpt tools
// follow. Three shading models do not justify a material system, a registry or
// an authored material asset, and a full PBR path is a later stage with its own
// approval rather than a fourth enumerator waiting to be filled in.
enum class ShadingModel {
    StudioSolid,       // the neutral modelling default
    MatCap,            // the high-readability sculpt/form view
    DebugSourceColor,  // DEBUG ONLY: the source mesh's per-vertex colours
};

constexpr int kShadingModelCount = 3;

// What the viewport is CLEARED to, behind everything.
//
// A closed enum, exactly like ShadingModel, and for the same reason: the Android
// UI has three appearances, and the geometry domain must not learn what an
// Android theme is. What crosses JNI is a viewport APPEARANCE — one of these —
// and native code owns what each one actually looks like. No Android type, no
// theme enum and no RGB authored above JNI reaches this file.
//
// Changing it is the cheapest kind of presentation change there is: the clear
// value is written into the render pass every frame anyway, so a switch rebuilds
// no geometry, mints no revision, re-uploads nothing and does not touch the
// swapchain, the pipeline or any GPU buffer.
// Three grounds are DARK — the original authored set: Warm Graphite leans red,
// Neutral Charcoal leans blue-grey, and Light Charcoal is the lightest DARK
// ground while a neutral clay render still reads as lit rather than washed
// out. `UI-PREF-R1` (UI-OWNER-42) adds two LIGHT grounds, Warm Light and Cool
// Light, derived through the same semantic roles; everything drawn against a
// ground asks `viewportBackgroundIsLight` rather than naming a member, so a
// tool colour is authored per ground FAMILY and the three dark answers stay
// exactly what they were. The three dark members keep their indices, because
// the index is what crosses JNI.
enum class ViewportBackground {
    WarmGraphite,     // the product default: a warm dark studio ground
    NeutralCharcoal,  // a cooler steel-grey ground
    LightCharcoal,    // the lightest DARK ground
    WarmLight,        // a warm, cream-biased light ground
    CoolLight,        // a cool, steel-biased light ground
};

constexpr int kViewportBackgroundCount = 5;
constexpr ViewportBackground kDefaultViewportBackground = ViewportBackground::WarmGraphite;

const char* viewportBackgroundName(ViewportBackground background);

// Maps the UI's index onto the enum. Out-of-range is REFUSED rather than
// clamped, for the same reason shadingModelFromIndex refuses.
bool viewportBackgroundFromIndex(int index, ViewportBackground* out);
int viewportBackgroundIndex(ViewportBackground background);

// Whether this ground is a LIGHT canvas, which is the one question every
// per-ground tool colour (grid, gizmo) asks. Stated once here rather than as a
// member comparison at each site, so adding a ground touches this file and the
// palettes, never a switch in a renderer.
bool viewportBackgroundIsLight(ViewportBackground background);

// The linear RGB the render pass clears to, in the order Vulkan wants.
//
// These MUST stay in step with `p1_viewport_background` ..
// `p5_viewport_background` in `colors.xml`, which is what the Android window is
// painted with before the surface has anything on it; a mismatch shows as a
// flash on launch. `DISP-VBG-03`..`05` and `DISP-VBG-08`/`09` pin the exact
// values so the set cannot drift silently.
void viewportBackgroundColor(ViewportBackground background, float* outRgb);

// How heavily the Construction gizmo's strokes are drawn (`UI-PREF-R1` F).
//
// A closed enum, not a width: the gizmo is a LINE LIST drawn at Vulkan's
// guaranteed 1-pixel width, and a "thick" stroke is a BUNDLE of parallel lines
// (see kGizmoStrokeOffsetUnits in forgeshape_gizmo.h), so the only honest
// choices are the bundles the geometry generator knows how to author. Regular
// is EXACTLY the geometry the product drew before the preference existed.
//
// Presentation only, on the display store's terms: it changes which canonical
// vertex list the renderer holds and nothing about where a handle is, what it
// grabs, or how far a drag moves — hit testing never reads it.
enum class GizmoStrokeWeight {
    Thin,     // the same bundle at half the spread: a finer instrument
    Regular,  // the accepted default, byte-identical to the pre-preference gizmo
    Bold,     // a wider, filled bundle for a heavier instrument
};

constexpr int kGizmoStrokeWeightCount = 3;
constexpr GizmoStrokeWeight kDefaultGizmoStrokeWeight = GizmoStrokeWeight::Regular;

const char* gizmoStrokeWeightName(GizmoStrokeWeight weight);
bool gizmoStrokeWeightFromIndex(int index, GizmoStrokeWeight* out);
int gizmoStrokeWeightIndex(GizmoStrokeWeight weight);

// Whether the world reference grid is drawn.
//
// ON by default. The grid is what makes an empty viewport legible: without it a
// body floats in a void with no scale, no horizon and no origin, and the first
// question a modelling application has to answer is "how big is this and where
// is it". It is subtle enough (see forgeshape_grid.cpp) that a user who wants
// the bare model turns it off deliberately rather than being forced to.
//
// It is a plain bool rather than an enum because there are exactly two answers
// and no third one is coming: a Sketch grid is a different contract on a
// different plane, not a third state of this one. See forgeshape_grid.h.
constexpr bool kDefaultGridVisible = true;

// Whether the SELECTED body is drawn with a persistent silhouette outline
// (`SEL-OUT-R1`, UI-OWNER-10 / UI-OWNER-11).
//
// ON by default, and for the reason the grid is: with it off, the only thing
// that says which body is selected is the Objects capsule's name, and a user
// who wants the bare model turns the edge off deliberately rather than
// discovering that selection has no visual answer.
//
// It sits beside the grid rather than in AppPreferences because it is exactly
// the same KIND of state: a transient, per-session, process-scoped viewport
// overlay that native code owns, that survives a HOME/resume for free, and that
// enters no `.forge` byte, no checkpoint, no fingerprint and no history. The
// persistent application preferences (palette, handedness, gizmo) live in the
// Settings page; this is a View/Overlay control and shares the grid's whole
// lifecycle, including being reset only when the process dies.
//
// A plain bool for the same reason the grid's is: "on" and "off" exhaust the
// answers. An outline WIDTH or COLOUR preference is deliberately not here — see
// forgeshape_selection_outline.h, where both are policy rather than choice.
constexpr bool kDefaultSelectionOutlineVisible = true;

// The product default. Studio Solid, so a freshly launched viewport shows a
// neutral modelling surface rather than a diagnostic.
constexpr ShadingModel kDefaultShadingModel = ShadingModel::StudioSolid;
constexpr SurfaceShading kDefaultSurfaceShading = SurfaceShading::Smooth;

const char* shadingModelName(ShadingModel model);

// Maps the UI's index onto the enum. Out-of-range is REFUSED rather than
// clamped, for the same reason sculptToolFromIndex refuses: an unknown mode is
// a caller bug, not a value to repair.
bool shadingModelFromIndex(int index, ShadingModel* out);
int shadingModelIndex(ShadingModel model);

bool surfaceShadingFromIndex(int index, SurfaceShading* out);
int surfaceShadingIndex(SurfaceShading shading);

// A coherent read of both values, so a frame cannot be recorded with a shading
// model from before a change and a surface shading from after it.
struct ViewportDisplaySettings {
    ShadingModel shading = kDefaultShadingModel;
    SurfaceShading surface = kDefaultSurfaceShading;
    ViewportBackground background = kDefaultViewportBackground;

    // Whether the world reference grid is drawn behind the model. It rides in
    // this snapshot rather than being read from the store by the renderer for
    // the same reason as the rest: the render thread must read presentation
    // state once, at a known point, and never mid-frame.
    bool gridVisible = kDefaultGridVisible;

    // Whether the selected body is drawn with its persistent outline. Rides in
    // the same snapshot as the grid and for the same reason: the render thread
    // must read presentation state once, at a known point, and never mid-frame
    // — otherwise a toggle landing between the mask pass and the composite
    // would record half an outline.
    bool selectionOutlineVisible = kDefaultSelectionOutlineVisible;

    // Whether the viewport may spend TIME expressing a change, or must land on
    // the final appearance at once. See setReducedMotion.
    bool reducedMotion = false;

    // How heavily the gizmo's strokes are drawn. Rides in the snapshot so the
    // renderer decides ONCE per frame whether the canonical vertex list it
    // holds is the one this weight names, and re-uploads only on a change.
    GizmoStrokeWeight gizmoStrokeWeight = kDefaultGizmoStrokeWeight;
};

// The two values, readable from the render thread and writable from the UI
// thread.
//
// Plain atomics rather than a mutex: each value is a single small integer, the
// two are independent, and the render thread must never block behind a UI
// thread that is mid-gesture. A snapshot that caught one new value and one old
// one would still be a completely valid pair to draw — there is no combination
// of shading model and surface shading that is invalid — so no lock is needed
// to keep them consistent with each other.
class DisplaySettingsStore {
public:
    ViewportDisplaySettings snapshot() const;

    ShadingModel shadingModel() const;
    SurfaceShading surfaceShading() const;
    ViewportBackground viewportBackground() const;
    bool gridVisible() const;
    bool selectionOutlineVisible() const;
    bool reducedMotion() const;

    // All four return true when the value actually changed, so a caller can log
    // a real transition and avoid reporting a no-op as one.
    bool setShadingModel(ShadingModel model);
    bool setSurfaceShading(SurfaceShading shading);
    bool setViewportBackground(ViewportBackground background);

    // Shows or hides the world reference grid.
    //
    // The cheapest presentation change in the renderer, and structurally so:
    // the grid's vertices are uploaded once at device creation and never again,
    // so this decides only whether one already-recorded draw call is issued.
    // No body's render mesh is rebuilt, no buffer is re-uploaded and no
    // MeshRevision is minted — see R1C2-03 and R1C2-06.
    bool setGridVisible(bool visible);

    // Shows or hides the selected body's persistent outline (`SEL-OUT-R1`).
    //
    // As cheap as the grid and structurally so: with it off the renderer
    // records neither the mask pass nor the composite draw, and with it on it
    // records both from GPU buffers that were already there for the body's own
    // draw. No body's render mesh is rebuilt, no vertex or index buffer is
    // re-uploaded, no MeshRevision is minted and no CAD body is regenerated —
    // see SELOUTR1-09, SELOUTR1-10 and SELOUTR1-17..19.
    bool setSelectionOutlineVisible(bool visible);

    // Whether the user has asked the SYSTEM for reduced motion.
    //
    // This is the narrow presentation-only seam that fact crosses, and it is
    // deliberately a plain bool. The signal originates in an Android setting
    // (the animator duration scale), and neither this file nor the renderer may
    // learn that: the Android layer reads the setting, decides what it means and
    // pushes the answer down, exactly as it pushes a viewport APPEARANCE rather
    // than a theme. Nothing here is truth — the only thing the value can change
    // is whether a selection acknowledgement takes 220 ms or no time at all.
    bool setReducedMotion(bool reduced);

    // The gizmo's stroke weight (`UI-PREF-R1` F). A user preference the Android
    // layer persists and pushes down on every launch, exactly as it pushes the
    // viewport appearance; counted as a display change like the grid, and on
    // the same terms: no revision, no publication, no history, no picking.
    GizmoStrokeWeight gizmoStrokeWeight() const;
    bool setGizmoStrokeWeight(GizmoStrokeWeight weight);

    // --- introspection (logging and self-tests only) ---
    uint64_t changeCount() const { return changeCount_.load(std::memory_order_relaxed); }

private:
    // Stored as the enumerator's own numeric value, which IS the UI index —
    // the index functions above assert that correspondence rather than
    // maintaining a second mapping table that could drift.
    std::atomic<int> shading_{static_cast<int>(kDefaultShadingModel)};
    std::atomic<int> surface_{static_cast<int>(kDefaultSurfaceShading)};
    std::atomic<int> background_{static_cast<int>(kDefaultViewportBackground)};
    std::atomic<bool> gridVisible_{kDefaultGridVisible};
    std::atomic<bool> selectionOutlineVisible_{kDefaultSelectionOutlineVisible};
    std::atomic<bool> reducedMotion_{false};
    std::atomic<int> gizmoStrokeWeight_{static_cast<int>(kDefaultGizmoStrokeWeight)};
    std::atomic<uint64_t> changeCount_{0};
};

// Process-scoped store. Like the camera, the selection, the mesh store and the
// sculpt session, it outlives every Surface, which is exactly why the chosen
// display settings survive HOME/resume with no save/restore code in the Android
// layer.
DisplaySettingsStore& displaySettings();

}  // namespace forgeshape
