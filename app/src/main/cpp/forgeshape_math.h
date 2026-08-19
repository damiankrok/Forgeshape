// ForgeShape minimal math.
//
// Deliberately tiny and self-owned: only what one perspective cube needs.
// No third-party math library is used.
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

}  // namespace forgeshape
