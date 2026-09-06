// Body Dimensions and Relative Scale — the exact overall size of a Construction
// Body, and the one solver that resizes it about a chosen side (Stage 020M,
// `UI-OWNER-33B`).
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no renderer, no UI
// type and no filesystem. It owns arithmetic and refusals, nothing else.
//
// No new project truth
// --------------------
// A body's DIMENSION is not stored anywhere and never will be. It is derived,
// on every read, from two things that already exist:
//
//     dimension[a] = unscaledLocalExtent[a] * absoluteScale[a]
//
// where the extent comes from the Construction Source's own parameters and the
// scale is the transform's authoritative, stored, unitless multiplier. Editing
// a dimension therefore writes SCALE (and, for a one-sided anchor, POSITION)
// and nothing else: no primitive parameter moves, no `.forge` field is added,
// no mesh is regenerated and no `MeshRevision` is minted. That is the whole
// reason this stage needs no format change.
//
// Why LOCAL bounds and not a world AABB
// -------------------------------------
// A world axis-aligned bounding box of a turned body reports the size of the
// box AROUND it, so rotating a 2x1x0.5 body would make its "dimensions" change
// while nothing about the body did. A dimension has to be a fact about the
// BODY. So the bounds are the Construction Source's own local-space extents —
// exact, read from the primitive parameters rather than measured off generated
// vertices — and the transform's rotation is not an input to the dimension at
// all. Neither is the camera: nothing here projects, and nothing here may.
//
// Absolute Scale and Relative Scale
// ---------------------------------
// ABSOLUTE Scale is the stored authoritative `TransformValues::scale*`. There is
// exactly one scale vector in this product and this file does not add a second.
//
// RELATIVE Scale is a temporary MULTIPLIER that exists only for the duration of
// one interaction: it opens at (1, 1, 1) every single time, commits as
// `newAbsolute = oldAbsolute * multiplier`, and is never stored, never
// serialized, never in a history step and never in the fingerprint. Nothing
// here holds one — the caller holds the three numbers the user typed and hands
// them over once.
//
// Deliberately NOT here: Directional Scale handles and modes (Stage 020D, still
// blocked), Sculpt dimensions (`SCULPT-DIM-01`, still blocked), Imported Mesh
// and CAD Body dimensions, CAD feature dimensions, snapping and Mirror.
#pragma once

#include "forgeshape_construction.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"
#include "forgeshape_transform.h"

namespace forgeshape {

// The three body axes, as an index into every triple in this file. Local axes:
// 0 is the body's own X, never the world's.
constexpr int kBodyAxisCount = 3;

// Below this an unscaled local extent is DEGENERATE and the axis cannot be
// resized: a plane's local Y is exactly zero, and dividing a target dimension
// by it would be a division by zero rather than a very large scale. It is a
// REFUSAL threshold, not a rounding rule — a degenerate axis keeps its truthful
// zero and no thickness is fabricated for it.
constexpr double kMinLocalExtentMeters = 1e-9;

// The smallest dimension a body may be resized TO. It is the scale floor
// carried up into metres by the smallest usable extent, so a request that would
// land on or under `kMinScaleFactor` is refused here by name rather than being
// refused later as an unexplained scale rejection.
constexpr double kMinDimensionMeters = 1e-9;

// A body's own axis-aligned bounds in LOCAL space, before the transform's scale
// is applied. Min and max rather than a half-extent triple, because the solver
// has to know WHERE the two sides are and not only how far apart they are: the
// six Construction primitives are all centred on their local origin today, and
// a solver that assumed that could not be reused by Stage 020D.
struct LocalBounds {
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;

    double min(int axis) const;
    double max(int axis) const;
    // max - min on that axis, never negative for valid bounds.
    double extent(int axis) const;
};

// True when every value is finite and no max is below its min. Says nothing
// about degeneracy: a plane's zero-thickness Y is VALID and is refused only
// where a resize needs to divide by it.
bool localBoundsValid(const LocalBounds& bounds);

// The EXACT local bounds of a Construction Source's active primitive, read from
// its parameters and never measured off generated vertices — a mesh is derived
// data and reading a dimension back out of one is the loop this project
// forbids. Returns false, writing nothing, for parameters that are not finite.
//
// Every one of the six is centred on the local origin (that is each generator's
// stated contract), so today every result is symmetric; the caller must not
// depend on that, which is why min and max are both reported.
bool constructionLocalBounds(const ConstructionObject& object, LocalBounds* out);

// The body's overall dimensions: the local extents multiplied by the stored
// Absolute Scale. Rotation and position are not read; the camera cannot be.
struct BodyDimensions {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    double axis(int axis) const;
};

BodyDimensions bodyDimensionsOf(const LocalBounds& bounds, const TransformValues& transform);

// Which side of the body stays put while the other moves.
//
// Center is NOT "the bounds centre stays put": it is the OWNER's rule that the
// scale changes and the POSITION does not, which is the pivot-based behaviour
// the transform already has. For today's origin-centred primitives the two
// descriptions coincide; when they cannot, the stated rule is the one that
// holds, because a resize the user did not ask to move must not move the body.
enum class ResizeAnchor {
    NegativeSide,
    Center,
    PositiveSide,
};

const char* resizeAnchorName(ResizeAnchor anchor);
bool resizeAnchorFromIndex(int index, ResizeAnchor* out);
int resizeAnchorIndex(ResizeAnchor anchor);

// Why a resize did not happen. A dimension, unlike a coordinate, is a SIZE: it
// must be finite and strictly positive, and refusals are named rather than
// clamped so a user who typed a nonsense value learns that instead of watching
// the body take a size they never asked for.
enum class ResizeStatus {
    Ok,
    // NaN or infinity in the request, the bounds or the current transform.
    NotFinite,
    // Zero, negative, or under the representable floor.
    NotPositive,
    // The axis index is not 0, 1 or 2.
    InvalidAxis,
    // The body's own unscaled extent on that axis is zero or nearly zero — a
    // plane's thickness. There is no scale that gives it a size, so the request
    // is refused rather than answered with an infinity.
    DegenerateAxis,
    // The solved values are finite but would not survive the transform's own
    // validation. Reported by name so a caller never applies a value the
    // transform is then going to reject.
    NotRepresentable,
};

const char* resizeStatusName(ResizeStatus status);

// ---------------------------------------------------------------------------
// THE shared resize/anchor solver
// ---------------------------------------------------------------------------
//
// One request in, one complete placement out. It is deliberately a pure
// function over values: it takes no scene, no history, no body and no id, so
// Stage 020D's Directional Scale handles can drive exactly this arithmetic from
// a drag without either caller re-deriving it — which is what `UI-OWNER-33B`
// asks for. No renderer math owns this contract, and there is no second
// implementation anywhere.
//
// The anchor arithmetic, stated once
// ----------------------------------
// With `Model = T * Rz * Ry * Rx * S`, a local point p sits at
//
//     P_world = T + R * S * p
//
// Only S[a] changes, so holding a point on axis `a` at local coordinate b
// stationary requires
//
//     T_new = T_old + (R * e_a) * (S_old[a] - S_new[a]) * b
//
// where `R * e_a` is column a of the rotation matrix. That is exact for ANY
// orientation — it composes the correction as a matrix rather than assuming the
// body is unturned — and the correction is independent of which point on the
// face was chosen, which is what makes "the negative side stays where it is" a
// true statement about a whole face rather than about one vertex.
//
// b is `bounds.min(a)` for NegativeSide and `bounds.max(a)` for PositiveSide.
// Center writes no position at all.
struct AxisResizeRequest {
    // The body's current authoritative placement. Read, never written.
    TransformValues current{};
    // The body's own unscaled local bounds.
    LocalBounds bounds{};
    // 0 X, 1 Y, 2 Z — the BODY's local axis.
    int axis = 0;
    // The Absolute Scale this axis is to take. Strictly positive.
    double newScale = 1.0;
    ResizeAnchor anchor = ResizeAnchor::Center;
};

// Everything one solve produces. On anything but Ok, `values` is `current`
// unchanged: a refused solve proposes nothing.
struct AxisResizeSolution {
    ResizeStatus status = ResizeStatus::InvalidAxis;
    TransformValues values{};
};

// Solves for a new Absolute Scale on one axis. Writes no state anywhere.
AxisResizeSolution solveAxisResize(const AxisResizeRequest& request);

// Solves for a TARGET DIMENSION on one axis, in metres, which is the product
// act: `newScale = target / extent`, then the same anchor arithmetic. Separate
// entry point rather than a flag, because "what size is it" and "what
// multiplier does it carry" are two different questions and only one of them
// has a unit.
AxisResizeSolution solveAxisDimension(const TransformValues& current, const LocalBounds& bounds,
                                      int axis, double targetMeters, ResizeAnchor anchor);

// What a Relative Scale commit produces. Same shape as an axis solve, named
// separately because it moves all three axes and never a position.
struct RelativeScaleSolution {
    ResizeStatus status = ResizeStatus::NotFinite;
    TransformValues values{};
};

// Solves a RELATIVE Scale commit: `newAbsolute = oldAbsolute * multiplier`,
// per axis, with the position untouched. Relative Scale is pivot-based in Stage
// 020M and deliberately exposes no one-sided anchor.
//
// Fails closed on all three at once — a request with one bad multiplier changes
// none of them — on exactly the terms `applyTransformValues` already refuses a
// bad ninth value.
RelativeScaleSolution solveRelativeScale(const TransformValues& current, double multiplierX,
                                         double multiplierY, double multiplierZ);

// The multiplier a Relative Scale interaction opens at, EVERY time. Named so
// the shell, the domain and the tests all say the same number, and so a reader
// meets the reset rule where the arithmetic is.
constexpr double kRelativeScaleIdentity = 1.0;

// ---------------------------------------------------------------------------
// The product acts, over the scene and the history
// ---------------------------------------------------------------------------
//
// One act is ONE `ScopedConstructionEdit`, exactly as every Stage 018A object
// command is: one exact dimension edit is one Undo, one Relative Scale Apply is
// one Undo, and a commit that finds nothing different records nothing. Nothing
// here publishes a mesh, mints a `MeshRevision` or moves a sculpt vertex — a
// dimension edit is a transform edit and a transform edit has never done any of
// those.

// Why a body-size act did not happen, when the reason is about the BODY rather
// than about the arithmetic.
enum class BodySizeStatus {
    Ok,
    // Valid, but identical to what the body already had. Nothing was written
    // and no step was recorded.
    Unchanged,
    UnknownBody,
    // Not a Construction Body. Stage 020M is Construction-only: an Imported
    // Mesh, a CAD Body and a Sculpt target are refused BY NAME rather than
    // answered with a guess, and the control is absent for them above JNI.
    RefusedRepresentation,
    // Stage 018A's lock. A locked body may not be MOVED, and a resize moves it.
    RefusedLocked,
    // Stage 020M: a hidden body has no dimension leaders to read, so the mode
    // it would be edited from cannot be entered. Refused without touching the
    // visibility the user set.
    RefusedHidden,
    // A Construction edit is already open.
    RefusedEditInProgress,
    // The arithmetic itself refused; see `resize` for which.
    RefusedGeometry,
};

const char* bodySizeStatusName(BodySizeStatus status);

// What one body-size act produced, so no caller re-derives it.
struct BodySizeResult {
    BodySizeStatus status = BodySizeStatus::UnknownBody;
    // Meaningful when status is RefusedGeometry.
    ResizeStatus resize = ResizeStatus::Ok;
    // The body's placement AFTER the call. On any refusal these are the
    // previous values, unchanged.
    TransformValues values{};
};

// Reads one body's dimensions. A pure read: it opens no edit, records nothing
// and refuses nothing but an unreadable body.
//
// Returns false — writing nothing — for an unknown body, a body that is not a
// Construction Body, or parameters that do not produce valid bounds.
bool sceneBodyDimensions(ObjectId id, const ConstructionScene& scene, LocalBounds* outBounds,
                         BodyDimensions* outDimensions);

// Sets one axis to an exact overall dimension, in metres, as one transaction.
BodySizeResult applyBodyDimension(ObjectId id, int axis, double targetMeters, ResizeAnchor anchor,
                                  ConstructionScene& scene, ConstructionHistory& history);

// Commits a Relative Scale multiplier into the stored Absolute Scale, as one
// transaction. The multiplier itself is not stored: what survives the call is
// the product, and the next interaction opens at (1, 1, 1) again because
// nothing remembers this one.
BodySizeResult applyBodyRelativeScale(ObjectId id, double multiplierX, double multiplierY,
                                      double multiplierZ, ConstructionScene& scene,
                                      ConstructionHistory& history);

// Whether this body can be measured and resized at all, which is what decides
// whether the Dimensions and Relative Scale controls are DRAWN: a Construction
// Body that is visible and unlocked. The domain guards above stay whatever this
// answers — removing a control is not removing a guard.
bool bodySizeEditable(const SceneObject& body);

}  // namespace forgeshape
