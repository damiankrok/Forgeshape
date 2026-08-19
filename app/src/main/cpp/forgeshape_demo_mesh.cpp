#include "forgeshape_demo_mesh.h"

namespace forgeshape {
namespace {

constexpr float kHalf = kDemoCubeHalfExtent;

// 8 logical corners, 12 triangles. Colour is per-corner, so the faces are
// gradients; that is enough to tell the cube's orientation apart by eye.
const MeshVertex kVertices[8] = {
    {{-kHalf, -kHalf, -kHalf}, {0.10f, 0.15f, 0.85f}},
    {{ kHalf, -kHalf, -kHalf}, {0.95f, 0.25f, 0.15f}},
    {{ kHalf,  kHalf, -kHalf}, {0.98f, 0.85f, 0.15f}},
    {{-kHalf,  kHalf, -kHalf}, {0.15f, 0.85f, 0.35f}},
    {{-kHalf, -kHalf,  kHalf}, {0.10f, 0.75f, 0.90f}},
    {{ kHalf, -kHalf,  kHalf}, {0.90f, 0.35f, 0.75f}},
    {{ kHalf,  kHalf,  kHalf}, {0.98f, 0.98f, 0.98f}},
    {{-kHalf,  kHalf,  kHalf}, {0.45f, 0.30f, 0.85f}},
};

// Every triangle is counter-clockwise seen from OUTSIDE the cube, which is the
// canonical ForgeShape convention documented in forgeshape_picking.h. The
// self-tests assert this for all 12 triangles, so back-face culling and
// front-face-only picking cannot silently disagree with the data.
//
// 32-bit since Stage 006: the whole runtime/render path uses uint32_t indices so
// future meshes are not structurally capped at 65,535 vertices.
const uint32_t kIndices[36] = {
    4, 5, 6, 6, 7, 4,  // +Z
    1, 0, 3, 3, 2, 1,  // -Z
    0, 4, 7, 7, 3, 0,  // -X
    5, 1, 2, 2, 6, 5,  // +X
    3, 7, 6, 6, 2, 3,  // +Y
    0, 1, 5, 5, 4, 0,  // -Y
};

}  // namespace

const MeshVertex* demoCubeVertices() { return kVertices; }

uint32_t demoCubeVertexCount() {
    return static_cast<uint32_t>(sizeof(kVertices) / sizeof(kVertices[0]));
}

const uint32_t* demoCubeIndices() { return kIndices; }

uint32_t demoCubeIndexCount() {
    return static_cast<uint32_t>(sizeof(kIndices) / sizeof(kIndices[0]));
}

TriangleMeshView demoCubeMeshView() {
    TriangleMeshView view{};
    view.positions = kVertices[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = demoCubeVertexCount();
    view.indices = kIndices;
    view.indexCount = demoCubeIndexCount();
    return view;
}

}  // namespace forgeshape
