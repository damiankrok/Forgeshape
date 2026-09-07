// The canvas control of the extrusion a sketch is about to become
// (`CAD-UX-S1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// `MotionEvent`, no pixel that is not a camera-derived one. Everything here is
// arithmetic over values the sketch session already owns, which is what lets a
// desktop adapter reuse it whole.
//
// What this file is, and what it deliberately is NOT
// --------------------------------------------------
// It is NOT a second model of the extrusion. `SketchSession` already owns the
// profile the user chose, the depth and the direction, and it is the only
// thing that writes them; a parallel intent struct holding copies of those
// three values would be exactly the second answer the architecture forbids.
// What was genuinely missing is three things, and they are what this file
// adds:
//
//   1. WHERE the manipulator stands, in world space  -- `CadExtrudeAnchors`,
//      derived from the sketch authoring frame, the chosen profile polygon and
//      the extrusion. No camera, no viewport and no zoom enters it, so the
//      solid cannot depend on how the sketch was looked at.
//   2. HOW BIG the control is drawn and grabbed      -- `CadExtrudeControlScale`,
//      a world reference size clamped into a screen band. Deliberately NOT the
//      gizmo rule: `gizmoWorldScale` holds a constant number of PIXELS at
//      every distance, which is right for a placement instrument and is the
//      opposite of what a control attached to the work should do.
//   3. WHAT A DRAG MEANS                             -- `CadExtrudeManipulator`,
//      one captured pointer whose basis is frozen at pointer-down, solved with
//      the gizmo own axis solver so that the same world displacement is the
//      same depth change at every zoom.
//
// Nothing here is truth. No value in this file is serialized, reaches a
// `.forge` byte, a checkpoint, the project fingerprint or a history step; the
// depth and the direction a drag produces are written back through
// `SketchSession::setExtrude`, the one door they already had.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_math.h"
#include "forgeshape_sketch.h"

namespace forgeshape {

// The sketch world authoring frame. Defined by `forgeshape_sketch_session.h`,
// which includes THIS header -- the tool is a part the session owns, not the
// other way round -- so it is named here and dereferenced only in the
// implementation.
struct SketchFrame;

// ---------------------------------------------------------------------------
// 1. Where the manipulator stands
// ---------------------------------------------------------------------------

// The area centroid of a closed profile polygon, in sketch (u, v).
//
// The AREA centroid rather than the mean of the vertices, because a profile
// vertices are not evenly distributed -- a circle carries 32 and a rectangle
// 4, and a chain of lines carries whatever the user drew -- and the arrow
// belongs in the middle of the SHAPE rather than in the middle of its vertex
// list. Falls back to the vertex mean for a polygon whose signed area is too
// small to divide by, which `extractClosedProfiles` has already refused, so
// the fallback is a guard rather than a behaviour.
bool sketchPolygonCentroid(const std::vector<SketchPoint>& polygon, SketchPoint* out);

// Everything the presentation layer needs about the extrusion, in WORLD space.
//
// Derived on every read from the frame, the profile and the extrusion. Nothing
// is stored, so there is no anchor that can go stale behind an edit, and two
// different cameras looking at one sketch produce identical anchors.
struct CadExtrudeAnchors {
    bool valid = false;
    // The profile centroid ON the support plane. `generateCadMesh` always puts
    // one cap on the sketch plane -- direction decides only which -- so this is
    // where the arrow starts whichever way the solid grows.
    Vec3 base{};
    // The frame unit normal. NOT flipped by the direction: it is what the plane
    // means, and a caller that needs the growth direction wants `axis`.
    Vec3 normal{};
    // The direction the solid actually grows: `+normal` along, `-normal`
    // against. Unit.
    Vec3 axis{};
    // `base + axis * depth`: the free cap centre, and the arrow point.
    Vec3 tip{};
    // Where the numeric chrome belongs: the middle of the shaft. Deliberately
    // not the tip -- a value pinned to the tip chases the finger during a drag,
    // and a value at the base sits on the drawing it measures.
    Vec3 label{};
    Meters depth = 0.0;
};

// False, writing nothing, for a degenerate frame, an empty polygon or a
// non-finite depth.
bool cadExtrudeAnchors(const SketchFrame& frame, const ClosedProfile& profile,
                       const ExtrudeFeature& extrude, CadExtrudeAnchors* out);

// ---------------------------------------------------------------------------
// 2. How big the control is
// ---------------------------------------------------------------------------
//
// The owner request, stated as arithmetic: the cluster is a WORLD object of
// reference size `W` metres, so pulling the camera back makes it smaller and
// coming in makes it larger, and it saturates at both ends rather than
// becoming unreadable at a distance or swallowing the viewport up close.
//
//     S_world = W / metersPerPixel          the size it would be, in pixels
//     scale   = clamp(S_world / S_ref, min, max)
//     S       = scale * S_ref               the size it IS, in pixels
//     s       = S * metersPerPixel          that size back in world metres
//
// `scale` is one dimensionless number, and it is the ONE number the drawing
// and the presentation share: the arrow head is built at `s` world metres and
// the Android cluster is drawn at `scale`. The hit CORRIDOR is deliberately
// not in it -- see kCadExtrudeGrabRadiusUnits.
//
// PROVISIONAL VALUES. These are a starting point with stated arithmetic, not
// an approved look, exactly as the two `SCULPT-FCM-R1` feel constants were:
//
//   * `W` 0.45 m is `S_ref` pixels wide at the sketch own default framing
//     (about nine metres over a 2400 px phone, ~0.00375 m/px), so a freshly
//     opened sketch shows the cluster at exactly its authored size.
//   * `S_ref` 120 px is a little under the gizmo own screen budget (its rotate
//     rings are 156 reference units across at the default visual scale), so
//     the extrude cluster never dominates the instrument the user places
//     bodies with.
//   * The MINIMUM is 0.80 and it is arithmetic rather than taste: the Android
//     cluster controls are authored at `R.dimen.cad_canvas_control` 60 dp, and
//     60 x 0.80 is exactly the 48 dp interactive floor. Lowering it without
//     raising that dimension would put a control under the floor.
//   * The MAXIMUM is 1.60, so the whole visible range is a factor of two --
//     enough that the attenuation reads, bounded enough that a close camera
//     cannot cover the profile being extruded.
constexpr float kCadExtrudeControlWorldMeters = 0.45f;
constexpr float kCadExtrudeControlReferencePixels = 120.0f;
constexpr float kCadExtrudeControlMinScale = 0.80f;
constexpr float kCadExtrudeControlMaxScale = 1.60f;

struct CadExtrudeControlScale {
    bool valid = false;
    float metersPerPixel = 0.0f;
    // Before the clamp, for the test that asserts monotonicity inside the band.
    float unclampedScale = 0.0f;
    float scale = 0.0f;   // the clamped multiplier the shell draws at
    float pixels = 0.0f;  // scale * kCadExtrudeControlReferencePixels
    float world = 0.0f;   // pixels * metersPerPixel: the head world size
    bool clampedLow = false;
    bool clampedHigh = false;
};

// The rule itself, over the one camera quantity it needs. Pure, so it can be
// swept over a range of distances in a test with no camera at all.
bool cadExtrudeControlScaleFor(float metersPerPixel, CadExtrudeControlScale* out);

// The same rule with the camera quantity read from the camera, at the depth of
// `anchor`. Draw and hit test both come through here, so they cannot disagree.
bool cadExtrudeControlScale(const CameraSnapshot& camera, const Vec3& anchor, int viewportHeight,
                            CadExtrudeControlScale* out);

// ---------------------------------------------------------------------------
// The arrow proportions
// ---------------------------------------------------------------------------
//
// Fractions of the control world size `s`, so the HEAD scales with the camera
// rule while the SHAFT stays the extrusion it measures. That split is the whole
// information the arrow carries: its length is the depth, and its head is the
// control.
constexpr double kCadExtrudeArrowHeadLengthFraction = 0.42;
constexpr double kCadExtrudeArrowHeadHalfWidthFraction = 0.16;
// The head is drawn as this many barbs evenly spaced about the axis, so it
// reads as a cone from any viewpoint rather than collapsing to a line when the
// camera happens to lie in the plane of a two-barb chevron.
constexpr int kCadExtrudeArrowBarbs = 6;
// A tick across the base, in the same drawing language the dimension
// annotations use, so the arrow reads as a measurement rather than as geometry.
constexpr double kCadExtrudeArrowBaseTickFraction = 0.18;

// How far off the drawn arrow a pointer may land and still take it, in gizmo
// REFERENCE units (dp).
//
// Deliberately NOT scaled by `CadExtrudeControlScale`, for exactly the reason
// the gizmo own hit corridors are not scaled by its visual-size preference: 24
// units either side is a 48-unit diameter, which is the interactive floor, and
// a corridor that shrank with the glyph would put the arrow under it at the
// minimum scale.
constexpr float kCadExtrudeGrabRadiusUnits = 24.0f;

// The smallest depth a DRAG may produce.
//
// A drag is clamped where a typed value is refused, and the difference is not
// an inconsistency: a typed value is a statement the user made and a refusal is
// the honest answer to a bad one, while a drag is a continuous gesture with no
// moment at which the user submitted zero. Stopping the arrow at a millimetre
// keeps the body valid and keeps the control under the finger; refusing
// mid-drag would strand the gesture with nothing to grab.
constexpr Meters kCadExtrudeMinDragDepthMeters = 1.0e-3;

// ---------------------------------------------------------------------------
// 3. What a drag means
// ---------------------------------------------------------------------------

// One pointer, dragging the arrow along the extrusion axis.
//
// The contract is the gizmo one, restated rather than reinvented: ONE captured
// pointer id for the whole life of the drag, a basis FROZEN at pointer-down so
// the preview growing under the finger cannot move the control out from under
// it, a second pointer or a cancel restoring the pre-drag value, and a
// degenerate viewpoint holding the last good value rather than guessing.
//
// It holds no depth of its own beyond the pre-drag one it must be able to put
// back: `SketchSession` remains the single writer of the extrusion.
class CadExtrudeManipulator {
public:
    bool capturing() const { return pointerId_ >= 0; }
    int32_t capturedPointerId() const { return pointerId_; }

    // Whether (x, y) lands on the arrow drawn for `anchors` under `camera`.
    //
    // Measured in SCREEN space against the same projected segment the renderer
    // drew, extended past the tip by the head that was drawn at the shared
    // scale, and widened by the reference-unit corridor above.
    bool hitTest(const CadExtrudeAnchors& anchors, const CameraSnapshot& camera, float x, float y,
                 int viewportWidth, int viewportHeight) const;

    // Captures the pointer and freezes the basis. False when the geometry
    // cannot produce one, in which case nothing is captured.
    bool beginDrag(int32_t pointerId, const CadExtrudeAnchors& anchors,
                   const CameraSnapshot& camera, float x, float y, int viewportWidth,
                   int viewportHeight);

    // The depth this sample indicates, from the frozen basis.
    //
    // False, writing nothing, when the axis cannot be solved -- the caller then
    // holds the depth it already has, which is what keeps a degenerate
    // viewpoint from producing a jump or a NaN.
    bool updateDrag(int32_t pointerId, const CameraSnapshot& camera, float x, float y,
                    int viewportWidth, int viewportHeight, Meters* outDepth);

    // Ends the drag, keeping whatever depth was last written.
    void endDrag();

    // Ends the drag and reports the depth the gesture started from, so the
    // caller can put it back. False when nothing was captured.
    bool cancelDrag(Meters* outRestoreDepth);

    Meters depthAtDown() const { return depthAtDown_; }
    AxisSolveStatus lastSolve() const { return lastSolve_; }
    // How many drags were begun. Diagnostics only.
    uint32_t dragCount() const { return dragCount_; }

private:
    int32_t pointerId_ = -1;
    // The basis, frozen at pointer-down and untouched for the life of the drag.
    Vec3 base_{};
    Vec3 axis_{};
    Meters depthAtDown_ = 0.0;
    float axisAtDown_ = 0.0f;
    Meters lastGoodDepth_ = 0.0;
    AxisSolveStatus lastSolve_ = AxisSolveStatus::Resolved;
    uint32_t dragCount_ = 0;
};

// ---------------------------------------------------------------------------
// The drawable arrow
// ---------------------------------------------------------------------------

// Appends the WORLD-space line list of the manipulator to `out`.
//
// `controlWorld` is `CadExtrudeControlScale::world`, the same number the hit
// test head extension uses. `grabbed` tags the vertices so the renderer draws
// them in the highlight colour -- the emphasis tag the sketch overlay already
// carries, so this needs no renderer change and no new overlay style.
void appendCadExtrudeArrow(std::vector<GizmoVertex>* out, const CadExtrudeAnchors& anchors,
                           double controlWorld, bool grabbed);

}  // namespace forgeshape
