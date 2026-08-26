// The Construction Move / Rotate gizmo — direct manipulation of a Body's
// placement, in the viewport, with the finger or the stylus.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type and
// no UI type appears here. What crosses in is a CameraSnapshot, a viewport size
// in pixels and a platform-neutral pointer position; what crosses out is a
// snapshot the renderer can draw and a transform the Construction domain owns.
//
// What this owns, and what it deliberately does not
// -------------------------------------------------
// It owns: which handle a pointer landed on, which pointer is captured, the
// drag solver, and the transaction boundary around one drag. It owns NO
// transform of its own — the authoritative Position and Rotation stay on the
// active body's ConstructionTransform throughout, so the renderer, the picker
// and the exact-value editors all read the same numbers mid-drag as they do at
// rest. There is no second solver above JNI and no parallel placement anywhere.
//
// One convention, stated once: WORLD AXES
// ---------------------------------------
// The three Move handles and the three Rotate rings are world X, Y and Z. They
// do not rotate with the body, there is no Local/World toggle, and the pivot is
// always the body's authoritative Construction placement origin — never a mesh
// bounding-box centre, never a screen-space centroid, never a camera-facing
// proxy. Those are derived products of the truth, and steering the truth by one
// of them is exactly the loop this project forbids.
//
// Rotation is applied through the EXISTING exact-transform convention and no
// second one is invented: the ring for an axis accumulates a signed angle and
// adds it to that axis's Euler component, which is what the Property Inspector
// reads and writes. Model = T * Rz * Ry * Rx (see forgeshape_transform.h), so
// the Z ring is a true world-Z rotation always, and the X and Y rings are
// world-true whenever the components outside them are zero. That is the MVP's
// stated limit, not an accident, and it is why there is no local-axis mode: a
// gizmo that claimed local axes would have to invent a second Euler order to
// deliver them.
//
// Screen-constant size
// --------------------
// The gizmo is anchored in 3D and SIZED in screen units, so it stays reachable
// at any zoom instead of becoming microscopic when the camera pulls back or
// swallowing the viewport when it dollies in. One uniform world scale is derived
// from the camera and the pivot's depth, and BOTH the drawing and the hit test
// use that same scale — which is what keeps what the user sees and what the
// finger can grab from ever disagreeing.
#pragma once

#include <cstdint>

#include "forgeshape_camera.h"
#include "forgeshape_display.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_object_id.h"
#include "forgeshape_picking.h"
#include "forgeshape_scene.h"
#include "forgeshape_transform.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// What a gizmo is, in two closed enums
// ---------------------------------------------------------------------------

// Exactly two modes. Scale is deliberately absent: a scale handle that wrote a
// primitive parameter would be a Construction shape edit wearing a transform
// gesture, and one that wrote a transform would need a scale the transform does
// not have. Neither is this stage's to decide.
enum class GizmoMode {
    Move,
    Rotate,
};

constexpr int kGizmoModeCount = 2;

const char* gizmoModeName(GizmoMode mode);
bool gizmoModeFromIndex(int index, GizmoMode* out);
int gizmoModeIndex(GizmoMode mode);

// Which world axis a handle is, or None for "no handle".
//
// There is no Free, no XY/XZ/YZ plane and no Screen entry, because there is no
// such handle: a control that cannot be drawn must not be nameable.
enum class GizmoAxis {
    None,
    X,
    Y,
    Z,
};

const char* gizmoAxisName(GizmoAxis axis);

// The unit world direction of an axis. None is the zero vector, which every
// caller treats as "no handle" rather than as a direction.
Vec3 gizmoAxisDirection(GizmoAxis axis);

// Which Euler component of a TransformValues this axis names, written as a
// small accessor pair rather than an index so no caller has to know the field
// order. Position and rotation share the axis enum because they share the axes.
Degrees transformRotationForAxis(const TransformValues& values, GizmoAxis axis);
void setTransformRotationForAxis(TransformValues* values, GizmoAxis axis, Degrees value);
Meters transformPositionForAxis(const TransformValues& values, GizmoAxis axis);
void setTransformPositionForAxis(TransformValues* values, GizmoAxis axis, Meters value);

// ---------------------------------------------------------------------------
// Size, in reference units
// ---------------------------------------------------------------------------
//
// A REFERENCE UNIT is one device-independent length — what Android calls a dp
// and what another platform would call a point. The domain sizes the gizmo in
// them; the platform adapter is the only thing that knows how many physical
// pixels one is on the screen in front of the user, and pushes that in.
//
// Sizing here rather than in the shell is what makes the 48-unit hit floor a
// property of the product instead of a property of one layout file.

// How long an axis shaft is, from the pivot to the tip of its arrowhead.
constexpr float kGizmoHandleLengthUnits = 96.0f;

// The ring radius for Rotate. Slightly inside the Move shaft length so the two
// modes read as the same instrument at the same size rather than as two.
constexpr float kGizmoRingRadiusUnits = 78.0f;

// Half the effective hit corridor around a shaft or an arc, so the full target
// is 48 units across — the interactive floor, reached with hit area rather than
// by drawing a 48-unit-thick line. The stroke stays thin; the target does not.
constexpr float kGizmoHitSlopUnits = 24.0f;

// Where a shaft's grabbable span starts, as a fraction of its length. The inner
// quarter is excluded on purpose: all three shafts converge at the pivot, so a
// touch there is a coin toss between axes rather than a choice of one.
constexpr float kGizmoShaftGrabStartFraction = 0.25f;

// And where it ends — past 1.0 so the arrowhead itself is inside the corridor.
constexpr float kGizmoShaftGrabEndFraction = 1.12f;

// The pivot marker's arm length, and the arrowhead's, both as fractions of the
// shaft. Drawing only; nothing hit-tests against them separately.
constexpr float kGizmoPivotMarkerFraction = 0.10f;
constexpr float kGizmoArrowLengthFraction = 0.20f;
constexpr float kGizmoArrowHalfWidthFraction = 0.075f;

// How many segments a ring is drawn and hit-tested with. Even, so the ring is
// symmetric about every axis plane; large enough that the polyline is visually a
// circle at the sizes above, and small enough that hit-testing three of them per
// touch is arithmetic rather than work.
constexpr int kGizmoRingSegments = 64;

// The adapter's one number: physical pixels per reference unit. Process-scoped
// because the viewport is, and pushed again whenever the window's display
// changes. Out-of-range or non-finite input is refused rather than clamped into
// a silently wrong scale, and the default below is a mid-density phone so a
// process that never pushes one is still usable rather than invisible.
constexpr float kGizmoDefaultPixelsPerReferenceUnit = 2.75f;
constexpr float kGizmoMinPixelsPerReferenceUnit = 0.25f;
constexpr float kGizmoMaxPixelsPerReferenceUnit = 16.0f;

bool setGizmoPixelsPerReferenceUnit(float scale);
float gizmoPixelsPerReferenceUnit();

// ---------------------------------------------------------------------------
// Projection helpers
// ---------------------------------------------------------------------------

// Projects a world point to view-local pixels — the exact inverse of the
// convention buildPickRay consumes: origin top-left, Y increasing downward.
//
// Returns false, leaving `out` untouched, for a non-positive viewport, a point
// behind or on the perspective eye plane, or any non-finite result.
bool projectWorldToScreen(const CameraSnapshot& camera, const Vec3& world, int viewportWidth,
                          int viewportHeight, float* outX, float* outY);

// How many world meters one pixel spans at `worldPoint`'s depth.
//
// In Perspective this depends on the point's distance from the eye; in
// Orthographic it is the same everywhere and the point is ignored except for
// its finiteness. Both come out of the camera's own projection matrix rather
// than from a second copy of the field of view, so this can never drift from
// what is drawn. Returns false for a degenerate camera or viewport.
bool worldMetersPerPixel(const CameraSnapshot& camera, const Vec3& worldPoint,
                         int viewportHeight, float* out);

// The one uniform world scale the gizmo is drawn and hit-tested at: the world
// length of one reference unit at the pivot's depth.
//
// Everything else is a multiple of this. Returns false when the camera cannot
// produce one, and callers treat that as "no gizmo this frame" rather than
// substituting a guess.
bool gizmoWorldScale(const CameraSnapshot& camera, const Vec3& pivot, int viewportHeight,
                     float* out);

// ---------------------------------------------------------------------------
// The drag solvers
// ---------------------------------------------------------------------------

// How an axis parameter was obtained, so a caller can log it and a test can
// assert that the degeneracy path was actually the one taken.
enum class AxisSolveStatus {
    // The ordinary case: closest approach between the pick ray and the infinite
    // axis line.
    Resolved,
    // The ray was too near parallel to the axis for that to be well conditioned,
    // so the intersection with the camera-facing plane THROUGH the axis was used
    // instead and projected back onto the axis.
    ResolvedByPlane,
    // The axis points too nearly at the viewer for either method to mean
    // anything. Nothing is written and the drag holds its last good value: a
    // guess here is what produces the jump this status exists to prevent.
    Unresolvable,
};

const char* axisSolveStatusName(AxisSolveStatus status);

// Below this the closest-approach denominator (1 - cos^2 between ray and axis)
// is too small to trust, and the plane fallback takes over. 0.02 is an angle of
// about 8 degrees between the ray and the axis.
constexpr float kGizmoAxisParallelDenominator = 0.02f;

// And below this the fallback plane is edge-on to the ray as well, which is the
// genuinely unresolvable case.
constexpr float kGizmoPlaneParallelEpsilon = 1e-3f;

// Solves for the parameter t along the line pivot + axis * t that the pointer
// ray indicates. Writes nothing and reports Unresolvable when the geometry
// cannot answer.
AxisSolveStatus solveAxisParameter(const Ray& ray, const Vec3& pivot, const Vec3& axis,
                                   float* outT);

// Intersects a ray with the plane through `planePoint` whose normal is
// `planeNormal` (need not be unit). Returns false when the ray is parallel to
// the plane, when the hit is behind the ray, or when anything is non-finite.
bool intersectRayPlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal,
                       Vec3* outHit);

// The signed angle from `from` to `to` measured about `axis`, by the right-hand
// rule, in radians and in (-pi, pi].
//
// atan2 of the cross product's axial component against the dot product, rather
// than an acos: acos loses all precision near 0 and pi and cannot produce a
// sign at all. Returns false when either vector is degenerate.
bool signedAngleAround(const Vec3& axis, const Vec3& from, const Vec3& to, float* outRadians);

// Wraps a raw angular difference into (-pi, pi].
//
// This is what makes a continuous drag continuous: consecutive samples are
// always less than half a turn apart at any sane frame rate, so the wrapped
// difference is the real motion and crossing +/-180 degrees is not a jump. It is
// also what lets a drag accumulate past a full turn — the accumulator is never
// itself reduced.
float unwrapAngleDelta(float radians);

// Below this the ring plane is too edge-on to the pointer ray to intersect
// meaningfully, and the drag holds its last good sample instead.
constexpr float kGizmoRingEdgeOnEpsilon = 0.02f;

// And this is how close to the pivot a ring-plane hit may land before the angle
// it implies is noise rather than a direction, as a fraction of the ring radius.
constexpr float kGizmoRingMinRadiusFraction = 0.08f;

// ---------------------------------------------------------------------------
// The quantization seam
// ---------------------------------------------------------------------------
//
// ONE place where a raw constrained delta becomes the applied one. Grid Snap is
// not this stage's to design — there is no approved contract for a translational
// increment, its relation to the display unit, or a rotational one — and there
// is deliberately no setting, no indicator and no hidden snapping here.
//
// What exists is the seam, so that when a snap contract IS approved it lands in
// two functions rather than in the gesture architecture. Both are the identity
// today, and a test asserts that they are.
Meters quantizeGizmoTranslation(Meters raw);
Degrees quantizeGizmoRotation(Degrees raw);

// ---------------------------------------------------------------------------
// What the renderer is handed
// ---------------------------------------------------------------------------

// Everything needed to draw the gizmo, and nothing that could be read back as
// truth: no ObjectId, no dimension, no primitive parameter. The renderer cannot
// learn which body this is, and must not.
struct GizmoSnapshot {
    bool visible = false;
    GizmoMode mode = GizmoMode::Move;
    // The handle currently held, or None. Drawn stronger; the others dimmer.
    GizmoAxis activeAxis = GizmoAxis::None;
    // World-space pivot: the active body's authoritative placement origin.
    Vec3 pivot{0.0f, 0.0f, 0.0f};
    // World length of ONE reference unit at that pivot. The renderer scales the
    // canonical gizmo by kGizmoHandleLengthUnits * this, so it and the hit test
    // are the same size by construction.
    float worldPerReferenceUnit = 0.0f;
};

// ---------------------------------------------------------------------------
// The drawn geometry
// ---------------------------------------------------------------------------
//
// Authored ONCE, in a canonical space whose unit IS the reference unit, and
// never regenerated: the pivot and the camera-derived scale are a matrix, not a
// buffer rewrite, so moving a body or dollying the camera re-uploads nothing.
//
// It is a line list for the same reason the grid is: an axis handle and a ring
// are lines, and giving them solid geometry would put a second kind of surface
// in a renderer whose one surface pipeline exists for Construction Bodies.

// One end of one line. `axis` carries a GizmoAxis's numeric value (1 X, 2 Y,
// 3 Z) as a float because it travels as a vertex attribute. The COLOUR is not
// baked in — see shaders/gizmo.vert.
struct GizmoVertex {
    float position[3];
    float axis;
};

// The buffer holds both modes back to back, so switching Move to Rotate changes
// which RANGE is drawn and nothing else: no upload, no reallocation, no
// pipeline change.
//
// A stroke is drawn as a BUNDLE of parallel lines rather than as one wide one.
//
// Vulkan's `lineWidth` above 1.0 requires the `wideLines` device feature, which
// ForgeShape does not request and must not start requesting to draw a handle.
// A single one-pixel line is legible but thin — and on a body sitting at the
// world origin it lands exactly on the grid's own axis line, where a hairline
// tool and a hairline reference are hard to tell apart. Four lines offset a
// reference unit around the stroke's own direction merge into a band that reads
// as an instrument at every density, and cost nothing but vertices in a buffer
// that is uploaded once.
//
// The offset is in the gizmo's own space, so a shaft pointing at the viewer
// thins as it foreshortens. That is the correct behaviour and not a defect: it
// is exactly the orientation in which the shaft is nearly invisible anyway, and
// the drag solver refuses it for the same reason.
constexpr float kGizmoStrokeOffsetUnits = 1.1f;
constexpr int kGizmoStrokeBundle = 5;  // the centre line plus four offsets

// Move: three pivot-marker arms (3 lines), three bundled shafts (3 * 5), three
// arrowheads of four lines each (12).
constexpr int kGizmoMoveLineCount = 3 + 3 * kGizmoStrokeBundle + 12;
constexpr int kGizmoMoveVertexCount = 2 * kGizmoMoveLineCount;
// Rotate: three rings of kGizmoRingSegments segments, each drawn twice — once at
// the radius and once a stroke offset outside it, for the same legibility reason
// the shafts are bundled. A full bundle per ring would be four times the
// vertices for an arc that is already long and easy to see.
constexpr int kGizmoRotateLineCount = 2 * 3 * kGizmoRingSegments;
constexpr int kGizmoRotateVertexCount = 2 * kGizmoRotateLineCount;
constexpr int kGizmoVertexCount = kGizmoMoveVertexCount + kGizmoRotateVertexCount;

// Where each mode's vertices begin, so the caller draws a range rather than
// deciding an offset from arithmetic of its own.
constexpr int kGizmoMoveFirstVertex = 0;
constexpr int kGizmoRotateFirstVertex = kGizmoMoveVertexCount;

// Fills `out` with kGizmoVertexCount vertices, Move's range first. Pure,
// deterministic and allocation-free; returns how many were written so a caller
// sizing a buffer from the constant and a caller reading the result cannot
// disagree.
int generateGizmoVertices(GizmoVertex* out, int capacity);

// A point on one ring, relative to the pivot, that lies on THAT ring and on no
// other one.
//
// The three rings genuinely intersect: the X ring (the YZ plane) and the Z ring
// (the XY plane) both pass through the +Y direction, and so on for all three
// pairs. A touch at such a crossing is ambiguous by geometry, and the hit test
// resolves it deterministically — X before Y before Z on an exact tie — rather
// than by iteration order being read as significance.
//
// Anything that needs to name a ring UNAMBIGUOUSLY therefore has to stay away
// from the crossings, and this is the one definition of where. It is 45 degrees
// between the ring plane's own two basis directions, which is as far from both
// crossings as a point on the ring can be.
Vec3 gizmoRingGrabOffset(GizmoAxis axis, float radius);

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------
//
// Viewport TOOL semantics, not a theme change: these are the same class of
// value as the grid's axis colours and are owned here for the same reason —
// nothing above JNI authors them, and the three approved UI appearance palettes
// are untouched by them.
//
// Axis identity never rests on brightness alone. Each axis has its own hue AND
// its own geometry — a shaft that points one way, a ring that lies in one plane
// — so the three remain distinguishable to a reader who cannot separate the
// hues at all.
void gizmoAxisColor(ViewportBackground background, GizmoAxis axis, float* outRgba);

// How strongly a handle is drawn while ANOTHER one is held, and while it is the
// one held. Nothing is dimmed at rest: a resting gizmo is one instrument, not
// one bright axis and two faded ones.
constexpr float kGizmoIdleAxisAlphaScale = 0.30f;
constexpr float kGizmoHeldAxisAlphaScale = 1.0f;

// ---------------------------------------------------------------------------
// The session
// ---------------------------------------------------------------------------

// THE owner of gizmo interaction.
//
// Bound to one scene and one history by reference, exactly as ConstructionHistory
// is bound to a scene, so a self-test can drive a whole drag against objects of
// its own and never depend on what a live session left behind.
//
// Not internally synchronised: callers hold the one existing state mutex, the
// same as the scene and the history it sits on.
class GizmoSession {
public:
    GizmoSession(ConstructionScene& scene, ConstructionHistory& history)
        : scene_(scene), history_(history) {}

    GizmoSession(const GizmoSession&) = delete;
    GizmoSession& operator=(const GizmoSession&) = delete;

    // -----------------------------------------------------------------------
    // Context: whether there is a gizmo at all
    // -----------------------------------------------------------------------

    // Set by the shell from mode and Tool Rail context: Construction, Transform,
    // and a body to act on. Turning it off cancels any drag in progress, because
    // a captured handle whose gizmo has gone cannot be released by the user.
    void setActive(bool active);
    bool active() const { return active_; }

    // Move or Rotate. Presentation state: it mints no revision, publishes no
    // geometry and records no history — switching is not an edit. Refused while
    // a drag is captured, so a mode cannot change under a moving finger.
    bool setMode(GizmoMode mode);
    GizmoMode mode() const { return mode_; }

    // What the renderer draws this frame. Invisible when inactive, when the
    // scene has no active body, or when the camera cannot produce a scale.
    GizmoSnapshot snapshot(const CameraSnapshot& camera, int viewportWidth,
                           int viewportHeight) const;

    // -----------------------------------------------------------------------
    // Hit testing
    // -----------------------------------------------------------------------

    // Which handle, if any, a pointer at this pixel would grab. Pure: it starts
    // nothing, captures nothing and mutates nothing, which is what lets the
    // input arbitration ask "is this gesture mine?" before deciding.
    GizmoAxis hitTest(const CameraSnapshot& camera, float screenX, float screenY,
                      int viewportWidth, int viewportHeight) const;

    // -----------------------------------------------------------------------
    // One drag = one pointer = one transaction = one history step
    // -----------------------------------------------------------------------

    // Captures `pointerId` on whichever handle is under the pixel, opens ONE
    // Construction edit, and remembers the body, the axis, the pivot and the
    // transform to go back to. Returns false — capturing nothing and opening
    // nothing — when no handle is there.
    bool beginDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight);

    // Moves the captured handle. An ordinary authoritative transform update: no
    // history entry, no second begin, no geometry publication, and the renderer
    // and the picker see the new placement immediately.
    //
    // Returns false for a pointer that is not the captured one, for a sample the
    // solver cannot resolve, and for a sample that changes nothing.
    bool updateDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX, float screenY,
                    int viewportWidth, int viewportHeight);

    // Closes the drag and records at most one step. Returns true when a step was
    // actually recorded, which is false for a tap and for a drag that came back
    // to where it started — the commit compares Construction state rather than
    // trusting that a gesture happened.
    bool commitDrag();

    // Closes the drag by putting the pre-drag placement back. Records nothing
    // and leaves the redo stack alone. This is what a second finger, an
    // ACTION_CANCEL and a lost selection all end in.
    void cancelDrag();

    bool capturing() const { return capturing_; }
    int32_t capturedPointerId() const { return pointerId_; }
    GizmoAxis capturedAxis() const { return axis_; }
    ObjectId capturedObjectId() const { return objectId_; }

    // Introspection for logging and self-tests. How many updates the current or
    // last drag actually applied, and how the last one was solved — which is how
    // "twelve hundred moves are still one step" and "the degeneracy path ran"
    // become checkable rather than asserted.
    uint64_t dragUpdateCount() const { return dragUpdates_; }

    // How many drags have COMMITTED a history step over the life of the
    // session. Monotone, so the Android shell can tell "the model moved under
    // the finger" from "the camera orbited" with one comparison — and refresh
    // the exact-value editors only when there is something new to show, rather
    // than discarding a half-typed draft after every viewport gesture.
    uint64_t committedDragCount() const { return committedDrags_; }
    AxisSolveStatus lastSolveStatus() const { return lastSolve_; }

private:
    // The pivot and transform of the body a drag is bound to, or false when
    // that body is no longer in the scene.
    bool capturedBodyTransform(TransformValues* out) const;

    bool applyMoveSample(const Ray& ray);
    bool applyRotateSample(const Ray& ray);

    ConstructionScene& scene_;
    ConstructionHistory& history_;

    bool active_ = false;
    // Move on entry, always. A tool that reopened in whatever sub-mode it was
    // last left in would make the first drag after a context switch a guess.
    GizmoMode mode_ = GizmoMode::Move;

    // --- capture state, valid only while capturing_ -------------------------
    bool capturing_ = false;
    // The ONE pointer this drag follows, by stable id and never by index. A
    // second finger is not a second chance to steer it; it cancels the drag.
    int32_t pointerId_ = -1;
    GizmoAxis axis_ = GizmoAxis::None;
    // Which body was grabbed. Checked on every update: a drag can only ever move
    // the body it started on, whatever the selection does underneath it.
    ObjectId objectId_ = kNoObject;
    GizmoMode dragMode_ = GizmoMode::Move;
    Vec3 pivot_{0.0f, 0.0f, 0.0f};
    TransformValues startValues_{};
    // Move: the axis parameter under the finger when it went down.
    float startAxisT_ = 0.0f;
    // Rotate: the last raw ring angle, and the unwrapped total since the down.
    // The total is deliberately NOT reduced modulo a turn — a drag past 360
    // degrees is a real thing the user did, and the exact-value convention keeps
    // what they did rather than canonicalising it.
    float lastRingAngle_ = 0.0f;
    float accumulatedAngle_ = 0.0f;
    bool haveRingSample_ = false;

    uint64_t dragUpdates_ = 0;
    uint64_t committedDrags_ = 0;
    AxisSolveStatus lastSolve_ = AxisSolveStatus::Resolved;
};

// The one process-scoped session, over the one process-scoped scene and history.
GizmoSession& gizmoSession();

}  // namespace forgeshape
