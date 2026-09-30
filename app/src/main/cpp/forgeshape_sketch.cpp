#include "forgeshape_sketch.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace forgeshape {

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

// The count is a literal in the header so Java can mirror it; this is what
// keeps the literal honest when an enumerator is appended.
static_assert(static_cast<int>(CadStatus::PlanarFaceRegenerationUnavailable) + 1 == kCadStatusCount,
              "kCadStatusCount must equal the number of CadStatus enumerators");

const char* cadStatusName(CadStatus status) {
    switch (status) {
        case CadStatus::Ok: return "Ok";
        case CadStatus::NonFinite: return "NonFinite";
        case CadStatus::OutOfRange: return "OutOfRange";
        case CadStatus::ZeroLengthLine: return "ZeroLengthLine";
        case CadStatus::DuplicateEdge: return "DuplicateEdge";
        case CadStatus::TooFewVertices: return "TooFewVertices";
        case CadStatus::ZeroSizeRectangle: return "ZeroSizeRectangle";
        case CadStatus::InvalidCircleRadius: return "InvalidCircleRadius";
        case CadStatus::TooManyEntities: return "TooManyEntities";
        case CadStatus::UnknownEntity: return "UnknownEntity";
        case CadStatus::InvalidWorkplane: return "InvalidWorkplane";
        case CadStatus::OpenProfile: return "OpenProfile";
        case CadStatus::SelfIntersectingProfile: return "SelfIntersectingProfile";
        case CadStatus::ZeroAreaProfile: return "ZeroAreaProfile";
        case CadStatus::BranchingChain: return "BranchingChain";
        case CadStatus::NoClosedProfile: return "NoClosedProfile";
        case CadStatus::AmbiguousProfile: return "AmbiguousProfile";
        case CadStatus::ProfileNotFound: return "ProfileNotFound";
        case CadStatus::NestedProfileUnsupported: return "NestedProfileUnsupported";
        case CadStatus::InvalidExtrudeDepth: return "InvalidExtrudeDepth";
        case CadStatus::InvalidExtrudeDirection: return "InvalidExtrudeDirection";
        case CadStatus::TriangulationFailed: return "TriangulationFailed";
        case CadStatus::RegenerationFailed: return "RegenerationFailed";
        case CadStatus::NotSketching: return "NotSketching";
        case CadStatus::NotCadBody: return "NotCadBody";
        case CadStatus::RefusedEditInProgress: return "RefusedEditInProgress";
        case CadStatus::InvalidArc: return "InvalidArc";
        case CadStatus::InvalidSpline: return "InvalidSpline";
        case CadStatus::SketchNotEmpty: return "SketchNotEmpty";
        case CadStatus::DependentFaceLost: return "DependentFaceLost";
        case CadStatus::InvalidExtrudeExtent: return "InvalidExtrudeExtent";
        case CadStatus::ProfileRegionMismatch: return "ProfileRegionMismatch";
        case CadStatus::OverlappingRegions: return "OverlappingRegions";
        case CadStatus::OverlappingHoles: return "OverlappingHoles";
        case CadStatus::TooManyRegions: return "TooManyRegions";
        case CadStatus::InvalidFeatureOperation: return "InvalidFeatureOperation";
        case CadStatus::TooManyFeatures: return "TooManyFeatures";
        case CadStatus::FeatureSupportInvalid: return "FeatureSupportInvalid";
        case CadStatus::SupportFaceLost: return "SupportFaceLost";
        case CadStatus::OperationNeedsTarget: return "OperationNeedsTarget";
        case CadStatus::AddDisjoint: return "AddDisjoint";
        case CadStatus::AddNoEffect: return "AddNoEffect";
        case CadStatus::CutNoIntersection: return "CutNoIntersection";
        case CadStatus::CutRemovesBody: return "CutRemovesBody";
        case CadStatus::KernelFailed: return "KernelFailed";
        case CadStatus::SketchIdInvalid: return "SketchIdInvalid";
        case CadStatus::DuplicateSketchId: return "DuplicateSketchId";
        case CadStatus::SketchNotFound: return "SketchNotFound";
        case CadStatus::HighWaterInvalid: return "HighWaterInvalid";
        case CadStatus::TooManySketches: return "TooManySketches";
        case CadStatus::SketchSupportInvalid: return "SketchSupportInvalid";
        case CadStatus::InvalidSelectionKind: return "InvalidSelectionKind";
        case CadStatus::PlanarFaceRefMalformed: return "PlanarFaceRefMalformed";
        case CadStatus::PlanarFaceRefNotCanonical: return "PlanarFaceRefNotCanonical";
        case CadStatus::DuplicatePlanarFace: return "DuplicatePlanarFace";
        case CadStatus::PlanarFaceUnresolved: return "PlanarFaceUnresolved";
        case CadStatus::PlanarFaceUnsupportedCurve: return "PlanarFaceUnsupportedCurve";
        case CadStatus::PlanarFaceAmbiguousOverlap: return "PlanarFaceAmbiguousOverlap";
        case CadStatus::PlanarFaceCapExceeded: return "PlanarFaceCapExceeded";
        case CadStatus::PlanarFaceDegenerate: return "PlanarFaceDegenerate";
        case CadStatus::PlanarFaceRegenerationUnavailable: return "PlanarFaceRegenerationUnavailable";
    }
    return "unknown";
}

int cadStatusCode(CadStatus status) { return static_cast<int>(status); }

bool cadStatusFromCode(int code, CadStatus* out) {
    if (out == nullptr || code < 0 || code >= kCadStatusCount) {
        return false;
    }
    *out = static_cast<CadStatus>(code);
    return true;
}

const char* sketchEntityKindName(SketchEntityKind kind) {
    switch (kind) {
        case SketchEntityKind::Line: return "Line";
        case SketchEntityKind::Polyline: return "Polyline";
        case SketchEntityKind::Rectangle: return "Rectangle";
        case SketchEntityKind::Circle: return "Circle";
        case SketchEntityKind::Arc: return "Arc";
        case SketchEntityKind::Spline: return "Spline";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// Small geometry
// ---------------------------------------------------------------------------

namespace {

bool sameBits(double a, double b) {
    uint64_t left = 0;
    uint64_t right = 0;
    std::memcpy(&left, &a, sizeof(left));
    std::memcpy(&right, &b, sizeof(right));
    return left == right;
}

bool samePointBits(const SketchPoint& a, const SketchPoint& b) {
    return sameBits(a.u, b.u) && sameBits(a.v, b.v);
}

bool coincident(const SketchPoint& a, const SketchPoint& b) {
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return (du * du + dv * dv) <= kSketchCoincidenceMeters * kSketchCoincidenceMeters;
}

CadStatus validatePoint(const SketchPoint& p) {
    if (!std::isfinite(p.u) || !std::isfinite(p.v)) {
        return CadStatus::NonFinite;
    }
    if (std::fabs(p.u) > kMaxSketchCoordinateMeters || std::fabs(p.v) > kMaxSketchCoordinateMeters) {
        return CadStatus::OutOfRange;
    }
    return CadStatus::Ok;
}

// A length must pass the SAME rule every primitive dimension passes -- finite,
// positive, and resolvable once it becomes a float half-extent -- and must
// stay inside the sketch range. One rule for what a Construction length is.
bool usableLength(Meters value) {
    return validateDimensionMeters(value) == DimensionValidation::Ok
           && value <= kMaxSketchCoordinateMeters;
}

double cross(const SketchPoint& o, const SketchPoint& a, const SketchPoint& b) {
    return (a.u - o.u) * (b.v - o.v) - (a.v - o.v) * (b.u - o.u);
}

bool onSegment(const SketchPoint& p, const SketchPoint& a, const SketchPoint& b) {
    return p.u <= std::fmax(a.u, b.u) && p.u >= std::fmin(a.u, b.u)
           && p.v <= std::fmax(a.v, b.v) && p.v >= std::fmin(a.v, b.v);
}

int orientationSign(const SketchPoint& o, const SketchPoint& a, const SketchPoint& b) {
    const double c = cross(o, a, b);
    if (c > 0.0) return 1;
    if (c < 0.0) return -1;
    return 0;
}

// Point strictly inside a polygon, by the even-odd rule. A point ON an edge
// is reported as not strictly inside.
bool pointStrictlyInside(const SketchPoint& p, const std::vector<SketchPoint>& polygon) {
    const size_t n = polygon.size();
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[j];
        if (orientationSign(a, b, p) == 0 && onSegment(p, a, b)) {
            return false;  // on the boundary
        }
        const bool crosses = ((a.v > p.v) != (b.v > p.v))
                             && (p.u < (b.u - a.u) * (p.v - a.v) / (b.v - a.v) + a.u);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

}  // namespace

bool sketchSegmentsIntersect(const SketchPoint& a0, const SketchPoint& a1,
                             const SketchPoint& b0, const SketchPoint& b1) {
    const int o1 = orientationSign(a0, a1, b0);
    const int o2 = orientationSign(a0, a1, b1);
    const int o3 = orientationSign(b0, b1, a0);
    const int o4 = orientationSign(b0, b1, a1);
    if (o1 != o2 && o3 != o4) {
        return true;
    }
    if (o1 == 0 && onSegment(b0, a0, a1)) return true;
    if (o2 == 0 && onSegment(b1, a0, a1)) return true;
    if (o3 == 0 && onSegment(a0, b0, b1)) return true;
    if (o4 == 0 && onSegment(a1, b0, b1)) return true;
    return false;
}

double polygonSignedAreaTwice(const std::vector<SketchPoint>& polygon) {
    double twice = 0.0;
    const size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[(i + 1) % n];
        twice += a.u * b.v - b.u * a.v;
    }
    return twice;
}

// ---------------------------------------------------------------------------
// Entities
// ---------------------------------------------------------------------------

bool sameSketchEntity(const SketchEntity& a, const SketchEntity& b) {
    if (a.id() != b.id() || a.kind() != b.kind()) {
        return false;
    }
    switch (a.kind()) {
        case SketchEntityKind::Line:
            return samePointBits(a.line()->start, b.line()->start)
                   && samePointBits(a.line()->end, b.line()->end);
        case SketchEntityKind::Polyline: {
            const SketchPolyline& l = *a.polyline();
            const SketchPolyline& r = *b.polyline();
            if (l.closed != r.closed || l.vertices.size() != r.vertices.size()) {
                return false;
            }
            for (size_t i = 0; i < l.vertices.size(); ++i) {
                if (!samePointBits(l.vertices[i], r.vertices[i])) {
                    return false;
                }
            }
            return true;
        }
        case SketchEntityKind::Rectangle:
            return samePointBits(a.rectangle()->center, b.rectangle()->center)
                   && sameBits(a.rectangle()->width, b.rectangle()->width)
                   && sameBits(a.rectangle()->height, b.rectangle()->height);
        case SketchEntityKind::Circle:
            return samePointBits(a.circle()->center, b.circle()->center)
                   && sameBits(a.circle()->radius, b.circle()->radius);
        case SketchEntityKind::Arc:
            return samePointBits(a.arc()->start, b.arc()->start)
                   && samePointBits(a.arc()->mid, b.arc()->mid)
                   && samePointBits(a.arc()->end, b.arc()->end);
        case SketchEntityKind::Spline: {
            const SketchSpline& l = *a.spline();
            const SketchSpline& r = *b.spline();
            if (l.points.size() != r.points.size()) {
                return false;
            }
            for (size_t i = 0; i < l.points.size(); ++i) {
                if (!samePointBits(l.points[i], r.points[i])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

CadStatus validateSketchEntity(const SketchEntity& entity) {
    switch (entity.kind()) {
        case SketchEntityKind::Line: {
            const SketchLine& line = *entity.line();
            CadStatus why = validatePoint(line.start);
            if (why != CadStatus::Ok) return why;
            why = validatePoint(line.end);
            if (why != CadStatus::Ok) return why;
            if (coincident(line.start, line.end)) {
                return CadStatus::ZeroLengthLine;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Polyline: {
            const SketchPolyline& polyline = *entity.polyline();
            if (polyline.vertices.size() > kMaxPolylineVertices) {
                return CadStatus::TooManyEntities;
            }
            if (polyline.vertices.size() < 2 || (polyline.closed && polyline.vertices.size() < 3)) {
                return CadStatus::TooFewVertices;
            }
            for (const SketchPoint& p : polyline.vertices) {
                const CadStatus why = validatePoint(p);
                if (why != CadStatus::Ok) return why;
            }
            for (size_t i = 1; i < polyline.vertices.size(); ++i) {
                if (coincident(polyline.vertices[i - 1], polyline.vertices[i])) {
                    return CadStatus::DuplicateEdge;
                }
            }
            if (polyline.closed
                && coincident(polyline.vertices.back(), polyline.vertices.front())) {
                // The closing edge is the one between the last vertex and the
                // first; a closed polyline that repeats its first vertex has a
                // zero-length closing edge.
                return CadStatus::DuplicateEdge;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Rectangle: {
            const SketchRectangle& rectangle = *entity.rectangle();
            const CadStatus why = validatePoint(rectangle.center);
            if (why != CadStatus::Ok) return why;
            if (!std::isfinite(rectangle.width) || !std::isfinite(rectangle.height)) {
                return CadStatus::NonFinite;
            }
            if (!usableLength(rectangle.width) || !usableLength(rectangle.height)) {
                return CadStatus::ZeroSizeRectangle;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Circle: {
            const SketchCircle& circle = *entity.circle();
            const CadStatus why = validatePoint(circle.center);
            if (why != CadStatus::Ok) return why;
            if (!std::isfinite(circle.radius)) {
                return CadStatus::NonFinite;
            }
            if (!usableLength(circle.radius)) {
                return CadStatus::InvalidCircleRadius;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Arc: {
            const SketchArc& arc = *entity.arc();
            for (const SketchPoint& p : {arc.start, arc.mid, arc.end}) {
                const CadStatus why = validatePoint(p);
                if (why != CadStatus::Ok) return why;
            }
            // The three points must be three DISTINCT points and must not be
            // collinear: either would leave no finite circle through them, and
            // an arc with no circle is not an arc. Refused by name rather than
            // quietly turned into the straight line it nearly is.
            if (coincident(arc.start, arc.mid) || coincident(arc.mid, arc.end)
                || coincident(arc.start, arc.end)) {
                return CadStatus::InvalidArc;
            }
            SketchPoint center;
            double radius = 0.0;
            double startAngle = 0.0;
            double sweep = 0.0;
            const CadStatus why = arcGeometry(arc, &center, &radius, &startAngle, &sweep);
            if (why != CadStatus::Ok) {
                return why;
            }
            // The derived circle must still be a usable Construction length and
            // must stay inside the sketch range, exactly as a circle's is.
            if (!usableLength(radius)) {
                return CadStatus::InvalidArc;
            }
            return CadStatus::Ok;
        }
        case SketchEntityKind::Spline: {
            const SketchSpline& spline = *entity.spline();
            if (spline.points.size() > kMaxSplinePoints) {
                return CadStatus::TooManyEntities;
            }
            if (spline.points.size() < 2) {
                return CadStatus::InvalidSpline;
            }
            for (const SketchPoint& p : spline.points) {
                const CadStatus why = validatePoint(p);
                if (why != CadStatus::Ok) return why;
            }
            for (size_t i = 1; i < spline.points.size(); ++i) {
                if (coincident(spline.points[i - 1], spline.points[i])) {
                    return CadStatus::InvalidSpline;
                }
            }
            // A chainable entity's two ends must be two places: a spline whose
            // ends meet is a loop this stage's one chain walker cannot read,
            // and it is refused rather than half-supported.
            if (coincident(spline.points.front(), spline.points.back())) {
                return CadStatus::InvalidSpline;
            }
            return CadStatus::Ok;
        }
    }
    return CadStatus::UnknownEntity;
}

// ---------------------------------------------------------------------------
// Curves: the derived polylines (`SKETCH-UX-R1`)
// ---------------------------------------------------------------------------

bool sketchEntityEndpoints(const SketchEntity& entity, SketchPoint* outStart,
                           SketchPoint* outEnd) {
    if (outStart == nullptr || outEnd == nullptr) {
        return false;
    }
    if (const SketchLine* line = entity.line()) {
        *outStart = line->start;
        *outEnd = line->end;
        return true;
    }
    if (const SketchArc* arc = entity.arc()) {
        *outStart = arc->start;
        *outEnd = arc->end;
        return true;
    }
    if (const SketchSpline* spline = entity.spline()) {
        if (spline->points.size() < 2) {
            return false;
        }
        *outStart = spline->points.front();
        *outEnd = spline->points.back();
        return true;
    }
    return false;
}

bool sketchEntityIsCurved(const SketchEntity& entity) {
    return entity.kind() == SketchEntityKind::Arc || entity.kind() == SketchEntityKind::Spline;
}

CadStatus arcGeometry(const SketchArc& arc, SketchPoint* outCenter, double* outRadius,
                      double* outStartAngle, double* outSweep) {
    // The circumcentre of the three points, from the perpendicular bisectors.
    // `d` is twice the signed area of the triangle: zero exactly when the three
    // points are collinear, which is the case with no finite circle.
    const double ax = arc.start.u;
    const double ay = arc.start.v;
    const double bx = arc.mid.u;
    const double by = arc.mid.v;
    const double cx = arc.end.u;
    const double cy = arc.end.v;
    const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (!std::isfinite(d)) {
        return CadStatus::NonFinite;
    }
    // The collinearity test is SCALED by the triangle's size rather than an
    // absolute epsilon: three points a kilometre apart and three a millimetre
    // apart are equally collinear when their area is negligible against their
    // own extent.
    const double scale = std::fmax(std::fabs(ax) + std::fabs(ay),
                                   std::fmax(std::fabs(bx) + std::fabs(by),
                                             std::fabs(cx) + std::fabs(cy)));
    const double floorScale = scale > 1.0 ? scale : 1.0;
    if (std::fabs(d) <= kSketchCoincidenceMeters * floorScale) {
        return CadStatus::InvalidArc;
    }
    const double a2 = ax * ax + ay * ay;
    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;
    const double ux = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
    const double uy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
    if (!std::isfinite(ux) || !std::isfinite(uy)) {
        return CadStatus::NonFinite;
    }
    const double radius = std::sqrt((ax - ux) * (ax - ux) + (ay - uy) * (ay - uy));
    if (!std::isfinite(radius) || radius <= 0.0) {
        return CadStatus::InvalidArc;
    }
    const double startAngle = std::atan2(ay - uy, ax - ux);
    const double midAngle = std::atan2(by - uy, bx - ux);
    const double endAngle = std::atan2(cy - uy, cx - ux);

    // Which of the circle's two arcs the user drew is decided by where `mid`
    // lies, not by a stored flag: sweep counter-clockwise if the middle point
    // is reached before the end going that way, clockwise otherwise. Both
    // deltas are normalized into (0, 2*pi) so the comparison is total.
    const double twoPi = 2.0 * 3.14159265358979323846;
    const auto ccwDelta = [twoPi](double from, double to) {
        double delta = to - from;
        while (delta <= 0.0) delta += twoPi;
        while (delta > twoPi) delta -= twoPi;
        return delta;
    };
    const double midCcw = ccwDelta(startAngle, midAngle);
    const double endCcw = ccwDelta(startAngle, endAngle);
    const double sweep = (midCcw < endCcw) ? endCcw : endCcw - twoPi;
    if (!std::isfinite(sweep) || std::fabs(sweep) <= 0.0) {
        return CadStatus::InvalidArc;
    }
    if (outCenter != nullptr) *outCenter = SketchPoint{ux, uy};
    if (outRadius != nullptr) *outRadius = radius;
    if (outStartAngle != nullptr) *outStartAngle = startAngle;
    if (outSweep != nullptr) *outSweep = sweep;
    return CadStatus::Ok;
}

namespace {

// How many straight segments one arc becomes: the same angular density a full
// circle gets, clamped. A pure function of the swept angle.
uint32_t arcSegmentCount(double sweep) {
    const double twoPi = 2.0 * 3.14159265358979323846;
    const double fraction = std::fabs(sweep) / twoPi;
    double wanted = std::ceil(fraction * static_cast<double>(kSketchCircleSegments));
    if (!std::isfinite(wanted) || wanted < 1.0) {
        wanted = 1.0;
    }
    uint32_t segments = static_cast<uint32_t>(wanted);
    if (segments < kMinArcSegments) segments = kMinArcSegments;
    if (segments > kMaxArcSegments) segments = kMaxArcSegments;
    return segments;
}

// One cubic Bezier span, evaluated at t.
SketchPoint bezierAt(const SketchPoint& p0, const SketchPoint& p1, const SketchPoint& p2,
                     const SketchPoint& p3, double t) {
    const double s = 1.0 - t;
    const double w0 = s * s * s;
    const double w1 = 3.0 * s * s * t;
    const double w2 = 3.0 * s * t * t;
    const double w3 = t * t * t;
    return SketchPoint{p0.u * w0 + p1.u * w1 + p2.u * w2 + p3.u * w3,
                       p0.v * w0 + p1.v * w1 + p2.v * w2 + p3.v * w3};
}

}  // namespace

CadStatus tessellateSketchCurve(const SketchEntity& entity, std::vector<SketchPoint>* out) {
    if (out == nullptr) {
        return CadStatus::UnknownEntity;
    }
    const CadStatus valid = validateSketchEntity(entity);
    if (valid != CadStatus::Ok) {
        return valid;
    }
    out->clear();
    if (const SketchLine* line = entity.line()) {
        out->push_back(line->start);
        out->push_back(line->end);
        return CadStatus::Ok;
    }
    if (const SketchArc* arc = entity.arc()) {
        SketchPoint center;
        double radius = 0.0;
        double startAngle = 0.0;
        double sweep = 0.0;
        const CadStatus why = arcGeometry(*arc, &center, &radius, &startAngle, &sweep);
        if (why != CadStatus::Ok) {
            return why;
        }
        const uint32_t segments = arcSegmentCount(sweep);
        out->reserve(segments + 1u);
        // The two ENDS are the authored points, written exactly rather than
        // recomputed from the angle: a chain must close on the numbers the snap
        // produced, and cos/sin of a derived angle would land an ulp away.
        out->push_back(arc->start);
        for (uint32_t i = 1; i < segments; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(segments);
            const double angle = startAngle + sweep * t;
            out->push_back(SketchPoint{center.u + radius * std::cos(angle),
                                       center.v + radius * std::sin(angle)});
        }
        out->push_back(arc->end);
        return CadStatus::Ok;
    }
    if (const SketchSpline* spline = entity.spline()) {
        const std::vector<SketchPoint>& p = spline->points;
        const size_t n = p.size();
        out->reserve((n - 1) * kSplineSegmentsPerSpan + 1u);
        out->push_back(p.front());
        for (size_t i = 0; i + 1 < n; ++i) {
            // Catmull-Rom tangents, with the end spans reflecting their one
            // neighbour so the curve still passes through the endpoint with a
            // defined direction. Converted to the equivalent cubic Bezier: the
            // curve INTERPOLATES p[i] and p[i+1] exactly.
            const SketchPoint& p1 = p[i];
            const SketchPoint& p2 = p[i + 1];
            const SketchPoint p0 = (i == 0) ? SketchPoint{2.0 * p1.u - p2.u, 2.0 * p1.v - p2.v}
                                            : p[i - 1];
            const SketchPoint p3 = (i + 2 < n) ? p[i + 2]
                                               : SketchPoint{2.0 * p2.u - p1.u, 2.0 * p2.v - p1.v};
            const SketchPoint c1{p1.u + (p2.u - p0.u) / 6.0, p1.v + (p2.v - p0.v) / 6.0};
            const SketchPoint c2{p2.u - (p3.u - p1.u) / 6.0, p2.v - (p3.v - p1.v) / 6.0};
            for (uint32_t s = 1; s <= kSplineSegmentsPerSpan; ++s) {
                if (s == kSplineSegmentsPerSpan) {
                    // The span's last point is the authored point itself, for
                    // the same reason an arc's ends are: exact, not evaluated.
                    out->push_back(p2);
                    break;
                }
                const double t = static_cast<double>(s) / static_cast<double>(kSplineSegmentsPerSpan);
                out->push_back(bezierAt(p1, c1, c2, p2, t));
            }
        }
        return CadStatus::Ok;
    }
    return CadStatus::UnknownEntity;
}

// ---------------------------------------------------------------------------
// Semantic topology (`CAD-A3`)
// ---------------------------------------------------------------------------

const char* cadFaceKindName(CadFaceKind kind) {
    switch (kind) {
        case CadFaceKind::CapPlane: return "CapPlane";
        case CadFaceKind::CapFar: return "CapFar";
        case CadFaceKind::Side: return "Side";
    }
    return "unknown";
}

uint64_t cadFaceTokenCode(const CadFaceToken& token) {
    // kind in the high byte, entity id in the middle, local index in the low
    // bits: a stable, order-independent code that two equal tokens share and
    // two different ones do not, for the lineage signature and for comparison.
    return (static_cast<uint64_t>(token.kind) << 56)
           | (static_cast<uint64_t>(token.edgeEntityId) << 16)
           | static_cast<uint64_t>(token.edgeLocalIndex & 0xFFFFu);
}

bool sameCadFaceToken(const CadFaceToken& a, const CadFaceToken& b) {
    return cadFaceTokenCode(a) == cadFaceTokenCode(b);
}

bool sameTopoRef(const TopoRef& a, const TopoRef& b) {
    return a.producerObjectId == b.producerObjectId
           && a.producerLocalFeatureId == b.producerLocalFeatureId
           && sameCadFaceToken(a.face, b.face) && a.lineageToken == b.lineageToken;
}

// ---------------------------------------------------------------------------
// The sketch
// ---------------------------------------------------------------------------

bool sameCadSketch(const CadSketch& a, const CadSketch& b) {
    if (a.plane != b.plane || a.nextEntityId != b.nextEntityId
        || a.entities.size() != b.entities.size()
        || a.hasFaceSupport != b.hasFaceSupport) {
        return false;
    }
    if (a.hasFaceSupport && !sameTopoRef(a.faceSupport, b.faceSupport)) {
        return false;
    }
    for (size_t i = 0; i < a.entities.size(); ++i) {
        if (!sameSketchEntity(a.entities[i], b.entities[i])) {
            return false;
        }
    }
    return true;
}

CadStatus validateCadSketch(const CadSketch& sketch) {
    if (workplaneIndex(sketch.plane) < 0 || workplaneIndex(sketch.plane) >= kWorkplaneCount) {
        return CadStatus::InvalidWorkplane;
    }
    // A face-supported sketch authors on the canonical XY in its own local
    // space; the support frame does the placing. Anything else would be two
    // answers to what the sketch's basis is. The TopoRef's own resolution
    // against a producer is checked where a scene exists, not here -- and
    // since `CAD-VERTICAL-SLICE-R1` that includes whether the named feature
    // exists, because a producer may now carry more than its first feature.
    if (sketch.hasFaceSupport) {
        if (sketch.plane != Workplane::XY
            || sketch.faceSupport.producerObjectId == kNoObject
            || sketch.faceSupport.producerLocalFeatureId == 0u) {
            return CadStatus::InvalidWorkplane;
        }
    }
    if (sketch.entities.size() > kMaxSketchEntities) {
        return CadStatus::TooManyEntities;
    }
    if (sketch.nextEntityId == kNoSketchEntity) {
        return CadStatus::UnknownEntity;
    }
    for (size_t i = 0; i < sketch.entities.size(); ++i) {
        const SketchEntity& entity = sketch.entities[i];
        if (entity.id() == kNoSketchEntity || entity.id() >= sketch.nextEntityId) {
            return CadStatus::UnknownEntity;
        }
        for (size_t j = 0; j < i; ++j) {
            if (sketch.entities[j].id() == entity.id()) {
                return CadStatus::UnknownEntity;
            }
        }
        const CadStatus why = validateSketchEntity(entity);
        if (why != CadStatus::Ok) {
            return why;
        }
    }
    return CadStatus::Ok;
}

CadStatus addSketchEntity(CadSketch* sketch, SketchEntity::Payload payload,
                          SketchEntityId* outId) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    if (sketch->entities.size() >= kMaxSketchEntities) {
        return CadStatus::TooManyEntities;
    }
    // Validated with a provisional id BEFORE anything is minted, so a refusal
    // costs no id and leaves the allocator exactly where it was.
    const SketchEntity candidate(sketch->nextEntityId, std::move(payload));
    const CadStatus why = validateSketchEntity(candidate);
    if (why != CadStatus::Ok) {
        return why;
    }
    if (sketch->nextEntityId == UINT32_MAX) {
        return CadStatus::TooManyEntities;
    }
    sketch->entities.push_back(candidate);
    ++sketch->nextEntityId;
    if (outId != nullptr) {
        *outId = candidate.id();
    }
    return CadStatus::Ok;
}

CadStatus replaceSketchEntity(CadSketch* sketch, SketchEntityId id, SketchEntity::Payload payload) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    for (SketchEntity& entity : sketch->entities) {
        if (entity.id() != id) {
            continue;
        }
        const SketchEntity candidate(id, std::move(payload));
        const CadStatus why = validateSketchEntity(candidate);
        if (why != CadStatus::Ok) {
            return why;
        }
        entity = candidate;
        return CadStatus::Ok;
    }
    return CadStatus::UnknownEntity;
}

CadStatus removeSketchEntity(CadSketch* sketch, SketchEntityId id) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    for (size_t i = 0; i < sketch->entities.size(); ++i) {
        if (sketch->entities[i].id() == id) {
            sketch->entities.erase(sketch->entities.begin() + static_cast<std::ptrdiff_t>(i));
            return CadStatus::Ok;
        }
    }
    return CadStatus::UnknownEntity;
}

const SketchEntity* findSketchEntity(const CadSketch& sketch, SketchEntityId id) {
    for (const SketchEntity& entity : sketch.entities) {
        if (entity.id() == id) {
            return &entity;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Profile polygons
// ---------------------------------------------------------------------------

std::vector<SketchPoint> rectangleProfilePolygon(const SketchRectangle& rectangle) {
    const double hw = rectangle.width * 0.5;
    const double hh = rectangle.height * 0.5;
    const double cu = rectangle.center.u;
    const double cv = rectangle.center.v;
    return {SketchPoint{cu - hw, cv - hh}, SketchPoint{cu + hw, cv - hh},
            SketchPoint{cu + hw, cv + hh}, SketchPoint{cu - hw, cv + hh}};
}

std::vector<SketchPoint> circleProfilePolygon(const SketchCircle& circle) {
    std::vector<SketchPoint> polygon;
    polygon.reserve(kSketchCircleSegments);
    const uint32_t quarter = kSketchCircleSegments / 4u;
    for (uint32_t i = 0; i < kSketchCircleSegments; ++i) {
        double c = 0.0;
        double s = 0.0;
        // The four cardinal points are exact 0 / +/-1 rather than trigonometry,
        // for the same reason the round primitives do it: the extents are then
        // exactly the radius.
        if (i % quarter == 0) {
            switch (i / quarter) {
                case 0: c = 1.0; s = 0.0; break;
                case 1: c = 0.0; s = 1.0; break;
                case 2: c = -1.0; s = 0.0; break;
                default: c = 0.0; s = -1.0; break;
            }
        } else {
            const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i)
                                 / static_cast<double>(kSketchCircleSegments);
            c = std::cos(angle);
            s = std::sin(angle);
        }
        polygon.push_back(SketchPoint{circle.center.u + circle.radius * c,
                                      circle.center.v + circle.radius * s});
    }
    return polygon;
}

namespace {

// The rules every candidate loop is held to. Normalizes the orientation to
// counter-clockwise on success; refuses, writing nothing, otherwise.
//
// `edgeEntityId`/`edgeLocalIndex`, when given, are the per-edge identity
// parallel to `polygon` (edge k connects polygon[k] -> polygon[k+1]). When the
// polygon is reversed to make it counter-clockwise they are transformed to
// stay aligned: the geometric edge is unchanged, so a side face's semantic
// identity does not flip with a winding correction.
CadStatus validateLoop(std::vector<SketchPoint>* polygon, double* outArea,
                       std::vector<SketchEntityId>* edgeEntityId = nullptr,
                       std::vector<uint32_t>* edgeLocalIndex = nullptr,
                       std::vector<uint8_t>* edgeCurved = nullptr) {
    const size_t n = polygon->size();
    if (n < 3) {
        return CadStatus::TooFewVertices;
    }
    if (n > kMaxProfileVertices) {
        return CadStatus::TooManyEntities;
    }
    for (size_t i = 0; i < n; ++i) {
        if (coincident((*polygon)[i], (*polygon)[(i + 1) % n])) {
            return CadStatus::DuplicateEdge;
        }
    }
    // Crossings BEFORE area: a bow tie's two lobes cancel to zero area, and
    // "it crosses itself" is the reason the user can act on. Non-adjacent
    // edges may not cross or touch; adjacent edges share a vertex by
    // construction and are skipped, so for a triangle every pair is adjacent.
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const bool adjacent = (j == i + 1) || (i == 0 && j == n - 1);
            if (adjacent) {
                continue;
            }
            if (sketchSegmentsIntersect((*polygon)[i], (*polygon)[(i + 1) % n], (*polygon)[j],
                                        (*polygon)[(j + 1) % n])) {
                return CadStatus::SelfIntersectingProfile;
            }
        }
    }
    const double twice = polygonSignedAreaTwice(*polygon);
    if (!std::isfinite(twice) || std::fabs(twice) * 0.5 < kMinProfileAreaSquareMeters) {
        return CadStatus::ZeroAreaProfile;
    }
    if (twice < 0.0) {
        std::reverse(polygon->begin(), polygon->end());
        // Reversing [p0..p_{n-1}] to [p_{n-1}..p0] makes new edge j the same
        // geometric edge as old edge (n-2-j) mod n. Rebuild the identity arrays
        // by that map so a side face keeps its token whichever way the profile
        // was drawn.
        const auto rebuild = [n](std::vector<SketchEntityId>* ids) {
            if (ids == nullptr || ids->size() != n) return;
            std::vector<SketchEntityId> out(n);
            for (size_t j = 0; j < n; ++j) {
                out[j] = (*ids)[(n - 2 - j + n) % n];
            }
            *ids = std::move(out);
        };
        rebuild(edgeEntityId);
        if (edgeLocalIndex != nullptr && edgeLocalIndex->size() == n) {
            std::vector<uint32_t> out(n);
            for (size_t j = 0; j < n; ++j) {
                out[j] = (*edgeLocalIndex)[(n - 2 - j + n) % n];
            }
            *edgeLocalIndex = std::move(out);
        }
        if (edgeCurved != nullptr && edgeCurved->size() == n) {
            std::vector<uint8_t> out(n);
            for (size_t j = 0; j < n; ++j) {
                out[j] = (*edgeCurved)[(n - 2 - j + n) % n];
            }
            *edgeCurved = std::move(out);
        }
    }
    *outArea = std::fabs(twice) * 0.5;
    return CadStatus::Ok;
}

struct LoopCandidate {
    SketchEntityId anchor = kNoSketchEntity;
    std::vector<SketchPoint> polygon;
    bool fromCircle = false;
    std::vector<SketchEntityId> members;
    // Parallel to `polygon`: which sketch entity and which of its edges owns
    // polygon edge k. See ClosedProfile.
    std::vector<SketchEntityId> edgeEntityId;
    std::vector<uint32_t> edgeLocalIndex;
    std::vector<uint8_t> edgeCurved;
};

// Reads every loop a set of CHAINABLE entities closes: Lines, Arcs and
// Splines, in one walker (`SKETCH-UX-R1` D3).
//
// Endpoints within the coincidence tolerance are one NODE. A component in
// which every node meets exactly two entity ends is one loop; a free end is an
// open profile and a node with three or more ends is a fork. Each component
// is reported once, under its smallest entity id.
//
// The chain is read from AUTHORED endpoints, never from a tessellation: a
// curve joins a line on the numbers the snap produced. Only when the loop's
// polygon is built does each member contribute its derived points, so the
// connectivity decision and the triangulation input are two separate things
// and a denser tessellation can never open or close a profile.
void chainCurves(const CadSketch& sketch, std::vector<LoopCandidate>* loops,
                 std::vector<ProfileRejection>* rejections) {
    struct LineRef {
        SketchEntityId id;
        SketchPoint p[2];
        int node[2];
        // The member's own derived polyline, start -> end inclusive, and
        // whether those segments approximate a curve.
        std::vector<SketchPoint> points;
        bool curved = false;
    };
    std::vector<LineRef> lines;
    for (const SketchEntity& entity : sketch.entities) {
        SketchPoint start;
        SketchPoint end;
        if (!sketchEntityEndpoints(entity, &start, &end)) {
            continue;
        }
        LineRef ref;
        ref.id = entity.id();
        ref.p[0] = start;
        ref.p[1] = end;
        ref.node[0] = -1;
        ref.node[1] = -1;
        ref.curved = sketchEntityIsCurved(entity);
        if (tessellateSketchCurve(entity, &ref.points) != CadStatus::Ok
            || ref.points.size() < 2) {
            // A member whose own geometry cannot be derived cannot take part.
            // The sketch was validated before extraction, so this is the
            // defensive branch rather than the expected one.
            rejections->push_back(ProfileRejection{entity.id(), CadStatus::UnknownEntity});
            continue;
        }
        lines.push_back(std::move(ref));
    }
    if (lines.empty()) {
        return;
    }
    std::sort(lines.begin(), lines.end(),
              [](const LineRef& a, const LineRef& b) { return a.id < b.id; });

    // Nodes: the first endpoint seen at a location is the representative.
    std::vector<SketchPoint> nodes;
    std::vector<int> degree;
    for (LineRef& line : lines) {
        for (int e = 0; e < 2; ++e) {
            int found = -1;
            for (size_t k = 0; k < nodes.size(); ++k) {
                if (coincident(nodes[k], line.p[e])) {
                    found = static_cast<int>(k);
                    break;
                }
            }
            if (found < 0) {
                found = static_cast<int>(nodes.size());
                nodes.push_back(line.p[e]);
                degree.push_back(0);
            }
            line.node[e] = found;
            ++degree[static_cast<size_t>(found)];
        }
    }

    // Components over lines, by shared nodes. Union-find over line indices.
    std::vector<int> parent(lines.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);
    const auto find = [&parent](int x) {
        while (parent[static_cast<size_t>(x)] != x) {
            parent[static_cast<size_t>(x)] = parent[static_cast<size_t>(parent[static_cast<size_t>(x)])];
            x = parent[static_cast<size_t>(x)];
        }
        return x;
    };
    std::vector<int> nodeOwner(nodes.size(), -1);
    for (size_t i = 0; i < lines.size(); ++i) {
        for (int e = 0; e < 2; ++e) {
            const int node = lines[i].node[e];
            int& owner = nodeOwner[static_cast<size_t>(node)];
            if (owner < 0) {
                owner = static_cast<int>(i);
            } else {
                parent[static_cast<size_t>(find(owner))] = find(static_cast<int>(i));
            }
        }
    }

    // Visit each component once, in order of its smallest line id, which is
    // the order `lines` is already in.
    std::vector<bool> reported(lines.size(), false);
    for (size_t start = 0; start < lines.size(); ++start) {
        if (reported[start]) {
            continue;
        }
        const int root = find(static_cast<int>(start));
        std::vector<size_t> member;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (find(static_cast<int>(i)) == root) {
                member.push_back(i);
                reported[i] = true;
            }
        }
        const SketchEntityId anchor = lines[start].id;

        bool fork = false;
        bool open = false;
        for (size_t i : member) {
            for (int e = 0; e < 2; ++e) {
                const int d = degree[static_cast<size_t>(lines[i].node[e])];
                if (d > 2) fork = true;
                if (d < 2) open = true;
            }
        }
        if (fork) {
            rejections->push_back(ProfileRejection{anchor, CadStatus::BranchingChain});
            continue;
        }
        if (open) {
            rejections->push_back(ProfileRejection{anchor, CadStatus::OpenProfile});
            continue;
        }

        // Every node has degree exactly two, so walking from the first line
        // returns to it and visits every line in the component exactly once.
        LoopCandidate loop;
        loop.anchor = anchor;
        std::vector<bool> used(lines.size(), false);
        size_t current = start;
        int enterNode = lines[start].node[0];
        int leaveNode = lines[start].node[1];
        bool tooManyVertices = false;
        for (size_t steps = 0; steps < member.size(); ++steps) {
            used[current] = true;
            loop.members.push_back(lines[current].id);
            const LineRef& ref = lines[current];
            // Which way this member is being traversed: forward when the walk
            // enters at its start.
            const bool forward = (ref.node[0] == enterNode);
            const size_t pointCount = ref.points.size();
            const uint32_t edgeCount = static_cast<uint32_t>(pointCount - 1);
            if (loop.polygon.size() + edgeCount > kMaxProfileVertices) {
                tooManyVertices = true;
                break;
            }
            // The member contributes its own derived points from the node it
            // was entered at, EXCLUDING the far end -- the next member (or the
            // closing wrap) contributes that. The first point is the NODE's
            // representative rather than the member's own copy, so two members
            // that met within tolerance produce exactly one polygon vertex.
            for (uint32_t j = 0; j < edgeCount; ++j) {
                const size_t sourceIndex = forward ? j : (pointCount - 1 - j);
                loop.polygon.push_back(j == 0 ? nodes[static_cast<size_t>(enterNode)]
                                              : ref.points[sourceIndex]);
                loop.edgeEntityId.push_back(ref.id);
                // The local index is the edge's place in the member's OWN
                // canonical start->end order, so a face token does not depend on
                // which way round the chain happened to be walked.
                loop.edgeLocalIndex.push_back(forward ? j : (edgeCount - 1u - j));
                loop.edgeCurved.push_back(ref.curved ? 1u : 0u);
            }
            // The other line at the leaving node.
            size_t next = lines.size();
            for (size_t i : member) {
                if (used[i]) continue;
                if (lines[i].node[0] == leaveNode || lines[i].node[1] == leaveNode) {
                    next = i;
                    break;
                }
            }
            if (next == lines.size()) {
                break;
            }
            enterNode = leaveNode;
            leaveNode = (lines[next].node[0] == leaveNode) ? lines[next].node[1]
                                                            : lines[next].node[0];
            current = next;
        }
        if (tooManyVertices) {
            rejections->push_back(ProfileRejection{anchor, CadStatus::TooManyEntities});
            continue;
        }
        if (loop.members.size() != member.size()) {
            // Cannot happen when every degree is two and the component is
            // connected; refused rather than trusted if it ever does.
            rejections->push_back(ProfileRejection{anchor, CadStatus::OpenProfile});
            continue;
        }
        std::sort(loop.members.begin(), loop.members.end());
        loops->push_back(std::move(loop));
    }
}

}  // namespace

ProfileExtraction extractClosedProfiles(const CadSketch& sketch) {
    ProfileExtraction out;
    std::vector<LoopCandidate> candidates;

    for (const SketchEntity& entity : sketch.entities) {
        LoopCandidate loop;
        loop.anchor = entity.id();
        loop.members = {entity.id()};
        if (const SketchRectangle* rectangle = entity.rectangle()) {
            loop.polygon = rectangleProfilePolygon(*rectangle);
        } else if (const SketchCircle* circle = entity.circle()) {
            loop.polygon = circleProfilePolygon(*circle);
            loop.fromCircle = true;
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            loop.polygon = polyline->vertices;
            bool closed = polyline->closed;
            if (!closed && loop.polygon.size() >= 4
                && coincident(loop.polygon.front(), loop.polygon.back())) {
                // Closed by endpoint snap: the repeated last vertex is the
                // closing edge's end, not a fifth corner.
                loop.polygon.pop_back();
                closed = true;
            }
            if (!closed) {
                out.rejections.push_back(ProfileRejection{entity.id(), CadStatus::OpenProfile});
                continue;
            }
        } else {
            continue;  // lines are chained below
        }
        // A rectangle, circle or polyline owns every edge of its own polygon:
        // edge k is (this entity, k). The line chain fills its own above.
        for (uint32_t k = 0; k < static_cast<uint32_t>(loop.polygon.size()); ++k) {
            loop.edgeEntityId.push_back(entity.id());
            loop.edgeLocalIndex.push_back(k);
            // A rectangle, a circle and a polyline own STRAIGHT polygon edges. The
            // circle is the exception the face module already knows about by
            // `fromCircle`, so nothing here marks its tessellation curved twice.
            loop.edgeCurved.push_back(0u);
        }
        candidates.push_back(std::move(loop));
    }
    chainCurves(sketch, &candidates, &out.rejections);

    for (LoopCandidate& candidate : candidates) {
        double area = 0.0;
        const CadStatus why = validateLoop(&candidate.polygon, &area, &candidate.edgeEntityId,
                                           &candidate.edgeLocalIndex, &candidate.edgeCurved);
        if (why != CadStatus::Ok) {
            out.rejections.push_back(ProfileRejection{candidate.anchor, why});
            continue;
        }
        ClosedProfile profile;
        profile.anchorEntityId = candidate.anchor;
        profile.polygon = std::move(candidate.polygon);
        profile.area = area;
        profile.fromCircle = candidate.fromCircle;
        profile.memberEntityIds = std::move(candidate.members);
        profile.edgeEntityId = std::move(candidate.edgeEntityId);
        profile.edgeLocalIndex = std::move(candidate.edgeLocalIndex);
        profile.edgeCurved = std::move(candidate.edgeCurved);
        out.profiles.push_back(std::move(profile));
    }

    std::sort(out.profiles.begin(), out.profiles.end(),
              [](const ClosedProfile& l, const ClosedProfile& r) {
                  return l.anchorEntityId < r.anchorEntityId;
              });
    std::sort(out.rejections.begin(), out.rejections.end(),
              [](const ProfileRejection& l, const ProfileRejection& r) {
                  return l.anchorEntityId < r.anchorEntityId;
              });
    return out;
}

const ClosedProfile* findClosedProfile(const ProfileExtraction& extraction,
                                       SketchEntityId anchorEntityId) {
    for (const ClosedProfile& profile : extraction.profiles) {
        if (profile.anchorEntityId == anchorEntityId) {
            return &profile;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Triangulation
// ---------------------------------------------------------------------------

CadStatus triangulateSimplePolygon(const std::vector<SketchPoint>& polygon,
                                   std::vector<uint32_t>* outIndices) {
    if (outIndices == nullptr) {
        return CadStatus::TriangulationFailed;
    }
    const size_t n = polygon.size();
    if (n < 3 || n > kMaxProfileVertices) {
        return CadStatus::TriangulationFailed;
    }
    if (polygonSignedAreaTwice(polygon) <= 0.0) {
        return CadStatus::TriangulationFailed;  // must be counter-clockwise
    }

    std::vector<uint32_t> remaining(n);
    for (size_t i = 0; i < n; ++i) remaining[i] = static_cast<uint32_t>(i);
    std::vector<uint32_t> triangles;
    triangles.reserve((n - 2) * 3);

    // Whether the triangle (a, b, c) is an ear of the remaining polygon: convex
    // at b, and containing no OTHER remaining vertex. A vertex on the ear's
    // boundary counts as inside, so a collinear vertex is never cut across.
    const auto isEar = [&](size_t ia, size_t ib, size_t ic) {
        const SketchPoint& a = polygon[remaining[ia]];
        const SketchPoint& b = polygon[remaining[ib]];
        const SketchPoint& c = polygon[remaining[ic]];
        if (cross(a, b, c) <= 0.0) {
            return false;
        }
        for (size_t k = 0; k < remaining.size(); ++k) {
            if (k == ia || k == ib || k == ic) continue;
            const SketchPoint& p = polygon[remaining[k]];
            if (cross(a, b, p) >= 0.0 && cross(b, c, p) >= 0.0 && cross(c, a, p) >= 0.0) {
                return false;
            }
        }
        return true;
    };

    // Bounded: each outer pass removes one vertex or gives up.
    size_t cursor = 0;
    while (remaining.size() > 3) {
        const size_t m = remaining.size();
        bool clipped = false;
        for (size_t attempt = 0; attempt < m; ++attempt) {
            const size_t ib = (cursor + attempt) % m;
            const size_t ia = (ib + m - 1) % m;
            const size_t ic = (ib + 1) % m;
            if (isEar(ia, ib, ic)) {
                triangles.push_back(remaining[ia]);
                triangles.push_back(remaining[ib]);
                triangles.push_back(remaining[ic]);
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(ib));
                cursor = ib % remaining.size();
                clipped = true;
                break;
            }
        }
        if (!clipped) {
            return CadStatus::TriangulationFailed;
        }
    }
    triangles.push_back(remaining[0]);
    triangles.push_back(remaining[1]);
    triangles.push_back(remaining[2]);
    *outIndices = std::move(triangles);
    return CadStatus::Ok;
}

}  // namespace forgeshape
