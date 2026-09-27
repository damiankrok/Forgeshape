// ForgeShape minimal math.
//
// Deliberately tiny and self-owned: only what the viewport, picking and the
// geometry domain need. No third-party math library is used (repository rule).
#pragma once

#include <cmath>

namespace forgeshape {

struct Vec3 {
    float x, y, z;
};

// Column-major 4x4 matrix, laid out exactly as GLSL expects it.
struct Mat4 {
    float m[16];
};

inline Mat4 mat4Identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

inline Mat4 mat4Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

inline Vec3 vec3Sub(const Vec3& a, const Vec3& b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3Add(const Vec3& a, const Vec3& b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec3Scale(const Vec3& v, float s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

inline bool vec3Finite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline bool mat4Finite(const Mat4& m) {
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(m.m[i])) {
            return false;
        }
    }
    return true;
}

inline Vec3 vec3Cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float vec3Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 vec3Normalize(const Vec3& v) {
    float len = std::sqrt(vec3Dot(v, v));
    if (len <= 0.0f) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return Vec3{v.x / len, v.y / len, v.z / len};
}

// ---------------------------------------------------------------------------
// Rigid transform primitives
// ---------------------------------------------------------------------------
//
// Right-handed world space, +Y up, COLUMN-VECTOR convention: a point is
// transformed as p' = M * p, so composing A then B is written B * A. Storage is
// column-major (m[column * 4 + row]), matching GLSL.
//
// A positive angle rotates counter-clockwise around its axis when that axis
// points toward the viewer: the right-hand rule.

inline Mat4 mat4RotationX(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Mat4 r = mat4Identity();
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

inline Mat4 mat4RotationY(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Mat4 r = mat4Identity();
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

inline Mat4 mat4RotationZ(float radians) {
    float c = std::cos(radians);
    float s = std::sin(radians);
    Mat4 r = mat4Identity();
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

inline Mat4 mat4Translation(const Vec3& t) {
    Mat4 r = mat4Identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

// A column-major affine matrix from three basis directions and a translation:
// the columns are `x`, `y`, `z` and the last column is `origin`. When the three
// are an orthonormal right-handed frame this maps a point `(u, v, n)` in that
// frame to `origin + u*x + v*y + n*z` in the parent space — which is exactly how
// a CAD face support places a child body's local space onto a producer's face
// (`CAD-A3`). No orthonormality is assumed here; the caller guarantees it.
inline Mat4 mat4FromBasis(const Vec3& x, const Vec3& y, const Vec3& z, const Vec3& origin) {
    Mat4 r = mat4Identity();
    r.m[0] = x.x;  r.m[4] = y.x;  r.m[8]  = z.x;  r.m[12] = origin.x;
    r.m[1] = x.y;  r.m[5] = y.y;  r.m[9]  = z.y;  r.m[13] = origin.y;
    r.m[2] = x.z;  r.m[6] = y.z;  r.m[10] = z.z;  r.m[14] = origin.z;
    return r;
}

// The inverse of an AFFINE matrix — one whose bottom row is (0,0,0,1), which is
// every model matrix ForgeShape produces. It inverts the upper-left 3x3 by
// cofactors and carries the translation through, so it works for a rotation, a
// non-uniform scale and their composition (a `producerModel * faceFrame` a CAD
// dependent needs), without the cost or the failure modes of a general 4x4
// inverse. Returns false, leaving `out` untouched, when the 3x3 is singular.
inline bool mat4AffineInverse(const Mat4& m, Mat4* out) {
    const float a = m.m[0], b = m.m[4], c = m.m[8];
    const float d = m.m[1], e = m.m[5], f = m.m[9];
    const float g = m.m[2], h = m.m[6], i = m.m[10];
    const float A = e * i - f * h;
    const float B = -(d * i - f * g);
    const float C = d * h - e * g;
    const float det = a * A + b * B + c * C;
    if (!(det != 0.0f) || !std::isfinite(det)) {
        return false;
    }
    const float invDet = 1.0f / det;
    // inv3 = adjugate / det, laid out column-major.
    const float i00 = A * invDet;
    const float i01 = -(b * i - c * h) * invDet;
    const float i02 = (b * f - c * e) * invDet;
    const float i10 = B * invDet;
    const float i11 = (a * i - c * g) * invDet;
    const float i12 = -(a * f - c * d) * invDet;
    const float i20 = C * invDet;
    const float i21 = -(a * h - b * g) * invDet;
    const float i22 = (a * e - b * d) * invDet;
    const float tx = m.m[12], ty = m.m[13], tz = m.m[14];
    Mat4 r = mat4Identity();
    r.m[0] = i00; r.m[4] = i01; r.m[8]  = i02;
    r.m[1] = i10; r.m[5] = i11; r.m[9]  = i12;
    r.m[2] = i20; r.m[6] = i21; r.m[10] = i22;
    // The inverse translation is -inv3 * t.
    r.m[12] = -(i00 * tx + i01 * ty + i02 * tz);
    r.m[13] = -(i10 * tx + i11 * ty + i12 * tz);
    r.m[14] = -(i20 * tx + i21 * ty + i22 * tz);
    if (!mat4Finite(r)) {
        return false;
    }
    *out = r;
    return true;
}

// The matrix a NORMAL is carried by under an affine model: the inverse transpose
// of the upper-left 3x3, embedded in a 4x4 with no translation. Equal to the
// rotation for a rigid model, and the correct shear-free normal transform for a
// non-uniform scale. Falls back to the model itself when the model is singular,
// which cannot happen for a valid placement.
inline Mat4 mat4NormalMatrix(const Mat4& model) {
    Mat4 inv;
    if (!mat4AffineInverse(model, &inv)) {
        return model;
    }
    Mat4 r = mat4Identity();
    // Transpose of the upper-left 3x3 of `inv`, translation cleared.
    r.m[0] = inv.m[0]; r.m[4] = inv.m[1]; r.m[8]  = inv.m[2];
    r.m[1] = inv.m[4]; r.m[5] = inv.m[5]; r.m[9]  = inv.m[6];
    r.m[2] = inv.m[8]; r.m[6] = inv.m[9]; r.m[10] = inv.m[10];
    return r;
}

// Transforms a POSITION (implicit w = 1), so translation applies.
inline Vec3 mat4TransformPoint(const Mat4& m, const Vec3& p) {
    return Vec3{
        m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
        m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
        m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14],
    };
}

// Transforms a DIRECTION (implicit w = 0), so translation is ignored. For a
// rigid transform (rotation only, no scale) this preserves length, which is what
// lets a ray parameter t stay in world units after the ray is moved into local
// object space.
inline Vec3 mat4TransformDirection(const Mat4& m, const Vec3& d) {
    return Vec3{
        m.m[0] * d.x + m.m[4] * d.y + m.m[8] * d.z,
        m.m[1] * d.x + m.m[5] * d.y + m.m[9] * d.z,
        m.m[2] * d.x + m.m[6] * d.y + m.m[10] * d.z,
    };
}

inline Mat4 mat4LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = vec3Normalize(vec3Sub(center, eye));
    Vec3 s = vec3Normalize(vec3Cross(f, up));
    Vec3 u = vec3Cross(s, f);

    Mat4 r = mat4Identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -vec3Dot(s, eye);
    r.m[13] = -vec3Dot(u, eye);
    r.m[14] = vec3Dot(f, eye);
    return r;
}

// Vulkan-style perspective: depth range [0, 1], Y already flipped for Vulkan's
// clip space so no negative-viewport trick is needed.
inline Mat4 mat4Perspective(float fovYRadians, float aspect, float zNear, float zFar) {
    float t = 1.0f / std::tan(fovYRadians * 0.5f);
    Mat4 r{};
    r.m[0] = t / aspect;
    r.m[5] = -t;
    r.m[10] = zFar / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (zNear * zFar) / (zNear - zFar);
    return r;
}

// Vulkan-style ORTHOGRAPHIC (parallel) projection, sharing every convention with
// mat4Perspective above: right-handed view space looking down -Z, depth range
// [0, 1], and the Y flip performed in the matrix rather than by a negative
// viewport.
//
// `halfHeightMeters` is half the world-space height the viewport shows, so the
// visible slab is 2 * halfHeightMeters tall and 2 * halfHeightMeters * aspect
// wide. It is a real world length, not an abstract zoom factor.
//
// The defining difference from the perspective matrix is m[11]: it stays 0, so
// w_clip is 1 for every vertex and nothing is divided by depth. That is what
// makes this a true parallel projection rather than a perspective one with a
// narrow field of view — equal lengths parallel to the image plane project to
// equal screen lengths no matter how far away they are.
//
// Mapping, for a camera-space point (x, y, z) with z negative in front:
//     x_ndc = x / (halfHeightMeters * aspect)
//     y_ndc = -y / halfHeightMeters
//     z_ndc = (-z - zNear) / (zFar - zNear)
// so z = -zNear lands on 0 and z = -zFar lands on 1, exactly as the perspective
// matrix does.
//
// zNear may legitimately be negative here. A parallel projection has no eye
// singularity and never divides by w, so a near plane behind the view origin is
// well defined; the caller decides whether it wants one.
inline Mat4 mat4Orthographic(float halfHeightMeters, float aspect, float zNear, float zFar) {
    Mat4 r{};
    r.m[0] = 1.0f / (halfHeightMeters * aspect);
    r.m[5] = -1.0f / halfHeightMeters;
    r.m[10] = 1.0f / (zNear - zFar);
    r.m[14] = zNear / (zNear - zFar);
    r.m[15] = 1.0f;
    return r;
}

// ---------------------------------------------------------------------------
// Binary64 geometry (`CAD-VERTICAL-SLICE-R1`)
// ---------------------------------------------------------------------------
//
// Authored CAD truth is binary64 metres, and a boolean between two solids is
// only as exact as the coordinates it is handed: a tool sketched on a body's
// cap must lie EXACTLY in that cap's plane, which a float frame cannot promise.
// The CAD feature chain therefore places and builds its solids in double and
// rounds to float once, at the render mesh. Deliberately minimal: a vector and
// the handful of operations the placement needs.
struct DVec3 {
    double x, y, z;
};

inline DVec3 dvec3(double x, double y, double z) { return DVec3{x, y, z}; }
inline DVec3 dvec3FromVec3(const Vec3& v) { return DVec3{v.x, v.y, v.z}; }
inline Vec3 vec3FromDVec3(const DVec3& v) {
    return Vec3{static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}
inline DVec3 dvec3Add(const DVec3& a, const DVec3& b) { return DVec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
inline DVec3 dvec3Sub(const DVec3& a, const DVec3& b) { return DVec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
inline DVec3 dvec3Scale(const DVec3& v, double s) { return DVec3{v.x * s, v.y * s, v.z * s}; }
inline double dvec3Dot(const DVec3& a, const DVec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline DVec3 dvec3Cross(const DVec3& a, const DVec3& b) {
    return DVec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline bool dvec3Finite(const DVec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
// Unit length, or false (writing nothing) for a vector too short to have a
// direction.
inline bool dvec3Normalized(const DVec3& v, DVec3* out) {
    const double len = std::sqrt(dvec3Dot(v, v));
    if (!(len > 1.0e-12) || !std::isfinite(len)) {
        return false;
    }
    *out = dvec3Scale(v, 1.0 / len);
    return true;
}

}  // namespace forgeshape
