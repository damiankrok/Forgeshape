#include "forgeshape_transform.h"

#include <cmath>

// Only for constructionObject(): since Stage 010 the process-scoped transform is
// not a separate singleton, it is the active Construction object's own placement.
// The header stays free of this dependency, so the transform type itself remains
// usable on its own (and is, in the self-tests).
#include "forgeshape_construction.h"

namespace forgeshape {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kRadiansToDegrees = 180.0 / kPi;
constexpr double kFullTurnDegrees = 360.0;

// Reduces to (-360, 360) before converting. This is a DERIVED-MATH step only:
// the authoritative degree values are never rewritten, so 370 stays 370 in
// domain state and only its trigonometry is taken at 10.
double radiansFromDegrees(Degrees degrees) {
    return std::fmod(degrees, kFullTurnDegrees) * kDegreesToRadians;
}

// Trigonometry is done in double and only the finished matrix entry is narrowed
// to float, so the derived matrix is as accurate as a float matrix can be.
Mat4 rotationMatrixX(Degrees degrees) {
    const double r = radiansFromDegrees(degrees);
    const double c = std::cos(r);
    const double s = std::sin(r);
    Mat4 m = mat4Identity();
    m.m[5] = static_cast<float>(c);
    m.m[6] = static_cast<float>(s);
    m.m[9] = static_cast<float>(-s);
    m.m[10] = static_cast<float>(c);
    return m;
}

Mat4 rotationMatrixY(Degrees degrees) {
    const double r = radiansFromDegrees(degrees);
    const double c = std::cos(r);
    const double s = std::sin(r);
    Mat4 m = mat4Identity();
    m.m[0] = static_cast<float>(c);
    m.m[2] = static_cast<float>(-s);
    m.m[8] = static_cast<float>(s);
    m.m[10] = static_cast<float>(c);
    return m;
}

Mat4 rotationMatrixZ(Degrees degrees) {
    const double r = radiansFromDegrees(degrees);
    const double c = std::cos(r);
    const double s = std::sin(r);
    Mat4 m = mat4Identity();
    m.m[0] = static_cast<float>(c);
    m.m[1] = static_cast<float>(s);
    m.m[4] = static_cast<float>(-s);
    m.m[5] = static_cast<float>(c);
    return m;
}

Vec3 positionVec(const TransformValues& v, double sign) {
    return Vec3{static_cast<float>(v.positionX * sign), static_cast<float>(v.positionY * sign),
                static_cast<float>(v.positionZ * sign)};
}

// A diagonal scale, and its exact inverse. The inverse is built by reciprocal
// rather than by numeric inversion for the same reason the rigid inverse is
// built factor by factor: it cannot drift away from the matrix it undoes.
Mat4 scaleMatrix(const TransformValues& v) {
    Mat4 m = mat4Identity();
    m.m[0] = static_cast<float>(v.scaleX);
    m.m[5] = static_cast<float>(v.scaleY);
    m.m[10] = static_cast<float>(v.scaleZ);
    return m;
}

Mat4 inverseScaleMatrix(const TransformValues& v) {
    Mat4 m = mat4Identity();
    // Every factor is validated strictly above kMinScaleFactor before it is
    // stored, so none of these divisions can be by zero.
    m.m[0] = static_cast<float>(1.0 / v.scaleX);
    m.m[5] = static_cast<float>(1.0 / v.scaleY);
    m.m[10] = static_cast<float>(1.0 / v.scaleZ);
    return m;
}

// Reads element (row, col) out of the column-major storage, so the decomposition
// below can be written the way the algebra is written.
inline double at(const Mat4& m, int row, int col) {
    return static_cast<double>(m.m[col * 4 + row]);
}

// Shifts `value` by whole turns until it lands within half a turn of `near`.
//
// This is the whole of the continuity story for a component. Two consecutive
// samples of a drag are far less than half a turn apart, so the nearest
// representative IS the one the user reached — and because the shift is applied
// to the DECOMPOSED value rather than to an accumulator, a drag that has already
// passed 400 degrees keeps counting up instead of wrapping back to 40.
double nearestTurn(double value, double near) {
    const double turns = std::floor((near - value) / kFullTurnDegrees + 0.5);
    return value + turns * kFullTurnDegrees;
}

// Below this a component is treated as NOT having moved, and the previous value
// is returned verbatim.
//
// This is not rounding a real change away. A decomposition arrives through
// atan2 and a float matrix, so a component the drag never touched comes back as
// a few times 1e-14 rather than as the zero it started at — and the exact-value
// editors would then show a body the user rotated about world Y as carrying a
// rotation of -0.00000000000006 about X. A billionth of a degree is well below
// anything a gesture can express and far above the residue a matrix round trip
// leaves, so the two are cleanly separable.
constexpr double kEulerStickyDegrees = 1e-9;

double stickyNearestTurn(double value, double near) {
    const double snapped = nearestTurn(value, near);
    return (std::fabs(snapped - near) < kEulerStickyDegrees) ? near : snapped;
}

// How far a whole orientation is from `previous`, once every component has been
// shifted into its window. Used only to choose between the two equivalent
// branches, so the sum of absolute differences is exactly the right measure:
// the branch that moves the fewest fields the least is the one that reads as
// "the body turned a little" rather than "all three numbers changed".
double branchDistance(const EulerDegrees& candidate, const EulerDegrees& previous) {
    return std::fabs(candidate.x - previous.x) + std::fabs(candidate.y - previous.y) +
           std::fabs(candidate.z - previous.z);
}

EulerDegrees snapBranchToPrevious(double x, double y, double z, const EulerDegrees& previous) {
    EulerDegrees out;
    out.x = stickyNearestTurn(x, previous.x);
    out.y = stickyNearestTurn(y, previous.y);
    out.z = stickyNearestTurn(z, previous.z);
    return out;
}

}  // namespace

const char* transformValidationName(TransformValidation why) {
    switch (why) {
        case TransformValidation::Ok: return "ok";
        case TransformValidation::NotFinite: return "not_finite";
        case TransformValidation::NotRepresentable: return "not_representable";
        case TransformValidation::NotPositive: return "not_positive";
    }
    return "unknown";
}

const char* transformUpdateStatusName(TransformUpdateStatus status) {
    switch (status) {
        case TransformUpdateStatus::Applied: return "applied";
        case TransformUpdateStatus::Unchanged: return "unchanged";
        case TransformUpdateStatus::Rejected: return "rejected";
    }
    return "unknown";
}

TransformValidation validateTransformValue(double value) {
    if (!std::isfinite(value)) {
        return TransformValidation::NotFinite;
    }
    // Zero and negative are ordinary coordinates and ordinary angles: unlike a
    // dimension, there is nothing to refuse about them. The only remaining
    // requirement is that the value survives into the derived float matrix.
    if (!std::isfinite(static_cast<float>(value))) {
        return TransformValidation::NotRepresentable;
    }
    return TransformValidation::Ok;
}

TransformValidation validateScaleValue(double value) {
    const TransformValidation shared = validateTransformValue(value);
    if (shared != TransformValidation::Ok) {
        return shared;
    }
    // The one positivity rule on this transform. Zero would make the model
    // matrix singular; negative would be a Mirror, which is not this product
    // feature and would silently invert winding order everywhere.
    if (!(value > kMinScaleFactor)) {
        return TransformValidation::NotPositive;
    }
    // And the reciprocal has to survive into the float inverse as well, which a
    // finite but enormous factor would not.
    if (!std::isfinite(static_cast<float>(1.0 / value))) {
        return TransformValidation::NotRepresentable;
    }
    return TransformValidation::Ok;
}

TransformUpdateStatus ConstructionTransform::setValues(const TransformValues& requested,
                                                       TransformValidation* outWhy) {
    // Validate all nine BEFORE writing anything, so a bad scale cannot leave a
    // half-applied position behind.
    const double placement[6] = {requested.positionX, requested.positionY, requested.positionZ,
                                 requested.rotationX, requested.rotationY, requested.rotationZ};
    TransformValidation why = TransformValidation::Ok;
    for (double value : placement) {
        why = validateTransformValue(value);
        if (why != TransformValidation::Ok) {
            break;
        }
    }
    if (why == TransformValidation::Ok) {
        const double scales[3] = {requested.scaleX, requested.scaleY, requested.scaleZ};
        for (double value : scales) {
            why = validateScaleValue(value);
            if (why != TransformValidation::Ok) {
                break;
            }
        }
    }
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != TransformValidation::Ok) {
        ++rejectedUpdates_;
        return TransformUpdateStatus::Rejected;  // every previous value stands
    }

    if (requested.positionX == values_.positionX && requested.positionY == values_.positionY &&
        requested.positionZ == values_.positionZ && requested.rotationX == values_.rotationX &&
        requested.rotationY == values_.rotationY && requested.rotationZ == values_.rotationZ &&
        requested.scaleX == values_.scaleX && requested.scaleY == values_.scaleY &&
        requested.scaleZ == values_.scaleZ) {
        return TransformUpdateStatus::Unchanged;
    }

    values_ = requested;
    ++updateCount_;
    return TransformUpdateStatus::Applied;
}

// ---------------------------------------------------------------------------
// Orientation as a matrix, and back again
// ---------------------------------------------------------------------------

Mat4 rotationMatrixFromEuler(const EulerDegrees& euler) {
    const Mat4 rx = rotationMatrixX(euler.x);
    const Mat4 ry = rotationMatrixY(euler.y);
    const Mat4 rz = rotationMatrixZ(euler.z);
    return mat4Multiply(rz, mat4Multiply(ry, rx));
}

Mat4 elementaryRotationMatrix(int axisIndex, Degrees degrees) {
    switch (axisIndex) {
        case 0: return rotationMatrixX(degrees);
        case 1: return rotationMatrixY(degrees);
        case 2: return rotationMatrixZ(degrees);
        default: break;
    }
    return mat4Identity();
}

// R = Rz(c) * Ry(b) * Rx(a) expands to
//
//     [  cc cb    cc sb sa - sc ca    cc sb ca + sc sa ]
//     [  sc cb    sc sb sa + cc ca    sc sb ca - cc sa ]
//     [ -sb       cb sa               cb ca            ]
//
// so away from the singularity  b = asin(-R20),  a = atan2(R21, R22)  and
// c = atan2(R10, R00). Everything else in this function is about WHICH of the
// infinitely many equivalent triples to return; see the header.
bool eulerFromRotationMatrix(const Mat4& rotation, const EulerDegrees& previous,
                             EulerDegrees* out) {
    if (out == nullptr || !mat4Finite(rotation) || !std::isfinite(previous.x) ||
        !std::isfinite(previous.y) || !std::isfinite(previous.z)) {
        return false;
    }

    double sinPitch = -at(rotation, 2, 0);
    // Numerical drift can push this a hair outside [-1, 1], where asin is NaN.
    if (sinPitch > 1.0) sinPitch = 1.0;
    if (sinPitch < -1.0) sinPitch = -1.0;
    const double cosPitch = std::sqrt(1.0 - sinPitch * sinPitch);

    if (cosPitch < kEulerGimbalEpsilon) {
        // Gimbal lock. Only the SUM or the DIFFERENCE of the outer two angles is
        // determined by the matrix, so one of them has to be chosen, and holding
        // the previous Z is the choice that moves the fewest numbers: a drag
        // that grazes straight up does not make the Z field jump.
        const double heldPitch = (sinPitch >= 0.0) ? 90.0 : -90.0;
        const double heldYaw = previous.z;
        if (sinPitch >= 0.0) {
            // b = +90 leaves  R01 = sin(a - c),  R02 = cos(a - c).
            const double combined =
                std::atan2(at(rotation, 0, 1), at(rotation, 0, 2)) * kRadiansToDegrees;
            *out = snapBranchToPrevious(heldYaw + combined, heldPitch, heldYaw, previous);
        } else {
            // b = -90 leaves  R01 = -sin(a + c),  R02 = -cos(a + c).
            const double combined =
                std::atan2(-at(rotation, 0, 1), -at(rotation, 0, 2)) * kRadiansToDegrees;
            *out = snapBranchToPrevious(combined - heldYaw, heldPitch, heldYaw, previous);
        }
        return std::isfinite(out->x) && std::isfinite(out->y) && std::isfinite(out->z);
    }

    // The two equivalent branches. They describe the same orientation, and a
    // decomposition that swapped between them mid-drag would flip all three
    // fields for no motion the user made — so both are produced and the one
    // nearer the last accepted answer wins.
    const double pitchA = std::asin(sinPitch) * kRadiansToDegrees;
    const double rollA = std::atan2(at(rotation, 2, 1), at(rotation, 2, 2)) * kRadiansToDegrees;
    const double yawA = std::atan2(at(rotation, 1, 0), at(rotation, 0, 0)) * kRadiansToDegrees;

    const EulerDegrees candidateA = snapBranchToPrevious(rollA, pitchA, yawA, previous);
    const EulerDegrees candidateB =
        snapBranchToPrevious(rollA + 180.0, 180.0 - pitchA, yawA + 180.0, previous);

    const EulerDegrees& chosen = (branchDistance(candidateB, previous) <
                                  branchDistance(candidateA, previous))
                                     ? candidateB
                                     : candidateA;
    if (!std::isfinite(chosen.x) || !std::isfinite(chosen.y) || !std::isfinite(chosen.z)) {
        return false;
    }
    *out = chosen;
    return true;
}

// Model = T * Rz * Ry * Rx * S  (column vectors: S acts on the object first).
Mat4 ConstructionTransform::modelMatrix() const {
    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(values_));
    const Mat4 t = mat4Translation(positionVec(values_, 1.0));
    return mat4Multiply(t, mat4Multiply(rotation, scaleMatrix(values_)));
}

// Model^-1 = S^-1 * Rx(-x) * Ry(-y) * Rz(-z) * T(-p), the exact inverse: each
// factor is inverted and the order reversed. Built from the authoritative values
// rather than by inverting the matrix numerically, so it cannot drift away from
// the model it is supposed to undo.
Mat4 ConstructionTransform::inverseModelMatrix() const {
    const Mat4 rx = rotationMatrixX(-values_.rotationX);
    const Mat4 ry = rotationMatrixY(-values_.rotationY);
    const Mat4 rz = rotationMatrixZ(-values_.rotationZ);
    const Mat4 t = mat4Translation(positionVec(values_, -1.0));
    const Mat4 rigidInverse = mat4Multiply(rx, mat4Multiply(ry, mat4Multiply(rz, t)));
    return mat4Multiply(inverseScaleMatrix(values_), rigidInverse);
}

Mat4 ConstructionTransform::rotationMatrix() const {
    return rotationMatrixFromEuler(eulerOf(values_));
}

// R * S^-1. For M = T * R * S with a diagonal, strictly positive S this IS the
// inverse transpose of the upper-left 3x3: (R S)^-T = R^-T S^-T = R S^-1,
// because R is orthonormal and S is its own transpose. It reduces to R exactly
// when the scale is (1,1,1), which is why an unscaled body shades identically to
// how it always did.
Mat4 ConstructionTransform::normalMatrix() const {
    return mat4Multiply(rotationMatrixFromEuler(eulerOf(values_)), inverseScaleMatrix(values_));
}

bool ConstructionTransform::isIdentity() const {
    return values_.positionX == 0.0 && values_.positionY == 0.0 && values_.positionZ == 0.0 &&
           values_.rotationX == 0.0 && values_.rotationY == 0.0 && values_.rotationZ == 0.0 &&
           isUnscaled();
}

bool ConstructionTransform::isUnscaled() const {
    return values_.scaleX == kDefaultScaleFactor && values_.scaleY == kDefaultScaleFactor &&
           values_.scaleZ == kDefaultScaleFactor;
}

TransformApplyResult applyTransformValues(ConstructionTransform& transform,
                                          const TransformValues& requested) {
    TransformApplyResult result;
    result.status = transform.setValues(requested, &result.validation);
    result.values = transform.values();
    return result;
}

// The placement of the one active Construction object. It is deliberately NOT a
// second singleton: identity, primitive and placement are one object's state, so
// a primitive change cannot lose the placement and nothing can drift.
ConstructionTransform& constructionTransform() {
    return constructionObject().transform();
}

TransformApplyResult applyConstructionTransform(const TransformValues& requested) {
    return applyTransformValues(constructionTransform(), requested);
}

}  // namespace forgeshape
