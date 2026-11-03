#include "forgeshape_mass_properties.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace forgeshape {

namespace {

DVec3 worldPoint(const Mat4& model, const MeshVertex& v) {
    const Vec3 p = mat4TransformPoint(model, Vec3{v.position[0], v.position[1], v.position[2]});
    return DVec3{p.x, p.y, p.z};
}

DVec3 cross(const DVec3& a, const DVec3& b) {
    return DVec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

}  // namespace

bool computeMassProperties(const MeshVertex* vertices, uint32_t vertexCount,
                           const uint32_t* indices, uint32_t indexCount, const Mat4& model,
                           MassProperties* out) {
    if (out == nullptr || vertices == nullptr || indices == nullptr || vertexCount == 0
        || indexCount < 3) {
        return false;
    }
    MassProperties m;
    // Weld by LOCAL position, so a render-only duplicate of a hard edge's
    // vertex is one vertex for the closedness question.
    std::map<std::tuple<float, float, float>, uint32_t> weld;
    std::vector<uint32_t> canon(vertexCount);
    std::vector<DVec3> world(vertexCount);
    bool first = true;
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const MeshVertex& v = vertices[i];
        const auto key = std::make_tuple(v.position[0], v.position[1], v.position[2]);
        canon[i] = weld.emplace(key, i).first->second;
        world[i] = worldPoint(model, v);
        const DVec3& p = world[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
            return false;
        }
        if (first) {
            m.boundsMin = p;
            m.boundsMax = p;
            first = false;
        }
        m.boundsMin = DVec3{std::min(m.boundsMin.x, p.x), std::min(m.boundsMin.y, p.y),
                            std::min(m.boundsMin.z, p.z)};
        m.boundsMax = DVec3{std::max(m.boundsMax.x, p.x), std::max(m.boundsMax.y, p.y),
                            std::max(m.boundsMax.z, p.z)};
    }
    std::map<std::pair<uint32_t, uint32_t>, int> edges;
    double volume = 0.0;
    DVec3 moment{0.0, 0.0, 0.0};
    for (uint32_t t = 0; t + 2 < indexCount; t += 3) {
        const uint32_t i0 = indices[t];
        const uint32_t i1 = indices[t + 1];
        const uint32_t i2 = indices[t + 2];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            return false;
        }
        const DVec3& a = world[i0];
        const DVec3& b = world[i1];
        const DVec3& c = world[i2];
        const DVec3 n = cross(dvec3Sub(b, a), dvec3Sub(c, a));
        m.area += 0.5 * std::sqrt(dvec3Dot(n, n));
        // The signed tetrahedron from the origin: its volume, and its centroid
        // (a + b + c) / 4 weighted by that volume.
        const double tet = dvec3Dot(a, cross(b, c)) / 6.0;
        volume += tet;
        moment = dvec3Add(moment, dvec3Scale(dvec3Add(dvec3Add(a, b), c), tet / 4.0));
        const uint32_t w[3] = {canon[i0], canon[i1], canon[i2]};
        for (int k = 0; k < 3; ++k) {
            const uint32_t p = w[k];
            const uint32_t q = w[(k + 1) % 3];
            if (p == q) continue;
            ++edges[std::make_pair(std::min(p, q), std::max(p, q))];
        }
    }
    m.closed = !edges.empty();
    for (const auto& e : edges) {
        if (e.second != 2) {
            m.closed = false;
            break;
        }
    }
    if (m.closed && std::fabs(volume) > 1.0e-18) {
        // A mesh wound inward measures negative: the magnitude is the volume,
        // and the centroid does not depend on the sign.
        m.centroid = dvec3Scale(moment, 1.0 / volume);
        m.volume = std::fabs(volume);
    } else {
        m.closed = false;
        m.centroid = dvec3Scale(dvec3Add(m.boundsMin, m.boundsMax), 0.5);
    }
    *out = m;
    return true;
}

}  // namespace forgeshape
