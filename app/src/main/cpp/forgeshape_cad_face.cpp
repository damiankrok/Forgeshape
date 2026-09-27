#include "forgeshape_cad_face.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "forgeshape_cad_feature.h"

namespace forgeshape {
namespace {

// Every frame is DERIVED in binary64 by the feature chain
// (forgeshape_cad_feature.h) and rounded to float here, once, for the callers
// that compose it into a float world model.
CadFace toCadFace(const CadFeatureFace& face) {
    CadFace out;
    out.token = face.token;
    out.origin = vec3FromDVec3(face.frame.origin);
    out.u = vec3FromDVec3(face.frame.u);
    out.v = vec3FromDVec3(face.frame.v);
    out.n = vec3FromDVec3(face.frame.n);
    out.eligible = face.eligible;
    return out;
}

}  // namespace

CadStatus enumerateCadFeatureFaces(const CadBodyState& state, uint32_t featureId,
                                   std::vector<CadFace>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadFeatureGeometry geometry;
    const CadStatus why = buildCadFeatureGeometry(state, featureId, &geometry);
    if (why != CadStatus::Ok) {
        return why;
    }
    std::vector<CadFace> faces;
    faces.reserve(geometry.faces.size());
    for (const CadFeatureFace& face : geometry.faces) {
        faces.push_back(toCadFace(face));
    }
    *out = std::move(faces);
    return CadStatus::Ok;
}

CadStatus enumerateCadFaces(const CadBodyState& state, std::vector<CadFace>* out) {
    return enumerateCadFeatureFaces(state, kCadFeatureId, out);
}

CadStatus cadFaceRangesFromMesh(const CadBodyMesh& mesh, std::vector<CadFaceRange>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    std::vector<CadFaceRange> ranges;
    const uint32_t triangles = static_cast<uint32_t>(mesh.triangleFace.size());
    uint32_t t = 0;
    while (t < triangles) {
        const uint32_t tag = mesh.triangleFace[t];
        if (tag >= mesh.faces.size()) {
            return CadStatus::RegenerationFailed;
        }
        uint32_t end = t + 1u;
        while (end < triangles && mesh.triangleFace[end] == tag) {
            ++end;
        }
        const CadMeshFace& face = mesh.faces[tag];
        ranges.push_back(CadFaceRange{t * 3u, (end - t) * 3u, face.token, face.eligible,
                                      face.featureId});
        t = end;
    }
    *out = std::move(ranges);
    return CadStatus::Ok;
}

CadStatus cadFaceRanges(const CadBodyState& state, std::vector<CadFaceRange>* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadBodyMesh mesh;
    const CadStatus why = regenerateCadBody(state, &mesh);
    if (why != CadStatus::Ok) {
        return why;
    }
    return cadFaceRangesFromMesh(mesh, out);
}

CadStatus resolveCadFeatureFace(const CadBodyState& state, uint32_t featureId,
                                const CadFaceToken& token, CadFace* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadFeatureGeometry geometry;
    const CadStatus why = buildCadFeatureGeometry(state, featureId, &geometry);
    if (why != CadStatus::Ok) {
        return why;
    }
    for (const CadFeatureFace& face : geometry.faces) {
        if (sameCadFaceToken(face.token, token)) {
            *out = toCadFace(face);
            return CadStatus::Ok;
        }
    }
    return CadStatus::ProfileNotFound;
}

CadStatus resolveCadFace(const CadBodyState& state, const CadFaceToken& token, CadFace* out) {
    return resolveCadFeatureFace(state, kCadFeatureId, token, out);
}

uint64_t cadFeatureTopologySignature(const CadBodyState& state, uint32_t featureId) {
    CadFeatureGeometry geometry;
    if (buildCadFeatureGeometry(state, featureId, &geometry) != CadStatus::Ok) {
        return 0;
    }
    return geometry.signature;
}

bool cadMeshCarriesFace(const CadBodyMesh& mesh, const CadFace& face) {
    const std::vector<MeshVertex>& v = mesh.mesh.vertices;
    const std::vector<uint32_t>& idx = mesh.mesh.indices;
    float scale = 1.0f;
    for (const MeshVertex& vertex : v) {
        for (float c : vertex.position) {
            scale = std::max(scale, std::fabs(c));
        }
    }
    // Float tolerances: the render mesh is rounded once from binary64.
    const float planeTolerance = 1.0e-5f * scale;
    const Vec3 n = vec3Normalize(face.n);
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const Vec3 p0{v[idx[t]].position[0], v[idx[t]].position[1], v[idx[t]].position[2]};
        const Vec3 p1{v[idx[t + 1]].position[0], v[idx[t + 1]].position[1], v[idx[t + 1]].position[2]};
        const Vec3 p2{v[idx[t + 2]].position[0], v[idx[t + 2]].position[1], v[idx[t + 2]].position[2]};
        const Vec3 cross = vec3Cross(vec3Sub(p1, p0), vec3Sub(p2, p0));
        const float len = std::sqrt(vec3Dot(cross, cross));
        if (!(len > 0.0f)) {
            continue;
        }
        if (vec3Dot(vec3Scale(cross, 1.0f / len), n) < 0.9999f) {
            continue;
        }
        if (std::fabs(vec3Dot(vec3Sub(p0, face.origin), n)) <= planeTolerance
            && std::fabs(vec3Dot(vec3Sub(p1, face.origin), n)) <= planeTolerance
            && std::fabs(vec3Dot(vec3Sub(p2, face.origin), n)) <= planeTolerance) {
            return true;
        }
    }
    return false;
}

uint64_t cadTopologySignature(const CadBodyState& state) {
    return cadFeatureTopologySignature(state, kCadFeatureId);
}

}  // namespace forgeshape
