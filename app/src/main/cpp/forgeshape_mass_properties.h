// What a body measures (`CAD-INSPECT-R1`): its volume, surface area, centre of
// mass and world bounding box, read off the mesh it draws.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no camera. A pure
// READ of a published mesh and a world model -- no revision, no history step,
// no `.forge` byte, no fingerprint, and nothing here is ever stored. The
// numbers are in metres (m, m², m³), in WORLD space, so a body's placement and
// scale are measured as the user sees them.
#pragma once

#include <cstdint>

#include "forgeshape_math.h"
#include "forgeshape_mesh.h"

namespace forgeshape {

struct MassProperties {
    // Whether the mesh is a closed surface (every edge, welded by position,
    // shared by exactly two triangles): only then are the volume and the
    // centre of mass meaningful.
    bool closed = false;
    double volume = 0.0;        // m³, by the divergence theorem; 0 when open
    double area = 0.0;          // m²
    DVec3 centroid{0.0, 0.0, 0.0};  // the centre of mass of uniform material
    DVec3 boundsMin{0.0, 0.0, 0.0};
    DVec3 boundsMax{0.0, 0.0, 0.0};
};

// Measures `indexCount / 3` triangles of `vertices` placed by `model`. False
// for an empty or non-finite mesh.
bool computeMassProperties(const MeshVertex* vertices, uint32_t vertexCount,
                           const uint32_t* indices, uint32_t indexCount, const Mat4& model,
                           MassProperties* out);

}  // namespace forgeshape
