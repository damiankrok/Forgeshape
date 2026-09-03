// What the renderer is handed to draw a sketch in progress: a line list in
// WORLD space, in ranges that are drawn with different weights.
//
// Presentation in the strongest sense the project has, exactly like the grid
// and the gizmo: it has no ObjectId, is never published through a MeshStore,
// is never picked by pickScene, is never exported and never reaches a `.forge`
// byte. The renderer cannot learn a sketch coordinate from it -- the vertices
// are already world positions -- and nothing may read a dimension back out of
// it.
//
// Platform-neutral C++17: no Vulkan type. The renderer uploads it through the
// gizmo's own line pipeline, so it borrows the gizmo's vertex layout: `axis`
// carries a hue tag and `handle` carries the emphasis tag (1 = drawn in the
// highlight colour, 0 = the range's neutral).
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "forgeshape_gizmo.h"

namespace forgeshape {

// How one range of the overlay is weighted. A closed enum and a switch in the
// renderer: the grid's rhythm is two weights, the axes carry a hue, and the
// entities are drawn at full weight.
enum class SketchOverlayStyle : uint8_t {
    GridMinor,
    GridMajor,
    Axes,
    Entities,
};

struct SketchOverlayRange {
    uint32_t firstVertex = 0;
    uint32_t vertexCount = 0;
    SketchOverlayStyle style = SketchOverlayStyle::Entities;
};

struct SketchOverlay {
    // Monotonic per session. The renderer re-uploads only when it changes, so
    // a frame with no sketch change costs no transfer.
    uint64_t revision = 0;
    std::vector<GizmoVertex> vertices;
    std::vector<SketchOverlayRange> ranges;
};

using SketchOverlayPtr = std::shared_ptr<const SketchOverlay>;

// The largest overlay the renderer will hold. The session bounds itself well
// below this; the renderer refuses rather than grows past it.
constexpr uint32_t kMaxSketchOverlayVertices = 65536;

}  // namespace forgeshape
