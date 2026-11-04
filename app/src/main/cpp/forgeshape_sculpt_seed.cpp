#include "forgeshape_sculpt_seed.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace forgeshape {

namespace {

uint32_t floatBits(float value) {
    // One position for +0 and -0: the weld is about WHERE a vertex is, and a
    // signed zero is the same place.
    if (value == 0.0f) {
        value = 0.0f;
    }
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

struct PositionKey {
    uint32_t x, y, z;
    bool operator==(const PositionKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct PositionKeyHash {
    size_t operator()(const PositionKey& k) const {
        uint64_t h = 1469598103934665603ull;
        for (uint32_t part : {k.x, k.y, k.z}) {
            h ^= part;
            h *= 1099511628211ull;
        }
        return static_cast<size_t>(h);
    }
};

uint64_t edgeKey(uint32_t a, uint32_t b) {
    const uint32_t lo = std::min(a, b);
    const uint32_t hi = std::max(a, b);
    return (static_cast<uint64_t>(lo) << 32) | hi;
}

float edgeLengthSquared(const std::vector<MeshVertex>& v, uint32_t a, uint32_t b) {
    const float dx = v[a].position[0] - v[b].position[0];
    const float dy = v[a].position[1] - v[b].position[1];
    const float dz = v[a].position[2] - v[b].position[2];
    return dx * dx + dy * dy + dz * dz;
}

}  // namespace

bool buildSculptSeedFromSolid(const ConstructionMesh& solid, ConstructionMesh* out,
                              SculptSeedReport* report) {
    if (out == nullptr || solid.vertices.empty() || solid.indices.size() < 3
        || (solid.indices.size() % 3u) != 0u) {
        return false;
    }
    SculptSeedReport facts;
    facts.sourceVertices = static_cast<uint32_t>(solid.vertices.size());
    facts.sourceTriangles = static_cast<uint32_t>(solid.indices.size() / 3u);

    // --- weld by exact position, in first-occurrence order ------------------
    std::vector<MeshVertex> vertices;
    vertices.reserve(solid.vertices.size());
    std::vector<uint32_t> remap(solid.vertices.size(), 0u);
    std::unordered_map<PositionKey, uint32_t, PositionKeyHash> seen;
    seen.reserve(solid.vertices.size() * 2u);
    float lo[3] = {INFINITY, INFINITY, INFINITY};
    float hi[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (size_t i = 0; i < solid.vertices.size(); ++i) {
        const MeshVertex& v = solid.vertices[i];
        for (int a = 0; a < 3; ++a) {
            if (!std::isfinite(v.position[a])) {
                return false;
            }
            lo[a] = std::min(lo[a], v.position[a]);
            hi[a] = std::max(hi[a], v.position[a]);
        }
        const PositionKey key{floatBits(v.position[0]), floatBits(v.position[1]),
                              floatBits(v.position[2])};
        auto found = seen.find(key);
        if (found != seen.end()) {
            remap[i] = found->second;
            continue;
        }
        const uint32_t index = static_cast<uint32_t>(vertices.size());
        MeshVertex copy = v;
        copy.mask = 0.0f;
        vertices.push_back(copy);
        seen.emplace(key, index);
        remap[i] = index;
    }
    facts.weldedVertices = static_cast<uint32_t>(vertices.size());

    std::vector<uint32_t> triangles;
    triangles.reserve(solid.indices.size());
    for (size_t t = 0; t + 2 < solid.indices.size(); t += 3) {
        const uint32_t ia = solid.indices[t], ib = solid.indices[t + 1], ic = solid.indices[t + 2];
        if (ia >= remap.size() || ib >= remap.size() || ic >= remap.size()) {
            return false;
        }
        const uint32_t a = remap[ia], b = remap[ib], c = remap[ic];
        if (a == b || b == c || c == a) {
            continue;  // the weld closed it up
        }
        triangles.push_back(a);
        triangles.push_back(b);
        triangles.push_back(c);
    }
    if (triangles.empty()) {
        return false;
    }

    const float dx = hi[0] - lo[0], dy = hi[1] - lo[1], dz = hi[2] - lo[2];
    const float diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float target = diagonal / kSculptSeedEdgeDivisions;
    facts.targetEdgeMeters = target;

    // --- conforming refinement: split every edge longer than the target ----
    //
    // Each pass marks edges (an edge is marked once, for both triangles that
    // share it) and replaces every triangle by 1, 2, 3 or 4 according to how
    // many of its edges are marked. Because the decision belongs to the EDGE,
    // the two triangles beside a split edge both use its one midpoint, so no
    // pass can leave a T-junction or a crack.
    if (target > 0.0f && std::isfinite(target)) {
        const float target2 = target * target;
        for (int pass = 0; pass < kSculptSeedMaxPasses; ++pass) {
            std::unordered_map<uint64_t, uint32_t> midpoint;  // edge -> new vertex
            const size_t triangleCount = triangles.size() / 3u;
            uint64_t predicted = 0;
            for (size_t t = 0; t < triangleCount; ++t) {
                int marked = 0;
                for (int e = 0; e < 3; ++e) {
                    const uint32_t a = triangles[t * 3 + e];
                    const uint32_t b = triangles[t * 3 + (e + 1) % 3];
                    if (edgeLengthSquared(vertices, a, b) > target2) {
                        ++marked;
                    }
                }
                predicted += static_cast<uint64_t>(marked == 0 ? 1 : marked + 1);
            }
            if (predicted == triangleCount) {
                break;  // nothing longer than the target is left
            }
            if (predicted > kSculptSeedMaxTriangles) {
                facts.budgetReached = true;
                break;
            }
            // Midpoints in first-encounter order, so the seed is deterministic.
            for (size_t t = 0; t < triangleCount; ++t) {
                for (int e = 0; e < 3; ++e) {
                    const uint32_t a = triangles[t * 3 + e];
                    const uint32_t b = triangles[t * 3 + (e + 1) % 3];
                    if (edgeLengthSquared(vertices, a, b) <= target2) {
                        continue;
                    }
                    const uint64_t key = edgeKey(a, b);
                    if (midpoint.count(key) != 0u) {
                        continue;
                    }
                    MeshVertex mid = vertices[a];
                    for (int k = 0; k < 3; ++k) {
                        mid.position[k] = 0.5f * (vertices[a].position[k] + vertices[b].position[k]);
                        mid.color[k] = 0.5f * (vertices[a].color[k] + vertices[b].color[k]);
                    }
                    mid.mask = 0.0f;
                    midpoint.emplace(key, static_cast<uint32_t>(vertices.size()));
                    vertices.push_back(mid);
                }
            }
            std::vector<uint32_t> next;
            next.reserve(static_cast<size_t>(predicted) * 3u);
            const auto emit = [&next](uint32_t a, uint32_t b, uint32_t c) {
                next.push_back(a);
                next.push_back(b);
                next.push_back(c);
            };
            for (size_t t = 0; t < triangleCount; ++t) {
                uint32_t v[3] = {triangles[t * 3], triangles[t * 3 + 1], triangles[t * 3 + 2]};
                bool split[3];
                int marked = 0;
                for (int e = 0; e < 3; ++e) {
                    split[e] = midpoint.count(edgeKey(v[e], v[(e + 1) % 3])) != 0u;
                    marked += split[e] ? 1 : 0;
                }
                if (marked == 0) {
                    emit(v[0], v[1], v[2]);
                    continue;
                }
                if (marked == 3) {
                    const uint32_t ab = midpoint[edgeKey(v[0], v[1])];
                    const uint32_t bc = midpoint[edgeKey(v[1], v[2])];
                    const uint32_t ca = midpoint[edgeKey(v[2], v[0])];
                    emit(v[0], ab, ca);
                    emit(ab, v[1], bc);
                    emit(ca, bc, v[2]);
                    emit(ab, bc, ca);
                    continue;
                }
                // Rotate cyclically (which keeps the winding) so that, for one
                // split edge, it is v0->v1, and for two, the UNSPLIT one is v2->v0.
                for (int turn = 0; turn < 3; ++turn) {
                    const bool done = marked == 1 ? split[0] : !split[2];
                    if (done) {
                        break;
                    }
                    const uint32_t tv = v[0];
                    v[0] = v[1];
                    v[1] = v[2];
                    v[2] = tv;
                    const bool ts = split[0];
                    split[0] = split[1];
                    split[1] = split[2];
                    split[2] = ts;
                }
                if (marked == 1) {
                    const uint32_t ab = midpoint[edgeKey(v[0], v[1])];
                    emit(v[0], ab, v[2]);
                    emit(ab, v[1], v[2]);
                    continue;
                }
                const uint32_t ab = midpoint[edgeKey(v[0], v[1])];
                const uint32_t bc = midpoint[edgeKey(v[1], v[2])];
                emit(ab, v[1], bc);
                // The quad v0-ab-bc-v2, cut along its shorter diagonal.
                if (edgeLengthSquared(vertices, v[0], bc) <= edgeLengthSquared(vertices, ab, v[2])) {
                    emit(v[0], ab, bc);
                    emit(v[0], bc, v[2]);
                } else {
                    emit(v[0], ab, v[2]);
                    emit(ab, bc, v[2]);
                }
            }
            triangles.swap(next);
            ++facts.passes;
            if (vertices.size() >= kMaxMeshVertices) {
                facts.budgetReached = true;
                break;
            }
        }
    }

    out->vertices = std::move(vertices);
    out->indices = std::move(triangles);
    out->renderBothSides = solid.renderBothSides;
    facts.vertices = static_cast<uint32_t>(out->vertices.size());
    facts.triangles = static_cast<uint32_t>(out->indices.size() / 3u);
    if (report != nullptr) {
        *report = facts;
    }
    return true;
}

}  // namespace forgeshape
