#include "forgeshape_cad_kernel.h"

#include <algorithm>
#include <cmath>
#include <numeric>

// The ONE translation unit that includes the vendored kernel. See
// forgeshape_cad_kernel.h for why nothing else may.
#include "manifold/manifold.h"
#include "manifold/polygon.h"

namespace forgeshape {
namespace {

bool arraysWellFormed(const CadSolid& solid) {
    if (solid.positions.size() % 3u != 0u || solid.indices.size() % 3u != 0u) {
        return false;
    }
    if (solid.faceTags.size() != solid.indices.size() / 3u) {
        return false;
    }
    const size_t vertexCount = solid.positions.size() / 3u;
    for (uint32_t index : solid.indices) {
        if (index >= vertexCount) {
            return false;
        }
    }
    return true;
}

bool positionsFinite(const std::vector<double>& positions) {
    for (double value : positions) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

manifold::MeshGL64 toMesh(const CadSolid& solid) {
    manifold::MeshGL64 mesh;
    mesh.numProp = 3;
    mesh.vertProperties = solid.positions;
    mesh.triVerts.assign(solid.indices.begin(), solid.indices.end());
    mesh.faceID.assign(solid.faceTags.begin(), solid.faceTags.end());
    return mesh;
}

// Whether every undirected edge is used by exactly two triangles, once in each
// direction: the closed, consistently oriented 2-manifold the kernel requires.
// Checked HERE rather than left to the kernel, because the kernel's own answer
// to an open mesh is a status the caller cannot tell apart from a bug.
bool closedAndOriented(const CadSolid& solid) {
    struct Edge {
        uint32_t a;
        uint32_t b;
    };
    std::vector<Edge> edges;
    edges.reserve(solid.indices.size());
    for (size_t t = 0; t + 2 < solid.indices.size(); t += 3) {
        for (int k = 0; k < 3; ++k) {
            const uint32_t a = solid.indices[t + static_cast<size_t>(k)];
            const uint32_t b = solid.indices[t + static_cast<size_t>((k + 1) % 3)];
            if (a == b) {
                return false;
            }
            edges.push_back(Edge{a, b});
        }
    }
    std::sort(edges.begin(), edges.end(), [](const Edge& x, const Edge& y) {
        return x.a != y.a ? x.a < y.a : x.b < y.b;
    });
    // Each directed edge must appear exactly once, and its reverse exactly once.
    for (size_t i = 0; i < edges.size(); ++i) {
        if (i + 1 < edges.size() && edges[i + 1].a == edges[i].a && edges[i + 1].b == edges[i].b) {
            return false;
        }
        const Edge reverse{edges[i].b, edges[i].a};
        const bool found = std::binary_search(
                edges.begin(), edges.end(), reverse, [](const Edge& x, const Edge& y) {
                    return x.a != y.a ? x.a < y.a : x.b < y.b;
                });
        if (!found) {
            return false;
        }
    }
    return !edges.empty();
}

// The kernel's own validity answer for a mesh it was handed.
CadKernelStatus fromKernelError(manifold::Manifold::Error error) {
    switch (error) {
        case manifold::Manifold::Error::NoError:
            return CadKernelStatus::Ok;
        case manifold::Manifold::Error::NonFiniteVertex:
            return CadKernelStatus::NonFinite;
        case manifold::Manifold::Error::NotManifold:
            return CadKernelStatus::NotManifold;
        case manifold::Manifold::Error::ResultTooLarge:
            return CadKernelStatus::ResultTooLarge;
        default:
            return CadKernelStatus::KernelError;
    }
}

// The kernel result, canonicalised: triangles grouped by ascending face tag
// (stable within a tag, so the kernel's own deterministic order decides ties)
// and vertices renumbered in first-use order, unused ones dropped.
CadKernelStatus fromKernelResult(const manifold::Manifold& result, CadSolid* out) {
    const CadKernelStatus status = fromKernelError(result.Status());
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    const manifold::MeshGL64 mesh = result.GetMeshGL64();
    const size_t triangleCount = mesh.triVerts.size() / 3u;
    if (triangleCount > kMaxCadKernelTriangles) {
        return CadKernelStatus::ResultTooLarge;
    }
    if (mesh.numProp < 3 || mesh.faceID.size() != triangleCount) {
        return CadKernelStatus::KernelError;
    }
    std::vector<uint32_t> order(triangleCount);
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [&mesh](uint32_t x, uint32_t y) {
        return mesh.faceID[x] < mesh.faceID[y];
    });

    CadSolid solid;
    const size_t vertexCount = mesh.vertProperties.size() / mesh.numProp;
    std::vector<int64_t> remap(vertexCount, -1);
    solid.indices.reserve(triangleCount * 3u);
    solid.faceTags.reserve(triangleCount);
    for (uint32_t t : order) {
        for (int k = 0; k < 3; ++k) {
            const uint64_t source = mesh.triVerts[static_cast<size_t>(t) * 3u + static_cast<size_t>(k)];
            if (source >= vertexCount) {
                return CadKernelStatus::KernelError;
            }
            if (remap[source] < 0) {
                remap[source] = static_cast<int64_t>(solid.positions.size() / 3u);
                for (int c = 0; c < 3; ++c) {
                    solid.positions.push_back(
                            mesh.vertProperties[source * mesh.numProp + static_cast<size_t>(c)]);
                }
            }
            solid.indices.push_back(static_cast<uint32_t>(remap[source]));
        }
        const uint64_t tag = mesh.faceID[t];
        if (tag > 0xFFFFFFFFull) {
            return CadKernelStatus::KernelError;
        }
        solid.faceTags.push_back(static_cast<uint32_t>(tag));
    }
    if (!positionsFinite(solid.positions)) {
        return CadKernelStatus::NonFinite;
    }
    *out = std::move(solid);
    return CadKernelStatus::Ok;
}

CadKernelStatus loadSolid(const CadSolid& solid, manifold::Manifold* out, bool requirePositive) {
    if (!arraysWellFormed(solid)) {
        return CadKernelStatus::InvalidInput;
    }
    if (!positionsFinite(solid.positions)) {
        return CadKernelStatus::NonFinite;
    }
    if (solid.triangleCount() > kMaxCadKernelTriangles) {
        return CadKernelStatus::ResultTooLarge;
    }
    if (!closedAndOriented(solid)) {
        return CadKernelStatus::NotManifold;
    }
    manifold::Manifold loaded(toMesh(solid));
    const CadKernelStatus status = fromKernelError(loaded.Status());
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    if (requirePositive && !(loaded.Volume() > 0.0)) {
        return CadKernelStatus::InvertedInput;
    }
    *out = std::move(loaded);
    return CadKernelStatus::Ok;
}

void measure(const manifold::Manifold& m, CadSolidMeasure* out) {
    if (out == nullptr) {
        return;
    }
    out->volume = m.Volume();
    out->triangles = static_cast<uint32_t>(m.NumTri());
    out->components = m.IsEmpty() ? 0u : static_cast<uint32_t>(m.Decompose().size());
}

}  // namespace

const char* cadKernelStatusName(CadKernelStatus status) {
    switch (status) {
        case CadKernelStatus::Ok:
            return "Ok";
        case CadKernelStatus::InvalidInput:
            return "InvalidInput";
        case CadKernelStatus::NonFinite:
            return "NonFinite";
        case CadKernelStatus::NotManifold:
            return "NotManifold";
        case CadKernelStatus::InvertedInput:
            return "InvertedInput";
        case CadKernelStatus::ResultTooLarge:
            return "ResultTooLarge";
        case CadKernelStatus::KernelError:
            return "KernelError";
    }
    return "Unknown";
}

CadKernelStatus cadKernelValidateSolid(const CadSolid& solid, CadSolidMeasure* outMeasure) {
    manifold::Manifold loaded;
    const CadKernelStatus status = loadSolid(solid, &loaded, /*requirePositive=*/true);
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    measure(loaded, outMeasure);
    return CadKernelStatus::Ok;
}

CadKernelStatus cadKernelMeasure(const CadSolid& solid, CadSolidMeasure* outMeasure) {
    if (solid.empty()) {
        if (outMeasure != nullptr) {
            *outMeasure = CadSolidMeasure{};
        }
        return CadKernelStatus::Ok;
    }
    manifold::Manifold loaded;
    const CadKernelStatus status = loadSolid(solid, &loaded, /*requirePositive=*/false);
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    measure(loaded, outMeasure);
    return CadKernelStatus::Ok;
}

CadKernelStatus cadKernelBoolean(const CadSolid& a, const CadSolid& b, CadBooleanOp op,
                                 CadSolid* out) {
    if (out == nullptr) {
        return CadKernelStatus::InvalidInput;
    }
    manifold::Manifold left;
    manifold::Manifold right;
    CadKernelStatus status = loadSolid(a, &left, /*requirePositive=*/true);
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    status = loadSolid(b, &right, /*requirePositive=*/true);
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    const manifold::OpType kernelOp =
            op == CadBooleanOp::Union ? manifold::OpType::Add : manifold::OpType::Subtract;
    const manifold::Manifold result = left.Boolean(right, kernelOp);
    CadSolid solid;
    status = fromKernelResult(result, &solid);
    if (status != CadKernelStatus::Ok) {
        return status;
    }
    *out = std::move(solid);
    return CadKernelStatus::Ok;
}

CadKernelStatus cadKernelTriangulateRegion(const std::vector<std::vector<SketchPoint>>& loops,
                                           std::vector<uint32_t>* outIndices) {
    if (outIndices == nullptr || loops.empty()) {
        return CadKernelStatus::InvalidInput;
    }
    manifold::PolygonsIdx polygons;
    polygons.reserve(loops.size());
    int base = 0;
    double totalArea = 0.0;
    for (size_t l = 0; l < loops.size(); ++l) {
        const std::vector<SketchPoint>& loop = loops[l];
        if (loop.size() < 3u) {
            return CadKernelStatus::InvalidInput;
        }
        double twice = 0.0;
        for (size_t i = 0; i < loop.size(); ++i) {
            const SketchPoint& p = loop[i];
            const SketchPoint& q = loop[(i + 1u) % loop.size()];
            if (!std::isfinite(p.u) || !std::isfinite(p.v)) {
                return CadKernelStatus::NonFinite;
            }
            twice += p.u * q.v - q.u * p.v;
        }
        // The outer boundary winds counter-clockwise and every hole clockwise:
        // the kernel's winding-number rule, stated here once so a caller may
        // pass either orientation.
        const bool wantCcw = l == 0u;
        const bool isCcw = twice > 0.0;
        manifold::SimplePolygonIdx contour;
        contour.reserve(loop.size());
        for (size_t i = 0; i < loop.size(); ++i) {
            const size_t source = (wantCcw == isCcw) ? i : loop.size() - 1u - i;
            contour.push_back(manifold::PolyVert{manifold::vec2(loop[source].u, loop[source].v),
                                                 base + static_cast<int>(source)});
        }
        totalArea += l == 0u ? std::fabs(twice) * 0.5 : -std::fabs(twice) * 0.5;
        polygons.push_back(std::move(contour));
        base += static_cast<int>(loop.size());
    }
    if (!(totalArea > 0.0)) {
        return CadKernelStatus::InvalidInput;
    }
    const std::vector<manifold::ivec3> triangles = manifold::TriangulateIdx(polygons);
    if (triangles.empty()) {
        return CadKernelStatus::KernelError;
    }
    // The triangles must cover exactly the region: every one counter-clockwise
    // and their areas summing to outer minus holes. A triangulation that does
    // not is refused rather than published as a cap with a crack in it.
    std::vector<SketchPoint> all;
    all.reserve(static_cast<size_t>(base));
    for (const std::vector<SketchPoint>& loop : loops) {
        all.insert(all.end(), loop.begin(), loop.end());
    }
    double covered = 0.0;
    std::vector<uint32_t> indices;
    indices.reserve(triangles.size() * 3u);
    for (const manifold::ivec3& tri : triangles) {
        for (int k = 0; k < 3; ++k) {
            if (tri[k] < 0 || tri[k] >= base) {
                return CadKernelStatus::KernelError;
            }
        }
        const SketchPoint& p0 = all[static_cast<size_t>(tri[0])];
        const SketchPoint& p1 = all[static_cast<size_t>(tri[1])];
        const SketchPoint& p2 = all[static_cast<size_t>(tri[2])];
        const double twice = (p1.u - p0.u) * (p2.v - p0.v) - (p2.u - p0.u) * (p1.v - p0.v);
        // A sliver at a collinear boundary vertex (two collinear lines in a
        // chain) is legitimately zero-area; a triangle that winds the WRONG
        // way is not, and is refused.
        if (!std::isfinite(twice) || twice < -1.0e-9 * std::max(totalArea, 1.0e-12)) {
            return CadKernelStatus::KernelError;
        }
        covered += twice * 0.5;
        indices.push_back(static_cast<uint32_t>(tri[0]));
        indices.push_back(static_cast<uint32_t>(tri[1]));
        indices.push_back(static_cast<uint32_t>(tri[2]));
    }
    if (std::fabs(covered - totalArea) > 1.0e-9 * std::max(1.0, totalArea)) {
        return CadKernelStatus::KernelError;
    }
    *outIndices = std::move(indices);
    return CadKernelStatus::Ok;
}

const char* cadKernelIdentity() {
    return "manifold-3.5.4";
}

}  // namespace forgeshape
