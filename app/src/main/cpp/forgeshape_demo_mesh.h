// The bootstrap demo cube DATA.
//
// THIS IS NOT THE GEOMETRY CORE and NOT the Construction mesh format. It is the
// single hard-coded cube the viewport has drawn since Stage 003, kept as the
// baseline geometry that the debug mesh fixtures build on.
//
// Since Stage 006 it is no longer read directly by the renderer or by picking:
// both of those consume whatever revision `MeshStore` currently publishes. This
// file only supplies the numbers for the baseline fixture.
//
// No JNI, no Android, no Vulkan types: the renderer describes this data to
// Vulkan, it does not live here.
#pragma once

#include <cstdint>

#include "forgeshape_mesh.h"

namespace forgeshape {

// Half extent of the demo cube, in world units.
constexpr float kDemoCubeHalfExtent = 0.9f;

const MeshVertex* demoCubeVertices();
uint32_t demoCubeVertexCount();

const uint32_t* demoCubeIndices();
uint32_t demoCubeIndexCount();

// Non-owning triangle view over the baseline data. Used by the self-tests to
// assert the canonical winding convention on the original cube; the runtime
// render/pick paths go through MeshStore instead.
TriangleMeshView demoCubeMeshView();

}  // namespace forgeshape
