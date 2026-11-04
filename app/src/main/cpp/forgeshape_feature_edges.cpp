#include "forgeshape_feature_edges.h"

#include <cmath>
#include <cstring>
#include <unordered_map>

namespace forgeshape {

namespace {

struct PositionKey {
    uint32_t bits[3];
    bool operator==(const PositionKey& o) const {
        return bits[0] == o.bits[0] && bits[1] == o.bits[1] && bits[2] == o.bits[2];
    }
};

struct PositionKeyHash {
    size_t operator()(const PositionKey& k) const {
        uint64_t h = 1469598103934665603ull;
        for (uint32_t b : k.bits) {
            h ^= b;
            h *= 1099511628211ull;
        }
        return static_cast<size_t>(h);
    }
};

PositionKey keyOf(const float p[3]) {
    PositionKey k{};
    for (int i = 0; i < 3; ++i) {
        // -0 and +0 are the same corner.
        const float v = p[i] == 0.0f ? 0.0f : p[i];
        std::memcpy(&k.bits[i], &v, sizeof(float));
    }
    return k;
}

struct EdgeRecord {
    uint32_t a = 0, b = 0;      // render indices that stand at the two corners
    int faces = 0;
    float n0[3] = {0, 0, 0};    // the first face's unit normal
    bool crease = false;
};

}  // namespace

bool deriveFeatureEdges(const RenderVertex* vertices, size_t vertexCount, const uint32_t* indices,
                        size_t indexCount, std::vector<uint32_t>* outPairs) {
    if (outPairs == nullptr || vertices == nullptr || indices == nullptr || indexCount % 3 != 0) {
        return false;
    }
    if (indexCount / 3 > kFeatureEdgeMaxTriangles) {
        return false;
    }
    for (size_t i = 0; i < indexCount; ++i) {
        if (indices[i] >= vertexCount) return false;
    }
    // Weld by exact position: one id per distinct corner.
    std::unordered_map<PositionKey, uint32_t, PositionKeyHash> welded;
    welded.reserve(vertexCount);
    std::vector<uint32_t> cornerOf(vertexCount);
    for (size_t v = 0; v < vertexCount; ++v) {
        const auto inserted = welded.emplace(keyOf(vertices[v].position),
                                             static_cast<uint32_t>(welded.size()));
        cornerOf[v] = inserted.first->second;
    }
    const float cosCrease = std::cos(kFeatureEdgeCreaseDegrees * 3.14159265358979f / 180.0f);
    std::unordered_map<uint64_t, EdgeRecord> edges;
    edges.reserve(indexCount);
    for (size_t t = 0; t + 2 < indexCount; t += 3) {
        const uint32_t i0 = indices[t], i1 = indices[t + 1], i2 = indices[t + 2];
        const float* p0 = vertices[i0].position;
        const float* p1 = vertices[i1].position;
        const float* p2 = vertices[i2].position;
        const double ux = p1[0] - p0[0], uy = p1[1] - p0[1], uz = p1[2] - p0[2];
        const double vx = p2[0] - p0[0], vy = p2[1] - p0[1], vz = p2[2] - p0[2];
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (!(len > 1e-20)) continue;  // degenerate: no face, no edge
        nx /= len;
        ny /= len;
        nz /= len;
        const uint32_t corner[3] = {cornerOf[i0], cornerOf[i1], cornerOf[i2]};
        const uint32_t render[3] = {i0, i1, i2};
        if (corner[0] == corner[1] || corner[1] == corner[2] || corner[0] == corner[2]) continue;
        for (int e = 0; e < 3; ++e) {
            uint32_t ca = corner[e], cb = corner[(e + 1) % 3];
            uint32_t ra = render[e], rb = render[(e + 1) % 3];
            if (ca > cb) {
                std::swap(ca, cb);
                std::swap(ra, rb);
            }
            const uint64_t key = (static_cast<uint64_t>(ca) << 32) | cb;
            EdgeRecord& rec = edges[key];
            if (rec.faces == 0) {
                rec.a = ra;
                rec.b = rb;
                rec.n0[0] = static_cast<float>(nx);
                rec.n0[1] = static_cast<float>(ny);
                rec.n0[2] = static_cast<float>(nz);
            } else if (rec.faces == 1) {
                const double d = rec.n0[0] * nx + rec.n0[1] * ny + rec.n0[2] * nz;
                rec.crease = d < cosCrease;
            }
            ++rec.faces;
        }
    }
    const size_t before = outPairs->size();
    for (const auto& entry : edges) {
        const EdgeRecord& rec = entry.second;
        if (rec.faces == 1 || rec.faces > 2 || rec.crease) {
            outPairs->push_back(rec.a);
            outPairs->push_back(rec.b);
        }
    }
    (void)before;
    return true;
}

}  // namespace forgeshape
