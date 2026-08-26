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

}  // namespace

// ---------------------------------------------------------------------------
// The two closed enums
// ---------------------------------------------------------------------------

const char* gizmoModeName(GizmoMode mode) {
    switch (mode) {
        case GizmoMode::Move: return "move";
        case GizmoMode::Rotate: return "rotate";
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
        default: return false;
    }
}

int gizmoModeIndex(GizmoMode mode) {
    return mode == GizmoMode::Rotate ? 1 : 0;
}

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
        case GizmoAxis::X: return Vec3{1.0f, 0.0f, 0.0f};
        case GizmoAxis::Y: return Vec3{0.0f, 1.0f, 0.0f};
        case GizmoAxis::Z: return Vec3{0.0f, 0.0f, 1.0f};
        case GizmoAxis::None: break;
    }
    return Vec3{0.0f, 0.0f, 0.0f};
}

Degrees transformRotationForAxis(const TransformValues& values, GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::X: return values.rotationX;
        case GizmoAxis::Y: return values.rotationY;
        case GizmoAxis::Z: return values.rotationZ;
        case GizmoAxis::None: break;
    }
    return 0.0;
}

void setTransformRotationForAxis(TransformValues* values, GizmoAxis axis, Degrees value) {
    if (values == nullptr) return;
    switch (axis) {
        case GizmoAxis::X: values->rotationX = value; break;
        case GizmoAxis::Y: values->rotationY = value; break;
        case GizmoAxis::Z: values->rotationZ = value; break;
        case GizmoAxis::None: break;
    }
}

Meters transformPositionForAxis(const TransformValues& values, GizmoAxis axis) {
    switch (axis) {
        case GizmoAxis::X: return values.positionX;
        case GizmoAxis::Y: return values.positionY;
        case GizmoAxis::Z: return values.positionZ;
        case GizmoAxis::None: break;
    }
    return 0.0;
}

void setTransformPositionForAxis(TransformValues* values, GizmoAxis axis, Meters value) {
    if (values == nullptr) return;
    switch (axis) {
        case GizmoAxis::X: values->positionX = value; break;
        case GizmoAxis::Y: values->positionY = value; break;
        case GizmoAxis::Z: values->positionZ = value; break;
        case GizmoAxis::None: break;
    }
}

// ---------------------------------------------------------------------------
// The adapter's one number
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
    // NDC to view-local pixels. The projection matrices already carry Vulkan's
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
// The quantization seam
// ---------------------------------------------------------------------------
//
// The identity, today and deliberately. See the header: the seam exists so a
// future approved Grid Snap contract lands here instead of in the gesture
// architecture, and there is no snapping, no setting and no indicator until one
// is approved.
Meters quantizeGizmoTranslation(Meters raw) { return raw; }
Degrees quantizeGizmoRotation(Degrees raw) { return raw; }

// ---------------------------------------------------------------------------
// The drawn geometry
// ---------------------------------------------------------------------------

namespace {

// The two unit vectors spanning the plane PERPENDICULAR to an axis. Taken from
// the axis enum's own partners rather than from a general orthonormal-basis
// routine, which would carry a degenerate case this closed set of three does
// not have.
void axisPlaneBasis(GizmoAxis axis, Vec3* outU, Vec3* outV) {
    switch (axis) {
        case GizmoAxis::X: *outU = Vec3{0, 1, 0}; *outV = Vec3{0, 0, 1}; break;
        case GizmoAxis::Y: *outU = Vec3{0, 0, 1}; *outV = Vec3{1, 0, 0}; break;
        default:           *outU = Vec3{1, 0, 0}; *outV = Vec3{0, 1, 0}; break;
    }
}

struct GizmoVertexWriter {
    GizmoVertex* out;
    int capacity;
    int written;

    void line(const Vec3& a, const Vec3& b, GizmoAxis axis) {
        if (written + 2 > capacity) {
            return;
        }
        const float tag = static_cast<float>(axis == GizmoAxis::X   ? 1
                                             : axis == GizmoAxis::Y ? 2
                                                                    : 3);
        const Vec3 ends[2] = {a, b};
        for (int i = 0; i < 2; ++i) {
            out[written].position[0] = ends[i].x;
            out[written].position[1] = ends[i].y;
            out[written].position[2] = ends[i].z;
            out[written].axis = tag;
            ++written;
        }
    }
};

}  // namespace

int generateGizmoVertices(GizmoVertex* out, int capacity) {
    if (out == nullptr || capacity < kGizmoVertexCount) {
        return 0;
    }
    GizmoVertexWriter writer{out, capacity, 0};
    const GizmoAxis axes[3] = {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};

    // --- Move: pivot marker, shafts, arrowheads --------------------------
    const float length = kGizmoHandleLengthUnits;
    const float arrow = length * kGizmoArrowLengthFraction;
    const float arrowHalfWidth = length * kGizmoArrowHalfWidthFraction;
    const float marker = length * kGizmoPivotMarkerFraction;
    for (int i = 0; i < 3; ++i) {
        const Vec3 direction = gizmoAxisDirection(axes[i]);
        // The pivot marker: a short arm through the origin on each axis, so the
        // point the transform is actually about is visible even when a shaft is
        // pointing away from the viewer and projects to almost nothing.
        writer.line(vec3Scale(direction, -marker), vec3Scale(direction, marker), axes[i]);

        Vec3 u{}, v{};
        axisPlaneBasis(axes[i], &u, &v);

        // The shaft, stopping where the arrowhead begins, drawn as a bundle: the
        // centre line plus four offset a stroke width around it. See
        // kGizmoStrokeOffsetUnits for why this is not one wide line.
        const Vec3 shaftEnd = vec3Scale(direction, length - arrow);
        const Vec3 strokeOffsets[kGizmoStrokeBundle] = {
            Vec3{0.0f, 0.0f, 0.0f},
            vec3Scale(u, kGizmoStrokeOffsetUnits),
            vec3Scale(u, -kGizmoStrokeOffsetUnits),
            vec3Scale(v, kGizmoStrokeOffsetUnits),
            vec3Scale(v, -kGizmoStrokeOffsetUnits),
        };
        for (int s = 0; s < kGizmoStrokeBundle; ++s) {
            writer.line(strokeOffsets[s], vec3Add(shaftEnd, strokeOffsets[s]), axes[i]);
        }

        // A four-line arrowhead rather than a cone: it reads as a direction from
        // every angle, costs four lines, and needs no second pipeline for solid
        // geometry.
        const Vec3 tip = vec3Scale(direction, length);
        const Vec3 base = vec3Scale(direction, length - arrow);
        const Vec3 spokes[4] = {vec3Scale(u, arrowHalfWidth), vec3Scale(u, -arrowHalfWidth),
                                vec3Scale(v, arrowHalfWidth), vec3Scale(v, -arrowHalfWidth)};
        for (int s = 0; s < 4; ++s) {
            writer.line(tip, vec3Add(base, spokes[s]), axes[i]);
        }
    }

    // --- Rotate: three world-axis rings ----------------------------------
    //
    // The ring PLANE is the axis's plane and stays there. It is deliberately not
    // billboarded toward the camera: a ring that turned to face the viewer would
    // stop showing which plane the rotation happens in, which is the one thing
    // it is there to say.
    // Two concentric passes a stroke width apart, for the legibility reason the
    // shafts are bundled. The HIT test still measures against the nominal radius
    // alone: the corridor is 48 units wide and a one-unit ring thickness is
    // inside it, so what is drawn and what can be grabbed do not disagree.
    const float radii[2] = {kGizmoRingRadiusUnits,
                            kGizmoRingRadiusUnits + kGizmoStrokeOffsetUnits};
    for (int i = 0; i < 3; ++i) {
        Vec3 u{}, v{};
        axisPlaneBasis(axes[i], &u, &v);
        for (int pass = 0; pass < 2; ++pass) {
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
                writer.line(p0, p1, axes[i]);
            }
        }
    }
    return writer.written;
}

Vec3 gizmoRingGrabOffset(GizmoAxis axis, float radius) {
    if (axis == GizmoAxis::None || !std::isfinite(radius)) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    Vec3 u{}, v{};
    axisPlaneBasis(axis, &u, &v);
    // 45 degrees between the plane's own two basis directions: the two ring
    // crossings sit exactly on those directions, so the bisector is the point on
    // this ring furthest from either of them. See the header.
    const float half = radius * 0.70710678f;
    return vec3Add(vec3Scale(u, half), vec3Scale(v, half));
}

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------

void gizmoAxisColor(ViewportBackground background, GizmoAxis axis, float* outRgba) {
    if (outRgba == nullptr) {
        return;
    }
    struct Palette {
        float x[4];
        float y[4];
        float z[4];
    };
    // The three hues extend the convention the world grid already draws with —
    // a warm X and a cool Z — by adding the Y the grid has no line for, and
    // raise all three to tool weight. A handle must read as an instrument the
    // user can grab, which a 0.5-alpha reference line does not.
    //
    // The two dark appearances share a palette: they differ in the ground they
    // are drawn on and not in what an axis MEANS, and giving them separate
    // handle hues would be three variants of one instrument for no reading gain.
    static const Palette kDark = {
        {0.94f, 0.42f, 0.36f, 0.95f},
        {0.52f, 0.86f, 0.44f, 0.95f},
        {0.40f, 0.62f, 0.96f, 0.95f},
    };
    // Darker and more saturated on the light ground, so contrast against the
    // background is comparable rather than the hue being nominally "the same".
    static const Palette kLight = {
        {0.80f, 0.22f, 0.18f, 0.95f},
        {0.16f, 0.56f, 0.24f, 0.95f},
        {0.13f, 0.40f, 0.84f, 0.95f},
    };

    const Palette& palette =
        (background == ViewportBackground::LightCharcoal) ? kLight : kDark;
    const float* source = palette.x;
    switch (axis) {
        case GizmoAxis::Y: source = palette.y; break;
        case GizmoAxis::Z: source = palette.z; break;
        case GizmoAxis::X:
        case GizmoAxis::None: break;
    }
    for (int i = 0; i < 4; ++i) {
        outRgba[i] = source[i];
    }
}

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
        // Move on entry, every time. See mode_.
        mode_ = GizmoMode::Move;
    }
}

bool GizmoSession::setMode(GizmoMode mode) {
    if (capturing_) {
        return false;
    }
    mode_ = mode;
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
    // The mode being DRAWN while a drag is live is the drag's mode, which cannot
    // differ from mode_ because setMode is refused mid-drag — stated here so a
    // future relaxation of that refusal cannot silently draw the wrong handles.
    out.mode = capturing_ ? dragMode_ : mode_;
    out.activeAxis = capturing_ ? axis_ : GizmoAxis::None;
    out.pivot = pivot;
    out.worldPerReferenceUnit = scale;
    return out;
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------
//
// In PIXELS, against the same geometry the renderer draws at the same scale.
// The alternative — three ray/cylinder and three ray/torus tests in world space
// — answers a different question from the one the user is asking, which is "is
// my finger on that line I can see". Measuring in pixels is also what makes the
// 48-unit floor a number this file can state rather than an aspiration.
GizmoAxis GizmoSession::hitTest(const CameraSnapshot& camera, float screenX, float screenY,
                                int viewportWidth, int viewportHeight) const {
    const GizmoSnapshot state = snapshot(camera, viewportWidth, viewportHeight);
    if (!state.visible || !std::isfinite(screenX) || !std::isfinite(screenY)) {
        return GizmoAxis::None;
    }
    const float slopPixels = kGizmoHitSlopUnits * gizmoPixelsPerReferenceUnit();
    const float slopSq = slopPixels * slopPixels;

    GizmoAxis best = GizmoAxis::None;
    float bestDistanceSq = slopSq;

    const GizmoAxis axes[3] = {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};
    for (int i = 0; i < 3; ++i) {
        const Vec3 direction = gizmoAxisDirection(axes[i]);
        float distanceSq = 0.0f;
        bool measured = false;

        if (state.mode == GizmoMode::Move) {
            const float length = kGizmoHandleLengthUnits * state.worldPerReferenceUnit;
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
        } else {
            // The ring, as the polyline it is drawn as. An edge-on ring projects
            // to a segment and this still measures it correctly, which is what
            // keeps a ring grabbable from every camera angle instead of only the
            // face-on ones.
            const float radius = kGizmoRingRadiusUnits * state.worldPerReferenceUnit;
            Vec3 u{}, v{};
            // Two unit vectors spanning the ring's plane. Which two does not
            // matter — the ring is a circle — so they are taken from the axis
            // enum's own partners rather than from a general orthonormal basis
            // routine that would have its own degenerate case.
            switch (axes[i]) {
                case GizmoAxis::X: u = Vec3{0, 1, 0}; v = Vec3{0, 0, 1}; break;
                case GizmoAxis::Y: u = Vec3{0, 0, 1}; v = Vec3{1, 0, 0}; break;
                default:           u = Vec3{1, 0, 0}; v = Vec3{0, 1, 0}; break;
            }
            float previousX = 0.0f, previousY = 0.0f;
            bool havePrevious = false;
            float ringBest = slopSq;
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
                    const float d =
                        pointSegmentDistanceSq(screenX, screenY, previousX, previousY, px, py);
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
        }

        // Strictly nearer wins, so X beats Y beats Z on an exact tie and the
        // answer is deterministic rather than dependent on iteration order
        // being read as significant.
        if (measured && distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = axes[i];
        }
    }
    return best;
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

bool GizmoSession::beginDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX,
                             float screenY, int viewportWidth, int viewportHeight) {
    if (capturing_) {
        return false;
    }
    const GizmoAxis axis = hitTest(camera, screenX, screenY, viewportWidth, viewportHeight);
    if (axis == GizmoAxis::None) {
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
    axis_ = axis;
    dragMode_ = state.mode;
    pivot_ = state.pivot;
    startValues_ = body->transform().values();
    dragUpdates_ = 0;
    lastSolve_ = AxisSolveStatus::Resolved;
    haveRingSample_ = false;
    accumulatedAngle_ = 0.0f;
    lastRingAngle_ = 0.0f;
    startAxisT_ = 0.0f;

    const Vec3 direction = gizmoAxisDirection(axis_);
    if (dragMode_ == GizmoMode::Move) {
        float t = 0.0f;
        const AxisSolveStatus status = solveAxisParameter(ray, pivot_, direction, &t);
        lastSolve_ = status;
        if (status == AxisSolveStatus::Unresolvable) {
            // The handle is drawn and was hit, but the axis points so nearly at
            // the viewer that no starting parameter exists. Refusing the capture
            // is the honest answer: a drag anchored on a guess would jump on its
            // first move.
            objectId_ = kNoObject;
            axis_ = GizmoAxis::None;
            return false;
        }
        startAxisT_ = t;
    } else {
        Vec3 hit{};
        if (!intersectRayPlane(ray, pivot_, direction, &hit)) {
            objectId_ = kNoObject;
            axis_ = GizmoAxis::None;
            return false;
        }
        float angle = 0.0f;
        if (!signedAngleAround(direction, Vec3{1.0f, 0.0f, 0.0f}, vec3Sub(hit, pivot_), &angle)) {
            // The reference direction is arbitrary and cancels out: only
            // DIFFERENCES of this angle are ever used, so any fixed vector not
            // parallel to the axis would do. Only its own degeneracy matters.
            const Vec3 alternate{0.0f, 1.0f, 0.0f};
            if (!signedAngleAround(direction, alternate, vec3Sub(hit, pivot_), &angle)) {
                objectId_ = kNoObject;
                axis_ = GizmoAxis::None;
                return false;
            }
        }
        lastRingAngle_ = angle;
        haveRingSample_ = true;
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
    const Vec3 direction = gizmoAxisDirection(axis_);
    float t = 0.0f;
    const AxisSolveStatus status = solveAxisParameter(ray, pivot_, direction, &t);
    lastSolve_ = status;
    if (status == AxisSolveStatus::Unresolvable) {
        // Hold. The placement keeps its last good value, which is a stationary
        // handle rather than a jump — and nothing non-finite can reach the
        // transform, because nothing is written at all.
        return false;
    }
    const double delta = quantizeGizmoTranslation(static_cast<double>(t - startAxisT_));
    if (!std::isfinite(delta)) {
        return false;
    }
    TransformValues values = startValues_;
    setTransformPositionForAxis(
        &values, axis_, transformPositionForAxis(startValues_, axis_) + delta);
    // From the START values every time, never from the current ones: an
    // incremental application would accumulate the solver's own rounding over a
    // long drag, and a held sample would then be a permanent small error rather
    // than a pause.
    SceneObject* body = scene_.findBody(objectId_);
    if (body == nullptr) {
        return false;
    }
    const TransformApplyResult result = applyTransformValues(body->transform(), values);
    return result.status == TransformUpdateStatus::Applied;
}

bool GizmoSession::applyRotateSample(const Ray& ray) {
    const Vec3 direction = gizmoAxisDirection(axis_);
    // The ring plane is edge-on to the pointer when the axis is perpendicular to
    // the ray. intersectRayPlane refuses that case, and refusing IS the
    // documented fallback: the drag holds its last valid sample, so the body
    // stops moving instead of spinning on noise, and resumes the moment the
    // camera or the finger gives the plane something to intersect again.
    Vec3 hit{};
    if (!intersectRayPlane(ray, pivot_, direction, &hit)) {
        return false;
    }
    const Vec3 spoke = vec3Sub(hit, pivot_);
    float angle = 0.0f;
    if (!signedAngleAround(direction, Vec3{1.0f, 0.0f, 0.0f}, spoke, &angle) &&
        !signedAngleAround(direction, Vec3{0.0f, 1.0f, 0.0f}, spoke, &angle)) {
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
    TransformValues values = startValues_;
    // Added to the START value and never canonicalised, so 350 + 30 is 380 and
    // the Property Inspector reads back 380 — the exact-transform convention
    // keeps what the user did rather than reducing it modulo a turn.
    setTransformRotationForAxis(
        &values, axis_, transformRotationForAxis(startValues_, axis_) + delta);
    SceneObject* body = scene_.findBody(objectId_);
    if (body == nullptr) {
        return false;
    }
    const TransformApplyResult result = applyTransformValues(body->transform(), values);
    return result.status == TransformUpdateStatus::Applied;
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
    Ray ray{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &ray)) {
        return false;
    }
    const bool changed = (dragMode_ == GizmoMode::Move) ? applyMoveSample(ray)
                                                        : applyRotateSample(ray);
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
    axis_ = GizmoAxis::None;
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
    axis_ = GizmoAxis::None;
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
