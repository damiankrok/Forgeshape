// ForgeShape derived render geometry — shading normals and the crease policy.
//
// Platform-independent: no JNI, no Android, no Vulkan and no UI type appears
// here, and nothing here holds a GPU resource. The renderer consumes what this
// module produces; this module knows nothing about the renderer.
//
// What this is, and what it is emphatically NOT
// ---------------------------------------------
// This is PRESENTATION data. It is derived, one-way, from a published
// RuntimeMesh:
//
//     Construction / Sculpt truth
//         -> authoritative RuntimeMesh (positions + indices + colour)
//             -> RenderMeshData (positions + NORMALS + colour)   <-- this file
//                 -> Vulkan buffers
//
// Nothing here is ever read back. A normal is never a dimension, never a
// Construction parameter, never a sculpt deformation and never picking
// topology. CPU picking continues to run on the SOURCE RuntimeMesh, which is
// why this module may freely duplicate vertices: a render vertex has no
// identity that anything outside the renderer can observe.
//
// Render-only vertex duplication
// ------------------------------
// A hard edge needs two different normals at the same position, and a vertex
// carries exactly one normal, so a corner that sits on a crease must become
// more than one RENDER vertex. Consequently:
//
//   * render vertex count >= source vertex count, and usually differs;
//   * render index count == source index count, always — a corner is remapped,
//     never added or removed, so the triangle list is the same triangles;
//   * diagnostics that report a count must say whether it is `source` or
//     `render`. They are different numbers about different things.
//
// The source RuntimeMesh is not modified, not re-published and not revised by
// anything in this file. Building render data mints no MeshRevision and no
// SculptRevision.
//
// Adjacency ownership
// -------------------
// This module derives its own adjacency from the index buffer it is handed. It
// deliberately does NOT reuse SculptTopology: the renderer must depend on the
// published RuntimeMesh and on nothing in the Sculpt domain, or "presentation
// only" would stop being true the moment sculpt state changed shape. The cost
// is one O(vertices + triangles) pass per geometry change, which is the same
// order as the upload it feeds.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_math.h"
#include "forgeshape_mesh.h"

namespace forgeshape {

// One vertex as the graphics pipeline consumes it.
//
// Position and colour are copied verbatim from the source vertex; the normal is
// the only thing this module computes. Colour is retained solely so the
// debug-only source-colour display mode still has something to draw — Studio
// Solid and MatCap both ignore it.
struct RenderVertex {
    float position[3];
    float normal[3];
    float color[3];
};

// How the surface is shaded. This is a PRESENTATION choice and changes only
// which normals are generated: the same triangles are drawn either way, no
// Construction parameter moves, no revision is minted and picking is untouched.
enum class SurfaceShading {
    Smooth,   // normals averaged across a crease-bounded group of faces
    Faceted,  // every triangle gets its own flat face normal
};

const char* surfaceShadingName(SurfaceShading shading);

// ---------------------------------------------------------------------------
// THE crease policy — one threshold, stated once
// ---------------------------------------------------------------------------
//
// Two triangles that share a vertex contribute to the SAME smoothed normal when
// the angle between their face normals is at most this; otherwise that vertex
// splits and the shared position gets one normal per group. That single rule
// produces every contract this stage owes, with no per-primitive special case
// and no magic number anywhere else in the codebase:
//
//   Box       adjacent faces meet at exactly 90 deg  -> always split -> six
//             planar faces with hard 90 deg edges and no rounded corners.
//   Cylinder  adjacent side quads differ by 360/32 = 11.25 deg -> smooth side;
//             cap triangles are coplanar (0 deg) -> flat cap; cap meets side at
//             90 deg -> hard rim.
//   Sphere    every adjacency is at most ~11.25 deg -> continuous smooth
//             shading, including the pole fans, whose 32 triangles all join one
//             group and average to the exact pole axis.
//   Cone      adjacent lateral faces differ by ~8 deg -> smooth side; the base
//             is coplanar -> flat; base meets side at >90 deg -> hard rim; the
//             apex fan joins one group and averages to the axis, which is a
//             finite, stable normal rather than a NaN or a black spike.
//   Capsule   hemisphere-to-middle differs by ~5.6 deg -> continuous smooth
//             shading across both seams; the equality case is generated as a
//             sphere and therefore shades as one.
//
// The value must sit comfortably above the coarsest curved adjacency
// (11.25 deg at kPrimitiveRadialSegments = 32) and comfortably below the
// sharpest edge a primitive can present (90 deg). 40 deg is near the middle of
// that band in angle, so neither bound is close, and a future tessellation
// change would have to roughly quadruple the segment angle before smooth
// surfaces began to facet.
constexpr float kCreaseAngleDegrees = 40.0f;

// The same threshold as a dot product, which is what the grouping actually
// compares. Two face normals belong together when dot(a, b) >= this.
float creaseCosineThreshold();

// Derived geometry, ready for the GPU. Owns its own storage; holds no reference
// to the source it was built from.
struct RenderMeshData {
    std::vector<RenderVertex> vertices;
    std::vector<uint32_t> indices;

    // What this was built FROM, retained so a diagnostic can report source and
    // render counts side by side and never confuse the two.
    uint32_t sourceVertexCount = 0;
    uint32_t sourceIndexCount = 0;
    SurfaceShading shading = SurfaceShading::Smooth;

    uint32_t vertexCount() const { return static_cast<uint32_t>(vertices.size()); }
    uint32_t indexCount() const { return static_cast<uint32_t>(indices.size()); }
    uint64_t vertexBytes() const { return vertices.size() * sizeof(RenderVertex); }
    uint64_t indexBytes() const { return indices.size() * sizeof(uint32_t); }

    void clear();
};

// True when every position and every normal is finite. A normal is additionally
// required to be either unit length or exactly zero — zero is the honest answer
// for a corner with no defined direction (only degenerate triangles touch it),
// and the shader has a documented fallback for it. Anything else means the
// generator produced a value it should not have.
bool renderMeshIsFinite(const RenderMeshData& data);

// Builds render geometry from source mesh data.
//
// Fails closed: returns false and leaves *out untouched when the source is not
// a usable triangle mesh (the same validateMeshData contract RuntimeMesh uses)
// or when the derived vertex count would exceed the hard mesh caps. Nothing
// partial is ever produced.
//
// Deterministic: the same source and the same shading always produce
// byte-identical output, which is what lets a self-test assert exact counts.
bool buildRenderMesh(const MeshVertex* vertices, uint32_t vertexCount,
                     const uint32_t* indices, uint32_t indexCount, SurfaceShading shading,
                     RenderMeshData* out);

// ---------------------------------------------------------------------------
// The rebuild policy
// ---------------------------------------------------------------------------
//
// Render geometry is rebuilt when — and only when — the thing it is derived
// from actually changed: a new source MeshRevision, or a different
// SurfaceShading. Everything else leaves it alone, which is what keeps this off
// the per-frame path.
//
// In particular this does NOT rebuild for: a camera move, a unit switch, the
// inspector opening or closing, a window resize or rotation, a mode or tool
// change, or switching between Studio Solid and MatCap — those last two are a
// fragment-stage uniform and touch no geometry at all.
//
// Not thread-safe by itself. The renderer owns one of these and touches it only
// on the render thread, which is the same thread that owns every GPU buffer.
class RenderMeshCache {
public:
    // Brings the cache up to date. Returns true when a rebuild actually
    // happened, so the caller knows it must re-upload; false means the cached
    // data was already correct for this (revision, shading) pair and nothing
    // was recomputed.
    //
    // A build failure leaves the previous cached data intact and reports it
    // through `outFailed`, so one bad revision cannot blank the viewport.
    bool refresh(const RuntimeMesh& source, SurfaceShading shading, bool* outFailed = nullptr);

    // Invalidates the cache without freeing it, forcing the next refresh() to
    // rebuild. Used when the consumer lost the data it had uploaded.
    void invalidate();

    bool valid() const { return valid_; }
    const RenderMeshData& data() const { return data_; }

    MeshRevision sourceRevision() const { return sourceRevision_; }
    ObjectId sourceObjectId() const { return sourceObjectId_; }
    SurfaceShading shading() const { return shading_; }

    // --- introspection (logging and self-tests only) ---
    //
    // `rebuildCount` is the number this stage's per-frame-rebuild proof rests
    // on: it must stay flat while the camera moves, while the display mode
    // changes and while frames are presented, and step by exactly one per
    // accepted geometry change.
    uint64_t rebuildCount() const { return rebuildCount_; }
    uint64_t skippedRefreshCount() const { return skippedRefreshCount_; }
    uint64_t failedRebuildCount() const { return failedRebuildCount_; }
    // Wall time of the most recent rebuild, for the stage's cost record.
    double lastRebuildMillis() const { return lastRebuildMillis_; }

private:
    RenderMeshData data_;
    bool valid_ = false;
    MeshRevision sourceRevision_ = kNoMeshRevision;
    ObjectId sourceObjectId_ = kNoObject;
    SurfaceShading shading_ = SurfaceShading::Smooth;
    uint64_t rebuildCount_ = 0;
    uint64_t skippedRefreshCount_ = 0;
    uint64_t failedRebuildCount_ = 0;
    double lastRebuildMillis_ = 0.0;
};

}  // namespace forgeshape
