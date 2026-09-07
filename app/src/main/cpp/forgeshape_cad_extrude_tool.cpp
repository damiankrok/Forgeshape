#include "forgeshape_cad_extrude_tool.h"

#include <cmath>

#include "forgeshape_picking.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {
namespace {

// One world-space line, in the gizmo vertex layout the sketch overlay borrows:
// `axis` 0 is the neutral hue and `handle` 1 is the emphasis tag.
void pushLine(std::vector<GizmoVertex>* out, const Vec3& a, const Vec3& b, float handle) {
    if (!vec3Finite(a) || !vec3Finite(b)) {
        return;
    }
    out->push_back(GizmoVertex{{a.x, a.y, a.z}, 0.0f, handle});
    out->push_back(GizmoVertex{{b.x, b.y, b.z}, 0.0f, handle});
}

// Two unit vectors perpendicular to `axis` and to each other. Deterministic:
// the seed is chosen by which component of the axis is smallest, so a
// principal-axis normal never seeds from a parallel vector.
void perpendicularBasis(const Vec3& axis, Vec3* outP, Vec3* outQ) {
    const float ax = std::fabs(axis.x);
    const float ay = std::fabs(axis.y);
    const float az = std::fabs(axis.z);
    Vec3 seed{1.0f, 0.0f, 0.0f};
    if (ax <= ay && ax <= az) {
        seed = Vec3{1.0f, 0.0f, 0.0f};
    } else if (ay <= az) {
        seed = Vec3{0.0f, 1.0f, 0.0f};
    } else {
        seed = Vec3{0.0f, 0.0f, 1.0f};
    }
    *outP = vec3Normalize(vec3Cross(axis, seed));
    *outQ = vec3Normalize(vec3Cross(axis, *outP));
}

// Distance in pixels from `p` to the segment `a`-`b`, and where along it the
// closest point fell (0 at `a`, 1 at `b`, unclamped).
float distanceToSegment(float px, float py, float ax, float ay, float bx, float by,
                        float* outParam) {
    const float dx = bx - ax;
    const float dy = by - ay;
    const float lengthSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSq > 1.0e-6f) {
        t = ((px - ax) * dx + (py - ay) * dy) / lengthSq;
    }
    const float clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const float cx = ax + dx * clamped;
    const float cy = ay + dy * clamped;
    if (outParam != nullptr) {
        *outParam = t;
    }
    return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}


// Bounded to a range. Named as `forgeshape_camera.cpp`'s own file-local helper
// is, and kept file-local for the same reason: a two-line clamp is not a math
// module's business.
float clampToRange(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Whether a vector came back from vec3Normalize as a real unit vector. The
// normalizer answers the zero vector for a zero input rather than failing, so
// this is how a degenerate frame axis is told from a good one.
bool unitLength(const Vec3& v) {
    return vec3Finite(v) && std::fabs(vec3Dot(v, v) - 1.0f) < 1.0e-3f;
}
}  // namespace

// ---------------------------------------------------------------------------
// Anchors
// ---------------------------------------------------------------------------

bool sketchPolygonCentroid(const std::vector<SketchPoint>& polygon, SketchPoint* out) {
    if (out == nullptr || polygon.size() < 3) {
        return false;
    }
    // The standard polygon area centroid. Every term is a product of authored
    // coordinates, so the result carries no camera and no zoom.
    double twiceArea = 0.0;
    double cu = 0.0;
    double cv = 0.0;
    const size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[(i + 1) % n];
        if (!std::isfinite(a.u) || !std::isfinite(a.v)) {
            return false;
        }
        const double cross = a.u * b.v - b.u * a.v;
        twiceArea += cross;
        cu += (a.u + b.u) * cross;
        cv += (a.v + b.v) * cross;
    }
    if (std::fabs(twiceArea) < 1.0e-12) {
        // A profile this degenerate is already refused by the extraction; the
        // vertex mean is a guard so no caller can divide by zero here.
        double su = 0.0;
        double sv = 0.0;
        for (const SketchPoint& p : polygon) {
            su += p.u;
            sv += p.v;
        }
        out->u = su / static_cast<double>(n);
        out->v = sv / static_cast<double>(n);
        return std::isfinite(out->u) && std::isfinite(out->v);
    }
    out->u = cu / (3.0 * twiceArea);
    out->v = cv / (3.0 * twiceArea);
    return std::isfinite(out->u) && std::isfinite(out->v);
}

bool cadExtrudeAnchors(const SketchFrame& frame, const ClosedProfile& profile,
                       const ExtrudeFeature& extrude, CadExtrudeAnchors* out) {
    if (out == nullptr) {
        return false;
    }
    const Meters positive = extrudePositiveDistance(extrude);
    const Meters negative = extrudeNegativeDistance(extrude);
    if (!std::isfinite(positive) || !std::isfinite(negative) || positive < 0.0 || negative < 0.0
        || positive + negative <= 0.0) {
        return false;
    }
    if (!vec3Finite(frame.origin) || !vec3Finite(frame.u) || !vec3Finite(frame.v)
        || !vec3Finite(frame.n)) {
        return false;
    }
    const Vec3 normal = vec3Normalize(frame.n);
    if (std::fabs(vec3Dot(normal, normal) - 1.0f) > 1.0e-3f) {
        return false;  // a degenerate normal is refused rather than invented
    }
    SketchPoint centroid;
    if (!sketchPolygonCentroid(profile.polygon, &centroid)) {
        return false;
    }
    CadExtrudeAnchors built;
    built.normal = normal;
    // The centroid carried onto the authoring frame, exactly the way every
    // sketch point reaches world space.
    built.base = vec3Add(frame.origin,
                         vec3Add(vec3Scale(frame.u, static_cast<float>(centroid.u)),
                                 vec3Scale(frame.v, static_cast<float>(centroid.v))));
    if (!vec3Finite(built.base)) {
        return false;
    }
    const Vec3 against = vec3Scale(normal, -1.0f);
    built.positive.axis = normal;
    built.positive.distance = positive;
    built.positive.present = positive > 0.0;
    built.positive.tip = vec3Add(built.base, vec3Scale(normal, static_cast<float>(positive)));
    built.positive.label =
            vec3Add(built.base, vec3Scale(normal, static_cast<float>(positive * 0.5)));
    built.negative.axis = against;
    built.negative.distance = negative;
    built.negative.present = negative > 0.0;
    built.negative.tip = vec3Add(built.base, vec3Scale(against, static_cast<float>(negative)));
    built.negative.label =
            vec3Add(built.base, vec3Scale(against, static_cast<float>(negative * 0.5)));
    if (!vec3Finite(built.positive.tip) || !vec3Finite(built.positive.label)
        || !vec3Finite(built.negative.tip) || !vec3Finite(built.negative.label)) {
        return false;
    }
    // The PRIMARY mirror: the side the mode's primary distance is on. For a One
    // Side extrusion that is the solid's own side, which is exactly what the
    // single-arrow fields meant before `CAD-EXT-R1`.
    built.primaryIsPositive = extrude.extent != ExtrudeExtentMode::OneSide
                              || extrude.direction == ExtrudeDirection::AlongNormal;
    const CadExtrudeSideAnchor& primary = built.side(built.primaryIsPositive);
    built.axis = primary.axis;
    built.tip = primary.tip;
    built.label = primary.label;
    built.depth = primary.distance;
    built.valid = true;
    *out = built;
    return true;
}

// ---------------------------------------------------------------------------
// The camera-attached scale
// ---------------------------------------------------------------------------

bool cadExtrudeControlScaleFor(float metersPerPixel, CadExtrudeControlScale* out) {
    if (out == nullptr || !std::isfinite(metersPerPixel) || metersPerPixel <= 0.0f) {
        return false;
    }
    CadExtrudeControlScale built;
    built.metersPerPixel = metersPerPixel;
    const float worldPixels = kCadExtrudeControlWorldMeters / metersPerPixel;
    if (!std::isfinite(worldPixels)) {
        return false;
    }
    built.unclampedScale = worldPixels / kCadExtrudeControlReferencePixels;
    built.scale = built.unclampedScale;
    if (built.scale < kCadExtrudeControlMinScale) {
        built.scale = kCadExtrudeControlMinScale;
        built.clampedLow = true;
    } else if (built.scale > kCadExtrudeControlMaxScale) {
        built.scale = kCadExtrudeControlMaxScale;
        built.clampedHigh = true;
    }
    built.pixels = built.scale * kCadExtrudeControlReferencePixels;
    built.world = built.pixels * metersPerPixel;
    if (!std::isfinite(built.pixels) || !std::isfinite(built.world) || built.world <= 0.0f) {
        return false;
    }
    built.valid = true;
    *out = built;
    return true;
}

bool cadExtrudeControlScale(const CameraSnapshot& camera, const Vec3& anchor, int viewportHeight,
                            CadExtrudeControlScale* out) {
    float perPixel = 0.0f;
    if (!worldMetersPerPixel(camera, anchor, viewportHeight, &perPixel)) {
        return false;
    }
    return cadExtrudeControlScaleFor(perPixel, out);
}

// ---------------------------------------------------------------------------
// The manipulator
// ---------------------------------------------------------------------------

bool CadExtrudeManipulator::hitTest(const CadExtrudeAnchors& anchors, const CameraSnapshot& camera,
                                    float x, float y, int viewportWidth, int viewportHeight,
                                    bool* outPositiveSide) const {
    if (!anchors.valid) {
        return false;
    }
    // The PRIMARY side first, deterministically: a Symmetric extrusion seen
    // almost edge-on projects two arrows into overlapping corridors, and a
    // stated preference is better than whichever the loop happened to reach.
    const bool order[2] = {anchors.primaryIsPositive, !anchors.primaryIsPositive};
    for (const bool positiveSide : order) {
        if (hitTestSide(anchors, positiveSide, camera, x, y, viewportWidth, viewportHeight)) {
            if (outPositiveSide != nullptr) {
                *outPositiveSide = positiveSide;
            }
            return true;
        }
    }
    return false;
}

bool CadExtrudeManipulator::hitTestSide(const CadExtrudeAnchors& anchors, bool positiveSide,
                                        const CameraSnapshot& camera, float x, float y,
                                        int viewportWidth, int viewportHeight) const {
    if (!anchors.valid || viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
    }
    const CadExtrudeSideAnchor& side = anchors.side(positiveSide);
    if (!side.present) {
        return false;  // a side with no extent has no arrow to have been hit
    }
    CadExtrudeControlScale scale;
    if (!cadExtrudeControlScale(camera, anchors.base, viewportHeight, &scale)) {
        return false;
    }
    // The head is drawn past the tip at the SHARED scale, so the grabbable
    // extent past the tip is exactly what was drawn there.
    const Vec3 headEnd = vec3Add(
            side.tip,
            vec3Scale(side.axis,
                      static_cast<float>(scale.world * kCadExtrudeArrowHeadLengthFraction)));
    float baseX = 0.0f;
    float baseY = 0.0f;
    float endX = 0.0f;
    float endY = 0.0f;
    if (!projectWorldToScreen(camera, anchors.base, viewportWidth, viewportHeight, &baseX, &baseY)
        || !projectWorldToScreen(camera, headEnd, viewportWidth, viewportHeight, &endX, &endY)) {
        return false;
    }
    const float corridor = kCadExtrudeGrabRadiusUnits * gizmoPixelsPerReferenceUnit();
    float param = 0.0f;
    const float distance = distanceToSegment(x, y, baseX, baseY, endX, endY, &param);
    if (!std::isfinite(distance)) {
        return false;
    }
    // Anywhere along the drawn arrow takes it, and the corridor rounds off both
    // ends -- a finger just short of the base or just past the head is nearer to
    // the arrow than to anything else, and refusing it would make the control
    // feel smaller than it looks.
    return distance <= corridor;
}

bool CadExtrudeManipulator::beginDrag(int32_t pointerId, const CadExtrudeAnchors& anchors,
                                      bool positiveSide, const CameraSnapshot& camera, float x,
                                      float y, int viewportWidth, int viewportHeight) {
    if (!anchors.valid || viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
    }
    const CadExtrudeSideAnchor& side = anchors.side(positiveSide);
    if (!side.present) {
        return false;
    }
    Ray ray;
    if (!buildPickRay(camera, x, y, viewportWidth, viewportHeight, &ray)) {
        return false;
    }
    float t = 0.0f;
    const AxisSolveStatus status = solveAxisParameter(ray, anchors.base, side.axis, &t);
    if (status == AxisSolveStatus::Unresolvable || !std::isfinite(t)) {
        // Nothing is captured: a drag whose very first sample cannot be solved
        // has no basis to freeze, and inventing one is the jump this refuses.
        return false;
    }
    pointerId_ = pointerId;
    base_ = anchors.base;
    axis_ = side.axis;
    positiveSide_ = positiveSide;
    depthAtDown_ = side.distance;
    axisAtDown_ = t;
    lastGoodDepth_ = side.distance;
    lastSolve_ = status;
    ++dragCount_;
    return true;
}

bool CadExtrudeManipulator::updateDrag(int32_t pointerId, const CameraSnapshot& camera, float x,
                                       float y, int viewportWidth, int viewportHeight,
                                       Meters* outDistance) {
    if (pointerId_ < 0 || pointerId != pointerId_ || outDistance == nullptr || viewportWidth <= 0
        || viewportHeight <= 0) {
        return false;
    }
    Ray ray;
    if (!buildPickRay(camera, x, y, viewportWidth, viewportHeight, &ray)) {
        return false;
    }
    float t = 0.0f;
    // The basis is the frozen one, never re-read from the live anchors: the
    // arrow grows as the depth changes, and solving against the moved tip would
    // make one finger displacement mean a different amount at the start and the
    // end of the same drag.
    const AxisSolveStatus status = solveAxisParameter(ray, base_, axis_, &t);
    lastSolve_ = status;
    if (status == AxisSolveStatus::Unresolvable || !std::isfinite(t)) {
        return false;  // hold the last good value
    }
    const double moved = static_cast<double>(t) - static_cast<double>(axisAtDown_);
    double depth = depthAtDown_ + moved;
    if (!std::isfinite(depth)) {
        return false;
    }
    if (depth < kCadExtrudeMinDragDepthMeters) {
        depth = kCadExtrudeMinDragDepthMeters;
    }
    lastGoodDepth_ = depth;
    *outDistance = depth;
    return true;
}

void CadExtrudeManipulator::endDrag() { pointerId_ = -1; }

bool CadExtrudeManipulator::cancelDrag(Meters* outRestoreDistance) {
    if (pointerId_ < 0) {
        return false;
    }
    pointerId_ = -1;
    if (outRestoreDistance != nullptr) {
        *outRestoreDistance = depthAtDown_;
    }
    return true;
}

// ---------------------------------------------------------------------------
// The arrow
// ---------------------------------------------------------------------------

namespace {

// One side's shaft and head. The base tick belongs to the BASE rather than to a
// side and is drawn once by the caller, so a two-sided extrusion does not stack
// two ticks on one point.
void appendOneArrow(std::vector<GizmoVertex>* out, const Vec3& base,
                    const CadExtrudeSideAnchor& side, double controlWorld, float handle) {
    if (!side.present) {
        return;
    }
    const double headLength = controlWorld * kCadExtrudeArrowHeadLengthFraction;
    const double headHalf = controlWorld * kCadExtrudeArrowHeadHalfWidthFraction;

    // The shaft IS the extrusion: base to tip, and its length is this side's
    // distance. It is the one part of this drawing that does not scale with the
    // camera.
    pushLine(out, base, side.tip, handle);

    Vec3 p;
    Vec3 q;
    perpendicularBasis(side.axis, &p, &q);
    if (!vec3Finite(p) || !vec3Finite(q)) {
        return;
    }

    // The head: barbs from a ring behind the tip forward to the point, plus the
    // ring itself, so it reads as a cone rather than as a flat chevron.
    const Vec3 headBase =
            vec3Add(side.tip, vec3Scale(side.axis, -static_cast<float>(headLength)));
    const Vec3 point = vec3Add(side.tip, vec3Scale(side.axis, static_cast<float>(headLength)));
    Vec3 previous{};
    Vec3 first{};
    for (int i = 0; i < kCadExtrudeArrowBarbs; ++i) {
        const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i)
                             / static_cast<double>(kCadExtrudeArrowBarbs);
        const Vec3 offset = vec3Add(vec3Scale(p, static_cast<float>(std::cos(angle) * headHalf)),
                                    vec3Scale(q, static_cast<float>(std::sin(angle) * headHalf)));
        const Vec3 ring = vec3Add(headBase, offset);
        pushLine(out, ring, point, handle);
        if (i == 0) {
            first = ring;
        } else {
            pushLine(out, previous, ring, handle);
        }
        previous = ring;
    }
    pushLine(out, previous, first, handle);
}

}  // namespace

void appendCadExtrudeArrow(std::vector<GizmoVertex>* out, const CadExtrudeAnchors& anchors,
                           double controlWorld, bool grabbed, bool grabbedSide) {
    if (out == nullptr || !anchors.valid || !std::isfinite(controlWorld) || controlWorld <= 0.0) {
        return;
    }
    const float positiveHandle = (grabbed && grabbedSide) ? 1.0f : 0.0f;
    const float negativeHandle = (grabbed && !grabbedSide) ? 1.0f : 0.0f;
    appendOneArrow(out, anchors.base, anchors.positive, controlWorld, positiveHandle);
    appendOneArrow(out, anchors.base, anchors.negative, controlWorld, negativeHandle);

    // The base tick, across the plane normal in the drawing convention the
    // dimension annotations use: it says the measurement starts here. ONE tick,
    // on the sketch plane, because both sides measure from the same place.
    const double tick = controlWorld * kCadExtrudeArrowBaseTickFraction;
    Vec3 p;
    Vec3 q;
    perpendicularBasis(anchors.normal, &p, &q);
    if (!vec3Finite(p) || !vec3Finite(q)) {
        return;
    }
    const float handle = grabbed ? 1.0f : 0.0f;
    pushLine(out, vec3Add(anchors.base, vec3Scale(p, -static_cast<float>(tick))),
             vec3Add(anchors.base, vec3Scale(p, static_cast<float>(tick))), handle);
    pushLine(out, vec3Add(anchors.base, vec3Scale(q, -static_cast<float>(tick))),
             vec3Add(anchors.base, vec3Scale(q, static_cast<float>(tick))), handle);
}


// ---------------------------------------------------------------------------
// Where the extrusion can be seen from (`CAD-UX-S1-C1`)
// ---------------------------------------------------------------------------

const char* cadFeatureViewSourceName(CadFeatureViewSource source) {
    switch (source) {
        case CadFeatureViewSource::PriorView: return "PriorView";
        case CadFeatureViewSource::ObliqueFallback: return "ObliqueFallback";
        case CadFeatureViewSource::Unavailable: return "Unavailable";
    }
    return "Unavailable";
}

Vec3 cadFeatureViewDirection(float yaw, float pitch) {
    // The orbit convention verbatim: x = cos(pitch)sin(yaw), y = sin(pitch),
    // z = cos(pitch)cos(yaw), pointing target -> eye.
    const float cp = std::cos(pitch);
    return Vec3{cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw)};
}

bool cadFeatureViewYawPitch(const Vec3& direction, float* outYaw, float* outPitch) {
    if (outYaw == nullptr || outPitch == nullptr) {
        return false;
    }
    const Vec3 d = vec3Normalize(direction);
    if (!vec3Finite(d)) {
        return false;
    }
    const float horizontal = std::sqrt(d.x * d.x + d.z * d.z);
    if (horizontal < 1.0e-4f) {
        // Straight up or straight down: yaw is the gimbal singularity the orbit
        // pose cannot express, and guessing one is what `frameSketchView` was
        // written to avoid. Refused rather than defaulted to zero.
        return false;
    }
    *outPitch = std::asin(clampToRange(d.y, -1.0f, 1.0f));
    *outYaw = std::atan2(d.x, d.z);
    return true;
}

float cadFeatureViewAxisSine(const Vec3& viewDirection, const Vec3& axis) {
    const Vec3 v = vec3Normalize(viewDirection);
    const Vec3 a = vec3Normalize(axis);
    if (!vec3Finite(v) || !vec3Finite(a)) {
        return 0.0f;
    }
    const float c = clampToRange(std::fabs(vec3Dot(v, a)), 0.0f, 1.0f);
    return std::sqrt(std::fmax(0.0f, 1.0f - c * c));
}

bool cadFeatureViewUsable(const Vec3& viewDirection, const Vec3& axis) {
    return cadFeatureViewAxisSine(viewDirection, axis) >= kCadFeatureViewMinAxisSine;
}

CadFeatureViewSource cadFeatureViewPose(const CameraController::Pose& current,
                                        const CameraController::Pose* prior,
                                        const SketchFrame& frame,
                                        const CadExtrudeAnchors& anchors,
                                        CameraController::Pose* out) {
    if (out == nullptr || !anchors.valid) {
        return CadFeatureViewSource::Unavailable;
    }
    const Vec3 axis = vec3Normalize(anchors.axis);
    if (!vec3Finite(axis) || !vec3Finite(anchors.base)) {
        return CadFeatureViewSource::Unavailable;
    }

    // 1. The user's own view, if it can already see the axis.
    //
    // Given back whole -- its direction, its distance, its projection and its
    // span are the user's and are not second-guessed -- with the target moved
    // onto the work so the arrow is certainly in frame.
    if (prior != nullptr && std::isfinite(prior->yaw) && std::isfinite(prior->pitch)) {
        const Vec3 priorDirection = cadFeatureViewDirection(prior->yaw, prior->pitch);
        if (cadFeatureViewUsable(priorDirection, axis)) {
            *out = *prior;
            out->target = anchors.base;
            return CadFeatureViewSource::PriorView;
        }
    }

    // 2. The deterministic oblique fallback, derived from the support frame
    //    alone.
    //
    // The eye stays on the `+n` side the sketch was seen from -- crossing the
    // plane would put the drawing behind the solid and read as a flip rather
    // than as a tilt -- and leans by a fixed angle in the frame's own (u, v).
    // No world up enters the construction, so a support normal that IS world
    // up is not a special case here; it becomes one only in the orbit pose
    // below, which is why the result is re-measured after the clamp.
    const Vec3 n = vec3Normalize(frame.n);
    const Vec3 u = vec3Normalize(frame.u);
    const Vec3 v = vec3Normalize(frame.v);
    // A zero-length axis normalizes to zero rather than failing, so a
    // degenerate frame would otherwise build its lean out of u and v alone and
    // hand back a view perpendicular to the axis -- a confident answer to a
    // question with no answer. Refused instead.
    if (!unitLength(n) || !unitLength(u) || !unitLength(v)) {
        return CadFeatureViewSource::Unavailable;
    }
    const float cosTheta = std::cos(kCadFeatureViewObliqueRadians);
    const float sinTheta = std::sin(kCadFeatureViewObliqueRadians);
    const float quarterTurn = 1.57079632679489661923f;
    for (int attempt = 0; attempt < kCadFeatureViewAzimuthAttempts; ++attempt) {
        const float psi = kCadFeatureViewAzimuthRadians + static_cast<float>(attempt) * quarterTurn;
        const Vec3 lean = vec3Add(vec3Scale(u, sinTheta * std::cos(psi)),
                                  vec3Scale(v, sinTheta * std::sin(psi)));
        const Vec3 candidate = vec3Normalize(vec3Add(vec3Scale(n, cosTheta), lean));
        float yaw = 0.0f;
        float pitch = 0.0f;
        if (!cadFeatureViewYawPitch(candidate, &yaw, &pitch)) {
            continue;
        }
        // The orbit pose clamps pitch, so what the camera will ACTUALLY look
        // along is the direction rebuilt from the clamped angles -- never the
        // candidate that went in. Measuring the candidate instead is how a
        // near-vertical support normal would produce a view that passed the
        // test and then did not exist.
        pitch = clampToRange(pitch, -kPitchLimitRadians, kPitchLimitRadians);
        const Vec3 installed = cadFeatureViewDirection(yaw, pitch);
        if (!cadFeatureViewUsable(installed, axis)) {
            continue;
        }
        *out = current;
        out->target = anchors.base;
        out->yaw = yaw;
        out->pitch = pitch;
        // The projection and the span are the sketch's own, so the profile is
        // the size it was a moment ago and only the DIRECTION changed. The
        // transition is a tilt, not a reframe.
        return CadFeatureViewSource::ObliqueFallback;
    }
    return CadFeatureViewSource::Unavailable;
}

}  // namespace forgeshape
