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

// Reduces to (-360, 360) before converting. This is a DERIVED-MATH step only:
// the authoritative degree values are never rewritten, so 370 stays 370 in
// domain state and only its trigonometry is taken at 10.
double radiansFromDegrees(Degrees degrees) {
    return std::fmod(degrees, 360.0) * kDegreesToRadians;
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

}  // namespace

const char* transformValidationName(TransformValidation why) {
    switch (why) {
        case TransformValidation::Ok: return "ok";
        case TransformValidation::NotFinite: return "not_finite";
        case TransformValidation::NotRepresentable: return "not_representable";
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

TransformUpdateStatus ConstructionTransform::setValues(const TransformValues& requested,
                                                       TransformValidation* outWhy) {
    // Validate all six BEFORE writing anything, so a bad rotation cannot leave a
    // half-applied position behind.
    const double ordered[6] = {requested.positionX, requested.positionY, requested.positionZ,
                               requested.rotationX, requested.rotationY, requested.rotationZ};
    TransformValidation why = TransformValidation::Ok;
    for (double value : ordered) {
        why = validateTransformValue(value);
        if (why != TransformValidation::Ok) {
            break;
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
        requested.rotationY == values_.rotationY && requested.rotationZ == values_.rotationZ) {
        return TransformUpdateStatus::Unchanged;
    }

    values_ = requested;
    ++updateCount_;
    return TransformUpdateStatus::Applied;
}

// Model = T * Rz * Ry * Rx  (column vectors: Rx acts on the object first).
Mat4 ConstructionTransform::modelMatrix() const {
    const Mat4 rx = rotationMatrixX(values_.rotationX);
    const Mat4 ry = rotationMatrixY(values_.rotationY);
    const Mat4 rz = rotationMatrixZ(values_.rotationZ);
    const Mat4 t = mat4Translation(positionVec(values_, 1.0));
    return mat4Multiply(t, mat4Multiply(rz, mat4Multiply(ry, rx)));
}

// Model^-1 = Rx(-x) * Ry(-y) * Rz(-z) * T(-p), the exact rigid inverse: each
// factor is inverted and the order reversed. Built from the authoritative values
// rather than by inverting the matrix numerically, so it cannot drift away from
// the model it is supposed to undo.
Mat4 ConstructionTransform::inverseModelMatrix() const {
    const Mat4 rx = rotationMatrixX(-values_.rotationX);
    const Mat4 ry = rotationMatrixY(-values_.rotationY);
    const Mat4 rz = rotationMatrixZ(-values_.rotationZ);
    const Mat4 t = mat4Translation(positionVec(values_, -1.0));
    return mat4Multiply(rx, mat4Multiply(ry, mat4Multiply(rz, t)));
}

bool ConstructionTransform::isIdentity() const {
    return values_.positionX == 0.0 && values_.positionY == 0.0 && values_.positionZ == 0.0 &&
           values_.rotationX == 0.0 && values_.rotationY == 0.0 && values_.rotationZ == 0.0;
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
