#include "forgeshape_freeform_subdivision.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace forgeshape {

namespace {

// The working mesh one level operates on: quads only, dense indices.
struct QuadLevel {
    std::vector<DVec3> positions;
    std::vector<uint32_t> quads;
    std::vector<FreeformFaceId> origin;
    // Creased edges only, as (min << 32 | max) -> weight, sorted by key. Most
    // edges are smooth and never appear here.
    std::vector<std::pair<uint64_t, double>> creases;
};

uint64_t pairKey(uint32_t a, uint32_t b) {
    const uint32_t lo = std::min(a, b);
    const uint32_t hi = std::max(a, b);
    return (static_cast<uint64_t>(lo) << 32) | hi;
}

double creaseOf(const QuadLevel& level, uint64_t key) {
    const auto it = std::lower_bound(level.creases.begin(), level.creases.end(),
                                     std::make_pair(key, -1.0));
    return it != level.creases.end() && it->first == key ? it->second : 0.0;
}

DVec3 lerp(const DVec3& a, const DVec3& b, double t) {
    return dvec3Add(a, dvec3Scale(dvec3Sub(b, a), t));
}

// One Catmull-Clark step. Deterministic: every loop runs in index order and
// every sort is over integer keys.
void subdivideOnce(const QuadLevel& in, QuadLevel* out) {
    const uint32_t nv = static_cast<uint32_t>(in.positions.size());
    const uint32_t nf = static_cast<uint32_t>(in.quads.size() / 4u);
    // Edges: (key, face, side) sorted, then unique.
    struct Side {
        uint64_t key;
        uint32_t face;
        uint32_t side;
    };
    std::vector<Side> sides;
    sides.reserve(static_cast<size_t>(nf) * 4u);
    for (uint32_t f = 0; f < nf; ++f) {
        for (uint32_t k = 0; k < 4; ++k) {
            sides.push_back(Side{pairKey(in.quads[f * 4u + k], in.quads[f * 4u + (k + 1u) % 4u]), f, k});
        }
    }
    std::sort(sides.begin(), sides.end(), [](const Side& a, const Side& b) {
        return a.key < b.key || (a.key == b.key && (a.face < b.face || (a.face == b.face && a.side < b.side)));
    });
    std::vector<uint64_t> edgeKey;
    std::vector<uint32_t> edgeFace0;
    std::vector<int64_t> edgeFace1;
    std::vector<uint32_t> faceSideEdge(static_cast<size_t>(nf) * 4u, 0u);
    for (size_t i = 0; i < sides.size(); ++i) {
        if (i == 0 || sides[i].key != sides[i - 1].key) {
            edgeKey.push_back(sides[i].key);
            edgeFace0.push_back(sides[i].face);
            edgeFace1.push_back(-1);
        } else {
            edgeFace1.back() = sides[i].face;
        }
        faceSideEdge[sides[i].face * 4u + sides[i].side] = static_cast<uint32_t>(edgeKey.size() - 1u);
    }
    const uint32_t ne = static_cast<uint32_t>(edgeKey.size());

    // Face points.
    std::vector<DVec3> facePoint(nf);
    for (uint32_t f = 0; f < nf; ++f) {
        DVec3 sum{0.0, 0.0, 0.0};
        for (uint32_t k = 0; k < 4; ++k) sum = dvec3Add(sum, in.positions[in.quads[f * 4u + k]]);
        facePoint[f] = dvec3Scale(sum, 0.25);
    }
    // Edge points.
    std::vector<DVec3> edgePoint(ne);
    std::vector<double> edgeCrease(ne, 0.0);
    std::vector<uint8_t> edgeBoundary(ne, 0u);
    for (uint32_t e = 0; e < ne; ++e) {
        const uint32_t a = static_cast<uint32_t>(edgeKey[e] >> 32);
        const uint32_t b = static_cast<uint32_t>(edgeKey[e] & 0xFFFFFFFFu);
        const DVec3 mid = dvec3Scale(dvec3Add(in.positions[a], in.positions[b]), 0.5);
        edgeCrease[e] = creaseOf(in, edgeKey[e]);
        if (edgeFace1[e] < 0) {
            edgeBoundary[e] = 1u;
            edgePoint[e] = mid;
            continue;
        }
        const DVec3 smooth = dvec3Scale(
                dvec3Add(dvec3Add(in.positions[a], in.positions[b]),
                         dvec3Add(facePoint[edgeFace0[e]], facePoint[static_cast<size_t>(edgeFace1[e])])),
                0.25);
        edgePoint[e] = lerp(smooth, mid, edgeCrease[e]);
    }
    // Vertex points: incident faces and edges, in index order.
    std::vector<std::vector<uint32_t>> vertexFaces(nv);
    std::vector<std::vector<uint32_t>> vertexEdges(nv);
    for (uint32_t f = 0; f < nf; ++f) {
        for (uint32_t k = 0; k < 4; ++k) vertexFaces[in.quads[f * 4u + k]].push_back(f);
    }
    for (uint32_t e = 0; e < ne; ++e) {
        vertexEdges[static_cast<uint32_t>(edgeKey[e] >> 32)].push_back(e);
        vertexEdges[static_cast<uint32_t>(edgeKey[e] & 0xFFFFFFFFu)].push_back(e);
    }
    auto otherEnd = [&edgeKey](uint32_t e, uint32_t v) {
        const uint32_t a = static_cast<uint32_t>(edgeKey[e] >> 32);
        const uint32_t b = static_cast<uint32_t>(edgeKey[e] & 0xFFFFFFFFu);
        return a == v ? b : a;
    };
    std::vector<DVec3> vertexPoint(nv);
    for (uint32_t v = 0; v < nv; ++v) {
        const DVec3 V = in.positions[v];
        std::vector<uint32_t> boundary;
        std::vector<uint32_t> creased;
        for (uint32_t e : vertexEdges[v]) {
            if (edgeBoundary[e]) boundary.push_back(e);
            else if (edgeCrease[e] > 0.0) creased.push_back(e);
        }
        if (!boundary.empty()) {
            if (vertexFaces[v].size() <= 1u || boundary.size() != 2u) {
                vertexPoint[v] = V;  // a corner holds still
            } else {
                const DVec3 a = in.positions[otherEnd(boundary[0], v)];
                const DVec3 b = in.positions[otherEnd(boundary[1], v)];
                vertexPoint[v] = dvec3Scale(dvec3Add(dvec3Scale(V, 6.0), dvec3Add(a, b)), 0.125);
            }
            continue;
        }
        const double n = static_cast<double>(vertexEdges[v].size());
        DVec3 q{0.0, 0.0, 0.0};
        for (uint32_t f : vertexFaces[v]) q = dvec3Add(q, facePoint[f]);
        q = dvec3Scale(q, 1.0 / static_cast<double>(vertexFaces[v].size()));
        DVec3 r{0.0, 0.0, 0.0};
        for (uint32_t e : vertexEdges[v]) {
            r = dvec3Add(r, dvec3Scale(dvec3Add(V, in.positions[otherEnd(e, v)]), 0.5));
        }
        r = dvec3Scale(r, 1.0 / n);
        const DVec3 smooth =
                dvec3Scale(dvec3Add(dvec3Add(q, dvec3Scale(r, 2.0)), dvec3Scale(V, n - 3.0)), 1.0 / n);
        if (creased.size() == 2u) {
            const DVec3 a = in.positions[otherEnd(creased[0], v)];
            const DVec3 b = in.positions[otherEnd(creased[1], v)];
            const DVec3 crease = dvec3Scale(dvec3Add(dvec3Scale(V, 6.0), dvec3Add(a, b)), 0.125);
            vertexPoint[v] = lerp(smooth, crease, 0.5 * (edgeCrease[creased[0]] + edgeCrease[creased[1]]));
        } else if (creased.size() >= 3u) {
            double weight = 0.0;
            for (uint32_t e : creased) weight += edgeCrease[e];
            vertexPoint[v] = lerp(smooth, V, weight / static_cast<double>(creased.size()));
        } else {
            vertexPoint[v] = smooth;
        }
    }

    // The child level: [vertex points | edge points | face points].
    QuadLevel next;
    next.positions.reserve(static_cast<size_t>(nv) + ne + nf);
    next.positions.insert(next.positions.end(), vertexPoint.begin(), vertexPoint.end());
    next.positions.insert(next.positions.end(), edgePoint.begin(), edgePoint.end());
    next.positions.insert(next.positions.end(), facePoint.begin(), facePoint.end());
    next.quads.reserve(static_cast<size_t>(nf) * 16u);
    next.origin.reserve(static_cast<size_t>(nf) * 4u);
    for (uint32_t f = 0; f < nf; ++f) {
        const uint32_t fp = nv + ne + f;
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t vk = in.quads[f * 4u + k];
            const uint32_t ek = nv + faceSideEdge[f * 4u + k];
            const uint32_t ekm = nv + faceSideEdge[f * 4u + (k + 3u) % 4u];
            next.quads.insert(next.quads.end(), {vk, ek, fp, ekm});
            next.origin.push_back(in.origin[f]);
        }
    }
    // Each creased parent edge's two halves keep its weight.
    for (uint32_t e = 0; e < ne; ++e) {
        if (edgeCrease[e] <= 0.0 || edgeBoundary[e]) continue;
        const uint32_t a = static_cast<uint32_t>(edgeKey[e] >> 32);
        const uint32_t b = static_cast<uint32_t>(edgeKey[e] & 0xFFFFFFFFu);
        next.creases.emplace_back(pairKey(a, nv + e), edgeCrease[e]);
        next.creases.emplace_back(pairKey(b, nv + e), edgeCrease[e]);
    }
    std::sort(next.creases.begin(), next.creases.end());
    *out = std::move(next);
}

}  // namespace

FreeformStatus subdivideFreeformCageAt(const FreeformCage& cage, uint32_t level, FreeformMesh* out) {
    if (level > kMaxFreeformSubdivisionLevel) return FreeformStatus::InvalidSubdivisionLevel;
    FreeformCage atLevel = cage;
    atLevel.subdivisionLevel = static_cast<uint8_t>(level);
    const FreeformStatus why = validateFreeformCage(atLevel);
    if (why != FreeformStatus::Ok) return why;
    FreeformTopology topology;
    buildFreeformTopology(cage, &topology);

    QuadLevel current;
    current.positions.reserve(cage.vertices.size());
    for (const FreeformVertex& v : cage.vertices) current.positions.push_back(v.position);
    current.quads = topology.faceVertices;
    for (const FreeformFace& face : cage.faces) current.origin.push_back(face.id);
    for (size_t e = 0; e < cage.edges.size(); ++e) {
        if (cage.edges[e].crease <= 0.0) continue;
        // The edge's two vertex indices, read off the first face side that uses it.
        const int32_t f = topology.edgeFaces[e * 2u];
        if (f < 0) continue;
        const size_t fu = static_cast<size_t>(f);
        for (int k = 0; k < 4; ++k) {
            if (topology.faceEdges[fu * 4u + k] == e) {
                current.creases.emplace_back(pairKey(topology.faceVertices[fu * 4u + k],
                                                     topology.faceVertices[fu * 4u + (k + 1) % 4]),
                                             cage.edges[e].crease);
                break;
            }
        }
    }
    std::sort(current.creases.begin(), current.creases.end());
    for (uint32_t l = 0; l < level; ++l) {
        QuadLevel next;
        subdivideOnce(current, &next);
        current = std::move(next);
    }

    FreeformMesh mesh;
    mesh.level = level;
    mesh.positions = std::move(current.positions);
    mesh.quads = std::move(current.quads);
    mesh.quadFace = std::move(current.origin);
    mesh.render.vertices.resize(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        MeshVertex& v = mesh.render.vertices[i];
        v.position[0] = static_cast<float>(mesh.positions[i].x);
        v.position[1] = static_cast<float>(mesh.positions[i].y);
        v.position[2] = static_cast<float>(mesh.positions[i].z);
        if (!std::isfinite(v.position[0]) || !std::isfinite(v.position[1])
            || !std::isfinite(v.position[2])) {
            return FreeformStatus::NonFinite;
        }
        for (int c = 0; c < 3; ++c) v.color[c] = kFreeformVertexColor[c];
    }
    const size_t quadCount = mesh.quads.size() / 4u;
    mesh.render.indices.reserve(quadCount * 6u);
    mesh.triangleFace.reserve(quadCount * 2u);
    for (size_t q = 0; q < quadCount; ++q) {
        const uint32_t* v = &mesh.quads[q * 4u];
        mesh.render.indices.insert(mesh.render.indices.end(), {v[0], v[1], v[2], v[0], v[2], v[3]});
        mesh.triangleFace.push_back(mesh.quadFace[q]);
        mesh.triangleFace.push_back(mesh.quadFace[q]);
    }
    // An open cage is a sheet: drawn and picked from both sides.
    bool open = false;
    for (uint8_t count : topology.edgeFaceCount) open = open || count == 1u;
    mesh.render.renderBothSides = open;
    if (out != nullptr) *out = std::move(mesh);
    return FreeformStatus::Ok;
}

FreeformStatus subdivideFreeformCage(const FreeformCage& cage, FreeformMesh* out) {
    return subdivideFreeformCageAt(cage, cage.subdivisionLevel, out);
}

uint64_t freeformMeshDigest(const FreeformMesh& mesh) {
    uint64_t hash = 0xCBF29CE484222325ull;
    auto mix = [&hash](const void* data, size_t size) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 0x100000001B3ull;
        }
    };
    for (const DVec3& p : mesh.positions) {
        mix(&p.x, sizeof(double));
        mix(&p.y, sizeof(double));
        mix(&p.z, sizeof(double));
    }
    mix(mesh.quads.data(), mesh.quads.size() * sizeof(uint32_t));
    return hash;
}

}  // namespace forgeshape
