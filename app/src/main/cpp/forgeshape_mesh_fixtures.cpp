#include "forgeshape_mesh_fixtures.h"

#include <cmath>

#include "forgeshape_demo_mesh.h"
#include "forgeshape_math.h"

namespace forgeshape {
namespace {

FixtureMesh baselineCopy() {
    FixtureMesh mesh;
    mesh.vertices.assign(demoCubeVertices(), demoCubeVertices() + demoCubeVertexCount());
    mesh.indices.assign(demoCubeIndices(), demoCubeIndices() + demoCubeIndexCount());
    return mesh;
}

// Applies a positive-determinant affine map, so the canonical outward winding
// of every triangle is preserved and neither culling nor front-face-only
// picking can start disagreeing with the data.
void deform(FixtureMesh& mesh, float sx, float sy, float sz, float shearXfromY) {
    for (MeshVertex& v : mesh.vertices) {
        const float x = v.position[0];
        const float y = v.position[1];
        const float z = v.position[2];
        v.position[0] = x * sx + y * shearXfromY;
        v.position[1] = y * sy;
        v.position[2] = z * sz;
    }
}

}  // namespace

FixtureMesh buildFixtureBaseline() { return baselineCopy(); }

FixtureMesh buildFixtureDeformed() {
    FixtureMesh mesh = baselineCopy();
    // Wide, flattened and skewed: unmistakable against the baseline cube from
    // the default camera pose, with identical vertex and index counts.
    deform(mesh, 1.85f, 0.55f, 1.0f, 0.60f);
    return mesh;
}

FixtureMesh buildFixtureLarge() {
    // Six subdivided faces, each vertex pushed out to a fixed radius: a
    // "spherified box". Deterministic, closed, and visibly a different shape
    // and a different size from the cube.
    constexpr float kRadius = 1.28f;
    const uint32_t n = kFixtureLargeSubdivisions;
    const float half = kDemoCubeHalfExtent;

    struct Face {
        Vec3 normal;
        Vec3 u;
        Vec3 v;
    };
    // u x v == normal for every face, which is what makes the quad order
    // (p00, p10, p11, p01) counter-clockwise seen from outside.
    const Face faces[6] = {
        {{ 1.0f, 0.0f, 0.0f}, { 0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-1.0f, 0.0f, 0.0f}, { 0.0f, 0.0f,  1.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f,  1.0f, 0.0f}, { 1.0f, 0.0f,  0.0f}, {0.0f, 0.0f, -1.0f}},
        {{0.0f, -1.0f, 0.0f}, { 1.0f, 0.0f,  0.0f}, {0.0f, 0.0f,  1.0f}},
        {{0.0f, 0.0f,  1.0f}, { 1.0f, 0.0f,  0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f,  0.0f}, {0.0f, 1.0f, 0.0f}},
    };

    FixtureMesh mesh;
    mesh.vertices.reserve(static_cast<size_t>(6) * (n + 1) * (n + 1));
    mesh.indices.reserve(static_cast<size_t>(6) * n * n * 6);

    for (const Face& face : faces) {
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        for (uint32_t j = 0; j <= n; ++j) {
            for (uint32_t i = 0; i <= n; ++i) {
                const float fu = (static_cast<float>(i) / static_cast<float>(n)) * 2.0f - 1.0f;
                const float fv = (static_cast<float>(j) / static_cast<float>(n)) * 2.0f - 1.0f;
                Vec3 p = vec3Scale(face.normal, half);
                p = vec3Add(p, vec3Scale(face.u, fu * half));
                p = vec3Add(p, vec3Scale(face.v, fv * half));
                const Vec3 dir = vec3Normalize(p);
                const Vec3 out = vec3Scale(dir, kRadius);

                MeshVertex vertex{};
                vertex.position[0] = out.x;
                vertex.position[1] = out.y;
                vertex.position[2] = out.z;
                // Direction-derived colour, so orientation stays readable and
                // the fixture is obviously not the baseline cube.
                vertex.color[0] = 0.5f + 0.5f * dir.x;
                vertex.color[1] = 0.5f + 0.5f * dir.y;
                vertex.color[2] = 0.5f + 0.5f * dir.z;
                mesh.vertices.push_back(vertex);
            }
        }
        const uint32_t row = n + 1;
        for (uint32_t j = 0; j < n; ++j) {
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t p00 = base + j * row + i;
                const uint32_t p10 = p00 + 1;
                const uint32_t p01 = p00 + row;
                const uint32_t p11 = p01 + 1;
                mesh.indices.push_back(p00);
                mesh.indices.push_back(p10);
                mesh.indices.push_back(p11);
                mesh.indices.push_back(p00);
                mesh.indices.push_back(p11);
                mesh.indices.push_back(p01);
            }
        }
    }
    return mesh;
}

FixtureMesh buildStressStep(uint32_t step) {
    FixtureMesh mesh = baselineCopy();
    const float s = static_cast<float>(step);
    // Every factor stays inside [0.55, 1.45], so the determinant is positive at
    // every step and the topology never changes.
    const float sx = 1.0f + 0.45f * std::sin(s * 0.21f);
    const float sy = 1.0f + 0.45f * std::sin(s * 0.37f + 1.1f);
    const float sz = 1.0f + 0.45f * std::sin(s * 0.13f + 2.3f);
    deform(mesh, sx, sy, sz, 0.0f);
    return mesh;
}

}  // namespace forgeshape
