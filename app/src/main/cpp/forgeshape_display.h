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

    // Both return true when the value actually changed, so a caller can log a
    // real transition and avoid reporting a no-op as one.
    bool setShadingModel(ShadingModel model);
    bool setSurfaceShading(SurfaceShading shading);

    // --- introspection (logging and self-tests only) ---
    uint64_t changeCount() const { return changeCount_.load(std::memory_order_relaxed); }

private:
    // Stored as the enumerator's own numeric value, which IS the UI index —
    // the index functions above assert that correspondence rather than
    // maintaining a second mapping table that could drift.
    std::atomic<int> shading_{static_cast<int>(kDefaultShadingModel)};
    std::atomic<int> surface_{static_cast<int>(kDefaultSurfaceShading)};
    std::atomic<uint64_t> changeCount_{0};
};

// Process-scoped store. Like the camera, the selection, the mesh store and the
// sculpt session, it outlives every Surface, which is exactly why the chosen
// display settings survive HOME/resume with no save/restore code in the Android
// layer.
DisplaySettingsStore& displaySettings();

}  // namespace forgeshape
