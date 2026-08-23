#include "forgeshape_picking.h"

#include <cmath>

namespace forgeshape {
namespace {

// Reads vertex `index` out of an interleaved position array.
Vec3 vertexAt(const TriangleMeshView& mesh, uint32_t index) {
    const auto* base = reinterpret_cast<const unsigned char*>(mesh.positions);
    const auto* p = reinterpret_cast<const float*>(base + mesh.positionStride * index);
    return Vec3{p[0], p[1], p[2]};
}

bool meshUsable(const TriangleMeshView& mesh) {
    return mesh.positions != nullptr && mesh.indices != nullptr &&
           mesh.positionStride >= sizeof(float) * 3 && mesh.vertexCount > 0 &&
           mesh.indexCount >= 3 && (mesh.indexCount % 3) == 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Screen point -> world ray
// ---------------------------------------------------------------------------
//
// The camera snapshot already carries the truth we need, so nothing is
// re-derived from FOV or aspect constants:
//
//   proj (column-major) maps camera space to clip space as
//       x_clip = m[0]  * x_cam
//       y_clip = m[5]  * y_cam
//       w_clip = -z_cam
//
//   view is a rigid transform whose rotation rows are the camera basis:
//       r0 = (m[0], m[4], m[8])   camera right
//       r1 = (m[1], m[5], m[9])   camera up
//       r2 = (m[2], m[6], m[10])  camera backward (= -forward)
//
// so a camera-space direction is rotated back into world space by
// d_world = d.x*r0 + d.y*r1 + d.z*r2.
//
// Android screen Y grows downward and Vulkan NDC Y also grows downward (the
// projection's negative m[5] is what performs that flip), so screen -> NDC
// needs no extra Y inversion here.
bool buildPickRay(const CameraSnapshot& camera, float screenX, float screenY,
                  int viewportWidth, int viewportHeight, Ray* out) {
    if (out == nullptr || viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
    }
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return false;
    }
    if (!mat4Finite(camera.proj) || !mat4Finite(camera.view) || !vec3Finite(camera.eye)) {
        return false;
    }

    const float sx = camera.proj.m[0];
    const float sy = camera.proj.m[5];
    if (std::fabs(sx) < 1e-8f || std::fabs(sy) < 1e-8f) {
        return false;
    }

    const float ndcX = (2.0f * screenX / static_cast<float>(viewportWidth)) - 1.0f;
    const float ndcY = (2.0f * screenY / static_cast<float>(viewportHeight)) - 1.0f;

    // The camera-space coordinates of this pixel, inverted out of whichever
    // projection is active. Both matrices scale x and y by m[0] and m[5], so
    // this single division serves both; what the two do with the result is where
    // they part company.
    const float camX = ndcX / sx;
    const float camY = ndcY / sy;

    const Vec3 r0{camera.view.m[0], camera.view.m[4], camera.view.m[8]};
    const Vec3 r1{camera.view.m[1], camera.view.m[5], camera.view.m[9]};
    const Vec3 r2{camera.view.m[2], camera.view.m[6], camera.view.m[10]};

    // Camera-space origin offset and direction, per projection.
    //
    //   Perspective — every ray leaves the SAME point (the pinhole) in a
    //   DIFFERENT direction. The pixel selects the direction: the point
    //   (camX, camY, -1) on the plane one meter in front of the eye.
    //
    //   Orthographic — every ray leaves a DIFFERENT point in the SAME direction.
    //   The pixel selects the origin, sliding it across the view plane by
    //   (camX, camY, 0); the direction is straight down the view axis for the
    //   whole screen, which is exactly what a parallel projection means.
    //
    // Using the pixel to move the origin is not an optional refinement. A pick
    // ray that still fanned out from a point while the image was drawn in
    // parallel would agree with the picture only at the screen centre and drift
    // further from it toward every edge.
    const bool orthographic = (camera.projection == ProjectionMode::Orthographic);

    const Vec3 dirCamera = orthographic ? Vec3{0.0f, 0.0f, -1.0f} : Vec3{camX, camY, -1.0f};

    Vec3 dirWorld = vec3Scale(r0, dirCamera.x);
    dirWorld = vec3Add(dirWorld, vec3Scale(r1, dirCamera.y));
    dirWorld = vec3Add(dirWorld, vec3Scale(r2, dirCamera.z));

    if (!vec3Finite(dirWorld)) {
        return false;
    }
    const float lengthSquared = vec3Dot(dirWorld, dirWorld);
    if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) {
        return false;
    }

    const Vec3 normalized = vec3Normalize(dirWorld);
    if (!vec3Finite(normalized)) {
        return false;
    }

    Vec3 origin = camera.eye;
    if (orthographic) {
        // camera.eye is the centre of the view plane (see CameraSnapshot), and
        // the plane is pulled far enough back that every drawn surface is in
        // front of it — so a positive hit distance still means "visible", and
        // the nearest-hit rule still picks the surface the rasterizer drew.
        origin = vec3Add(origin, vec3Scale(r0, camX));
        origin = vec3Add(origin, vec3Scale(r1, camY));
        if (!vec3Finite(origin)) {
            return false;
        }
    }

    out->origin = origin;
    out->direction = normalized;
    return true;
}

// ---------------------------------------------------------------------------
// World ray -> local object ray
// ---------------------------------------------------------------------------
//
// Moving the ray is the cheap direction: one point and one direction, instead of
// every vertex of the mesh. It is also what keeps the object's RuntimeMesh
// untouched by a transform, so a move or a rotate costs no mesh revision.
//
// The origin is a POSITION (translation applies) and the direction is a
// DIRECTION (translation does not). Because the inverse is rigid, the direction
// is not renormalized: it already has unit length, and rescaling it would
// silently change the meaning of the returned distance.
bool transformRayToLocal(const Ray& worldRay, const Mat4& inverseModel, Ray* out) {
    if (out == nullptr) {
        return false;
    }
    if (!mat4Finite(inverseModel) || !vec3Finite(worldRay.origin) ||
        !vec3Finite(worldRay.direction)) {
        return false;
    }

    const Vec3 origin = mat4TransformPoint(inverseModel, worldRay.origin);
    const Vec3 direction = mat4TransformDirection(inverseModel, worldRay.direction);
    if (!vec3Finite(origin) || !vec3Finite(direction)) {
        return false;
    }
    const float lengthSquared = vec3Dot(direction, direction);
    if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) {
        return false;
    }

    out->origin = origin;
    out->direction = direction;
    return true;
}

// ---------------------------------------------------------------------------
// Ray / triangle
// ---------------------------------------------------------------------------
//
// Moller-Trumbore. With the canonical convention (N = e1 x e2 points outward)
// the determinant satisfies det = -dot(direction, N), so det > 0 is exactly
// "front-facing to this ray" and the culling branch needs no separate normal.
bool intersectRayTriangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                          bool frontFacesOnly, float* outT) {
    if (outT == nullptr) {
        return false;
    }
    if (!vec3Finite(ray.origin) || !vec3Finite(ray.direction) ||
        !vec3Finite(v0) || !vec3Finite(v1) || !vec3Finite(v2)) {
        return false;
    }

    const Vec3 e1 = vec3Sub(v1, v0);
    const Vec3 e2 = vec3Sub(v2, v0);
    const Vec3 p = vec3Cross(ray.direction, e2);
    const float det = vec3Dot(e1, p);

    if (frontFacesOnly) {
        if (det < kRayTriangleEpsilon) {
            return false;  // back-facing, parallel, or degenerate
        }
    } else if (std::fabs(det) < kRayTriangleEpsilon) {
        return false;  // parallel or degenerate
    }

    const float invDet = 1.0f / det;
    const Vec3 s = vec3Sub(ray.origin, v0);

    // The containment bounds are widened by kBarycentricEpsilon so a hit lying
    // exactly on an edge shared with a neighbouring triangle cannot be rejected
    // by both of them; see that constant for why an exact test is ABI-dependent.
    const float u = vec3Dot(s, p) * invDet;
    if (!std::isfinite(u) || u < -kBarycentricEpsilon || u > 1.0f + kBarycentricEpsilon) {
        return false;
    }

    const Vec3 q = vec3Cross(s, e1);
    const float v = vec3Dot(ray.direction, q) * invDet;
    if (!std::isfinite(v) || v < -kBarycentricEpsilon ||
        (u + v) > 1.0f + kBarycentricEpsilon) {
        return false;
    }

    const float t = vec3Dot(e2, q) * invDet;
    if (!std::isfinite(t) || t <= kMinRayDistance) {
        return false;  // behind the origin, or the origin sits on the surface
    }

    *outT = t;
    return true;
}

TriangleHit pickTriangleMesh(const Ray& ray, const TriangleMeshView& mesh, bool frontFacesOnly) {
    TriangleHit best{};
    if (!meshUsable(mesh)) {
        return best;
    }

    const uint32_t triangleCount = mesh.indexCount / 3;
    for (uint32_t tri = 0; tri < triangleCount; ++tri) {
        const uint32_t i0 = mesh.indices[tri * 3 + 0];
        const uint32_t i1 = mesh.indices[tri * 3 + 1];
        const uint32_t i2 = mesh.indices[tri * 3 + 2];
        if (i0 >= mesh.vertexCount || i1 >= mesh.vertexCount || i2 >= mesh.vertexCount) {
            continue;
        }

        float t = 0.0f;
        if (!intersectRayTriangle(ray, vertexAt(mesh, i0), vertexAt(mesh, i1), vertexAt(mesh, i2),
                                  frontFacesOnly, &t)) {
            continue;
        }
        if (best.hit && t >= best.t) {
            continue;  // keep the nearest positive hit
        }
        best.hit = true;
        best.t = t;
        best.triangleIndex = static_cast<int>(tri);
    }

    if (best.hit) {
        best.position = vec3Add(ray.origin, vec3Scale(ray.direction, best.t));
    }
    return best;
}

bool meshObeysCanonicalWinding(const TriangleMeshView& mesh, const Vec3& center) {
    if (!meshUsable(mesh)) {
        return false;
    }
    const uint32_t triangleCount = mesh.indexCount / 3;
    for (uint32_t tri = 0; tri < triangleCount; ++tri) {
        const uint32_t i0 = mesh.indices[tri * 3 + 0];
        const uint32_t i1 = mesh.indices[tri * 3 + 1];
        const uint32_t i2 = mesh.indices[tri * 3 + 2];
        if (i0 >= mesh.vertexCount || i1 >= mesh.vertexCount || i2 >= mesh.vertexCount) {
            return false;
        }
        const Vec3 v0 = vertexAt(mesh, i0);
        const Vec3 v1 = vertexAt(mesh, i1);
        const Vec3 v2 = vertexAt(mesh, i2);
        const Vec3 normal = vec3Cross(vec3Sub(v1, v0), vec3Sub(v2, v0));
        const Vec3 centroid =
            vec3Scale(vec3Add(vec3Add(v0, v1), v2), 1.0f / 3.0f);
        // Outward means the normal agrees with "centre -> face".
        if (vec3Dot(normal, vec3Sub(centroid, center)) <= 0.0f) {
            return false;
        }
    }
    return true;
}

}  // namespace forgeshape
