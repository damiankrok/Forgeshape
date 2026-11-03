#include "forgeshape_cad_edge_finish.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>

#include "forgeshape_cad_feature.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

// The circle density every curve in the product is tessellated at: a Fillet's
// arc takes its share of 32 steps per full turn, never fewer than two.
constexpr int kFilletStepsPerTurn = 32;

DVec3 solidPoint(const CadSolid& solid, uint32_t index) {
    return DVec3{solid.positions[index * 3u], solid.positions[index * 3u + 1u],
                 solid.positions[index * 3u + 2u]};
}

double dvec3Length(const DVec3& v) { return std::sqrt(dvec3Dot(v, v)); }

// Whether `featureId` names an Extrude that adds material: the only kind of
// feature whose own prism edges are the body's edges in R1.
CadStatus finishTargetStatus(const CadBodyState& state, uint32_t featureId) {
    CadFeatureView view;
    if (!findCadFeature(state, featureId, &view)) {
        return CadStatus::EdgeNotFound;
    }
    // A Cut's and an Intersect's edges are where its tool met the body, not
    // the tool's own prism edges (`CAD-INTERSECT-R1` keeps R1's rule).
    if (view.kind != CadFeatureKind::Extrude || view.operation == CadFeatureOperation::Cut
        || view.operation == CadFeatureOperation::Intersect) {
        return CadStatus::EdgeFinishTargetUnsupported;
    }
    return CadStatus::Ok;
}

// One face's triangles touching the edge: the vertex of each opposite the edge,
// which says which side of the edge the face lies on and how far it reaches.
struct EdgeSide {
    std::vector<DVec3> opposite;
};

}  // namespace

CadStatus enumerateCadFeatureEdges(const CadBodyState& state, uint32_t featureId,
                                   std::vector<CadFeatureEdge>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadFeatureView view;
    if (!findCadFeature(state, featureId, &view)) {
        return CadStatus::EdgeNotFound;
    }
    CadFeatureGeometry geometry;
    const CadStatus geometryWhy = buildCadFeatureGeometry(state, featureId, &geometry);
    if (geometryWhy != CadStatus::Ok) {
        return geometryWhy;
    }
    CadSolid prism;
    const CadStatus solidWhy = appendCadFeatureSolid(geometry, 0u, &prism);
    if (solidWhy != CadStatus::Ok) {
        return solidWhy;
    }
    const bool targetable = view.kind == CadFeatureKind::Extrude
                            && view.operation != CadFeatureOperation::Cut
                            && view.operation != CadFeatureOperation::Intersect;

    // Weld: one index per distinct position, so an edge two triangles share is
    // one key whether or not the generator shared the vertex.
    std::map<std::tuple<double, double, double>, uint32_t> weld;
    std::vector<uint32_t> canon(prism.vertexCount());
    for (uint32_t i = 0; i < prism.vertexCount(); ++i) {
        const DVec3 p = solidPoint(prism, i);
        const auto key = std::make_tuple(p.x, p.y, p.z);
        auto it = weld.find(key);
        if (it == weld.end()) {
            it = weld.emplace(key, i).first;
        }
        canon[i] = it->second;
    }

    // Every mesh edge: the tags of the triangles on it and their third vertex.
    struct MeshEdgeUse {
        uint32_t tag;
        uint32_t opposite;
    };
    std::map<std::pair<uint32_t, uint32_t>, std::vector<MeshEdgeUse>> uses;
    for (uint32_t t = 0; t < prism.triangleCount(); ++t) {
        const uint32_t v[3] = {canon[prism.indices[t * 3u]], canon[prism.indices[t * 3u + 1u]],
                               canon[prism.indices[t * 3u + 2u]]};
        for (int k = 0; k < 3; ++k) {
            const uint32_t a = v[k];
            const uint32_t b = v[(k + 1) % 3];
            const uint32_t c = v[(k + 2) % 3];
            if (a == b) continue;
            uses[std::make_pair(std::min(a, b), std::max(a, b))].push_back(
                    MeshEdgeUse{prism.faceTags[t], c});
        }
    }

    // Group the mesh edges that lie between two DIFFERENT faces by face pair.
    struct Group {
        std::vector<std::pair<uint32_t, uint32_t>> segments;
        EdgeSide sideA;
        EdgeSide sideB;
    };
    std::map<std::pair<uint32_t, uint32_t>, Group> groups;
    for (const auto& entry : uses) {
        if (entry.second.size() != 2u) {
            continue;  // a closed prism has exactly two; anything else is not an edge here
        }
        uint32_t ta = entry.second[0].tag;
        uint32_t tb = entry.second[1].tag;
        if (ta == tb) {
            continue;
        }
        uint32_t oa = entry.second[0].opposite;
        uint32_t ob = entry.second[1].opposite;
        if (ta > tb) {
            std::swap(ta, tb);
            std::swap(oa, ob);
        }
        Group& g = groups[std::make_pair(ta, tb)];
        g.segments.push_back(entry.first);
        g.sideA.opposite.push_back(solidPoint(prism, oa));
        g.sideB.opposite.push_back(solidPoint(prism, ob));
    }

    double scale = 1.0;
    for (uint32_t i = 0; i < prism.vertexCount(); ++i) {
        const DVec3 p = solidPoint(prism, i);
        scale = std::max(scale, std::max(std::fabs(p.x), std::max(std::fabs(p.y), std::fabs(p.z))));
    }
    const double tolerance = 1.0e-9 * scale;

    std::vector<CadFeatureEdge> edges;
    for (const auto& entry : groups) {
        const uint32_t ta = entry.first.first;
        const uint32_t tb = entry.first.second;
        if (ta >= geometry.faces.size() || tb >= geometry.faces.size()) {
            continue;
        }
        const CadFeatureFace& fa = geometry.faces[ta];
        const CadFeatureFace& fb = geometry.faces[tb];
        const Group& g = entry.second;

        CadFeatureEdge edge;
        edge.ref.featureId = featureId;
        edge.ref.faceA = fa.token;
        edge.ref.faceB = fb.token;
        const bool swapped = !cadEdgeRefCanonical(edge.ref);
        edge.ref = canonicalCadEdgeRef(edge.ref);
        const CadFeatureFace& faceA = swapped ? fb : fa;
        const CadFeatureFace& faceB = swapped ? fa : fb;
        const EdgeSide& sideA = swapped ? g.sideB : g.sideA;
        const EdgeSide& sideB = swapped ? g.sideA : g.sideB;
        edge.normalA = faceA.frame.n;
        edge.normalB = faceB.frame.n;

        // Straight: every piece lies on the line of the first.
        const DVec3 p0 = solidPoint(prism, g.segments.front().first);
        const DVec3 p1 = solidPoint(prism, g.segments.front().second);
        DVec3 d;
        bool straight = dvec3Normalized(dvec3Sub(p1, p0), &d);
        double tMin = 0.0;
        double tMax = 0.0;
        bool first = true;
        for (const auto& seg : g.segments) {
            for (const uint32_t vi : {seg.first, seg.second}) {
                const DVec3 p = solidPoint(prism, vi);
                const DVec3 rel = dvec3Sub(p, p0);
                const double t = dvec3Dot(rel, d);
                const DVec3 off = dvec3Sub(rel, dvec3Scale(d, t));
                if (dvec3Length(off) > tolerance) {
                    straight = false;
                }
                if (first) {
                    tMin = tMax = t;
                    first = false;
                } else {
                    tMin = std::min(tMin, t);
                    tMax = std::max(tMax, t);
                }
            }
        }
        edge.straight = straight && (tMax - tMin) > tolerance;
        if (edge.straight) {
            edge.start = dvec3Add(p0, dvec3Scale(d, tMin));
            edge.end = dvec3Add(p0, dvec3Scale(d, tMax));
            edge.direction = d;
            edge.length = tMax - tMin;
            // In each face: perpendicular to the edge, in the face's plane,
            // toward the triangle that stands on the edge.
            const auto inward = [&](const DVec3& n, const EdgeSide& side, DVec3* in,
                                    double* width) {
                DVec3 c;
                if (!dvec3Normalized(dvec3Cross(n, d), &c)) {
                    return false;
                }
                double best = 0.0;
                for (const DVec3& q : side.opposite) {
                    const double s = dvec3Dot(dvec3Sub(q, edge.start), c);
                    if (std::fabs(s) > std::fabs(best)) best = s;
                }
                if (std::fabs(best) <= tolerance) {
                    return false;
                }
                if (best < 0.0) {
                    c = dvec3Scale(c, -1.0);
                }
                *in = c;
                *width = std::fabs(best);
                return true;
            };
            const bool okA = inward(edge.normalA, sideA, &edge.inwardA, &edge.widthA);
            const bool okB = inward(edge.normalB, sideB, &edge.inwardB, &edge.widthB);
            if (okA && okB) {
                // Convex: stepping into face A goes BELOW face B's plane.
                edge.convex = dvec3Dot(edge.inwardA, edge.normalB) < -1.0e-12;
                const double c = std::max(-1.0, std::min(1.0, dvec3Dot(edge.inwardA, edge.inwardB)));
                edge.angleDegrees = std::acos(c) * 180.0 / kPi;
            } else {
                edge.straight = false;
            }
        }
        if (!targetable) {
            edge.why = CadStatus::EdgeFinishTargetUnsupported;
        } else if (!faceA.eligible || !faceB.eligible || !edge.straight || !edge.convex
                   || edge.angleDegrees < kMinEdgeFinishAngleDegrees) {
            edge.why = CadStatus::EdgeNotEligible;
        } else {
            edge.eligible = true;
            edge.why = CadStatus::Ok;
        }
        edges.push_back(edge);
    }
    *out = std::move(edges);
    return CadStatus::Ok;
}

CadStatus enumerateCadFinishableEdges(const CadBodyState& state,
                                      std::vector<CadFeatureEdge>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    std::vector<CadFeatureEdge> all;
    const uint32_t count = cadFeatureCount(state);
    for (uint32_t i = 0; i < count; ++i) {
        CadFeatureView view;
        cadFeatureAt(state, i, &view);
        if (finishTargetStatus(state, view.featureId) != CadStatus::Ok) {
            continue;
        }
        std::vector<CadFeatureEdge> edges;
        const CadStatus why = enumerateCadFeatureEdges(state, view.featureId, &edges);
        if (why != CadStatus::Ok) {
            return why;
        }
        for (const CadFeatureEdge& edge : edges) {
            if (edge.eligible) {
                all.push_back(edge);
            }
        }
    }
    *out = std::move(all);
    return CadStatus::Ok;
}

CadStatus resolveCadFinishEdge(const CadBodyState& state, const CadEdgeRef& ref,
                               CadFeatureEdge* out) {
    const CadStatus target = finishTargetStatus(state, ref.featureId);
    if (target != CadStatus::Ok) {
        return target;
    }
    std::vector<CadFeatureEdge> edges;
    const CadStatus why = enumerateCadFeatureEdges(state, ref.featureId, &edges);
    if (why != CadStatus::Ok) {
        return why;
    }
    const CadEdgeRef wanted = canonicalCadEdgeRef(ref);
    for (const CadFeatureEdge& edge : edges) {
        if (sameCadEdgeRef(edge.ref, wanted)) {
            if (out != nullptr) {
                *out = edge;
            }
            return edge.eligible ? CadStatus::Ok : edge.why;
        }
    }
    return CadStatus::EdgeNotFound;
}

double cadEdgeFinishSetback(CadEdgeFinishKind kind, double distance, double angleDegrees) {
    if (kind == CadEdgeFinishKind::Chamfer) {
        return distance;
    }
    const double half = angleDegrees * kPi / 360.0;
    const double tanHalf = std::tan(half);
    if (!(tanHalf > 1.0e-12) || !std::isfinite(tanHalf)) {
        return 0.0;
    }
    return distance / tanHalf;
}

CadStatus validateCadEdgeFinishes(const CadBodyState& state, uint32_t* outFailedId) {
    auto fail = [outFailedId](CadStatus why, uint32_t id) {
        if (outFailedId != nullptr) {
            *outFailedId = id;
        }
        return why;
    };
    if (state.edgeFinishes.empty()) {
        return CadStatus::Ok;
    }
    if (state.edgeFinishes.size() > kMaxCadEdgeFinishes) {
        return fail(CadStatus::TooManyEdgeFinishes, 0u);
    }
    uint32_t previous = 0;
    for (const CadEdgeFinish& finish : state.edgeFinishes) {
        const uint32_t id = finish.finishId;
        // A finish's id is a feature id's sibling: non-zero, ascending, below
        // the high-water mark and never one a feature holds.
        if (id == 0u || id <= previous || id >= state.nextFeatureId
            || findCadFeature(state, id, nullptr)) {
            return fail(CadStatus::EdgeFinishInvalid, id);
        }
        previous = id;
        if (cadEdgeFinishKindIndex(finish.kind) < 0
            || cadEdgeFinishKindIndex(finish.kind) >= kCadEdgeFinishKindCount) {
            return fail(CadStatus::EdgeFinishInvalid, id);
        }
        if (!std::isfinite(finish.distance) || finish.distance < kMinEdgeFinishDistance
            || finish.distance > kMaxEdgeFinishDistance) {
            return fail(CadStatus::EdgeFinishInvalid, id);
        }
        if (finish.edges.empty() || finish.edges.size() > kMaxEdgesPerFinish) {
            return fail(CadStatus::EdgeFinishInvalid, id);
        }
        for (size_t i = 0; i < finish.edges.size(); ++i) {
            const CadEdgeRef& edge = finish.edges[i];
            if (!cadEdgeRefCanonical(edge)) {
                return fail(CadStatus::EdgeFinishInvalid, id);
            }
            for (size_t j = 0; j < i; ++j) {
                if (sameCadEdgeRef(finish.edges[j], edge)) {
                    return fail(CadStatus::EdgeFinishInvalid, id);
                }
            }
            CadFeatureEdge resolved;
            const CadStatus why = resolveCadFinishEdge(state, edge, &resolved);
            if (why != CadStatus::Ok) {
                return fail(why, id);
            }
            const double setback =
                    cadEdgeFinishSetback(finish.kind, finish.distance, resolved.angleDegrees);
            const double room = std::min(resolved.widthA, resolved.widthB);
            if (!(setback > 0.0) || setback >= room * (1.0 - 1.0e-9)) {
                return fail(CadStatus::EdgeFinishTooLarge, id);
            }
        }
    }
    return CadStatus::Ok;
}

CadStatus cadFinishableEdgesOfFace(const CadBodyState& state, uint32_t featureId,
                                   const CadFaceToken& face, std::vector<CadEdgeRef>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    const CadStatus target = finishTargetStatus(state, featureId);
    if (target != CadStatus::Ok) {
        return target;
    }
    std::vector<CadFeatureEdge> edges;
    const CadStatus why = enumerateCadFeatureEdges(state, featureId, &edges);
    if (why != CadStatus::Ok) {
        return why;
    }
    out->clear();
    for (const CadFeatureEdge& edge : edges) {
        if (edge.eligible
            && (sameCadFaceToken(edge.ref.faceA, face) || sameCadFaceToken(edge.ref.faceB, face))
            && out->size() < kMaxEdgesPerFinish) {
            out->push_back(edge.ref);
        }
    }
    return CadStatus::Ok;
}

double suggestCadEdgeFinishDistance(CadEdgeFinishKind kind,
                                    const std::vector<CadFeatureEdge>& edges) {
    double room = 0.0;
    bool any = false;
    double angle = 90.0;
    for (const CadFeatureEdge& edge : edges) {
        const double fit = std::min(edge.widthA, edge.widthB);
        if (!any || fit < room) {
            room = fit;
            angle = edge.angleDegrees;
        }
        any = true;
    }
    if (!any || !(room > 0.0)) {
        return 0.0;
    }
    // The setback a fifth of the room; a Fillet's radius is that setback times
    // tan(alpha / 2).
    double target = 0.2 * room;
    if (kind == CadEdgeFinishKind::Fillet) {
        target *= std::tan(angle * kPi / 360.0);
    }
    // Round DOWN to 1, 2 or 5 x 10^k, so the value reads as typed.
    const double exponent = std::floor(std::log10(target));
    const double unit = std::pow(10.0, exponent);
    const double mantissa = target / unit;
    // A hair of slack, so tan(45 deg) = 0.9999999999999999 still reads as 1.
    const double slack = 1.0e-9;
    const double nice = mantissa >= 5.0 - slack ? 5.0 : (mantissa >= 2.0 - slack ? 2.0 : 1.0);
    const double value = nice * unit;
    if (!std::isfinite(value) || value < kMinEdgeFinishDistance) {
        return 0.0;
    }
    return std::min(value, kMaxEdgeFinishDistance);
}

double suggestCadEdgeFinishDistance(const CadBodyState& state, CadEdgeFinishKind kind,
                                    const std::vector<CadEdgeRef>& edges) {
    std::vector<CadFeatureEdge> resolved;
    for (const CadEdgeRef& ref : edges) {
        CadFeatureEdge edge;
        if (resolveCadFinishEdge(state, ref, &edge) == CadStatus::Ok) {
            resolved.push_back(edge);
        }
    }
    return suggestCadEdgeFinishDistance(kind, resolved);
}

CadStatus resolveCadFinishEdges(const CadBodyState& state, const std::vector<CadEdgeRef>& refs,
                                std::vector<CadFeatureEdge>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    std::map<uint32_t, std::vector<CadFeatureEdge>> byFeature;
    std::vector<CadFeatureEdge> resolved;
    for (const CadEdgeRef& ref : refs) {
        auto it = byFeature.find(ref.featureId);
        if (it == byFeature.end()) {
            const CadStatus target = finishTargetStatus(state, ref.featureId);
            if (target != CadStatus::Ok) {
                return target;
            }
            std::vector<CadFeatureEdge> edges;
            const CadStatus why = enumerateCadFeatureEdges(state, ref.featureId, &edges);
            if (why != CadStatus::Ok) {
                return why;
            }
            it = byFeature.emplace(ref.featureId, std::move(edges)).first;
        }
        const CadEdgeRef wanted = canonicalCadEdgeRef(ref);
        bool found = false;
        for (const CadFeatureEdge& edge : it->second) {
            if (sameCadEdgeRef(edge.ref, wanted)) {
                if (!edge.eligible) {
                    return edge.why;
                }
                resolved.push_back(edge);
                found = true;
                break;
            }
        }
        if (!found) {
            return CadStatus::EdgeNotFound;
        }
    }
    *out = std::move(resolved);
    return CadStatus::Ok;
}

namespace {

// The shared shape of the three acts: refuse an edit already open, find the CAD
// body, let `change` build the requested state from the current one, then one
// transaction around one `applyState`.
template <typename Change>
CadStatus applyEdgeFinishAct(ConstructionScene& scene, ConstructionHistory& history,
                             ObjectId bodyId, Change change) {
    if (history.editInProgress()) {
        return CadStatus::RefusedEditInProgress;
    }
    SceneObject* object = scene.findBody(bodyId);
    CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) {
        return CadStatus::NotCadBody;
    }
    CadBodyState next = body->state();
    const CadStatus built = change(&next);
    if (built != CadStatus::Ok) {
        return built;
    }
    const CadStatus valid = validateCadBodyState(next);
    if (valid != CadStatus::Ok) {
        return valid;
    }
    CadStatus why = CadStatus::Ok;
    {
        ScopedConstructionEdit edit(history);
        why = body->applyState(next);
        if (why == CadStatus::Ok) {
            publishSceneObject(*object);
        }
    }
    return why;
}

}  // namespace

CadStatus addCadEdgeFinishToBody(ConstructionScene& scene, ConstructionHistory& history,
                                 ObjectId bodyId, CadEdgeFinishKind kind, double distance,
                                 std::vector<CadEdgeRef> edges, uint32_t* outFinishId) {
    uint32_t minted = 0;
    const CadStatus why = applyEdgeFinishAct(scene, history, bodyId, [&](CadBodyState* next) {
        if (next->edgeFinishes.size() >= kMaxCadEdgeFinishes) {
            return CadStatus::TooManyEdgeFinishes;
        }
        minted = appendCadEdgeFinish(next, kind, distance, std::move(edges));
        return minted != 0 ? CadStatus::Ok : CadStatus::EdgeFinishInvalid;
    });
    if (why == CadStatus::Ok && outFinishId != nullptr) {
        *outFinishId = minted;
    }
    return why;
}

CadStatus editCadEdgeFinishOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                  ObjectId bodyId, uint32_t finishId, CadEdgeFinishKind kind,
                                  double distance) {
    return applyEdgeFinishAct(scene, history, bodyId, [&](CadBodyState* next) {
        for (CadEdgeFinish& finish : next->edgeFinishes) {
            if (finish.finishId == finishId) {
                finish.kind = kind;
                finish.distance = distance;
                return CadStatus::Ok;
            }
        }
        return CadStatus::EdgeNotFound;
    });
}

CadStatus deleteCadEdgeFinishOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                    ObjectId bodyId, uint32_t finishId) {
    return applyEdgeFinishAct(scene, history, bodyId, [&](CadBodyState* next) {
        for (size_t i = 0; i < next->edgeFinishes.size(); ++i) {
            if (next->edgeFinishes[i].finishId == finishId) {
                next->edgeFinishes.erase(next->edgeFinishes.begin() + static_cast<long>(i));
                return CadStatus::Ok;
            }
        }
        return CadStatus::EdgeNotFound;
    });
}

bool cadSolidContainsPoint(const CadSolid& solid, const DVec3& point) {
    // An irrational-looking direction, so the ray almost never grazes an edge
    // or a vertex of a generated prism; parity of the crossings.
    DVec3 dir;
    dvec3Normalized(DVec3{0.5377, 0.6411, 0.5466}, &dir);
    int crossings = 0;
    for (uint32_t t = 0; t < solid.triangleCount(); ++t) {
        const DVec3 a = solidPoint(solid, solid.indices[t * 3u]);
        const DVec3 b = solidPoint(solid, solid.indices[t * 3u + 1u]);
        const DVec3 c = solidPoint(solid, solid.indices[t * 3u + 2u]);
        const DVec3 e1 = dvec3Sub(b, a);
        const DVec3 e2 = dvec3Sub(c, a);
        const DVec3 p = dvec3Cross(dir, e2);
        const double det = dvec3Dot(e1, p);
        if (std::fabs(det) < 1.0e-18) continue;
        const double inv = 1.0 / det;
        const DVec3 s = dvec3Sub(point, a);
        const double u = dvec3Dot(s, p) * inv;
        if (u < 0.0 || u > 1.0) continue;
        const DVec3 q = dvec3Cross(s, e1);
        const double v = dvec3Dot(dir, q) * inv;
        if (v < 0.0 || u + v > 1.0) continue;
        const double dist = dvec3Dot(e2, q) * inv;
        if (dist > 1.0e-12) {
            ++crossings;
        }
    }
    return (crossings % 2) == 1;
}

bool cadEdgeIsOutsideEdgeOfBody(const CadFeatureEdge& edge, const CadSolid& body) {
    if (body.empty() || !(edge.length > 0.0)) {
        return false;
    }
    // A probe distance small beside the edge and its faces, so it stays on
    // this edge's faces and in this edge's corner.
    const double room = std::min({edge.widthA, edge.widthB, edge.length});
    const double e = std::max(1.0e-7, 0.05 * room);
    for (double s : {0.3, 0.5, 0.7}) {
        const DVec3 p = dvec3Add(edge.start, dvec3Scale(edge.direction, s * edge.length));
        // Just beyond face A and face B, a little in from the edge: open space
        // on an outside edge, material where the face is buried or flush.
        const DVec3 overA = dvec3Add(p, dvec3Add(dvec3Scale(edge.normalA, e),
                                                 dvec3Scale(edge.inwardA, 0.5 * e)));
        const DVec3 overB = dvec3Add(p, dvec3Add(dvec3Scale(edge.normalB, e),
                                                 dvec3Scale(edge.inwardB, 0.5 * e)));
        // In the corner the finish removes: material on an edge that is there.
        const DVec3 corner = dvec3Add(p, dvec3Scale(dvec3Add(edge.inwardA, edge.inwardB), 0.5 * e));
        if (cadSolidContainsPoint(body, overA) || cadSolidContainsPoint(body, overB)
            || !cadSolidContainsPoint(body, corner)) {
            return false;
        }
    }
    return true;
}

CadSolid cadSolidFromBodyMesh(const CadBodyMesh& mesh) {
    CadSolid solid;
    solid.positions.reserve(mesh.mesh.vertices.size() * 3u);
    for (const MeshVertex& v : mesh.mesh.vertices) {
        solid.positions.insert(solid.positions.end(),
                               {static_cast<double>(v.position[0]),
                                static_cast<double>(v.position[1]),
                                static_cast<double>(v.position[2])});
    }
    solid.indices = mesh.mesh.indices;
    solid.faceTags.assign(solid.indices.size() / 3u, 0u);
    return solid;
}

CadStatus buildCadEdgeFinishTool(const CadFeatureEdge& edge, CadEdgeFinishKind kind,
                                 double distance, const CadSolid& body, uint32_t tag,
                                 CadSolid* out) {
    if (out == nullptr || !edge.eligible) {
        return CadStatus::EdgeNotEligible;
    }
    if (!std::isfinite(distance) || distance < kMinEdgeFinishDistance
        || distance > kMaxEdgeFinishDistance) {
        return CadStatus::EdgeFinishInvalid;
    }
    const double alpha = edge.angleDegrees * kPi / 180.0;
    const double t = cadEdgeFinishSetback(kind, distance, edge.angleDegrees);
    if (!(t > 0.0) || t >= std::min(edge.widthA, edge.widthB) * (1.0 - 1.0e-9)) {
        return CadStatus::EdgeFinishTooLarge;
    }
    // The cross-section, as offsets from the edge point, perpendicular to it.
    const DVec3& nA = edge.normalA;
    const DVec3& nB = edge.normalB;
    const DVec3 A = dvec3Scale(edge.inwardA, t);
    const DVec3 B = dvec3Scale(edge.inwardB, t);
    const double k = t;  // how far outside the material the tool reaches
    std::vector<DVec3> ring;
    ring.push_back(A);
    ring.push_back(dvec3Add(A, dvec3Scale(nA, k)));
    ring.push_back(dvec3Scale(dvec3Add(nA, nB), k));
    ring.push_back(dvec3Add(B, dvec3Scale(nB, k)));
    ring.push_back(B);
    if (kind == CadEdgeFinishKind::Fillet) {
        DVec3 bisector;
        if (!dvec3Normalized(dvec3Add(edge.inwardA, edge.inwardB), &bisector)) {
            return CadStatus::EdgeNotEligible;
        }
        const double sinHalf = std::sin(alpha * 0.5);
        if (!(sinHalf > 1.0e-12)) {
            return CadStatus::EdgeNotEligible;
        }
        const DVec3 centre = dvec3Scale(bisector, distance / sinHalf);
        const DVec3 a = dvec3Sub(A, centre);
        const DVec3 b = dvec3Sub(B, centre);
        DVec3 e1;
        if (!dvec3Normalized(a, &e1)) {
            return CadStatus::EdgeNotEligible;
        }
        DVec3 e2;
        if (!dvec3Normalized(dvec3Sub(b, dvec3Scale(e1, dvec3Dot(b, e1))), &e2)) {
            return CadStatus::EdgeNotEligible;
        }
        const double sweep = kPi - alpha;
        const int steps = std::max(2, static_cast<int>(std::ceil(
                                              sweep / (2.0 * kPi) * kFilletStepsPerTurn)));
        // From B back to A, exclusive of both: the arc bulges toward the edge.
        for (int i = steps - 1; i >= 1; --i) {
            const double theta = sweep * static_cast<double>(i) / static_cast<double>(steps);
            ring.push_back(dvec3Add(centre, dvec3Add(dvec3Scale(e1, distance * std::cos(theta)),
                                                     dvec3Scale(e2, distance * std::sin(theta)))));
        }
    }

    // Per end: run on past the edge where the material ends there, stop flush
    // where it continues. Probed just beyond the end, inside the corner the
    // finish removes.
    const DVec3 corner = dvec3Scale(dvec3Add(edge.inwardA, edge.inwardB), 0.25 * t);
    const double probe = std::min(0.5 * t, 0.25 * edge.length);
    const DVec3 beyondStart =
            dvec3Add(dvec3Sub(edge.start, dvec3Scale(edge.direction, probe)), corner);
    const DVec3 beyondEnd = dvec3Add(dvec3Add(edge.end, dvec3Scale(edge.direction, probe)), corner);
    const double runStart = cadSolidContainsPoint(body, beyondStart) ? 0.0 : k;
    const double runEnd = cadSolidContainsPoint(body, beyondEnd) ? 0.0 : k;
    const DVec3 o0 = dvec3Sub(edge.start, dvec3Scale(edge.direction, runStart));
    const DVec3 o1 = dvec3Add(edge.end, dvec3Scale(edge.direction, runEnd));

    CadSolid tool;
    const uint32_t m = static_cast<uint32_t>(ring.size());
    auto push = [&tool](const DVec3& p) {
        tool.positions.push_back(p.x);
        tool.positions.push_back(p.y);
        tool.positions.push_back(p.z);
    };
    // 0: centre0, 1..m: ring0, m+1: centre1, m+2..2m+1: ring1.
    push(o0);
    for (const DVec3& r : ring) push(dvec3Add(o0, r));
    push(o1);
    for (const DVec3& r : ring) push(dvec3Add(o1, r));
    const uint32_t c0 = 0u;
    const uint32_t c1 = m + 1u;
    auto tri = [&tool, tag](uint32_t a, uint32_t b, uint32_t c) {
        tool.indices.insert(tool.indices.end(), {a, b, c});
        tool.faceTags.push_back(tag);
    };
    for (uint32_t i = 0; i < m; ++i) {
        const uint32_t j = (i + 1u) % m;
        tri(c0, 1u + j, 1u + i);
        tri(c1, c1 + 1u + i, c1 + 1u + j);
        tri(1u + i, 1u + j, c1 + 1u + j);
        tri(1u + i, c1 + 1u + j, c1 + 1u + i);
    }
    // Wind it outward whichever way the ring turned.
    if (cadSolidVolume(tool) < 0.0) {
        for (size_t i = 0; i < tool.indices.size(); i += 3) {
            std::swap(tool.indices[i + 1], tool.indices[i + 2]);
        }
    }
    *out = std::move(tool);
    return CadStatus::Ok;
}

}  // namespace forgeshape
