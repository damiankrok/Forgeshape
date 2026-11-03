#include "forgeshape_sketch_snap.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_sketch_arrangement.h"

namespace forgeshape {

const char* sketchSnapKindName(SketchSnapKind kind) {
    switch (kind) {
        case SketchSnapKind::None: return "None";
        case SketchSnapKind::Grid: return "Grid";
        case SketchSnapKind::Endpoint: return "Endpoint";
        case SketchSnapKind::Intersection: return "Intersection";
        case SketchSnapKind::Midpoint: return "Midpoint";
        case SketchSnapKind::Center: return "Center";
        case SketchSnapKind::Origin: return "Origin";
        case SketchSnapKind::HorizontalGuide: return "HorizontalGuide";
        case SketchSnapKind::VerticalGuide: return "VerticalGuide";
    }
    return "unknown";
}

SketchSnapCandidates collectSketchSnapCandidates(const CadSketch& sketch,
                                                 const SketchArrangement* allCurves) {
    SketchSnapCandidates out;
    std::vector<const SketchEntity*> entities;
    entities.reserve(sketch.entities.size());
    for (const SketchEntity& entity : sketch.entities) {
        entities.push_back(&entity);
    }
    // Semantic order: the tie rule must not depend on the vector's order.
    std::sort(entities.begin(), entities.end(),
              [](const SketchEntity* a, const SketchEntity* b) { return a->id() < b->id(); });
    for (const SketchEntity* entity : entities) {
        if (const SketchLine* line = entity->line()) {
            out.endpoints.push_back(line->start);
            out.endpoints.push_back(line->end);
        } else if (const SketchPolyline* polyline = entity->polyline()) {
            out.endpoints.insert(out.endpoints.end(), polyline->vertices.begin(),
                                 polyline->vertices.end());
        } else if (const SketchRectangle* rectangle = entity->rectangle()) {
            const std::vector<SketchPoint> corners = rectangleProfilePolygon(*rectangle);
            out.endpoints.insert(out.endpoints.end(), corners.begin(), corners.end());
            out.centers.push_back(rectangle->center);
        } else if (const SketchCircle* circle = entity->circle()) {
            out.centers.push_back(circle->center);
        } else if (const SketchArc* arc = entity->arc()) {
            // The three AUTHORED points: snapping to a curve means snapping to
            // something the user placed.
            out.endpoints.push_back(arc->start);
            out.endpoints.push_back(arc->mid);
            out.endpoints.push_back(arc->end);
            SketchPoint centre;
            double radius = 0.0;
            if (arcGeometry(*arc, &centre, &radius, nullptr, nullptr) == CadStatus::Ok) {
                out.centers.push_back(centre);
            }
        } else if (const SketchSpline* spline = entity->spline()) {
            out.endpoints.insert(out.endpoints.end(), spline->points.begin(),
                                 spline->points.end());
        }
        // Midpoints of every straight edge, analytically.
        for (const SketchStraightEdge& edge : sketchEntityStraightEdges(*entity)) {
            out.midpoints.push_back(SketchPoint{(edge.start.u + edge.end.u) * 0.5,
                                                (edge.start.v + edge.end.v) * 0.5});
        }
    }
    // Intersections: the SAME analytic contacts the planar arrangement uses,
    // over every curve (Construction included). A node is an intersection when
    // some fragment is cut there by a partner -- a crossing or a T-junction --
    // rather than merely meeting another curve end to end.
    if (!sketch.entities.empty()) {
        const SketchArrangement derived =
                allCurves != nullptr ? SketchArrangement{}
                                     : deriveSketchArrangement(cadSketchAllCurvesView(sketch));
        const SketchArrangement& arrangement = allCurves != nullptr ? *allCurves : derived;
        if (arrangement.status != ArrangementStatus::Ok) {
            out.intersectionsDerived = false;
        } else {
            std::vector<uint8_t> isIntersection(arrangement.nodes.size(), 0u);
            for (const ArrangementFragment& fragment : arrangement.fragments) {
                if (fragment.ref.startCut.kind == ArrangementCutKind::Intersection
                    && fragment.startNode < isIntersection.size()) {
                    isIntersection[fragment.startNode] = 1u;
                }
                if (fragment.ref.endCut.kind == ArrangementCutKind::Intersection
                    && fragment.endNode < isIntersection.size()) {
                    isIntersection[fragment.endNode] = 1u;
                }
            }
            for (size_t n = 0; n < arrangement.nodes.size(); ++n) {
                if (isIntersection[n] != 0u) {
                    out.intersections.push_back(arrangement.nodes[n]);
                }
            }
        }
    }
    return out;
}

namespace {

double distance(const SketchPoint& a, const SketchPoint& b) {
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return std::sqrt(du * du + dv * dv);
}

// The nearest of `points` within `tolerance`, strictly nearer to replace an
// earlier one, so an exact tie keeps the earlier (semantic) candidate.
bool nearest(const std::vector<SketchPoint>& points, const SketchPoint& raw, double tolerance,
             const SketchPoint* exclude, SketchPoint* out, double* outDistance) {
    bool found = false;
    double best = tolerance;
    for (const SketchPoint& p : points) {
        if (exclude != nullptr && distance(p, *exclude) <= kSketchCoincidenceMeters) {
            continue;
        }
        const double d = distance(raw, p);
        if (d <= tolerance && (!found || d < best)) {
            best = d;
            *out = p;
            found = true;
        }
    }
    if (found && outDistance != nullptr) *outDistance = best;
    return found;
}

double gridRound(double value, double step) {
    return std::round(value / step) * step;
}

}  // namespace

SketchSnapResult snapSketchPoint(const SketchSnapCandidates& candidates, const SketchPoint& raw,
                                 double toleranceMeters, double gridStep,
                                 const std::vector<SketchPoint>& extraEndpoints,
                                 const SketchPoint* guideAnchor, const SketchPoint* dragStart) {
    SketchSnapResult result;
    result.point = raw;
    result.kind = SketchSnapKind::None;
    const bool snapping = std::isfinite(toleranceMeters) && toleranceMeters > 0.0;
    if (snapping) {
        // 1 Endpoint (the sketch's, then the in-progress ones, in that order).
        std::vector<SketchPoint> endpoints = candidates.endpoints;
        endpoints.insert(endpoints.end(), extraEndpoints.begin(), extraEndpoints.end());
        const struct {
            const std::vector<SketchPoint>* points;
            SketchSnapKind kind;
        } levels[] = {
                {&endpoints, SketchSnapKind::Endpoint},
                {&candidates.intersections, SketchSnapKind::Intersection},
                {&candidates.midpoints, SketchSnapKind::Midpoint},
                {&candidates.centers, SketchSnapKind::Center},
        };
        // The priority is applied in TWO apertures: first among what lies
        // within the inner one (a third of the finger's), then within the
        // whole. So an exact point under the finger is never passed over for a
        // higher-priority one a whole finger-width away, and within either
        // aperture the order is the one stated above.
        const SketchPoint origin{0.0, 0.0};
        for (const double aperture : {toleranceMeters * kSketchSnapInnerApertureFraction,
                                      toleranceMeters}) {
            for (const auto& level : levels) {
                SketchPoint at;
                if (nearest(*level.points, raw, aperture, dragStart, &at, nullptr)) {
                    result.point = at;
                    result.kind = level.kind;
                    return result;
                }
            }
            // 5 Origin: exactly (0, 0).
            if (distance(raw, origin) <= aperture
                && (dragStart == nullptr
                    || distance(origin, *dragStart) > kSketchCoincidenceMeters)) {
                result.point = origin;
                result.kind = SketchSnapKind::Origin;
                return result;
            }
        }
        // 6 Guides: align ONE coordinate with a reference point's exact value.
        // The references are every exact point the levels above offer, the
        // origin, and the gesture's own start.
        std::vector<SketchPoint> sources = endpoints;
        sources.insert(sources.end(), candidates.midpoints.begin(), candidates.midpoints.end());
        sources.insert(sources.end(), candidates.centers.begin(), candidates.centers.end());
        sources.push_back(origin);
        if (guideAnchor != nullptr) {
            sources.push_back(*guideAnchor);
        }
        // A guide captures within HALF the point tolerance: a point snap is a
        // finger landing on a target, an alignment is a hint the finger should
        // be able to slide past without being dragged onto it.
        const double guideTolerance = toleranceMeters * kSketchGuideToleranceFraction;
        double bestH = guideTolerance;
        double bestV = guideTolerance;
        for (const SketchPoint& s : sources) {
            const double dv = std::fabs(raw.v - s.v);
            // A source at the raw point's own place is a point snap's business;
            // a guide aligns with something AWAY from the finger.
            if (dv <= guideTolerance && std::fabs(raw.u - s.u) > toleranceMeters
                && (!result.horizontalGuide || dv < bestH)) {
                bestH = dv;
                result.horizontalGuide = true;
                result.horizontalSource = s;
            }
            const double du = std::fabs(raw.u - s.u);
            if (du <= guideTolerance && std::fabs(raw.v - s.v) > toleranceMeters
                && (!result.verticalGuide || du < bestV)) {
                bestV = du;
                result.verticalGuide = true;
                result.verticalSource = s;
            }
        }
        if (result.horizontalGuide || result.verticalGuide) {
            const bool grid = std::isfinite(gridStep) && gridStep > 0.0;
            result.point.v = result.horizontalGuide ? result.horizontalSource.v
                                                    : (grid ? gridRound(raw.v, gridStep) : raw.v);
            result.point.u = result.verticalGuide ? result.verticalSource.u
                                                  : (grid ? gridRound(raw.u, gridStep) : raw.u);
            result.kind = result.horizontalGuide ? SketchSnapKind::HorizontalGuide
                                                 : SketchSnapKind::VerticalGuide;
            // Two alignments can meet exactly at the drag's own start; that is
            // no alignment the finger asked for, so the grid answers instead.
            if (dragStart == nullptr || distance(result.point, *dragStart) > kSketchCoincidenceMeters) {
                return result;
            }
            result.horizontalGuide = false;
            result.verticalGuide = false;
            result.point = raw;
            result.kind = SketchSnapKind::None;
        }
    }
    // 7 Grid: exact multiples of the current step.
    if (std::isfinite(gridStep) && gridStep > 0.0) {
        result.point.u = gridRound(raw.u, gridStep);
        result.point.v = gridRound(raw.v, gridStep);
        result.kind = SketchSnapKind::Grid;
    }
    return result;
}

}  // namespace forgeshape
