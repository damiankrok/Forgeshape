#include "forgeshape_sketch_arrangement.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kTol = kSketchCoincidenceMeters;

// ---------------------------------------------------------------------------
// Source edges
// ---------------------------------------------------------------------------
//
// Every supported curve becomes one or more SOURCE EDGES with one parameter
// t: a segment runs a -> b over [0, 1]; a circle runs counter-clockwise from
// its own +u point over [0, 1) (closed); an arc runs from its authored start
// through its authored end over [0, 1], angle = start + t * sweep; a spline
// span runs from authored point i to authored point i + 1 over [0, 1] along
// the cubic Bezier `sketchSplineSpan` states. Endpoints are the AUTHORED
// points, never evaluated ones.

enum class EdgeShape : uint8_t { Segment, Circle, Arc, Bezier };

struct SourceEdge {
    SketchEntityId entity = kNoSketchEntity;
    uint32_t local = 0;
    EdgeShape shape = EdgeShape::Segment;
    SketchPoint a{};  // segment start / authored arc start / authored span start
    SketchPoint b{};  // segment end / authored arc end / authored span end
    SketchPoint center{};
    double radius = 0.0;
    double start = 0.0;  // angle at t = 0
    double sweep = 0.0;  // signed; 2*pi for a circle
    // A spline span's Bezier control points; ctrl[0] == a and ctrl[3] == b
    // exactly.
    SketchPoint ctrl[4]{};
    double minU = 0.0, minV = 0.0, maxU = 0.0, maxV = 0.0;

    bool closed() const { return shape == EdgeShape::Circle; }
    bool round() const { return shape == EdgeShape::Circle || shape == EdgeShape::Arc; }
    bool bezier() const { return shape == EdgeShape::Bezier; }
    // A piece of a curve rather than an exact straight edge.
    bool curved() const { return shape != EdgeShape::Segment; }
};

SketchPoint lerp(const SketchPoint& a, const SketchPoint& b, double t) {
    return SketchPoint{a.u + (b.u - a.u) * t, a.v + (b.v - a.v) * t};
}

double dist(const SketchPoint& a, const SketchPoint& b) {
    return std::hypot(a.u - b.u, a.v - b.v);
}

double crossUV(double au, double av, double bu, double bv) {
    return au * bv - av * bu;
}

// Normalizes into [0, 2*pi).
double wrapAngle(double angle) {
    double w = std::fmod(angle, kTwoPi);
    if (w < 0.0) w += kTwoPi;
    if (w >= kTwoPi) w = 0.0;
    return w;
}

// ---------------------------------------------------------------------------
// Polynomials over a parameter interval
// ---------------------------------------------------------------------------
//
// The contact condition of a spline span with a line (degree 3), a circle
// (degree 6) or a horizontal ray (degree 3) is a polynomial in the span's
// parameter. Its real roots are isolated between the roots of its derivative
// -- on each such interval it is monotone, so it has at most one root there --
// and bisected to the last representable bit. Deterministic: no seed, no
// iteration count that depends on anything but the coefficients.

constexpr int kMaxPolyDegree = 6;

double polyEval(const double* c, int degree, double t) {
    double value = c[degree];
    for (int k = degree - 1; k >= 0; --k) value = value * t + c[k];
    return value;
}

double bisectRoot(const double* c, int degree, double lo, double hi, double fLo) {
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (!(mid > lo && mid < hi)) break;
        const double fm = polyEval(c, degree, mid);
        if (fm == 0.0) return mid;
        if ((fm < 0.0) == (fLo < 0.0)) {
            lo = mid;
            fLo = fm;
        } else {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

// Real roots in [lo, hi], ascending, each a sign change or an exact zero.
void polyRoots(const double* c, int degree, double lo, double hi, std::vector<double>* roots) {
    while (degree > 0 && c[degree] == 0.0) --degree;
    if (degree <= 0) return;
    std::vector<double> breaks{lo};
    if (degree >= 2) {
        double d[kMaxPolyDegree];
        for (int k = 1; k <= degree; ++k) d[k - 1] = c[k] * k;
        std::vector<double> critical;
        polyRoots(d, degree - 1, lo, hi, &critical);
        for (double r : critical) {
            if (r > breaks.back() && r < hi) breaks.push_back(r);
        }
    }
    breaks.push_back(hi);
    const auto add = [roots](double r) {
        if (roots->empty() || roots->back() != r) roots->push_back(r);
    };
    for (size_t k = 0; k + 1 < breaks.size(); ++k) {
        const double a = breaks[k];
        const double b = breaks[k + 1];
        const double fa = polyEval(c, degree, a);
        const double fb = polyEval(c, degree, b);
        if (fa == 0.0) {
            add(a);
        } else if (fb != 0.0 && (fa < 0.0) != (fb < 0.0)) {
            add(bisectRoot(c, degree, a, b, fa));
        }
    }
    if (polyEval(c, degree, hi) == 0.0) add(hi);
}

// The derivative's roots strictly inside (lo, hi): where the polynomial turns.
void polyCritical(const double* c, int degree, double lo, double hi, std::vector<double>* out) {
    while (degree > 0 && c[degree] == 0.0) --degree;
    if (degree < 2) return;
    double d[kMaxPolyDegree];
    for (int k = 1; k <= degree; ++k) d[k - 1] = c[k] * k;
    std::vector<double> roots;
    polyRoots(d, degree - 1, lo, hi, &roots);
    for (double r : roots) {
        if (r > lo && r < hi) out->push_back(r);
    }
}

// ---------------------------------------------------------------------------
// One spline span as a curve
// ---------------------------------------------------------------------------

// The span's power-basis coefficients per coordinate: B(t) = sum c[k] t^k.
void bezierPower(const SourceEdge& e, double cu[4], double cv[4]) {
    const SketchPoint* p = e.ctrl;
    cu[0] = p[0].u;
    cu[1] = 3.0 * (p[1].u - p[0].u);
    cu[2] = 3.0 * (p[0].u - 2.0 * p[1].u + p[2].u);
    cu[3] = p[3].u - 3.0 * p[2].u + 3.0 * p[1].u - p[0].u;
    cv[0] = p[0].v;
    cv[1] = 3.0 * (p[1].v - p[0].v);
    cv[2] = 3.0 * (p[0].v - 2.0 * p[1].v + p[2].v);
    cv[3] = p[3].v - 3.0 * p[2].v + 3.0 * p[1].v - p[0].v;
}

SketchBezierSpan spanOf(const SourceEdge& e) {
    return SketchBezierSpan{e.ctrl[0], e.ctrl[1], e.ctrl[2], e.ctrl[3]};
}

// First derivative, along increasing t.
SketchPoint bezierDerivative(const SourceEdge& e, double t) {
    const SketchPoint* p = e.ctrl;
    const double s = 1.0 - t;
    const double w0 = 3.0 * s * s;
    const double w1 = 6.0 * s * t;
    const double w2 = 3.0 * t * t;
    return SketchPoint{w0 * (p[1].u - p[0].u) + w1 * (p[2].u - p[1].u) + w2 * (p[3].u - p[2].u),
                       w0 * (p[1].v - p[0].v) + w1 * (p[2].v - p[1].v) + w2 * (p[3].v - p[2].v)};
}

SketchPoint bezierSecond(const SourceEdge& e, double t) {
    const SketchPoint* p = e.ctrl;
    const double s = 1.0 - t;
    return SketchPoint{6.0 * (s * (p[2].u - 2.0 * p[1].u + p[0].u) + t * (p[3].u - 2.0 * p[2].u + p[1].u)),
                       6.0 * (s * (p[2].v - 2.0 * p[1].v + p[0].v) + t * (p[3].v - 2.0 * p[2].v + p[1].v))};
}

// The direction of travel along increasing t. Where the first derivative
// vanishes (a span whose handle sits on its end) the curve leaves along the
// second derivative, and arrives against it.
SketchPoint bezierDirection(const SourceEdge& e, double t) {
    SketchPoint d = bezierDerivative(e, t);
    const double scale = std::fmax(dist(e.ctrl[0], e.ctrl[3]), 1.0e-30);
    if (std::hypot(d.u, d.v) <= 1.0e-12 * scale) {
        const SketchPoint dd = bezierSecond(e, t);
        const double sign = t < 0.5 ? 1.0 : -1.0;
        d = SketchPoint{sign * dd.u, sign * dd.v};
    }
    return d;
}

SketchPoint edgePoint(const SourceEdge& e, double t) {
    if (e.shape == EdgeShape::Segment) {
        if (t <= 0.0) return e.a;
        if (t >= 1.0) return e.b;
        return lerp(e.a, e.b, t);
    }
    if (e.shape == EdgeShape::Arc || e.shape == EdgeShape::Bezier) {
        if (t <= 0.0) return e.a;
        if (t >= 1.0) return e.b;
    }
    if (e.shape == EdgeShape::Bezier) {
        return sketchBezierPoint(spanOf(e), t);
    }
    const double angle = e.start + t * e.sweep;
    return SketchPoint{e.center.u + e.radius * std::cos(angle),
                       e.center.v + e.radius * std::sin(angle)};
}

// The parameter of a point already known to lie on the edge's circle, by its
// angle; false when it is outside an arc's span.
bool roundParam(const SourceEdge& e, const SketchPoint& p, double* outT) {
    const double angle = std::atan2(p.v - e.center.v, p.u - e.center.u);
    if (e.shape == EdgeShape::Circle) {
        *outT = wrapAngle(angle) / kTwoPi;
        return true;
    }
    const double delta = e.sweep > 0.0 ? wrapAngle(angle - e.start) : wrapAngle(e.start - angle);
    const double t = delta / std::fabs(e.sweep);
    if (t > 1.0) {
        return false;
    }
    *outT = t;
    return true;
}

// Unit tangent along increasing t.
void edgeTangent(const SourceEdge& e, double t, double* du, double* dv) {
    if (e.shape == EdgeShape::Segment) {
        const double len = dist(e.a, e.b);
        *du = (e.b.u - e.a.u) / len;
        *dv = (e.b.v - e.a.v) / len;
        return;
    }
    if (e.shape == EdgeShape::Bezier) {
        const SketchPoint d = bezierDirection(e, t);
        const double len = std::hypot(d.u, d.v);
        *du = len > 0.0 ? d.u / len : 1.0;
        *dv = len > 0.0 ? d.v / len : 0.0;
        return;
    }
    const double angle = e.start + t * e.sweep;
    const double s = e.sweep > 0.0 ? 1.0 : -1.0;
    *du = -std::sin(angle) * s;
    *dv = std::cos(angle) * s;
}

// Signed curvature at t along increasing t: + turning left.
double edgeCurvature(const SourceEdge& e, double t) {
    if (e.shape == EdgeShape::Segment) return 0.0;
    if (e.shape == EdgeShape::Bezier) {
        const SketchPoint d = bezierDerivative(e, t);
        const SketchPoint dd = bezierSecond(e, t);
        const double speed = std::hypot(d.u, d.v);
        if (!(speed > 0.0)) return 0.0;
        return crossUV(d.u, d.v, dd.u, dd.v) / (speed * speed * speed);
    }
    return (e.sweep > 0.0 ? 1.0 : -1.0) / e.radius;
}

void setBounds(SourceEdge* e) {
    if (e->shape == EdgeShape::Segment) {
        e->minU = std::fmin(e->a.u, e->b.u);
        e->maxU = std::fmax(e->a.u, e->b.u);
        e->minV = std::fmin(e->a.v, e->b.v);
        e->maxV = std::fmax(e->a.v, e->b.v);
    } else if (e->shape == EdgeShape::Bezier) {
        // The control polygon's box contains the curve.
        e->minU = e->maxU = e->ctrl[0].u;
        e->minV = e->maxV = e->ctrl[0].v;
        for (int k = 1; k < 4; ++k) {
            e->minU = std::fmin(e->minU, e->ctrl[k].u);
            e->maxU = std::fmax(e->maxU, e->ctrl[k].u);
            e->minV = std::fmin(e->minV, e->ctrl[k].v);
            e->maxV = std::fmax(e->maxV, e->ctrl[k].v);
        }
    } else {
        // The whole circle bounds an arc too: conservative and exact enough
        // for a pre-filter.
        e->minU = e->center.u - e->radius;
        e->maxU = e->center.u + e->radius;
        e->minV = e->center.v - e->radius;
        e->maxV = e->center.v + e->radius;
    }
}

SourceEdge segmentEdge(SketchEntityId id, uint32_t local, const SketchPoint& a,
                       const SketchPoint& b) {
    SourceEdge e;
    e.entity = id;
    e.local = local;
    e.shape = EdgeShape::Segment;
    e.a = a;
    e.b = b;
    setBounds(&e);
    return e;
}

ArrangementStatus collectSourceEdges(const CadSketch& sketch, std::vector<SourceEdge>* out) {
    std::vector<const SketchEntity*> entities;
    entities.reserve(sketch.entities.size());
    for (const SketchEntity& entity : sketch.entities) {
        entities.push_back(&entity);
    }
    // Semantic order: nothing downstream may depend on the vector's order.
    std::sort(entities.begin(), entities.end(),
              [](const SketchEntity* x, const SketchEntity* y) { return x->id() < y->id(); });
    for (const SketchEntity* entity : entities) {
        const SketchEntityId id = entity->id();
        if (const SketchLine* line = entity->line()) {
            out->push_back(segmentEdge(id, 0u, line->start, line->end));
        } else if (const SketchPolyline* polyline = entity->polyline()) {
            const size_t n = polyline->vertices.size();
            for (size_t k = 0; k + 1 < n; ++k) {
                out->push_back(segmentEdge(id, static_cast<uint32_t>(k), polyline->vertices[k],
                                           polyline->vertices[k + 1]));
            }
            if (polyline->closed && n >= 3) {
                out->push_back(segmentEdge(id, static_cast<uint32_t>(n - 1), polyline->vertices[n - 1],
                                           polyline->vertices[0]));
            }
        } else if (const SketchRectangle* rectangle = entity->rectangle()) {
            const std::vector<SketchPoint> corners = rectangleProfilePolygon(*rectangle);
            for (uint32_t k = 0; k < 4u; ++k) {
                out->push_back(segmentEdge(id, k, corners[k], corners[(k + 1u) % 4u]));
            }
        } else if (const SketchCircle* circle = entity->circle()) {
            SourceEdge e;
            e.entity = id;
            e.local = 0;
            e.shape = EdgeShape::Circle;
            e.center = circle->center;
            e.radius = circle->radius;
            e.start = 0.0;
            e.sweep = kTwoPi;
            e.a = e.b = SketchPoint{circle->center.u + circle->radius, circle->center.v};
            setBounds(&e);
            out->push_back(e);
        } else if (const SketchArc* arc = entity->arc()) {
            SourceEdge e;
            e.entity = id;
            e.local = 0;
            e.shape = EdgeShape::Arc;
            if (arcGeometry(*arc, &e.center, &e.radius, &e.start, &e.sweep) != CadStatus::Ok) {
                return ArrangementStatus::InvalidSketch;
            }
            e.a = arc->start;
            e.b = arc->end;
            setBounds(&e);
            out->push_back(e);
        } else if (const SketchSpline* spline = entity->spline()) {
            // One source edge per authored span: span i is edge-local index i,
            // the semantic name its fragments carry.
            const size_t spans = spline->points.size() - 1u;
            for (size_t i = 0; i < spans; ++i) {
                SketchBezierSpan span;
                if (!sketchSplineSpan(*spline, static_cast<uint32_t>(i), &span)) {
                    return ArrangementStatus::InvalidSketch;
                }
                SourceEdge e;
                e.entity = id;
                e.local = static_cast<uint32_t>(i);
                e.shape = EdgeShape::Bezier;
                e.ctrl[0] = span.p0;
                e.ctrl[1] = span.c1;
                e.ctrl[2] = span.c2;
                e.ctrl[3] = span.p3;
                e.a = span.p0;
                e.b = span.p3;
                setBounds(&e);
                out->push_back(e);
                if (out->size() > kMaxArrangementSourceEdges) {
                    return ArrangementStatus::CapExceeded;
                }
            }
        } else {
            return ArrangementStatus::UnsupportedCurve;
        }
        if (out->size() > kMaxArrangementSourceEdges) {
            return ArrangementStatus::CapExceeded;
        }
    }
    return ArrangementStatus::Ok;
}

// ---------------------------------------------------------------------------
// Pairwise contacts
// ---------------------------------------------------------------------------

// One point where two source edges meet, with each edge's parameter there and
// whether that is the edge's own start (1) or end (2) rather than interior (0).
struct Contact {
    SketchPoint point{};
    double t[2] = {0.0, 0.0};
    uint8_t end[2] = {0, 0};
};

// The parameter in [lo, hi] of the span point nearest p: the best of a fixed
// sampling, polished by Newton on the squared distance and kept only when it is
// better. Deterministic; the distance it returns is measured on the curve.
double nearestOnBezier(const SourceEdge& e, const SketchPoint& p, double lo, double hi,
                       double* outT) {
    constexpr int kSamples = 32;
    double bestT = lo;
    double best = dist(p, edgePoint(e, lo));
    for (int k = 1; k <= kSamples; ++k) {
        const double t = lo + (hi - lo) * static_cast<double>(k) / kSamples;
        const double d = dist(p, edgePoint(e, t));
        if (d < best) {
            best = d;
            bestT = t;
        }
    }
    double t = bestT;
    for (int iteration = 0; iteration < 32; ++iteration) {
        const SketchPoint q = edgePoint(e, t);
        const SketchPoint d1 = bezierDerivative(e, t);
        const SketchPoint d2 = bezierSecond(e, t);
        const double ru = q.u - p.u;
        const double rv = q.v - p.v;
        const double f = ru * d1.u + rv * d1.v;
        const double fp = d1.u * d1.u + d1.v * d1.v + ru * d2.u + rv * d2.v;
        if (!(fp > 0.0)) break;
        const double next = std::fmax(lo, std::fmin(hi, t - f / fp));
        if (next == t) break;
        t = next;
    }
    const double d = dist(p, edgePoint(e, t));
    if (d < best) {
        best = d;
        bestT = t;
    }
    *outT = bestT;
    return best;
}

// Distance from p to the edge, and the parameter of the nearest point.
double distanceToEdge(const SourceEdge& e, const SketchPoint& p, double* outT) {
    if (e.shape == EdgeShape::Bezier) {
        return nearestOnBezier(e, p, 0.0, 1.0, outT);
    }
    if (e.shape == EdgeShape::Segment) {
        const double ru = e.b.u - e.a.u;
        const double rv = e.b.v - e.a.v;
        double t = ((p.u - e.a.u) * ru + (p.v - e.a.v) * rv) / (ru * ru + rv * rv);
        t = std::fmax(0.0, std::fmin(1.0, t));
        *outT = t;
        return dist(p, edgePoint(e, t));
    }
    double t = 0.0;
    if (roundParam(e, p, &t)) {
        *outT = t;
        return std::fabs(dist(p, e.center) - e.radius);
    }
    // Outside an arc's span: its nearer authored end.
    const double da = dist(p, e.a);
    const double db = dist(p, e.b);
    *outT = da <= db ? 0.0 : 1.0;
    return std::fmin(da, db);
}

uint8_t endFlag(const SourceEdge& e, double t) {
    if (e.closed()) return 0;
    if (t <= 0.0) return 1;
    if (t >= 1.0) return 2;
    return 0;
}

// Adds a contact, merging with one already within tolerance. An endpoint-
// derived contact (its point IS an authored point) wins over a computed one.
void addContact(std::vector<Contact>* contacts, const Contact& c) {
    for (Contact& existing : *contacts) {
        if (dist(existing.point, c.point) <= kTol) {
            for (int s = 0; s < 2; ++s) {
                if (existing.end[s] == 0 && c.end[s] != 0) {
                    existing.end[s] = c.end[s];
                    existing.t[s] = c.t[s];
                    existing.point = c.point;
                }
            }
            return;
        }
    }
    contacts->push_back(c);
}

// Endpoint proximity, both ways: an authored endpoint within tolerance of the
// other curve is a contact at exactly that endpoint (a T-junction, or an
// endpoint coincidence when it is also at the other's end).
void endpointContacts(const SourceEdge& e0, const SourceEdge& e1, std::vector<Contact>* out) {
    const SourceEdge* edges[2] = {&e0, &e1};
    for (int s = 0; s < 2; ++s) {
        const SourceEdge& self = *edges[s];
        const SourceEdge& other = *edges[1 - s];
        if (self.closed()) continue;
        for (int k = 0; k < 2; ++k) {
            const SketchPoint& p = k == 0 ? self.a : self.b;
            double otherT = 0.0;
            if (distanceToEdge(other, p, &otherT) > kTol) continue;
            Contact c;
            c.point = p;
            c.t[s] = k == 0 ? 0.0 : 1.0;
            c.end[s] = static_cast<uint8_t>(k == 0 ? 1 : 2);
            // Snap the other edge's parameter to its own end when the point is
            // its authored end.
            if (!other.closed() && dist(p, other.a) <= kTol) otherT = 0.0;
            if (!other.closed() && dist(p, other.b) <= kTol) otherT = 1.0;
            c.t[1 - s] = otherT;
            c.end[1 - s] = endFlag(other, otherT);
            addContact(out, c);
        }
    }
}

ArrangementStatus segmentSegment(const SourceEdge& e0, const SourceEdge& e1,
                                 std::vector<Contact>* out) {
    const double ru = e0.b.u - e0.a.u, rv = e0.b.v - e0.a.v;
    const double su = e1.b.u - e1.a.u, sv = e1.b.v - e1.a.v;
    const double rLen = std::hypot(ru, rv);
    const double sLen = std::hypot(su, sv);
    const double qu = e1.a.u - e0.a.u, qv = e1.a.v - e0.a.v;
    const double rxs = crossUV(ru, rv, su, sv);
    if (std::fabs(rxs) <= 1.0e-12 * rLen * sLen) {
        // Parallel. Collinear within tolerance and sharing more than a
        // tolerance of length is an overlap: no finite set of nodes exists.
        const double off = std::fabs(crossUV(qu, qv, ru, rv)) / rLen;
        if (off <= kTol) {
            const double t0 = (qu * ru + qv * rv) / (rLen * rLen);
            const double t1 = ((qu + su) * ru + (qv + sv) * rv) / (rLen * rLen);
            const double lo = std::fmax(0.0, std::fmin(t0, t1));
            const double hi = std::fmin(1.0, std::fmax(t0, t1));
            if ((hi - lo) * rLen > kTol) {
                return ArrangementStatus::AmbiguousOverlap;
            }
        }
    } else {
        const double t = crossUV(qu, qv, su, sv) / rxs;
        const double u = crossUV(qu, qv, ru, rv) / rxs;
        if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0) {
            Contact c;
            c.point = edgePoint(e0, t);
            c.t[0] = t;
            c.t[1] = u;
            addContact(out, c);
        }
    }
    endpointContacts(e0, e1, out);
    return ArrangementStatus::Ok;
}

// A segment and a circle or arc. A line at tolerance-distance from the circle
// is TANGENT: it touches without crossing and makes no node.
ArrangementStatus segmentRound(const SourceEdge& seg, const SourceEdge& round, bool segFirst,
                               std::vector<Contact>* out, uint32_t* tangents) {
    const double ru = seg.b.u - seg.a.u, rv = seg.b.v - seg.a.v;
    const double pu = seg.a.u - round.center.u, pv = seg.a.v - round.center.v;
    const double a = ru * ru + rv * rv;
    const double b = 2.0 * (ru * pu + rv * pv);
    const double lineDist = std::fabs(crossUV(ru, rv, -pu, -pv)) / std::sqrt(a);
    std::vector<Contact> local;
    const int si = segFirst ? 0 : 1;
    const int ri = 1 - si;
    if (std::fabs(lineDist - round.radius) <= kTol) {
        // Tangent: counted when the touch point is on both, never a node.
        const double tt = -b / (2.0 * a);
        double rt = 0.0;
        if (tt >= 0.0 && tt <= 1.0 && roundParam(round, edgePoint(seg, tt), &rt)) {
            ++*tangents;
        }
    } else if (lineDist < round.radius) {
        const double c = pu * pu + pv * pv - round.radius * round.radius;
        const double disc = std::sqrt(std::fmax(0.0, b * b - 4.0 * a * c));
        const double roots[2] = {(-b - disc) / (2.0 * a), (-b + disc) / (2.0 * a)};
        for (double t : roots) {
            if (t < 0.0 || t > 1.0) continue;
            const SketchPoint p = edgePoint(seg, t);
            double rt = 0.0;
            if (!roundParam(round, p, &rt)) continue;
            Contact contact;
            contact.point = p;
            contact.t[si] = t;
            contact.t[ri] = rt;
            addContact(&local, contact);
        }
    }
    endpointContacts(segFirst ? seg : round, segFirst ? round : seg, &local);
    for (const Contact& c : local) addContact(out, c);
    return ArrangementStatus::Ok;
}

// Two circles or arcs. Coincident circles sharing more than a tolerance of
// arc are an overlap; tangent circles touch without crossing (no lens).
ArrangementStatus roundRound(const SourceEdge& e0, const SourceEdge& e1,
                             std::vector<Contact>* out, uint32_t* tangents) {
    const double du = e1.center.u - e0.center.u;
    const double dv = e1.center.v - e0.center.v;
    const double d = std::hypot(du, dv);
    const double r0 = e0.radius, r1 = e1.radius;
    std::vector<Contact> local;
    if (d <= kTol && std::fabs(r0 - r1) <= kTol) {
        // The same circle. Any shared span longer than the tolerance is an
        // overlap; arcs meeting only at their ends are ordinary contacts.
        const auto spanHas = [](const SourceEdge& e, double angle) {
            if (e.shape == EdgeShape::Circle) return true;
            const double delta =
                    e.sweep > 0.0 ? wrapAngle(angle - e.start) : wrapAngle(e.start - angle);
            return delta <= std::fabs(e.sweep);
        };
        // Two spans on one circle share at least a tolerance of arc exactly
        // when one span's end, stepped half a tolerance inward, or its
        // midpoint, lies in the other span.
        const SourceEdge* es[2] = {&e0, &e1};
        for (int s = 0; s < 2; ++s) {
            const SourceEdge& self = *es[s];
            const SourceEdge& other = *es[1 - s];
            const double step = 0.5 * kTol / self.radius * (self.sweep > 0.0 ? 1.0 : -1.0);
            const double probes[3] = {self.start + step, self.start + 0.5 * self.sweep,
                                      self.start + self.sweep - step};
            for (double angle : probes) {
                if (spanHas(other, angle)) {
                    return ArrangementStatus::AmbiguousOverlap;
                }
            }
        }
        endpointContacts(e0, e1, &local);
        for (const Contact& c : local) addContact(out, c);
        return ArrangementStatus::Ok;
    }
    const bool outerTangent = std::fabs(d - (r0 + r1)) <= kTol;
    const bool innerTangent = d > kTol && std::fabs(d - std::fabs(r0 - r1)) <= kTol;
    if (outerTangent || innerTangent) {
        // One touch point along the centre line.
        const double sign = (outerTangent || r0 >= r1) ? 1.0 : -1.0;
        const SketchPoint p{e0.center.u + sign * du / d * r0, e0.center.v + sign * dv / d * r0};
        double t0 = 0.0, t1 = 0.0;
        if (roundParam(e0, p, &t0) && roundParam(e1, p, &t1)) {
            ++*tangents;
        }
    } else if (d < r0 + r1 && d > std::fabs(r0 - r1)) {
        const double a = (d * d + r0 * r0 - r1 * r1) / (2.0 * d);
        const double h = std::sqrt(std::fmax(0.0, r0 * r0 - a * a));
        const double eu = du / d, ev = dv / d;
        const SketchPoint base{e0.center.u + a * eu, e0.center.v + a * ev};
        const SketchPoint points[2] = {SketchPoint{base.u - h * ev, base.v + h * eu},
                                       SketchPoint{base.u + h * ev, base.v - h * eu}};
        for (const SketchPoint& p : points) {
            double t0 = 0.0, t1 = 0.0;
            if (!roundParam(e0, p, &t0) || !roundParam(e1, p, &t1)) continue;
            Contact c;
            c.point = p;
            c.t[0] = t0;
            c.t[1] = t1;
            addContact(&local, c);
        }
    }
    endpointContacts(e0, e1, &local);
    for (const Contact& c : local) addContact(out, c);
    return ArrangementStatus::Ok;
}

// ---------------------------------------------------------------------------
// Spline spans against everything (`CAD-V6-S2-CORRECTION-FILL-HUD-R1`)
// ---------------------------------------------------------------------------

// Roots of a contact function along a span that are really one TOUCH.
//
// `breaks` is 0, the function's turning points ascending, 1; `roots` its
// roots. At a turning point where the curve comes within the coincidence
// tolerance of the other curve, the curves touch: if the curve dips across and
// back -- a root on each side of the turning point -- both roots are that one
// touch and are dropped, exactly as a line within a tolerance of a circle has
// always been tangent rather than two crossings a micrometre apart. A lone root
// beside such a turning point is a crossing that happens to turn right after,
// and is kept. Every touch's parameter is reported for the tangent count.
void dropTouchingRoots(const std::vector<double>& breaks, const std::vector<double>& roots,
                       const std::vector<double>& turningDistance, std::vector<uint8_t>* drop,
                       std::vector<double>* touches) {
    drop->assign(roots.size(), 0u);
    for (size_t j = 1; j + 1 < breaks.size(); ++j) {
        if (turningDistance[j] > kTol) continue;
        const double tc = breaks[j];
        int64_t left = -1;
        int64_t right = -1;
        for (size_t i = 0; i < roots.size(); ++i) {
            if ((*drop)[i] != 0u) continue;
            if (roots[i] >= breaks[j - 1] && roots[i] <= tc) left = static_cast<int64_t>(i);
            if (right < 0 && roots[i] >= tc && roots[i] <= breaks[j + 1]) {
                right = static_cast<int64_t>(i);
            }
        }
        if (left >= 0 && right >= 0) {
            (*drop)[static_cast<size_t>(left)] = 1u;
            (*drop)[static_cast<size_t>(right)] = 1u;
            touches->push_back(tc);
        } else if (left < 0 && right < 0) {
            touches->push_back(tc);
        }
    }
}

// The span's breakpoints for a contact polynomial: 0, its turning points, 1.
std::vector<double> spanBreaks(const double* g, int degree) {
    std::vector<double> breaks{0.0};
    polyCritical(g, degree, 0.0, 1.0, &breaks);
    breaks.push_back(1.0);
    return breaks;
}

// A spline span and a straight segment: the span's signed distance to the
// segment's line is a cubic in t.
ArrangementStatus bezierSegment(const SourceEdge& bez, const SourceEdge& seg, bool bezFirst,
                                std::vector<Contact>* out, uint32_t* tangents) {
    const double ru = seg.b.u - seg.a.u;
    const double rv = seg.b.v - seg.a.v;
    const double len = std::hypot(ru, rv);
    const double nu = -rv / len;
    const double nv = ru / len;
    double cu[4];
    double cv[4];
    bezierPower(bez, cu, cv);
    double g[4];
    for (int k = 0; k < 4; ++k) g[k] = nu * cu[k] + nv * cv[k];
    g[0] -= nu * seg.a.u + nv * seg.a.v;
    const auto segParam = [&](const SketchPoint& p) {
        return ((p.u - seg.a.u) * ru + (p.v - seg.a.v) * rv) / (len * len);
    };
    const auto lineDistance = [&](double t) {
        const SketchPoint p = edgePoint(bez, t);
        return nu * (p.u - seg.a.u) + nv * (p.v - seg.a.v);
    };
    const std::vector<double> breaks = spanBreaks(g, 3);
    std::vector<double> turning(breaks.size(), 0.0);
    double farthest = 0.0;
    for (size_t j = 0; j < breaks.size(); ++j) {
        turning[j] = std::fabs(lineDistance(breaks[j]));
        farthest = std::fmax(farthest, turning[j]);
    }
    const int si = bezFirst ? 1 : 0;
    const int bi = 1 - si;
    std::vector<Contact> local;
    if (farthest <= kTol) {
        // The whole span lies along the line (a two-point spline is a straight
        // span): sharing more than a tolerance of the segment is an overlap.
        double lo = INFINITY;
        double hi = -INFINITY;
        for (int k = 0; k <= 16; ++k) {
            const double s = segParam(edgePoint(bez, k / 16.0));
            lo = std::fmin(lo, s);
            hi = std::fmax(hi, s);
        }
        if ((std::fmin(1.0, hi) - std::fmax(0.0, lo)) * len > kTol) {
            return ArrangementStatus::AmbiguousOverlap;
        }
    } else {
        std::vector<double> roots;
        polyRoots(g, 3, 0.0, 1.0, &roots);
        std::vector<uint8_t> drop;
        std::vector<double> touches;
        dropTouchingRoots(breaks, roots, turning, &drop, &touches);
        for (double tc : touches) {
            const double s = segParam(edgePoint(bez, tc));
            if (s >= 0.0 && s <= 1.0) ++*tangents;
        }
        for (size_t i = 0; i < roots.size(); ++i) {
            if (drop[i] != 0u) continue;
            const SketchPoint p = edgePoint(bez, roots[i]);
            const double s = segParam(p);
            if (s < 0.0 || s > 1.0) continue;
            Contact c;
            c.point = p;
            c.t[bi] = roots[i];
            c.t[si] = s;
            addContact(&local, c);
        }
    }
    endpointContacts(bezFirst ? bez : seg, bezFirst ? seg : bez, &local);
    for (const Contact& c : local) addContact(out, c);
    return ArrangementStatus::Ok;
}

// A spline span and a circle or arc: |B(t) - c|^2 - r^2 is a sextic in t.
ArrangementStatus bezierRound(const SourceEdge& bez, const SourceEdge& round, bool bezFirst,
                              std::vector<Contact>* out, uint32_t* tangents) {
    double cu[4];
    double cv[4];
    bezierPower(bez, cu, cv);
    cu[0] -= round.center.u;
    cv[0] -= round.center.v;
    double g[7] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) g[i + j] += cu[i] * cu[j] + cv[i] * cv[j];
    }
    g[0] -= round.radius * round.radius;
    const auto radial = [&](double t) {
        return dist(edgePoint(bez, t), round.center) - round.radius;
    };
    const std::vector<double> breaks = spanBreaks(g, 6);
    std::vector<double> turning(breaks.size(), 0.0);
    double farthest = 0.0;
    for (size_t j = 0; j < breaks.size(); ++j) {
        turning[j] = std::fabs(radial(breaks[j]));
        farthest = std::fmax(farthest, turning[j]);
    }
    const int ri = bezFirst ? 1 : 0;
    const int bi = 1 - ri;
    std::vector<Contact> local;
    if (farthest <= kTol) {
        // The whole span lies on the circle: an overlap wherever it lies on
        // the edge's own span.
        for (int k = 0; k <= 16; ++k) {
            double rt = 0.0;
            if (roundParam(round, edgePoint(bez, k / 16.0), &rt)) {
                return ArrangementStatus::AmbiguousOverlap;
            }
        }
    } else {
        std::vector<double> roots;
        polyRoots(g, 6, 0.0, 1.0, &roots);
        std::vector<uint8_t> drop;
        std::vector<double> touches;
        dropTouchingRoots(breaks, roots, turning, &drop, &touches);
        for (double tc : touches) {
            double rt = 0.0;
            if (roundParam(round, edgePoint(bez, tc), &rt)) ++*tangents;
        }
        for (size_t i = 0; i < roots.size(); ++i) {
            if (drop[i] != 0u) continue;
            const SketchPoint p = edgePoint(bez, roots[i]);
            double rt = 0.0;
            if (!roundParam(round, p, &rt)) continue;
            Contact c;
            c.point = p;
            c.t[bi] = roots[i];
            c.t[ri] = rt;
            addContact(&local, c);
        }
    }
    endpointContacts(bezFirst ? bez : round, bezFirst ? round : bez, &local);
    for (const Contact& c : local) addContact(out, c);
    return ArrangementStatus::Ok;
}

// --- span against span: subdivision, then Newton on the exact curves -------

// A piece of one span over [t0, t1] of the SPAN's parameter, as its own Bezier.
struct BezierPiece {
    SketchPoint p[4];
    double t0 = 0.0;
    double t1 = 1.0;
};

BezierPiece wholePiece(const SourceEdge& e) {
    BezierPiece piece;
    for (int k = 0; k < 4; ++k) piece.p[k] = e.ctrl[k];
    return piece;
}

// de Casteljau at the middle.
void splitPiece(const BezierPiece& in, BezierPiece* left, BezierPiece* right) {
    const SketchPoint a = lerp(in.p[0], in.p[1], 0.5);
    const SketchPoint b = lerp(in.p[1], in.p[2], 0.5);
    const SketchPoint c = lerp(in.p[2], in.p[3], 0.5);
    const SketchPoint d = lerp(a, b, 0.5);
    const SketchPoint e = lerp(b, c, 0.5);
    const SketchPoint m = lerp(d, e, 0.5);
    const double tm = 0.5 * (in.t0 + in.t1);
    *left = BezierPiece{{in.p[0], a, d, m}, in.t0, tm};
    *right = BezierPiece{{m, e, c, in.p[3]}, tm, in.t1};
}

void pieceBox(const BezierPiece& q, double box[4]) {
    box[0] = box[2] = q.p[0].u;
    box[1] = box[3] = q.p[0].v;
    for (int k = 1; k < 4; ++k) {
        box[0] = std::fmin(box[0], q.p[k].u);
        box[1] = std::fmin(box[1], q.p[k].v);
        box[2] = std::fmax(box[2], q.p[k].u);
        box[3] = std::fmax(box[3], q.p[k].v);
    }
}

// How far the handles stand off the chord's line: the most the curve can.
double pieceFlatness(const BezierPiece& q) {
    const double cu = q.p[3].u - q.p[0].u;
    const double cv = q.p[3].v - q.p[0].v;
    const double len = std::hypot(cu, cv);
    if (!(len > 0.0)) {
        return std::fmax(dist(q.p[1], q.p[0]), dist(q.p[2], q.p[0]));
    }
    const double d1 = std::fabs(crossUV(cu, cv, q.p[1].u - q.p[0].u, q.p[1].v - q.p[0].v)) / len;
    const double d2 = std::fabs(crossUV(cu, cv, q.p[2].u - q.p[0].u, q.p[2].v - q.p[0].v)) / len;
    return std::fmax(d1, d2);
}

// Subdivision stops when both pieces are flat to a twentieth of the coincidence
// tolerance or at this depth; the visits are bounded per span pair, and a pair
// that exhausts them shares a stretch no finite node set splits.
constexpr double kBezierFlatness = 0.05 * kTol;
constexpr int kMaxBezierDepth = 40;
constexpr uint32_t kMaxBezierPairVisits = 1u << 15;
// How far either side of a contact the two curves are compared to tell a
// crossing from a touch: twenty coincidence tolerances along the curve.
constexpr double kSideProbeMeters = 20.0 * kTol;

struct BezierSearch {
    // (sA, tB) parameter pairs to refine, in discovery order.
    std::vector<std::pair<double, double>> candidates;
    uint32_t visits = 0;
    bool exhausted = false;
};

// Where two flat pieces' chords meet (or, parallel, come within tolerance),
// as span parameters.
void chordCandidate(const BezierPiece& a, const BezierPiece& b, BezierSearch* search) {
    const double ru = a.p[3].u - a.p[0].u, rv = a.p[3].v - a.p[0].v;
    const double qu = b.p[3].u - b.p[0].u, qv = b.p[3].v - b.p[0].v;
    const double wu = b.p[0].u - a.p[0].u, wv = b.p[0].v - a.p[0].v;
    const double rLen = std::hypot(ru, rv);
    const double qLen = std::hypot(qu, qv);
    const double rxq = crossUV(ru, rv, qu, qv);
    constexpr double kMargin = 0.1;
    double sigma = 0.0;
    double tau = 0.0;
    if (rLen > 0.0 && qLen > 0.0 && std::fabs(rxq) > 1.0e-12 * rLen * qLen) {
        sigma = crossUV(wu, wv, qu, qv) / rxq;
        tau = crossUV(wu, wv, ru, rv) / rxq;
        if (sigma < -kMargin || sigma > 1.0 + kMargin || tau < -kMargin || tau > 1.0 + kMargin) {
            return;
        }
    } else {
        // Parallel (or a point): the nearest approach, if within tolerance.
        const double r2 = rLen * rLen;
        sigma = r2 > 0.0 ? ((wu + 0.5 * qu) * ru + (wv + 0.5 * qv) * rv) / r2 : 0.0;
        sigma = std::fmax(0.0, std::fmin(1.0, sigma));
        const SketchPoint pa = lerp(a.p[0], a.p[3], sigma);
        const double q2 = qLen * qLen;
        tau = q2 > 0.0 ? ((pa.u - b.p[0].u) * qu + (pa.v - b.p[0].v) * qv) / q2 : 0.0;
        tau = std::fmax(0.0, std::fmin(1.0, tau));
        if (dist(pa, lerp(b.p[0], b.p[3], tau)) > kTol) return;
    }
    sigma = std::fmax(0.0, std::fmin(1.0, sigma));
    tau = std::fmax(0.0, std::fmin(1.0, tau));
    search->candidates.emplace_back(a.t0 + (a.t1 - a.t0) * sigma, b.t0 + (b.t1 - b.t0) * tau);
}

void searchPieces(const BezierPiece& a, const BezierPiece& b, int depth, BezierSearch* search) {
    if (search->exhausted) return;
    if (++search->visits > kMaxBezierPairVisits) {
        search->exhausted = true;
        return;
    }
    double ba[4];
    double bb[4];
    pieceBox(a, ba);
    pieceBox(b, bb);
    if (ba[0] > bb[2] + kTol || bb[0] > ba[2] + kTol || ba[1] > bb[3] + kTol
        || bb[1] > ba[3] + kTol) {
        return;
    }
    const bool flatA = pieceFlatness(a) <= kBezierFlatness;
    const bool flatB = pieceFlatness(b) <= kBezierFlatness;
    if ((flatA && flatB) || depth >= kMaxBezierDepth) {
        chordCandidate(a, b, search);
        return;
    }
    const double sizeA = std::fmax(ba[2] - ba[0], ba[3] - ba[1]);
    const double sizeB = std::fmax(bb[2] - bb[0], bb[3] - bb[1]);
    BezierPiece left;
    BezierPiece right;
    if (!flatA && (flatB || sizeA >= sizeB)) {
        splitPiece(a, &left, &right);
        searchPieces(left, b, depth + 1, search);
        searchPieces(right, b, depth + 1, search);
    } else {
        splitPiece(b, &left, &right);
        searchPieces(a, left, depth + 1, search);
        searchPieces(a, right, depth + 1, search);
    }
}

// Newton on A(s) - B(t) = 0 from a chord estimate, parameters held in [0, 1].
// True when the refined point lies within tolerance on both curves.
bool refineSpanContact(const SourceEdge& ea, const SourceEdge& eb, double* s, double* t) {
    for (int iteration = 0; iteration < 48; ++iteration) {
        const SketchPoint pa = edgePoint(ea, *s);
        const SketchPoint pb = edgePoint(eb, *t);
        const double fu = pa.u - pb.u;
        const double fv = pa.v - pb.v;
        const SketchPoint da = bezierDerivative(ea, *s);
        const SketchPoint db = bezierDerivative(eb, *t);
        // J = [da, -db]; solve J * (ds, dt) = -F.
        const double det = crossUV(da.u, da.v, -db.u, -db.v);
        if (!(std::fabs(det) > 0.0)) break;
        const double ds = crossUV(-fu, -fv, -db.u, -db.v) / det;
        const double dt = crossUV(da.u, da.v, -fu, -fv) / det;
        const double ns = std::fmax(0.0, std::fmin(1.0, *s + ds));
        const double nt = std::fmax(0.0, std::fmin(1.0, *t + dt));
        if (ns == *s && nt == *t) break;
        *s = ns;
        *t = nt;
    }
    return dist(edgePoint(ea, *s), edgePoint(eb, *t)) <= kTol;
}

// Signed distance from q to span e near parameter s (+ on e's left).
double signedDistanceToSpan(const SourceEdge& e, double s, const SketchPoint& q) {
    double t = s;
    nearestOnBezier(e, q, std::fmax(0.0, s - 0.25), std::fmin(1.0, s + 0.25), &t);
    const SketchPoint p = edgePoint(e, t);
    const SketchPoint d = bezierDirection(e, t);
    const double len = std::hypot(d.u, d.v);
    if (!(len > 0.0)) return 0.0;
    return crossUV(d.u, d.v, q.u - p.u, q.v - p.v) / len;
}

enum class SpanContactKind : uint8_t { Crossing, Touch, Shared };

// Whether span b passes from one side of span a to the other at (s, t): b is
// probed a few tolerances either side along its own length and each probe's
// signed distance to a is measured on a's exact curve. Both probes still ON a
// (to the noise of the arithmetic) is a shared stretch.
SpanContactKind classifySpanContact(const SourceEdge& a, const SourceEdge& b, double s, double t) {
    const SketchPoint db = bezierDirection(b, t);
    const double speed = std::hypot(db.u, db.v);
    const double step = speed > 0.0 ? kSideProbeMeters / speed : 0.0;
    const double minus = signedDistanceToSpan(a, s, edgePoint(b, std::fmax(0.0, t - step)));
    const double plus = signedDistanceToSpan(a, s, edgePoint(b, std::fmin(1.0, t + step)));
    double scale = 1.0;
    for (int k = 0; k < 4; ++k) {
        scale = std::fmax(scale, std::fmax(std::fabs(a.ctrl[k].u), std::fabs(a.ctrl[k].v)));
    }
    const double noise = 1.0e-12 * scale;
    if (std::fabs(minus) <= noise && std::fabs(plus) <= noise) {
        return SpanContactKind::Shared;
    }
    if (std::fabs(minus) > noise && std::fabs(plus) > noise && (minus > 0.0) != (plus > 0.0)) {
        return SpanContactKind::Crossing;
    }
    return SpanContactKind::Touch;
}

bool nearSpanEnd(const SourceEdge& e, const SketchPoint& p) {
    return dist(p, e.a) <= kTol || dist(p, e.b) <= kTol;
}

ArrangementStatus bezierBezier(const SourceEdge& e0, const SourceEdge& e1,
                               std::vector<Contact>* out, uint32_t* tangents) {
    // The same span twice (two splines through the same points, either way
    // round) shares its whole length.
    const bool same = e0.ctrl[0].u == e1.ctrl[0].u && e0.ctrl[0].v == e1.ctrl[0].v
                      && e0.ctrl[1].u == e1.ctrl[1].u && e0.ctrl[1].v == e1.ctrl[1].v
                      && e0.ctrl[2].u == e1.ctrl[2].u && e0.ctrl[2].v == e1.ctrl[2].v
                      && e0.ctrl[3].u == e1.ctrl[3].u && e0.ctrl[3].v == e1.ctrl[3].v;
    const bool reversed = e0.ctrl[0].u == e1.ctrl[3].u && e0.ctrl[0].v == e1.ctrl[3].v
                          && e0.ctrl[1].u == e1.ctrl[2].u && e0.ctrl[1].v == e1.ctrl[2].v
                          && e0.ctrl[2].u == e1.ctrl[1].u && e0.ctrl[2].v == e1.ctrl[1].v
                          && e0.ctrl[3].u == e1.ctrl[0].u && e0.ctrl[3].v == e1.ctrl[0].v;
    if (same || reversed) {
        return ArrangementStatus::AmbiguousOverlap;
    }
    BezierSearch search;
    searchPieces(wholePiece(e0), wholePiece(e1), 0, &search);
    if (search.exhausted) {
        return ArrangementStatus::AmbiguousOverlap;
    }
    std::vector<Contact> local;
    std::vector<SketchPoint> decided;
    for (const std::pair<double, double>& candidate : search.candidates) {
        double s = candidate.first;
        double t = candidate.second;
        if (!refineSpanContact(e0, e1, &s, &t)) continue;
        const SketchPoint p = edgePoint(e0, s);
        // An authored end is the endpoint contacts' business, and a point
        // already decided is not decided twice.
        if (nearSpanEnd(e0, p) || nearSpanEnd(e1, p)) continue;
        bool seen = false;
        for (const SketchPoint& q : decided) seen = seen || dist(p, q) <= kTol;
        if (seen) continue;
        decided.push_back(p);
        switch (classifySpanContact(e0, e1, s, t)) {
            case SpanContactKind::Shared:
                return ArrangementStatus::AmbiguousOverlap;
            case SpanContactKind::Touch:
                ++*tangents;
                break;
            case SpanContactKind::Crossing: {
                Contact c;
                c.point = p;
                c.t[0] = s;
                c.t[1] = t;
                addContact(&local, c);
                break;
            }
        }
    }
    endpointContacts(e0, e1, &local);
    for (const Contact& c : local) addContact(out, c);
    return ArrangementStatus::Ok;
}

// How far a piece's control polygon turns, radians.
double pieceTurning(const BezierPiece& q) {
    double total = 0.0;
    double pu = 0.0;
    double pv = 0.0;
    bool have = false;
    for (int k = 0; k < 3; ++k) {
        const double du = q.p[k + 1].u - q.p[k].u;
        const double dv = q.p[k + 1].v - q.p[k].v;
        if (!(std::hypot(du, dv) > 0.0)) continue;
        if (have) total += std::fabs(std::atan2(crossUV(pu, pv, du, dv), pu * du + pv * dv));
        pu = du;
        pv = dv;
        have = true;
    }
    return total;
}

// A span cannot cross itself where its control polygon turns by less than a
// quarter turn, so it is cut into such pieces (bounded depth) and every two
// pieces are searched against each other; a contact anywhere but the joint two
// neighbouring pieces share is the span meeting itself.
ArrangementStatus spanSelfIntersection(const SourceEdge& e) {
    std::vector<BezierPiece> pieces;
    std::vector<std::pair<BezierPiece, int>> stack{{wholePiece(e), 0}};
    while (!stack.empty()) {
        const std::pair<BezierPiece, int> top = stack.back();
        stack.pop_back();
        if (pieceTurning(top.first) < 0.5 * kPi || top.second >= 12) {
            pieces.push_back(top.first);
            continue;
        }
        BezierPiece left;
        BezierPiece right;
        splitPiece(top.first, &left, &right);
        // Right first, so the left half is processed (and appended) first.
        stack.push_back({right, top.second + 1});
        stack.push_back({left, top.second + 1});
    }
    for (size_t i = 0; i < pieces.size(); ++i) {
        for (size_t j = i + 1; j < pieces.size(); ++j) {
            BezierSearch search;
            searchPieces(pieces[i], pieces[j], 0, &search);
            if (search.exhausted) {
                return ArrangementStatus::SelfIntersectingCurve;
            }
            for (const std::pair<double, double>& candidate : search.candidates) {
                double s = candidate.first;
                double t = candidate.second;
                if (!refineSpanContact(e, e, &s, &t)) continue;
                const SketchPoint p = edgePoint(e, s);
                const SketchPoint q = edgePoint(e, t);
                // Two parameters within tolerance along the curve itself are one
                // place on it, not a meeting.
                const double between = std::fabs(s - t);
                const SketchPoint mid = edgePoint(e, 0.5 * (s + t));
                if (between < 1.0e-9 || (dist(p, mid) <= kTol && dist(q, mid) <= kTol)) continue;
                if (j == i + 1 && dist(p, pieces[i].p[3]) <= kTol) continue;
                return ArrangementStatus::SelfIntersectingCurve;
            }
        }
    }
    return ArrangementStatus::Ok;
}

bool boundsOverlap(const SourceEdge& a, const SourceEdge& b) {
    return a.minU <= b.maxU + kTol && b.minU <= a.maxU + kTol && a.minV <= b.maxV + kTol
           && b.minV <= a.maxV + kTol;
}

// A contact recorded against the pair of edge indices it joins.
struct PairContact {
    uint32_t edge[2] = {0, 0};
    Contact contact;
};

ArrangementStatus collectContacts(const std::vector<SourceEdge>& edges,
                                  std::vector<PairContact>* out, ArrangementStats* stats) {
    for (uint32_t i = 0; i < edges.size(); ++i) {
        for (uint32_t j = i + 1; j < edges.size(); ++j) {
            const SourceEdge& a = edges[i];
            const SourceEdge& b = edges[j];
            if (!boundsOverlap(a, b)) continue;
            std::vector<Contact> contacts;
            ArrangementStatus why = ArrangementStatus::Ok;
            if (a.bezier() && b.bezier()) {
                why = bezierBezier(a, b, &contacts, &stats->tangents);
            } else if (a.bezier()) {
                why = b.round() ? bezierRound(a, b, true, &contacts, &stats->tangents)
                                : bezierSegment(a, b, true, &contacts, &stats->tangents);
            } else if (b.bezier()) {
                why = a.round() ? bezierRound(b, a, false, &contacts, &stats->tangents)
                                : bezierSegment(b, a, false, &contacts, &stats->tangents);
            } else if (!a.round() && !b.round()) {
                why = segmentSegment(a, b, &contacts);
            } else if (!a.round()) {
                why = segmentRound(a, b, true, &contacts, &stats->tangents);
            } else if (!b.round()) {
                why = segmentRound(b, a, false, &contacts, &stats->tangents);
            } else {
                why = roundRound(a, b, &contacts, &stats->tangents);
            }
            if (why != ArrangementStatus::Ok) return why;
            for (const Contact& c : contacts) {
                const bool end0 = c.end[0] != 0, end1 = c.end[1] != 0;
                if (end0 && end1) {
                    ++stats->endpointCoincidences;
                } else if (end0 || end1) {
                    ++stats->tJunctions;
                } else {
                    ++stats->crossings;
                }
                out->push_back(PairContact{{i, j}, c});
                if (out->size() > kMaxArrangementContacts) return ArrangementStatus::CapExceeded;
            }
        }
    }
    return ArrangementStatus::Ok;
}

// ---------------------------------------------------------------------------
// Nodes: every authored endpoint and every contact point, clustered at the
// coincidence tolerance. A cluster is a connected component of the "within
// tolerance" relation, so it does not depend on the order points arrive in.
// ---------------------------------------------------------------------------

struct NodePoint {
    SketchPoint point{};
    // Semantic ordering key: authored endpoints (0) before contacts (1), then
    // the edge index (edges are in semantic order) and a sub-index.
    uint32_t key[3] = {0, 0, 0};
};

bool keyLess(const NodePoint& a, const NodePoint& b) {
    return std::lexicographical_compare(a.key, a.key + 3, b.key, b.key + 3);
}

uint32_t findRoot(std::vector<uint32_t>* parent, uint32_t x) {
    while ((*parent)[x] != x) {
        (*parent)[x] = (*parent)[(*parent)[x]];
        x = (*parent)[x];
    }
    return x;
}

// Returns, for each input point, its node index; writes node positions in
// canonical order (by each cluster's smallest key).
std::vector<uint32_t> clusterNodes(const std::vector<NodePoint>& points,
                                   std::vector<SketchPoint>* nodes) {
    const uint32_t n = static_cast<uint32_t>(points.size());
    std::vector<uint32_t> byU(n);
    std::iota(byU.begin(), byU.end(), 0u);
    std::sort(byU.begin(), byU.end(), [&](uint32_t x, uint32_t y) {
        if (points[x].point.u != points[y].point.u) return points[x].point.u < points[y].point.u;
        return keyLess(points[x], points[y]);
    });
    std::vector<uint32_t> parent(n);
    std::iota(parent.begin(), parent.end(), 0u);
    for (uint32_t i = 0; i < n; ++i) {
        const SketchPoint& p = points[byU[i]].point;
        for (uint32_t j = i + 1; j < n; ++j) {
            const SketchPoint& q = points[byU[j]].point;
            if (q.u - p.u > kTol) break;
            if (dist(p, q) <= kTol) {
                const uint32_t ra = findRoot(&parent, byU[i]);
                const uint32_t rb = findRoot(&parent, byU[j]);
                if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
            }
        }
    }
    // Each cluster's representative: its smallest-key point.
    std::vector<int64_t> rep(n, -1);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t r = findRoot(&parent, i);
        if (rep[r] < 0 || keyLess(points[i], points[static_cast<uint32_t>(rep[r])])) {
            rep[r] = i;
        }
    }
    std::vector<uint32_t> roots;
    for (uint32_t i = 0; i < n; ++i) {
        if (findRoot(&parent, i) == i) roots.push_back(i);
    }
    std::sort(roots.begin(), roots.end(), [&](uint32_t x, uint32_t y) {
        return keyLess(points[static_cast<uint32_t>(rep[x])], points[static_cast<uint32_t>(rep[y])]);
    });
    std::vector<uint32_t> nodeOfRoot(n, 0);
    nodes->clear();
    for (uint32_t k = 0; k < roots.size(); ++k) {
        nodeOfRoot[roots[k]] = k;
        nodes->push_back(points[static_cast<uint32_t>(rep[roots[k]])].point);
    }
    std::vector<uint32_t> out(n);
    for (uint32_t i = 0; i < n; ++i) out[i] = nodeOfRoot[findRoot(&parent, i)];
    return out;
}

// ---------------------------------------------------------------------------
// Fragments
// ---------------------------------------------------------------------------

struct Cut {
    double t = 0.0;
    uint32_t node = 0;
    ArrangementCut ref{};
};

struct Fragment {
    uint32_t edge = 0;
    double t0 = 0.0;
    double t1 = 0.0;  // may exceed 1 for a circle's wrap fragment
    uint32_t node0 = 0;
    uint32_t node1 = 0;
    FragmentRef ref{};
    bool live = true;
};

// ---------------------------------------------------------------------------
// Geometry over fragments (exact for segments and arcs)
// ---------------------------------------------------------------------------

// Signed area contribution (Green) of walking fragment f forward or reversed.
double fragmentArea(const SourceEdge& e, const Fragment& f, bool reversed) {
    double value = 0.0;
    if (e.shape == EdgeShape::Bezier) {
        // u v' - v u' along a cubic is a polynomial of degree 5 in t, which
        // three Gauss-Legendre nodes integrate exactly.
        static const double kNodes[3] = {-0.7745966692414834, 0.0, 0.7745966692414834};
        static const double kWeights[3] = {5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0};
        const double half = 0.5 * (f.t1 - f.t0);
        const double mid = 0.5 * (f.t1 + f.t0);
        double sum = 0.0;
        for (int k = 0; k < 3; ++k) {
            const double t = mid + half * kNodes[k];
            const SketchPoint p = sketchBezierPoint(spanOf(e), t);
            const SketchPoint d = bezierDerivative(e, t);
            sum += kWeights[k] * (p.u * d.v - p.v * d.u);
        }
        value = 0.5 * half * sum;
        return reversed ? -value : value;
    }
    if (e.shape == EdgeShape::Segment) {
        const SketchPoint p = edgePoint(e, f.t0);
        const SketchPoint q = edgePoint(e, f.t1);
        value = 0.5 * crossUV(p.u, p.v, q.u, q.v);
    } else {
        const double th0 = e.start + f.t0 * e.sweep;
        const double th1 = e.start + f.t1 * e.sweep;
        value = 0.5 * (e.center.u * e.radius * (std::sin(th1) - std::sin(th0))
                       - e.center.v * e.radius * (std::cos(th1) - std::cos(th0))
                       + e.radius * e.radius * (th1 - th0));
    }
    return reversed ? -value : value;
}

double angleBetween(const SketchPoint& p, const SketchPoint& a, const SketchPoint& b) {
    const double au = a.u - p.u, av = a.v - p.v, bu = b.u - p.u, bv = b.v - p.v;
    return std::atan2(crossUV(au, av, bu, bv), au * bu + av * bv);
}

// The angle fragment f subtends at p, walked forward or reversed. An arc is
// cut into pieces of at most a quarter turn; a piece whose circular segment
// (between its chord and itself) contains p subtends its chord angle plus a
// full turn in its own direction.
// Signed crossings of the ray from p towards +u by the span over [t0, t1]: +1
// where the curve passes upward. The class at the two ends is read off
// `edgePoint`, the evaluation the chord uses, so a ray through an end is judged
// the same way for the curve and for its chord.
int bezierRayCrossings(const SourceEdge& e, double t0, double t1, const SketchPoint& p) {
    double cu[4];
    double cv[4];
    bezierPower(e, cu, cv);
    const double g[4] = {cv[0] - p.v, cv[1], cv[2], cv[3]};
    std::vector<double> breaks{t0};
    polyCritical(g, 3, t0, t1, &breaks);
    breaks.push_back(t1);
    const size_t last = breaks.size() - 1;
    const auto above = [&](size_t k) {
        if (k == 0) return edgePoint(e, t0).v >= p.v;
        if (k == last) return edgePoint(e, t1).v >= p.v;
        return polyEval(g, 3, breaks[k]) >= 0.0;
    };
    int count = 0;
    for (size_t k = 0; k < last; ++k) {
        const bool from = above(k);
        const bool to = above(k + 1);
        if (from == to) continue;
        double lo = breaks[k];
        double hi = breaks[k + 1];
        for (int i = 0; i < 200; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (!(mid > lo && mid < hi)) break;
            if ((polyEval(g, 3, mid) >= 0.0) == from) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        if (edgePoint(e, 0.5 * (lo + hi)).u > p.u) count += to ? 1 : -1;
    }
    return count;
}

int chordRayCrossings(const SketchPoint& a, const SketchPoint& b, const SketchPoint& p) {
    const bool from = a.v >= p.v;
    const bool to = b.v >= p.v;
    if (from == to) return 0;
    const double t = (p.v - a.v) / (b.v - a.v);
    return a.u + (b.u - a.u) * t > p.u ? (to ? 1 : -1) : 0;
}

double fragmentWinding(const SourceEdge& e, const Fragment& f, bool reversed, const SketchPoint& p) {
    double total = 0.0;
    if (e.shape == EdgeShape::Bezier) {
        // The chord's angle, corrected by the whole turns the closed loop
        // (the piece, then its chord backwards) makes around p -- that loop's
        // winding is its signed ray crossings. Exact, with no tessellation.
        const SketchPoint a = edgePoint(e, f.t0);
        const SketchPoint b = edgePoint(e, f.t1);
        const int turns = bezierRayCrossings(e, f.t0, f.t1, p) - chordRayCrossings(a, b, p);
        total = angleBetween(p, a, b) + kTwoPi * turns;
        return reversed ? -total : total;
    }
    if (e.shape == EdgeShape::Segment) {
        total = angleBetween(p, edgePoint(e, f.t0), edgePoint(e, f.t1));
    } else {
        const double sweep = (f.t1 - f.t0) * e.sweep;
        const int pieces = std::max(1, static_cast<int>(std::ceil(std::fabs(sweep) / (kPi / 2.0))));
        const bool inside = dist(p, e.center) < e.radius;
        for (int k = 0; k < pieces; ++k) {
            const double ta = f.t0 + (f.t1 - f.t0) * k / pieces;
            const double tb = f.t0 + (f.t1 - f.t0) * (k + 1) / pieces;
            const double tha = e.start + ta * e.sweep;
            const double thb = e.start + tb * e.sweep;
            const SketchPoint a{e.center.u + e.radius * std::cos(tha),
                                e.center.v + e.radius * std::sin(tha)};
            const SketchPoint b{e.center.u + e.radius * std::cos(thb),
                                e.center.v + e.radius * std::sin(thb)};
            double piece = angleBetween(p, a, b);
            if (inside) {
                // The arc bulges to the right of a CCW chord (left of a CW one).
                const double side = crossUV(b.u - a.u, b.v - a.v, p.u - a.u, p.v - a.v);
                const bool ccw = (thb - tha) > 0.0;
                if ((ccw && side < 0.0) || (!ccw && side > 0.0)) {
                    piece += ccw ? kTwoPi : -kTwoPi;
                }
            }
            total += piece;
        }
    }
    return reversed ? -total : total;
}

double fragmentDistance(const SourceEdge& e, const Fragment& f, const SketchPoint& p) {
    if (e.shape == EdgeShape::Bezier) {
        double t = 0.0;
        return nearestOnBezier(e, p, f.t0, f.t1, &t);
    }
    if (e.shape == EdgeShape::Segment) {
        const SketchPoint a = edgePoint(e, f.t0), b = edgePoint(e, f.t1);
        const double ru = b.u - a.u, rv = b.v - a.v;
        const double len2 = ru * ru + rv * rv;
        double t = len2 > 0.0 ? ((p.u - a.u) * ru + (p.v - a.v) * rv) / len2 : 0.0;
        t = std::fmax(0.0, std::fmin(1.0, t));
        return dist(p, lerp(a, b, t));
    }
    // On the fragment's own angular span: the radial distance; else the ends.
    const double angle = std::atan2(p.v - e.center.v, p.u - e.center.u);
    const double th0 = e.start + f.t0 * e.sweep;
    const double span = std::fabs((f.t1 - f.t0) * e.sweep);
    const double delta = e.sweep > 0.0 ? wrapAngle(angle - th0) : wrapAngle(th0 - angle);
    if (delta <= span) return std::fabs(dist(p, e.center) - e.radius);
    return std::fmin(dist(p, edgePoint(e, f.t0)), dist(p, edgePoint(e, f.t1)));
}

// ---------------------------------------------------------------------------
// Half-edges
// ---------------------------------------------------------------------------

struct HalfEdge {
    uint32_t fragment = 0;
    bool reversed = false;
    uint32_t origin = 0;
    double angle = 0.0;      // outgoing tangent direction at the origin
    double curvature = 0.0;  // signed, relative to the walking direction
};

uint32_t twinOf(uint32_t h) { return h ^ 1u; }

}  // namespace

// ---------------------------------------------------------------------------
// Semantic orders
// ---------------------------------------------------------------------------

const char* arrangementStatusName(ArrangementStatus status) {
    switch (status) {
        case ArrangementStatus::Ok: return "Ok";
        case ArrangementStatus::InvalidSketch: return "InvalidSketch";
        case ArrangementStatus::UnsupportedCurve: return "UnsupportedCurve";
        case ArrangementStatus::AmbiguousOverlap: return "AmbiguousOverlap";
        case ArrangementStatus::DegenerateFace: return "DegenerateFace";
        case ArrangementStatus::CapExceeded: return "CapExceeded";
        case ArrangementStatus::InvalidSelection: return "InvalidSelection";
        case ArrangementStatus::PinchedSelection: return "PinchedSelection";
        case ArrangementStatus::SelfIntersectingCurve: return "SelfIntersectingCurve";
    }
    return "Unknown";
}

namespace {
template <typename T>
int cmp(const T& a, const T& b) {
    return a < b ? -1 : (b < a ? 1 : 0);
}
}  // namespace

int compareArrangementCut(const ArrangementCut& a, const ArrangementCut& b) {
    if (int c = cmp(static_cast<int>(a.kind), static_cast<int>(b.kind))) return c;
    if (int c = cmp(a.partnerEntityId, b.partnerEntityId)) return c;
    if (int c = cmp(a.partnerEdgeLocalIndex, b.partnerEdgeLocalIndex)) return c;
    return cmp(a.ordinal, b.ordinal);
}

int compareFragmentRef(const FragmentRef& a, const FragmentRef& b) {
    if (int c = cmp(a.sourceEntityId, b.sourceEntityId)) return c;
    if (int c = cmp(a.sourceEdgeLocalIndex, b.sourceEdgeLocalIndex)) return c;
    if (int c = compareArrangementCut(a.startCut, b.startCut)) return c;
    if (int c = compareArrangementCut(a.endCut, b.endCut)) return c;
    return cmp(static_cast<int>(a.reversed), static_cast<int>(b.reversed));
}

namespace {
int compareCycle(const FragmentCycle& a, const FragmentCycle& b) {
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (int c = compareFragmentRef(a[i], b[i])) return c;
    }
    return cmp(a.size(), b.size());
}

// Rotates a cycle to start at its smallest fragment. Every fragment appears
// at most once per cycle (bridges are pruned), so the rotation is unique.
void canonicalizeCycle(FragmentCycle* cycle) {
    if (cycle->empty()) return;
    size_t best = 0;
    for (size_t i = 1; i < cycle->size(); ++i) {
        if (compareFragmentRef((*cycle)[i], (*cycle)[best]) < 0) best = i;
    }
    std::rotate(cycle->begin(), cycle->begin() + static_cast<std::ptrdiff_t>(best), cycle->end());
}
}  // namespace

int comparePlanarFaceRef(const PlanarFaceRef& a, const PlanarFaceRef& b) {
    if (int c = compareCycle(a.outer, b.outer)) return c;
    const size_t n = std::min(a.holes.size(), b.holes.size());
    for (size_t i = 0; i < n; ++i) {
        if (int c = compareCycle(a.holes[i], b.holes[i])) return c;
    }
    return cmp(a.holes.size(), b.holes.size());
}

bool samePlanarFaceRef(const PlanarFaceRef& a, const PlanarFaceRef& b) {
    return comparePlanarFaceRef(a, b) == 0;
}

bool resolvePlanarFaceRef(const SketchArrangement& arrangement, const PlanarFaceRef& ref,
                          size_t* outIndex) {
    if (arrangement.status != ArrangementStatus::Ok) return false;
    for (size_t i = 0; i < arrangement.faces.size(); ++i) {
        if (samePlanarFaceRef(arrangement.faces[i].ref, ref)) {
            if (outIndex != nullptr) *outIndex = i;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Derivation
// ---------------------------------------------------------------------------

namespace {

SketchArrangement failed(ArrangementStatus status, const ArrangementStats& stats) {
    SketchArrangement out;
    out.status = status;
    out.stats = stats;
    return out;
}

// Cuts per edge, in the edge's own parameter order: contacts interior to the
// edge, each named by its partner and its ordinal among that partner's
// contacts, merged per node (several curves meeting at one point give ONE cut,
// named by the smallest of their refs).
std::vector<std::vector<Cut>> cutsPerEdge(const std::vector<SourceEdge>& edges,
                                          const std::vector<PairContact>& contacts,
                                          const std::vector<uint32_t>& contactNode) {
    struct Raw {
        double t;
        uint32_t node;
        uint32_t partner;
    };
    std::vector<std::vector<Raw>> raw(edges.size());
    for (size_t c = 0; c < contacts.size(); ++c) {
        for (int s = 0; s < 2; ++s) {
            const uint32_t edge = contacts[c].edge[s];
            if (contacts[c].contact.end[s] != 0) continue;  // at its own end: no cut
            raw[edge].push_back(Raw{contacts[c].contact.t[s], contactNode[c],
                                    contacts[c].edge[1 - s]});
        }
    }
    std::vector<std::vector<Cut>> out(edges.size());
    for (size_t e = 0; e < edges.size(); ++e) {
        std::vector<Raw>& list = raw[e];
        // Ordinals: per partner edge, in this edge's parameter order (ties by
        // node, which is itself semantic).
        std::sort(list.begin(), list.end(), [](const Raw& x, const Raw& y) {
            if (x.partner != y.partner) return x.partner < y.partner;
            if (x.t != y.t) return x.t < y.t;
            return x.node < y.node;
        });
        std::vector<Cut> cuts;
        for (size_t i = 0; i < list.size(); ++i) {
            uint32_t ordinal = 0;
            for (size_t j = i; j > 0 && list[j - 1].partner == list[i].partner; --j) ++ordinal;
            Cut cut;
            cut.t = list[i].t;
            cut.node = list[i].node;
            cut.ref.kind = ArrangementCutKind::Intersection;
            cut.ref.partnerEntityId = edges[list[i].partner].entity;
            cut.ref.partnerEdgeLocalIndex = edges[list[i].partner].local;
            cut.ref.ordinal = ordinal;
            // One cut per node on this edge: the smallest ref names it.
            bool merged = false;
            for (Cut& existing : cuts) {
                if (existing.node == cut.node) {
                    if (compareArrangementCut(cut.ref, existing.ref) < 0) {
                        existing.ref = cut.ref;
                        existing.t = cut.t;
                    }
                    merged = true;
                    break;
                }
            }
            if (!merged) cuts.push_back(cut);
        }
        std::sort(cuts.begin(), cuts.end(), [](const Cut& x, const Cut& y) {
            if (x.t != y.t) return x.t < y.t;
            return x.node < y.node;
        });
        out[e] = std::move(cuts);
    }
    return out;
}

}  // namespace

SketchArrangement deriveSketchArrangement(const CadSketch& sketch) {
    ArrangementStats stats;
    if (validateCadSketch(sketch) != CadStatus::Ok) {
        return failed(ArrangementStatus::InvalidSketch, stats);
    }
    std::vector<SourceEdge> edges;
    if (const ArrangementStatus why = collectSourceEdges(sketch, &edges);
        why != ArrangementStatus::Ok) {
        return failed(why, stats);
    }
    stats.sourceEdges = static_cast<uint32_t>(edges.size());
    // A span is one source edge: one that meets itself has no node to be
    // split at, so the cells it would make are refused rather than guessed.
    for (const SourceEdge& edge : edges) {
        if (!edge.bezier()) continue;
        if (const ArrangementStatus why = spanSelfIntersection(edge); why != ArrangementStatus::Ok) {
            return failed(why, stats);
        }
    }

    std::vector<PairContact> contacts;
    if (const ArrangementStatus why = collectContacts(edges, &contacts, &stats);
        why != ArrangementStatus::Ok) {
        return failed(why, stats);
    }

    // --- nodes --------------------------------------------------------------
    std::vector<NodePoint> points;
    for (uint32_t e = 0; e < edges.size(); ++e) {
        if (edges[e].closed()) continue;
        points.push_back(NodePoint{edges[e].a, {0u, e, 0u}});
        points.push_back(NodePoint{edges[e].b, {0u, e, 1u}});
    }
    const size_t firstContactPoint = points.size();
    for (uint32_t c = 0; c < contacts.size(); ++c) {
        points.push_back(NodePoint{contacts[c].contact.point, {1u, contacts[c].edge[0], c}});
    }
    // A circle with no cut still needs one node: its own parameter origin.
    std::vector<std::vector<Cut>> cuts;
    {
        std::vector<SketchPoint> provisional;
        const std::vector<uint32_t> nodeOf = clusterNodes(points, &provisional);
        std::vector<uint32_t> contactNode(contacts.size());
        for (size_t c = 0; c < contacts.size(); ++c) contactNode[c] = nodeOf[firstContactPoint + c];
        cuts = cutsPerEdge(edges, contacts, contactNode);
    }
    for (uint32_t e = 0; e < edges.size(); ++e) {
        if (edges[e].closed() && cuts[e].empty()) {
            points.push_back(NodePoint{edges[e].a, {2u, e, 0u}});
        }
    }
    std::vector<SketchPoint> nodes;
    const std::vector<uint32_t> nodeOf = clusterNodes(points, &nodes);
    {
        std::vector<uint32_t> contactNode(contacts.size());
        for (size_t c = 0; c < contacts.size(); ++c) contactNode[c] = nodeOf[firstContactPoint + c];
        cuts = cutsPerEdge(edges, contacts, contactNode);
    }
    stats.nodes = static_cast<uint32_t>(nodes.size());

    // --- fragments ------------------------------------------------------------
    std::vector<Fragment> fragments;
    {
        size_t pointIndex = 0;
        std::vector<uint32_t> startNode(edges.size()), endNode(edges.size()), originNode(edges.size());
        for (uint32_t e = 0; e < edges.size(); ++e) {
            if (edges[e].closed()) continue;
            startNode[e] = nodeOf[pointIndex++];
            endNode[e] = nodeOf[pointIndex++];
        }
        size_t originIndex = firstContactPoint + contacts.size();
        for (uint32_t e = 0; e < edges.size(); ++e) {
            if (edges[e].closed() && cuts[e].empty()) originNode[e] = nodeOf[originIndex++];
        }
        const ArrangementCut sourceStart{ArrangementCutKind::SourceStart, kNoSketchEntity, 0, 0};
        const ArrangementCut sourceEnd{ArrangementCutKind::SourceEnd, kNoSketchEntity, 0, 0};
        for (uint32_t e = 0; e < edges.size(); ++e) {
            const SourceEdge& edge = edges[e];
            std::vector<Cut> list;
            if (edge.closed()) {
                if (cuts[e].empty()) {
                    Fragment f;
                    f.edge = e;
                    f.t0 = 0.0;
                    f.t1 = 1.0;
                    f.node0 = f.node1 = originNode[e];
                    f.ref = FragmentRef{edge.entity, edge.local, sourceStart, sourceEnd, false};
                    fragments.push_back(f);
                    continue;
                }
                const std::vector<Cut>& c = cuts[e];
                for (size_t i = 0; i < c.size(); ++i) {
                    const Cut& from = c[i];
                    const Cut& to = c[(i + 1) % c.size()];
                    Fragment f;
                    f.edge = e;
                    f.t0 = from.t;
                    f.t1 = (i + 1 < c.size()) ? to.t : to.t + 1.0;
                    f.node0 = from.node;
                    f.node1 = to.node;
                    f.ref = FragmentRef{edge.entity, edge.local, from.ref, to.ref, false};
                    fragments.push_back(f);
                }
                continue;
            }
            list.push_back(Cut{0.0, startNode[e], sourceStart});
            for (const Cut& cut : cuts[e]) {
                if (cut.node == startNode[e] || cut.node == endNode[e]) continue;
                list.push_back(cut);
            }
            list.push_back(Cut{1.0, endNode[e], sourceEnd});
            for (size_t i = 0; i + 1 < list.size(); ++i) {
                if (list[i].node == list[i + 1].node) continue;  // zero length
                Fragment f;
                f.edge = e;
                f.t0 = list[i].t;
                f.t1 = list[i + 1].t;
                f.node0 = list[i].node;
                f.node1 = list[i + 1].node;
                f.ref = FragmentRef{edge.entity, edge.local, list[i].ref, list[i + 1].ref, false};
                fragments.push_back(f);
            }
        }
    }
    std::sort(fragments.begin(), fragments.end(), [](const Fragment& x, const Fragment& y) {
        return compareFragmentRef(x.ref, y.ref) < 0;
    });
    stats.fragments = static_cast<uint32_t>(fragments.size());

    // --- half-edges: 2k forward, 2k+1 reversed ---------------------------------
    std::vector<HalfEdge> halves(fragments.size() * 2);
    for (uint32_t f = 0; f < fragments.size(); ++f) {
        const SourceEdge& e = edges[fragments[f].edge];
        for (int r = 0; r < 2; ++r) {
            HalfEdge& h = halves[2 * f + r];
            h.fragment = f;
            h.reversed = r == 1;
            h.origin = r == 0 ? fragments[f].node0 : fragments[f].node1;
            double du = 0.0, dv = 0.0;
            // The angle form takes a circle's wrap parameter (> 1) directly.
            edgeTangent(e, r == 0 ? fragments[f].t0 : fragments[f].t1, &du, &dv);
            if (r == 1) {
                du = -du;
                dv = -dv;
            }
            h.angle = std::atan2(dv, du);
            h.curvature = r == 0 ? edgeCurvature(e, fragments[f].t0)
                                 : -edgeCurvature(e, fragments[f].t1);
        }
    }

    // Prune dangling fragments (a node of degree 1), then bridges (both sides
    // one face), until neither remains: what is left bounds faces.
    std::vector<std::vector<uint32_t>> cycles;
    std::vector<uint32_t> cycleOf;
    for (size_t round = 0; round <= fragments.size(); ++round) {
        bool pruned = true;
        while (pruned) {
            pruned = false;
            std::vector<uint32_t> degree(nodes.size(), 0);
            for (const Fragment& f : fragments) {
                if (!f.live) continue;
                ++degree[f.node0];
                ++degree[f.node1];
            }
            for (Fragment& f : fragments) {
                if (f.live && (degree[f.node0] == 1 || degree[f.node1] == 1)) {
                    f.live = false;
                    pruned = true;
                }
            }
        }
        // Rotation system: outgoing half-edges per node, counter-clockwise by
        // tangent angle; a tie (two curves leaving tangentially) by signed
        // curvature, then by semantic identity.
        std::vector<std::vector<uint32_t>> around(nodes.size());
        for (uint32_t h = 0; h < halves.size(); ++h) {
            if (fragments[halves[h].fragment].live) around[halves[h].origin].push_back(h);
        }
        std::vector<uint32_t> position(halves.size(), 0);
        for (std::vector<uint32_t>& list : around) {
            // Angles are compared QUANTIZED (1e-9 rad), so two tangential
            // departures computed by different formulas tie and fall through
            // to curvature, and the comparator stays a strict weak order.
            std::sort(list.begin(), list.end(), [&](uint32_t x, uint32_t y) {
                const HalfEdge& a = halves[x];
                const HalfEdge& b = halves[y];
                const long long qa = std::llround(a.angle * 1.0e9);
                const long long qb = std::llround(b.angle * 1.0e9);
                if (qa != qb) return qa < qb;
                if (a.curvature != b.curvature) return a.curvature < b.curvature;
                if (int c = compareFragmentRef(fragments[a.fragment].ref, fragments[b.fragment].ref)) {
                    return c < 0;
                }
                return a.reversed < b.reversed;
            });
            for (uint32_t k = 0; k < list.size(); ++k) position[list[k]] = k;
        }
        // next(h) = the half-edge leaving h's destination immediately
        // clockwise of twin(h): the face on the LEFT of every half-edge.
        const auto next = [&](uint32_t h) {
            const uint32_t t = twinOf(h);
            const std::vector<uint32_t>& list = around[halves[t].origin];
            const uint32_t k = position[t];
            return list[(k + list.size() - 1) % list.size()];
        };
        cycles.clear();
        cycleOf.assign(halves.size(), UINT32_MAX);
        for (uint32_t start = 0; start < halves.size(); ++start) {
            if (!fragments[halves[start].fragment].live || cycleOf[start] != UINT32_MAX) continue;
            std::vector<uint32_t> cycle;
            uint32_t h = start;
            for (size_t guard = 0; guard <= halves.size(); ++guard) {
                cycleOf[h] = static_cast<uint32_t>(cycles.size());
                cycle.push_back(h);
                h = next(h);
                if (h == start) break;
            }
            cycles.push_back(std::move(cycle));
        }
        bool bridge = false;
        for (uint32_t f = 0; f < fragments.size(); ++f) {
            if (fragments[f].live && cycleOf[2 * f] == cycleOf[2 * f + 1]) {
                fragments[f].live = false;
                bridge = true;
            }
        }
        if (!bridge) break;
    }
    for (Fragment& f : fragments) {
        if (!f.live) ++stats.prunedFragments;
    }

    // --- faces ------------------------------------------------------------------
    // A cycle's signed area: positive -> the outer boundary of a bounded face
    // (walked counter-clockwise); negative -> the outer boundary of a
    // connected component seen from outside, i.e. a hole of whatever face
    // contains that component, or of the unbounded exterior.
    std::vector<double> area(cycles.size(), 0.0);
    for (size_t c = 0; c < cycles.size(); ++c) {
        for (uint32_t h : cycles[c]) {
            area[c] += fragmentArea(edges[fragments[halves[h].fragment].edge],
                                    fragments[halves[h].fragment], halves[h].reversed);
        }
        if (std::fabs(area[c]) < kMinProfileAreaSquareMeters) {
            return failed(ArrangementStatus::DegenerateFace, stats);
        }
    }
    // Components, so a hole is never tested against a cycle of its own.
    std::vector<uint32_t> nodeParent(nodes.size());
    std::iota(nodeParent.begin(), nodeParent.end(), 0u);
    for (const Fragment& f : fragments) {
        if (!f.live) continue;
        const uint32_t a = findRoot(&nodeParent, f.node0);
        const uint32_t b = findRoot(&nodeParent, f.node1);
        if (a != b) nodeParent[std::max(a, b)] = std::min(a, b);
    }
    const auto componentOf = [&](size_t c) {
        return findRoot(&nodeParent, halves[cycles[c][0]].origin);
    };
    const auto windingAt = [&](size_t c, const SketchPoint& p) {
        double total = 0.0;
        for (uint32_t h : cycles[c]) {
            total += fragmentWinding(edges[fragments[halves[h].fragment].edge],
                                     fragments[halves[h].fragment], halves[h].reversed, p);
        }
        return static_cast<int>(std::lround(total / kTwoPi));
    };
    const auto distanceTo = [&](size_t c, const SketchPoint& p) {
        double best = INFINITY;
        for (uint32_t h : cycles[c]) {
            const Fragment& f = fragments[halves[h].fragment];
            best = std::fmin(best, fragmentDistance(edges[f.edge], f, p));
        }
        return best;
    };
    std::vector<int64_t> holeOwner(cycles.size(), -1);
    for (size_t hc = 0; hc < cycles.size(); ++hc) {
        if (area[hc] > 0.0) continue;
        int64_t owner = -1;
        for (size_t fc = 0; fc < cycles.size(); ++fc) {
            if (area[fc] <= 0.0 || componentOf(fc) == componentOf(hc)) continue;
            // A sample on the hole's boundary clear of this cycle's boundary
            // (a tangent touch makes no node, so a sample may sit on one).
            // Golden-ratio fractions: never all at a symmetric figure's
            // tangency points, as quarter and half fractions of a single
            // inscribed circle all are.
            static const double kSamples[] = {0.3819660112501051, 0.6180339887498949,
                                              0.1458980337503155, 0.8541019662496845,
                                              0.2360679774997897};
            bool decided = false;
            bool inside = false;
            for (uint32_t h : cycles[hc]) {
                const Fragment& f = fragments[halves[h].fragment];
                for (double s : kSamples) {
                    const double t = f.t0 + (f.t1 - f.t0) * s;
                    const SketchPoint p = edgePoint(edges[f.edge],
                                                    edges[f.edge].closed() ? t - std::floor(t) : t);
                    if (distanceTo(fc, p) <= kTol) continue;
                    inside = windingAt(fc, p) != 0;
                    decided = true;
                    break;
                }
                if (decided) break;
            }
            if (!decided) {
                // Every sample sat on this cycle's boundary: nothing honest to
                // say about containment, so nothing is guessed.
                return failed(ArrangementStatus::DegenerateFace, stats);
            }
            if (inside && (owner < 0 || area[fc] < area[static_cast<size_t>(owner)])) {
                owner = static_cast<int64_t>(fc);
            }
        }
        holeOwner[hc] = owner;
    }
    const auto refCycle = [&](size_t c) {
        FragmentCycle out;
        for (uint32_t h : cycles[c]) {
            FragmentRef ref = fragments[halves[h].fragment].ref;
            ref.reversed = halves[h].reversed;
            out.push_back(ref);
        }
        canonicalizeCycle(&out);
        return out;
    };
    SketchArrangement out;
    for (size_t fc = 0; fc < cycles.size(); ++fc) {
        if (area[fc] <= 0.0) continue;
        AtomicPlanarFace face;
        face.ref.outer = refCycle(fc);
        face.area = area[fc];
        for (size_t hc = 0; hc < cycles.size(); ++hc) {
            if (holeOwner[hc] == static_cast<int64_t>(fc)) {
                face.ref.holes.push_back(refCycle(hc));
                face.area += area[hc];
            }
        }
        std::sort(face.ref.holes.begin(), face.ref.holes.end(),
                  [](const FragmentCycle& x, const FragmentCycle& y) {
                      return compareCycle(x, y) < 0;
                  });
        out.faces.push_back(std::move(face));
    }
    std::sort(out.faces.begin(), out.faces.end(),
              [](const AtomicPlanarFace& x, const AtomicPlanarFace& y) {
                  return comparePlanarFaceRef(x.ref, y.ref) < 0;
              });
    out.nodes = std::move(nodes);
    for (const Fragment& f : fragments) {
        ArrangementFragment af;
        af.ref = f.ref;
        af.startNode = f.node0;
        af.endNode = f.node1;
        af.curved = edges[f.edge].curved();
        af.boundsFace = f.live;
        const SourceEdge& e = edges[f.edge];
        af.points.push_back(out.nodes[f.node0]);
        if (e.bezier()) {
            // On the span's own parameter, at the density the profile
            // tessellation gives a whole span: a whole span reproduces its
            // interior points exactly. The ends are the NODES.
            const double wanted =
                    std::ceil(static_cast<double>(kSplineSegmentsPerSpan) * (f.t1 - f.t0) - 1.0e-9);
            const uint32_t segments = static_cast<uint32_t>(std::fmax(2.0, wanted));
            for (uint32_t i = 1; i < segments; ++i) {
                const double t = f.t0 + (f.t1 - f.t0) * (static_cast<double>(i) / segments);
                af.points.push_back(sketchBezierPoint(spanOf(e), t));
            }
        } else if (e.round()) {
            // Clipped to the fragment's own sweep, at an authored arc's
            // density; the two ends are the NODES, never re-evaluated.
            const double sweep = (f.t1 - f.t0) * e.sweep;
            const uint32_t segments = sketchArcSegmentCount(sweep);
            for (uint32_t i = 1; i < segments; ++i) {
                const double t = f.t0 + (f.t1 - f.t0) * (static_cast<double>(i) / segments);
                const double angle = e.start + t * e.sweep;
                af.points.push_back(SketchPoint{e.center.u + e.radius * std::cos(angle),
                                                e.center.v + e.radius * std::sin(angle)});
            }
        }
        af.points.push_back(out.nodes[f.node1]);
        out.fragments.push_back(af);
    }
    out.stats = stats;
    out.status = ArrangementStatus::Ok;
    return out;
}

// ---------------------------------------------------------------------------
// The union of chosen faces (`CAD-V6-S2`)
// ---------------------------------------------------------------------------

namespace {

// Half-edge id over the PUBLIC fragment list: 2k walks fragment k forward,
// 2k+1 against it.
uint32_t halfOf(uint32_t fragment, bool reversed) { return fragment * 2u + (reversed ? 1u : 0u); }

bool fragmentIndexOf(const SketchArrangement& arrangement, const FragmentRef& ref, uint32_t* out) {
    FragmentRef key = ref;
    key.reversed = false;
    const auto it = std::lower_bound(
            arrangement.fragments.begin(), arrangement.fragments.end(), key,
            [](const ArrangementFragment& f, const FragmentRef& k) {
                return compareFragmentRef(f.ref, k) < 0;
            });
    if (it == arrangement.fragments.end() || compareFragmentRef(it->ref, key) != 0) {
        return false;
    }
    *out = static_cast<uint32_t>(it - arrangement.fragments.begin());
    return true;
}

double polygonArea(const std::vector<SketchPoint>& polygon) {
    double twice = 0.0;
    const size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[(i + 1) % n];
        twice += crossUV(a.u, a.v, b.u, b.v);
    }
    return 0.5 * twice;
}

bool strictlyInside(const SketchPoint& p, const std::vector<SketchPoint>& polygon) {
    bool inside = false;
    const size_t n = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[j];
        if ((a.v > p.v) != (b.v > p.v)) {
            const double u = (b.u - a.u) * (p.v - a.v) / (b.v - a.v) + a.u;
            if (p.u < u) inside = !inside;
        }
    }
    return inside;
}

double distanceToPolygon(const SketchPoint& p, const std::vector<SketchPoint>& polygon) {
    double best = INFINITY;
    const size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[(i + 1) % n];
        const double ru = b.u - a.u, rv = b.v - a.v;
        const double len2 = ru * ru + rv * rv;
        double t = len2 > 0.0 ? ((p.u - a.u) * ru + (p.v - a.v) * rv) / len2 : 0.0;
        t = std::fmax(0.0, std::fmin(1.0, t));
        best = std::fmin(best, dist(p, lerp(a, b, t)));
    }
    return best;
}

// The union of ONE edge-connected group of chosen faces (see
// `partitionSelectedPlanarFacesBySharedBoundary`), appended to `out` unsorted.
// Its node bookkeeping is the GROUP's own: a loop of this group passing a node
// another loop of this group already passed is a genuine pinch of this group's
// boundary and is refused, while a node another GROUP passes is none of its
// business -- that is a point contact between two separate components.
ArrangementStatus mergeEdgeConnectedFaces(const SketchArrangement& arrangement,
                                          const std::vector<size_t>& faceIndices,
                                          std::vector<PlanarProfileComponent>* out) {
    // Every chosen face's boundary cycles as half-edge ids, and where each
    // half-edge sits: (cycle, position). A directed half-edge bounds exactly
    // one face, so a repeat is a repeated face.
    std::vector<std::vector<uint32_t>> cycles;
    const uint32_t halfCount = static_cast<uint32_t>(arrangement.fragments.size() * 2u);
    std::vector<int64_t> cycleOf(halfCount, -1);
    std::vector<uint32_t> positionOf(halfCount, 0);
    std::vector<size_t> seen;
    for (size_t index : faceIndices) {
        if (index >= arrangement.faces.size()
            || std::find(seen.begin(), seen.end(), index) != seen.end()) {
            return ArrangementStatus::InvalidSelection;
        }
        seen.push_back(index);
        const PlanarFaceRef& ref = arrangement.faces[index].ref;
        std::vector<const FragmentCycle*> boundary{&ref.outer};
        for (const FragmentCycle& hole : ref.holes) boundary.push_back(&hole);
        for (const FragmentCycle* cycle : boundary) {
            std::vector<uint32_t> halves;
            for (const FragmentRef& fragment : *cycle) {
                uint32_t k = 0;
                if (!fragmentIndexOf(arrangement, fragment, &k)) {
                    return ArrangementStatus::InvalidSelection;
                }
                const uint32_t h = halfOf(k, fragment.reversed);
                if (cycleOf[h] >= 0) {
                    return ArrangementStatus::InvalidSelection;
                }
                cycleOf[h] = static_cast<int64_t>(cycles.size());
                positionOf[h] = static_cast<uint32_t>(halves.size());
                halves.push_back(h);
            }
            cycles.push_back(std::move(halves));
        }
    }
    const auto chosen = [&](uint32_t h) { return cycleOf[h] >= 0; };
    // A half-edge bounds the union exactly when its twin does not.
    const auto remains = [&](uint32_t h) { return chosen(h) && !chosen(h ^ 1u); };
    const auto successor = [&](uint32_t h) {
        const std::vector<uint32_t>& cycle = cycles[static_cast<size_t>(cycleOf[h])];
        return cycle[(positionOf[h] + 1u) % cycle.size()];
    };
    // From h, its own face's successor; past a cancelled one, to the next
    // chosen face around the same node. Bounded by the half-edge count.
    const auto nextOnUnion = [&](uint32_t h, uint32_t* outNext) {
        uint32_t s = successor(h);
        for (uint32_t guard = 0; guard <= halfCount; ++guard) {
            if (remains(s)) {
                *outNext = s;
                return true;
            }
            s = successor(s ^ 1u);
        }
        return false;
    };
    const auto originNode = [&](uint32_t h) {
        const ArrangementFragment& f = arrangement.fragments[h >> 1];
        return (h & 1u) == 0u ? f.startNode : f.endNode;
    };

    std::vector<PlanarProfileLoop> outers;
    std::vector<PlanarProfileLoop> holes;
    std::vector<uint8_t> walked(halfCount, 0);
    // Every node a union loop of THIS group passes, across all of the group's
    // loops: within one edge-connected group, a loop revisiting a node -- or a
    // hole touching its own outer at one -- pinches the boundary. Two GROUPS
    // meeting at a node never reach this vector together.
    std::vector<uint8_t> nodeUsed(arrangement.nodes.size(), 0);
    for (uint32_t start = 0; start < halfCount; ++start) {
        if (!remains(start) || walked[start] != 0u) continue;
        std::vector<uint32_t> loop;
        std::vector<uint32_t> nodesSeen;
        uint32_t h = start;
        for (uint32_t guard = 0; guard <= halfCount; ++guard) {
            walked[h] = 1u;
            const uint32_t node = originNode(h);
            if (node >= nodeUsed.size() || nodeUsed[node] != 0u) {
                return ArrangementStatus::PinchedSelection;
            }
            nodeUsed[node] = 1u;
            nodesSeen.push_back(node);
            loop.push_back(h);
            uint32_t next = 0;
            if (!nextOnUnion(h, &next)) {
                return ArrangementStatus::InvalidSelection;
            }
            h = next;
            if (h == start) break;
        }
        if (h != start) {
            return ArrangementStatus::InvalidSelection;
        }
        // Canonical rotation: start at the smallest fragment ref.
        size_t best = 0;
        const auto refOf = [&](uint32_t half) {
            FragmentRef ref = arrangement.fragments[half >> 1].ref;
            ref.reversed = (half & 1u) != 0u;
            return ref;
        };
        for (size_t i = 1; i < loop.size(); ++i) {
            if (compareFragmentRef(refOf(loop[i]), refOf(loop[best])) < 0) best = i;
        }
        std::rotate(loop.begin(), loop.begin() + static_cast<std::ptrdiff_t>(best), loop.end());
        PlanarProfileLoop result;
        for (uint32_t k = 0; k < loop.size(); ++k) {
            const uint32_t half = loop[k];
            const ArrangementFragment& f = arrangement.fragments[half >> 1];
            result.fragments.push_back(refOf(half));
            result.fragmentCurved.push_back(f.curved ? 1u : 0u);
            std::vector<SketchPoint> points = f.points;
            if ((half & 1u) != 0u) std::reverse(points.begin(), points.end());
            // Every point but the last: the next fragment starts there.
            for (size_t i = 0; i + 1 < points.size(); ++i) {
                result.polygon.push_back(points[i]);
                result.edgeFragment.push_back(k);
            }
        }
        const double signedArea = polygonArea(result.polygon);
        if (!(std::fabs(signedArea) >= kMinProfileAreaSquareMeters)) {
            return ArrangementStatus::DegenerateFace;
        }
        if (signedArea < 0.0) {
            // A hole, walked clockwise: stored counter-clockwise, every edge
            // keeping the fragment it lies on.
            const size_t n = result.polygon.size();
            std::vector<SketchPoint> polygon(n);
            std::vector<uint32_t> edges(n);
            for (size_t i = 0; i < n; ++i) {
                polygon[i] = result.polygon[(n - i) % n];
                // Reversed edge i runs polygon'[i] -> polygon'[i+1], i.e. the
                // original edge (n - 1 - i).
                edges[i] = result.edgeFragment[n - 1 - i];
            }
            result.polygon = std::move(polygon);
            result.edgeFragment = std::move(edges);
            result.area = -signedArea;
            holes.push_back(std::move(result));
        } else {
            result.area = signedArea;
            outers.push_back(std::move(result));
        }
    }
    if (outers.empty()) {
        return ArrangementStatus::InvalidSelection;
    }
    std::vector<PlanarProfileComponent> components(outers.size());
    for (size_t i = 0; i < outers.size(); ++i) {
        components[i].outer = std::move(outers[i]);
    }
    for (PlanarProfileLoop& hole : holes) {
        // A point on the hole's boundary clear of every outer boundary decides
        // which outer contains it; the smallest such outer owns it.
        int64_t owner = -1;
        for (size_t c = 0; c < components.size(); ++c) {
            const std::vector<SketchPoint>& outer = components[c].outer.polygon;
            bool decided = false;
            bool inside = false;
            const size_t n = hole.polygon.size();
            for (size_t k = 0; k < n && !decided; ++k) {
                const SketchPoint mid = lerp(hole.polygon[k], hole.polygon[(k + 1) % n], 0.5);
                if (distanceToPolygon(mid, outer) <= kTol) continue;
                inside = strictlyInside(mid, outer);
                decided = true;
            }
            if (decided && inside
                && (owner < 0 || components[c].outer.area
                                         < components[static_cast<size_t>(owner)].outer.area)) {
                owner = static_cast<int64_t>(c);
            }
        }
        if (owner < 0) {
            return ArrangementStatus::DegenerateFace;
        }
        components[static_cast<size_t>(owner)].holes.push_back(std::move(hole));
    }
    for (PlanarProfileComponent& component : components) {
        std::sort(component.holes.begin(), component.holes.end(),
                  [](const PlanarProfileLoop& x, const PlanarProfileLoop& y) {
                      return compareCycle(x.fragments, y.fragments) < 0;
                  });
        component.area = component.outer.area;
        for (const PlanarProfileLoop& hole : component.holes) component.area -= hole.area;
    }
    for (PlanarProfileComponent& component : components) out->push_back(std::move(component));
    return ArrangementStatus::Ok;
}

}  // namespace

ArrangementStatus partitionSelectedPlanarFacesBySharedBoundary(
        const SketchArrangement& arrangement, const std::vector<size_t>& faceIndices,
        std::vector<std::vector<size_t>>* outGroups) {
    if (outGroups == nullptr || arrangement.status != ArrangementStatus::Ok
        || faceIndices.empty()) {
        return arrangement.status != ArrangementStatus::Ok ? arrangement.status
                                                           : ArrangementStatus::InvalidSelection;
    }
    // Which chosen face (by its slot in `faceIndices`) owns each directed
    // half-edge. A directed half-edge bounds exactly one face, so a repeat is a
    // repeated face.
    const uint32_t halfCount = static_cast<uint32_t>(arrangement.fragments.size() * 2u);
    std::vector<int64_t> ownerOf(halfCount, -1);
    for (size_t slot = 0; slot < faceIndices.size(); ++slot) {
        const size_t index = faceIndices[slot];
        if (index >= arrangement.faces.size()
            || std::find(faceIndices.begin(), faceIndices.begin() + static_cast<std::ptrdiff_t>(slot),
                         index) != faceIndices.begin() + static_cast<std::ptrdiff_t>(slot)) {
            return ArrangementStatus::InvalidSelection;
        }
        const PlanarFaceRef& ref = arrangement.faces[index].ref;
        std::vector<const FragmentCycle*> boundary{&ref.outer};
        for (const FragmentCycle& hole : ref.holes) boundary.push_back(&hole);
        for (const FragmentCycle* cycle : boundary) {
            for (const FragmentRef& fragment : *cycle) {
                uint32_t k = 0;
                if (!fragmentIndexOf(arrangement, fragment, &k)) {
                    return ArrangementStatus::InvalidSelection;
                }
                const uint32_t h = halfOf(k, fragment.reversed);
                if (ownerOf[h] >= 0) {
                    return ArrangementStatus::InvalidSelection;
                }
                ownerOf[h] = static_cast<int64_t>(slot);
            }
        }
    }
    // Union-find over the slots. Two chosen faces join ONLY through a fragment
    // both of them bound -- one walks it each way. A shared node is not a
    // shared fragment, so faces meeting only at a point stay apart.
    std::vector<size_t> parent(faceIndices.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
    const auto root = [&](size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    for (uint32_t k = 0; k < halfCount / 2u; ++k) {
        const int64_t a = ownerOf[halfOf(k, false)];
        const int64_t b = ownerOf[halfOf(k, true)];
        if (a < 0 || b < 0) continue;
        const size_t ra = root(static_cast<size_t>(a));
        const size_t rb = root(static_cast<size_t>(b));
        if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
    }
    // Deterministic: each group's face indices ascending, groups ordered by
    // their smallest face index.
    std::vector<std::vector<size_t>> groups;
    std::vector<int64_t> groupOfRoot(faceIndices.size(), -1);
    std::vector<size_t> sortedSlots(faceIndices.size());
    for (size_t i = 0; i < sortedSlots.size(); ++i) sortedSlots[i] = i;
    std::sort(sortedSlots.begin(), sortedSlots.end(),
              [&](size_t x, size_t y) { return faceIndices[x] < faceIndices[y]; });
    for (size_t slot : sortedSlots) {
        const size_t r = root(slot);
        if (groupOfRoot[r] < 0) {
            groupOfRoot[r] = static_cast<int64_t>(groups.size());
            groups.emplace_back();
        }
        groups[static_cast<size_t>(groupOfRoot[r])].push_back(faceIndices[slot]);
    }
    *outGroups = std::move(groups);
    return ArrangementStatus::Ok;
}

ArrangementStatus mergePlanarFaces(const SketchArrangement& arrangement,
                                   const std::vector<size_t>& faceIndices,
                                   std::vector<PlanarProfileComponent>* out) {
    if (out == nullptr || arrangement.status != ArrangementStatus::Ok || faceIndices.empty()) {
        return arrangement.status != ArrangementStatus::Ok ? arrangement.status
                                                           : ArrangementStatus::InvalidSelection;
    }
    // Edge-connected groups first (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`), each
    // merged on its own: shared fragments cancel inside a group exactly as
    // before, a group's own pinch is still refused, and two groups that touch
    // at a point are simply two components -- each prism gets its own vertex
    // rings, so the solid stays closed and oriented with no shared vertex.
    std::vector<std::vector<size_t>> groups;
    const ArrangementStatus partitioned =
            partitionSelectedPlanarFacesBySharedBoundary(arrangement, faceIndices, &groups);
    if (partitioned != ArrangementStatus::Ok) {
        return partitioned;
    }
    std::vector<PlanarProfileComponent> components;
    for (const std::vector<size_t>& group : groups) {
        const ArrangementStatus why = mergeEdgeConnectedFaces(arrangement, group, &components);
        if (why != ArrangementStatus::Ok) {
            return why;
        }
    }
    // The canonical order (by the outer cycle) is independent of how the
    // components were found, so a selection that merged before derives
    // bit-identically now.
    std::sort(components.begin(), components.end(),
              [](const PlanarProfileComponent& x, const PlanarProfileComponent& y) {
                  return compareCycle(x.outer.fragments, y.outer.fragments) < 0;
              });
    *out = std::move(components);
    return ArrangementStatus::Ok;
}

}  // namespace forgeshape
