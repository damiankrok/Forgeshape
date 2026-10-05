#include "forgeshape_cad_revolve.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_sketch_region.h"

namespace forgeshape {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// Signed distance from the infinite axis line, positive on the side its left
// normal (-dv, du) points to.
double axisSide(const RevolveAxis2D& axis, const SketchPoint& p) {
    return (p.u - axis.start.u) * -axis.dv + (p.v - axis.start.v) * axis.du;
}

double axisAlong(const RevolveAxis2D& axis, const SketchPoint& p) {
    return (p.u - axis.start.u) * axis.du + (p.v - axis.start.v) * axis.dv;
}

// The angle in [0, 2*pi) of `a` measured from zero, for the arc test below.
double positiveAngle(double a) {
    double r = std::fmod(a, kTwoPi);
    if (r < 0.0) {
        r += kTwoPi;
    }
    return r;
}

// The closest the TRUE arc of a circle (centre c, radius R) between two of its
// own points a and b -- the short way round, which is what a tessellation edge
// spans -- comes to the axis, measured on side `side`. Exact: the extreme of a
// linear function over a circle arc is at an end or at the one angle where the
// circle's normal points straight at the axis.
double arcClosestApproach(const RevolveAxis2D& axis, int side, const SketchPoint& c, double R,
                          const SketchPoint& a, const SketchPoint& b) {
    const double ra = side * axisSide(axis, a);
    const double rb = side * axisSide(axis, b);
    double best = std::min(ra, rb);
    const double pa = std::atan2(a.v - c.v, a.u - c.u);
    const double pb = std::atan2(b.v - c.v, b.u - c.u);
    // Direction from the centre in which side*rho is smallest: -side*(left normal).
    const double target = std::atan2(-side * axis.du, -side * -axis.dv);
    double delta = std::remainder(pb - pa, kTwoPi);  // the short way, (-pi, pi]
    const double rel = target - pa;
    bool inside = false;
    if (delta >= 0.0) {
        inside = positiveAngle(rel) <= delta;
    } else {
        inside = positiveAngle(-rel) <= -delta;
    }
    if (inside) {
        best = std::min(best, side * axisSide(axis, c) - R);
    }
    return best;
}

// A bound on how far any tessellation edge of a spline can stray from its own
// chord: the curve between two samples of one span is a Bezier sub-interval of
// parameter length at most 1/kSplineSegmentsPerSpan, whose distance from its
// chord is at most h^2/8 * max|B''|, with |B''| <= 6 * the largest second
// difference of the span's control points.
double splineChordDeviationBound(const SketchSpline& spline) {
    double worst = 0.0;
    const uint32_t spans = spline.points.size() >= 2
                                   ? static_cast<uint32_t>(spline.points.size() - 1u)
                                   : 0u;
    for (uint32_t i = 0; i < spans; ++i) {
        SketchBezierSpan span;
        if (!sketchSplineSpan(spline, i, &span)) {
            continue;
        }
        const double d0u = span.p0.u - 2.0 * span.c1.u + span.c2.u;
        const double d0v = span.p0.v - 2.0 * span.c1.v + span.c2.v;
        const double d1u = span.c1.u - 2.0 * span.c2.u + span.p3.u;
        const double d1v = span.c1.v - 2.0 * span.c2.v + span.p3.v;
        worst = std::max(worst, std::max(std::hypot(d0u, d0v), std::hypot(d1u, d1v)));
    }
    const double h = 1.0 / static_cast<double>(kSplineSegmentsPerSpan);
    return h * h / 8.0 * 6.0 * worst;
}

// The closest the true curve under one curved polygon edge comes to the axis on
// `side`, or the polygon value when the edge's entity is not a curve this build
// knows how to bound.
double curvedEdgeClosestApproach(const CadSketch& sketch, const RevolveAxis2D& axis, int side,
                                 SketchEntityId entityId, const SketchPoint& a,
                                 const SketchPoint& b) {
    const double ra = side * axisSide(axis, a);
    const double rb = side * axisSide(axis, b);
    const SketchEntity* entity = findSketchEntity(sketch, entityId);
    if (entity == nullptr) {
        return std::min(ra, rb);
    }
    if (const SketchCircle* circle = entity->circle()) {
        return arcClosestApproach(axis, side, circle->center, circle->radius, a, b);
    }
    if (const SketchArc* arc = entity->arc()) {
        SketchPoint centre{};
        double radius = 0.0;
        double start = 0.0;
        double sweep = 0.0;
        if (arcGeometry(*arc, &centre, &radius, &start, &sweep) == CadStatus::Ok) {
            return arcClosestApproach(axis, side, centre, radius, a, b);
        }
        return std::min(ra, rb);
    }
    if (const SketchSpline* spline = entity->spline()) {
        // Conservative: a spline whose samples stand closer to the axis than
        // its own deviation bound is not PROVEN to stay clear, and is refused
        // rather than assumed to.
        return std::min(ra, rb) - splineChordDeviationBound(*spline);
    }
    return std::min(ra, rb);
}

bool inComponentMaterial(const SketchPoint& p, const std::vector<std::vector<SketchPoint>>& loops) {
    if (loops.empty() || !sketchPointStrictlyInside(p, loops.front())) {
        return false;
    }
    for (size_t h = 1; h < loops.size(); ++h) {
        if (sketchPointStrictlyInside(p, loops[h])) {
            return false;
        }
    }
    return true;
}

// Whether two components' areas overlap or touch, by their loops: any two
// edges meeting, or one standing in the other's material.
bool componentsMeet(const std::vector<std::vector<SketchPoint>>& a,
                    const std::vector<std::vector<SketchPoint>>& b) {
    for (const std::vector<SketchPoint>& la : a) {
        for (size_t i = 0; i < la.size(); ++i) {
            const SketchPoint& a0 = la[i];
            const SketchPoint& a1 = la[(i + 1) % la.size()];
            for (const std::vector<SketchPoint>& lb : b) {
                for (size_t j = 0; j < lb.size(); ++j) {
                    if (sketchSegmentsIntersect(a0, a1, lb[j], lb[(j + 1) % lb.size()])) {
                        return true;
                    }
                }
            }
        }
    }
    return (!a.empty() && !a.front().empty() && inComponentMaterial(a.front().front(), b))
           || (!b.empty() && !b.front().empty() && inComponentMaterial(b.front().front(), a));
}

double signedAreaTwice(const std::vector<double>& t, const std::vector<double>& r) {
    double twice = 0.0;
    const size_t n = t.size();
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n;
        twice += t[i] * r[j] - t[j] * r[i];
    }
    return twice;
}

// One loop mapped into the sweep's (t, r) half-plane and oriented: the outer
// counter-clockwise, every hole clockwise, so the material is on the LEFT of
// every directed edge.
struct SweepLoop {
    std::vector<double> t;
    std::vector<double> r;
    std::vector<uint8_t> onAxis;
    std::vector<uint32_t> edgeTag;
};

SweepLoop mapLoop(const RevolveAxis2D& axis, int side, const RevolveLoop& loop, bool wantCcw) {
    SweepLoop out;
    const size_t n = loop.polygon.size();
    out.t.resize(n);
    out.r.resize(n);
    out.onAxis.resize(n);
    for (size_t i = 0; i < n; ++i) {
        out.t[i] = axisAlong(axis, loop.polygon[i]);
        double r = side * axisSide(axis, loop.polygon[i]);
        // ON the axis is ON the axis: the vertex becomes one exact apex rather
        // than a ring of copies a micrometre apart.
        if (std::fabs(r) <= kRevolveAxisToleranceMeters) {
            r = 0.0;
            out.onAxis[i] = 1u;
        } else {
            out.onAxis[i] = 0u;
        }
        out.r[i] = r;
    }
    out.edgeTag = loop.edgeTag;
    const bool ccw = signedAreaTwice(out.t, out.r) > 0.0;
    if (ccw != wantCcw && n > 0) {
        // Reverse the walk. New edge j runs old vertex n-1-j -> n-2-j, which is
        // old edge (n-2-j) mod n walked backwards, and keeps that edge's tag.
        SweepLoop reversed;
        reversed.t.resize(n);
        reversed.r.resize(n);
        reversed.onAxis.resize(n);
        reversed.edgeTag.resize(n);
        for (size_t j = 0; j < n; ++j) {
            reversed.t[j] = out.t[n - 1 - j];
            reversed.r[j] = out.r[n - 1 - j];
            reversed.onAxis[j] = out.onAxis[n - 1 - j];
            reversed.edgeTag[j] = out.edgeTag[(2 * n - 2 - j) % n];
        }
        out = std::move(reversed);
    }
    return out;
}

}  // namespace

uint32_t revolveSegmentCount(double angleDegrees) {
    return sketchArcSegmentCount(angleDegrees * kPi / 180.0);
}

RevolveFrame revolveFrame(const CadFrame64& placement, const RevolveAxis2D& axis,
                          RevolveDirection direction) {
    RevolveFrame frame;
    frame.origin = cadFramePoint(placement, axis.start.u, axis.start.v, 0.0);
    frame.axis = dvec3Add(dvec3Scale(placement.u, axis.du), dvec3Scale(placement.v, axis.dv));
    frame.radial = dvec3Add(dvec3Scale(placement.u, -axis.dv), dvec3Scale(placement.v, axis.du));
    const double sense = direction == RevolveDirection::Negative ? -1.0 : 1.0;
    frame.tangent = dvec3Scale(dvec3Cross(frame.axis, frame.radial), sense);
    return frame;
}

CadStatus classifyRevolveComponents(const CadSketch& sketch, const RevolveAxis2D& axis,
                                    double angleDegrees,
                                    const std::vector<RevolveComponent>& components,
                                    std::vector<int>* outSides) {
    std::vector<int> sides;
    sides.reserve(components.size());
    for (const RevolveComponent& component : components) {
        bool positive = false;
        bool negative = false;
        for (const RevolveLoop& loop : component.loops) {
            for (const SketchPoint& p : loop.polygon) {
                const double rho = axisSide(axis, p);
                positive = positive || rho > kRevolveAxisToleranceMeters;
                negative = negative || rho < -kRevolveAxisToleranceMeters;
            }
        }
        if (positive && negative) {
            return CadStatus::RevolveProfileCrossesAxis;
        }
        if (!positive && !negative) {
            return CadStatus::RevolveZeroRadius;
        }
        const int side = positive ? 1 : -1;
        // Every vertex is clear (or on the axis); every CURVED edge must be too,
        // judged on the true curve rather than its chord.
        for (const RevolveLoop& loop : component.loops) {
            const size_t n = loop.polygon.size();
            for (size_t k = 0; k < n; ++k) {
                if (k >= loop.edgeCurved.size() || loop.edgeCurved[k] == 0u) {
                    continue;
                }
                const SketchEntityId entity = k < loop.edgeEntity.size() ? loop.edgeEntity[k]
                                                                         : kNoSketchEntity;
                const double closest = curvedEdgeClosestApproach(
                        sketch, axis, side, entity, loop.polygon[k], loop.polygon[(k + 1) % n]);
                if (closest < -kRevolveAxisToleranceMeters) {
                    return CadStatus::RevolveProfileCrossesAxis;
                }
            }
        }
        sides.push_back(side);
    }
    // Opposite-side components sweep the same space from opposite ends once the
    // sweep reaches half a turn: mirrored across the axis they must not meet,
    // or the result would need a boolean union R1 does not make.
    if (angleDegrees >= 180.0) {
        for (size_t a = 0; a < components.size(); ++a) {
            for (size_t b = 0; b < components.size(); ++b) {
                if (sides[a] <= 0 || sides[b] >= 0) {
                    continue;
                }
                std::vector<std::vector<SketchPoint>> left;
                std::vector<std::vector<SketchPoint>> mirrored;
                for (const RevolveLoop& loop : components[a].loops) left.push_back(loop.polygon);
                for (const RevolveLoop& loop : components[b].loops) {
                    std::vector<SketchPoint> m;
                    m.reserve(loop.polygon.size());
                    for (const SketchPoint& p : loop.polygon) {
                        const double rho = axisSide(axis, p);
                        m.push_back(SketchPoint{p.u + 2.0 * rho * axis.dv, p.v - 2.0 * rho * axis.du});
                    }
                    mirrored.push_back(std::move(m));
                }
                if (componentsMeet(left, mirrored)) {
                    return CadStatus::RevolveComponentsOverlap;
                }
            }
        }
    }
    if (outSides != nullptr) {
        *outSides = std::move(sides);
    }
    return CadStatus::Ok;
}

CadStatus appendRevolveSolid(const CadFrame64& placement, const RevolveAxis2D& axis,
                             double angleDegrees, RevolveDirection direction,
                             const std::vector<RevolveComponent>& components,
                             const std::vector<int>& sides, uint32_t startCapTag,
                             uint32_t endCapTag, CadSolid* solid) {
    if (solid == nullptr || sides.size() != components.size()) {
        return CadStatus::RegenerationFailed;
    }
    const bool full = angleDegrees == kMaxRevolveAngleDegrees;
    const uint32_t segments = revolveSegmentCount(angleDegrees);
    const double angle = full ? kTwoPi : angleDegrees * kPi / 180.0;
    const uint32_t rings = full ? segments : segments + 1u;
    // The angles, once: a full turn's last ring is its first, so no seam vertex
    // is ever emitted twice; a partial turn ends EXACTLY at the authored angle.
    std::vector<double> cosines(rings);
    std::vector<double> sines(rings);
    for (uint32_t k = 0; k < rings; ++k) {
        const double theta = (!full && k == segments)
                                     ? angle
                                     : angle * static_cast<double>(k) / static_cast<double>(segments);
        cosines[k] = k == 0 ? 1.0 : std::cos(theta);
        sines[k] = k == 0 ? 0.0 : std::sin(theta);
    }
    const double sense = direction == RevolveDirection::Negative ? -1.0 : 1.0;
    const DVec3 origin = cadFramePoint(placement, axis.start.u, axis.start.v, 0.0);
    const DVec3 axisDir = dvec3Add(dvec3Scale(placement.u, axis.du), dvec3Scale(placement.v, axis.dv));
    const DVec3 leftNormal =
            dvec3Add(dvec3Scale(placement.u, -axis.dv), dvec3Scale(placement.v, axis.du));

    CadSolid out = *solid;
    for (size_t c = 0; c < components.size(); ++c) {
        const int side = sides[c];
        const DVec3 radial = dvec3Scale(leftNormal, static_cast<double>(side));
        // Built right-handed (D, E1, D x E1), in which every face below winds
        // outward; a Negative sweep mirrors the parametrization and is turned
        // back by reversing this component's triangles at the end.
        const DVec3 tangent = dvec3Cross(axisDir, radial);
        const auto point = [&](double t, double r, uint32_t k) {
            const double rc = r * cosines[k];
            const double rs = r * sines[k] * sense;
            return dvec3Add(dvec3Add(origin, dvec3Scale(axisDir, t)),
                            dvec3Add(dvec3Scale(radial, rc), dvec3Scale(tangent, rs)));
        };
        const auto push = [&out](const DVec3& p) {
            out.positions.insert(out.positions.end(), {p.x, p.y, p.z});
            return out.vertexCount() - 1u;
        };
        const size_t firstTriangle = out.faceTags.size();

        std::vector<SweepLoop> loops;
        for (size_t l = 0; l < components[c].loops.size(); ++l) {
            loops.push_back(mapLoop(axis, side, components[c].loops[l], /*wantCcw=*/l == 0u));
        }
        // Vertex rings; an on-axis vertex is an apex (two on a full turn when
        // two fans meet there, so each fan closes on its own disc).
        std::vector<std::vector<uint32_t>> ringBase(loops.size());
        std::vector<std::vector<uint32_t>> apexIn(loops.size());
        std::vector<std::vector<uint32_t>> apexOut(loops.size());
        for (size_t l = 0; l < loops.size(); ++l) {
            const SweepLoop& loop = loops[l];
            const size_t n = loop.t.size();
            ringBase[l].assign(n, 0u);
            apexIn[l].assign(n, 0u);
            apexOut[l].assign(n, 0u);
            for (size_t i = 0; i < n; ++i) {
                if (loop.onAxis[i] == 0u) {
                    ringBase[l][i] = out.vertexCount();
                    for (uint32_t k = 0; k < rings; ++k) {
                        push(point(loop.t[i], loop.r[i], k));
                    }
                    continue;
                }
                const DVec3 apex = point(loop.t[i], 0.0, 0);
                const bool inFan = loop.onAxis[(i + n - 1) % n] == 0u;
                const bool outFan = loop.onAxis[(i + 1) % n] == 0u;
                apexIn[l][i] = push(apex);
                apexOut[l][i] = (full && inFan && outFan) ? push(apex) : apexIn[l][i];
            }
        }
        const auto ring = [&](size_t l, size_t i, uint32_t k) {
            return ringBase[l][i] + (full ? k % segments : k);
        };
        // The swept surfaces: one strip per boundary edge, material on the left.
        for (size_t l = 0; l < loops.size(); ++l) {
            const SweepLoop& loop = loops[l];
            const size_t n = loop.t.size();
            for (size_t i = 0; i < n; ++i) {
                const size_t j = (i + 1) % n;
                const bool iAxis = loop.onAxis[i] != 0u;
                const bool jAxis = loop.onAxis[j] != 0u;
                if (iAxis && jAxis) {
                    continue;  // an edge ON the axis sweeps nothing
                }
                const uint32_t tag = loop.edgeTag[i];
                for (uint32_t k = 0; k < segments; ++k) {
                    const uint32_t k1 = k + 1u;
                    if (iAxis) {
                        out.indices.insert(out.indices.end(),
                                           {apexOut[l][i], ring(l, j, k), ring(l, j, k1)});
                        out.faceTags.push_back(tag);
                    } else if (jAxis) {
                        out.indices.insert(out.indices.end(),
                                           {ring(l, i, k), apexIn[l][j], ring(l, i, k1)});
                        out.faceTags.push_back(tag);
                    } else {
                        out.indices.insert(out.indices.end(),
                                           {ring(l, i, k), ring(l, j, k), ring(l, j, k1),
                                            ring(l, i, k), ring(l, j, k1), ring(l, i, k1)});
                        out.faceTags.push_back(tag);
                        out.faceTags.push_back(tag);
                    }
                }
            }
        }
        // The two caps of a partial sweep: the profile itself at theta = 0
        // (facing back along the sweep) and at the end angle (facing forward).
        if (!full) {
            std::vector<std::vector<SketchPoint>> polys;
            for (const SweepLoop& loop : loops) {
                std::vector<SketchPoint> poly;
                for (size_t i = 0; i < loop.t.size(); ++i) {
                    poly.push_back(SketchPoint{loop.t[i], loop.r[i]});
                }
                polys.push_back(std::move(poly));
            }
            std::vector<uint32_t> cap;
            if (polys.size() == 1u) {
                if (triangulateSimplePolygon(polys[0], &cap) != CadStatus::Ok) {
                    return CadStatus::TriangulationFailed;
                }
            } else if (cadKernelTriangulateRegion(polys, &cap) != CadKernelStatus::Ok) {
                return CadStatus::TriangulationFailed;
            }
            // Concatenated (loop, vertex) -> mesh vertex, at the first and last ring.
            std::vector<uint32_t> startVertex;
            std::vector<uint32_t> endVertex;
            for (size_t l = 0; l < loops.size(); ++l) {
                for (size_t i = 0; i < loops[l].t.size(); ++i) {
                    const bool onAxis = loops[l].onAxis[i] != 0u;
                    startVertex.push_back(onAxis ? apexIn[l][i] : ring(l, i, 0u));
                    endVertex.push_back(onAxis ? apexIn[l][i] : ring(l, i, segments));
                }
            }
            for (size_t t = 0; t + 2 < cap.size(); t += 3) {
                out.indices.insert(out.indices.end(), {startVertex[cap[t]], startVertex[cap[t + 2]],
                                                       startVertex[cap[t + 1]]});
                out.faceTags.push_back(startCapTag);
            }
            for (size_t t = 0; t + 2 < cap.size(); t += 3) {
                out.indices.insert(out.indices.end(), {endVertex[cap[t]], endVertex[cap[t + 1]],
                                                       endVertex[cap[t + 2]]});
                out.faceTags.push_back(endCapTag);
            }
        }
        if (sense < 0.0) {
            for (size_t t = firstTriangle; t < out.faceTags.size(); ++t) {
                std::swap(out.indices[t * 3u + 1u], out.indices[t * 3u + 2u]);
            }
        }
    }
    for (double value : out.positions) {
        if (!std::isfinite(value)) {
            return CadStatus::RegenerationFailed;
        }
    }
    *solid = std::move(out);
    return CadStatus::Ok;
}

}  // namespace forgeshape
