#include "forgeshape_body_mirror.h"

#include <cmath>

namespace forgeshape {
namespace {

// A world reflection and the local symmetry are both pure sign flips, so the
// product `F * R * Qx` is built directly from R's entries rather than through
// two `mat4Multiply` calls. That is not an optimisation: it keeps every entry
// of the result BIT-IDENTICAL to the corresponding entry of R apart from its
// sign, which is what makes `det(R')` exactly `+det(R)` and stops the
// determinant assertion from measuring rounding instead of algebra.
//
//   * Qx = diag(-1, +1, +1) on the RIGHT negates column 0.
//   * F  = diag with -1 in `reflectedAxis` on the LEFT negates that row.
Mat4 mirroredRotationMatrix(const Mat4& rotation, int reflectedAxis) {
    Mat4 result = rotation;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            const bool flip = (col == 0) != (row == reflectedAxis);
            if (flip) {
                result.m[col * 4 + row] = -rotation.m[col * 4 + row];
            }
        }
    }
    return result;
}

// The reflected coordinate. A component that reflects to zero is written as
// +0.0: negating 0.0 yields -0.0, which compares equal but does not ENCODE
// equal, and a body standing exactly on the mirror plane must not produce a
// `.forge` file that differs from one typed to the same place.
double reflectedCoordinate(double value) {
    return value == 0.0 ? 0.0 : -value;
}

double positionComponent(const TransformValues& values, int axis) {
    switch (axis) {
        case 0: return values.positionX;
        case 1: return values.positionY;
        default: return values.positionZ;
    }
}

void setPositionComponent(TransformValues* values, int axis, double value) {
    switch (axis) {
        case 0: values->positionX = value; break;
        case 1: values->positionY = value; break;
        default: values->positionZ = value; break;
    }
}

bool placementIsFinite(const TransformValues& v) {
    const double all[9] = {v.positionX, v.positionY, v.positionZ,
                           v.rotationX, v.rotationY, v.rotationZ,
                           v.scaleX,    v.scaleY,    v.scaleZ};
    for (double value : all) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

}  // namespace

const char* mirrorPlaneName(MirrorPlane plane) {
    switch (plane) {
        case MirrorPlane::Xy: return "XY";
        case MirrorPlane::Xz: return "XZ";
        case MirrorPlane::Yz: return "YZ";
    }
    return "unknown";
}

int mirrorReflectedAxis(MirrorPlane plane) {
    // THE mapping, stated once. A plane is named by the two axes it CONTAINS,
    // so the axis it reflects is the third one.
    switch (plane) {
        case MirrorPlane::Xy: return 2;
        case MirrorPlane::Xz: return 1;
        case MirrorPlane::Yz: return 0;
    }
    return -1;
}

bool mirrorPlaneFromIndex(int index, MirrorPlane* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = MirrorPlane::Xy; return true;
        case 1: *out = MirrorPlane::Xz; return true;
        case 2: *out = MirrorPlane::Yz; return true;
        default: return false;
    }
}

int mirrorPlaneIndex(MirrorPlane plane) {
    switch (plane) {
        case MirrorPlane::Xy: return 0;
        case MirrorPlane::Xz: return 1;
        case MirrorPlane::Yz: return 2;
    }
    return -1;
}

const char* mirrorStatusName(MirrorStatus status) {
    switch (status) {
        case MirrorStatus::Ok: return "Ok";
        case MirrorStatus::InvalidPlane: return "InvalidPlane";
        case MirrorStatus::NotFinite: return "NotFinite";
        case MirrorStatus::NotRepresentable: return "NotRepresentable";
    }
    return "unknown";
}

MirrorStatus mirrorPlacement(const TransformValues& source, MirrorPlane plane,
                             TransformValues* out) {
    if (out == nullptr) {
        return MirrorStatus::NotRepresentable;
    }
    const int axis = mirrorReflectedAxis(plane);
    if (axis < 0) {
        return MirrorStatus::InvalidPlane;
    }
    if (!placementIsFinite(source)) {
        return MirrorStatus::NotFinite;
    }

    // Built into a local and copied out only on success, so a refusal leaves
    // the caller's values untouched even when it passes its own storage in.
    TransformValues mirrored = source;
    setPositionComponent(&mirrored, axis, reflectedCoordinate(positionComponent(source, axis)));

    // The scale is carried across UNCHANGED and stays strictly positive. This
    // is the whole positive-scale contract: the reflection lives in the
    // orientation and never in a sign here.
    mirrored.scaleX = source.scaleX;
    mirrored.scaleY = source.scaleY;
    mirrored.scaleZ = source.scaleZ;

    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(source));
    const Mat4 reflected = mirroredRotationMatrix(rotation, axis);
    EulerDegrees euler{};
    if (!eulerFromRotationMatrix(reflected, eulerOf(source), &euler)) {
        return MirrorStatus::NotRepresentable;
    }
    setEuler(&mirrored, euler);

    // Every one of the nine must survive what the transform itself would ask of
    // it, so a caller never applies a placement that is then rejected.
    const double coordinates[6] = {mirrored.positionX, mirrored.positionY, mirrored.positionZ,
                                   mirrored.rotationX, mirrored.rotationY, mirrored.rotationZ};
    for (double value : coordinates) {
        if (validateTransformValue(value) != TransformValidation::Ok) {
            return MirrorStatus::NotRepresentable;
        }
    }
    const double scales[3] = {mirrored.scaleX, mirrored.scaleY, mirrored.scaleZ};
    for (double value : scales) {
        if (validateScaleValue(value) != TransformValidation::Ok) {
            return MirrorStatus::NotRepresentable;
        }
    }

    *out = mirrored;
    return MirrorStatus::Ok;
}

const char* mirrorEligibilityName(MirrorEligibility eligibility) {
    switch (eligibility) {
        case MirrorEligibility::Eligible: return "Eligible";
        case MirrorEligibility::NotConstruction: return "NotConstruction";
        case MirrorEligibility::NotCad: return "NotCad";
        case MirrorEligibility::HasSculptTruth: return "HasSculptTruth";
        case MirrorEligibility::NoConstructionSource: return "NoConstructionSource";
    }
    return "unknown";
}

MirrorEligibility mirrorEligibilityOf(const SceneObject& body) {
    if (body.isCad()) {
        return MirrorEligibility::NotCad;
    }
    if (!body.hasConstructionSource()) {
        return MirrorEligibility::NotConstruction;
    }
    if (body.frozenSculpt().mesh.frozen()) {
        // CURRENT or RETAINED sculpt truth, one question: a frozen mesh is the
        // body's own geometry whether or not a stroke has landed on it yet, and
        // nothing here may claim a reflection of vertices it does not reflect.
        return MirrorEligibility::HasSculptTruth;
    }
    if (body.constructionOrNull() == nullptr) {
        return MirrorEligibility::NoConstructionSource;
    }
    return MirrorEligibility::Eligible;
}

}  // namespace forgeshape
