#include "forgeshape_sketch_drafting.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_cad_body.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_dimension.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

double distance(const SketchPoint& a, const SketchPoint& b) {
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return std::sqrt(du * du + dv * dv);
}

double segmentDistance(const SketchPoint& p, const SketchPoint& a, const SketchPoint& b) {
    const double du = b.u - a.u;
    const double dv = b.v - a.v;
    const double len2 = du * du + dv * dv;
    double t = 0.0;
    if (len2 > 0.0) {
        t = ((p.u - a.u) * du + (p.v - a.v) * dv) / len2;
        t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    }
    return distance(p, SketchPoint{a.u + t * du, a.v + t * dv});
}

double polylineDistance(const SketchPoint& p, const std::vector<SketchPoint>& points) {
    double d = INFINITY;
    for (size_t i = 1; i < points.size(); ++i) {
        d = std::fmin(d, segmentDistance(p, points[i - 1], points[i]));
    }
    if (points.size() == 1) d = distance(p, points[0]);
    return d;
}

SketchPoint onCircle(const SketchPoint& c, double r, double angle) {
    return SketchPoint{c.u + r * std::cos(angle), c.v + r * std::sin(angle)};
}

double angleOf(const SketchPoint& c, const SketchPoint& p) {
    return std::atan2(p.v - c.v, p.u - c.u);
}

// The angle travelled from `from` to `to` going in `sign`'s sense, in [0, 2pi).
double travel(double from, double to, double sign) {
    double d = sign >= 0.0 ? to - from : from - to;
    d = std::fmod(d, kTwoPi);
    if (d < 0.0) d += kTwoPi;
    return d;
}

// An arc from P to Q on circle (c, r) going in `sign`'s sense, through its
// angular middle. P and Q are kept EXACTLY; only the through-point is derived.
SketchArc arcBetween(const SketchPoint& c, double r, const SketchPoint& p, const SketchPoint& q,
                     double sign) {
    const double a0 = angleOf(c, p);
    const double span = travel(a0, angleOf(c, q), sign);
    SketchArc arc;
    arc.start = p;
    arc.end = q;
    arc.mid = onCircle(c, r, a0 + sign * span * 0.5);
    return arc;
}

bool sameIds(const std::vector<SketchEntityId>& ids, SketchEntityId id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}  // namespace

double sketchEntityDistance(const SketchEntity& entity, const SketchPoint& point) {
    if (const SketchLine* line = entity.line()) {
        return segmentDistance(point, line->start, line->end);
    }
    if (const SketchPolyline* polyline = entity.polyline()) {
        double d = INFINITY;
        const size_t n = polyline->vertices.size();
        for (size_t i = 0; i + 1 < n; ++i) {
            d = std::fmin(d, segmentDistance(point, polyline->vertices[i], polyline->vertices[i + 1]));
        }
        if (polyline->closed && n >= 3) {
            d = std::fmin(d, segmentDistance(point, polyline->vertices[n - 1], polyline->vertices[0]));
        }
        return d;
    }
    if (const SketchRectangle* rectangle = entity.rectangle()) {
        const std::vector<SketchPoint> corners = rectangleProfilePolygon(*rectangle);
        double d = INFINITY;
        for (size_t i = 0; i < 4; ++i) {
            d = std::fmin(d, segmentDistance(point, corners[i], corners[(i + 1) % 4]));
        }
        return d;
    }
    if (const SketchCircle* circle = entity.circle()) {
        return std::fabs(distance(point, circle->center) - circle->radius);
    }
    // A curve against its DERIVED polyline: what the user aims at is the
    // stroke they can see, and the stroke is that polyline.
    std::vector<SketchPoint> points;
    if (tessellateSketchCurve(entity, &points) == CadStatus::Ok && !points.empty()) {
        return polylineDistance(point, points);
    }
    return INFINITY;
}

SketchEntityId hitSketchEntity(const CadSketch& sketch, const SketchPoint& point, double tolerance) {
    SketchEntityId best = kNoSketchEntity;
    double bestDistance = tolerance;
    for (const SketchEntity& entity : sketch.entities) {
        const double d = sketchEntityDistance(entity, point);
        // Ties keep the earlier entity: deterministic, and the rule scene
        // picking uses.
        if (d < bestDistance) {
            bestDistance = d;
            best = entity.id();
        }
    }
    return best;
}

bool nearestSketchStraightEdge(const CadSketch& sketch, SketchEntityId entityId,
                               const SketchPoint& point, CadSketchEdgeRef* out) {
    const SketchEntity* entity = findSketchEntity(sketch, entityId);
    if (entity == nullptr) {
        return false;
    }
    bool found = false;
    double best = INFINITY;
    for (const SketchStraightEdge& edge : sketchEntityStraightEdges(*entity)) {
        const double d = segmentDistance(point, edge.start, edge.end);
        if (d < best) {
            best = d;
            found = true;
            if (out != nullptr) {
                out->entityId = entityId;
                out->edgeLocalIndex = edge.edgeLocalIndex;
            }
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
// Construction role and Delete
// ---------------------------------------------------------------------------

CadStatus setSketchEntitiesRole(CadSketch* sketch, const std::vector<SketchEntityId>& ids,
                                SketchEntityRole role) {
    if (sketch == nullptr) {
        return CadStatus::UnknownEntity;
    }
    for (SketchEntityId id : ids) {
        if (findSketchEntity(*sketch, id) == nullptr) {
            return CadStatus::UnknownEntity;
        }
    }
    for (SketchEntity& entity : sketch->entities) {
        if (sameIds(ids, entity.id())) {
            entity.setRole(role);
        }
    }
    return CadStatus::Ok;
}

SketchEntityRole sketchRoleToggleTarget(const CadSketch& sketch,
                                        const std::vector<SketchEntityId>& ids) {
    for (SketchEntityId id : ids) {
        const SketchEntity* entity = findSketchEntity(sketch, id);
        if (entity != nullptr && entity->role() == SketchEntityRole::Regular) {
            return SketchEntityRole::Construction;
        }
    }
    return SketchEntityRole::Regular;
}

CadStatus deleteSketchEntities(CadSketch* sketch, const std::vector<SketchEntityId>& ids,
                               uint32_t* outRemovedDimensions) {
    if (outRemovedDimensions != nullptr) *outRemovedDimensions = 0;
    if (sketch == nullptr || ids.empty()) {
        return CadStatus::UnknownEntity;
    }
    for (SketchEntityId id : ids) {
        if (findSketchEntity(*sketch, id) == nullptr) {
            return CadStatus::UnknownEntity;
        }
    }
    CadSketch candidate = *sketch;
    std::vector<SketchDimension> kept;
    uint32_t removed = 0;
    for (const SketchDimension& dimension : candidate.dimensions) {
        const bool firstGone = sameIds(ids, dimension.first.entityId);
        const bool hasSecond = dimension.kind == SketchDimensionKind::EdgeAngle;
        const bool secondGone = hasSecond && sameIds(ids, dimension.second.entityId);
        if (!firstGone && !secondGone) {
            kept.push_back(dimension);
        } else if (firstGone && (!hasSecond || secondGone)) {
            ++removed;  // it measured only what the user deleted
        } else {
            return CadStatus::SketchDimensionDependency;
        }
    }
    candidate.dimensions = std::move(kept);
    for (SketchEntityId id : ids) {
        removeSketchEntity(&candidate, id);
    }
    const CadStatus why = validateCadSketch(candidate);
    if (why != CadStatus::Ok) {
        return why;
    }
    *sketch = std::move(candidate);
    if (outRemovedDimensions != nullptr) *outRemovedDimensions = removed;
    return CadStatus::Ok;
}

// ---------------------------------------------------------------------------
// Trim
// ---------------------------------------------------------------------------

namespace {

// What remains of a straight edge a -> b when the interval [A, B] (in a -> b
// order) is removed: up to two segments. `cutStart` / `cutEnd` say whether A / B
// are interior cuts rather than the edge's own ends.
void straightRemains(const SketchPoint& a, const SketchPoint& b, const SketchPoint& A,
                     const SketchPoint& B, bool cutStart, bool cutEnd,
                     std::vector<SketchLine>* out) {
    if (cutStart) out->push_back(SketchLine{a, A});
    if (cutEnd) out->push_back(SketchLine{B, b});
}

}  // namespace

CadStatus planSketchTrim(const CadSketch& sketch, const SketchPoint& point, double tolerance,
                         SketchTrimPlan* out) {
    if (out == nullptr) {
        return CadStatus::DraftingNoTarget;
    }
    const SketchEntityId target = hitSketchEntity(sketch, point, tolerance);
    if (target == kNoSketchEntity) {
        return CadStatus::DraftingNoTarget;
    }
    const SketchEntity* entity = findSketchEntity(sketch, target);
    if (entity->spline() != nullptr) {
        return CadStatus::TrimSplineUnsupported;
    }
    // No dimension is guessed onto a piece: the user removes it first.
    if (!sketchDimensionsReferencing(sketch, target).empty()) {
        return CadStatus::SketchDimensionDependency;
    }
    const SketchArrangement arrangement = deriveSketchArrangement(cadSketchAllCurvesView(sketch));
    if (arrangement.status != ArrangementStatus::Ok) {
        return cadStatusForArrangement(arrangement.status);
    }
    // The fragment of the target under the finger: exactly the interval
    // bounded by the nearest cuts on either side.
    const ArrangementFragment* fragment = nullptr;
    double best = INFINITY;
    for (const ArrangementFragment& f : arrangement.fragments) {
        if (f.ref.sourceEntityId != target) continue;
        const double d = polylineDistance(point, f.points);
        if (d < best) {
            best = d;
            fragment = &f;
        }
    }
    if (fragment == nullptr) {
        return CadStatus::DraftingNoTarget;
    }
    SketchTrimPlan plan;
    plan.target = target;
    plan.edgeLocalIndex = fragment->ref.sourceEdgeLocalIndex;
    plan.removed = fragment->points;
    plan.result = sketch;
    const bool cutStart = fragment->ref.startCut.kind != ArrangementCutKind::SourceStart;
    const bool cutEnd = fragment->ref.endCut.kind != ArrangementCutKind::SourceEnd;
    const SketchPoint A = arrangement.nodes[fragment->startNode];
    const SketchPoint B = arrangement.nodes[fragment->endNode];
    const SketchEntityRole role = entity->role();
    CadSketch& result = plan.result;

    // Writes the pieces: the first keeps the target's id when `keepId` (the
    // kind is unchanged), every other piece is minted fresh with the role.
    const auto writePieces = [&](std::vector<SketchEntity::Payload> pieces, bool keepId) -> CadStatus {
        if (pieces.empty()) {
            plan.targetDeleted = true;
            return removeSketchEntity(&result, target);
        }
        size_t first = 0;
        if (keepId) {
            const CadStatus why = replaceSketchEntity(&result, target, pieces[0]);
            if (why != CadStatus::Ok) return why;
            first = 1;
        } else {
            removeSketchEntity(&result, target);
        }
        for (size_t i = first; i < pieces.size(); ++i) {
            SketchEntityId id = kNoSketchEntity;
            const CadStatus why = addSketchEntity(&result, pieces[i], &id, role);
            if (why != CadStatus::Ok) return why;
            plan.createdIds.push_back(id);
        }
        return CadStatus::Ok;
    };

    CadStatus why = CadStatus::Ok;
    if (const SketchLine* line = entity->line()) {
        std::vector<SketchLine> remains;
        straightRemains(line->start, line->end, A, B, cutStart, cutEnd, &remains);
        std::vector<SketchEntity::Payload> pieces(remains.begin(), remains.end());
        why = writePieces(std::move(pieces), /*keepId=*/true);
    } else if (const SketchRectangle* rectangle = entity->rectangle()) {
        // A trimmed rectangle has no width to edit any more: it is decomposed,
        // honestly, into the Lines it is drawn with, and the trimmed edge loses
        // its interval.
        const std::vector<SketchPoint> c = rectangleProfilePolygon(*rectangle);
        std::vector<SketchEntity::Payload> pieces;
        for (uint32_t k = 0; k < 4u; ++k) {
            const SketchPoint& a = c[k];
            const SketchPoint& b = c[(k + 1u) % 4u];
            if (k != plan.edgeLocalIndex) {
                pieces.push_back(SketchLine{a, b});
                continue;
            }
            std::vector<SketchLine> remains;
            straightRemains(a, b, A, B, cutStart, cutEnd, &remains);
            pieces.insert(pieces.end(), remains.begin(), remains.end());
        }
        plan.rectangleConverted = true;
        why = writePieces(std::move(pieces), /*keepId=*/false);
    } else if (const SketchPolyline* polyline = entity->polyline()) {
        const std::vector<SketchPoint>& v = polyline->vertices;
        const size_t n = v.size();
        const size_t i = plan.edgeLocalIndex;
        std::vector<SketchEntity::Payload> pieces;
        if (!polyline->closed) {
            SketchPolyline left;
            for (size_t k = 0; k <= i; ++k) left.vertices.push_back(v[k]);
            if (cutStart) left.vertices.push_back(A);
            SketchPolyline right;
            if (cutEnd) right.vertices.push_back(B);
            for (size_t k = i + 1; k < n; ++k) right.vertices.push_back(v[k]);
            if (left.vertices.size() >= 2) pieces.push_back(left);
            if (right.vertices.size() >= 2) pieces.push_back(right);
        } else {
            // A closed polyline opens at the removed interval: one open run
            // from the interval's far end round to its near end.
            SketchPolyline open;
            if (cutEnd) open.vertices.push_back(B);
            for (size_t k = 1; k <= n; ++k) open.vertices.push_back(v[(i + k) % n]);
            if (cutStart) open.vertices.push_back(A);
            if (open.vertices.size() >= 2) pieces.push_back(open);
        }
        why = writePieces(std::move(pieces), /*keepId=*/true);
    } else if (const SketchCircle* circle = entity->circle()) {
        // A circle cut at fewer than two points has no bounded interval: the
        // whole circle is what is under the finger.
        std::vector<SketchEntity::Payload> pieces;
        const bool whole = fragment->startNode == fragment->endNode;
        if (!whole && cutStart && cutEnd) {
            // What remains runs counter-clockwise from B round to A, on the
            // ORIGINAL circle's centre and radius.
            pieces.push_back(arcBetween(circle->center, circle->radius, B, A, 1.0));
        }
        why = writePieces(std::move(pieces), /*keepId=*/false);
    } else if (const SketchArc* arc = entity->arc()) {
        SketchPoint centre;
        double radius = 0.0;
        double start = 0.0;
        double sweep = 0.0;
        why = arcGeometry(*arc, &centre, &radius, &start, &sweep);
        if (why == CadStatus::Ok) {
            const double sign = sweep >= 0.0 ? 1.0 : -1.0;
            std::vector<SketchEntity::Payload> pieces;
            if (cutStart) pieces.push_back(arcBetween(centre, radius, arc->start, A, sign));
            if (cutEnd) pieces.push_back(arcBetween(centre, radius, B, arc->end, sign));
            why = writePieces(std::move(pieces), /*keepId=*/true);
        }
    } else {
        why = CadStatus::TrimSplineUnsupported;
    }
    if (why != CadStatus::Ok) {
        return why;
    }
    why = validateCadSketch(result);
    if (why != CadStatus::Ok) {
        return why;
    }
    std::sort(plan.createdIds.begin(), plan.createdIds.end());
    *out = std::move(plan);
    return CadStatus::Ok;
}

// ---------------------------------------------------------------------------
// Extend
// ---------------------------------------------------------------------------

namespace {

// Everything the sketch draws, as a bounding box: what the probe must reach.
void growBounds(const SketchEntity& entity, double* lo, double* hi) {
    const auto take = [&](const SketchPoint& p) {
        lo[0] = std::fmin(lo[0], p.u);
        lo[1] = std::fmin(lo[1], p.v);
        hi[0] = std::fmax(hi[0], p.u);
        hi[1] = std::fmax(hi[1], p.v);
    };
    if (const SketchCircle* circle = entity.circle()) {
        take(SketchPoint{circle->center.u - circle->radius, circle->center.v - circle->radius});
        take(SketchPoint{circle->center.u + circle->radius, circle->center.v + circle->radius});
        return;
    }
    if (const SketchRectangle* rectangle = entity.rectangle()) {
        for (const SketchPoint& p : rectangleProfilePolygon(*rectangle)) take(p);
        return;
    }
    if (const SketchPolyline* polyline = entity.polyline()) {
        for (const SketchPoint& p : polyline->vertices) take(p);
        return;
    }
    std::vector<SketchPoint> points;
    if (tessellateSketchCurve(entity, &points) == CadStatus::Ok) {
        for (const SketchPoint& p : points) take(p);
    }
    // A spline's Bezier hull can stand slightly outside its samples; the probe
    // carries a margin for that below.
}

struct ProbeHit {
    bool found = false;
    double along = INFINITY;  // metres along a straight probe, radians along an arc
    SketchPoint point{};
};

// Intersects the probe (entity id 1 of a two-entity sketch) with `other`
// through the arrangement, keeping the nearest interior contact. `measure`
// turns a contact point into a distance along the probe.
template <typename Measure>
CadStatus probeAgainst(const SketchEntity& probe, const SketchEntity& other, Measure measure,
                       double minAlong, ProbeHit* hit) {
    CadSketch pair;
    pair.entities.emplace_back(1u, probe.payload());
    pair.entities.emplace_back(2u, other.payload());
    pair.nextEntityId = 3;
    const SketchArrangement arrangement = deriveSketchArrangement(pair);
    if (arrangement.status == ArrangementStatus::AmbiguousOverlap) {
        return CadStatus::ExtendAmbiguous;
    }
    if (arrangement.status != ArrangementStatus::Ok) {
        return cadStatusForArrangement(arrangement.status);
    }
    for (const ArrangementFragment& f : arrangement.fragments) {
        if (f.ref.sourceEntityId != 1u) continue;
        for (int side = 0; side < 2; ++side) {
            const ArrangementCut& cut = side == 0 ? f.ref.startCut : f.ref.endCut;
            if (cut.kind != ArrangementCutKind::Intersection) continue;
            const SketchPoint p = arrangement.nodes[side == 0 ? f.startNode : f.endNode];
            const double along = measure(p);
            if (along > minAlong && along < hit->along) {
                hit->found = true;
                hit->along = along;
                hit->point = p;
            }
        }
    }
    return CadStatus::Ok;
}

}  // namespace

CadStatus planSketchExtend(const CadSketch& sketch, const SketchPoint& point, double tolerance,
                           SketchExtendPlan* out) {
    if (out == nullptr) {
        return CadStatus::DraftingNoTarget;
    }
    const SketchEntityId target = hitSketchEntity(sketch, point, tolerance);
    if (target == kNoSketchEntity) {
        return CadStatus::DraftingNoTarget;
    }
    const SketchEntity* entity = findSketchEntity(sketch, target);
    SketchExtendPlan plan;
    plan.target = target;
    plan.result = sketch;

    // Bounds of everything else, with a margin, so the probe reaches every
    // curve and stops well inside the sketch range.
    double lo[2] = {INFINITY, INFINITY};
    double hi[2] = {-INFINITY, -INFINITY};
    for (const SketchEntity& other : sketch.entities) {
        growBounds(other, lo, hi);
    }

    if (entity->line() != nullptr || entity->polyline() != nullptr) {
        SketchPoint P;
        SketchPoint behind;
        if (const SketchLine* line = entity->line()) {
            if (sketchLineLengthDriven(sketch, target)) {
                return CadStatus::SketchDimensionLocked;
            }
            plan.atEnd = distance(point, line->end) <= distance(point, line->start);
            P = plan.atEnd ? line->end : line->start;
            behind = plan.atEnd ? line->start : line->end;
        } else {
            const SketchPolyline& polyline = *entity->polyline();
            if (polyline.closed) {
                return CadStatus::ExtendUnsupported;
            }
            const std::vector<SketchPoint>& v = polyline.vertices;
            plan.atEnd = distance(point, v.back()) <= distance(point, v.front());
            P = plan.atEnd ? v.back() : v.front();
            behind = plan.atEnd ? v[v.size() - 2] : v[1];
        }
        const double l = distance(P, behind);
        const double du = (P.u - behind.u) / l;
        const double dv = (P.v - behind.v) / l;
        // The probe: the EXACT continuation of the edge, out to the far side of
        // everything drawn, clipped to the sketch range.
        double reach = 0.0;
        for (const SketchPoint& corner : {SketchPoint{lo[0], lo[1]}, SketchPoint{lo[0], hi[1]},
                                          SketchPoint{hi[0], lo[1]}, SketchPoint{hi[0], hi[1]}}) {
            reach = std::fmax(reach, distance(P, corner));
        }
        reach = reach * 1.05 + 1.0e-3;
        const double limit = kMaxSketchCoordinateMeters * 0.999;
        if (du > 0.0) reach = std::fmin(reach, (limit - P.u) / du);
        if (du < 0.0) reach = std::fmin(reach, (-limit - P.u) / du);
        if (dv > 0.0) reach = std::fmin(reach, (limit - P.v) / dv);
        if (dv < 0.0) reach = std::fmin(reach, (-limit - P.v) / dv);
        if (!(reach > kSketchCoincidenceMeters)) {
            return CadStatus::ExtendNoTarget;
        }
        const SketchEntity probe(1u, SketchLine{P, SketchPoint{P.u + du * reach, P.v + dv * reach}});
        ProbeHit hit;
        const auto measure = [&](const SketchPoint& p) { return (p.u - P.u) * du + (p.v - P.v) * dv; };
        for (const SketchEntity& other : sketch.entities) {
            // A Line never meets its own continuation; a Polyline may (its own
            // earlier segments are real targets).
            if (other.id() == target && entity->line() != nullptr) continue;
            const CadStatus why = probeAgainst(probe, other, measure, kSketchCoincidenceMeters, &hit);
            if (why != CadStatus::Ok) return why;
        }
        if (!hit.found) {
            return CadStatus::ExtendNoTarget;
        }
        plan.from = P;
        plan.to = hit.point;
        plan.added = {P, hit.point};
        SketchEntity::Payload payload = entity->payload();
        if (const SketchLine* line = entity->line()) {
            SketchLine extended = *line;
            (plan.atEnd ? extended.end : extended.start) = hit.point;
            payload = extended;
        } else {
            SketchPolyline extended = *entity->polyline();
            (plan.atEnd ? extended.vertices.back() : extended.vertices.front()) = hit.point;
            payload = extended;
        }
        const CadStatus why = replaceSketchEntity(&plan.result, target, payload);
        if (why != CadStatus::Ok) return why;
    } else if (const SketchArc* arc = entity->arc()) {
        SketchPoint centre;
        double radius = 0.0;
        double start = 0.0;
        double sweep = 0.0;
        CadStatus why = arcGeometry(*arc, &centre, &radius, &start, &sweep);
        if (why != CadStatus::Ok) return why;
        const double sign = sweep >= 0.0 ? 1.0 : -1.0;
        plan.atEnd = distance(point, arc->end) <= distance(point, arc->start);
        // The continuation runs on round the SAME circle, away from the arc,
        // stopping just short of the arc's other end.
        const SketchPoint P = plan.atEnd ? arc->end : arc->start;
        const double sense = plan.atEnd ? sign : -sign;
        const double fromAngle = angleOf(centre, P);
        const double span = kTwoPi - std::fabs(sweep) - 2.0e-3;
        if (!(span > 1.0e-3)) {
            return CadStatus::ExtendNoTarget;
        }
        SketchArc probeArc;
        probeArc.start = P;
        probeArc.mid = onCircle(centre, radius, fromAngle + sense * span * 0.5);
        probeArc.end = onCircle(centre, radius, fromAngle + sense * span);
        const SketchEntity probe(1u, probeArc);
        ProbeHit hit;
        const auto measure = [&](const SketchPoint& p) {
            return travel(fromAngle, angleOf(centre, p), sense);
        };
        const double minAlong = kSketchCoincidenceMeters / radius;
        for (const SketchEntity& other : sketch.entities) {
            if (other.id() == target) continue;  // its own circle: an overlap
            why = probeAgainst(probe, other, measure, minAlong, &hit);
            if (why != CadStatus::Ok) return why;
        }
        if (!hit.found) {
            return CadStatus::ExtendNoTarget;
        }
        plan.from = P;
        plan.to = hit.point;
        const uint32_t steps = std::max<uint32_t>(2u, sketchArcSegmentCount(hit.along));
        for (uint32_t k = 0; k <= steps; ++k) {
            plan.added.push_back(k == steps ? hit.point
                                            : onCircle(centre, radius,
                                                       fromAngle + sense * hit.along * k / steps));
        }
        // The new arc, through the middle of its new sweep; the untouched end
        // keeps its authored value.
        const SketchArc extended = plan.atEnd
                                           ? arcBetween(centre, radius, arc->start, hit.point, sign)
                                           : arcBetween(centre, radius, hit.point, arc->end, sign);
        why = replaceSketchEntity(&plan.result, target, extended);
        if (why != CadStatus::Ok) return why;
    } else {
        return CadStatus::ExtendUnsupported;
    }
    const CadStatus why = validateCadSketch(plan.result);
    if (why != CadStatus::Ok) {
        return why;
    }
    *out = std::move(plan);
    return CadStatus::Ok;
}

// ---------------------------------------------------------------------------
// Offset
// ---------------------------------------------------------------------------

bool sketchEntityOffsettable(const SketchEntity& entity) {
    return entity.spline() == nullptr;
}

bool sketchOffsetDistanceAt(const SketchEntity& entity, const SketchPoint& point, double* out) {
    if (out == nullptr) return false;
    if (const SketchLine* line = entity.line()) {
        const double l = distance(line->start, line->end);
        if (!(l > 0.0)) return false;
        const double nu = -(line->end.v - line->start.v) / l;
        const double nv = (line->end.u - line->start.u) / l;
        *out = (point.u - line->start.u) * nu + (point.v - line->start.v) * nv;
        return std::isfinite(*out);
    }
    if (const SketchCircle* circle = entity.circle()) {
        *out = distance(point, circle->center) - circle->radius;
        return std::isfinite(*out);
    }
    if (const SketchArc* arc = entity.arc()) {
        SketchPoint centre;
        double radius = 0.0;
        if (arcGeometry(*arc, &centre, &radius, nullptr, nullptr) != CadStatus::Ok) return false;
        *out = distance(point, centre) - radius;
        return std::isfinite(*out);
    }
    if (const SketchRectangle* rectangle = entity.rectangle()) {
        // On the offset rectangle's boundary, the larger of the two axis
        // overhangs IS the distance.
        *out = std::fmax(std::fabs(point.u - rectangle->center.u) - rectangle->width * 0.5,
                         std::fabs(point.v - rectangle->center.v) - rectangle->height * 0.5);
        return std::isfinite(*out);
    }
    if (const SketchPolyline* polyline = entity.polyline()) {
        // The nearest segment, signed by which side of it the point is on.
        const std::vector<SketchPoint>& v = polyline->vertices;
        const size_t n = v.size();
        const size_t segments = polyline->closed ? n : n - 1;
        double best = INFINITY;
        double signedBest = 0.0;
        for (size_t i = 0; i < segments; ++i) {
            const SketchPoint& a = v[i];
            const SketchPoint& b = v[(i + 1) % n];
            const double d = segmentDistance(point, a, b);
            if (d < best) {
                best = d;
                const double cross = (b.u - a.u) * (point.v - a.v) - (b.v - a.v) * (point.u - a.u);
                signedBest = cross >= 0.0 ? d : -d;
            }
        }
        *out = signedBest;
        return std::isfinite(*out);
    }
    return false;
}

CadStatus offsetSketchEntity(const SketchEntity& entity, double d,
                             std::vector<SketchEntity::Payload>* out) {
    if (out == nullptr) return CadStatus::OffsetInvalid;
    out->clear();
    if (entity.spline() != nullptr) {
        return CadStatus::OffsetSplineUnsupported;
    }
    if (!std::isfinite(d) || std::fabs(d) <= kSketchCoincidenceMeters
        || std::fabs(d) > kMaxSketchCoordinateMeters) {
        return CadStatus::OffsetInvalid;
    }
    SketchEntity::Payload payload;
    if (const SketchLine* line = entity.line()) {
        const double l = distance(line->start, line->end);
        const double nu = -(line->end.v - line->start.v) / l;
        const double nv = (line->end.u - line->start.u) / l;
        payload = SketchLine{SketchPoint{line->start.u + nu * d, line->start.v + nv * d},
                             SketchPoint{line->end.u + nu * d, line->end.v + nv * d}};
    } else if (const SketchCircle* circle = entity.circle()) {
        SketchCircle offset = *circle;
        offset.radius = circle->radius + d;
        if (!(offset.radius > kSketchCoincidenceMeters)) return CadStatus::OffsetInvalid;
        payload = offset;
    } else if (const SketchArc* arc = entity.arc()) {
        SketchPoint centre;
        double radius = 0.0;
        const CadStatus why = arcGeometry(*arc, &centre, &radius, nullptr, nullptr);
        if (why != CadStatus::Ok) return why;
        const double r = radius + d;
        if (!(r > kSketchCoincidenceMeters)) return CadStatus::OffsetInvalid;
        // Each authored point moves RADIALLY: the angles, and so the sweep and
        // its sense, are kept.
        const double k = r / radius;
        const auto radial = [&](const SketchPoint& p) {
            return SketchPoint{centre.u + (p.u - centre.u) * k, centre.v + (p.v - centre.v) * k};
        };
        payload = SketchArc{radial(arc->start), radial(arc->mid), radial(arc->end)};
    } else if (const SketchRectangle* rectangle = entity.rectangle()) {
        SketchRectangle offset = *rectangle;
        offset.width = rectangle->width + 2.0 * d;
        offset.height = rectangle->height + 2.0 * d;
        if (!(offset.width > kSketchCoincidenceMeters) || !(offset.height > kSketchCoincidenceMeters)) {
            return CadStatus::OffsetInvalid;
        }
        payload = offset;
    } else if (const SketchPolyline* polyline = entity.polyline()) {
        const std::vector<SketchPoint>& v = polyline->vertices;
        const size_t n = v.size();
        const bool closed = polyline->closed;
        const size_t segments = closed ? n : n - 1;
        std::vector<double> nu(segments), nv(segments);
        for (size_t i = 0; i < segments; ++i) {
            const SketchPoint& a = v[i];
            const SketchPoint& b = v[(i + 1) % n];
            const double l = distance(a, b);
            if (!(l > 0.0)) return CadStatus::OffsetInvalid;
            nu[i] = -(b.v - a.v) / l;
            nv[i] = (b.u - a.u) / l;
        }
        SketchPolyline offset;
        offset.closed = closed;
        offset.vertices.resize(n);
        for (size_t k = 0; k < n; ++k) {
            const bool hasPrev = closed || k > 0;
            const bool hasNext = closed || k + 1 < n;
            if (!hasPrev || !hasNext) {
                // An open end moves straight along its one segment's normal.
                const size_t s = hasNext ? k : k - 1;
                offset.vertices[k] = SketchPoint{v[k].u + nu[s] * d, v[k].v + nv[s] * d};
                continue;
            }
            const size_t prev = (k + segments - 1) % segments;
            const size_t next = k % segments;
            // The MITER: where the two offset lines meet, v + d (n1 + n2) /
            // (1 + n1.n2). Its length over |d| is 1/cos(half the turn).
            const double dot = nu[prev] * nu[next] + nv[prev] * nv[next];
            if (!(1.0 + dot > 1.0e-12)) return CadStatus::OffsetInvalid;  // a reversal
            const double mu = (nu[prev] + nu[next]) / (1.0 + dot);
            const double mv = (nv[prev] + nv[next]) / (1.0 + dot);
            if (std::sqrt(mu * mu + mv * mv) > kOffsetMiterLimit) {
                return CadStatus::OffsetMiterLimit;
            }
            offset.vertices[k] = SketchPoint{v[k].u + mu * d, v[k].v + mv * d};
        }
        // Every offset segment must keep its direction (no collapse, no
        // reversal) and no two non-adjacent ones may meet.
        for (size_t i = 0; i < segments; ++i) {
            const SketchPoint& a = v[i];
            const SketchPoint& b = v[(i + 1) % n];
            const SketchPoint& oa = offset.vertices[i];
            const SketchPoint& ob = offset.vertices[(i + 1) % n];
            const double along = (ob.u - oa.u) * (b.u - a.u) + (ob.v - oa.v) * (b.v - a.v);
            if (!(along > 0.0) || distance(oa, ob) <= kSketchCoincidenceMeters) {
                return CadStatus::OffsetInvalid;
            }
        }
        for (size_t i = 0; i < segments; ++i) {
            for (size_t j = i + 1; j < segments; ++j) {
                const bool adjacent = j == i + 1 || (closed && i == 0 && j == segments - 1);
                if (adjacent) continue;
                if (sketchSegmentsIntersect(offset.vertices[i], offset.vertices[(i + 1) % n],
                                            offset.vertices[j], offset.vertices[(j + 1) % n])) {
                    return CadStatus::OffsetSelfIntersecting;
                }
            }
        }
        payload = offset;
    } else {
        return CadStatus::OffsetInvalid;
    }
    const CadStatus why = validateSketchEntity(SketchEntity(1u, payload));
    if (why != CadStatus::Ok) {
        return why == CadStatus::OutOfRange ? CadStatus::OffsetInvalid : why;
    }
    out->push_back(std::move(payload));
    return CadStatus::Ok;
}

CadStatus applySketchOffset(CadSketch* sketch, SketchEntityId source, double signedDistance,
                            std::vector<SketchEntityId>* outIds) {
    if (sketch == nullptr) return CadStatus::UnknownEntity;
    const SketchEntity* entity = findSketchEntity(*sketch, source);
    if (entity == nullptr) return CadStatus::UnknownEntity;
    std::vector<SketchEntity::Payload> pieces;
    const CadStatus why = offsetSketchEntity(*entity, signedDistance, &pieces);
    if (why != CadStatus::Ok) return why;
    CadSketch candidate = *sketch;
    std::vector<SketchEntityId> ids;
    const SketchEntityRole role = entity->role();
    for (SketchEntity::Payload& piece : pieces) {
        SketchEntityId id = kNoSketchEntity;
        const CadStatus added = addSketchEntity(&candidate, std::move(piece), &id, role);
        if (added != CadStatus::Ok) return added;
        ids.push_back(id);
    }
    *sketch = std::move(candidate);
    if (outIds != nullptr) *outIds = std::move(ids);
    return CadStatus::Ok;
}

// ---------------------------------------------------------------------------
// Mirror
// ---------------------------------------------------------------------------

CadStatus resolveSketchMirrorAxis(const CadSketch& sketch, const CadSketchEdgeRef& axis,
                                  SketchPoint* outPoint, double* outDu, double* outDv) {
    const SketchEntity* entity = findSketchEntity(sketch, axis.entityId);
    if (entity == nullptr) {
        return CadStatus::MirrorAxisInvalid;
    }
    for (const SketchStraightEdge& edge : sketchEntityStraightEdges(*entity)) {
        if (edge.edgeLocalIndex != axis.edgeLocalIndex) continue;
        const double du = edge.end.u - edge.start.u;
        const double dv = edge.end.v - edge.start.v;
        const double l = std::sqrt(du * du + dv * dv);
        if (!std::isfinite(l) || l <= kSketchCoincidenceMeters) {
            return CadStatus::MirrorAxisInvalid;
        }
        if (outPoint != nullptr) *outPoint = edge.start;
        if (outDu != nullptr) *outDu = du / l;
        if (outDv != nullptr) *outDv = dv / l;
        return CadStatus::Ok;
    }
    return CadStatus::MirrorAxisInvalid;
}

SketchPoint reflectSketchPoint(const SketchPoint& p, const SketchPoint& a, double du, double dv) {
    // The foot of the perpendicular, then as far again past it.
    const double t = (p.u - a.u) * du + (p.v - a.v) * dv;
    const double fu = a.u + du * t;
    const double fv = a.v + dv * t;
    return SketchPoint{2.0 * fu - p.u, 2.0 * fv - p.v};
}

CadStatus mirrorSketchEntities(const CadSketch& sketch, const std::vector<SketchEntityId>& ids,
                               const CadSketchEdgeRef& axis, std::vector<SketchMirrorPiece>* out) {
    if (out == nullptr) return CadStatus::MirrorNothingSelected;
    out->clear();
    SketchPoint a;
    double du = 0.0;
    double dv = 0.0;
    const CadStatus axisWhy = resolveSketchMirrorAxis(sketch, axis, &a, &du, &dv);
    if (axisWhy != CadStatus::Ok) return axisWhy;
    for (SketchEntityId id : ids) {
        if (findSketchEntity(sketch, id) == nullptr) return CadStatus::UnknownEntity;
    }
    // An axis parallel to a sketch axis EXACTLY keeps a rectangle a rectangle.
    const bool axisAligned = du == 0.0 || dv == 0.0;
    const auto r = [&](const SketchPoint& p) { return reflectSketchPoint(p, a, du, dv); };
    for (const SketchEntity& entity : sketch.entities) {
        if (!sameIds(ids, entity.id()) || entity.id() == axis.entityId) continue;
        const SketchEntityRole role = entity.role();
        if (const SketchLine* line = entity.line()) {
            out->push_back({SketchLine{r(line->start), r(line->end)}, role});
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            SketchPolyline m = *polyline;
            for (SketchPoint& p : m.vertices) p = r(p);
            out->push_back({m, role});
        } else if (const SketchCircle* circle = entity.circle()) {
            SketchCircle m = *circle;
            m.center = r(circle->center);
            out->push_back({m, role});
        } else if (const SketchArc* arc = entity.arc()) {
            // Three points ON the curve reflect to three points on its mirror:
            // the through-point keeps the same arc, and its sense flips with
            // the reflection by itself.
            out->push_back({SketchArc{r(arc->start), r(arc->mid), r(arc->end)}, role});
        } else if (const SketchSpline* spline = entity.spline()) {
            SketchSpline m = *spline;
            for (SketchPoint& p : m.points) p = r(p);
            out->push_back({m, role});
        } else if (const SketchRectangle* rectangle = entity.rectangle()) {
            if (axisAligned) {
                SketchRectangle m = *rectangle;
                m.center = r(rectangle->center);
                out->push_back({m, role});
            } else {
                // A rectangle reflected across a slanted axis is a rotated
                // rectangle, which is not a parameter this sketch has: four
                // Lines, in the rectangle's own canonical edge order.
                const std::vector<SketchPoint> c = rectangleProfilePolygon(*rectangle);
                for (uint32_t k = 0; k < 4u; ++k) {
                    out->push_back({SketchLine{r(c[k]), r(c[(k + 1u) % 4u])}, role});
                }
            }
        }
    }
    if (out->empty()) return CadStatus::MirrorNothingSelected;
    return CadStatus::Ok;
}

CadStatus applySketchMirror(CadSketch* sketch, const std::vector<SketchEntityId>& ids,
                            const CadSketchEdgeRef& axis, std::vector<SketchEntityId>* outIds) {
    if (sketch == nullptr) return CadStatus::MirrorNothingSelected;
    std::vector<SketchMirrorPiece> pieces;
    const CadStatus why = mirrorSketchEntities(*sketch, ids, axis, &pieces);
    if (why != CadStatus::Ok) return why;
    CadSketch candidate = *sketch;
    std::vector<SketchEntityId> created;
    for (SketchMirrorPiece& piece : pieces) {
        SketchEntityId id = kNoSketchEntity;
        const CadStatus added = addSketchEntity(&candidate, std::move(piece.payload), &id, piece.role);
        if (added != CadStatus::Ok) return added;
        created.push_back(id);
    }
    *sketch = std::move(candidate);
    if (outIds != nullptr) *outIds = std::move(created);
    return CadStatus::Ok;
}

}  // namespace forgeshape
