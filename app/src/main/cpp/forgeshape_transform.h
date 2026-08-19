// ForgeShape Construction transform — where the box is, not what it is.
//
// Platform-independent: no JNI, no Android, no Vulkan, no renderer and no UI
// type appears here, and it holds no GPU resource and no mesh.
//
// Source-of-truth split
// ---------------------
//     ConstructionBox        W/H/D  -> the box's LOCAL geometry (a RuntimeMesh)
//     ConstructionTransform  P/R    -> where that local geometry sits in the world
//
// These are deliberately separate truths. The RuntimeMesh is generated from the
// dimensions alone and is expressed in OBJECT SPACE; the transform never touches
// it. That is what makes a move or a rotate cost no mesh revision, no vertex
// rewrite and no GPU upload: only a derived 4x4 matrix changes.
//
// Unit contract
// -------------
//   position: double METERS, named ...Meters
//   rotation: double DEGREES, named ...Degrees
//
// Degrees are the authoritative rotation unit because that is what the product
// exposes; radians exist only inside the derived trigonometry below.
//
// Scope: exactly one transform for exactly one box. No scale, no shear, no
// parent, no hierarchy, no scene graph.
#pragma once

#include <cstdint>

#include "forgeshape_math.h"

namespace forgeshape {

// The authoritative transform units.
using Meters = double;   // also declared by the Construction box; same meaning
using Degrees = double;

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
// written right to left:
//
//     Model = T * Rz * Ry * Rx
//
// so Rx is applied to the object first and the translation last. The inverse is
// therefore
//
//     Model^-1 = Rx(-x) * Ry(-y) * Rz(-z) * T(-p)
//
// which is exact for a rigid transform and is what picking uses to carry a world
// ray into object space. There is no second convention anywhere in ForgeShape:
// the renderer consumes `modelMatrix()` and the picker consumes
// `inverseModelMatrix()`, both produced here.

constexpr Meters kDefaultPositionMeters = 0.0;
constexpr Degrees kDefaultRotationDegrees = 0.0;

// Why a requested transform value was refused. Unlike a dimension, a coordinate
// has no positivity requirement: zero and negative are ordinary, valid places.
enum class TransformValidation {
    Ok,
    NotFinite,        // NaN or infinity
    NotRepresentable, // finite as a double, but the derived float matrix is not
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

// The six authoritative numbers.
struct TransformValues {
    Meters positionX = kDefaultPositionMeters;
    Meters positionY = kDefaultPositionMeters;
    Meters positionZ = kDefaultPositionMeters;
    Degrees rotationX = kDefaultRotationDegrees;
    Degrees rotationY = kDefaultRotationDegrees;
    Degrees rotationZ = kDefaultRotationDegrees;
};

// The exact placement of the Construction box.
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

    // How many times the values actually changed. This is introspection for
    // logging and self-tests, and it is emphatically NOT a MeshRevision: a
    // transform change never publishes mesh geometry.
    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // FAILS CLOSED: if any of the six is invalid, NOTHING is applied — not even
    // the valid ones — and every previous value stands. A request identical to
    // the current state reports Unchanged.
    //
    // `outWhy` receives the reason for the FIRST invalid value found, in
    // position X/Y/Z then rotation X/Y/Z order, and Ok otherwise.
    TransformUpdateStatus setValues(const TransformValues& requested,
                                    TransformValidation* outWhy = nullptr);

    // Derived data, recomputed on demand from the authoritative values. Nothing
    // caches a matrix as truth, and no value is ever read back out of one.
    Mat4 modelMatrix() const;
    Mat4 inverseModelMatrix() const;

    // True when the transform is the identity placement, which is what lets a
    // log line say plainly whether the box has been moved at all.
    bool isIdentity() const;

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
// One call validates all six values, updates them atomically, and reports what
// happened. It publishes NO mesh revision and performs NO upload, by
// construction — it cannot, because it has no access to the mesh store.
TransformApplyResult applyTransformValues(ConstructionTransform& transform,
                                          const TransformValues& requested);

// The placement of the one active Construction object.
//
// Since Stage 010 this is not a singleton of its own: it returns
// `constructionObject().transform()`, so identity, primitive and placement are
// one object's state. Placement therefore survives a Box <-> Cylinder change as
// well as home/resume and swapchain recreation.
ConstructionTransform& constructionTransform();

TransformApplyResult applyConstructionTransform(const TransformValues& requested);

}  // namespace forgeshape
