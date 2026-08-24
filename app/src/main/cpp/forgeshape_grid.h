// ForgeShape world reference grid — a viewport REFERENCE, never model geometry.
//
// Platform-independent: no JNI, no Android, no Vulkan and no UI type appears
// here. What this file owns is the grid's *contract* — where its plane is, how
// far it reaches, how often a line falls and what each line is for — plus one
// pure generator that turns that contract into line vertices.
//
// The grid is presentation in the strongest sense the project has. It is not a
// Construction Body and not part of one:
//
//   * it has no ObjectId and never enters ConstructionScene or SceneSnapshot;
//   * it has no MeshRevision and is never published through a MeshStore;
//   * it is not a RuntimeMesh, so nothing about it is pickable — pickScene
//     iterates scene items and the grid is not one;
//   * it takes no part in Freeze, in Resume or in a sculpt stroke;
//   * it is not exported, and in this stage it is not a snap target either.
//
// A future Sketch grid — the one with snapping, drawn on a sketch plane rather
// than on the world floor — is a DIFFERENT contract with its own approval, and
// must not be grown out of this file.
//
// Ownership of *visibility* is elsewhere: DisplaySettingsStore, beside the
// shading model and the viewport background, because it is the same class of
// value. This file answers only "what does the grid look like when it is on".
#pragma once

#include <cstdint>

#include "forgeshape_display.h"

namespace forgeshape {

// The grid lies on the world XZ plane at y = 0, in a +Y-up right-handed world —
// the same convention every other file in the project states. It is a floor,
// not a backdrop, and it does not rotate, tilt or follow the camera.
constexpr float kGridPlaneY = 0.0f;

// How far a minor line is from the next one, in WORLD METERS.
//
// One meter, deliberately, and the reason is the camera rather than taste: the
// initial orbit distance is 8.2 m at a 60 deg vertical field of view, so the
// default framing is about 9.5 m tall and a one-meter cell puts roughly ten
// cells across the viewport. That is dense enough to judge scale and sparse
// enough to read at a glance — and because the spacing IS the unit the Property
// Inspector shows, counting cells is counting meters with no conversion.
constexpr float kGridMinorSpacingMeters = 1.0f;

// Every fifth minor line is drawn as a major one, giving a 5 m rhythm on top of
// the 1 m one. Two weights are enough to see structure; a third would be a CAD
// decade system, which this stage is explicitly told not to build.
constexpr int kGridMajorEveryNMinor = 5;

// How far the grid reaches from the origin along each axis, in world meters.
//
// Twenty meters either way covers roughly four screen-heights at the default
// framing and about two Construction Bodies' worth of placement room beyond
// anything the exact-value editors currently make easy to author. It is a fixed
// extent on purpose: a grid that re-tessellated itself as the camera dollied
// would be a multi-decade CAD grid system, and nothing here needs one. The
// radial fade below is what keeps a fixed extent from ending in a visible
// square edge.
constexpr float kGridHalfExtentMeters = 20.0f;

// Where the radial fade begins, as a fraction of the half extent. Inside this
// the grid is at full weight; outside it dissolves to nothing by the border, so
// the extent reads as a horizon rather than as a cut. Applied in the shader,
// per vertex, from the vertex's own distance to the world origin in XZ.
constexpr float kGridFadeStartFraction = 0.55f;

// What a given line is FOR, which is the only thing that decides how it is
// drawn. A closed enum and a switch — the same rule the sculpt tools and the
// shading models follow.
enum class GridLineTier {
    Minor,  // the 1 m rhythm
    Major,  // every kGridMajorEveryNMinor-th line
    AxisX,  // the world X axis: the line through the origin running along X
    AxisZ,  // the world Z axis: the line through the origin running along Z
};

constexpr int kGridLineTierCount = 4;

// One end of one line. Position is world space; `tier` is a GridLineTier's
// numeric value carried as a float because it travels as a vertex attribute.
//
// The tier is baked into the buffer and the COLOUR is not, which is what makes
// switching appearance free: a theme change rewrites four push-constant vec4s
// and re-uploads nothing.
struct GridVertex {
    float position[3];
    float tier;
};

// How many lines and vertices the contract above produces. Both are compile-
// time constants because the grid is generated once, at device creation, and
// never regenerated: there is no parameter a user can move.
constexpr int kGridLinesPerAxis =
    2 * static_cast<int>(kGridHalfExtentMeters / kGridMinorSpacingMeters) + 1;
constexpr int kGridLineCount = 2 * kGridLinesPerAxis;
constexpr int kGridVertexCount = 2 * kGridLineCount;

// Which tier the line at `index` belongs to, where index 0 is the line furthest
// in the negative direction and kGridLinesPerAxis - 1 the furthest positive.
//
// `alongX` says which way the LINE runs, not which axis it is offset on: the
// line through the origin that runs along X is the X axis, and it is the one at
// the centre index of the set offset along Z.
GridLineTier gridLineTier(int index, bool alongX);

// Fills `out` with kGridVertexCount vertices: every line as an unconnected
// pair, ready for VK_PRIMITIVE_TOPOLOGY_LINE_LIST. Pure, deterministic and
// allocation-free; `out` must have room for kGridVertexCount entries.
//
// Returns the number of vertices written, so a caller sizing a buffer from the
// constant and a caller reading the result cannot disagree.
int generateGridVertices(GridVertex* out, int capacity);

// The colour a tier is drawn in for a given viewport appearance: RGB plus the
// alpha it is blended at.
//
// This is the grid's half of the narrow presentation seam viewportBackgroundColor
// already established. The geometry domain still never learns what an Android
// theme is — what crosses is a ViewportBackground, and native code owns what
// each appearance looks like. Nothing above JNI authors these values.
void gridLineColor(ViewportBackground background, GridLineTier tier, float* outRgba);

}  // namespace forgeshape
