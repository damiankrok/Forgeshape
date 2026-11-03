// Measuring the distance between two picked points (`CAD-MEASURE-R1`).
//
// A READ and nothing else: the two points are session presentation held by the
// shell's adapter, never project truth -- no `.forge` byte, no history step,
// no checkpoint, no fingerprint, no revision. A point is where a tap landed on
// a drawn surface, snapped to the hit triangle's nearest corner when that
// corner is within `kMeasureSnapPixels` of the finger on screen, so a box's
// corner is measured exactly rather than wherever the finger happened to be.
//
// Platform-neutral C++17: no JNI, no Android, no renderer type beyond the
// overlay value the sketch slot already carries.
#pragma once

#include <cstdint>

#include "forgeshape_gizmo.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_sketch_overlay.h"

namespace forgeshape {

// How close on screen a triangle corner must be to the tap to be taken
// instead of the hit point, in pixels.
constexpr float kMeasureSnapPixels = 24.0f;

struct MeasureState {
    // 0 nothing picked, 1 the first point, 2 both.
    int count = 0;
    Vec3 a{0.0f, 0.0f, 0.0f};
    Vec3 b{0.0f, 0.0f, 0.0f};
    bool snappedA = false;
    bool snappedB = false;
};

// Records one picked point: the first, then the second; a third starts a new
// measurement from it, so tapping on keeps measuring without a Clear.
void measureRecordPoint(MeasureState* state, const Vec3& point, bool snapped);

struct MeasureResult {
    bool complete = false;
    double distance = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
};

// The distance and its world-axis components, from A to B. Incomplete until
// both points are picked.
MeasureResult measureResult(const MeasureState& state);

// The point a tap measures: `hit` (world), or the corner of the hit triangle
// `triangle` of `mesh` (placed by `model`) nearest the tap on screen when one
// is within `kMeasureSnapPixels` of (`tapX`, `tapY`). `*snapped` says which.
Vec3 measureSnapPoint(const CameraSnapshot& camera, int viewportWidth, int viewportHeight,
                      float tapX, float tapY, const RuntimeMesh& mesh, const Mat4& model,
                      int triangle, const Vec3& hit, bool* snapped);

// The overlay that shows a measurement: a small cross at each picked point,
// `markerSize` world metres across, and the line between them. Empty for
// nothing picked. Its revision is a hash of its vertices, so the renderer
// re-uploads only when something moved.
SketchOverlayPtr measureOverlay(const MeasureState& state, float markerSize);

}  // namespace forgeshape
