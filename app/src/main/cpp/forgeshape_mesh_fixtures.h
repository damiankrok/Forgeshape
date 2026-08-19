// DEBUG / TEST mesh fixtures.
//
// These are TEST INFRASTRUCTURE, not product functionality. They exist only so
// that the runtime mesh publication path and the GPU upload path can be proven
// with real, deterministic, visibly distinguishable geometry.
//
// They are NOT primitives, NOT a Construction feature, NOT the Geometry Core,
// and nothing user-facing creates them. `PRODUCT.md` deliberately does not
// document them.
//
// No JNI, no Android, no Vulkan types.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_mesh.h"

namespace forgeshape {

// One built fixture, ready to publish.
struct FixtureMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

// Fixture A — baseline. The original bootstrap cube: 8 vertices, 36 indices.
FixtureMesh buildFixtureBaseline();

// Fixture B — same topology, changed vertices. Identical vertex and index
// counts to Fixture A, with a deterministic non-uniform stretch and shear, so a
// publish of this fixture must reuse the existing GPU capacity. All scale
// factors are positive, so the canonical outward winding is preserved.
FixtureMesh buildFixtureDeformed();

// Fixture C — larger replacement. A deterministically subdivided, spherified
// box: materially more vertices and indices than the baseline, visibly
// different, still a closed triangle solid with canonical outward winding.
// Subdivision level is fixed so the counts are reproducible.
constexpr uint32_t kFixtureLargeSubdivisions = 6;  // per box edge
FixtureMesh buildFixtureLarge();

// Stress step — baseline topology, deformed as a deterministic function of the
// step index. Same counts as Fixture A, so repeated publication exercises the
// capacity-reuse path rather than reallocation.
FixtureMesh buildStressStep(uint32_t step);

}  // namespace forgeshape
