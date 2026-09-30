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
// through its authored end over [0, 1], angle = start + t * sweep. Endpoints
// are the AUTHORED points, never evaluated ones.

enum class EdgeShape : uint8_t { Segment, Circle, Arc };

struct SourceEdge {
    SketchEntityId entity = kNoSketchEntity;
    uint32_t local = 0;
    EdgeShape shape = EdgeShape::Segment;
    SketchPoint a{};  // segment start / authored arc start
    SketchPoint b{};  // segment end / authored arc end
    SketchPoint center{};
    double radius = 0.0;
    double start = 0.0;  // angle at t = 0
    double sweep = 0.0;  // signed; 2*pi for a circle
    double minU = 0.0, minV = 0.0, maxU = 0.0, maxV = 0.0;

    bool closed() const { return shape == EdgeShape::Circle; }
    bool round() const { return shape != EdgeShape::Segment; }
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

SketchPoint edgePoint(const SourceEdge& e, double t) {
    if (e.shape == EdgeShape::Segment) {
        if (t <= 0.0) return e.a;
        if (t >= 1.0) return e.b;
        return lerp(e.a, e.b, t);
    }
    if (e.shape == EdgeShape::Arc) {
        if (t <= 0.0) return e.a;
        if (t >= 1.0) return e.b;
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
    const double angle = e.start + t * e.sweep;
    const double s = e.sweep > 0.0 ? 1.0 : -1.0;
    *du = -std::sin(angle) * s;
    *dv = std::cos(angle) * s;
}

// Signed curvature along increasing t: + turning left.
double edgeCurvature(const SourceEdge& e) {
    if (e.shape == EdgeShape::Segment) return 0.0;
    return (e.sweep > 0.0 ? 1.0 : -1.0) / e.radius;
}

void setBounds(SourceEdge* e) {
    if (e->shape == EdgeShape::Segment) {
        e->minU = std::fmin(e->a.u, e->b.u);
        e->maxU = std::fmax(e->a.u, e->b.u);
        e->minV = std::fmin(e->a.v, e->b.v);
        e->maxV = std::fmax(e->a.v, e->b.v);
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
        if (entity.kind() == SketchEntityKind::Spline) {
            return ArrangementStatus::UnsupportedCurve;
        }
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

// Distance from p to the edge, and the parameter of the nearest point.
double distanceToEdge(const SourceEdge& e, const SketchPoint& p, double* outT) {
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
            if (!a.round() && !b.round()) {
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
double fragmentWinding(const SourceEdge& e, const Fragment& f, bool reversed, const SketchPoint& p) {
    double total = 0.0;
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
            h.curvature = r == 0 ? edgeCurvature(e) : -edgeCurvature(e);
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
        af.curved = edges[f.edge].round();
        af.boundsFace = f.live;
        out.fragments.push_back(af);
    }
    out.stats = stats;
    out.status = ArrangementStatus::Ok;
    return out;
}

}  // namespace forgeshape
