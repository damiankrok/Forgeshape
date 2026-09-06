#include "forgeshape_body_dimensions.h"

#include <cmath>

namespace forgeshape {
namespace {

bool finite(double v) { return std::isfinite(v); }

// Column `axis` of R = Rz * Ry * Rx, as doubles.
//
// It goes through `rotationMatrixFromEuler` rather than writing the trigonometry
// again, because that function is the ONE bridge between the Euler truth and a
// matrix (`forgeshape_transform.h`) and a second copy of the convention here is
// exactly how the gizmo, the exporter and this solver would come to disagree
// about what "the body's local X" means. The matrix is float, so the column is
// promoted; the anchor arithmetic below then uses the SAME column for the old
// and the new placement, which is what makes the held face stationary to double
// rounding rather than to the matrix's own precision.
bool rotationColumn(const TransformValues& values, int axis, double* out) {
    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(values));
    for (int row = 0; row < 3; ++row) {
        const double v = static_cast<double>(rotation.m[axis * 4 + row]);
        if (!finite(v)) {
            return false;
        }
        out[row] = v;
    }
    return true;
}

double scaleOf(const TransformValues& values, int axis) {
    switch (axis) {
        case 0: return values.scaleX;
        case 1: return values.scaleY;
        default: return values.scaleZ;
    }
}

void setScale(TransformValues* values, int axis, double scale) {
    switch (axis) {
        case 0: values->scaleX = scale; break;
        case 1: values->scaleY = scale; break;
        default: values->scaleZ = scale; break;
    }
}

bool validAxis(int axis) { return axis >= 0 && axis < kBodyAxisCount; }

// Everything the transform itself would refuse, asked here so a solution is
// never handed back that `applyTransformValues` is then going to reject. The
// order matters only in that the most specific answer wins: a non-finite value
// is never reported as merely not positive.
ResizeStatus validateSolved(const TransformValues& values) {
    const double all[9] = {values.positionX, values.positionY, values.positionZ,
                           values.rotationX, values.rotationY, values.rotationZ,
                           values.scaleX,    values.scaleY,    values.scaleZ};
    for (double v : all) {
        if (!finite(v)) {
            return ResizeStatus::NotFinite;
        }
    }
    for (int axis = 0; axis < kBodyAxisCount; ++axis) {
        if (validateScaleValue(scaleOf(values, axis)) == TransformValidation::NotPositive) {
            return ResizeStatus::NotPositive;
        }
    }
    for (double v : all) {
        if (validateTransformValue(v) == TransformValidation::NotRepresentable) {
            return ResizeStatus::NotRepresentable;
        }
    }
    return ResizeStatus::Ok;
}

}  // namespace

double LocalBounds::min(int axis) const {
    switch (axis) {
        case 0: return minX;
        case 1: return minY;
        default: return minZ;
    }
}

double LocalBounds::max(int axis) const {
    switch (axis) {
        case 0: return maxX;
        case 1: return maxY;
        default: return maxZ;
    }
}

double LocalBounds::extent(int axis) const { return max(axis) - min(axis); }

bool localBoundsValid(const LocalBounds& bounds) {
    for (int axis = 0; axis < kBodyAxisCount; ++axis) {
        if (!finite(bounds.min(axis)) || !finite(bounds.max(axis))) {
            return false;
        }
        if (bounds.max(axis) < bounds.min(axis)) {
            return false;
        }
    }
    return true;
}

bool constructionLocalBounds(const ConstructionObject& object, LocalBounds* out) {
    if (out == nullptr) {
        return false;
    }
    // Half-extents on the body's own X, Y and Z. Every Construction generator
    // states that it is centred on the local origin, so the bounds are
    // symmetric — but the struct carries min AND max so the solver never
    // assumes it, and Stage 020D can reuse it for bounds that are not.
    double hx = 0.0;
    double hy = 0.0;
    double hz = 0.0;
    switch (object.kind()) {
        case PrimitiveKind::Box: {
            const BoxDimensionsMeters d = object.box().dimensionsMeters();
            hx = d.width * 0.5;
            hy = d.height * 0.5;
            hz = d.depth * 0.5;
            break;
        }
        case PrimitiveKind::Cylinder: {
            const CylinderDimensionsMeters d = object.cylinder().dimensionsMeters();
            // 32 radial segments divisible by four, so a generated X/Z bound is
            // EXACTLY the radius (see kPrimitiveRadialSegments) and this is the
            // same number the mesh reaches — derived independently, never read
            // back out of a vertex.
            hx = hz = d.diameter * 0.5;
            hy = d.height * 0.5;
            break;
        }
        case PrimitiveKind::Sphere: {
            const SphereDimensionsMeters d = object.sphere().dimensionsMeters();
            hx = hy = hz = d.diameter * 0.5;
            break;
        }
        case PrimitiveKind::Cone: {
            const ConeDimensionsMeters d = object.cone().dimensionsMeters();
            hx = hz = d.bottomDiameter * 0.5;
            hy = d.height * 0.5;
            break;
        }
        case PrimitiveKind::Capsule: {
            const CapsuleDimensionsMeters d = object.capsule().dimensionsMeters();
            hx = hz = d.diameter * 0.5;
            hy = d.totalHeight * 0.5;
            break;
        }
        case PrimitiveKind::Plane: {
            const PlaneDimensionsMeters d = object.plane().dimensionsMeters();
            hx = d.width * 0.5;
            // A plane is a zero-thickness sheet, and its local Y extent is
            // exactly zero. That is the TRUTHFUL value and it is reported as
            // such: no thickness is fabricated for it, and the resize path
            // refuses that axis by name rather than dividing by it.
            hy = 0.0;
            hz = d.depth * 0.5;
            break;
        }
    }
    if (!finite(hx) || !finite(hy) || !finite(hz) || hx < 0.0 || hy < 0.0 || hz < 0.0) {
        return false;
    }
    out->minX = -hx;
    out->maxX = hx;
    out->minY = -hy;
    out->maxY = hy;
    out->minZ = -hz;
    out->maxZ = hz;
    return true;
}

double BodyDimensions::axis(int axis) const {
    switch (axis) {
        case 0: return x;
        case 1: return y;
        default: return z;
    }
}

BodyDimensions bodyDimensionsOf(const LocalBounds& bounds, const TransformValues& transform) {
    BodyDimensions dimensions;
    dimensions.x = bounds.extent(0) * transform.scaleX;
    dimensions.y = bounds.extent(1) * transform.scaleY;
    dimensions.z = bounds.extent(2) * transform.scaleZ;
    return dimensions;
}

const char* resizeAnchorName(ResizeAnchor anchor) {
    switch (anchor) {
        case ResizeAnchor::NegativeSide: return "NegativeSide";
        case ResizeAnchor::Center: return "Center";
        case ResizeAnchor::PositiveSide: return "PositiveSide";
    }
    return "unknown";
}

bool resizeAnchorFromIndex(int index, ResizeAnchor* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = ResizeAnchor::NegativeSide; return true;
        case 1: *out = ResizeAnchor::Center; return true;
        case 2: *out = ResizeAnchor::PositiveSide; return true;
        default: return false;
    }
}

int resizeAnchorIndex(ResizeAnchor anchor) {
    switch (anchor) {
        case ResizeAnchor::NegativeSide: return 0;
        case ResizeAnchor::Center: return 1;
        case ResizeAnchor::PositiveSide: return 2;
    }
    return 1;
}

const char* resizeStatusName(ResizeStatus status) {
    switch (status) {
        case ResizeStatus::Ok: return "Ok";
        case ResizeStatus::NotFinite: return "NotFinite";
        case ResizeStatus::NotPositive: return "NotPositive";
        case ResizeStatus::InvalidAxis: return "InvalidAxis";
        case ResizeStatus::DegenerateAxis: return "DegenerateAxis";
        case ResizeStatus::NotRepresentable: return "NotRepresentable";
    }
    return "unknown";
}

AxisResizeSolution solveAxisResize(const AxisResizeRequest& request) {
    AxisResizeSolution solution;
    solution.values = request.current;
    if (!validAxis(request.axis)) {
        solution.status = ResizeStatus::InvalidAxis;
        return solution;
    }
    if (!finite(request.newScale) || !localBoundsValid(request.bounds)) {
        solution.status = ResizeStatus::NotFinite;
        return solution;
    }
    const ResizeStatus currentStatus = validateSolved(request.current);
    if (currentStatus != ResizeStatus::Ok) {
        // A body whose stored placement is already unusable is not something a
        // resize may quietly repair: the caller keeps what it has.
        solution.status = currentStatus;
        return solution;
    }
    switch (validateScaleValue(request.newScale)) {
        case TransformValidation::Ok: break;
        case TransformValidation::NotFinite: solution.status = ResizeStatus::NotFinite; return solution;
        case TransformValidation::NotPositive:
            solution.status = ResizeStatus::NotPositive;
            return solution;
        case TransformValidation::NotRepresentable:
            solution.status = ResizeStatus::NotRepresentable;
            return solution;
    }

    const int axis = request.axis;
    const double oldScale = scaleOf(request.current, axis);
    TransformValues next = request.current;
    setScale(&next, axis, request.newScale);

    if (request.anchor != ResizeAnchor::Center) {
        // The one-sided anchor. `b` is the LOCAL coordinate of the face that
        // stays put; the correction carries it through the body's own
        // orientation, which is why this is right for a turned body and a
        // world-AABB answer would not be.
        const double b = request.anchor == ResizeAnchor::NegativeSide ? request.bounds.min(axis)
                                                                     : request.bounds.max(axis);
        double column[3] = {0.0, 0.0, 0.0};
        if (!rotationColumn(request.current, axis, column)) {
            solution.status = ResizeStatus::NotFinite;
            solution.values = request.current;
            return solution;
        }
        const double delta = (oldScale - request.newScale) * b;
        next.positionX += column[0] * delta;
        next.positionY += column[1] * delta;
        next.positionZ += column[2] * delta;
    }
    // Center writes NO position: the OWNER's rule for this anchor is that the
    // scale changes and the placement does not.

    const ResizeStatus validated = validateSolved(next);
    if (validated != ResizeStatus::Ok) {
        solution.status = validated;
        solution.values = request.current;
        return solution;
    }
    solution.status = ResizeStatus::Ok;
    solution.values = next;
    return solution;
}

AxisResizeSolution solveAxisDimension(const TransformValues& current, const LocalBounds& bounds,
                                      int axis, double targetMeters, ResizeAnchor anchor) {
    AxisResizeSolution solution;
    solution.values = current;
    if (!validAxis(axis)) {
        solution.status = ResizeStatus::InvalidAxis;
        return solution;
    }
    if (!finite(targetMeters) || !localBoundsValid(bounds)) {
        solution.status = ResizeStatus::NotFinite;
        return solution;
    }
    if (targetMeters < kMinDimensionMeters) {
        // Zero and negative are refused, never clamped: a body has no size the
        // user did not ask for, and a negative dimension is a Mirror this
        // product does not have.
        solution.status = ResizeStatus::NotPositive;
        return solution;
    }
    const double extent = bounds.extent(axis);
    if (!(extent > kMinLocalExtentMeters)) {
        // A plane's thickness. No scale gives it a size, so it is refused by
        // name rather than divided by.
        solution.status = ResizeStatus::DegenerateAxis;
        return solution;
    }
    AxisResizeRequest request;
    request.current = current;
    request.bounds = bounds;
    request.axis = axis;
    request.newScale = targetMeters / extent;
    request.anchor = anchor;
    return solveAxisResize(request);
}

RelativeScaleSolution solveRelativeScale(const TransformValues& current, double multiplierX,
                                         double multiplierY, double multiplierZ) {
    RelativeScaleSolution solution;
    solution.values = current;
    const double multipliers[3] = {multiplierX, multiplierY, multiplierZ};
    // All three are checked BEFORE any is written: a Relative Scale Apply with
    // one bad field changes none of them, exactly as one bad value refuses all
    // nine on the transform itself.
    for (double m : multipliers) {
        if (!finite(m)) {
            solution.status = ResizeStatus::NotFinite;
            return solution;
        }
        if (m <= 0.0) {
            solution.status = ResizeStatus::NotPositive;
            return solution;
        }
    }
    const ResizeStatus currentStatus = validateSolved(current);
    if (currentStatus != ResizeStatus::Ok) {
        solution.status = currentStatus;
        return solution;
    }
    TransformValues next = current;
    next.scaleX = current.scaleX * multiplierX;
    next.scaleY = current.scaleY * multiplierY;
    next.scaleZ = current.scaleZ * multiplierZ;
    // The POSITION is untouched. Relative Scale is pivot-based in Stage 020M
    // and exposes no one-sided anchor.
    const ResizeStatus validated = validateSolved(next);
    if (validated != ResizeStatus::Ok) {
        solution.status = validated;
        solution.values = current;
        return solution;
    }
    solution.status = ResizeStatus::Ok;
    solution.values = next;
    return solution;
}

const char* bodySizeStatusName(BodySizeStatus status) {
    switch (status) {
        case BodySizeStatus::Ok: return "Ok";
        case BodySizeStatus::Unchanged: return "Unchanged";
        case BodySizeStatus::UnknownBody: return "UnknownBody";
        case BodySizeStatus::RefusedRepresentation: return "RefusedRepresentation";
        case BodySizeStatus::RefusedLocked: return "RefusedLocked";
        case BodySizeStatus::RefusedHidden: return "RefusedHidden";
        case BodySizeStatus::RefusedEditInProgress: return "RefusedEditInProgress";
        case BodySizeStatus::RefusedGeometry: return "RefusedGeometry";
    }
    return "unknown";
}

bool bodySizeEditable(const SceneObject& body) {
    return body.hasConstructionSource() && body.visible() && !body.locked();
}

bool sceneBodyDimensions(ObjectId id, const ConstructionScene& scene, LocalBounds* outBounds,
                         BodyDimensions* outDimensions) {
    const SceneObject* body = scene.findBody(id);
    if (body == nullptr) {
        return false;
    }
    const ConstructionObject* construction = body->constructionOrNull();
    if (construction == nullptr) {
        // Construction-only, by scope. An Imported Mesh and a CAD Body have
        // dimensions a later stage may choose to answer; this one does not
        // invent them.
        return false;
    }
    LocalBounds bounds;
    if (!constructionLocalBounds(*construction, &bounds) || !localBoundsValid(bounds)) {
        return false;
    }
    if (outBounds != nullptr) {
        *outBounds = bounds;
    }
    if (outDimensions != nullptr) {
        *outDimensions = bodyDimensionsOf(bounds, body->transform().values());
    }
    return true;
}

namespace {

// The preamble every body-size act shares, in the order a refusal costs least:
// an open edit, then the body, then what the body IS, then what the user said
// about it. Nothing is opened and nothing is written until all four pass.
BodySizeStatus resolveSizeTarget(ObjectId id, ConstructionScene& scene,
                                 const ConstructionHistory& history, SceneObject** out) {
    if (history.editInProgress()) {
        return BodySizeStatus::RefusedEditInProgress;
    }
    SceneObject* body = scene.findBody(id);
    if (body == nullptr) {
        return BodySizeStatus::UnknownBody;
    }
    if (!body->hasConstructionSource()) {
        return BodySizeStatus::RefusedRepresentation;
    }
    if (body->locked()) {
        // Stage 018A's lock, on its own terms: a locked body may not be MOVED,
        // and a resize moves it.
        return BodySizeStatus::RefusedLocked;
    }
    if (!body->visible()) {
        // Stage 020M. The dimension leaders are read off geometry that is not
        // drawn, so the mode they belong to cannot be entered over a hidden
        // body. The visibility the user set is not touched by the refusal.
        return BodySizeStatus::RefusedHidden;
    }
    *out = body;
    return BodySizeStatus::Ok;
}

// The one write both acts share: apply a solved placement inside ONE
// transaction, and report Unchanged rather than opening an empty step when the
// solved values are the ones the body already had.
BodySizeResult commitSolvedPlacement(SceneObject& body, ConstructionHistory& history,
                                     const TransformValues& solved) {
    BodySizeResult result;
    result.values = body.transform().values();
    if (sameConstructionPlacement(result.values, solved)) {
        result.status = BodySizeStatus::Unchanged;
        return result;
    }
    {
        // One exact dimension edit is one Undo; one Relative Scale Apply is one
        // Undo. Nothing is published: a transform edit mints no MeshRevision,
        // rebuilds no mesh and moves no sculpt vertex, and this is a transform
        // edit however it was expressed.
        ScopedConstructionEdit edit(history);
        applyTransformValues(body.transform(), solved);
    }
    result.status = BodySizeStatus::Ok;
    result.values = body.transform().values();
    return result;
}

}  // namespace

BodySizeResult applyBodyDimension(ObjectId id, int axis, double targetMeters, ResizeAnchor anchor,
                                  ConstructionScene& scene, ConstructionHistory& history) {
    BodySizeResult result;
    SceneObject* body = nullptr;
    const BodySizeStatus resolved = resolveSizeTarget(id, scene, history, &body);
    if (resolved != BodySizeStatus::Ok) {
        result.status = resolved;
        if (body != nullptr) {
            result.values = body->transform().values();
        }
        return result;
    }
    result.values = body->transform().values();
    LocalBounds bounds;
    if (!constructionLocalBounds(*body->constructionOrNull(), &bounds)) {
        result.status = BodySizeStatus::RefusedGeometry;
        result.resize = ResizeStatus::NotFinite;
        return result;
    }
    const AxisResizeSolution solution =
        solveAxisDimension(result.values, bounds, axis, targetMeters, anchor);
    if (solution.status != ResizeStatus::Ok) {
        result.status = BodySizeStatus::RefusedGeometry;
        result.resize = solution.status;
        return result;
    }
    return commitSolvedPlacement(*body, history, solution.values);
}

BodySizeResult applyBodyRelativeScale(ObjectId id, double multiplierX, double multiplierY,
                                      double multiplierZ, ConstructionScene& scene,
                                      ConstructionHistory& history) {
    BodySizeResult result;
    SceneObject* body = nullptr;
    const BodySizeStatus resolved = resolveSizeTarget(id, scene, history, &body);
    if (resolved != BodySizeStatus::Ok) {
        result.status = resolved;
        if (body != nullptr) {
            result.values = body->transform().values();
        }
        return result;
    }
    result.values = body->transform().values();
    const RelativeScaleSolution solution =
        solveRelativeScale(result.values, multiplierX, multiplierY, multiplierZ);
    if (solution.status != ResizeStatus::Ok) {
        result.status = BodySizeStatus::RefusedGeometry;
        result.resize = solution.status;
        return result;
    }
    return commitSolvedPlacement(*body, history, solution.values);
}

}  // namespace forgeshape
