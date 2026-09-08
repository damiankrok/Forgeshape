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
    // The technical-drawing dimension annotation on a selected straight Line
    // (`SKETCH-UX-R1` E): extension lines, a dimension line and its two end
    // ticks. An INTERACTION overlay in the strongest sense -- it is not
    // geometry, it is never extruded, exported or serialized, and the numeric
    // label beside it is drawn by the shell rather than by these vertices.
    Dimension,
};

// How many values SketchOverlayStyle has, stated beside the enum on purpose.
//
// A style with no mapping is not a compile error and not a crash: it draws at
// alpha 0, which is INVISIBLE with every test still green. That is exactly what
// happened to `Dimension` between `SKETCH-UX-R1` and `UI-3D-STATE-C2`, so the
// count is here for the exhaustive case in the gizmo self-test to walk, and a
// value added above without moving it is caught by the mapping's own switch,
// which has no `default:`.
constexpr int kSketchOverlayStyleCount = 5;

// The per-style half of what the renderer pushes to draw ONE overlay range: the
// two scalars that differ between styles. Everything else in the push -- the
// view-projection, the three axis hues, the highlight colour, the emphasis tag
// -- is the same for every range and stays in the renderer.
//
// Extracted from the renderer's switch so every value the enum can take is
// reachable without a Vulkan device, which is the only way "this style is
// drawn at all" can be a test rather than a screenshot.
struct SketchOverlayStyleWeights {
    // Packed into `axisX.w`: the grey a vertex with no hue tag is drawn in.
    float neutralLevel = 0.0f;
    // Packed into `highlight.w`: the base alpha `gizmo.vert` multiplies every
    // vertex of the range by. Never 0 for a style the renderer draws.
    float alpha = 0.0f;
};

// The mapping. A pure function over values: no renderer, no device, no frame.
//
// Returns false, writing nothing, for a style code outside the enum -- the
// renderer skips such a range rather than recording a draw that could not be
// seen.
bool sketchOverlayStyleWeights(SketchOverlayStyle style, ViewportBackground background,
                               SketchOverlayStyleWeights* out);

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
