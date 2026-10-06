#include "forgeshape_cad_feature.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_cad_revolve.h"

namespace forgeshape {
namespace {

// FNV-1a over 64 bits -- the lineage-token FORMAT (DATA_PACKAGE_SPEC.md §7c).
// The same two constants forgeshape_cad_face.cpp has always used.
constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void mixU64(uint64_t& h, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (i * 8)) & 0xFFu;
        h *= kFnvPrime;
    }
}

CadFrame64 workplaneFrame64(Workplane plane) {
    const WorkplaneFrame f = workplaneFrame(plane);
    CadFrame64 out;
    // Every component of a principal frame is exactly 0 or +/-1, so the float
    // frame converts to double without rounding.
    out.origin = DVec3{0.0, 0.0, 0.0};
    out.u = dvec3FromVec3(f.uAxis);
    out.v = dvec3FromVec3(f.vAxis);
    out.n = dvec3FromVec3(f.normal);
    return out;
}

// A direction in sketch-local (du, dv, dw) mapped into body-local.
DVec3 frameDirection(const CadFrame64& f, double du, double dv, double dw) {
    return dvec3Add(dvec3Add(dvec3Scale(f.u, du), dvec3Scale(f.v, dv)), dvec3Scale(f.n, dw));
}

// The wall frame standing on the sketch segment a -> b, placed into the body.
// `hole` selects the inner-wall orientation: the material is OUTSIDE a hole,
// so its wall faces into the hole.
CadFrame64 sideFrame(const CadFeatureGeometry& g, const SketchPoint& a, const SketchPoint& b,
                     bool hole);

// The frame of a face as the BODY's surface sees it. A Cut's faces are the
// tool's faces turned inside out -- the pocket's floor faces up out of the
// material, its walls face into the pocket -- so their normal (and, to stay
// right-handed, their v) is reversed; u is kept so the in-face axis does not
// move. Every other feature's faces are the prism's own.
CadFrame64 materialOutward(const CadFeatureGeometry& g, CadFrame64 frame) {
    if (g.operation == CadFeatureOperation::Cut) {
        frame.n = dvec3Scale(frame.n, -1.0);
        frame.v = dvec3Scale(frame.v, -1.0);
    }
    return frame;
}

// The face of one loop edge in sketch-local terms, placed into the body.
CadFeatureFace sideFace(const CadFeatureGeometry& g, const ClosedProfile& loop, uint32_t k,
                        bool hole, bool lineageFeatureEligible) {
    const uint32_t n = static_cast<uint32_t>(loop.polygon.size());
    CadFeatureFace face;
    face.token.kind = CadFaceKind::Side;
    face.token.edgeEntityId = loop.edgeEntityId.size() == n ? loop.edgeEntityId[k] : loop.anchorEntityId;
    face.token.edgeLocalIndex = loop.edgeLocalIndex.size() == n ? loop.edgeLocalIndex[k] : k;
    const bool curved = loop.fromCircle || (loop.edgeCurved.size() == n && loop.edgeCurved[k] != 0u);
    face.eligible = g.kind == CadFeatureKind::Extrude && !curved;
    face.lineageEligible = lineageFeatureEligible && !curved;
    face.frame = materialOutward(g, sideFrame(g, loop.polygon[k], loop.polygon[(k + 1u) % n], hole));
    return face;
}

CadFrame64 sideFrame(const CadFeatureGeometry& g, const SketchPoint& a, const SketchPoint& b,
                     bool hole) {
    double eu = b.u - a.u;
    double ev = b.v - a.v;
    const double len = std::sqrt(eu * eu + ev * ev);
    if (len > 0.0) {
        eu /= len;
        ev /= len;
    } else {
        eu = 1.0;
        ev = 0.0;
    }
    // Local outward (in the sketch plane) and the local in-face axis.
    //   outer: outward = edge x N = ( ev, -eu), u =  edge
    //   hole:  outward = N x edge = (-ev,  eu), u = -edge
    const double ou = hole ? -ev : ev;
    const double ov = hole ? eu : -eu;
    const double uu = hole ? -eu : eu;
    const double uv = hole ? -ev : ev;
    CadFrame64 frame;
    frame.n = frameDirection(g.placement, ou, ov, 0.0);
    frame.u = frameDirection(g.placement, uu, uv, 0.0);
    frame.v = dvec3Cross(frame.n, frame.u);
    // The centre of the wall quad.
    const double mu = (a.u + b.u) * 0.5;
    const double mv = (a.v + b.v) * 0.5;
    const double mw = (g.planeCapOffset + g.farCapOffset) * 0.5;
    frame.origin = cadFramePoint(g.placement, mu, mv, mw);
    return frame;
}

CadFeatureFace capFace(const CadFeatureGeometry& g, const std::vector<SketchPoint>& primaryOuter,
                       bool onPlane, bool lineageFeatureEligible) {
    CadFeatureFace face;
    face.token.kind = onPlane ? CadFaceKind::CapPlane : CadFaceKind::CapFar;
    face.eligible = true;
    face.lineageEligible = lineageFeatureEligible;
    const double offset = onPlane ? g.planeCapOffset : g.farCapOffset;
    // The VERTEX MEAN of the first region's outer loop: what R0's cap frame
    // always used, kept so a face-supported dependent does not move.
    double su = 0.0;
    double sv = 0.0;
    for (const SketchPoint& p : primaryOuter) {
        su += p.u;
        sv += p.v;
    }
    const double inv = 1.0 / static_cast<double>(primaryOuter.size());
    face.frame.origin = cadFramePoint(g.placement, su * inv, sv * inv, offset);
    const double sign = onPlane ? -g.extrudeSign : g.extrudeSign;
    face.frame.n = frameDirection(g.placement, 0.0, 0.0, sign);
    face.frame.u = g.placement.u;
    face.frame.v = sign > 0.0 ? g.placement.v : dvec3Scale(g.placement.v, -1.0);
    face.frame = materialOutward(g, face.frame);
    return face;
}

// The two offsets and which cap is which, from the extrusion alone: the same
// sentence forgeshape_cad_face.cpp has always used -- CapPlane is the cap the
// extrusion grows FROM, CapFar the one it grows TO.
void setExtrudeOffsets(CadFeatureGeometry* g) {
    const double positive = extrudePositiveDistance(g->extrude);
    const double negative = extrudeNegativeDistance(g->extrude);
    g->nearOffset = -negative;
    g->farOffset = positive;
    if (g->extrude.direction == ExtrudeDirection::AlongNormal) {
        g->planeCapOffset = -negative;
        g->farCapOffset = positive;
        g->extrudeSign = 1.0;
    } else {
        g->planeCapOffset = positive;
        g->farCapOffset = -negative;
        g->extrudeSign = -1.0;
    }
}

// The lineage signature, on the §7c rule: the selection's anchor (0 for a face
// selection), the face count, then every face's token code and eligibility.
uint64_t featureSignature(const CadFeatureGeometry& g) {
    uint64_t h = kFnvOffset;
    mixU64(h, static_cast<uint64_t>(g.extrude.profileEntityId));
    mixU64(h, g.faces.size());
    for (const CadFeatureFace& face : g.faces) {
        mixU64(h, cadFaceTokenCode(face.token));
        mixU64(h, face.lineageEligible ? 1u : 0u);
    }
    return h == 0 ? 1 : h;
}

bool framesFinite(const CadFeatureGeometry& g) {
    for (const CadFeatureFace& face : g.faces) {
        if (!dvec3Finite(face.frame.origin) || !dvec3Finite(face.frame.u)
            || !dvec3Finite(face.frame.v) || !dvec3Finite(face.frame.n)) {
            return false;
        }
    }
    return true;
}

// The side token of a union-boundary fragment: the whole-edge token when the
// fragment IS its source edge, a fragment token otherwise (`CadFaceToken`).
CadFaceToken fragmentSideToken(const FragmentRef& fragment) {
    CadFaceToken token;
    token.kind = CadFaceKind::Side;
    token.edgeEntityId = fragment.sourceEntityId;
    token.edgeLocalIndex = fragment.sourceEdgeLocalIndex;
    const bool whole = fragment.startCut.kind == ArrangementCutKind::SourceStart
                       && fragment.endCut.kind == ArrangementCutKind::SourceEnd;
    if (!whole) {
        token.fragment = true;
        token.fragmentStart = fragment.startCut;
        token.fragmentEnd = fragment.endCut;
    }
    return token;
}

// One Side face per fragment of `loop`, in the loop's canonical walk order.
// The wall's frame stands on the fragment's chord in the (counter-clockwise)
// polygon -- exactly its one polygon edge when it is straight.
void appendFragmentSides(CadFeatureGeometry* g, const PlanarProfileLoop& loop, bool hole,
                         bool lineageFeatureEligible) {
    const size_t n = loop.polygon.size();
    for (uint32_t k = 0; k < loop.fragments.size(); ++k) {
        // The contiguous run of polygon edges on fragment k: its first edge is
        // one whose predecessor lies on another fragment.
        size_t first = n;
        for (size_t e = 0; e < n; ++e) {
            if (loop.edgeFragment[e] == k && loop.edgeFragment[(e + n - 1) % n] != k) {
                first = e;
                break;
            }
        }
        SketchPoint a{};
        SketchPoint b{};
        if (first == n) {
            // The whole loop is this one fragment (an unsplit circle): a chord
            // of zero length, which only a curved -- never eligible -- side has.
            a = b = loop.polygon.front();
        } else {
            size_t last = first;
            while (loop.edgeFragment[(last + 1) % n] == k && (last + 1) % n != first) {
                last = (last + 1) % n;
            }
            a = loop.polygon[first];
            b = loop.polygon[(last + 1) % n];
        }
        CadFeatureFace face;
        face.token = fragmentSideToken(loop.fragments[k]);
        face.eligible = g->kind == CadFeatureKind::Extrude && loop.fragmentCurved[k] == 0u;
        face.lineageEligible = lineageFeatureEligible && loop.fragmentCurved[k] == 0u;
        face.frame = materialOutward(*g, sideFrame(*g, a, b, hole));
        g->faces.push_back(face);
    }
}

// A PlanarFaces feature (`CAD-V6-S2`): the stored faces resolved EXACTLY against
// the sketch's arrangement, their union merged, and one Side per boundary
// fragment. No nearest face, no fallback, and nothing derived is stored.
CadStatus derivePlanarFeature(CadFeatureGeometry* g) {
    SketchArrangement arrangement;
    std::vector<size_t> faces;
    CadStatus why = resolvePlanarFaceSelection(g->sketch, g->extrude, &arrangement, &faces);
    if (why != CadStatus::Ok) {
        return why;
    }
    why = mergePlanarFaceSelection(arrangement, faces, &g->planarComponents);
    if (why != CadStatus::Ok) {
        return why;
    }
    if (g->planarComponents.empty()) {
        return CadStatus::ProfileNotFound;
    }
    setExtrudeOffsets(g);
    // The lineage's frozen R1 bit (`lineageEligible`); where a sketch may
    // stand is decided per face.
    const bool featureEligible = g->operation != CadFeatureOperation::Cut;
    const std::vector<SketchPoint>& primaryOuter = g->planarComponents.front().outer.polygon;
    g->faces.push_back(capFace(*g, primaryOuter, /*onPlane=*/true, featureEligible));
    g->faces.push_back(capFace(*g, primaryOuter, /*onPlane=*/false, featureEligible));
    for (const PlanarProfileComponent& component : g->planarComponents) {
        appendFragmentSides(g, component.outer, /*hole=*/false, featureEligible);
        for (const PlanarProfileLoop& hole : component.holes) {
            appendFragmentSides(g, hole, /*hole=*/true, featureEligible);
        }
    }
    if (!framesFinite(*g)) {
        return CadStatus::RegenerationFailed;
    }
    g->signature = featureSignature(*g);
    return CadStatus::Ok;
}

CadStatus deriveFeature(uint32_t featureId, CadFeatureOperation operation, const CadSketch& sketch,
                        const ExtrudeFeature& extrude, const CadFrame64& placement,
                        CadFeatureGeometry* out) {
    CadFeatureGeometry g;
    g.featureId = featureId;
    g.operation = operation;
    g.sketch = sketch;
    g.extrude = extrude;
    g.placement = placement;
    if (g.extrude.selection == CadSelectionKind::PlanarFaces) {
        const CadStatus planarWhy = derivePlanarFeature(&g);
        if (planarWhy != CadStatus::Ok) {
            return planarWhy;
        }
        *out = std::move(g);
        return CadStatus::Ok;
    }
    const CadStatus why = validateCadFeatureGeometry(g.sketch, g.extrude, &g.regions);
    if (why != CadStatus::Ok) {
        return why;
    }
    for (const ProfileRegionRef& ref : extrudeRegions(g.extrude)) {
        for (uint32_t r = 0; r < g.regions.regions.size(); ++r) {
            if (g.regions.regions[r].outerAnchorId == ref.outerAnchorId) {
                g.chosen.push_back(r);
                break;
            }
        }
    }
    if (g.chosen.empty()) {
        return CadStatus::ProfileNotFound;
    }
    g.components = mergeSelectedRegions(g.regions, extrudeRegions(g.extrude));
    if (g.components.empty()) {
        return CadStatus::ProfileNotFound;
    }
    setExtrudeOffsets(&g);
    // A Cut leaves its faces behind as the inside of a pocket, facing the
    // other way (`materialOutward`). Whether one may carry a sketch is the
    // face's own answer; this is only the lineage's frozen R1 bit.
    const bool featureEligible = operation != CadFeatureOperation::Cut;
    const std::vector<ClosedProfile>& loops = g.regions.loops.profiles;
    const ClosedProfile& primaryOuter = loops[g.components.front().outerLoop];
    g.faces.push_back(capFace(g, primaryOuter.polygon, /*onPlane=*/true, featureEligible));
    g.faces.push_back(capFace(g, primaryOuter.polygon, /*onPlane=*/false, featureEligible));
    for (const SketchRegionComponent& component : g.components) {
        const ClosedProfile& outer = loops[component.outerLoop];
        for (uint32_t k = 0; k < outer.polygon.size(); ++k) {
            g.faces.push_back(sideFace(g, outer, k, /*hole=*/false, featureEligible));
        }
        for (uint32_t h : component.holeLoops) {
            const ClosedProfile& hole = loops[h];
            for (uint32_t k = 0; k < hole.polygon.size(); ++k) {
                g.faces.push_back(sideFace(g, hole, k, /*hole=*/true, featureEligible));
            }
        }
    }
    if (!framesFinite(g)) {
        return CadStatus::RegenerationFailed;
    }
    g.signature = featureSignature(g);
    *out = std::move(g);
    return CadStatus::Ok;
}

// The selected union as revolve components (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`):
// every loop's polygon with, per edge, the tag of the side face it sweeps (in
// exactly the order the face table lists them, from `sideTag`), the entity it
// lies on and whether it is a piece of a curve.
std::vector<RevolveComponent> revolveComponentsOf(const CadFeatureGeometry& g, uint32_t sideTag) {
    std::vector<RevolveComponent> components;
    if (!g.planarComponents.empty()) {
        for (const PlanarProfileComponent& component : g.planarComponents) {
            RevolveComponent out;
            std::vector<const PlanarProfileLoop*> loops{&component.outer};
            for (const PlanarProfileLoop& hole : component.holes) loops.push_back(&hole);
            for (const PlanarProfileLoop* loop : loops) {
                RevolveLoop r;
                r.polygon = loop->polygon;
                for (uint32_t fragment : loop->edgeFragment) {
                    r.edgeTag.push_back(sideTag + fragment);
                    r.edgeEntity.push_back(fragment < loop->fragments.size()
                                                   ? loop->fragments[fragment].sourceEntityId
                                                   : kNoSketchEntity);
                    r.edgeCurved.push_back(fragment < loop->fragmentCurved.size()
                                                   ? loop->fragmentCurved[fragment]
                                                   : 0u);
                }
                sideTag += static_cast<uint32_t>(loop->fragments.size());
                out.loops.push_back(std::move(r));
            }
            components.push_back(std::move(out));
        }
        return components;
    }
    const std::vector<ClosedProfile>& profiles = g.regions.loops.profiles;
    for (const SketchRegionComponent& component : g.components) {
        RevolveComponent out;
        std::vector<uint32_t> loops{component.outerLoop};
        loops.insert(loops.end(), component.holeLoops.begin(), component.holeLoops.end());
        for (uint32_t index : loops) {
            const ClosedProfile& profile = profiles[index];
            const size_t n = profile.polygon.size();
            RevolveLoop r;
            r.polygon = profile.polygon;
            for (size_t k = 0; k < n; ++k) {
                r.edgeTag.push_back(sideTag++);
                r.edgeEntity.push_back(profile.edgeEntityId.size() == n ? profile.edgeEntityId[k]
                                                                        : profile.anchorEntityId);
                const bool curved = profile.fromCircle
                                    || (profile.edgeCurved.size() == n && profile.edgeCurved[k] != 0u);
                r.edgeCurved.push_back(curved ? 1u : 0u);
            }
            out.loops.push_back(std::move(r));
        }
        components.push_back(std::move(out));
    }
    return components;
}

// A Revolve (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`): the selection resolved by the
// rules an Extrude's is, the axis resolved exactly, the side of every union
// component decided on the true curves, and the face table -- a start and an
// end cap on a partial sweep, then one swept side per boundary edge (fragment
// for a PlanarFaces selection) in the order an Extrude lists its walls. Every
// face is INELIGIBLE in R1: nothing may stand on a revolved face yet.
CadStatus deriveRevolveFeature(CadFeatureGeometry* g) {
    CadStatus why = validateRevolveParameters(g->revolve);
    if (why != CadStatus::Ok) {
        return why;
    }
    why = resolveRevolveAxis(g->sketch, g->revolve.axis, &g->revolveAxis);
    if (why != CadStatus::Ok) {
        return why;
    }
    if (g->extrude.selection == CadSelectionKind::PlanarFaces) {
        SketchArrangement arrangement;
        std::vector<size_t> faces;
        why = resolvePlanarFaceSelection(g->sketch, g->extrude, &arrangement, &faces);
        if (why != CadStatus::Ok) {
            return why;
        }
        why = mergePlanarFaceSelection(arrangement, faces, &g->planarComponents);
        if (why != CadStatus::Ok) {
            return why;
        }
        if (g->planarComponents.empty()) {
            return CadStatus::ProfileNotFound;
        }
    } else {
        why = validateCadFeatureGeometry(g->sketch, g->extrude, &g->regions);
        if (why != CadStatus::Ok) {
            return why;
        }
        for (const ProfileRegionRef& ref : extrudeRegions(g->extrude)) {
            for (uint32_t r = 0; r < g->regions.regions.size(); ++r) {
                if (g->regions.regions[r].outerAnchorId == ref.outerAnchorId) {
                    g->chosen.push_back(r);
                    break;
                }
            }
        }
        g->components = mergeSelectedRegions(g->regions, extrudeRegions(g->extrude));
        if (g->chosen.empty() || g->components.empty()) {
            return CadStatus::ProfileNotFound;
        }
    }
    why = classifyRevolveComponents(g->sketch, g->revolveAxis, g->revolve.angleDegrees,
                                    revolveComponentsOf(*g, 0u), &g->revolveSides);
    if (why != CadStatus::Ok) {
        return why;
    }
    // The faces. Their frames are the sketch's own, placed: a revolved face
    // carries no sketch in R1, so a frame here only has to be finite.
    const bool full = revolveIsFullTurn(g->revolve);
    if (!full) {
        for (const bool start : {true, false}) {
            CadFeatureFace cap;
            cap.token.kind = start ? CadFaceKind::CapPlane : CadFaceKind::CapFar;
            // Not a support: R1 derives no exact frame for a revolved cap.
            cap.eligible = false;
            cap.lineageEligible = false;
            cap.frame = g->placement;
            g->faces.push_back(cap);
        }
    }
    if (!g->planarComponents.empty()) {
        for (const PlanarProfileComponent& component : g->planarComponents) {
            appendFragmentSides(g, component.outer, /*hole=*/false, /*featureEligible=*/false);
            for (const PlanarProfileLoop& hole : component.holes) {
                appendFragmentSides(g, hole, /*hole=*/true, /*featureEligible=*/false);
            }
        }
    } else {
        const std::vector<ClosedProfile>& loops = g->regions.loops.profiles;
        for (const SketchRegionComponent& component : g->components) {
            const ClosedProfile& outer = loops[component.outerLoop];
            for (uint32_t k = 0; k < outer.polygon.size(); ++k) {
                g->faces.push_back(sideFace(*g, outer, k, /*hole=*/false, /*featureEligible=*/false));
            }
            for (uint32_t h : component.holeLoops) {
                const ClosedProfile& hole = loops[h];
                for (uint32_t k = 0; k < hole.polygon.size(); ++k) {
                    g->faces.push_back(sideFace(*g, hole, k, /*hole=*/true, /*featureEligible=*/false));
                }
            }
        }
    }
    if (!framesFinite(*g)) {
        return CadStatus::RegenerationFailed;
    }
    g->signature = featureSignature(*g);
    return CadStatus::Ok;
}

}  // namespace

DVec3 cadFramePoint(const CadFrame64& frame, double a, double b, double c) {
    return dvec3Add(frame.origin, frameDirection(frame, a, b, c));
}

namespace {

// Where a sketch's (u, v, w) stands in the body, and whether it may stand
// there: the root sketch on its workplane, any other on the named face of a
// feature already in `chain`.
CadStatus sketchPlacement(const CadBodyState& state, CadSketchId sketchId, const CadSketch& sketch,
                          const CadFeatureSupport* support,
                          const std::vector<CadFeatureGeometry>& chain, CadFrame64* out) {
    if (support == nullptr) {
        // Only the base's sketch is placed by its own workplane (and, for the
        // whole body, its TopoRef); any other root sketch is a second answer
        // to where the body is.
        if (sketchId != state.baseSketchId) {
            return CadStatus::SketchSupportInvalid;
        }
        *out = workplaneFrame64(sketch.plane);
        return CadStatus::Ok;
    }
    // A sketch on one of the body's own faces is authored on its canonical
    // local XY and placed by its support alone; a TopoRef there would be a
    // second, cross-body answer to where it stands.
    if (sketch.plane != Workplane::XY || sketch.hasFaceSupport) {
        return CadStatus::FeatureSupportInvalid;
    }
    const CadFeatureGeometry* supporting = nullptr;
    for (const CadFeatureGeometry& earlier : chain) {
        if (earlier.featureId == support->featureId) {
            supporting = &earlier;
            break;
        }
    }
    const CadFeatureFace* face = nullptr;
    if (supporting != nullptr && supporting->signature == support->lineageToken) {
        for (const CadFeatureFace& candidate : supporting->faces) {
            if (sameCadFaceToken(candidate.token, support->face)) {
                face = &candidate;
                break;
            }
        }
    }
    if (face == nullptr || !face->eligible) {
        return CadStatus::FeatureSupportInvalid;
    }
    *out = face->frame;
    return CadStatus::Ok;
}

// The one walk behind both entry points: every feature in order, whichever
// kind of selection it makes, each placed on its sketch's support.
CadStatus walkCadChain(const CadBodyState& state, std::vector<CadFeatureGeometry>* out,
                       uint32_t* outFailedFeatureId, uint32_t throughFeatureId) {
    if (outFailedFeatureId != nullptr) {
        *outFailedFeatureId = 0;
    }
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    const uint32_t count = cadFeatureCount(state);
    if (count > kMaxCadFeatures) {
        return CadStatus::TooManyFeatures;
    }
    std::vector<CadFeatureGeometry> chain;
    chain.reserve(count);
    uint32_t previousId = 0;
    const auto refuse = [outFailedFeatureId](CadStatus why, uint32_t featureId) {
        if (outFailedFeatureId != nullptr) *outFailedFeatureId = featureId;
        return why;
    };
    for (uint32_t index = 0; index < count; ++index) {
        CadFeatureView view;
        cadFeatureAt(state, index, &view);
        if (!(view.featureId > previousId)) {
            return refuse(CadStatus::TooManyFeatures, view.featureId);
        }
        previousId = view.featureId;
        if (previousId > throughFeatureId) {
            break;
        }
        const bool base = index == 0;
        // New Body is the base and only the base; Add and Cut are later
        // features and only later features.
        if (base != (view.operation == CadFeatureOperation::NewBody)
            || static_cast<int>(view.operation) >= kCadFeatureOperationCount) {
            return refuse(CadStatus::InvalidFeatureOperation, view.featureId);
        }
        if (view.sketch == nullptr) {
            return refuse(CadStatus::SketchNotFound, view.featureId);
        }
        CadFrame64 placement;
        const CadStatus placed =
                sketchPlacement(state, view.sketchId, *view.sketch, view.support, chain, &placement);
        if (placed != CadStatus::Ok) {
            return refuse(placed, view.featureId);
        }
        CadFeatureGeometry g;
        CadStatus why = CadStatus::Ok;
        if (view.kind == CadFeatureKind::Revolve && view.revolve != nullptr) {
            g.featureId = view.featureId;
            g.operation = view.operation;
            g.sketch = *view.sketch;
            g.kind = CadFeatureKind::Revolve;
            g.revolve = *view.revolve;
            g.extrude = revolveSelectionCarrier(*view.revolve);
            g.placement = placement;
            why = deriveRevolveFeature(&g);
        } else {
            why = deriveFeature(view.featureId, view.operation, *view.sketch, *view.extrude,
                                placement, &g);
        }
        if (why != CadStatus::Ok) {
            return refuse(why, view.featureId);
        }
        chain.push_back(std::move(g));
    }
    *out = std::move(chain);
    return CadStatus::Ok;
}

}  // namespace

CadStatus buildCadChainGeometry(const CadBodyState& state, std::vector<CadFeatureGeometry>* out,
                                uint32_t* outFailedFeatureId, uint32_t throughFeatureId) {
    return walkCadChain(state, out, outFailedFeatureId, throughFeatureId);
}

CadStatus validateCadChain(const CadBodyState& state, uint32_t* outFailedFeatureId) {
    std::vector<CadFeatureGeometry> chain;
    const CadStatus why = walkCadChain(state, &chain, outFailedFeatureId, 0xFFFFFFFFu);
    if (why != CadStatus::Ok) {
        return why;
    }
    // A retained sketch no feature extrudes is still truth: its entities hold
    // to the sketch's own rule and its placement must resolve against the
    // chain as it stands, so an upstream edit that strips its face is refused
    // on the terms a consumed sketch's is.
    for (const CadSketchRecord& record : state.sketches) {
        bool consumed = record.sketchId == state.baseSketchId;
        for (const CadFeature& feature : state.laterFeatures) {
            consumed = consumed || feature.sketchId == record.sketchId;
        }
        if (consumed) {
            continue;
        }
        const CadStatus sketchWhy = validateCadSketch(record.sketch);
        if (sketchWhy != CadStatus::Ok) {
            return sketchWhy;
        }
        CadFrame64 placement;
        const CadStatus placed =
                sketchPlacement(state, record.sketchId, record.sketch,
                                record.hasFeatureSupport ? &record.featureSupport : nullptr, chain,
                                &placement);
        if (placed != CadStatus::Ok) {
            return placed;
        }
    }
    return CadStatus::Ok;
}

CadStatus buildCadFeatureGeometry(const CadBodyState& state, uint32_t featureId,
                                  CadFeatureGeometry* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadFeatureView view;
    if (!findCadFeature(state, featureId, &view)) {
        return CadStatus::ProfileNotFound;
    }
    std::vector<CadFeatureGeometry> chain;
    const CadStatus why = buildCadChainGeometry(state, &chain, nullptr, featureId);
    if (why != CadStatus::Ok) {
        return why;
    }
    for (CadFeatureGeometry& g : chain) {
        if (g.featureId == featureId) {
            *out = std::move(g);
            return CadStatus::Ok;
        }
    }
    return CadStatus::ProfileNotFound;
}

namespace {

// One prism: loop 0 is the outer boundary and every other loop a hole, each
// counter-clockwise in (u, v); `edgeTags[l][k]` is the face tag of loop l's
// edge k. Lower ring then upper ring, loop by loop in the concatenated order
// the cap triangulation indexes.
CadStatus appendPrism(const CadFeatureGeometry& g, const std::vector<std::vector<SketchPoint>>& polys,
                      const std::vector<std::vector<uint32_t>>& edgeTags, uint32_t upperTag,
                      uint32_t lowerTag, CadSolid* out) {
    std::vector<uint32_t> cap;
    if (polys.size() == 1u) {
        if (triangulateSimplePolygon(polys[0], &cap) != CadStatus::Ok) {
            return CadStatus::TriangulationFailed;
        }
    } else if (cadKernelTriangulateRegion(polys, &cap) != CadKernelStatus::Ok) {
        return CadStatus::TriangulationFailed;
    }
    uint32_t total = 0;
    for (const std::vector<SketchPoint>& p : polys) {
        total += static_cast<uint32_t>(p.size());
    }
    const uint32_t base = out->vertexCount();
    for (int level = 0; level < 2; ++level) {
        const double w = level == 0 ? g.nearOffset : g.farOffset;
        for (const std::vector<SketchPoint>& p : polys) {
            for (const SketchPoint& q : p) {
                const DVec3 x = cadFramePoint(g.placement, q.u, q.v, w);
                out->positions.insert(out->positions.end(), {x.x, x.y, x.z});
            }
        }
    }
    const uint32_t lower = base;
    const uint32_t upper = base + total;
    for (size_t t = 0; t + 2 < cap.size(); t += 3) {
        out->indices.insert(out->indices.end(),
                            {upper + cap[t], upper + cap[t + 1], upper + cap[t + 2]});
        out->faceTags.push_back(upperTag);
    }
    for (size_t t = 0; t + 2 < cap.size(); t += 3) {
        out->indices.insert(out->indices.end(),
                            {lower + cap[t], lower + cap[t + 2], lower + cap[t + 1]});
        out->faceTags.push_back(lowerTag);
    }
    uint32_t offset = 0;
    for (size_t l = 0; l < polys.size(); ++l) {
        const uint32_t n = static_cast<uint32_t>(polys[l].size());
        const bool hole = l > 0u;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t j = (i + 1u) % n;
            const uint32_t li = lower + offset + i;
            const uint32_t lj = lower + offset + j;
            const uint32_t ui = upper + offset + i;
            const uint32_t uj = upper + offset + j;
            if (!hole) {
                out->indices.insert(out->indices.end(), {li, lj, uj, li, uj, ui});
            } else {
                out->indices.insert(out->indices.end(), {li, uj, lj, li, ui, uj});
            }
            out->faceTags.push_back(edgeTags[l][i]);
            out->faceTags.push_back(edgeTags[l][i]);
        }
        offset += n;
    }
    return CadStatus::Ok;
}

}  // namespace

CadStatus appendCadFeatureSolid(const CadFeatureGeometry& g, uint32_t tagOffset, CadSolid* solid) {
    if (solid == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    if (g.kind == CadFeatureKind::Revolve) {
        // The face table lists the two caps first on a partial sweep and none
        // on a full turn; the sides follow in the same walk either way.
        const bool full = revolveIsFullTurn(g.revolve);
        const uint32_t sideTag = tagOffset + (full ? 0u : 2u);
        return appendRevolveSolid(g.placement, g.revolveAxis, g.revolve.angleDegrees,
                                  g.revolve.direction, revolveComponentsOf(g, sideTag),
                                  g.revolveSides, tagOffset + 0u, tagOffset + 1u, solid);
    }
    CadSolid out = *solid;
    // The two caps are the table's first two entries; which one the +N set of
    // vertices forms depends on the side the solid grows on, exactly as R0's
    // mesh and face ranges always decided it.
    const uint32_t tagPlane = tagOffset + 0u;
    const uint32_t tagFar = tagOffset + 1u;
    const bool upperIsPlaneCap = g.extrude.direction == ExtrudeDirection::AgainstNormal;
    const uint32_t upperTag = upperIsPlaneCap ? tagPlane : tagFar;
    const uint32_t lowerTag = upperIsPlaneCap ? tagFar : tagPlane;
    uint32_t sideTag = tagOffset + 2u;
    if (!g.planarComponents.empty()) {
        // One Side face per boundary FRAGMENT: every polygon edge carries the
        // tag of the fragment it lies on, so a curved fragment's facets are one
        // face, in the order `derivePlanarFeature` listed them.
        for (const PlanarProfileComponent& component : g.planarComponents) {
            std::vector<const PlanarProfileLoop*> loops{&component.outer};
            for (const PlanarProfileLoop& hole : component.holes) loops.push_back(&hole);
            std::vector<std::vector<SketchPoint>> polys;
            std::vector<std::vector<uint32_t>> tags;
            for (const PlanarProfileLoop* loop : loops) {
                polys.push_back(loop->polygon);
                std::vector<uint32_t> edgeTags;
                for (uint32_t fragment : loop->edgeFragment) {
                    edgeTags.push_back(sideTag + fragment);
                }
                tags.push_back(std::move(edgeTags));
                sideTag += static_cast<uint32_t>(loop->fragments.size());
            }
            const CadStatus why = appendPrism(g, polys, tags, upperTag, lowerTag, &out);
            if (why != CadStatus::Ok) {
                return why;
            }
        }
    } else {
        for (const SketchRegionComponent& component : g.components) {
            std::vector<std::vector<SketchPoint>> polys = sketchComponentLoops(g.regions, component);
            std::vector<std::vector<uint32_t>> tags;
            for (const std::vector<SketchPoint>& p : polys) {
                std::vector<uint32_t> edgeTags;
                for (size_t k = 0; k < p.size(); ++k) {
                    edgeTags.push_back(sideTag++);
                }
                tags.push_back(std::move(edgeTags));
            }
            const CadStatus why = appendPrism(g, polys, tags, upperTag, lowerTag, &out);
            if (why != CadStatus::Ok) {
                return why;
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

bool cadSolidHasFaceOn(const CadSolid& solid, const CadFrame64& frame) {
    // Scale-aware tolerances: a coordinate a micrometre off a metre-sized plane
    // is on it; a normal within about a tenth of a degree faces the same way.
    double scale = 1.0;
    for (double value : solid.positions) {
        scale = std::max(scale, std::fabs(value));
    }
    const double planeTolerance = 1.0e-9 * scale;
    const size_t triangles = solid.indices.size() / 3u;
    for (size_t t = 0; t < triangles; ++t) {
        DVec3 p[3];
        for (int k = 0; k < 3; ++k) {
            const uint32_t idx = solid.indices[t * 3u + static_cast<size_t>(k)];
            p[k] = DVec3{solid.positions[idx * 3u], solid.positions[idx * 3u + 1u],
                         solid.positions[idx * 3u + 2u]};
        }
        DVec3 normal;
        if (!dvec3Normalized(dvec3Cross(dvec3Sub(p[1], p[0]), dvec3Sub(p[2], p[0])), &normal)) {
            continue;
        }
        if (dvec3Dot(normal, frame.n) < 0.99999) {
            continue;
        }
        bool onPlane = true;
        for (int k = 0; k < 3 && onPlane; ++k) {
            onPlane = std::fabs(dvec3Dot(dvec3Sub(p[k], frame.origin), frame.n)) <= planeTolerance;
        }
        if (onPlane) {
            return true;
        }
    }
    return false;
}

double cadSolidVolume(const CadSolid& solid) {
    double sum = 0.0;
    const size_t triangles = solid.indices.size() / 3u;
    for (size_t t = 0; t < triangles; ++t) {
        DVec3 p[3];
        for (int k = 0; k < 3; ++k) {
            const uint32_t idx = solid.indices[t * 3u + static_cast<size_t>(k)];
            p[k] = DVec3{solid.positions[idx * 3u], solid.positions[idx * 3u + 1u],
                         solid.positions[idx * 3u + 2u]};
        }
        sum += dvec3Dot(p[0], dvec3Cross(p[1], p[2]));
    }
    return sum / 6.0;
}

}  // namespace forgeshape
