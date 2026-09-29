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
#include "forgeshape_sketch_region.h"

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

// One side's manipulator, in WORLD space. `CAD-EXT-R1` made the extrusion two
// sides, so the arrow, the tip and the label became per side; everything else
// about them is what it always was.
struct CadExtrudeSideAnchor {
    // Whether this side has any extent, and therefore an arrow to draw and
    // grab. A One Side extrusion has exactly one present side; Symmetric has
    // two of equal length; Two Sides has two independent ones, of which one
    // may legitimately be zero and is then absent.
    bool present = false;
    // Unit, pointing away from the sketch plane on this side.
    Vec3 axis{};
    // `base + axis * distance`: the cap centre this side reaches, and the
    // arrow point.
    Vec3 tip{};
    // Where the numeric chrome belongs: the middle of the shaft. Deliberately
    // not the tip -- a value pinned to the tip chases the finger during a drag,
    // and a value at the base sits on the drawing it measures.
    Vec3 label{};
    Meters distance = 0.0;
};

// Everything the presentation layer needs about the extrusion, in WORLD space.
//
// Derived on every read from the frame, the profile and the extrusion. Nothing
// is stored, so there is no anchor that can go stale behind an edit, and two
// different cameras looking at one sketch produce identical anchors.
struct CadExtrudeAnchors {
    bool valid = false;
    // The profile centroid ON the support plane -- the sketch's own plane, not
    // a cap, so it is the one point both sides grow from and it does not move
    // when either distance changes.
    Vec3 base{};
    // The frame unit normal. NOT flipped by anything: it is what the plane
    // means, and `positive`/`negative` are named against it.
    Vec3 normal{};
    // The `+N` and `-N` sides. THE two-sided truth, and what a hit test, a
    // drag and the drawing all read.
    CadExtrudeSideAnchor positive{};
    CadExtrudeSideAnchor negative{};

    // The PRIMARY side: the one the mode's primary distance is on -- the solid's
    // own side in One Side, and `+N` in Symmetric and Two Sides. The fields
    // below mirror it, because the chrome cluster and every caller that
    // predates `CAD-EXT-R1` speak of one arrow and are still right about a One
    // Side extrusion.
    Vec3 axis{};
    Vec3 tip{};
    Vec3 label{};
    Meters depth = 0.0;
    // True when `axis`/`tip`/`label` describe the `+N` side.
    bool primaryIsPositive = true;

    const CadExtrudeSideAnchor& side(bool positiveSide) const {
        return positiveSide ? positive : negative;
    }
};

// False, writing nothing, for a degenerate frame, an empty polygon, a
// non-finite distance or an extrusion with no extent at all.
bool cadExtrudeAnchors(const SketchFrame& frame, const ClosedProfile& profile,
                       const ExtrudeFeature& extrude, CadExtrudeAnchors* out);

// The same anchors from an explicit base point in sketch (u, v)
// (`CAD-VERTICAL-SLICE-R1`): a region with a hole stands its arrow on its own
// material -- its area centroid when that is not in a hole, otherwise a point
// strictly inside it -- rather than on a centroid a centred hole would swallow.
bool cadExtrudeAnchorsAt(const SketchFrame& frame, const SketchPoint& base,
                         const ExtrudeFeature& extrude, CadExtrudeAnchors* out);

// THE rule for where the arrow of a region selection stands, in sketch (u, v):
// the first chosen region's area centroid -- R0's anchor exactly, for a region
// without holes -- unless a hole swallows it, when a point strictly inside the
// material stands in. One function, so the live session and a committed body's
// retained-sketch chip cannot disagree. False when nothing is chosen.
bool extrudeSelectionAnchorPoint(const SketchRegionExtraction& regions,
                                 const ExtrudeFeature& extrude, SketchPoint* out);

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
//   * The MINIMUM is 0.80. Since `CAD-VERTICAL-SLICE-R1` the Android HUD
//     scales only its GLYPHS by this multiplier -- `clamp(28 dp x scale, 24,
//     32)` -- and never its hit areas, which stay at least 48 dp at every
//     scale; so 0.80 is where the glyph reaches its 24 dp floor, and the
//     interactive floor no longer depends on this number at all.
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
    // Which side the captured drag belongs to. Meaningful only while
    // `capturing()`; frozen at pointer-down with the basis, so a Symmetric
    // extrusion growing under the finger cannot move the gesture to the other
    // arrow half way through.
    bool capturedPositiveSide() const { return positiveSide_; }

    // Whether (x, y) lands on ONE side's arrow, drawn for `anchors` under
    // `camera`.
    //
    // Measured in SCREEN space against the same projected segment the renderer
    // drew, extended past the tip by the head that was drawn at the shared
    // scale, and widened by the reference-unit corridor above.
    bool hitTestSide(const CadExtrudeAnchors& anchors, bool positiveSide,
                     const CameraSnapshot& camera, float x, float y, int viewportWidth,
                     int viewportHeight) const;

    // Which side (x, y) takes, preferring the PRIMARY one when both corridors
    // contain the point -- a Symmetric extrusion seen almost edge-on can
    // overlap both, and a deterministic answer beats a nearest-pixel race.
    // False when neither side takes it.
    bool hitTest(const CadExtrudeAnchors& anchors, const CameraSnapshot& camera, float x, float y,
                 int viewportWidth, int viewportHeight, bool* outPositiveSide = nullptr) const;

    // Captures the pointer on one side and freezes the basis. False when the
    // geometry cannot produce one, in which case nothing is captured.
    bool beginDrag(int32_t pointerId, const CadExtrudeAnchors& anchors, bool positiveSide,
                   const CameraSnapshot& camera, float x, float y, int viewportWidth,
                   int viewportHeight);

    // The captured side's distance this sample indicates, from the frozen
    // basis.
    //
    // False, writing nothing, when the axis cannot be solved -- the caller then
    // holds the distance it already has, which is what keeps a degenerate
    // viewpoint from producing a jump or a NaN.
    bool updateDrag(int32_t pointerId, const CameraSnapshot& camera, float x, float y,
                    int viewportWidth, int viewportHeight, Meters* outDistance);

    // Ends the drag, keeping whatever distance was last written.
    void endDrag();

    // Ends the drag and reports the distance the gesture started from, so the
    // caller can put it back. False when nothing was captured.
    bool cancelDrag(Meters* outRestoreDistance);

    Meters depthAtDown() const { return depthAtDown_; }
    AxisSolveStatus lastSolve() const { return lastSolve_; }
    // How many drags were begun. Diagnostics only.
    uint32_t dragCount() const { return dragCount_; }

private:
    int32_t pointerId_ = -1;
    // The basis, frozen at pointer-down and untouched for the life of the drag.
    Vec3 base_{};
    Vec3 axis_{};
    bool positiveSide_ = true;
    Meters depthAtDown_ = 0.0;
    float axisAtDown_ = 0.0f;
    Meters lastGoodDepth_ = 0.0;
    AxisSolveStatus lastSolve_ = AxisSolveStatus::Resolved;
    uint32_t dragCount_ = 0;
};

// ---------------------------------------------------------------------------
// 4. Where the extrusion can be SEEN from (`CAD-UX-S1-C1`)
// ---------------------------------------------------------------------------
//
// `OQ-CAD-UX-01`, stated as arithmetic. A sketch is drawn through a view aimed
// EXACTLY along its support normal, and that normal IS the extrusion axis, so
// while the sketch is being authored the arrow points straight at the eye, its
// shaft has no screen extent and `solveAxisParameter` has nothing to resolve
// against. The fix is not a second gesture model reading a raw screen delta --
// that would give the drag a meaning the world axis does not have. The fix is
// that the sketch's exact view and the extrusion's preview are two DIFFERENT
// presentations of one authored truth, and Finish Sketch is the moment the
// second one begins.
//
// Everything here is presentation. It reads a camera pose and a frame and it
// answers with another camera pose; no value below becomes a `CadBodyState`,
// reaches a `.forge` byte, a checkpoint, the project fingerprint or a history
// step, and no authored coordinate can be moved by any of it.

// Which of the two policies produced the preview pose, so a caller can log the
// path actually taken and a case can assert it rather than infer it.
enum class CadFeatureViewSource : uint8_t {
    // The user's own pre-sketch 3D view already sees the axis usefully; it is
    // given back, re-centred on the work.
    PriorView,
    // No prior view, or one that looks down the axis: a deterministic oblique
    // view derived from the support frame alone.
    ObliqueFallback,
    // Neither could produce a usable view. Nothing is installed and the caller
    // keeps the view it has -- fail closed, on the manipulator's own terms.
    Unavailable,
};

const char* cadFeatureViewSourceName(CadFeatureViewSource source);

// PROVISIONAL VALUES, with stated arithmetic. Not owner visual acceptance.
//
// `solveAxisParameter` falls back to its plane method below a closest-approach
// denominator of `kGizmoAxisParallelDenominator` (0.02), which is exactly
// sin^2 between the ray and the axis -- an angle of about 8.1 degrees. A view
// that merely cleared that would be resolvable and still unusable, because the
// shaft would be a few pixels long. 0.35 is about 20.5 degrees, comfortably
// clear of the solver's own limit, and it is measured as a SINE so it is
// symmetric about edge-on and needs no direction convention.
constexpr float kCadFeatureViewMinAxisSine = 0.35f;

// How far off the support normal the fallback stands. 0.62 rad is about 35.5
// degrees: far enough that the axis projects to a real segment (sin = 0.58,
// well past the threshold above), near enough that the profile is still read
// as the shape that was just drawn rather than edge-on.
constexpr float kCadFeatureViewObliqueRadians = 0.62f;

// Where around the normal it stands, in the frame's own (u, v). Any azimuth
// gives the same axis projection; this one is a fixed choice so that one
// sketch always produces one view.
constexpr float kCadFeatureViewAzimuthRadians = 0.90f;

// A candidate direction is turned into the orbit yaw/pitch the general 3D
// camera speaks, and pitch is CLAMPED there. For a support normal roughly
// `kCadFeatureViewObliqueRadians` off world up, one azimuth can aim the tilt
// along the meridian and land inside that clamp, which would silently change
// the direction and with it the projection. So the candidate is re-derived
// from the clamped angles and re-measured, and a failure steps the azimuth by
// a quarter turn: at most one quadrant can point up the meridian, so a bounded
// four attempts is a proof rather than a hope.
constexpr int kCadFeatureViewAzimuthAttempts = 4;

// The orbit convention's direction, target -> eye. Identical by construction to
// `CameraController::orbitDirection`, which is private; a case asserts the two
// agree so the copy cannot drift.
Vec3 cadFeatureViewDirection(float yaw, float pitch);

// The inverse. False when the direction is non-finite or so nearly vertical
// that yaw carries no information.
bool cadFeatureViewYawPitch(const Vec3& direction, float* outYaw, float* outPitch);

// |sin| of the angle between a target->eye direction and the extrusion axis:
// 0 when the axis points at the eye, 1 when it lies across the view. The one
// number the policy below is written in.
float cadFeatureViewAxisSine(const Vec3& viewDirection, const Vec3& axis);

bool cadFeatureViewUsable(const Vec3& viewDirection, const Vec3& axis);

// The whole policy, as a pure function over values.
//
// `current` is the pose the sketch is being looked through (its orthographic
// span is the framing to keep). `prior` is the user's pre-sketch pose, or null
// when there was none -- the first-project bootstrap opens a sketch over an
// empty scene and has no earlier view at all. `out` is written only when the
// result is not `Unavailable`.
//
// Both paths re-centre the target on the work anchor: the direction is the
// user's, the centre is the extrusion's, which is what keeps the arrow in
// frame when the sketch is nowhere near where the camera was last pointed.
CadFeatureViewSource cadFeatureViewPose(const CameraController::Pose& current,
                                        const CameraController::Pose* prior,
                                        const SketchFrame& frame,
                                        const CadExtrudeAnchors& anchors,
                                        CameraController::Pose* out);

// ---------------------------------------------------------------------------
// The drawable arrow
// ---------------------------------------------------------------------------

// Appends the WORLD-space line list of the manipulator to `out` -- ONE arrow
// per side that has extent, so a Symmetric or Two Sides extrusion is two arrows
// out of one call and a One Side extrusion is exactly the one it always was.
//
// `controlWorld` is `CadExtrudeControlScale::world`, the same number the hit
// test head extension uses. `grabbed` tags the vertices so the renderer draws
// them in the highlight colour -- the emphasis tag the sketch overlay already
// carries, so this needs no renderer change and no new overlay style; it is
// applied to `grabbedSide` alone, so the side under the finger is the side that
// lights up.
// The operation preview's tint, rgb and mix weight (`CAD-VERTICAL-SLICE-R1`):
// Add a positive green, Cut a destructive red, New Body a neutral cool accent.
// Presentation policy, never a preference and never truth -- and never the only
// carrier of what the operation is: the HUD names it by shape and by label.
void cadOperationPreviewTint(CadFeatureOperation operation, float out[4]);

void appendCadExtrudeArrow(std::vector<GizmoVertex>* out, const CadExtrudeAnchors& anchors,
                           double controlWorld, bool grabbed, bool grabbedSide = true);

}  // namespace forgeshape
