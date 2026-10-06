// The Construction transform gizmo — direct manipulation of a Body placement,
// in the viewport, with the finger or the stylus.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type and
// no UI type appears here. What crosses in is a CameraSnapshot, a viewport size
// in pixels and a platform-neutral pointer position; what crosses out is a
// snapshot the renderer can draw and a transform the Construction domain owns.
//
// What this owns, and what it deliberately does not
// -------------------------------------------------
// It owns: which handle a pointer landed on, which pointer is captured, the
// drag solvers, and the transaction boundary around one drag. It owns NO
// transform of its own — the authoritative Position, Rotation and Scale stay on
// the active body ConstructionTransform throughout, so the renderer, the picker
// and the exact-value editors all read the same numbers mid-drag as they do at
// rest. There is no second solver above JNI and no parallel placement anywhere.
//
// Three modes, two spaces, one pivot
// ----------------------------------
//     Move    axis X/Y/Z and plane XY/XZ/YZ    World or Local
//     Rotate  ring X/Y/Z                       World or Local
//     Scale   axis, plane and uniform          Local only
//
// The pivot is ALWAYS the body authoritative Construction placement origin —
// never a mesh bounding-box centre, never a screen-space centroid, never a
// camera-facing proxy. Those are derived products of the truth, and steering the
// truth by one of them is exactly the loop this project forbids.
//
// WORLD means the world basis X/Y/Z. LOCAL means the body own basis, which is
// the three columns of its rotation matrix — the SCALE is deliberately left out
// of that basis, so a stretched body still has a local X that points one way and
// a handle direction never depends on how large the body happens to be.
//
// Why Scale has no World
// ----------------------
// A world-axis scale of a rotated body is a shear, not a scale: it cannot be
// written as T * R * S for any diagonal S, so it could not be stored in the
// authoritative transform and could not be read back by the exact-value editors.
// The honest answer is to not offer it, and the space selector is ABSENT in
// Scale rather than shown and refused.
//
// Rotation, composed as matrices and stored as Euler degrees
// ----------------------------------------------------------
// A ring drag never adds its angle to one Euler component -- that is only
// correct when the other two are zero, and on a mixed orientation it rotates
// about neither the world axis nor the local one. Each sample composes
//
//     World:  R_target = Relem(A, delta) * R_start
//     Local:  R_target = R_start * Relem(A, delta)
//
// from the IMMUTABLE start orientation and the accumulated angle, then
// decomposes R_target back to Euler degrees through the one branch-continuous
// helper in forgeshape_transform.h. A mixed drag therefore legitimately moves
// more than one Euler field; correctness is a statement about the ORIENTATION.
//
// Screen-constant size
// --------------------
// The gizmo is anchored in 3D and SIZED in screen units, so it stays reachable
// at any zoom instead of becoming microscopic when the camera pulls back or
// swallowing the viewport when it dollies in. One uniform world scale is derived
// from the camera and the pivot depth, and BOTH the drawing and the hit test use
// that same scale — which is what keeps what the user sees and what the finger
// can grab from ever disagreeing. The BODY scale is not part of it: stretching a
// body must not stretch the instrument used to stretch it.
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
// What a gizmo is, in three closed enums
// ---------------------------------------------------------------------------

enum class GizmoMode {
    Move,
    Rotate,
    Scale,
};

constexpr int kGizmoModeCount = 3;

const char* gizmoModeName(GizmoMode mode);
bool gizmoModeFromIndex(int index, GizmoMode* out);
int gizmoModeIndex(GizmoMode mode);

// Which basis a drag is constrained to.
enum class GizmoSpace {
    World,
    Local,
};

constexpr int kGizmoSpaceCount = 2;

const char* gizmoSpaceName(GizmoSpace space);
bool gizmoSpaceFromIndex(int index, GizmoSpace* out);
int gizmoSpaceIndex(GizmoSpace space);

// Which basis axis a handle is ABOUT or COLOURED BY, or None.
//
// This is identity, not geometry: it names the hue and the basis column, and it
// is what a plane handle borrows from the axis perpendicular to it.
enum class GizmoAxis {
    None,
    X,
    Y,
    Z,
};

const char* gizmoAxisName(GizmoAxis axis);

// The unit WORLD direction of an axis. None is the zero vector, which every
// caller treats as "no handle" rather than as a direction. Local-space
// directions do not come from here — they come from a GizmoBasis.
Vec3 gizmoAxisDirection(GizmoAxis axis);

// Which thing a pointer actually grabbed.
//
// Separate from GizmoAxis because a plane handle and a uniform handle are not
// axes and must not be nameable as one: the XY plane is constrained to two basis
// directions and the uniform handle to none at all.
enum class GizmoHandle {
    None,
    AxisX,
    AxisY,
    AxisZ,
    PlaneXY,
    PlaneXZ,
    PlaneYZ,
    Uniform,
};

// The transport encoding, and the ONE place it is written down. 1..3 are the
// three axes and keep the values the axis-only gizmo already reported, so a
// reader of the older code and a reader of this one agree about what a 2 means.
constexpr int kGizmoHandleCodeNone = 0;
constexpr int kGizmoHandleCodeCount = 8;

const char* gizmoHandleName(GizmoHandle handle);
int gizmoHandleCode(GizmoHandle handle);
bool gizmoHandleFromCode(int code, GizmoHandle* out);

bool gizmoHandleIsAxis(GizmoHandle handle);
bool gizmoHandleIsPlane(GizmoHandle handle);

// 0 X, 1 Y, 2 Z for an axis handle; -1 for everything else.
int gizmoHandleAxisIndex(GizmoHandle handle);

// The two basis indices a plane handle moves in, and the one it does not.
// Returns false for a handle that is not a plane.
bool gizmoPlaneAxisIndices(GizmoHandle handle, int* outFirst, int* outSecond);
int gizmoPlaneNormalIndex(GizmoHandle handle);

// The axis whose HUE this handle is drawn in. An axis handle uses its own; a
// plane handle uses the axis PERPENDICULAR to it, which is the convention every
// professional tool draws and is what makes "the blue square" unambiguously the
// XY plane; the uniform handle has no axis and is drawn neutral.
GizmoAxis gizmoHandleColorAxis(GizmoHandle handle);

// Which handles exist in a mode, in hit-test priority order. Fills `out` and
// returns how many were written; `capacity` below the count writes nothing.
constexpr int kGizmoMaxHandles = 7;
int gizmoHandlesForMode(GizmoMode mode, GizmoHandle* out, int capacity);

// ---------------------------------------------------------------------------
// The constrained basis
// ---------------------------------------------------------------------------

// The three unit directions ONE drag is constrained to, frozen when the pointer
// went down.
//
// Frozen, and that is the whole point: in Local space the basis is derived from
// the body own rotation, and a rotate drag changes that rotation continuously.
// A basis re-read every sample would chase its own output and spiral.
struct GizmoBasis {
    Vec3 axis[3];
};

// World gives the world axes; Local gives the columns of the body rotation
// matrix. Scale-free in both cases — see the file comment.
GizmoBasis gizmoBasisFor(GizmoSpace space, const TransformValues& values);

// The basis as a matrix, for the renderer: the three directions as columns, no
// translation and no scale. Identity for World.
Mat4 gizmoBasisMatrix(const GizmoBasis& basis);

// ---------------------------------------------------------------------------
// Reading and writing one component of a TransformValues by basis index
// ---------------------------------------------------------------------------
//
// Written as small accessors rather than an index into the struct so no caller
// has to know the field order, and so a caller cannot reach a position when it
// meant a scale.
Meters transformPositionAt(const TransformValues& values, int axisIndex);
void setTransformPositionAt(TransformValues* values, int axisIndex, Meters value);
ScaleFactor transformScaleAt(const TransformValues& values, int axisIndex);
void setTransformScaleAt(TransformValues* values, int axisIndex, ScaleFactor value);

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

// How long an axis shaft is, from the pivot to the tip of its arrowhead (Move)
// or the far face of its cube (Scale).
constexpr float kGizmoHandleLengthUnits = 96.0f;

// The ring radius for Rotate. Slightly inside the Move shaft length so the two
// modes read as the same instrument at the same size rather than as two.
constexpr float kGizmoRingRadiusUnits = 78.0f;

// Half the effective hit corridor around a shaft or an arc, so the full target
// is 48 units across — the interactive floor, reached with hit area rather than
// by drawing a 48-unit-thick line. The stroke stays thin; the target does not.
constexpr float kGizmoHitSlopUnits = 24.0f;

// Where a shaft grabbable span starts, as a fraction of its length. The inner
// quarter is excluded on purpose: all three shafts converge at the pivot, so a
// touch there is a coin toss between axes rather than a choice of one — and in
// Scale it is where the uniform handle lives.
constexpr float kGizmoShaftGrabStartFraction = 0.25f;

// And where it ends — past 1.0 so the arrowhead or the cube is inside it.
constexpr float kGizmoShaftGrabEndFraction = 1.12f;

// The pivot marker arm length, and the arrowhead, both as fractions of the
// shaft. Drawing only; nothing hit-tests against them separately.
//
// The marker is drawn ONCE, NEUTRAL, and it is not part of any shaft. It used
// to be three arms in the three axis hues added inside the shaft builder, which
// made it read as the roots of the shafts rather than as the point the whole
// transform is about — the one mark on the instrument that belongs to no axis
// looked exactly like the three that do. Neutral, it is legible as itself, and
// it stays inside kGizmoPivotDeadRadiusUnits so what is drawn as "not a handle"
// is also inside the disc that grabs nothing.
//
// In SCALE it is not drawn at all: the uniform cube stands on the same point and
// IS a handle there, and two marks on one point is what made a control
// indistinguishable from a reference mark.
constexpr float kGizmoPivotMarkerFraction = 0.16f;
constexpr float kGizmoArrowLengthFraction = 0.20f;
// Wider than it was (0.075). An arrowhead 9 units across at a shaft 96 long is
// a hairline V; this is still well inside the 24-unit corridor around the shaft,
// so nothing about which handle a touch lands on changes.
constexpr float kGizmoArrowHalfWidthFraction = 0.10f;

// The two-axis plane handle: a square in the plane, spanning these distances
// along each of its two basis directions.
//
// The near edge is deliberately just OUTSIDE the 24-unit axis corridor, so the
// drawn square never overlaps the drawn axis targets. The centre is where the
// handle is grabbed and where a drag reads its reference direction from.
constexpr float kGizmoPlaneInnerUnits = 27.0f;
constexpr float kGizmoPlaneOuterUnits = 53.0f;
constexpr float kGizmoPlaneCentreUnits =
    0.5f * (kGizmoPlaneInnerUnits + kGizmoPlaneOuterUnits);

// The radius, around the projected centre of a plane handle, inside which a
// touch grabs it: 48 units across, the same floor every other handle meets.
constexpr float kGizmoPlaneHitRadiusUnits = 24.0f;

// The uniform-scale handle at the pivot, and the cubes at the ends of the three
// Scale shafts. Half-extents, in reference units.
//
// The uniform cube is deliberately the LARGEST mark on the instrument and is
// drawn as a double outline: it is the one handle that acts on all three axes,
// it sits where every shaft converges, and at 9 units it was the smallest and
// faintest thing in the drawing while carrying the widest consequence. Its hit
// radius is unchanged — see kGizmoUniformHitRadiusUnits — so this is legibility
// and not reach.
constexpr float kGizmoUniformCubeHalfUnits = 12.0f;
constexpr float kGizmoScaleCubeHalfUnits = 7.0f;

// And the radius around the projected PIVOT inside which a touch grabs the
// uniform handle. Also 48 units across.
constexpr float kGizmoUniformHitRadiusUnits = 24.0f;

// The same disc, in MOVE and ROTATE, names no handle at all.
//
// All three shafts converge at the pivot and all three rings pass around it, so
// a touch at the centre is a coin toss between axes rather than a choice of one.
// kGizmoShaftGrabStartFraction alone does not achieve that: it starts the
// grabbable span a quarter of the way out, but the 24-unit corridor AROUND that
// span reaches all the way back to the pivot again, and foreshortening pulls it
// further in. Excluding the disc outright is what makes "the inner quarter is
// not a handle" true rather than intended.
//
// In SCALE the same disc IS a handle — the uniform cube — so the exclusion is
// deliberately per-mode rather than a property of the pivot.
constexpr float kGizmoPivotDeadRadiusUnits = 24.0f;

// How many segments a ring is drawn and hit-tested with. Even, so the ring is
// symmetric about every axis plane; large enough that the polyline is visually a
// circle at the sizes above, and small enough that hit-testing three of them per
// touch is arithmetic rather than work.
constexpr int kGizmoRingSegments = 64;

// The adapter one number: physical pixels per reference unit. Process-scoped
// because the viewport is, and pushed again whenever the window display
// changes. Out-of-range or non-finite input is refused rather than clamped into
// a silently wrong scale, and the default below is a mid-density phone so a
// process that never pushes one is still usable rather than invisible.
constexpr float kGizmoDefaultPixelsPerReferenceUnit = 2.75f;
constexpr float kGizmoMinPixelsPerReferenceUnit = 0.25f;
constexpr float kGizmoMaxPixelsPerReferenceUnit = 16.0f;

bool setGizmoPixelsPerReferenceUnit(float scale);
float gizmoPixelsPerReferenceUnit();

// The VISUAL SIZE preference (UI-PREF-R1 E, UI-OWNER-32): one bounded multiplier
// on how large the instrument is drawn and where its handles stand, in
// reference units. 1.0 is exactly the accepted gizmo.
//
// What it scales is the PLACEMENT of every handle — shaft length, ring radius,
// plane-square distance, cube size — for the drawing AND the hit test alike, so
// what is seen and what can be grabbed still cannot differ. What it deliberately
// does NOT scale is any hit CORRIDOR (kGizmoHitSlopUnits and the three radii
// below stay in reference units, so the 48-unit floor holds at the smallest
// setting) or any drag AMOUNT: the axis and plane solvers are world-space
// arithmetic on the ray, the ring solver is an angle about the pivot, and the
// scale mapping's reference length reads kGizmoHandleLengthUnits at the
// canonical scale — so the same pixel drag moves, turns or stretches a body by
// the same amount at every visual size.
//
// The bounds come from the screen. The FLOOR is set by the two hit radii
// that do not scale: a plane handle is grabbed within 24 units of its centre
// and the pivot's 24-unit dead disc grabs nothing, so a plane centre whose
// PROJECTED distance from the pivot falls under 24 units is unreachable. From
// an oblique three-quarter view that projection is about 0.68 of the centre
// distance (40 units at 1.0 -> 27 projected, a 3-unit margin the accepted gizmo
// already lives with); at 0.9 it is 24.4 and still clear, at 0.75 it is 20 and
// the handle is lost. The CEILING: at 1.5 the Rotate rings are 117 units in
// radius — 234 units across, which still leaves a 360-unit-wide window a margin
// on both sides. Out-of-range and non-finite are REFUSED at the session, never clamped
// into a silently different size; the Android layer clamps its stored value
// before it ever asks (see AppPreferences).
constexpr float kGizmoDefaultVisualScale = 1.0f;
constexpr float kGizmoMinVisualScale = 0.9f;
constexpr float kGizmoMaxVisualScale = 1.5f;

bool gizmoVisualScaleIsValid(float scale);

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

// How many world meters one pixel spans at `worldPoint` depth.
//
// In Perspective this depends on the point distance from the eye; in
// Orthographic it is the same everywhere and the point is ignored except for
// its finiteness. Both come out of the camera own projection matrix rather than
// from a second copy of the field of view, so this can never drift from what is
// drawn. Returns false for a degenerate camera or viewport.
bool worldMetersPerPixel(const CameraSnapshot& camera, const Vec3& worldPoint,
                         int viewportHeight, float* out);

// The one uniform world scale the gizmo is drawn and hit-tested at: the world
// length of one reference unit at the pivot depth.
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
// atan2 of the cross product axial component against the dot product, rather
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
// The scale mapping
// ---------------------------------------------------------------------------
//
// ONE formula for all three scale handles, so there is one thing to describe and
// one thing to test:
//
//     factor = 1 + (pointer - down) . dir / referencePixels
//
// where `dir` is the handle own direction ON SCREEN and `referencePixels` is
// how long that handle is on screen. It is measured from the POINTER DOWN point
// rather than from the pivot, so the factor is exactly 1 at zero drag wherever
// on the handle the user grabbed; it is monotone in the pointer displacement,
// finite, and clamped strictly positive so it can never pass through zero into a
// mirror.
//
// The reference direction is the projected handle direction for an axis, the
// projected in-plane diagonal for a plane, and the screen diagonal (right and
// up) for the uniform handle, which has no direction of its own.

// The smallest factor a drag may produce. A floor, not a rounding rule: pulling
// the pointer past the pivot pins the body at a sliver rather than turning it
// inside out.
constexpr double kGizmoMinScaleFactor = 1e-3;

// How long the uniform handle is treated as being on screen, in reference
// units: one shaft, so a uniform drag and an axis drag of the same pixel length
// scale by roughly the same amount and the tool feels like one instrument.
constexpr float kGizmoUniformScaleReferenceUnits = kGizmoHandleLengthUnits;

// A handle that projects shorter than this has no usable screen direction — it
// is pointing at the viewer — and a scale drag on it is REFUSED at capture
// rather than anchored on a direction that is mostly rounding error.
constexpr float kGizmoScaleMinReferenceUnits = 16.0f;

// ---------------------------------------------------------------------------
// The quantization and placement seam
// ---------------------------------------------------------------------------
//
// ONE place where a solved target becomes the applied one:
//
//     raw pointer -> coordinate constraint (the solvers above)
//                 -> optional placement / snap modifier (HERE)
//                 -> authoritative apply
//
// Surface Snap and Grid Snap are not this stage to design — there is no approved
// contract for a translational increment, its relation to the display unit, a
// rotational one, or what a surface even means for a body that has not been
// picked. What exists is the seam, so that when one IS approved it lands in
// these functions rather than in the gesture architecture.
//
// All four are the identity today, and self-tests assert that they are. There is
// deliberately no setting, no indicator and no hidden snapping anywhere.
Meters quantizeGizmoTranslation(Meters raw);
Degrees quantizeGizmoRotation(Degrees raw);
ScaleFactor quantizeGizmoScale(ScaleFactor raw);

// The whole-placement modifier: the last thing between a solved target and the
// authoritative apply. It receives the solved target, the placement the drag
// started from, and what kind of drag this is, so an approved snap can consult
// all three without any of the solvers learning about it.
TransformValues applyGizmoPlacementModifier(const TransformValues& target,
                                            const TransformValues& start, GizmoMode mode,
                                            GizmoHandle handle, GizmoSpace space);

// ---------------------------------------------------------------------------
// What the renderer is handed
// ---------------------------------------------------------------------------

// Everything needed to draw the gizmo, and nothing that could be read back as
// truth: no ObjectId, no dimension, no primitive parameter, no body scale. The
// renderer cannot learn which body this is, and must not.
struct GizmoSnapshot {
    bool visible = false;
    GizmoMode mode = GizmoMode::Move;
    GizmoSpace space = GizmoSpace::World;
    // The handle currently held, or None. Drawn stronger; the others dimmer.
    GizmoHandle activeHandle = GizmoHandle::None;
    // World-space pivot: the active body authoritative placement origin.
    Vec3 pivot{0.0f, 0.0f, 0.0f};
    // The basis the handles point along, as a rotation matrix. Identity in
    // World; the body orientation in Local. It carries NO scale, so a stretched
    // body does not stretch the instrument.
    Mat4 orientation = mat4Identity();
    // World length of ONE reference unit at that pivot. The renderer scales the
    // canonical gizmo by this, so it and the hit test are the same size by
    // construction.
    float worldPerReferenceUnit = 0.0f;
    // The user's visual size preference, carried beside the camera-derived
    // scale rather than folded into it: the drawing and the hit test multiply
    // the two (gizmoPlacementScale), the scale-drag reference length reads the
    // camera-derived one alone. See kGizmoDefaultVisualScale.
    float visualScale = kGizmoDefaultVisualScale;
};

// Which handle of `state`, if any, a pointer at this pixel would grab -- the
// hit test `GizmoSession::hitTest` runs, as a pure function of a snapshot and
// the camera, so another owner of a placement (a Freeform cage selection,
// `MODELING-FOUNDATIONS-R1` B) draws and grabs the SAME instrument by the same
// rule rather than a second copy of it. See GizmoSession::hitTest for the
// priority tiers.
GizmoHandle gizmoHitTestSnapshot(const GizmoSnapshot& state, const CameraSnapshot& camera,
                                 float screenX, float screenY, int viewportWidth,
                                 int viewportHeight);

// The one world scale a handle is PLACED at — the camera-derived reference unit
// times the visual size preference. Every "where is this handle" answer, drawn
// or hit-tested, goes through this so the two cannot disagree.
float gizmoPlacementScale(const GizmoSnapshot& state);

// ---------------------------------------------------------------------------
// The drawn geometry
// ---------------------------------------------------------------------------
//
// Authored ONCE, in a canonical space whose unit IS the reference unit, and
// never regenerated: the pivot, the basis and the camera-derived scale are a
// matrix, not a buffer rewrite, so moving a body, turning it, switching space or
// dollying the camera re-uploads nothing.
//
// It is a line list for the same reason the grid is: an axis handle, a ring, a
// square and a cube are all lines, and giving them solid geometry would put a
// second kind of surface in a renderer whose one surface pipeline exists for
// Construction Bodies.

// One end of one line.
//
// `axis` carries the COLOUR tag: 0 neutral, 1 X, 2 Y, 3 Z. `handle` carries the
// GizmoHandle code, so the held handle can be drawn stronger without a plane and
// the axis it borrows its hue from being highlighted together. Both travel as
// vertex attributes; the colours themselves do not — see shaders/gizmo.vert.
struct GizmoVertex {
    float position[3];
    float axis;
    float handle;
};

// A stroke is drawn as a BUNDLE of parallel lines rather than as one wide one.
//
// Vulkan `lineWidth` above 1.0 requires the `wideLines` device feature, which
// ForgeShape does not request and must not start requesting to draw a handle.
// A single one-pixel line is legible but thin — and on a body sitting at the
// world origin it lands exactly on the grid own axis line, where a hairline tool
// and a hairline reference are hard to tell apart. Four lines offset a reference
// unit around the stroke own direction merge into a band that reads as an
// instrument at every density, and cost nothing but vertices in a buffer that is
// uploaded once.
//
// The offset is in the gizmo own space, so a shaft pointing at the viewer thins
// as it foreshortens. That is the correct behaviour and not a defect: it is
// exactly the orientation in which the shaft is nearly invisible anyway, and the
// drag solvers refuse it for the same reason.
constexpr float kGizmoStrokeOffsetUnits = 1.1f;
constexpr int kGizmoStrokeBundle = 5;  // the centre line plus four offsets

// The STROKE WEIGHT preference (UI-PREF-R1 F) is a bundle RECIPE, and Regular
// is exactly the recipe the constants above describe — the pre-preference gizmo,
// vertex for vertex. Thin is the same bundle at half the spread. Bold widens the
// shaft bundle to thirteen lines (the + pattern, the x pattern between them and a
// second + one spread further out, which fills the band rather than fanning
// it), bundles the arrowhead strokes, and adds a concentric pass to the rings,
// the plane squares and the Scale cubes. Every recipe is still one-pixel lines:
// no wideLines feature is requested for any weight.
//
// Hit testing never reads a weight. A wider band is still inside the 48-unit
// corridor around the nominal stroke, so what is drawn heavier is grabbed
// exactly where the thinner one was.
struct GizmoStrokeStyle {
    float spread;          // the offset unit between bundled lines
    int shaftBundle;       // lines per shaft: 5 (+ pattern) or 13
    int arrowBundle;       // lines per arrowhead stroke: 1, or 5 when bundled
    int ringPasses;        // concentric ring passes: 2 (r, r+s) or 4 (r-s .. r+2s)
    int squarePasses;      // concentric plane-square passes: 2 or 3
    int axisCubeOutlines;  // outlines per Scale end cube: 1 or 2
    int uniformOutlines;   // outlines of the uniform cube: 2 or 3
};

constexpr GizmoStrokeStyle gizmoStrokeStyle(GizmoStrokeWeight weight) {
    switch (weight) {
        case GizmoStrokeWeight::Thin:
            return GizmoStrokeStyle{0.5f * kGizmoStrokeOffsetUnits, kGizmoStrokeBundle, 1, 2, 2,
                                    1, 2};
        case GizmoStrokeWeight::Bold:
            return GizmoStrokeStyle{kGizmoStrokeOffsetUnits, 13, 5, 4, 3, 2, 3};
        case GizmoStrokeWeight::Regular:
            break;
    }
    return GizmoStrokeStyle{kGizmoStrokeOffsetUnits, kGizmoStrokeBundle, 1, 2, 2, 1, 2};
}

// The widest bundle any weight authors: sizes the offset table in the writer.
constexpr int kGizmoStrokeBundleMax = 13;

// A plane handle square: drawn twice a stroke offset apart for the same
// legibility reason the shafts are bundled, PLUS the two diagonals across it.
//
// The diagonals are what make a plane handle a different KIND of mark from an
// axis handle. Every handle used to be a hollow outline in an axis hue, so a
// square you can drag on a plane and a shaft you can drag along an axis were
// told apart by position alone — and over a body's own faces both read as stray
// selection wireframe. A crossed square reads as a surface.
constexpr int kGizmoPlaneSquareLineCount = 2 * 4 + 2;
constexpr int kGizmoPlaneLineCount = 3 * kGizmoPlaneSquareLineCount;
// A cube is twelve edges.
constexpr int kGizmoCubeLineCount = 12;

// The neutral pivot mark: three arms through the origin, drawn once. See
// kGizmoPivotMarkerFraction.
constexpr int kGizmoPivotMarkLineCount = 3;

// An arrowhead: four spokes back from the tip, and two lines across their ends
// closing it. The cross is what makes the head read as a head rather than as
// two more hairlines leaving the tip.
constexpr int kGizmoArrowLineCount = 6;

// Move: the pivot mark, three bundled shafts, three arrowheads, three plane
// squares.
constexpr int kGizmoMoveLineCount = kGizmoPivotMarkLineCount + 3 * kGizmoStrokeBundle +
                                    3 * kGizmoArrowLineCount + kGizmoPlaneLineCount;
constexpr int kGizmoMoveVertexCount = 2 * kGizmoMoveLineCount;

// Rotate: three rings of kGizmoRingSegments segments, each drawn twice — once at
// the radius and once a stroke offset outside it. A full bundle per ring would
// be four times the vertices for an arc that is already long and easy to see.
// Plus the same pivot mark, which Rotate had none of at all: three rings around
// a point with nothing at the point does not say where the rotation is centred.
constexpr int kGizmoRotateLineCount = 2 * 3 * kGizmoRingSegments + kGizmoPivotMarkLineCount;
constexpr int kGizmoRotateVertexCount = 2 * kGizmoRotateLineCount;

// Scale: the same shafts with a cube at the end of each instead of an
// arrowhead, the three plane squares, and the uniform cube at the pivot drawn
// as a double outline. NO pivot mark — the uniform cube is what stands there.
constexpr int kGizmoScaleLineCount =
    3 * kGizmoStrokeBundle + 3 * kGizmoCubeLineCount + kGizmoPlaneLineCount +
    2 * kGizmoCubeLineCount;
constexpr int kGizmoScaleVertexCount = 2 * kGizmoScaleLineCount;

constexpr int kGizmoVertexCount =
    kGizmoMoveVertexCount + kGizmoRotateVertexCount + kGizmoScaleVertexCount;

// The same counts for ANY weight. The constants above are the Regular answers
// and are kept as the names every existing reader uses; the static_asserts
// below are what make "Regular is the pre-preference gizmo" a compile-time
// fact rather than a comment.
constexpr int gizmoPlaneLineCountFor(const GizmoStrokeStyle& style) {
    return 3 * (4 * style.squarePasses + 2);
}
constexpr int gizmoMoveLineCountFor(GizmoStrokeWeight weight) {
    const GizmoStrokeStyle style = gizmoStrokeStyle(weight);
    return kGizmoPivotMarkLineCount + 3 * style.shaftBundle +
           3 * kGizmoArrowLineCount * style.arrowBundle + gizmoPlaneLineCountFor(style);
}
constexpr int gizmoRotateLineCountFor(GizmoStrokeWeight weight) {
    return gizmoStrokeStyle(weight).ringPasses * 3 * kGizmoRingSegments +
           kGizmoPivotMarkLineCount;
}
constexpr int gizmoScaleLineCountFor(GizmoStrokeWeight weight) {
    const GizmoStrokeStyle style = gizmoStrokeStyle(weight);
    return 3 * style.shaftBundle + 3 * kGizmoCubeLineCount * style.axisCubeOutlines +
           gizmoPlaneLineCountFor(style) + kGizmoCubeLineCount * style.uniformOutlines;
}
constexpr int gizmoVertexCountFor(GizmoStrokeWeight weight) {
    return 2 * (gizmoMoveLineCountFor(weight) + gizmoRotateLineCountFor(weight) +
                gizmoScaleLineCountFor(weight));
}

static_assert(2 * gizmoMoveLineCountFor(GizmoStrokeWeight::Regular) == kGizmoMoveVertexCount,
              "Regular must author exactly the pre-preference Move geometry");
static_assert(2 * gizmoRotateLineCountFor(GizmoStrokeWeight::Regular) == kGizmoRotateVertexCount,
              "Regular must author exactly the pre-preference Rotate geometry");
static_assert(2 * gizmoScaleLineCountFor(GizmoStrokeWeight::Regular) == kGizmoScaleVertexCount,
              "Regular must author exactly the pre-preference Scale geometry");
static_assert(gizmoVertexCountFor(GizmoStrokeWeight::Thin) == kGizmoVertexCount,
              "Thin is the Regular recipe at half spread, so it has the Regular count");

// The largest list any weight produces, which is what the renderer's one buffer
// is sized to so a weight change is a re-upload and never a reallocation.
constexpr int kGizmoVertexCountMax = gizmoVertexCountFor(GizmoStrokeWeight::Bold);
static_assert(kGizmoVertexCountMax >= kGizmoVertexCount, "Bold is the widest recipe");

// Where each mode vertices begin, so the caller draws a range rather than
// deciding an offset from arithmetic of its own.
constexpr int kGizmoMoveFirstVertex = 0;
constexpr int kGizmoRotateFirstVertex = kGizmoMoveVertexCount;
constexpr int kGizmoScaleFirstVertex = kGizmoRotateFirstVertex + kGizmoRotateVertexCount;

// The vertex range one mode draws, in the list one WEIGHT authors — Move then
// Rotate then Scale, so the offsets depend on the weight. Returns false for
// nothing to draw. The two-argument form is the Regular list.
bool gizmoVertexRange(GizmoMode mode, GizmoStrokeWeight weight, int* outFirst, int* outCount);
bool gizmoVertexRange(GizmoMode mode, int* outFirst, int* outCount);

// Fills `out` with gizmoVertexCountFor(weight) vertices, Move then Rotate then
// Scale. Pure, deterministic and allocation-free; returns how many were written
// so a caller sizing a buffer from the constant and a caller reading the result
// cannot disagree, and 0 when `capacity` cannot hold the weight's list. The
// two-argument form is the Regular list and is byte-identical to what the
// product drew before the weight existed.
int generateGizmoVertices(GizmoVertex* out, int capacity, GizmoStrokeWeight weight);
int generateGizmoVertices(GizmoVertex* out, int capacity);

// A point on one ring, relative to the pivot IN CANONICAL GIZMO SPACE, that lies
// on THAT ring and on no other one.
//
// The three rings genuinely intersect: the X ring (the YZ plane) and the Z ring
// (the XY plane) both pass through the +Y direction, and so on for all three
// pairs. A touch at such a crossing is ambiguous by geometry, and the hit test
// resolves it deterministically — X before Y before Z on an exact tie — rather
// than by iteration order being read as significance.
//
// Anything that needs to name a ring UNAMBIGUOUSLY therefore has to stay away
// from the crossings, and this is the one definition of where. It is 45 degrees
// between the ring plane own two basis directions, which is as far from both
// crossings as a point on the ring can be.
Vec3 gizmoRingGrabOffset(GizmoAxis axis, float radius);

// The two basis INDICES spanning the plane perpendicular to an axis index, in a
// fixed order, so a ring and its hit test cannot disagree about which way round
// they are.
void gizmoPerpendicularIndices(int axisIndex, int* outU, int* outV);

// Where a handle is grabbed, in WORLD space: the middle of a shaft grab span,
// a point on a ring away from the crossings, the centre of a plane square, or
// the pivot for the uniform handle.
//
// Derived from the same snapshot the renderer draws and the hit test measures,
// so the ONE definition of "where the handle is" serves drawing, hit testing,
// the scale reference direction and verification alike. A test that hard-coded
// a pixel would be true for one window and one camera only.
//
// Returns false, writing nothing, for an invisible snapshot or GizmoHandle::None.
bool gizmoHandleGrabPoint(const GizmoSnapshot& state, GizmoHandle handle, Vec3* out);

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------
//
// Viewport TOOL semantics, not a theme change: these are the same class of value
// as the grid axis colours and are owned here for the same reason — nothing
// above JNI authors them, and the three approved UI appearance palettes are
// untouched by them.
//
// Axis identity never rests on colour alone. Each axis has its own hue AND its
// own geometry — a shaft that points one way, a ring that lies in one plane, a
// square in one plane, a cube on one shaft — so the three remain distinguishable
// to a reader who cannot separate the hues at all.

// The alpha every handle is drawn at, before the held/idle weight below.
constexpr float kGizmoAxisAlpha = 0.95f;

void gizmoAxisColor(ViewportBackground background, GizmoAxis axis, float* outRgba);

// The colour a HELD handle is drawn in.
//
// One warm hue that NO axis owns, so it can never be mistaken for an axis
// identity, and so "which handle is this" and "is it the one I am holding" stay
// two independent readings. It is stated alongside the weight below rather than
// instead of it: a reader who cannot separate the hue still sees the other
// handles drop away.
void gizmoHighlightColor(ViewportBackground background, float* outRgb);

// The grey a handle with no axis is drawn at — today, the uniform-scale cube.
float gizmoNeutralLevel(ViewportBackground background);

// How strongly a handle is drawn while ANOTHER one is held, and while it is the
// one held. Nothing is dimmed at rest: a resting gizmo is one instrument, not
// one bright handle and six faded ones.
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

    // Move, Rotate or Scale. Presentation state: it mints no revision, publishes
    // no geometry and records no history — switching is not an edit. Refused
    // while a drag is captured, so a mode cannot change under a moving finger.
    //
    // Entering Scale forces Local and REMEMBERS the space the user had; leaving
    // Scale puts that space back, so a round trip through Scale does not quietly
    // change what a Move handle means.
    bool setMode(GizmoMode mode);
    GizmoMode mode() const { return mode_; }

    // World or Local. Presentation state on the same terms as the mode.
    //
    // Refused while capturing, and refused in Scale for anything but Local:
    // a world-axis scale of a rotated body is a shear (see the file comment),
    // and the selector is absent there rather than shown and refused.
    bool setSpace(GizmoSpace space);
    GizmoSpace space() const { return space_; }

    // True when the space is the user choice rather than a consequence of the
    // mode. The shell draws the selector exactly when this is true.
    bool spaceIsSelectable() const { return mode_ != GizmoMode::Scale; }

    // The visual size preference (UI-PREF-R1 E). Presentation on the mode's
    // terms — no revision, no publication, no history — and REFUSED outside
    // [kGizmoMinVisualScale, kGizmoMaxVisualScale] or non-finite, leaving the
    // current value standing. Accepted mid-drag: the captured handle keeps its
    // world-space anchor, so a resize under a held finger moves nothing.
    bool setVisualScale(float scale);
    float visualScale() const { return visualScale_; }

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
    //
    // Resolved in PRIORITY TIERS, smallest target first — the uniform handle,
    // then the plane handles, then the axes — and by nearest projected distance
    // within a tier, X before Y before Z on an exact tie. Without the tiers the
    // three shafts, which are long, would win every contest against the small
    // handles that sit between them.
    GizmoHandle hitTest(const CameraSnapshot& camera, float screenX, float screenY,
                        int viewportWidth, int viewportHeight) const;

    // -----------------------------------------------------------------------
    // One drag = one pointer = one transaction = one history step
    // -----------------------------------------------------------------------

    // Captures `pointerId` on whichever handle is under the pixel, opens ONE
    // Construction edit, and remembers the body, the handle, the pivot, the
    // frozen basis and the transform to go back to. Returns false — capturing
    // nothing and opening nothing — when no handle is there.
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
    GizmoHandle capturedHandle() const { return handle_; }
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

    // The world direction a handle points along, in the frozen basis.
    Vec3 basisDirection(int axisIndex) const;

    // Writes `values` to the captured body through the placement seam and the
    // one authoritative entry point. Returns whether anything actually changed.
    bool applyTarget(const TransformValues& values);

    bool applyMoveSample(const Ray& ray);
    bool applyRotateSample(const Ray& ray);
    bool applyScaleSample(float screenX, float screenY);

    ConstructionScene& scene_;
    ConstructionHistory& history_;

    bool active_ = false;
    // Move on entry, always. A tool that reopened in whatever sub-mode it was
    // last left in would make the first drag after a context switch a guess.
    GizmoMode mode_ = GizmoMode::Move;
    // And World on entry: the space a user has never chosen is the one that
    // needs no explanation.
    GizmoSpace space_ = GizmoSpace::World;
    // The space to go back to when Scale is left. Not a second truth about the
    // current space — it is only ever read at the moment Scale is left.
    GizmoSpace restoreSpace_ = GizmoSpace::World;
    // How large the instrument is drawn and placed, as a multiplier on the
    // reference unit. Bounded; see kGizmoDefaultVisualScale.
    float visualScale_ = kGizmoDefaultVisualScale;

    // --- capture state, valid only while capturing_ -------------------------
    bool capturing_ = false;
    // The ONE pointer this drag follows, by stable id and never by index. A
    // second finger is not a second chance to steer it; it cancels the drag.
    int32_t pointerId_ = -1;
    GizmoHandle handle_ = GizmoHandle::None;
    // Which body was grabbed. Checked on every update: a drag can only ever move
    // the body it started on, whatever the selection does underneath it.
    ObjectId objectId_ = kNoObject;
    GizmoMode dragMode_ = GizmoMode::Move;
    GizmoSpace dragSpace_ = GizmoSpace::World;
    Vec3 pivot_{0.0f, 0.0f, 0.0f};
    TransformValues startValues_{};
    // Frozen at the down. See GizmoBasis: in Local it is derived from a rotation
    // a rotate drag is about to change, so re-reading it would chase itself.
    GizmoBasis basis_{};
    // The start orientation, kept as a matrix so every sample composes from an
    // immutable start rather than from its own previous output.
    Mat4 startRotation_ = mat4Identity();
    // The last Euler triple the decomposition produced and the transform
    // accepted. It is the continuity anchor: the next decomposition picks the
    // branch nearest this one.
    EulerDegrees lastEuler_{};

    // Move, axis: the axis parameter under the finger when it went down.
    float startAxisT_ = 0.0f;
    // Move, plane: the in-plane point under the finger when it went down.
    Vec3 startPlaneHit_{0.0f, 0.0f, 0.0f};

    // Rotate: the world direction the ring turns about, the last raw ring angle,
    // and the unwrapped total since the down. The total is deliberately NOT
    // reduced modulo a turn — a drag past 360 degrees is a real thing the user
    // did, and the exact-value convention keeps what they did rather than
    // canonicalising it.
    Vec3 ringNormal_{0.0f, 1.0f, 0.0f};
    float lastRingAngle_ = 0.0f;
    float accumulatedAngle_ = 0.0f;
    bool haveRingSample_ = false;

    // Scale: where the pointer went down, the handle unit direction on screen,
    // and how long that handle is on screen. See the scale mapping above.
    float scaleDownX_ = 0.0f;
    float scaleDownY_ = 0.0f;
    float scaleDirX_ = 1.0f;
    float scaleDirY_ = 0.0f;
    float scaleReferencePixels_ = 1.0f;

    uint64_t dragUpdates_ = 0;
    uint64_t committedDrags_ = 0;
    AxisSolveStatus lastSolve_ = AxisSolveStatus::Resolved;
};

// The one process-scoped session, over the one process-scoped scene and history.
GizmoSession& gizmoSession();

}  // namespace forgeshape
