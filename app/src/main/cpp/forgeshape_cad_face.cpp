#include "forgeshape_cad_face.h"

#include <cmath>
#include <cstring>

namespace forgeshape {
namespace {

constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void mixU64(uint64_t& h, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        h ^= (v >> (i * 8)) & 0xFFu;
        h *= kFnvPrime;
    }
}

Vec3 normalizedOr(const Vec3& v, const Vec3& fallback) {
    const Vec3 n = vec3Normalize(v);
    return vec3Finite(n) && vec3Dot(n, n) > 0.5f ? n : fallback;
}

// The chosen profile and the extrusion offsets, shared by every entry point.
struct ExtrudeContext {
    const ClosedProfile* profile = nullptr;
    ProfileExtraction extraction;
    Workplane plane = Workplane::XY;
    WorkplaneFrame frame;
    double planeCapOffset = 0.0;  // offset of the cap ON the sketch plane
    double farCapOffset = 0.0;    // offset of the cap at the extrusion tip
    // The direction, as a signed unit along the plane normal, that points from
    // the plane cap toward the solid (i.e. the extrusion direction).
    double extrudeSign = 1.0;
};

CadStatus buildContext(const CadBodyState& state, ExtrudeContext* ctx) {
    const CadStatus why = validateCadBodyState(state, &ctx->extraction);
    if (why != CadStatus::Ok) {
        return why;
    }
    ctx->profile = findClosedProfile(ctx->extraction, state.extrude.profileEntityId);
    if (ctx->profile == nullptr) {
        return CadStatus::ProfileNotFound;
    }
    ctx->plane = state.sketch.plane;
    ctx->frame = workplaneFrame(ctx->plane);
    // The plane cap is always at offset 0; the far cap is at the extrusion tip.
    // AlongNormal grows to +depth, AgainstNormal to -depth. See generateCadMesh.
    if (state.extrude.direction == ExtrudeDirection::AlongNormal) {
        ctx->planeCapOffset = 0.0;
        ctx->farCapOffset = state.extrude.depth;
        ctx->extrudeSign = 1.0;
    } else {
        ctx->planeCapOffset = 0.0;
        ctx->farCapOffset = -state.extrude.depth;
        ctx->extrudeSign = -1.0;
    }
    return CadStatus::Ok;
}

SketchPoint profileCentroid(const ClosedProfile& profile) {
    double u = 0.0;
    double v = 0.0;
    for (const SketchPoint& p : profile.polygon) {
        u += p.u;
        v += p.v;
    }
    const double inv = 1.0 / static_cast<double>(profile.polygon.size());
    return SketchPoint{u * inv, v * inv};
}

// Builds a cap face. `onPlane` selects the cap lying on the sketch plane;
// otherwise the far cap.
CadFace makeCap(const ExtrudeContext& ctx, bool onPlane) {
    CadFace face;
    face.token.kind = onPlane ? CadFaceKind::CapPlane : CadFaceKind::CapFar;
    face.eligible = true;
    const double offset = onPlane ? ctx.planeCapOffset : ctx.farCapOffset;
    const SketchPoint centroid = profileCentroid(*ctx.profile);
    face.origin = workplaneToLocalAtOffset(ctx.plane, centroid, offset);
    // Outward normal points away from the solid. The solid lies on the
    // +extrudeSign side of the plane cap; the far cap's outward is +extrudeSign.
    const float sign = onPlane ? -static_cast<float>(ctx.extrudeSign)
                               : static_cast<float>(ctx.extrudeSign);
    face.n = vec3Scale(ctx.frame.normal, sign);
    face.u = ctx.frame.uAxis;
    // Choose V so that u x v = n: flip the plane's V when the outward normal is
    // the plane normal reversed.
    face.v = (sign > 0.0f) ? ctx.frame.vAxis : vec3Scale(ctx.frame.vAxis, -1.0f);
    return face;
}

// Builds the side face for profile edge k (polygon[k] -> polygon[k+1]).
CadFace makeSide(const ExtrudeContext& ctx, uint32_t k) {
    const ClosedProfile& profile = *ctx.profile;
    const uint32_t n = static_cast<uint32_t>(profile.polygon.size());
    const SketchPoint a2 = profile.polygon[k];
    const SketchPoint b2 = profile.polygon[(k + 1u) % n];
    const Vec3 nearA = workplaneToLocalAtOffset(ctx.plane, a2, 0.0);
    const Vec3 nearB = workplaneToLocalAtOffset(ctx.plane, b2, 0.0);
    const Vec3 edge = vec3Sub(nearB, nearA);
    const Vec3 edgeDir = normalizedOr(edge, ctx.frame.uAxis);
    // Outward normal of a CCW edge is edge x planeNormal (verified for XY).
    const Vec3 outward = normalizedOr(vec3Cross(edgeDir, ctx.frame.normal), ctx.frame.normal);

    CadFace face;
    face.token.kind = CadFaceKind::Side;
    face.token.edgeEntityId = profile.edgeEntityId.size() == n ? profile.edgeEntityId[k]
                                                               : profile.anchorEntityId;
    face.token.edgeLocalIndex = profile.edgeLocalIndex.size() == n ? profile.edgeLocalIndex[k] : k;
    // A circle's side is cylindrical: reported so a tap resolves, never eligible.
    face.eligible = !profile.fromCircle;
    face.n = outward;
    face.u = edgeDir;
    face.v = normalizedOr(vec3Cross(outward, edgeDir), ctx.frame.vAxis);
    // Origin at the centre of the quad: the four corners averaged.
    const Vec3 farA = workplaneToLocalAtOffset(ctx.plane, a2, ctx.farCapOffset);
    const Vec3 farB = workplaneToLocalAtOffset(ctx.plane, b2, ctx.farCapOffset);
    Vec3 sum = vec3Add(vec3Add(nearA, nearB), vec3Add(farA, farB));
    face.origin = vec3Scale(sum, 0.25f);
    return face;
}

}  // namespace

CadStatus enumerateCadFaces(const CadBodyState& state, std::vector<CadFace>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    ExtrudeContext ctx;
    const CadStatus why = buildContext(state, &ctx);
    if (why != CadStatus::Ok) {
        return why;
    }
    std::vector<CadFace> faces;
    faces.push_back(makeCap(ctx, /*onPlane=*/true));
    faces.push_back(makeCap(ctx, /*onPlane=*/false));
    const uint32_t n = static_cast<uint32_t>(ctx.profile->polygon.size());
    for (uint32_t k = 0; k < n; ++k) {
        faces.push_back(makeSide(ctx, k));
    }
    *out = std::move(faces);
    return CadStatus::Ok;
}

CadStatus cadFaceRanges(const CadBodyState& state, std::vector<CadFaceRange>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    ExtrudeContext ctx;
    const CadStatus why = buildContext(state, &ctx);
    if (why != CadStatus::Ok) {
        return why;
    }
    const uint32_t n = static_cast<uint32_t>(ctx.profile->polygon.size());
    // generateCadMesh emits: far cap (3(n-2)), near cap (3(n-2)), then 6 per
    // side edge. The "far" set is the mesh's upper vertices; whether the far set
    // is the plane cap or the far cap depends on the direction.
    const uint32_t capIndices = 3u * (n - 2u);
    const bool farSetIsPlaneCap = state.extrude.direction == ExtrudeDirection::AgainstNormal;

    std::vector<CadFaceRange> ranges;
    // Range 0: the mesh's far/upper cap.
    ranges.push_back(CadFaceRange{
        0u, capIndices,
        CadFaceToken{farSetIsPlaneCap ? CadFaceKind::CapPlane : CadFaceKind::CapFar, 0u, 0u}, true});
    // Range 1: the mesh's near/lower cap.
    ranges.push_back(CadFaceRange{
        capIndices, capIndices,
        CadFaceToken{farSetIsPlaneCap ? CadFaceKind::CapFar : CadFaceKind::CapPlane, 0u, 0u}, true});
    // The sides, 6 indices each, in profile-edge order.
    uint32_t cursor = 2u * capIndices;
    for (uint32_t k = 0; k < n; ++k) {
        const CadFace side = makeSide(ctx, k);
        ranges.push_back(CadFaceRange{cursor, 6u, side.token, side.eligible});
        cursor += 6u;
    }
    *out = std::move(ranges);
    return CadStatus::Ok;
}

CadStatus resolveCadFace(const CadBodyState& state, const CadFaceToken& token, CadFace* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    std::vector<CadFace> faces;
    const CadStatus why = enumerateCadFaces(state, &faces);
    if (why != CadStatus::Ok) {
        return why;
    }
    for (const CadFace& face : faces) {
        if (sameCadFaceToken(face.token, token)) {
            *out = face;
            return CadStatus::Ok;
        }
    }
    return CadStatus::ProfileNotFound;
}

uint64_t cadTopologySignature(const CadBodyState& state) {
    std::vector<CadFace> faces;
    if (enumerateCadFaces(state, &faces) != CadStatus::Ok) {
        return 0;
    }
    uint64_t h = kFnvOffset;
    // The chosen profile identity and the eligible-set shape, then every token.
    mixU64(h, static_cast<uint64_t>(state.extrude.profileEntityId));
    mixU64(h, faces.size());
    for (const CadFace& face : faces) {
        mixU64(h, cadFaceTokenCode(face.token));
        mixU64(h, face.eligible ? 1u : 0u);
    }
    // Never zero: zero is the "no topology" answer buildContext failures give.
    return h == 0 ? 1 : h;
}

}  // namespace forgeshape
