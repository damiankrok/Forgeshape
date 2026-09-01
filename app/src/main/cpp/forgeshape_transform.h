// ForgeShape Construction transform — where the body is and how big it is
// drawn, never what it is.
//
// Platform-independent: no JNI, no Android, no Vulkan, no renderer and no UI
// type appears here, and it holds no GPU resource and no mesh.
//
// Source-of-truth split
// ---------------------
//     ConstructionBox        W/H/D  -> the box's LOCAL geometry (a RuntimeMesh)
//     ConstructionTransform  P/R/S  -> where that local geometry sits in the world
//
// These are deliberately separate truths. The RuntimeMesh is generated from the
// dimensions alone and is expressed in OBJECT SPACE; the transform never touches
// it. That is what makes a move, a rotate AND A SCALE cost no mesh revision, no
// vertex rewrite and no GPU upload: only a derived 4x4 matrix changes.
//
// Unit contract
// -------------
//   position: double METERS, named ...Meters
//   rotation: double DEGREES, named ...Degrees
//   scale:    double, UNITLESS multiplier, named ...Factor
//
// Degrees are the authoritative rotation unit because that is what the product
// exposes; radians exist only inside the derived trigonometry below.
//
// Scale is a pure multiplier and deliberately has NO unit and NO display unit.
// It is not a dimension: a 2 m box at scale 2 is drawn 4 m across and its
// Construction parameter is still 2 m. That separation is the whole reason a
// scale may live on the transform at all — it moves a derived matrix and never
// a primitive parameter, so the Construction Source is untouched by it.
//
// Scope: exactly one transform for exactly one body. No shear, no parent, no
// hierarchy, no scene graph, and NO MIRROR: scale is strictly positive, so a
// zero or negative factor is refused rather than silently inverting winding
// order and making every normal and every front-face test wrong.
#pragma once

#include <cstdint>

#include "forgeshape_math.h"

namespace forgeshape {

// The authoritative transform units.
using Meters = double;   // also declared by the Construction box; same meaning
using Degrees = double;
using ScaleFactor = double;  // unitless multiplier; see the file comment

// ---------------------------------------------------------------------------
// Axis and Euler convention  (THE definition; renderer and picking both obey it)
// ---------------------------------------------------------------------------
//
// Right-handed world space with +Y up, column-vector math (p' = M * p).
//
// A positive angle rotates by the RIGHT-HAND RULE about its axis: looking down
// the axis toward the origin, positive is counter-clockwise.
//
// Rotations compose in LOCAL X -> Y -> Z order, which for column vectors is
// written right to left, and the scale sits INSIDE all of them:
//
//     Model = T * Rz * Ry * Rx * S
//
// so S acts on the object first, then Rx, and the translation last. Scale is
// therefore LOCAL / OBJECT space — it stretches the body along the body's own
// axes, which is the only meaning that survives a rotation without inventing a
// shear, and is why the product offers no world-space scale. The inverse is
//
//     Model^-1 = S^-1 * Rx(-x) * Ry(-y) * Rz(-z) * T(-p)
//
// which is exact: every factor inverted and the order reversed. It is what
// picking uses to carry a world ray into object space. There is no second
// convention anywhere in ForgeShape: the renderer consumes `modelMatrix()` and
// the picker consumes `inverseModelMatrix()`, both produced here.
//
// Normals need a THIRD derived matrix. For M = T * R * S the correct normal
// transform is the inverse transpose of the upper-left 3x3, which for a
// diagonal, strictly positive S reduces exactly to R * S^-1 — see
// `normalMatrix()`. It equals R when the scale is (1,1,1), which is why nothing
// about an unscaled body changes.

constexpr Meters kDefaultPositionMeters = 0.0;
constexpr Degrees kDefaultRotationDegrees = 0.0;

// A body is born at its true size, so the default multiplier is one.
constexpr ScaleFactor kDefaultScaleFactor = 1.0;

// The smallest scale a body may hold.
//
// It exists so the transform can never become singular: at zero the model
// matrix has no inverse, picking has no local ray and the normal matrix divides
// by zero. This is a REFUSAL threshold and not a rounding rule — a request at or
// below it leaves every previous value standing rather than being clamped into
// a size the user did not ask for.
constexpr ScaleFactor kMinScaleFactor = 1e-6;

// Why a requested transform value was refused. Unlike a dimension, a coordinate
// has no positivity requirement: zero and negative are ordinary, valid places
// and ordinary, valid angles. A SCALE is the one exception on this transform.
enum class TransformValidation {
    Ok,
    NotFinite,        // NaN or infinity
    NotRepresentable, // finite as a double, but the derived float matrix is not
    NotPositive,      // a scale at or below kMinScaleFactor — zero, or a mirror
};

const char* transformValidationName(TransformValidation why);

enum class TransformUpdateStatus {
    Applied,    // the values changed
    Unchanged,  // valid but identical; nothing was written
    Rejected,   // invalid; every previous value is fully preserved
};

const char* transformUpdateStatusName(TransformUpdateStatus status);

// True when this single value is usable. Position and rotation share the rule
// because the only real constraint is that the derived float matrix stays
// finite; there is no arbitrary product limit on either.
TransformValidation validateTransformValue(double value);

// A scale must pass everything above AND stay strictly above kMinScaleFactor —
// see the no-Mirror note at the top of this file.
TransformValidation validateScaleValue(double value);

// The nine authoritative numbers.
struct TransformValues {
    Meters positionX = kDefaultPositionMeters;
    Meters positionY = kDefaultPositionMeters;
    Meters positionZ = kDefaultPositionMeters;
    Degrees rotationX = kDefaultRotationDegrees;
    Degrees rotationY = kDefaultRotationDegrees;
    Degrees rotationZ = kDefaultRotationDegrees;
    ScaleFactor scaleX = kDefaultScaleFactor;
    ScaleFactor scaleY = kDefaultScaleFactor;
    ScaleFactor scaleZ = kDefaultScaleFactor;
};

// ---------------------------------------------------------------------------
// Orientation as a matrix, and back again
// ---------------------------------------------------------------------------
//
// The gizmo composes world and local rotations as MATRICES — the only way to
// say "turn this about the world X axis" that stays true for a body already
// turned about two other axes — while the authoritative truth is still three
// Euler degrees. These functions are the ONE bridge between the two forms, so
// the convention above is written as code in exactly one place and a
// decomposition branch is chosen in exactly one place.

// The three Euler angles on their own, in authoritative degrees.
//
// A named type rather than three loose doubles because the helpers below take
// and return a whole orientation, and a mix-up between three same-typed
// arguments is precisely the defect a name prevents.
struct EulerDegrees {
    Degrees x = kDefaultRotationDegrees;
    Degrees y = kDefaultRotationDegrees;
    Degrees z = kDefaultRotationDegrees;
};

inline EulerDegrees eulerOf(const TransformValues& values) {
    return EulerDegrees{values.rotationX, values.rotationY, values.rotationZ};
}

inline void setEuler(TransformValues* values, const EulerDegrees& euler) {
    if (values == nullptr) return;
    values->rotationX = euler.x;
    values->rotationY = euler.y;
    values->rotationZ = euler.z;
}

// R = Rz * Ry * Rx, exactly as the convention above states. Rotation only: no
// translation and no scale, so it is orthonormal and is its own
// inverse-transpose.
Mat4 rotationMatrixFromEuler(const EulerDegrees& euler);

// One elementary rotation about a WORLD basis axis. `axisIndex` is 0 X, 1 Y,
// 2 Z; anything else yields the identity.
Mat4 elementaryRotationMatrix(int axisIndex, Degrees degrees);

// Decomposes a pure rotation matrix back into the authoritative Euler degrees,
// choosing the representation NEAREST `previous`.
//
// Why "nearest" and not "principal"
// ---------------------------------
// A ZYX Euler triple is not unique, and two facts make the naive answer wrong
// for a drag:
//
//   * every component is periodic, so a body the user has spun to 400 degrees
//     would come back as 40 and the next sample would look like a 360 jump; and
//   * (x, y, z) and (x + 180, 180 - y, z + 180) name the SAME orientation, so a
//     decomposition free to swap branches mid-drag flips all three fields at
//     once for no motion the user made.
//
// So both branches are generated, every component is shifted by whole turns to
// land within half a turn of `previous`, and the closer branch wins. That is
// what makes a continuous drag continuous, keeps a drag past 360 — and past 720
// — accumulating instead of wrapping, and keeps the exact-value editors
// readable while a handle is held.
//
// Near +/- 90 degrees of pitch the X and Z components are genuinely not
// separable: the classic gimbal degeneracy. There the Z component is HELD at
// whatever `previous` had and the whole of the remaining turn is put into X.
// That is deterministic, finite and orientation-correct — the matrix it
// rebuilds is the matrix it was given, which is the only property that can be
// asserted at a singularity.
//
// Returns false, writing nothing, for a non-finite matrix or a non-finite
// `previous`. A caller that gets false holds its last good value.
bool eulerFromRotationMatrix(const Mat4& rotation, const EulerDegrees& previous,
                             EulerDegrees* out);

// Half a turn either way, in degrees: the window a component is shifted into
// around `previous`. Named because more than one file asserts against it.
constexpr Degrees kEulerBranchWindowDegrees = 180.0;

// |cos(pitch)| below this is treated as gimbal lock. That is about 0.006 of a
// degree from straight up or down — far enough inside the singularity that the
// ordinary branch stays in charge for every viewpoint a user can actually hold,
// and wide enough that the ordinary atan2 is never asked to resolve two
// vanishing components against each other.
constexpr double kEulerGimbalEpsilon = 1e-4;

// The exact placement of the Construction body.
//
// Rotation values are stored EXACTLY AS GIVEN and are never canonicalized: 370
// degrees stays 370, and -90 stays -90. Reducing modulo 360 happens only inside
// the derived trigonometry, so what the user typed is what comes back out, and
// re-applying the same numbers is reliably Unchanged.
class ConstructionTransform {
public:
    TransformValues values() const { return values_; }

    Meters positionXMeters() const { return values_.positionX; }
    Meters positionYMeters() const { return values_.positionY; }
    Meters positionZMeters() const { return values_.positionZ; }
    Degrees rotationXDegrees() const { return values_.rotationX; }
    Degrees rotationYDegrees() const { return values_.rotationY; }
    Degrees rotationZDegrees() const { return values_.rotationZ; }
    ScaleFactor scaleXFactor() const { return values_.scaleX; }
    ScaleFactor scaleYFactor() const { return values_.scaleY; }
    ScaleFactor scaleZFactor() const { return values_.scaleZ; }

    // How many times the values actually changed. This is introspection for
    // logging and self-tests, and it is emphatically NOT a MeshRevision: a
    // transform change never publishes mesh geometry.
    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // FAILS CLOSED: if any of the nine is invalid, NOTHING is applied — not even
    // the valid ones — and every previous value stands. A request identical to
    // the current state reports Unchanged.
    //
    // `outWhy` receives the reason for the FIRST invalid value found, in
    // position X/Y/Z, rotation X/Y/Z, then scale X/Y/Z order, and Ok otherwise.
    TransformUpdateStatus setValues(const TransformValues& requested,
                                    TransformValidation* outWhy = nullptr);

    // Derived data, recomputed on demand from the authoritative values. Nothing
    // caches a matrix as truth, and no value is ever read back out of one.
    Mat4 modelMatrix() const;
    Mat4 inverseModelMatrix() const;

    // The body ORIENTATION alone: R = Rz * Ry * Rx, with no position and no
    // scale. This is the basis a Local-space gizmo handle points along, and it
    // is deliberately scale-free — a stretched body still has a local X that
    // points one way, and letting the scale into it would make a handle's
    // direction depend on how large the body happens to be.
    Mat4 rotationMatrix() const;

    // The LINEAR part of the placement: L = Rz * Ry * Rx * S, with no
    // translation. Exactly the model matrix with T factored off, so
    // `modelMatrix() == T(position) * localMatrix()` holds by construction
    // rather than by coincidence.
    //
    // It exists because a static interchange export bakes orientation and size
    // into vertices while leaving the pivot at the node (ARCH-OWNER-07), and
    // that split needs a name. Nothing in the renderer or the picker uses it:
    // they still consume `modelMatrix()` and `inverseModelMatrix()`, so this
    // adds a VIEW of the one composition order and never a second one.
    Mat4 localMatrix() const;

    // The matrix a NORMAL is carried by: R * S^-1, which is the inverse
    // transpose of the model's upper-left 3x3 for a diagonal positive scale.
    // Equal to `rotationMatrix()` whenever the scale is (1,1,1).
    //
    // It is NOT normalised. A non-uniform scale changes a normal's length as
    // well as its direction, and the shading stage normalises per fragment
    // anyway; doing it twice would only cost work.
    Mat4 normalMatrix() const;

    // True when the transform is the identity placement — at the origin,
    // unturned and unscaled — which is what lets a log line say plainly whether
    // the body has been touched at all.
    bool isIdentity() const;

    // True when the three scale factors are all exactly one. Separate from
    // isIdentity because the renderer and the picker care about this alone: it
    // is what says a body transform is RIGID and the cheap shortcuts hold.
    bool isUnscaled() const;

private:
    TransformValues values_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// Everything one apply produces, so no caller re-derives it.
struct TransformApplyResult {
    TransformUpdateStatus status = TransformUpdateStatus::Rejected;
    TransformValidation validation = TransformValidation::Ok;
    // The authoritative values AFTER the call. On Rejected these are the
    // previous ones, unchanged.
    TransformValues values{};
};

// THE Construction transform entry point.
//
// One call validates all nine values, updates them atomically, and reports what
// happened. It publishes NO mesh revision and performs NO upload, by
// construction — it cannot, because it has no access to the mesh store.
TransformApplyResult applyTransformValues(ConstructionTransform& transform,
                                          const TransformValues& requested);

// The placement of the ACTIVE body, whichever representation it has.
//
// Not a singleton of its own: it returns the active `SceneObject`'s own
// `transform()` (defined in forgeshape_transform.cpp over the scene), so
// identity and placement are one body's state and an Imported Mesh — which has
// no Construction Source at all — is moved through exactly this entry point.
// Placement therefore survives a Box <-> Cylinder change as well as home/resume
// and swapchain recreation.
ConstructionTransform& constructionTransform();

TransformApplyResult applyConstructionTransform(const TransformValues& requested);

}  // namespace forgeshape
