// The feature edges a viewport draws over a shaded body (`WORKBENCH-UX-R1`):
// the dark lines along a part's real edges that make a CAD part read as one --
// a box's twelve, a cylinder's two rims, a hole's bore ends -- where shading
// alone let two faces of a similar tone, or a dark face and the dark ground,
// merge.
//
// PRESENTATION in the strongest sense: derived from the render mesh the body
// already draws, held beside it on the GPU, rebuilt only when that mesh is, and
// never serialized, compared or read back. It is ONE rule for every
// representation -- a crease angle, the threshold the shading already splits
// normals on -- so it asks nothing about what a body is: a 32-segment
// cylinder's facets part by 11.25 degrees and draw no line, its rims part by 90
// and do, and two coplanar caps of an Add draw no seam.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "forgeshape_render_mesh.h"

namespace forgeshape {

// Two faces whose normals part by more than this draw an edge between them.
constexpr float kFeatureEdgeCreaseDegrees = 40.0f;

// Above this many triangles a body draws no edges: the derivation is linear but
// it runs on the render thread, and a body that large is a scan or a sculpt,
// whose creases are noise rather than edges.
constexpr size_t kFeatureEdgeMaxTriangles = 400000;

// Appends to `outPairs` two indices into `vertices` per feature edge: every
// edge whose two triangles' normals part by more than the crease angle, every
// open boundary edge, and every edge more than two triangles share. Vertices
// are welded by EXACT position first, because the render mesh splits a vertex
// at each hard edge and those copies must count as one corner. Degenerate
// triangles are skipped. Returns false (writing nothing) when the input is
// malformed or over the bound.
bool deriveFeatureEdges(const RenderVertex* vertices, size_t vertexCount, const uint32_t* indices,
                        size_t indexCount, std::vector<uint32_t>* outPairs);

}  // namespace forgeshape
