// Catmull-Clark subdivision of a Freeform control cage
// (`MODELING-FOUNDATIONS-R1` B).
//
// Platform-neutral C++17. A pure function of the cage: no camera, no zoom, no
// viewport, no clock, no hash order -- the same cage at the same level always
// yields the same positions bit for bit, in binary64, and the same indices.
//
// The rules (each level, over an all-quad mesh)
// ---------------------------------------------
//   face point    F = the mean of the face's four corners.
//   edge point    a BOUNDARY edge: its midpoint M. An interior edge: the smooth
//                 point S = (v0 + v1 + F0 + F1) / 4, blended toward M by the
//                 edge's crease weight c: S + c (M - S).
//   vertex point  a BOUNDARY vertex on one face (a corner): itself. On two or
//                 more faces: (6 V + a + b) / 8 over its two boundary
//                 neighbours. An INTERIOR vertex of valence n: the smooth point
//                 S = (Q + 2 R + (n - 3) V) / n (Q the mean of its face points,
//                 R the mean of its edge midpoints), blended by its creased
//                 edges -- exactly two: toward (6 V + a + b) / 8 by their mean
//                 weight; three or more: toward V (a corner) by their mean
//                 weight; one: unchanged (a dart).
//   children      every quad becomes four quads (V_k, E_k, F, E_k-1), keeping
//                 the parent's winding; each half of a parent edge inherits its
//                 crease weight, unchanged at every level (a held weight, not a
//                 decaying semi-sharpness); edges inside a face are smooth.
//
// Level 0 is the cage itself. Each derived quad remembers the CONTROL face it
// came from, which is how a tap on the smooth surface resolves to a cage face
// -- never by a triangle index.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_freeform.h"
#include "forgeshape_math.h"

namespace forgeshape {

// The derived surface. Transient: built from a cage, published as a render
// mesh, never stored, compared or serialized.
struct FreeformMesh {
    // binary64 positions and quads of the limit-approximating surface.
    std::vector<DVec3> positions;
    std::vector<uint32_t> quads;  // 4 per quad, outward winding
    // Per derived quad: the control face it came from.
    std::vector<FreeformFaceId> quadFace;
    // The render mesh: float positions, two triangles per quad (0,1,2)(0,2,3),
    // and per TRIANGLE the control face.
    ConstructionMesh render;
    std::vector<FreeformFaceId> triangleFace;
    uint32_t level = 0;
};

// Subdivides `cage` to its own stored level. Refuses (writing nothing) a cage
// that does not validate.
FreeformStatus subdivideFreeformCage(const FreeformCage& cage, FreeformMesh* out);

// The same at an explicit level (0..4), for verification and measurement.
FreeformStatus subdivideFreeformCageAt(const FreeformCage& cage, uint32_t level, FreeformMesh* out);

// FNV-1a 64 over the derived positions' bits and the quad indices: equal for
// equal surfaces, so determinism is a value that can be compared.
uint64_t freeformMeshDigest(const FreeformMesh& mesh);

}  // namespace forgeshape
