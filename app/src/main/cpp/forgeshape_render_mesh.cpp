#include "forgeshape_render_mesh.h"

#include <chrono>
#include <cmath>
#include <cstring>

namespace forgeshape {
namespace {

// Reads a source vertex position as a Vec3. The source stores bare float[3] so
// it can be memcpy'd; the arithmetic here wants the vector type.
inline Vec3 positionOf(const MeshVertex& v) {
    return Vec3{v.position[0], v.position[1], v.position[2]};
}

inline void writeVec3(float (&dst)[3], const Vec3& v) {
    dst[0] = v.x;
    dst[1] = v.y;
    dst[2] = v.z;
}

// One triangle's geometric normal, UNNORMALIZED.
//
// `(v1 - v0) x (v2 - v0)` has length 2*area, so summing these weights a large
// triangle more than a sliver at no extra cost — the same area weighting
// computeVertexNormals() uses for the sculpt cache, deliberately, so a surface
// does not change character between the two paths. A degenerate triangle
// contributes the zero vector rather than a NaN.
inline Vec3 faceNormalWeighted(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 n = vec3Cross(vec3Sub(b, a), vec3Sub(c, a));
    return vec3Finite(n) ? n : Vec3{0.0f, 0.0f, 0.0f};
}

// The Sculpt Mask weight a render vertex inherits from its source vertex.
//
// Clamped HERE, once, because this is the last CPU stage before the vertex
// buffer: the domain already refuses a non-finite or out-of-range mask
// (SculptMesh::setMaskWeight), and every other producer leaves the field at
// zero, so this exists to make the shader's input provably in [0, 1] without
// the shader having to defend against a value that cannot legitimately reach
// it. A non-finite value is 0 — no mask — rather than an invented one.
inline float sanitizedMask(const MeshVertex& v) {
    if (!std::isfinite(v.mask) || v.mask <= 0.0f) {
        return 0.0f;
    }
    return (v.mask >= 1.0f) ? 1.0f : v.mask;
}

// A tiny union-find over the corners meeting at ONE vertex.
//
// It is local and stack-sized on purpose: the sets being merged are a single
// vertex's incident triangles (six for a box corner, thirty-two for a sphere
// pole), never the whole mesh, so there is no global structure to allocate,
// invalidate or keep in sync.
uint32_t findRoot(std::vector<uint32_t>& parent, uint32_t x) {
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];  // path halving
        x = parent[x];
    }
    return x;
}

void unite(std::vector<uint32_t>& parent, uint32_t a, uint32_t b) {
    a = findRoot(parent, a);
    b = findRoot(parent, b);
    if (a != b) {
        // Lower index wins, so grouping is deterministic and the emitted vertex
        // order does not depend on iteration incidentals.
        if (b < a) {
            parent[a] = b;
        } else {
            parent[b] = a;
        }
    }
}

double nowMillis() {
    using clock = std::chrono::steady_clock;
    const auto t = clock::now().time_since_epoch();
    return std::chrono::duration<double, std::milli>(t).count();
}

// Faceted: every triangle becomes three private vertices carrying that
// triangle's own flat normal. This intentionally exposes triangle structure —
// that is what the mode is for — and it is the simplest possible build, with no
// adjacency and no grouping.
bool buildFaceted(const MeshVertex* vertices, const uint32_t* indices, uint32_t indexCount,
                  RenderMeshData* out) {
    const uint32_t triangleCount = indexCount / 3;

    out->vertices.resize(indexCount);
    out->indices.resize(indexCount);

    for (uint32_t t = 0; t < triangleCount; ++t) {
        const uint32_t i0 = indices[t * 3 + 0];
        const uint32_t i1 = indices[t * 3 + 1];
        const uint32_t i2 = indices[t * 3 + 2];

        const Vec3 p0 = positionOf(vertices[i0]);
        const Vec3 p1 = positionOf(vertices[i1]);
        const Vec3 p2 = positionOf(vertices[i2]);

        // Zero for a degenerate triangle: honest, finite, and handled by the
        // shader's documented fallback rather than invented here.
        const Vec3 n = vec3Normalize(faceNormalWeighted(p0, p1, p2));

        const uint32_t src[3] = {i0, i1, i2};
        for (uint32_t corner = 0; corner < 3; ++corner) {
            const uint32_t slot = t * 3 + corner;
            RenderVertex& rv = out->vertices[slot];
            const MeshVertex& sv = vertices[src[corner]];
            std::memcpy(rv.position, sv.position, sizeof(rv.position));
            std::memcpy(rv.color, sv.color, sizeof(rv.color));
            rv.mask = sanitizedMask(sv);
            writeVec3(rv.normal, n);
            out->indices[slot] = slot;
        }
    }
    return true;
}

// Smooth: group each vertex's incident triangles by the crease policy and emit
// one render vertex per group.
bool buildSmooth(const MeshVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                 uint32_t indexCount, RenderMeshData* out) {
    const uint32_t triangleCount = indexCount / 3;
    const float cosThreshold = creaseCosineThreshold();

    // Per-triangle normals: the weighted one is what gets summed, the unit one
    // is what the angle test compares. Computing both once avoids normalizing
    // the same face up to three times.
    std::vector<Vec3> weighted(triangleCount);
    std::vector<Vec3> unitNormal(triangleCount);
    for (uint32_t t = 0; t < triangleCount; ++t) {
        const Vec3 p0 = positionOf(vertices[indices[t * 3 + 0]]);
        const Vec3 p1 = positionOf(vertices[indices[t * 3 + 1]]);
        const Vec3 p2 = positionOf(vertices[indices[t * 3 + 2]]);
        weighted[t] = faceNormalWeighted(p0, p1, p2);
        unitNormal[t] = vec3Normalize(weighted[t]);
    }

    // Corner lists in flat CSR form: for each source vertex, which corners
    // (index-buffer slots) refer to it. Counting pass, then a fill pass, so
    // nothing reallocates per vertex.
    std::vector<uint32_t> cornerStart(vertexCount + 1, 0);
    for (uint32_t c = 0; c < indexCount; ++c) {
        ++cornerStart[indices[c] + 1];
    }
    for (uint32_t v = 0; v < vertexCount; ++v) {
        cornerStart[v + 1] += cornerStart[v];
    }
    std::vector<uint32_t> cornerList(indexCount);
    {
        std::vector<uint32_t> cursor(cornerStart.begin(), cornerStart.end() - 1);
        for (uint32_t c = 0; c < indexCount; ++c) {
            cornerList[cursor[indices[c]]++] = c;
        }
    }

    // Where each corner ended up, so the index buffer can be remapped after
    // every vertex has been split.
    std::vector<uint32_t> renderIndexOfCorner(indexCount, 0);

    out->vertices.clear();
    out->vertices.reserve(vertexCount);

    std::vector<uint32_t> parent;
    std::vector<uint32_t> emitted;

    for (uint32_t v = 0; v < vertexCount; ++v) {
        const uint32_t begin = cornerStart[v];
        const uint32_t end = cornerStart[v + 1];
        const uint32_t count = end - begin;

        if (count == 0) {
            // An orphan source vertex: no triangle uses it. It still has to
            // exist as a render vertex or the index remap would shift, but
            // nothing references it and it gets the zero normal.
            RenderVertex rv{};
            std::memcpy(rv.position, vertices[v].position, sizeof(rv.position));
            std::memcpy(rv.color, vertices[v].color, sizeof(rv.color));
            rv.mask = sanitizedMask(vertices[v]);
            rv.normal[0] = rv.normal[1] = rv.normal[2] = 0.0f;
            out->vertices.push_back(rv);
            continue;
        }

        parent.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            parent[i] = i;
        }

        // Pairwise, because `count` is the number of triangles touching one
        // vertex — six at a box corner, thirty-two at a sphere pole — so the
        // quadratic term is bounded by the tessellation, not by mesh size.
        // Transitivity through union-find is what lets a pole fan whose
        // extreme members are far apart in azimuth still become one group.
        for (uint32_t i = 0; i < count; ++i) {
            const Vec3& ni = unitNormal[cornerList[begin + i] / 3];
            for (uint32_t j = i + 1; j < count; ++j) {
                const Vec3& nj = unitNormal[cornerList[begin + j] / 3];
                // A degenerate face has the zero normal, so this dot product is
                // 0 and it never joins a group: it contributes nothing and
                // cannot drag a real surface off its direction.
                if (vec3Dot(ni, nj) >= cosThreshold) {
                    unite(parent, i, j);
                }
            }
        }

        // One emitted render vertex per group root, in root order, so the
        // output is deterministic.
        emitted.assign(count, UINT32_MAX);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t root = findRoot(parent, i);
            if (emitted[root] == UINT32_MAX) {
                emitted[root] = static_cast<uint32_t>(out->vertices.size());
                RenderVertex rv{};
                std::memcpy(rv.position, vertices[v].position, sizeof(rv.position));
                std::memcpy(rv.color, vertices[v].color, sizeof(rv.color));
                rv.mask = sanitizedMask(vertices[v]);
                rv.normal[0] = rv.normal[1] = rv.normal[2] = 0.0f;
                out->vertices.push_back(rv);
            }
            const uint32_t corner = cornerList[begin + i];
            const uint32_t target = emitted[root];
            renderIndexOfCorner[corner] = target;

            // Accumulate this face's area-weighted normal onto its group.
            RenderVertex& rv = out->vertices[target];
            const Vec3& w = weighted[corner / 3];
            rv.normal[0] += w.x;
            rv.normal[1] += w.y;
            rv.normal[2] += w.z;
        }
    }

    // Normalize once, at the end. A group whose faces cancelled out exactly
    // (or that saw only degenerate faces) keeps the zero vector.
    for (RenderVertex& rv : out->vertices) {
        const Vec3 n = vec3Normalize(Vec3{rv.normal[0], rv.normal[1], rv.normal[2]});
        writeVec3(rv.normal, n);
    }

    out->indices.resize(indexCount);
    for (uint32_t c = 0; c < indexCount; ++c) {
        out->indices[c] = renderIndexOfCorner[c];
    }
    return true;
}

// The bounded two-sided render-only exception (see buildRenderMesh's doc
// comment): duplicates the whole one-sided result, negating every normal and
// reversing every triangle's winding on the copy, so the existing global
// back-face-culling pipeline draws the duplicate from the far side while still
// culling it from the near side. Operates purely on already-built RENDER data;
// touches no source vertex, no source index and no picking topology.
void appendMirroredBackFace(RenderMeshData* out) {
    const uint32_t baseVertexCount = static_cast<uint32_t>(out->vertices.size());
    const uint32_t baseIndexCount = static_cast<uint32_t>(out->indices.size());

    out->vertices.reserve(baseVertexCount * 2);
    for (uint32_t i = 0; i < baseVertexCount; ++i) {
        RenderVertex mirrored = out->vertices[i];
        mirrored.normal[0] = -mirrored.normal[0];
        mirrored.normal[1] = -mirrored.normal[1];
        mirrored.normal[2] = -mirrored.normal[2];
        out->vertices.push_back(mirrored);
    }

    out->indices.reserve(baseIndexCount * 2);
    for (uint32_t t = 0; t < baseIndexCount / 3; ++t) {
        const uint32_t i0 = out->indices[t * 3 + 0];
        const uint32_t i1 = out->indices[t * 3 + 1];
        const uint32_t i2 = out->indices[t * 3 + 2];
        // Reversed winding, on the mirrored (normal-negated) vertex set.
        out->indices.push_back(baseVertexCount + i0);
        out->indices.push_back(baseVertexCount + i2);
        out->indices.push_back(baseVertexCount + i1);
    }
}

}  // namespace

const char* surfaceShadingName(SurfaceShading shading) {
    switch (shading) {
        case SurfaceShading::Smooth: return "Smooth";
        case SurfaceShading::Faceted: return "Faceted";
    }
    return "Unknown";
}

float creaseCosineThreshold() {
    constexpr float kPi = 3.14159265358979323846f;
    return std::cos(kCreaseAngleDegrees * kPi / 180.0f);
}

void RenderMeshData::clear() {
    vertices.clear();
    indices.clear();
    sourceVertexCount = 0;
    sourceIndexCount = 0;
    shading = SurfaceShading::Smooth;
}

bool renderMeshIsFinite(const RenderMeshData& data) {
    for (const RenderVertex& v : data.vertices) {
        for (int i = 0; i < 3; ++i) {
            if (!std::isfinite(v.position[i]) || !std::isfinite(v.normal[i])) {
                return false;
            }
        }
        // Either a unit normal or the honest zero. Anything in between means
        // the normalization step was skipped for this vertex.
        const float lenSq = v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] +
                            v.normal[2] * v.normal[2];
        if (lenSq > 1e-6f && std::fabs(lenSq - 1.0f) > 1e-3f) {
            return false;
        }
    }
    return true;
}

bool buildRenderMesh(const MeshVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                     uint32_t indexCount, SurfaceShading shading, RenderMeshData* out,
                     bool renderBothSides) {
    if (out == nullptr) {
        return false;
    }
    // The same fail-closed gate the authoritative mesh uses, so render data can
    // never be built from something RuntimeMesh itself would have refused.
    if (validateMeshData(vertices, vertexCount, indices, indexCount) != MeshValidation::Ok) {
        return false;
    }
    // Faceted is the worst case at three render vertices per triangle, which is
    // exactly indexCount, doubled again when the two-sided exception applies.
    // Checking that bound covers both modes, because smooth grouping can never
    // emit more vertices than there are corners.
    const uint64_t worstCaseVertices =
        static_cast<uint64_t>(indexCount) * (renderBothSides ? 2u : 1u);
    if (worstCaseVertices > kMaxMeshVertices) {
        return false;
    }

    RenderMeshData built;
    const bool ok = (shading == SurfaceShading::Faceted)
                        ? buildFaceted(vertices, indices, indexCount, &built)
                        : buildSmooth(vertices, vertexCount, indices, indexCount, &built);
    if (!ok) {
        return false;
    }

    if (renderBothSides) {
        appendMirroredBackFace(&built);
    }

    built.sourceVertexCount = vertexCount;
    built.sourceIndexCount = indexCount;
    built.shading = shading;

    // Nothing partial: the caller's data is replaced only once a complete,
    // finite result exists.
    if (!renderMeshIsFinite(built)) {
        return false;
    }
    *out = std::move(built);
    return true;
}

bool RenderMeshCache::refresh(const RuntimeMesh& source, SurfaceShading shading, bool* outFailed) {
    if (outFailed != nullptr) {
        *outFailed = false;
    }

    // THE rebuild gate. Everything that is not a new revision or a different
    // surface shading stops here, which is what keeps normal generation off the
    // per-frame path. renderBothSides is a fixed fact of the source mesh's own
    // topology (see RuntimeMesh::renderBothSides()), not a separate axis of
    // change: it cannot flip without the revision changing too, because the
    // only way to get a different renderBothSides value is to publish a
    // different mesh.
    if (valid_ && sourceRevision_ == source.revision() && sourceObjectId_ == source.objectId() &&
        shading_ == shading) {
        ++skippedRefreshCount_;
        return false;
    }

    const double startMillis = nowMillis();
    RenderMeshData built;
    if (!buildRenderMesh(source.vertices(), source.vertexCount(), source.indices(),
                         source.indexCount(), shading, &built, source.renderBothSides())) {
        // Keep whatever was already cached: one unbuildable revision must not
        // blank the viewport.
        ++failedRebuildCount_;
        if (outFailed != nullptr) {
            *outFailed = true;
        }
        return false;
    }

    data_ = std::move(built);
    valid_ = true;
    sourceRevision_ = source.revision();
    sourceObjectId_ = source.objectId();
    shading_ = shading;
    ++rebuildCount_;
    lastRebuildMillis_ = nowMillis() - startMillis;
    return true;
}

void RenderMeshCache::invalidate() {
    valid_ = false;
    sourceRevision_ = kNoMeshRevision;
    sourceObjectId_ = kNoObject;
}

}  // namespace forgeshape
