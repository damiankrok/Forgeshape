// ForgeShape CPU picking.
//
// Turns a screen point plus the current camera truth into a world-space ray,
// and intersects that ray with indexed triangle data. Deterministic, CPU only:
// there is no GPU id buffer and no readback.
//
// This module deliberately contains NO JNI, NO Android, NO Vulkan and NO
// renderer types. It reads a CameraSnapshot (the camera module's public output)
// rather than keeping any camera state of its own, and it reads triangle data
// through a non-owning view rather than keeping a second copy of any mesh.
//
// Object identity is NOT decided here; see forgeshape_selection.h.
#pragma once

#include <cstddef>
#include <cstdint>

#include "forgeshape_camera.h"
#include "forgeshape_math.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// Canonical ForgeShape facing convention
// ---------------------------------------------------------------------------
// A triangle's vertices are ordered COUNTER-CLOCKWISE when the triangle is
// viewed from OUTSIDE the surface, in right-handed world space. Equivalently
// the geometric normal is
//
//     N = (v1 - v0) x (v2 - v0)
//
// and it points away from the solid. A triangle is front-facing to a ray when
// dot(N, rayDirection) < 0.
//
// The renderer's Y-flipped Vulkan projection mirrors screen-space winding, so
// the same convention appears CLOCKWISE in framebuffer coordinates; the
// pipeline therefore uses VK_FRONT_FACE_CLOCKWISE with VK_CULL_MODE_BACK_BIT.
// CPU picking and the rasterizer consequently agree on what is visible.

struct Ray {
    Vec3 origin;     // world space
    Vec3 direction;  // world space, unit length
};

// Non-owning view of indexed triangle data. `positions` points at the first
// float of the first vertex position; `positionStride` is the byte distance
// between consecutive vertices, so interleaved vertex formats can be picked
// without unpacking or copying them.
struct TriangleMeshView {
    const float* positions = nullptr;
    size_t positionStride = 0;
    uint32_t vertexCount = 0;
    // 32-bit since Stage 006, matching VK_INDEX_TYPE_UINT32 on the render side,
    // so the CPU and GPU views of a mesh cannot disagree about index width and
    // meshes are not structurally capped at 65,535 vertices.
    const uint32_t* indices = nullptr;
    uint32_t indexCount = 0;
};

struct TriangleHit {
    bool hit = false;
    float t = 0.0f;             // distance along the (unit) ray direction
    int triangleIndex = -1;     // index of the triangle within the mesh view
    Vec3 position{0.0f, 0.0f, 0.0f};
};

// Rays closer than this are treated as starting on the surface and rejected,
// which keeps a ray whose origin lies exactly on a face from self-hitting.
constexpr float kMinRayDistance = 1e-4f;

// |determinant| below this means the ray is parallel to the triangle plane, or
// the triangle is degenerate. Both are rejected.
constexpr float kRayTriangleEpsilon = 1e-8f;

// Builds the world-space ray through a view-local pixel.
//
// `screenX` / `screenY` are in the same coordinate system Android hands us:
// pixels, origin top-left, Y increasing downward. `viewportWidth/Height` must
// be the size of that same view. The camera's own projection matrix is used, so
// FOV, aspect and the Vulkan depth/Y convention are never restated here.
//
// Returns false (leaving `out` untouched) for a non-positive viewport, a
// degenerate projection, or any non-finite result.
bool buildPickRay(const CameraSnapshot& camera, float screenX, float screenY,
                  int viewportWidth, int viewportHeight, Ray* out);

// Carries a world-space ray into an object's local space with the object's
// INVERSE model transform, so the ray can be intersected against the object's
// unchanged local geometry instead of transforming every vertex.
//
// The transform must be RIGID (rotation + translation, no scale), which is the
// only kind ForgeShape has. Because a rigid transform preserves length, the
// direction stays unit length and the resulting hit distance `t` is still in
// world meters — so distances from transformed and untransformed objects remain
// directly comparable.
//
// Returns false (leaving `out` untouched) for a non-finite matrix or result.
bool transformRayToLocal(const Ray& worldRay, const Mat4& inverseModel, Ray* out);

// Moller-Trumbore. Returns true on a hit strictly in front of the ray origin.
// When `frontFacesOnly` is true, only triangles whose canonical outward normal
// faces the ray are accepted, matching what the rasterizer draws.
//
// Safe for parallel rays, degenerate triangles, triangles behind the origin and
// non-finite input: all of those return false rather than producing a NaN.
bool intersectRayTriangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                          bool frontFacesOnly, float* outT);

// Nearest positive hit across every triangle in the view. Malformed views
// (null pointers, index count not a multiple of three, out-of-range indices)
// yield a miss instead of reading out of bounds.
TriangleHit pickTriangleMesh(const Ray& ray, const TriangleMeshView& mesh, bool frontFacesOnly);

// True when every triangle in the view obeys the canonical convention for a
// solid centred at `center`: the outward normal points away from the centre.
// Used by the self-tests to prove the mesh data and the culling mode agree.
bool meshObeysCanonicalWinding(const TriangleMeshView& mesh, const Vec3& center);

}  // namespace forgeshape
