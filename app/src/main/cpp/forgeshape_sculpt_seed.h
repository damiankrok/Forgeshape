// A sculptable seed from a solid's derived mesh (`CAD-SCULPT-R1`).
//
// A CAD Body's truth is its feature chain; what it DRAWS is a derived mesh made
// for drawing: vertices duplicated along every hard edge (each face carries its
// own copy so the renderer can shade it flat), and as few triangles as the
// shape allows — a box is twelve, a long wall is two thin slivers. A brush moves
// the vertices inside its radius, so on that mesh it would tear the faces apart
// at every edge and find nothing to move in the middle of a face.
//
// This module turns such a mesh into one a brush can work: ONE connected
// surface (vertices welded by exact position, so a stroke across an edge pulls
// both faces together) with edges no longer than a fraction of the part's size
// (`kSculptSeedEdgeDivisions`), refined by conforming edge splits so no
// T-junction or crack is ever introduced. It is a pure function of the mesh it
// is given — no camera, no scene, no session — and deterministic: the same
// solid always seeds the same vertices in the same order.
//
// It is called ONCE, by `buildSculptSourceMesh`, when a CAD Body is frozen (and
// when it is reset from its source). The CAD Body is never written: its chain
// stays the truth, and a later CAD edit only marks the sculpt mesh stale.
#pragma once

#include <cstdint>

#include "forgeshape_construction.h"

namespace forgeshape {

// The target edge length is the part's bounding-box diagonal divided by this:
// about a hundred brush-sized steps across the part, fine enough to carve a
// detail, coarse enough that a whole part stays interactive.
constexpr float kSculptSeedEdgeDivisions = 96.0f;
// The refinement stops before a pass would exceed this many triangles: a seed
// the brush cannot move at an interactive rate is no seed at all.
constexpr uint32_t kSculptSeedMaxTriangles = 320000u;
// A bound on the passes, each of which at most halves the longest edges.
constexpr int kSculptSeedMaxPasses = 16;

struct SculptSeedReport {
    uint32_t sourceVertices = 0;
    uint32_t sourceTriangles = 0;
    uint32_t weldedVertices = 0;
    uint32_t vertices = 0;
    uint32_t triangles = 0;
    int passes = 0;
    float targetEdgeMeters = 0.0f;
    // True when the triangle budget, not the target edge length, ended the
    // refinement: the seed is coarser than asked for, and the log says so.
    bool budgetReached = false;
};

// Welds `solid` by exact position (a signed zero is one position), drops the
// triangles the weld made degenerate, and refines it until no edge is longer
// than the target or the budget is reached. Winding is preserved, so the seed
// faces the way the solid did. False, writing nothing, for an empty or
// non-finite solid or one with no triangle left after the weld.
bool buildSculptSeedFromSolid(const ConstructionMesh& solid, ConstructionMesh* out,
                              SculptSeedReport* report = nullptr);

}  // namespace forgeshape
