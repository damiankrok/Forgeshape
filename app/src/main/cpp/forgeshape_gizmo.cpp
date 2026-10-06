#include "forgeshape_gizmo.h"

#include <atomic>
#include <cmath>

namespace forgeshape {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// Physical pixels per reference unit, pushed by the platform adapter. Atomic
// rather than mutex-guarded because it is one independent float that no domain
// invariant is paired with — the same reasoning the display settings use.
std::atomic<float> g_pixelsPerReferenceUnit{kGizmoDefaultPixelsPerReferenceUnit};

float radiansToDegrees(float radians) { return radians * (180.0f / kPi); }

// Squared distance, in pixels, from a point to a segment. Returns the squared
// distance so no caller pays for a square root it only compares.
float pointSegmentDistanceSq(float px, float py, float ax, float ay, float bx, float by) {
    const float vx = bx - ax;
    const float vy = by - ay;
    const float lengthSq = vx * vx + vy * vy;
    float t = 0.0f;
    if (lengthSq > 0.0f) {
        t = ((px - ax) * vx + (py - ay) * vy) / lengthSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    const float dx = px - (ax + vx * t);
    const float dy = py - (ay + vy * t);
    return dx * dx + dy * dy;
}

Vec3 canonicalAxis(int index) {
    switch (index) {
        case 0: return Vec3{1.0f, 0.0f, 0.0f};
        case 1: return Vec3{0.0f, 1.0f, 0.0f};
        case 2: return Vec3{0.0f, 0.0f, 1.0f};
        default: break;
    }
    return Vec3{0.0f, 0.0f, 0.0f};
}

// The COLOUR tag a vertex carries: 0 neutral, 1 X, 2 Y, 3 Z. See GizmoVertex.
float colorTagFor(GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::X: return 1.0f;
        case GizmoAxis::Y: return 2.0f;
        case GizmoAxis::Z: return 3.0f;
        case GizmoAxis::None: break;
    }
    return 0.0f;
}

GizmoAxis axisFromIndex(int index) {
    switch (index) {
        case 0: return GizmoAxis::X;
        case 1: return GizmoAxis::Y;
        case 2: return GizmoAxis::Z;
        default: break;
    }
    return GizmoAxis::None;
}

GizmoHandle axisHandleFromIndex(int index) {
    switch (index) {
        case 0: return GizmoHandle::AxisX;
        case 1: return GizmoHandle::AxisY;
        case 2: return GizmoHandle::AxisZ;
        default: break;
    }
    return GizmoHandle::None;
}

// The three plane handles in a fixed order, so drawing, hit testing and the
// self-tests all enumerate them the same way.
const GizmoHandle kPlaneHandles[3] = {GizmoHandle::PlaneXY, GizmoHandle::PlaneXZ,
                                      GizmoHandle::PlaneYZ};

}  // namespace

// ---------------------------------------------------------------------------
// The closed enums
// ---------------------------------------------------------------------------

const char* gizmoModeName(GizmoMode mode) {
    switch (mode) {
        case GizmoMode::Move: return "move";
        case GizmoMode::Rotate: return "rotate";
        case GizmoMode::Scale: return "scale";
    }
    return "unknown";
}

bool gizmoModeFromIndex(int index, GizmoMode* out) {
    // Refused rather than clamped, matching projectionModeFromIndex and
    // sculptToolFromIndex: an unknown mode is a caller bug, not a value to
    // repair into something that happens to draw.
    switch (index) {
        case 0: if (out) *out = GizmoMode::Move; return true;
        case 1: if (out) *out = GizmoMode::Rotate; return true;
        case 2: if (out) *out = GizmoMode::Scale; return true;
        default: break;
    }
    return false;
}

int gizmoModeIndex(GizmoMode mode) {
    switch (mode) {
        case GizmoMode::Move: return 0;
        case GizmoMode::Rotate: return 1;
        case GizmoMode::Scale: return 2;
    }
    return 0;
}

const char* gizmoSpaceName(GizmoSpace space) {
    return space == GizmoSpace::Local ? "local" : "world";
}

bool gizmoSpaceFromIndex(int index, GizmoSpace* out) {
    switch (index) {
        case 0: if (out) *out = GizmoSpace::World; return true;
        case 1: if (out) *out = GizmoSpace::Local; return true;
        default: break;
    }
    return false;
}

int gizmoSpaceIndex(GizmoSpace space) { return space == GizmoSpace::Local ? 1 : 0; }

const char* gizmoAxisName(GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::None: return "none";
        case GizmoAxis::X: return "x";
        case GizmoAxis::Y: return "y";
        case GizmoAxis::Z: return "z";
    }
    return "unknown";
}

Vec3 gizmoAxisDirection(GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::X: return canonicalAxis(0);
        case GizmoAxis::Y: return canonicalAxis(1);
        case GizmoAxis::Z: return canonicalAxis(2);
        case GizmoAxis::None: break;
    }
    return Vec3{0.0f, 0.0f, 0.0f};
}

const char* gizmoHandleName(GizmoHandle handle) {
    switch (handle) {
        case GizmoHandle::None: return "none";
        case GizmoHandle::AxisX: return "x";
        case GizmoHandle::AxisY: return "y";
        case GizmoHandle::AxisZ: return "z";
        case GizmoHandle::PlaneXY: return "xy";
        case GizmoHandle::PlaneXZ: return "xz";
        case GizmoHandle::PlaneYZ: return "yz";
        case GizmoHandle::Uniform: return "uniform";
    }
    return "unknown";
}

int gizmoHandleCode(GizmoHandle handle) {
    switch (handle) {
        case GizmoHandle::None: return 0;
        case GizmoHandle::AxisX: return 1;
        case GizmoHandle::AxisY: return 2;
        case GizmoHandle::AxisZ: return 3;
        case GizmoHandle::PlaneXY: return 4;
        case GizmoHandle::PlaneXZ: return 5;
        case GizmoHandle::PlaneYZ: return 6;
        case GizmoHandle::Uniform: return 7;
    }
    return 0;
}

bool gizmoHandleFromCode(int code, GizmoHandle* out) {
    switch (code) {
        case 0: if (out) *out = GizmoHandle::None; return true;
        case 1: if (out) *out = GizmoHandle::AxisX; return true;
        case 2: if (out) *out = GizmoHandle::AxisY; return true;
        case 3: if (out) *out = GizmoHandle::AxisZ; return true;
        case 4: if (out) *out = GizmoHandle::PlaneXY; return true;
        case 5: if (out) *out = GizmoHandle::PlaneXZ; return true;
        case 6: if (out) *out = GizmoHandle::PlaneYZ; return true;
        case 7: if (out) *out = GizmoHandle::Uniform; return true;
        default: break;
    }
    return false;
}

bool gizmoHandleIsAxis(GizmoHandle handle) {
    return handle == GizmoHandle::AxisX || handle == GizmoHandle::AxisY ||
           handle == GizmoHandle::AxisZ;
}

bool gizmoHandleIsPlane(GizmoHandle handle) {
    return handle == GizmoHandle::PlaneXY || handle == GizmoHandle::PlaneXZ ||
           handle == GizmoHandle::PlaneYZ;
}

int gizmoHandleAxisIndex(GizmoHandle handle) {
    switch (handle) {
        case GizmoHandle::AxisX: return 0;
        case GizmoHandle::AxisY: return 1;
        case GizmoHandle::AxisZ: return 2;
        default: break;
    }
    return -1;
}

bool gizmoPlaneAxisIndices(GizmoHandle handle, int* outFirst, int* outSecond) {
    int a = 0;
    int b = 0;
    switch (handle) {
        case GizmoHandle::PlaneXY: a = 0; b = 1; break;
        case GizmoHandle::PlaneXZ: a = 0; b = 2; break;
        case GizmoHandle::PlaneYZ: a = 1; b = 2; break;
        default: return false;
    }
    if (outFirst) *outFirst = a;
    if (outSecond) *outSecond = b;
    return true;
}

int gizmoPlaneNormalIndex(GizmoHandle handle) {
    switch (handle) {
        case GizmoHandle::PlaneXY: return 2;
        case GizmoHandle::PlaneXZ: return 1;
        case GizmoHandle::PlaneYZ: return 0;
        default: break;
    }
    return -1;
}

GizmoAxis gizmoHandleColorAxis(GizmoHandle handle) {
    if (gizmoHandleIsAxis(handle)) {
        return axisFromIndex(gizmoHandleAxisIndex(handle));
    }
    if (gizmoHandleIsPlane(handle)) {
        // The axis PERPENDICULAR to the plane: the XY square is the blue one,
        // which is the convention every professional tool draws and is what
        // makes a plane handle nameable at a glance.
        return axisFromIndex(gizmoPlaneNormalIndex(handle));
    }
    return GizmoAxis::None;
}

int gizmoHandlesForMode(GizmoMode mode, GizmoHandle* out, int capacity) {
    // Smallest target first, which is the order hitTest resolves tiers in: the
    // uniform handle sits inside the plane handles, which sit between the
    // shafts, and a long shaft would otherwise win every contest.
    GizmoHandle move[6] = {GizmoHandle::PlaneXY, GizmoHandle::PlaneXZ, GizmoHandle::PlaneYZ,
                           GizmoHandle::AxisX,   GizmoHandle::AxisY,   GizmoHandle::AxisZ};
    GizmoHandle rotate[3] = {GizmoHandle::AxisX, GizmoHandle::AxisY, GizmoHandle::AxisZ};
    GizmoHandle scale[7] = {GizmoHandle::Uniform, GizmoHandle::PlaneXY, GizmoHandle::PlaneXZ,
                            GizmoHandle::PlaneYZ, GizmoHandle::AxisX,   GizmoHandle::AxisY,
                            GizmoHandle::AxisZ};
    const GizmoHandle* source = move;
    int count = 6;
    if (mode == GizmoMode::Rotate) {
        source = rotate;
        count = 3;
    } else if (mode == GizmoMode::Scale) {
        source = scale;
        count = 7;
    }
    if (out == nullptr || capacity < count) {
        return 0;
    }
    for (int i = 0; i < count; ++i) {
        out[i] = source[i];
    }
    return count;
}

void gizmoPerpendicularIndices(int axisIndex, int* outU, int* outV) {
    int u = 0;
    int v = 1;
    switch (axisIndex) {
        case 0: u = 1; v = 2; break;
        case 1: u = 2; v = 0; break;
        default: u = 0; v = 1; break;
    }
    if (outU) *outU = u;
    if (outV) *outV = v;
}

// ---------------------------------------------------------------------------
// The constrained basis
// ---------------------------------------------------------------------------

GizmoBasis gizmoBasisFor(GizmoSpace space, const TransformValues& values) {
    GizmoBasis basis;
    for (int i = 0; i < 3; ++i) {
        basis.axis[i] = canonicalAxis(i);
    }
    if (space != GizmoSpace::Local) {
        return basis;
    }
    // The three COLUMNS of the body rotation matrix, which are the world
    // directions its local X, Y and Z point along. Deliberately built from
    // rotationMatrix() and not modelMatrix(): a scaled body still has a local X
    // that points one way, and letting the scale in would make a handle
    // direction depend on how large the body happens to be.
    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(values));
    if (!mat4Finite(rotation)) {
        return basis;  // world axes rather than a basis nothing can be solved in
    }
    for (int i = 0; i < 3; ++i) {
        const Vec3 column{rotation.m[i * 4 + 0], rotation.m[i * 4 + 1], rotation.m[i * 4 + 2]};
        const Vec3 unit = vec3Normalize(column);
        // A rotation matrix is orthonormal, so this normalize is defensive
        // rather than corrective. A degenerate column would mean the matrix was
        // not a rotation at all, and holding the world axis is the honest answer.
        if (vec3Dot(unit, unit) > 0.5f) {
            basis.axis[i] = unit;
        }
    }
    return basis;
}

Mat4 gizmoBasisMatrix(const GizmoBasis& basis) {
    Mat4 m = mat4Identity();
    for (int i = 0; i < 3; ++i) {
        m.m[i * 4 + 0] = basis.axis[i].x;
        m.m[i * 4 + 1] = basis.axis[i].y;
        m.m[i * 4 + 2] = basis.axis[i].z;
        m.m[i * 4 + 3] = 0.0f;
    }
    return m;
}

// ---------------------------------------------------------------------------
// Component accessors
// ---------------------------------------------------------------------------

Meters transformPositionAt(const TransformValues& values, int axisIndex) {
    switch (axisIndex) {
        case 0: return values.positionX;
        case 1: return values.positionY;
        case 2: return values.positionZ;
        default: break;
    }
    return 0.0;
}

void setTransformPositionAt(TransformValues* values, int axisIndex, Meters value) {
    if (values == nullptr) return;
    switch (axisIndex) {
        case 0: values->positionX = value; break;
        case 1: values->positionY = value; break;
        case 2: values->positionZ = value; break;
        default: break;
    }
}

ScaleFactor transformScaleAt(const TransformValues& values, int axisIndex) {
    switch (axisIndex) {
        case 0: return values.scaleX;
        case 1: return values.scaleY;
        case 2: return values.scaleZ;
        default: break;
    }
    return kDefaultScaleFactor;
}

void setTransformScaleAt(TransformValues* values, int axisIndex, ScaleFactor value) {
    if (values == nullptr) return;
    switch (axisIndex) {
        case 0: values->scaleX = value; break;
        case 1: values->scaleY = value; break;
        case 2: values->scaleZ = value; break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// The adapter one number
// ---------------------------------------------------------------------------

bool setGizmoPixelsPerReferenceUnit(float scale) {
    if (!std::isfinite(scale) || scale < kGizmoMinPixelsPerReferenceUnit ||
        scale > kGizmoMaxPixelsPerReferenceUnit) {
        return false;
    }
    g_pixelsPerReferenceUnit.store(scale, std::memory_order_relaxed);
    return true;
}

float gizmoPixelsPerReferenceUnit() {
    return g_pixelsPerReferenceUnit.load(std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

bool projectWorldToScreen(const CameraSnapshot& camera, const Vec3& world, int viewportWidth,
                          int viewportHeight, float* outX, float* outY) {
    if (viewportWidth <= 0 || viewportHeight <= 0 || !vec3Finite(world) ||
        !mat4Finite(camera.view) || !mat4Finite(camera.proj)) {
        return false;
    }
    const Mat4 viewProj = mat4Multiply(camera.proj, camera.view);
    // The full 4-component product: w matters, and mat4TransformPoint discards
    // it. A perspective divide by the w this drops is exactly the step that
    // makes a projection a projection.
    const float x = viewProj.m[0] * world.x + viewProj.m[4] * world.y + viewProj.m[8] * world.z +
                    viewProj.m[12];
    const float y = viewProj.m[1] * world.x + viewProj.m[5] * world.y + viewProj.m[9] * world.z +
                    viewProj.m[13];
    const float w = viewProj.m[3] * world.x + viewProj.m[7] * world.y + viewProj.m[11] * world.z +
                    viewProj.m[15];
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 1e-6f) {
        // w <= 0 is a point at or behind the perspective eye plane. There is no
        // pixel for it, and inventing one would put a handle on screen that is
        // geometrically behind the viewer.
        return false;
    }
    const float ndcX = x / w;
    const float ndcY = y / w;
    // NDC to view-local pixels. The projection matrices already carry Vulkan
    // Y flip (see mat4Perspective), so +y in NDC is already DOWN the screen and
    // nothing is flipped a second time here — which is what makes this the exact
    // inverse of buildPickRay rather than its mirror image.
    const float px = (ndcX * 0.5f + 0.5f) * static_cast<float>(viewportWidth);
    const float py = (ndcY * 0.5f + 0.5f) * static_cast<float>(viewportHeight);
    if (!std::isfinite(px) || !std::isfinite(py)) {
        return false;
    }
    if (outX) *outX = px;
    if (outY) *outY = py;
    return true;
}

bool worldMetersPerPixel(const CameraSnapshot& camera, const Vec3& worldPoint, int viewportHeight,
                         float* out) {
    if (viewportHeight <= 0 || !vec3Finite(worldPoint) || !mat4Finite(camera.view) ||
        !mat4Finite(camera.proj)) {
        return false;
    }
    // |proj.m[5]| is the vertical scale both projections share: 1/tan(fovY/2)
    // for the perspective one and 1/halfHeight for the parallel one. Reading it
    // from the matrix rather than from a remembered field of view is what keeps
    // this from ever disagreeing with what is drawn.
    const float verticalScale = std::fabs(camera.proj.m[5]);
    if (!std::isfinite(verticalScale) || verticalScale <= 0.0f) {
        return false;
    }
    float pixelsPerWorld = 0.5f * static_cast<float>(viewportHeight) * verticalScale;
    if (camera.projection == ProjectionMode::Perspective) {
        // View-space depth of the point: row 2 of the view matrix, negated,
        // because the view looks down -Z.
        const Vec3 viewSpace = mat4TransformPoint(camera.view, worldPoint);
        const float depth = std::fabs(viewSpace.z);
        if (!std::isfinite(depth) || depth <= 1e-4f) {
            return false;
        }
        pixelsPerWorld /= depth;
    }
    if (!std::isfinite(pixelsPerWorld) || pixelsPerWorld <= 1e-6f) {
        return false;
    }
    if (out) *out = 1.0f / pixelsPerWorld;
    return true;
}

bool gizmoWorldScale(const CameraSnapshot& camera, const Vec3& pivot, int viewportHeight,
                     float* out) {
    float metersPerPixel = 0.0f;
    if (!worldMetersPerPixel(camera, pivot, viewportHeight, &metersPerPixel)) {
        return false;
    }
    const float scale = metersPerPixel * gizmoPixelsPerReferenceUnit();
    if (!std::isfinite(scale) || scale <= 0.0f) {
        return false;
    }
    if (out) *out = scale;
    return true;
}

// ---------------------------------------------------------------------------
// The solvers
// ---------------------------------------------------------------------------

const char* axisSolveStatusName(AxisSolveStatus status) {
    switch (status) {
        case AxisSolveStatus::Resolved: return "resolved";
        case AxisSolveStatus::ResolvedByPlane: return "plane_fallback";
        case AxisSolveStatus::Unresolvable: return "unresolvable";
    }
    return "unknown";
}

bool intersectRayPlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal,
                       Vec3* outHit) {
    if (!vec3Finite(ray.origin) || !vec3Finite(ray.direction) || !vec3Finite(planePoint) ||
        !vec3Finite(planeNormal)) {
        return false;
    }
    const Vec3 n = vec3Normalize(planeNormal);
    const float denominator = vec3Dot(n, ray.direction);
    if (!std::isfinite(denominator) || std::fabs(denominator) < kGizmoPlaneParallelEpsilon) {
        return false;
    }
    const float t = vec3Dot(vec3Sub(planePoint, ray.origin), n) / denominator;
    if (!std::isfinite(t)) {
        return false;
    }
    const Vec3 hit = vec3Add(ray.origin, vec3Scale(ray.direction, t));
    if (!vec3Finite(hit)) {
        return false;
    }
    if (outHit) *outHit = hit;
    return true;
}

AxisSolveStatus solveAxisParameter(const Ray& ray, const Vec3& pivot, const Vec3& axis,
                                   float* outT) {
    if (!vec3Finite(ray.origin) || !vec3Finite(ray.direction) || !vec3Finite(pivot) ||
        !vec3Finite(axis)) {
        return AxisSolveStatus::Unresolvable;
    }
    const Vec3 u = vec3Normalize(axis);
    const Vec3 d = vec3Normalize(ray.direction);
    if (vec3Dot(u, u) < 0.5f || vec3Dot(d, d) < 0.5f) {
        return AxisSolveStatus::Unresolvable;  // a degenerate axis or direction
    }

    // Closest approach between the pick ray and the INFINITE axis line. With
    // both directions unit length the classic 2x2 system reduces to this, and
    // the denominator is 1 - cos^2 of the angle between them: it vanishes
    // precisely when the two are parallel, which is the case the fallback
    // exists for.
    const Vec3 w0 = vec3Sub(ray.origin, pivot);
    const float b = vec3Dot(d, u);
    const float denominator = 1.0f - b * b;
    if (std::isfinite(denominator) && denominator >= kGizmoAxisParallelDenominator) {
        const float dw = vec3Dot(d, w0);
        const float ew = vec3Dot(u, w0);
        const float t = (ew - b * dw) / denominator;
        if (std::isfinite(t)) {
            if (outT) *outT = t;
            return AxisSolveStatus::Resolved;
        }
        return AxisSolveStatus::Unresolvable;
    }

    // The camera-facing plane that CONTAINS the axis: its normal is the part of
    // the view direction perpendicular to the axis, which is the most face-on
    // such plane there is. Intersect, then project the hit back onto the axis —
    // the constraint is still the axis, only the surface used to read the
    // pointer against has changed.
    const Vec3 perpendicular = vec3Sub(d, vec3Scale(u, b));
    const Vec3 normal = vec3Normalize(perpendicular);
    if (vec3Dot(normal, normal) < 0.5f) {
        return AxisSolveStatus::Unresolvable;
    }
    Vec3 hit{};
    if (!intersectRayPlane(ray, pivot, normal, &hit)) {
        return AxisSolveStatus::Unresolvable;
    }
    const float t = vec3Dot(vec3Sub(hit, pivot), u);
    if (!std::isfinite(t)) {
        return AxisSolveStatus::Unresolvable;
    }
    if (outT) *outT = t;
    return AxisSolveStatus::ResolvedByPlane;
}

bool signedAngleAround(const Vec3& axis, const Vec3& from, const Vec3& to, float* outRadians) {
    const Vec3 n = vec3Normalize(axis);
    // Both vectors are flattened into the plane the angle is measured in, so a
    // hit that is slightly off the plane through floating point cannot tilt the
    // answer.
    const Vec3 a = vec3Normalize(vec3Sub(from, vec3Scale(n, vec3Dot(from, n))));
    const Vec3 b = vec3Normalize(vec3Sub(to, vec3Scale(n, vec3Dot(to, n))));
    if (vec3Dot(n, n) < 0.5f || vec3Dot(a, a) < 0.5f || vec3Dot(b, b) < 0.5f) {
        return false;
    }
    const float sine = vec3Dot(n, vec3Cross(a, b));
    const float cosine = vec3Dot(a, b);
    if (!std::isfinite(sine) || !std::isfinite(cosine)) {
        return false;
    }
    const float angle = std::atan2(sine, cosine);
    if (!std::isfinite(angle)) {
        return false;
    }
    if (outRadians) *outRadians = angle;
    return true;
}

float unwrapAngleDelta(float radians) {
    if (!std::isfinite(radians)) {
        return 0.0f;
    }
    float wrapped = std::fmod(radians + kPi, kTwoPi);
    if (wrapped < 0.0f) {
        wrapped += kTwoPi;
    }
    return wrapped - kPi;
}

// ---------------------------------------------------------------------------
// The quantization and placement seam
// ---------------------------------------------------------------------------
//
// The identity, today and deliberately. See the header: the seam exists so a
// future approved Surface Snap or Grid Snap contract lands here instead of in
// the gesture architecture, and there is no snapping, no setting and no
// indicator until one is approved.
Meters quantizeGizmoTranslation(Meters raw) { return raw; }
Degrees quantizeGizmoRotation(Degrees raw) { return raw; }
ScaleFactor quantizeGizmoScale(ScaleFactor raw) { return raw; }

TransformValues applyGizmoPlacementModifier(const TransformValues& target,
                                            const TransformValues& start, GizmoMode mode,
                                            GizmoHandle handle, GizmoSpace space) {
    // Every argument is deliberately taken and deliberately unused. An approved
    // snap needs all four — where the drag started, what kind of drag it is and
    // in which basis — and the whole point of declaring the seam now is that
    // adding one changes this function and nothing else.
    (void)start;
    (void)mode;
    (void)handle;
    (void)space;
    return target;
}

// ---------------------------------------------------------------------------
// The drawn geometry
// ---------------------------------------------------------------------------

namespace {

struct GizmoVertexWriter {
    GizmoVertex* out;
    int capacity;
    int written;
    // The bundle recipe every stroke follows. See GizmoStrokeStyle.
    GizmoStrokeStyle style;

    void line(const Vec3& a, const Vec3& b, GizmoAxis colorAxis, GizmoHandle handle) {
        if (written + 2 > capacity) {
            return;
        }
        const float tag = colorTagFor(colorAxis);
        const float handleCode = static_cast<float>(gizmoHandleCode(handle));
        const Vec3 ends[2] = {a, b};
        for (int i = 0; i < 2; ++i) {
            out[written].position[0] = ends[i].x;
            out[written].position[1] = ends[i].y;
            out[written].position[2] = ends[i].z;
            out[written].axis = tag;
            out[written].handle = handleCode;
            ++written;
        }
    }

    // The offsets one bundled stroke is drawn at, perpendicular to a direction.
    //
    // Five is the + pattern (centre, ±u, ±v at one spread); thirteen adds the x
    // pattern between them and a second + one spread further out, so the band
    // is FILLED rather than fanned. Returns how many were written.
    int bundleOffsets(const Vec3& u, const Vec3& v, int count, Vec3* offsets) const {
        const float s = style.spread;
        int n = 0;
        offsets[n++] = Vec3{0.0f, 0.0f, 0.0f};
        if (count >= 5) {
            offsets[n++] = vec3Scale(u, s);
            offsets[n++] = vec3Scale(u, -s);
            offsets[n++] = vec3Scale(v, s);
            offsets[n++] = vec3Scale(v, -s);
        }
        if (count >= 13) {
            const float d = s * 0.70710678f;
            offsets[n++] = vec3Add(vec3Scale(u, d), vec3Scale(v, d));
            offsets[n++] = vec3Add(vec3Scale(u, d), vec3Scale(v, -d));
            offsets[n++] = vec3Add(vec3Scale(u, -d), vec3Scale(v, d));
            offsets[n++] = vec3Add(vec3Scale(u, -d), vec3Scale(v, -d));
            offsets[n++] = vec3Scale(u, 2.0f * s);
            offsets[n++] = vec3Scale(u, -2.0f * s);
            offsets[n++] = vec3Scale(v, 2.0f * s);
            offsets[n++] = vec3Scale(v, -2.0f * s);
        }
        return n;
    }

    // One stroke of ANY direction as a bundle: the perpendicular pair is derived
    // from the segment itself. Used for the arrowhead strokes, whose direction is
    // not a basis axis. A count of 1 is the plain line.
    void bundledLine(const Vec3& a, const Vec3& b, GizmoAxis colorAxis, GizmoHandle handle,
                     int count) {
        if (count <= 1) {
            line(a, b, colorAxis, handle);
            return;
        }
        const Vec3 direction = vec3Normalize(vec3Sub(b, a));
        // Any axis not parallel to the stroke gives a usable perpendicular.
        const Vec3 helper = std::fabs(direction.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f}
                                                          : Vec3{1.0f, 0.0f, 0.0f};
        const Vec3 u = vec3Normalize(vec3Cross(direction, helper));
        const Vec3 v = vec3Cross(direction, u);
        Vec3 offsets[kGizmoStrokeBundleMax];
        const int n = bundleOffsets(u, v, count, offsets);
        for (int i = 0; i < n; ++i) {
            line(vec3Add(a, offsets[i]), vec3Add(b, offsets[i]), colorAxis, handle);
        }
    }

    // Twelve edges of an axis-aligned box, in CANONICAL gizmo space. The basis
    // rotation is applied by the model matrix, so a cube drawn here is a cube
    // aligned to whichever basis the gizmo is currently in.
    void cube(const Vec3& centre, float half, GizmoAxis colorAxis, GizmoHandle handle) {
        Vec3 corner[8];
        for (int i = 0; i < 8; ++i) {
            corner[i] = Vec3{centre.x + ((i & 1) ? half : -half),
                             centre.y + ((i & 2) ? half : -half),
                             centre.z + ((i & 4) ? half : -half)};
        }
        // Each pair differs in exactly one bit, which is exactly an edge.
        for (int i = 0; i < 8; ++i) {
            for (int bit = 1; bit <= 4; bit <<= 1) {
                const int j = i | bit;
                if (j != i) {
                    line(corner[i], corner[j], colorAxis, handle);
                }
            }
        }
    }

    // One plane handle: a square in the (u, v) plane, drawn twice a stroke
    // offset apart for the same legibility reason the shafts are bundled.
    void planeSquare(GizmoHandle handle) {
        int a = 0;
        int b = 0;
        if (!gizmoPlaneAxisIndices(handle, &a, &b)) {
            return;
        }
        const GizmoAxis colorAxis = gizmoHandleColorAxis(handle);
        const Vec3 u = canonicalAxis(a);
        const Vec3 v = canonicalAxis(b);
        for (int pass = 0; pass < style.squarePasses; ++pass) {
            const float grow = static_cast<float>(pass) * style.spread;
            const float inner = kGizmoPlaneInnerUnits - grow;
            const float outer = kGizmoPlaneOuterUnits + grow;
            const Vec3 corners[4] = {
                vec3Add(vec3Scale(u, inner), vec3Scale(v, inner)),
                vec3Add(vec3Scale(u, outer), vec3Scale(v, inner)),
                vec3Add(vec3Scale(u, outer), vec3Scale(v, outer)),
                vec3Add(vec3Scale(u, inner), vec3Scale(v, outer)),
            };
            for (int i = 0; i < 4; ++i) {
                line(corners[i], corners[(i + 1) % 4], colorAxis, handle);
            }
        }
        // The two diagonals, on the nominal square. See
        // kGizmoPlaneSquareLineCount: this is what makes a plane handle read as
        // a surface rather than as one more hollow outline in an axis hue.
        const float inner = kGizmoPlaneInnerUnits;
        const float outer = kGizmoPlaneOuterUnits;
        const Vec3 lowLow = vec3Add(vec3Scale(u, inner), vec3Scale(v, inner));
        const Vec3 highHigh = vec3Add(vec3Scale(u, outer), vec3Scale(v, outer));
        const Vec3 highLow = vec3Add(vec3Scale(u, outer), vec3Scale(v, inner));
        const Vec3 lowHigh = vec3Add(vec3Scale(u, inner), vec3Scale(v, outer));
        line(lowLow, highHigh, colorAxis, handle);
        line(highLow, lowHigh, colorAxis, handle);
    }

    // The pivot mark: three neutral arms through the origin, drawn once.
    //
    // Neutral and separate from the shafts, because the point the transform is
    // about belongs to no axis. See kGizmoPivotMarkerFraction.
    void pivotMark() {
        const float arm = kGizmoHandleLengthUnits * kGizmoPivotMarkerFraction;
        for (int index = 0; index < 3; ++index) {
            const Vec3 direction = canonicalAxis(index);
            line(vec3Scale(direction, -arm), vec3Scale(direction, arm), GizmoAxis::None,
                 GizmoHandle::None);
        }
    }

    // The bundled shaft an axis handle shares between Move and Scale.
    // `shaftEnd` is where the tip decoration begins.
    //
    // The pivot arms used to be drawn here, one per axis in that axis's hue. They
    // are pivotMark()'s now, and neutral: see kGizmoPivotMarkerFraction.
    void axisShaft(int index, float shaftEnd) {
        const GizmoAxis colorAxis = axisFromIndex(index);
        const GizmoHandle handle = axisHandleFromIndex(index);
        const Vec3 direction = canonicalAxis(index);

        int ui = 0;
        int vi = 0;
        gizmoPerpendicularIndices(index, &ui, &vi);
        const Vec3 u = canonicalAxis(ui);
        const Vec3 v = canonicalAxis(vi);
        const Vec3 end = vec3Scale(direction, shaftEnd);
        // The + pattern at one spread is exactly the Regular bundle; the wider
        // recipes add to it and never move it, so the centre line of every
        // weight is the same line.
        Vec3 strokeOffsets[kGizmoStrokeBundleMax];
        const int n = bundleOffsets(u, v, style.shaftBundle, strokeOffsets);
        for (int s = 0; s < n; ++s) {
            line(strokeOffsets[s], vec3Add(end, strokeOffsets[s]), colorAxis, handle);
        }
    }
};

}  // namespace

bool gizmoVertexRange(GizmoMode mode, GizmoStrokeWeight weight, int* outFirst, int* outCount) {
    const int move = 2 * gizmoMoveLineCountFor(weight);
    const int rotate = 2 * gizmoRotateLineCountFor(weight);
    const int scale = 2 * gizmoScaleLineCountFor(weight);
    int first = 0;
    int count = move;
    switch (mode) {
        case GizmoMode::Move: break;
        case GizmoMode::Rotate:
            first = move;
            count = rotate;
            break;
        case GizmoMode::Scale:
            first = move + rotate;
            count = scale;
            break;
    }
    if (outFirst) *outFirst = first;
    if (outCount) *outCount = count;
    return count > 0;
}

bool gizmoVertexRange(GizmoMode mode, int* outFirst, int* outCount) {
    return gizmoVertexRange(mode, GizmoStrokeWeight::Regular, outFirst, outCount);
}

int generateGizmoVertices(GizmoVertex* out, int capacity) {
    return generateGizmoVertices(out, capacity, GizmoStrokeWeight::Regular);
}

int generateGizmoVertices(GizmoVertex* out, int capacity, GizmoStrokeWeight weight) {
    if (out == nullptr || capacity < gizmoVertexCountFor(weight)) {
        return 0;
    }
    const GizmoStrokeStyle style = gizmoStrokeStyle(weight);
    GizmoVertexWriter writer{out, capacity, 0, style};

    // --- Move: pivot mark, shafts, arrowheads, plane squares -------------
    const float length = kGizmoHandleLengthUnits;
    const float arrow = length * kGizmoArrowLengthFraction;
    const float arrowHalfWidth = length * kGizmoArrowHalfWidthFraction;
    writer.pivotMark();
    for (int i = 0; i < 3; ++i) {
        writer.axisShaft(i, length - arrow);

        // A six-line arrowhead rather than a cone: four spokes back from the tip
        // and two lines closing across their ends. It reads as a direction from
        // every angle, and the closing cross is what separates a head from two
        // more hairlines — without it the whole instrument was one weight of
        // line from the pivot to the tip. Still no second pipeline for solid
        // geometry.
        int ui = 0;
        int vi = 0;
        gizmoPerpendicularIndices(i, &ui, &vi);
        const Vec3 direction = canonicalAxis(i);
        const Vec3 tip = vec3Scale(direction, length);
        const Vec3 base = vec3Scale(direction, length - arrow);
        const Vec3 spokes[4] = {vec3Scale(canonicalAxis(ui), arrowHalfWidth),
                                vec3Scale(canonicalAxis(ui), -arrowHalfWidth),
                                vec3Scale(canonicalAxis(vi), arrowHalfWidth),
                                vec3Scale(canonicalAxis(vi), -arrowHalfWidth)};
        for (int s = 0; s < 4; ++s) {
            writer.bundledLine(tip, vec3Add(base, spokes[s]), axisFromIndex(i),
                               axisHandleFromIndex(i), style.arrowBundle);
        }
        writer.bundledLine(vec3Add(base, spokes[0]), vec3Add(base, spokes[1]), axisFromIndex(i),
                           axisHandleFromIndex(i), style.arrowBundle);
        writer.bundledLine(vec3Add(base, spokes[2]), vec3Add(base, spokes[3]), axisFromIndex(i),
                           axisHandleFromIndex(i), style.arrowBundle);
    }
    for (int p = 0; p < 3; ++p) {
        writer.planeSquare(kPlaneHandles[p]);
    }

    // --- Rotate: three rings in the current basis ------------------------
    //
    // The ring PLANE is the basis axis plane and stays there. It is deliberately
    // not billboarded toward the camera: a ring that turned to face the viewer
    // would stop showing which plane the rotation happens in, which is the one
    // thing it is there to say.
    //
    // Two concentric passes a stroke width apart, for the legibility reason the
    // shafts are bundled. The HIT test still measures against the nominal radius
    // alone: the corridor is 48 units wide and a one-unit ring thickness is
    // inside it, so what is drawn and what can be grabbed do not disagree.
    // Two passes at r and r+s is the Regular recipe; four passes run from r-s
    // to r+2s, so the nominal radius is always one of the passes drawn.
    float radii[4] = {kGizmoRingRadiusUnits, kGizmoRingRadiusUnits + style.spread,
                      kGizmoRingRadiusUnits - style.spread,
                      kGizmoRingRadiusUnits + 2.0f * style.spread};
    // The same neutral pivot mark Move has. Three rings around a point with
    // nothing at the point does not say where the rotation is centred, and the
    // centre is the one thing a rotation is entirely about.
    writer.pivotMark();
    for (int i = 0; i < 3; ++i) {
        int ui = 0;
        int vi = 0;
        gizmoPerpendicularIndices(i, &ui, &vi);
        const Vec3 u = canonicalAxis(ui);
        const Vec3 v = canonicalAxis(vi);
        for (int pass = 0; pass < style.ringPasses; ++pass) {
            const float radius = radii[pass];
            for (int s = 0; s < kGizmoRingSegments; ++s) {
                const float a0 = kTwoPi * static_cast<float>(s) /
                                 static_cast<float>(kGizmoRingSegments);
                const float a1 = kTwoPi * static_cast<float>(s + 1) /
                                 static_cast<float>(kGizmoRingSegments);
                const Vec3 p0 = vec3Add(vec3Scale(u, radius * std::cos(a0)),
                                        vec3Scale(v, radius * std::sin(a0)));
                const Vec3 p1 = vec3Add(vec3Scale(u, radius * std::cos(a1)),
                                        vec3Scale(v, radius * std::sin(a1)));
                writer.line(p0, p1, axisFromIndex(i), axisHandleFromIndex(i));
            }
        }
    }

    // --- Scale: cubes instead of arrowheads, plus the uniform cube -------
    //
    // A cube endpoint is the professional vocabulary for "this stretches" the
    // same way an arrowhead is for "this moves", and it is what makes the two
    // modes readable apart at a glance rather than by remembering which one is
    // selected.
    for (int i = 0; i < 3; ++i) {
        const float cubeCentre = length - kGizmoScaleCubeHalfUnits;
        writer.axisShaft(i, cubeCentre);
        for (int outline = 0; outline < style.axisCubeOutlines; ++outline) {
            writer.cube(vec3Scale(canonicalAxis(i), cubeCentre),
                        kGizmoScaleCubeHalfUnits + static_cast<float>(outline) * style.spread,
                        axisFromIndex(i), axisHandleFromIndex(i));
        }
    }
    for (int p = 0; p < 3; ++p) {
        writer.planeSquare(kPlaneHandles[p]);
    }
    // The uniform handle: a cube at the pivot, drawn NEUTRAL because it belongs
    // to no axis and lighting it in an axis hue would say it did — and drawn as
    // a DOUBLE outline, a stroke offset apart, because it is the one handle
    // acting on all three axes and it stands where every shaft converges. No
    // pivot mark is drawn in Scale: this cube is what stands on that point, and
    // a reference mark and a control sharing one point is exactly what made the
    // control unreadable.
    for (int outline = 0; outline < style.uniformOutlines; ++outline) {
        writer.cube(Vec3{0.0f, 0.0f, 0.0f},
                    kGizmoUniformCubeHalfUnits + static_cast<float>(outline) * style.spread,
                    GizmoAxis::None, GizmoHandle::Uniform);
    }

    return writer.written;
}

float gizmoPlacementScale(const GizmoSnapshot& state) {
    return state.worldPerReferenceUnit * state.visualScale;
}

bool gizmoVisualScaleIsValid(float scale) {
    return std::isfinite(scale) && scale >= kGizmoMinVisualScale && scale <= kGizmoMaxVisualScale;
}

Vec3 gizmoRingGrabOffset(GizmoAxis axis, float radius) {
    if (axis == GizmoAxis::None || !std::isfinite(radius)) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    int index = 0;
    switch (axis) {
        case GizmoAxis::Y: index = 1; break;
        case GizmoAxis::Z: index = 2; break;
        default: index = 0; break;
    }
    int ui = 0;
    int vi = 0;
    gizmoPerpendicularIndices(index, &ui, &vi);
    // 45 degrees between the plane own two basis directions: the two ring
    // crossings sit exactly on those directions, so the bisector is the point on
    // this ring furthest from either of them. See the header.
    const float half = radius * 0.70710678f;
    return vec3Add(vec3Scale(canonicalAxis(ui), half), vec3Scale(canonicalAxis(vi), half));
}

bool gizmoHandleGrabPoint(const GizmoSnapshot& state, GizmoHandle handle, Vec3* out) {
    if (out == nullptr || !state.visible || handle == GizmoHandle::None ||
        !(state.worldPerReferenceUnit > 0.0f)) {
        return false;
    }
    // The basis, read back out of the snapshot the renderer is drawing with, so
    // there is no second copy of "which way does local X point".
    const Mat4& basis = state.orientation;
    auto basisAxis = [&basis](int index) {
        return Vec3{basis.m[index * 4 + 0], basis.m[index * 4 + 1], basis.m[index * 4 + 2]};
    };
    // PLACED at the visual size, exactly as the renderer draws it.
    const float scale = gizmoPlacementScale(state);

    if (handle == GizmoHandle::Uniform) {
        *out = state.pivot;
        return true;
    }
    if (gizmoHandleIsPlane(handle)) {
        int a = 0;
        int b = 0;
        gizmoPlaneAxisIndices(handle, &a, &b);
        const float d = kGizmoPlaneCentreUnits * scale;
        *out = vec3Add(state.pivot, vec3Add(vec3Scale(basisAxis(a), d), vec3Scale(basisAxis(b), d)));
        return vec3Finite(*out);
    }
    const int index = gizmoHandleAxisIndex(handle);
    if (index < 0) {
        return false;
    }
    if (state.mode == GizmoMode::Rotate) {
        // Deliberately NOT on a basis direction: that is exactly where two rings
        // cross, and a point there names no single axis. See gizmoRingGrabOffset.
        int ui = 0;
        int vi = 0;
        gizmoPerpendicularIndices(index, &ui, &vi);
        const float half = kGizmoRingRadiusUnits * scale * 0.70710678f;
        *out = vec3Add(state.pivot,
                       vec3Add(vec3Scale(basisAxis(ui), half), vec3Scale(basisAxis(vi), half)));
        return vec3Finite(*out);
    }
    const float middle =
        0.5f * (kGizmoShaftGrabStartFraction + kGizmoShaftGrabEndFraction);
    *out = vec3Add(state.pivot,
                   vec3Scale(basisAxis(index), kGizmoHandleLengthUnits * scale * middle));
    return vec3Finite(*out);
}

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------

namespace {

struct GizmoPalette {
    float x[3];
    float y[3];
    float z[3];
    float highlight[3];
    float neutral;
};

// The three hues extend the convention the world grid already draws with — a
// warm X and a cool Z — by adding the Y the grid has no line for, and raise all
// three to tool weight. A handle must read as an instrument the user can grab,
// which a 0.5-alpha reference line does not.
//
// The two dark appearances share a palette: they differ in the ground they are
// drawn on and not in what an axis MEANS, and giving them separate handle hues
// would be three variants of one instrument for no reading gain.
const GizmoPalette kDarkPalette = {
    {0.94f, 0.42f, 0.36f},
    {0.52f, 0.86f, 0.44f},
    {0.40f, 0.62f, 0.96f},
    // Held: a warm amber that no axis owns, so it can never be misread as an
    // axis identity.
    {1.00f, 0.84f, 0.28f},
    0.92f,
};

// Darker and more saturated on the light ground, so contrast against the
// background is comparable rather than the hue being nominally "the same".
const GizmoPalette kLightPalette = {
    {0.80f, 0.22f, 0.18f},
    {0.16f, 0.56f, 0.24f},
    {0.13f, 0.40f, 0.84f},
    {0.86f, 0.56f, 0.02f},
    0.18f,
};

// Light Charcoal has always taken the saturated palette, and the two LIGHT
// grounds of UI-PREF-R1 take it for the reason it was authored: contrast against
// a pale ground needs darker, more saturated hues. The two darker grounds are
// untouched.
const GizmoPalette& paletteFor(ViewportBackground background) {
    return (background == ViewportBackground::LightCharcoal ||
            viewportBackgroundIsLight(background))
               ? kLightPalette
               : kDarkPalette;
}

}  // namespace

void gizmoAxisColor(ViewportBackground background, GizmoAxis axis, float* outRgba) {
    if (outRgba == nullptr) {
        return;
    }
    const GizmoPalette& palette = paletteFor(background);
    const float* source = palette.x;
    switch (axis) {
        case GizmoAxis::Y: source = palette.y; break;
        case GizmoAxis::Z: source = palette.z; break;
        case GizmoAxis::X:
        case GizmoAxis::None: break;
    }
    for (int i = 0; i < 3; ++i) {
        outRgba[i] = source[i];
    }
    outRgba[3] = kGizmoAxisAlpha;
}

void gizmoHighlightColor(ViewportBackground background, float* outRgb) {
    if (outRgb == nullptr) {
        return;
    }
    const GizmoPalette& palette = paletteFor(background);
    for (int i = 0; i < 3; ++i) {
        outRgb[i] = palette.highlight[i];
    }
}

float gizmoNeutralLevel(ViewportBackground background) { return paletteFor(background).neutral; }

// ---------------------------------------------------------------------------
// GizmoSession
// ---------------------------------------------------------------------------

void GizmoSession::setActive(bool active) {
    if (active_ == active) {
        return;
    }
    if (!active && capturing_) {
        // The gizmo is going away under a held handle. Cancel rather than
        // commit: the user did not let go, so what is on screen mid-drag is not
        // a result they chose.
        cancelDrag();
    }
    active_ = active;
    if (active_) {
        // Move in World on entry, every time. A tool that reopened in whatever
        // sub-mode and basis it was last left in would make the first drag after
        // a context switch a guess.
        mode_ = GizmoMode::Move;
        space_ = GizmoSpace::World;
        restoreSpace_ = GizmoSpace::World;
    }
}

bool GizmoSession::setMode(GizmoMode mode) {
    if (capturing_) {
        return false;
    }
    if (mode == GizmoMode::Scale) {
        if (mode_ != GizmoMode::Scale) {
            // Remembered so leaving Scale can put it back. A round trip through
            // Scale must not quietly change what a Move handle means.
            restoreSpace_ = space_;
        }
        space_ = GizmoSpace::Local;
    } else if (mode_ == GizmoMode::Scale) {
        space_ = restoreSpace_;
    }
    mode_ = mode;
    return true;
}

bool GizmoSession::setSpace(GizmoSpace space) {
    if (capturing_) {
        return false;
    }
    if (mode_ == GizmoMode::Scale) {
        // Local is already what Scale is in, so asking for it is honoured as a
        // no-op; World is refused rather than silently applied, because a
        // world-axis scale of a rotated body is a shear. The workspace withdraws
        // the selector here as well — removing a control is not removing a guard.
        return space == GizmoSpace::Local;
    }
    space_ = space;
    restoreSpace_ = space;
    return true;
}

bool GizmoSession::setVisualScale(float scale) {
    if (!gizmoVisualScaleIsValid(scale)) {
        return false;
    }
    visualScale_ = scale;
    return true;
}

GizmoSnapshot GizmoSession::snapshot(const CameraSnapshot& camera, int viewportWidth,
                                     int viewportHeight) const {
    GizmoSnapshot out;
    if (!active_ || scene_.activeBodyId() == kNoObject || viewportWidth <= 0 ||
        viewportHeight <= 0) {
        return out;
    }
    const SceneObject* body = scene_.findBody(scene_.activeBodyId());
    if (body == nullptr) {
        return out;
    }
    const TransformValues values = body->transform().values();
    const Vec3 pivot{static_cast<float>(values.positionX), static_cast<float>(values.positionY),
                     static_cast<float>(values.positionZ)};
    float scale = 0.0f;
    if (!gizmoWorldScale(camera, pivot, viewportHeight, &scale)) {
        return out;
    }
    out.visible = true;
    // The mode and space being DRAWN while a drag is live are the drag own,
    // which cannot differ from the session because both setters are refused
    // mid-drag — stated here so a future relaxation of that refusal cannot
    // silently draw the wrong handles.
    out.mode = capturing_ ? dragMode_ : mode_;
    out.space = capturing_ ? dragSpace_ : space_;
    out.activeHandle = capturing_ ? handle_ : GizmoHandle::None;
    out.pivot = pivot;
    // Frozen basis while capturing, for the reason GizmoBasis states: in Local
    // a rotate drag is changing the very rotation the basis comes from, and a
    // basis re-read every frame would turn under the finger that is turning it.
    out.orientation = gizmoBasisMatrix(capturing_ ? basis_
                                                  : gizmoBasisFor(space_, values));
    out.worldPerReferenceUnit = scale;
    out.visualScale = visualScale_;
    return out;
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------
//
// In PIXELS, against the same geometry the renderer draws at the same scale.
// The alternative — ray/cylinder, ray/torus, ray/box tests in world space —
// answers a different question from the one the user is asking, which is "is my
// finger on that thing I can see". Measuring in pixels is also what makes the
// 48-unit floor a number this file can state rather than an aspiration.
GizmoHandle GizmoSession::hitTest(const CameraSnapshot& camera, float screenX, float screenY,
                                  int viewportWidth, int viewportHeight) const {
    return gizmoHitTestSnapshot(snapshot(camera, viewportWidth, viewportHeight), camera, screenX,
                                screenY, viewportWidth, viewportHeight);
}

GizmoHandle gizmoHitTestSnapshot(const GizmoSnapshot& state, const CameraSnapshot& camera,
                                 float screenX, float screenY, int viewportWidth,
                                 int viewportHeight) {
    if (!state.visible || !std::isfinite(screenX) || !std::isfinite(screenY)) {
        return GizmoHandle::None;
    }
    const float pixelsPerUnit = gizmoPixelsPerReferenceUnit();
    const Mat4& basis = state.orientation;
    auto basisAxis = [&basis](int index) {
        return Vec3{basis.m[index * 4 + 0], basis.m[index * 4 + 1], basis.m[index * 4 + 2]};
    };

    // The pivot disc, in Move and Rotate: a touch at the centre names no handle
    // rather than an arbitrary one. See kGizmoPivotDeadRadiusUnits. In Scale the
    // disc is the uniform handle, so the exclusion is skipped there.
    if (state.mode != GizmoMode::Scale) {
        float px = 0.0f, py = 0.0f;
        if (projectWorldToScreen(camera, state.pivot, viewportWidth, viewportHeight, &px, &py)) {
            const float dead = kGizmoPivotDeadRadiusUnits * pixelsPerUnit;
            const float dx = screenX - px;
            const float dy = screenY - py;
            if (dx * dx + dy * dy < dead * dead) {
                return GizmoHandle::None;
            }
        }
    }

    GizmoHandle handles[kGizmoMaxHandles];
    const int count = gizmoHandlesForMode(state.mode, handles, kGizmoMaxHandles);

    // Resolved in TIERS. `gizmoHandlesForMode` returns smallest target first, so
    // walking it and returning as soon as a tier produces a hit is exactly the
    // priority the header states: uniform, then planes, then axes. Without it
    // the three shafts, which are long, would win every contest against the
    // small handles that sit between them.
    auto tierOf = [](GizmoHandle handle) {
        if (handle == GizmoHandle::Uniform) return 0;
        return gizmoHandleIsPlane(handle) ? 1 : 2;
    };

    int index = 0;
    while (index < count) {
        const int tier = tierOf(handles[index]);
        GizmoHandle best = GizmoHandle::None;
        float bestDistanceSq = 0.0f;

        while (index < count && tierOf(handles[index]) == tier) {
            const GizmoHandle handle = handles[index];
            ++index;

            float slopPixels = kGizmoHitSlopUnits * pixelsPerUnit;
            float distanceSq = 0.0f;
            bool measured = false;

            if (handle == GizmoHandle::Uniform) {
                slopPixels = kGizmoUniformHitRadiusUnits * pixelsPerUnit;
                float px = 0.0f, py = 0.0f;
                if (projectWorldToScreen(camera, state.pivot, viewportWidth, viewportHeight, &px,
                                         &py)) {
                    distanceSq = (screenX - px) * (screenX - px) + (screenY - py) * (screenY - py);
                    measured = true;
                }
            } else if (gizmoHandleIsPlane(handle)) {
                slopPixels = kGizmoPlaneHitRadiusUnits * pixelsPerUnit;
                Vec3 centre{};
                float px = 0.0f, py = 0.0f;
                if (gizmoHandleGrabPoint(state, handle, &centre) &&
                    projectWorldToScreen(camera, centre, viewportWidth, viewportHeight, &px,
                                         &py)) {
                    distanceSq = (screenX - px) * (screenX - px) + (screenY - py) * (screenY - py);
                    measured = true;
                }
            } else if (state.mode == GizmoMode::Rotate) {
                // The ring, as the polyline it is drawn as. An edge-on ring
                // projects to a segment and this still measures it correctly,
                // which is what keeps a ring grabbable from every camera angle
                // instead of only the face-on ones.
                const int axisIndex = gizmoHandleAxisIndex(handle);
                const float radius = kGizmoRingRadiusUnits * gizmoPlacementScale(state);
                int ui = 0;
                int vi = 0;
                gizmoPerpendicularIndices(axisIndex, &ui, &vi);
                const Vec3 u = basisAxis(ui);
                const Vec3 v = basisAxis(vi);
                float previousX = 0.0f, previousY = 0.0f;
                bool havePrevious = false;
                float ringBest = slopPixels * slopPixels;
                for (int s = 0; s <= kGizmoRingSegments; ++s) {
                    const float angle = kTwoPi * static_cast<float>(s) /
                                        static_cast<float>(kGizmoRingSegments);
                    const Vec3 point = vec3Add(
                        state.pivot, vec3Add(vec3Scale(u, radius * std::cos(angle)),
                                             vec3Scale(v, radius * std::sin(angle))));
                    float px = 0.0f, py = 0.0f;
                    if (!projectWorldToScreen(camera, point, viewportWidth, viewportHeight, &px,
                                              &py)) {
                        havePrevious = false;
                        continue;
                    }
                    if (havePrevious) {
                        const float d = pointSegmentDistanceSq(screenX, screenY, previousX,
                                                               previousY, px, py);
                        if (d < ringBest) {
                            ringBest = d;
                            measured = true;
                        }
                    }
                    previousX = px;
                    previousY = py;
                    havePrevious = true;
                }
                distanceSq = ringBest;
            } else {
                const int axisIndex = gizmoHandleAxisIndex(handle);
                const Vec3 direction = basisAxis(axisIndex);
                const float length = kGizmoHandleLengthUnits * gizmoPlacementScale(state);
                const Vec3 start = vec3Add(
                    state.pivot, vec3Scale(direction, length * kGizmoShaftGrabStartFraction));
                const Vec3 end =
                    vec3Add(state.pivot, vec3Scale(direction, length * kGizmoShaftGrabEndFraction));
                float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
                if (projectWorldToScreen(camera, start, viewportWidth, viewportHeight, &ax, &ay) &&
                    projectWorldToScreen(camera, end, viewportWidth, viewportHeight, &bx, &by)) {
                    distanceSq = pointSegmentDistanceSq(screenX, screenY, ax, ay, bx, by);
                    measured = true;
                }
            }

            const float slopSq = slopPixels * slopPixels;
            // Strictly nearer wins, so the enumeration order breaks an exact tie
            // deterministically — X before Y before Z, XY before XZ before YZ —
            // rather than iteration order being read as significance.
            if (measured && distanceSq <= slopSq &&
                (best == GizmoHandle::None || distanceSq < bestDistanceSq)) {
                bestDistanceSq = distanceSq;
                best = handle;
            }
        }

        if (best != GizmoHandle::None) {
            return best;
        }
    }
    return GizmoHandle::None;
}

// ---------------------------------------------------------------------------
// The drag
// ---------------------------------------------------------------------------

bool GizmoSession::capturedBodyTransform(TransformValues* out) const {
    const SceneObject* body = scene_.findBody(objectId_);
    if (body == nullptr) {
        return false;
    }
    if (out) *out = body->transform().values();
    return true;
}

Vec3 GizmoSession::basisDirection(int axisIndex) const {
    if (axisIndex < 0 || axisIndex > 2) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return basis_.axis[axisIndex];
}

bool GizmoSession::applyTarget(const TransformValues& values) {
    SceneObject* body = scene_.findBody(objectId_);
    if (body == nullptr) {
        return false;
    }
    // THE placement seam, and the only path from a solved target to the
    // authoritative transform. See applyGizmoPlacementModifier.
    const TransformValues constrained =
        applyGizmoPlacementModifier(values, startValues_, dragMode_, handle_, dragSpace_);
    const TransformApplyResult result = applyTransformValues(body->transform(), constrained);
    return result.status == TransformUpdateStatus::Applied;
}

bool GizmoSession::beginDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX,
                             float screenY, int viewportWidth, int viewportHeight) {
    if (capturing_) {
        return false;
    }
    const GizmoHandle handle = hitTest(camera, screenX, screenY, viewportWidth, viewportHeight);
    if (handle == GizmoHandle::None) {
        // Nothing captured and — the part that matters — no edit opened. A touch
        // that misses every handle is an ordinary viewport gesture and must cost
        // the history nothing at all.
        return false;
    }
    const GizmoSnapshot state = snapshot(camera, viewportWidth, viewportHeight);
    if (!state.visible) {
        return false;
    }
    SceneObject* body = scene_.findBody(scene_.activeBodyId());
    if (body == nullptr) {
        return false;
    }

    Ray ray{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &ray)) {
        return false;
    }

    objectId_ = body->objectId();
    handle_ = handle;
    dragMode_ = state.mode;
    dragSpace_ = state.space;
    pivot_ = state.pivot;
    startValues_ = body->transform().values();
    basis_ = gizmoBasisFor(dragSpace_, startValues_);
    startRotation_ = rotationMatrixFromEuler(eulerOf(startValues_));
    lastEuler_ = eulerOf(startValues_);
    dragUpdates_ = 0;
    lastSolve_ = AxisSolveStatus::Resolved;
    haveRingSample_ = false;
    accumulatedAngle_ = 0.0f;
    lastRingAngle_ = 0.0f;
    startAxisT_ = 0.0f;

    // Everything a drag needs to be anchored on is worked out HERE, once, and a
    // failure to anchor refuses the capture outright. A drag anchored on a guess
    // would jump on its first move, which is the whole reason each of these
    // paths refuses rather than substituting something plausible.
    const int axisIndex = gizmoHandleAxisIndex(handle_);
    if (dragMode_ == GizmoMode::Move) {
        if (gizmoHandleIsPlane(handle_)) {
            const int normalIndex = gizmoPlaneNormalIndex(handle_);
            if (!intersectRayPlane(ray, pivot_, basisDirection(normalIndex), &startPlaneHit_)) {
                objectId_ = kNoObject;
                handle_ = GizmoHandle::None;
                return false;
            }
        } else {
            float t = 0.0f;
            const AxisSolveStatus status =
                solveAxisParameter(ray, pivot_, basisDirection(axisIndex), &t);
            lastSolve_ = status;
            if (status == AxisSolveStatus::Unresolvable) {
                objectId_ = kNoObject;
                handle_ = GizmoHandle::None;
                return false;
            }
            startAxisT_ = t;
        }
    } else if (dragMode_ == GizmoMode::Rotate) {
        ringNormal_ = basisDirection(axisIndex);
        Vec3 hit{};
        if (!intersectRayPlane(ray, pivot_, ringNormal_, &hit)) {
            objectId_ = kNoObject;
            handle_ = GizmoHandle::None;
            return false;
        }
        float angle = 0.0f;
        // The reference direction is arbitrary and cancels out: only DIFFERENCES
        // of this angle are ever used, so any fixed vector not parallel to the
        // ring normal would do. Only its own degeneracy matters.
        if (!signedAngleAround(ringNormal_, Vec3{1.0f, 0.0f, 0.0f}, vec3Sub(hit, pivot_),
                               &angle) &&
            !signedAngleAround(ringNormal_, Vec3{0.0f, 1.0f, 0.0f}, vec3Sub(hit, pivot_),
                               &angle)) {
            objectId_ = kNoObject;
            handle_ = GizmoHandle::None;
            return false;
        }
        lastRingAngle_ = angle;
        haveRingSample_ = true;
    } else {
        // Scale. The reference is a SCREEN direction and a SCREEN length: see
        // the scale mapping in the header.
        const float pixelsPerUnit = gizmoPixelsPerReferenceUnit();
        if (handle_ == GizmoHandle::Uniform) {
            // The uniform handle has no direction of its own, so the drag reads
            // the screen diagonal — right and up, which is "bigger" in every
            // tool that has one. Screen Y grows downward, hence the negative.
            scaleDirX_ = 0.70710678f;
            scaleDirY_ = -0.70710678f;
            scaleReferencePixels_ = kGizmoUniformScaleReferenceUnits * pixelsPerUnit;
        } else {
            // The DRAWN extent of this handle, not the point the finger landed
            // on: the reference length is "how long is this handle on screen",
            // so a shaft measures a full shaft and a plane square measures its
            // centre diagonal. Taking the grab point instead would make the
            // sensitivity depend on where along the handle the user grabbed.
            Vec3 reference{};
            bool haveReference = false;
            // The reference is measured on a CANONICAL snapshot -- the visual size
            // preference set aside -- so a plane handle's centre and a shaft's
            // length are the same ruler at every visual size.
            GizmoSnapshot canonical = state;
            canonical.visualScale = kGizmoDefaultVisualScale;
            if (gizmoHandleIsPlane(handle_)) {
                haveReference = gizmoHandleGrabPoint(canonical, handle_, &reference);
            } else {
                // At the CANONICAL scale, deliberately not the visual one: the
                // reference length is the ruler a scale drag is measured
                // against, and the visual size preference must change how the
                // instrument looks and never how far a drag stretches the body.
                // (A plane handle's reference is its centre, which the visual
                // scale does move; the uniform handle's is a constant.)
                reference = vec3Add(pivot_,
                                    vec3Scale(basisDirection(axisIndex),
                                              kGizmoHandleLengthUnits * state.worldPerReferenceUnit));
                haveReference = vec3Finite(reference);
            }
            float px = 0.0f, py = 0.0f, hx = 0.0f, hy = 0.0f;
            if (!haveReference ||
                !projectWorldToScreen(camera, pivot_, viewportWidth, viewportHeight, &px, &py) ||
                !projectWorldToScreen(camera, reference, viewportWidth, viewportHeight, &hx,
                                      &hy)) {
                objectId_ = kNoObject;
                handle_ = GizmoHandle::None;
                return false;
            }
            const float dx = hx - px;
            const float dy = hy - py;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (!std::isfinite(length) ||
                length < kGizmoScaleMinReferenceUnits * pixelsPerUnit) {
                // The handle points at the viewer: it has no usable screen
                // direction, and anchoring on one that is mostly rounding error
                // is exactly the jump this refusal prevents.
                objectId_ = kNoObject;
                handle_ = GizmoHandle::None;
                return false;
            }
            scaleDirX_ = dx / length;
            scaleDirY_ = dy / length;
            scaleReferencePixels_ = length;
        }
        if (!std::isfinite(scaleReferencePixels_) || scaleReferencePixels_ <= 0.0f) {
            objectId_ = kNoObject;
            handle_ = GizmoHandle::None;
            return false;
        }
        scaleDownX_ = screenX;
        scaleDownY_ = screenY;
    }

    // ONE edit for the whole drag, opened here and closed exactly once in
    // commitDrag or cancelDrag. Every update between them is an ordinary
    // mutation that joins it, which is what makes a thousand-sample drag one
    // history step without any counting.
    history_.beginEdit();
    capturing_ = true;
    pointerId_ = pointerId;
    return true;
}

bool GizmoSession::applyMoveSample(const Ray& ray) {
    // The world displacement this sample asks for, accumulated in DOUBLE so a
    // world-axis drag writes the same exact number the typed editor would.
    double offset[3] = {0.0, 0.0, 0.0};

    if (gizmoHandleIsPlane(handle_)) {
        int a = 0;
        int b = 0;
        gizmoPlaneAxisIndices(handle_, &a, &b);
        const int normalIndex = gizmoPlaneNormalIndex(handle_);
        Vec3 hit{};
        if (!intersectRayPlane(ray, pivot_, basisDirection(normalIndex), &hit)) {
            // Hold: the plane is edge-on to the pointer. The placement keeps its
            // last good value, which is a stationary handle rather than a jump.
            return false;
        }
        const Vec3 raw = vec3Sub(hit, startPlaneHit_);
        // PROJECTED onto the two allowed directions rather than used whole. The
        // third basis component is therefore untouched by construction and not
        // by a subsequent correction — which is what makes "a plane drag never
        // moves the body off its plane" a property rather than a promise.
        const Vec3 first = basisDirection(a);
        const Vec3 second = basisDirection(b);
        const double du = quantizeGizmoTranslation(static_cast<double>(vec3Dot(raw, first)));
        const double dv = quantizeGizmoTranslation(static_cast<double>(vec3Dot(raw, second)));
        if (!std::isfinite(du) || !std::isfinite(dv)) {
            return false;
        }
        offset[0] = static_cast<double>(first.x) * du + static_cast<double>(second.x) * dv;
        offset[1] = static_cast<double>(first.y) * du + static_cast<double>(second.y) * dv;
        offset[2] = static_cast<double>(first.z) * du + static_cast<double>(second.z) * dv;
    } else {
        const int axisIndex = gizmoHandleAxisIndex(handle_);
        const Vec3 direction = basisDirection(axisIndex);
        float t = 0.0f;
        const AxisSolveStatus status = solveAxisParameter(ray, pivot_, direction, &t);
        lastSolve_ = status;
        if (status == AxisSolveStatus::Unresolvable) {
            // Hold. Nothing non-finite can reach the transform, because nothing
            // is written at all.
            return false;
        }
        const double delta = quantizeGizmoTranslation(static_cast<double>(t - startAxisT_));
        if (!std::isfinite(delta)) {
            return false;
        }
        offset[0] = static_cast<double>(direction.x) * delta;
        offset[1] = static_cast<double>(direction.y) * delta;
        offset[2] = static_cast<double>(direction.z) * delta;
    }

    // From the START values every time, never from the current ones: an
    // incremental application would accumulate the solver own rounding over a
    // long drag, and a held sample would then be a permanent small error rather
    // than a pause.
    TransformValues values = startValues_;
    for (int i = 0; i < 3; ++i) {
        setTransformPositionAt(&values, i, transformPositionAt(startValues_, i) + offset[i]);
    }
    return applyTarget(values);
}

bool GizmoSession::applyRotateSample(const Ray& ray) {
    // The ring plane is edge-on to the pointer when the ring normal is
    // perpendicular to the ray. intersectRayPlane refuses that case, and
    // refusing IS the documented fallback: the drag holds its last valid sample,
    // so the body stops turning instead of spinning on noise, and resumes the
    // moment the camera or the finger gives the plane something to intersect.
    Vec3 hit{};
    if (!intersectRayPlane(ray, pivot_, ringNormal_, &hit)) {
        return false;
    }
    const Vec3 spoke = vec3Sub(hit, pivot_);
    float angle = 0.0f;
    if (!signedAngleAround(ringNormal_, Vec3{1.0f, 0.0f, 0.0f}, spoke, &angle) &&
        !signedAngleAround(ringNormal_, Vec3{0.0f, 1.0f, 0.0f}, spoke, &angle)) {
        return false;
    }
    if (!haveRingSample_) {
        lastRingAngle_ = angle;
        haveRingSample_ = true;
        return false;
    }
    // The unwrap is the whole continuity story: consecutive samples are far less
    // than half a turn apart, so the wrapped difference is the real motion.
    // Crossing +/-180 degrees is therefore not a jump, and the accumulator is
    // never itself reduced — so a drag past a full turn keeps going.
    const float step = unwrapAngleDelta(angle - lastRingAngle_);
    lastRingAngle_ = angle;
    accumulatedAngle_ += step;
    if (!std::isfinite(accumulatedAngle_)) {
        return false;
    }
    const double delta =
        quantizeGizmoRotation(static_cast<double>(radiansToDegrees(accumulatedAngle_)));
    if (!std::isfinite(delta)) {
        return false;
    }

    // The composition, from the IMMUTABLE start orientation every sample:
    //
    //     World:  R_target = Relem(A, delta) * R_start   (turn, then place)
    //     Local:  R_target = R_start * Relem(A, delta)   (place, then turn)
    //
    // Adding `delta` to one Euler component instead is only correct when the
    // other two are zero, and on a mixed orientation it turns the body about
    // neither the world axis nor the local one. That is the defect this stage
    // exists to remove, and the reason a mixed drag legitimately changes more
    // than one Euler field.
    const int axisIndex = gizmoHandleAxisIndex(handle_);
    const Mat4 elementary = elementaryRotationMatrix(axisIndex, delta);
    const Mat4 target = (dragSpace_ == GizmoSpace::World)
                            ? mat4Multiply(elementary, startRotation_)
                            : mat4Multiply(startRotation_, elementary);

    EulerDegrees euler{};
    // The branch nearest the LAST ACCEPTED answer, which is what keeps a drag
    // continuous through +/-180, past 360 and past 720, and keeps the fields the
    // drag is not moving reading exactly what they read before.
    if (!eulerFromRotationMatrix(target, lastEuler_, &euler)) {
        return false;
    }
    TransformValues values = startValues_;
    setEuler(&values, euler);
    const bool changed = applyTarget(values);
    // Advanced whenever the decomposition succeeded, not only when the transform
    // moved: a sample that resolved to the same numbers is still the anchor the
    // next one must be continuous with.
    lastEuler_ = euler;
    return changed;
}

bool GizmoSession::applyScaleSample(float screenX, float screenY) {
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return false;
    }
    // ONE formula for all three scale handles. See the scale mapping in the
    // header: measured from the pointer DOWN point, so the factor is exactly 1
    // at zero drag wherever on the handle the user grabbed.
    const double along = static_cast<double>(screenX - scaleDownX_) *
                             static_cast<double>(scaleDirX_) +
                         static_cast<double>(screenY - scaleDownY_) *
                             static_cast<double>(scaleDirY_);
    double factor = quantizeGizmoScale(1.0 + along / static_cast<double>(scaleReferencePixels_));
    if (!std::isfinite(factor)) {
        return false;
    }
    // Clamped strictly positive so a pointer dragged past the pivot pins the
    // body at a sliver rather than passing through zero into a mirror. A floor,
    // not a rounding rule.
    if (factor < kGizmoMinScaleFactor) {
        factor = kGizmoMinScaleFactor;
    }

    TransformValues values = startValues_;
    if (handle_ == GizmoHandle::Uniform) {
        // One factor on all three, so the existing ratios are preserved exactly:
        // a body already twice as tall as it is wide stays twice as tall.
        for (int i = 0; i < 3; ++i) {
            setTransformScaleAt(&values, i, transformScaleAt(startValues_, i) * factor);
        }
    } else if (gizmoHandleIsPlane(handle_)) {
        int a = 0;
        int b = 0;
        gizmoPlaneAxisIndices(handle_, &a, &b);
        setTransformScaleAt(&values, a, transformScaleAt(startValues_, a) * factor);
        setTransformScaleAt(&values, b, transformScaleAt(startValues_, b) * factor);
    } else {
        const int axisIndex = gizmoHandleAxisIndex(handle_);
        setTransformScaleAt(&values, axisIndex,
                            transformScaleAt(startValues_, axisIndex) * factor);
    }
    return applyTarget(values);
}

bool GizmoSession::updateDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX,
                              float screenY, int viewportWidth, int viewportHeight) {
    if (!capturing_ || pointerId != pointerId_) {
        return false;
    }
    // The body a drag started on is the only body it can ever move. If it left
    // the scene — an undo of its creation from anywhere else could do that — the
    // drag has nothing to act on and stops writing rather than retargeting to
    // whatever happens to be active now.
    TransformValues current{};
    if (!capturedBodyTransform(&current)) {
        return false;
    }

    bool changed = false;
    if (dragMode_ == GizmoMode::Scale) {
        // The only solver that works in screen space, because a scale factor is
        // a screen-space question: there is no world quantity a pointer offset
        // could be intersected against.
        changed = applyScaleSample(screenX, screenY);
    } else {
        Ray ray{};
        if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &ray)) {
            return false;
        }
        changed = (dragMode_ == GizmoMode::Move) ? applyMoveSample(ray) : applyRotateSample(ray);
    }
    if (changed) {
        ++dragUpdates_;
    }
    return changed;
}

bool GizmoSession::commitDrag() {
    if (!capturing_) {
        return false;
    }
    capturing_ = false;
    pointerId_ = -1;
    handle_ = GizmoHandle::None;
    // Exactly one commit, and it records a step only if the Construction state
    // genuinely differs from the pre-drag one. A tap, and a drag that came back
    // to where it started, both end here with nothing recorded and — crucially —
    // with the redo stack untouched.
    const bool recorded = history_.commitEdit();
    if (recorded) {
        ++committedDrags_;
    }
    return recorded;
}

void GizmoSession::cancelDrag() {
    if (!capturing_) {
        return;
    }
    capturing_ = false;
    pointerId_ = -1;
    handle_ = GizmoHandle::None;
    // The captured state goes back exactly, including the case where the drag
    // never moved anything. Nothing is recorded and the redo stack is left
    // alone: a cancelled drag is not a new branch of history.
    history_.cancelEdit();
}

// ---------------------------------------------------------------------------
// The one process-scoped session
// ---------------------------------------------------------------------------

GizmoSession& gizmoSession() {
    static GizmoSession session(constructionScene(), constructionHistory());
    return session;
}

}  // namespace forgeshape
