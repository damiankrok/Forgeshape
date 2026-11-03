#include "forgeshape_cad_hole.h"

#include "forgeshape_cad_edge_finish.h"

#include <algorithm>
#include <cmath>

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

// The circle density every curve in the product is tessellated at.
constexpr uint32_t kHoleSegments = kSketchCircleSegments;

bool inRange(double value, double lo, double hi) {
    return std::isfinite(value) && value >= lo && value <= hi;
}

// The 1/2/5 x 10^k value at or above `v`.
double niceAtLeast(double v) {
    if (!(v > 0.0) || !std::isfinite(v)) return 0.0;
    const double decade = std::pow(10.0, std::floor(std::log10(v)));
    for (double m : {1.0, 2.0, 5.0, 10.0}) {
        if (m * decade >= v * (1.0 - 1e-9)) return m * decade;
    }
    return 10.0 * decade;
}

// How far above the face the tool starts, and how far past the body a
// Through All tool runs: enough for the kernel to see a clean opening and a
// clean exit, small beside the hole.
double holeMargin(const CadFeatureHole& hole) {
    const double widest = std::max({hole.diameter, hole.boreDiameter, hole.sinkDiameter});
    return std::max(1.0e-5, 0.05 * widest);
}

// One ring of the tool's profile: `w` along the face normal (negative is into
// the material) and its radius.
struct HoleRing {
    double w = 0.0;
    double r = 0.0;
};

template <typename Change>
CadStatus applyHoleAct(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                       Change change) {
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

CadStatus validateCadHoleValues(const CadFeatureHole& h) {
    // Placed from edges or at a point -- one encoding each (`CAD-TO-FACE-R1`).
    if (h.byEdges) {
        if (h.u != 0.0 || h.v != 0.0 || !inRange(h.edgeDistanceA, kMinHoleDepth, kMaxHolePosition)
            || !inRange(h.edgeDistanceB, kMinHoleDepth, kMaxHolePosition)
            || sameCadFaceToken(h.edgeFaceA, h.edgeFaceB)) {
            return CadStatus::HoleInvalid;
        }
    } else if (!sameCadFaceToken(h.edgeFaceA, CadFaceToken{})
               || !sameCadFaceToken(h.edgeFaceB, CadFaceToken{}) || h.edgeDistanceA != 0.0
               || h.edgeDistanceB != 0.0) {
        return CadStatus::HoleInvalid;
    }
    if (!inRange(h.u, -kMaxHolePosition, kMaxHolePosition)
        || !inRange(h.v, -kMaxHolePosition, kMaxHolePosition)
        || !inRange(h.diameter, kMinHoleDiameter, kMaxHoleDiameter)) {
        return CadStatus::HoleInvalid;
    }
    // Through All stores no depth; a blind hole states one.
    if (h.throughAll ? h.depth != 0.0 : !inRange(h.depth, kMinHoleDepth, kMaxHoleDepth)) {
        return CadStatus::HoleInvalid;
    }
    const bool noBore = h.boreDiameter == 0.0 && h.boreDepth == 0.0;
    const bool noSink = h.sinkDiameter == 0.0 && h.sinkAngleDegrees == 0.0;
    switch (h.kind) {
        case CadHoleKind::Simple:
            return noBore && noSink ? CadStatus::Ok : CadStatus::HoleInvalid;
        case CadHoleKind::Counterbore:
            if (!noSink || !(h.boreDiameter > h.diameter)
                || !inRange(h.boreDiameter, kMinHoleDiameter, kMaxHoleDiameter)
                || !inRange(h.boreDepth, kMinHoleDepth, kMaxHoleDepth)
                || (!h.throughAll && !(h.boreDepth < h.depth))) {
                return CadStatus::HoleInvalid;
            }
            return CadStatus::Ok;
        case CadHoleKind::Countersink: {
            if (!noBore || !(h.sinkDiameter > h.diameter)
                || !inRange(h.sinkDiameter, kMinHoleDiameter, kMaxHoleDiameter)
                || !inRange(h.sinkAngleDegrees, kMinCountersinkAngleDegrees,
                            kMaxCountersinkAngleDegrees)) {
                return CadStatus::HoleInvalid;
            }
            const double sinkDepth = cadHoleCountersinkDepth(h);
            if (!(sinkDepth >= kMinHoleDepth) || (!h.throughAll && !(sinkDepth < h.depth))) {
                return CadStatus::HoleInvalid;
            }
            return CadStatus::Ok;
        }
    }
    return CadStatus::HoleInvalid;
}

namespace {

// One edge of the hole's face as the face's own plane sees it: the edge face
// `token` of the support feature, and the in-plane equation of the line the
// two faces share -- a point at (u, v) stands `a·u + b·v + c` in from it,
// measured in the face's plane.
struct HoleEdgeLine {
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
};

CadStatus holeEdgeLine(const CadFeatureGeometry& feature, const CadFrame64& face,
                       const CadFaceToken& token, HoleEdgeLine* out) {
    for (const CadFeatureFace& other : feature.faces) {
        if (!sameCadFaceToken(other.token, token)) continue;
        // A curved side's frame is one facet's: no single line to measure from.
        if (!other.eligible) return CadStatus::HoleEdgeNotFound;
        // The other face's normal within the hole's face: zero when the two
        // are parallel, which share no edge.
        const double along = dvec3Dot(other.frame.n, face.n);
        const DVec3 inPlane = dvec3Sub(other.frame.n, dvec3Scale(face.n, along));
        const double s = std::sqrt(dvec3Dot(inPlane, inPlane));
        if (!(s > 1.0e-6)) return CadStatus::HoleEdgeNotFound;
        // The other face is material-outward, so the face's material lies on
        // the side its normal points away from: in = -dot(p - g, n) / s.
        out->a = -dvec3Dot(face.u, other.frame.n) / s;
        out->b = -dvec3Dot(face.v, other.frame.n) / s;
        out->c = -dvec3Dot(dvec3Sub(face.origin, other.frame.origin), other.frame.n) / s;
        return CadStatus::Ok;
    }
    return CadStatus::HoleEdgeNotFound;
}

const CadFeatureGeometry* holeSupportFeature(const std::vector<CadFeatureGeometry>& chain,
                                             const CadFeatureHole& hole) {
    for (const CadFeatureGeometry& g : chain) {
        if (g.featureId == hole.support.featureId) return &g;
    }
    return nullptr;
}

}  // namespace

CadStatus cadHolePoint(const std::vector<CadFeatureGeometry>& chain, const CadFeatureHole& hole,
                       double* outU, double* outV) {
    CadFrame64 face;
    if (!resolveCadFeatureSupportFrame(chain, hole.support, &face)) {
        return CadStatus::HoleSupportInvalid;
    }
    double u = hole.u;
    double v = hole.v;
    if (hole.byEdges) {
        const CadFeatureGeometry* feature = holeSupportFeature(chain, hole);
        if (feature == nullptr) return CadStatus::HoleSupportInvalid;
        if (sameCadFaceToken(hole.edgeFaceA, hole.support.face)
            || sameCadFaceToken(hole.edgeFaceB, hole.support.face)) {
            return CadStatus::HoleEdgeNotFound;
        }
        HoleEdgeLine first;
        HoleEdgeLine second;
        CadStatus why = holeEdgeLine(*feature, face, hole.edgeFaceA, &first);
        if (why != CadStatus::Ok) return why;
        why = holeEdgeLine(*feature, face, hole.edgeFaceB, &second);
        if (why != CadStatus::Ok) return why;
        // a·u + b·v + c = d for both edges: two lines, one point -- unless the
        // edges run the same way (each row is a unit normal, so the
        // determinant is the sine between them).
        const double det = first.a * second.b - second.a * first.b;
        if (!(std::fabs(det) > 1.0e-6)) return CadStatus::HoleEdgesParallel;
        const double r1 = hole.edgeDistanceA - first.c;
        const double r2 = hole.edgeDistanceB - second.c;
        u = (r1 * second.b - r2 * first.b) / det;
        v = (first.a * r2 - second.a * r1) / det;
        if (!std::isfinite(u) || !std::isfinite(v)) return CadStatus::HoleEdgesParallel;
    }
    if (outU != nullptr) *outU = u;
    if (outV != nullptr) *outV = v;
    return CadStatus::Ok;
}

bool cadHoleFrame(const std::vector<CadFeatureGeometry>& chain, const CadFeatureHole& hole,
                  CadFrame64* out) {
    CadFrame64 face;
    if (!resolveCadFeatureSupportFrame(chain, hole.support, &face)) {
        return false;
    }
    double u = 0.0;
    double v = 0.0;
    if (cadHolePoint(chain, hole, &u, &v) != CadStatus::Ok) {
        return false;
    }
    face.origin = cadFramePoint(face, u, v, 0.0);
    if (out != nullptr) *out = face;
    return true;
}

CadStatus suggestCadHoleEdges(const CadBodyState& state, const CadFeatureHole& hole,
                              CadFeatureHole* out) {
    if (out == nullptr) return CadStatus::HoleInvalid;
    std::vector<CadFeatureGeometry> chain;
    if (buildCadChainGeometry(state, &chain, nullptr, hole.support.featureId) != CadStatus::Ok) {
        return CadStatus::HoleSupportInvalid;
    }
    double u = 0.0;
    double v = 0.0;
    const CadStatus pointWhy = cadHolePoint(chain, hole, &u, &v);
    if (pointWhy != CadStatus::Ok) return pointWhy;
    CadFrame64 face;
    const CadFeatureGeometry* feature = holeSupportFeature(chain, hole);
    if (feature == nullptr || !resolveCadFeatureSupportFrame(chain, hole.support, &face)) {
        return CadStatus::HoleSupportInvalid;
    }
    // The real edges of the face: the support feature's own prism edges that
    // have the hole's face on one side.
    std::vector<CadFeatureEdge> edges;
    if (enumerateCadFeatureEdges(state, hole.support.featureId, &edges) != CadStatus::Ok) {
        return CadStatus::HoleEdgeNotFound;
    }
    struct Option {
        CadFaceToken token;
        HoleEdgeLine line;
        double distance;
    };
    std::vector<Option> options;
    for (const CadFeatureEdge& edge : edges) {
        CadFaceToken other{};
        if (sameCadFaceToken(edge.ref.faceA, hole.support.face)) {
            other = edge.ref.faceB;
        } else if (sameCadFaceToken(edge.ref.faceB, hole.support.face)) {
            other = edge.ref.faceA;
        } else {
            continue;
        }
        bool seen = false;
        for (const Option& o : options) seen = seen || sameCadFaceToken(o.token, other);
        if (seen) continue;
        HoleEdgeLine line;
        if (holeEdgeLine(*feature, face, other, &line) != CadStatus::Ok) continue;
        const double distance = line.a * u + line.b * v + line.c;
        // Only an edge the point stands inside of.
        if (!(distance > 0.0)) continue;
        options.push_back(Option{other, line, distance});
    }
    std::stable_sort(options.begin(), options.end(), [](const Option& x, const Option& y) {
        if (x.distance != y.distance) return x.distance < y.distance;
        return cadFaceTokenCode(x.token) < cadFaceTokenCode(y.token);
    });
    for (size_t i = 0; i < options.size(); ++i) {
        for (size_t j = i + 1; j < options.size(); ++j) {
            const double sine = options[i].line.a * options[j].line.b
                                - options[j].line.a * options[i].line.b;
            if (std::fabs(sine) < 0.2) continue;  // nearly the same way: try the next
            CadFeatureHole placed = hole;
            placed.byEdges = true;
            placed.u = 0.0;
            placed.v = 0.0;
            // The first edge is the one that reads first on the face: the
            // one whose distance runs most along the face's u.
            const bool swap = std::fabs(options[j].line.a) > std::fabs(options[i].line.a);
            const Option& a = swap ? options[j] : options[i];
            const Option& b = swap ? options[i] : options[j];
            placed.edgeFaceA = a.token;
            placed.edgeFaceB = b.token;
            placed.edgeDistanceA = a.distance;
            placed.edgeDistanceB = b.distance;
            *out = placed;
            return CadStatus::Ok;
        }
    }
    return CadStatus::HoleEdgeNotFound;
}

bool cadHoleFrame(const CadBodyState& state, const CadFeatureHole& hole, CadFrame64* out) {
    std::vector<CadFeatureGeometry> chain;
    if (buildCadChainGeometry(state, &chain, nullptr, hole.support.featureId) != CadStatus::Ok) {
        return false;
    }
    return cadHoleFrame(chain, hole, out);
}

CadStatus validateCadFeatureHoles(const CadBodyState& state, uint32_t* outFailedId) {
    auto fail = [outFailedId](CadStatus why, uint32_t id) {
        if (outFailedId != nullptr) *outFailedId = id;
        return why;
    };
    if (state.holes.empty()) {
        return CadStatus::Ok;
    }
    if (state.holes.size() > kMaxCadHoles) {
        return fail(CadStatus::TooManyHoles, state.holes.back().holeId);
    }
    uint32_t previous = kCadFeatureId;
    for (const CadFeatureHole& h : state.holes) {
        // Ids: ascending, in the shared id space, below the high-water mark,
        // and never a feature's, a finish's or a pattern's.
        if (h.holeId <= previous || h.holeId >= state.nextFeatureId) {
            return fail(CadStatus::HoleInvalid, h.holeId);
        }
        previous = h.holeId;
        for (const CadFeature& f : state.laterFeatures) {
            if (f.featureId == h.holeId) return fail(CadStatus::HoleInvalid, h.holeId);
        }
        for (const CadEdgeFinish& f : state.edgeFinishes) {
            if (f.finishId == h.holeId) return fail(CadStatus::HoleInvalid, h.holeId);
        }
        for (const CadFeaturePattern& p : state.patterns) {
            if (p.patternId == h.holeId) return fail(CadStatus::HoleInvalid, h.holeId);
        }
        const CadStatus own = validateCadHoleValues(h);
        if (own != CadStatus::Ok) {
            return fail(own, h.holeId);
        }
        if (h.support.featureId >= h.holeId) {
            return fail(CadStatus::HoleSupportInvalid, h.holeId);
        }
    }
    // Every support resolves on the chain as it derives -- one walk for all.
    std::vector<CadFeatureGeometry> chain;
    uint32_t failedFeature = 0;
    const CadStatus chainWhy = buildCadChainGeometry(state, &chain, &failedFeature);
    if (chainWhy != CadStatus::Ok) {
        return fail(chainWhy, failedFeature);
    }
    for (const CadFeatureHole& h : state.holes) {
        const CadStatus pointWhy = cadHolePoint(chain, h, nullptr, nullptr);
        if (pointWhy != CadStatus::Ok) {
            return fail(pointWhy, h.holeId);
        }
    }
    return CadStatus::Ok;
}

CadStatus buildCadHoleTool(const CadFeatureHole& hole, const CadFrame64& frame,
                           const CadSolid& body, uint32_t tag, CadSolid* out) {
    if (out == nullptr || validateCadHoleValues(hole) != CadStatus::Ok) {
        return CadStatus::HoleInvalid;
    }
    const double margin = holeMargin(hole);
    const double r = 0.5 * hole.diameter;
    // The deepest the step at the top reaches, and how deep the bore goes.
    double step = 0.0;
    if (hole.kind == CadHoleKind::Counterbore) step = hole.boreDepth;
    if (hole.kind == CadHoleKind::Countersink) step = cadHoleCountersinkDepth(hole);
    double depth = hole.depth;
    if (hole.throughAll) {
        // As far as the body reaches below the face, measured on the body as
        // it stands at this point of the chain: a later Add is not pierced.
        double below = -INFINITY;
        for (size_t i = 0; i + 2 < body.positions.size(); i += 3) {
            const DVec3 p{body.positions[i], body.positions[i + 1], body.positions[i + 2]};
            below = std::max(below, dvec3Dot(dvec3Sub(frame.origin, p), frame.n));
        }
        if (!(below > 0.0)) {
            return CadStatus::HoleNoEffect;
        }
        depth = std::max(below, step) + margin;
    }
    // The profile, top to bottom.
    std::vector<HoleRing> rings;
    switch (hole.kind) {
        case CadHoleKind::Simple:
            rings = {{margin, r}, {-depth, r}};
            break;
        case CadHoleKind::Counterbore: {
            const double big = 0.5 * hole.boreDiameter;
            rings = {{margin, big}, {-hole.boreDepth, big}, {-hole.boreDepth, r}, {-depth, r}};
            break;
        }
        case CadHoleKind::Countersink: {
            const double big = 0.5 * hole.sinkDiameter;
            const double slope = std::tan(0.5 * hole.sinkAngleDegrees * kPi / 180.0);
            rings = {{margin, big + margin * slope}, {-step, r}, {-depth, r}};
            break;
        }
    }
    const uint32_t n = kHoleSegments;
    CadSolid tool;
    const auto push = [&tool](const DVec3& p) {
        tool.positions.insert(tool.positions.end(), {p.x, p.y, p.z});
        return tool.vertexCount() - 1u;
    };
    const auto triangle = [&tool, tag](uint32_t a, uint32_t b, uint32_t c) {
        tool.indices.insert(tool.indices.end(), {a, b, c});
        tool.faceTags.push_back(tag);
    };
    std::vector<uint32_t> first(rings.size());
    for (size_t k = 0; k < rings.size(); ++k) {
        first[k] = tool.vertexCount();
        for (uint32_t j = 0; j < n; ++j) {
            const double theta = 2.0 * kPi * static_cast<double>(j) / static_cast<double>(n);
            push(cadFramePoint(frame, rings[k].r * std::cos(theta), rings[k].r * std::sin(theta),
                               rings[k].w));
        }
    }
    const uint32_t top = push(cadFramePoint(frame, 0.0, 0.0, rings.front().w));
    const uint32_t bottom = push(cadFramePoint(frame, 0.0, 0.0, rings.back().w));
    const size_t last = rings.size() - 1u;
    for (uint32_t j = 0; j < n; ++j) {
        const uint32_t jn = (j + 1u) % n;
        // u x v = n, so increasing angle is counter-clockwise seen from +n:
        // the top cap faces +n, the bottom cap -n.
        triangle(top, first[0] + j, first[0] + jn);
        triangle(bottom, first[last] + jn, first[last] + j);
        // Each band faces away from the axis (a wall) or down (a step's floor).
        for (size_t k = 0; k < last; ++k) {
            const uint32_t a = first[k] + j;
            const uint32_t b = first[k] + jn;
            const uint32_t c = first[k + 1] + jn;
            const uint32_t d = first[k + 1] + j;
            triangle(a, d, c);
            triangle(a, c, b);
        }
    }
    for (double value : tool.positions) {
        if (!std::isfinite(value)) {
            return CadStatus::RegenerationFailed;
        }
    }
    *out = std::move(tool);
    return CadStatus::Ok;
}

double suggestCadHoleDiameter(const CadBodyState& state) {
    CadFeatureGeometry g;
    CadSolid solid;
    if (buildCadFeatureGeometry(state, kCadFeatureId, &g) != CadStatus::Ok
        || appendCadFeatureSolid(g, 0u, &solid) != CadStatus::Ok || solid.positions.empty()) {
        return 0.0;
    }
    double lo[3] = {INFINITY, INFINITY, INFINITY};
    double hi[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (size_t i = 0; i + 2 < solid.positions.size(); i += 3) {
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(lo[a], solid.positions[i + a]);
            hi[a] = std::max(hi[a], solid.positions[i + a]);
        }
    }
    const double smallest = std::min({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
    const double d = niceAtLeast(0.2 * smallest);
    return std::clamp(d, kMinHoleDiameter, kMaxHoleDiameter);
}

double snapCadHolePosition(double value, double diameter) {
    const double step = niceAtLeast(0.25 * diameter);
    if (!(step > 0.0) || !std::isfinite(value)) return value;
    const double snapped = std::round(value / step) * step;
    // -0.0 and 0.0 are two encodings of one point.
    return snapped == 0.0 ? 0.0 : snapped;
}

CadStatus addCadFeatureHoleToBody(ConstructionScene& scene, ConstructionHistory& history,
                                  ObjectId bodyId, CadFeatureHole hole, uint32_t* outHoleId) {
    uint32_t minted = 0;
    const CadStatus why = applyHoleAct(scene, history, bodyId, [&](CadBodyState* next) {
        if (next->holes.size() >= kMaxCadHoles) {
            return CadStatus::TooManyHoles;
        }
        minted = appendCadFeatureHole(next, std::move(hole));
        return minted != 0 ? CadStatus::Ok : CadStatus::HoleInvalid;
    });
    if (why == CadStatus::Ok && outHoleId != nullptr) {
        *outHoleId = minted;
    }
    return why;
}

CadStatus editCadFeatureHoleOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                   ObjectId bodyId, uint32_t holeId, CadFeatureHole hole) {
    return applyHoleAct(scene, history, bodyId, [&](CadBodyState* next) {
        for (CadFeatureHole& existing : next->holes) {
            if (existing.holeId == holeId) {
                hole.holeId = holeId;
                hole.support = existing.support;
                existing = std::move(hole);
                return CadStatus::Ok;
            }
        }
        return CadStatus::HoleNotFound;
    });
}

CadStatus deleteCadFeatureHoleOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                     ObjectId bodyId, uint32_t holeId) {
    return applyHoleAct(scene, history, bodyId, [&](CadBodyState* next) {
        for (size_t i = 0; i < next->holes.size(); ++i) {
            if (next->holes[i].holeId == holeId) {
                next->holes.erase(next->holes.begin() + static_cast<long>(i));
                return CadStatus::Ok;
            }
        }
        return CadStatus::HoleNotFound;
    });
}

}  // namespace forgeshape
