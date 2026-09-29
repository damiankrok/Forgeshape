#include "forgeshape_cad_feature.h"

#include <algorithm>
#include <cmath>

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

// The face of one loop edge in sketch-local terms, placed into the body.
// `hole` selects the inner-wall orientation: the material is OUTSIDE a hole,
// so its wall faces into the hole.
CadFeatureFace sideFace(const CadFeatureGeometry& g, const ClosedProfile& loop, uint32_t k,
                        bool hole, bool featureEligible) {
    const uint32_t n = static_cast<uint32_t>(loop.polygon.size());
    const SketchPoint& a = loop.polygon[k];
    const SketchPoint& b = loop.polygon[(k + 1u) % n];
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
    CadFeatureFace face;
    face.token.kind = CadFaceKind::Side;
    face.token.edgeEntityId = loop.edgeEntityId.size() == n ? loop.edgeEntityId[k] : loop.anchorEntityId;
    face.token.edgeLocalIndex = loop.edgeLocalIndex.size() == n ? loop.edgeLocalIndex[k] : k;
    const bool curved = loop.fromCircle || (loop.edgeCurved.size() == n && loop.edgeCurved[k] != 0u);
    face.eligible = featureEligible && !curved;
    face.frame.n = frameDirection(g.placement, ou, ov, 0.0);
    face.frame.u = frameDirection(g.placement, uu, uv, 0.0);
    face.frame.v = dvec3Cross(face.frame.n, face.frame.u);
    // The centre of the wall quad.
    const double mu = (a.u + b.u) * 0.5;
    const double mv = (a.v + b.v) * 0.5;
    const double mw = (g.planeCapOffset + g.farCapOffset) * 0.5;
    face.frame.origin = cadFramePoint(g.placement, mu, mv, mw);
    return face;
}

CadFeatureFace capFace(const CadFeatureGeometry& g, const ClosedProfile& primaryOuter, bool onPlane,
                       bool featureEligible) {
    CadFeatureFace face;
    face.token.kind = onPlane ? CadFaceKind::CapPlane : CadFaceKind::CapFar;
    face.eligible = featureEligible;
    const double offset = onPlane ? g.planeCapOffset : g.farCapOffset;
    // The VERTEX MEAN of the first region's outer loop: what R0's cap frame
    // always used, kept so a face-supported dependent does not move.
    double su = 0.0;
    double sv = 0.0;
    for (const SketchPoint& p : primaryOuter.polygon) {
        su += p.u;
        sv += p.v;
    }
    const double inv = 1.0 / static_cast<double>(primaryOuter.polygon.size());
    face.frame.origin = cadFramePoint(g.placement, su * inv, sv * inv, offset);
    const double sign = onPlane ? -g.extrudeSign : g.extrudeSign;
    face.frame.n = frameDirection(g.placement, 0.0, 0.0, sign);
    face.frame.u = g.placement.u;
    face.frame.v = sign > 0.0 ? g.placement.v : dvec3Scale(g.placement.v, -1.0);
    return face;
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
    const double positive = extrudePositiveDistance(g.extrude);
    const double negative = extrudeNegativeDistance(g.extrude);
    g.nearOffset = -negative;
    g.farOffset = positive;
    // The same sentence forgeshape_cad_face.cpp has always used: CapPlane is the
    // cap the extrusion grows FROM, CapFar the one it grows TO.
    if (g.extrude.direction == ExtrudeDirection::AlongNormal) {
        g.planeCapOffset = -negative;
        g.farCapOffset = positive;
        g.extrudeSign = 1.0;
    } else {
        g.planeCapOffset = positive;
        g.farCapOffset = -negative;
        g.extrudeSign = -1.0;
    }
    // A Cut leaves its faces behind as the inside of a pocket, facing the
    // other way; none of them may carry a sketch in R1.
    const bool featureEligible = operation != CadFeatureOperation::Cut;
    const std::vector<ClosedProfile>& loops = g.regions.loops.profiles;
    const ClosedProfile& primaryOuter = loops[g.regions.regions[g.chosen[0]].outerLoop];
    g.faces.push_back(capFace(g, primaryOuter, /*onPlane=*/true, featureEligible));
    g.faces.push_back(capFace(g, primaryOuter, /*onPlane=*/false, featureEligible));
    for (uint32_t r : g.chosen) {
        const SketchRegion& region = g.regions.regions[r];
        const ClosedProfile& outer = loops[region.outerLoop];
        for (uint32_t k = 0; k < outer.polygon.size(); ++k) {
            g.faces.push_back(sideFace(g, outer, k, /*hole=*/false, featureEligible));
        }
        for (uint32_t h : region.holeLoops) {
            const ClosedProfile& hole = loops[h];
            for (uint32_t k = 0; k < hole.polygon.size(); ++k) {
                g.faces.push_back(sideFace(g, hole, k, /*hole=*/true, featureEligible));
            }
        }
    }
    for (const CadFeatureFace& face : g.faces) {
        if (!dvec3Finite(face.frame.origin) || !dvec3Finite(face.frame.u)
            || !dvec3Finite(face.frame.v) || !dvec3Finite(face.frame.n)) {
            return CadStatus::RegenerationFailed;
        }
    }
    // The lineage signature, on the §7c rule: the first region's outer anchor,
    // the face count, then every face's token code and eligibility.
    uint64_t h = kFnvOffset;
    mixU64(h, static_cast<uint64_t>(g.extrude.profileEntityId));
    mixU64(h, g.faces.size());
    for (const CadFeatureFace& face : g.faces) {
        mixU64(h, cadFaceTokenCode(face.token));
        mixU64(h, face.eligible ? 1u : 0u);
    }
    g.signature = h == 0 ? 1 : h;
    *out = std::move(g);
    return CadStatus::Ok;
}

}  // namespace

DVec3 cadFramePoint(const CadFrame64& frame, double a, double b, double c) {
    return dvec3Add(frame.origin, frameDirection(frame, a, b, c));
}

CadStatus buildCadChainGeometry(const CadBodyState& state, std::vector<CadFeatureGeometry>* out,
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
    for (uint32_t index = 0; index < count; ++index) {
        CadFeatureView view;
        cadFeatureAt(state, index, &view);
        if (!(view.featureId > previousId)) {
            if (outFailedFeatureId != nullptr) *outFailedFeatureId = view.featureId;
            return CadStatus::TooManyFeatures;
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
            if (outFailedFeatureId != nullptr) *outFailedFeatureId = view.featureId;
            return CadStatus::InvalidFeatureOperation;
        }
        CadFrame64 placement = workplaneFrame64(view.sketch->plane);
        if (!base) {
            // A later feature's sketch is authored on its canonical local XY
            // and placed by its support alone; a TopoRef there would be a
            // second, cross-body answer to where it stands.
            if (view.sketch->plane != Workplane::XY || view.sketch->hasFaceSupport) {
                if (outFailedFeatureId != nullptr) *outFailedFeatureId = view.featureId;
                return CadStatus::FeatureSupportInvalid;
            }
            const CadFeatureSupport& support = *view.support;
            const CadFeatureGeometry* supporting = nullptr;
            for (const CadFeatureGeometry& earlier : chain) {
                if (earlier.featureId == support.featureId) {
                    supporting = &earlier;
                    break;
                }
            }
            const CadFeatureFace* face = nullptr;
            if (supporting != nullptr && supporting->signature == support.lineageToken) {
                for (const CadFeatureFace& candidate : supporting->faces) {
                    if (sameCadFaceToken(candidate.token, support.face)) {
                        face = &candidate;
                        break;
                    }
                }
            }
            if (face == nullptr || !face->eligible) {
                if (outFailedFeatureId != nullptr) *outFailedFeatureId = view.featureId;
                return CadStatus::FeatureSupportInvalid;
            }
            placement = face->frame;
        }
        CadFeatureGeometry g;
        const CadStatus why =
                deriveFeature(view.featureId, view.operation, *view.sketch, *view.extrude, placement, &g);
        if (why != CadStatus::Ok) {
            if (outFailedFeatureId != nullptr) *outFailedFeatureId = view.featureId;
            return why;
        }
        chain.push_back(std::move(g));
    }
    *out = std::move(chain);
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

CadStatus appendCadFeatureSolid(const CadFeatureGeometry& g, uint32_t tagOffset, CadSolid* solid) {
    if (solid == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadSolid out = *solid;
    const std::vector<ClosedProfile>& loops = g.regions.loops.profiles;
    // The two caps are the table's first two entries; which one the +N set of
    // vertices forms depends on the side the solid grows on, exactly as R0's
    // mesh and face ranges always decided it.
    const uint32_t tagPlane = tagOffset + 0u;
    const uint32_t tagFar = tagOffset + 1u;
    const bool upperIsPlaneCap = g.extrude.direction == ExtrudeDirection::AgainstNormal;
    const uint32_t upperTag = upperIsPlaneCap ? tagPlane : tagFar;
    const uint32_t lowerTag = upperIsPlaneCap ? tagFar : tagPlane;
    uint32_t sideTag = tagOffset + 2u;
    for (uint32_t r : g.chosen) {
        const SketchRegion& region = g.regions.regions[r];
        std::vector<std::vector<SketchPoint>> polys = sketchRegionLoops(g.regions, region);
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
        const uint32_t base = out.vertexCount();
        // Lower ring then upper ring, loop by loop in the concatenated order the
        // cap triangulation indexes.
        for (int level = 0; level < 2; ++level) {
            const double w = level == 0 ? g.nearOffset : g.farOffset;
            for (const std::vector<SketchPoint>& p : polys) {
                for (const SketchPoint& q : p) {
                    const DVec3 x = cadFramePoint(g.placement, q.u, q.v, w);
                    out.positions.insert(out.positions.end(), {x.x, x.y, x.z});
                }
            }
        }
        const uint32_t lower = base;
        const uint32_t upper = base + total;
        for (size_t t = 0; t + 2 < cap.size(); t += 3) {
            out.indices.insert(out.indices.end(),
                               {upper + cap[t], upper + cap[t + 1], upper + cap[t + 2]});
            out.faceTags.push_back(upperTag);
        }
        for (size_t t = 0; t + 2 < cap.size(); t += 3) {
            out.indices.insert(out.indices.end(),
                               {lower + cap[t], lower + cap[t + 2], lower + cap[t + 1]});
            out.faceTags.push_back(lowerTag);
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
                    out.indices.insert(out.indices.end(), {li, lj, uj, li, uj, ui});
                } else {
                    out.indices.insert(out.indices.end(), {li, uj, lj, li, ui, uj});
                }
                out.faceTags.push_back(sideTag);
                out.faceTags.push_back(sideTag);
                ++sideTag;
            }
            offset += n;
        }
        (void)loops;
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
