#include "forgeshape_matcap.h"

#include <cmath>

#include "forgeshape_math.h"

namespace forgeshape {
namespace {

// ---------------------------------------------------------------------------
// THE ForgeShape MatCap formula
// ---------------------------------------------------------------------------
//
// Everything below is the asset. It is a small analytic studio response
// evaluated over the visible hemisphere, chosen for READABILITY of form rather
// than for physical accuracy: this is a modelling aid, not a renderer.
//
// Space: VIEW space, right-handed, +X right, +Y up, +Z toward the viewer. A
// surface facing the camera therefore has the normal (0, 0, 1), which lands at
// the centre of the texture. Because the whole model is expressed in view
// space, the lighting travels with the camera — the defining property of a
// MatCap, and the same choice Studio Solid makes, so the two modes agree about
// where the light is coming from and switching between them does not relight
// the object.
//
// Three directional terms plus a hemispherical ambient, all normalized at use:
//
//   key   upper-LEFT and slightly toward the viewer. Upper-left is the
//         convention every sculpting and CAD viewport uses, and matching it
//         means a form reads the way the user already expects.
//   fill  lower-RIGHT, cool and much weaker. Its job is to keep the shadow
//         side legible instead of black, so curvature stays readable where the
//         key does not reach.
//   rim   a view-dependent silhouette lift, kept small. It separates the object
//         from the dark viewport background without becoming a glow.
//
// The specular term is deliberately restrained (a tight, dim Blinn-Phong lobe):
// enough to tell a planar face from a curved one at a glance, not enough to
// read as a material. There is no roughness, no metalness, no environment and
// no tone-mapping stack — those belong to a later PBR Preview, not here.

// The MatCap shares Studio Solid's GEOMETRY of light — same key direction, same
// fill direction, same hemispherical ambient — so switching modes does not move
// the light and a form does not appear to turn over. What differs is the TUNING:
// this preset is deliberately higher-contrast and glossier, a polished clay
// rather than Studio Solid's matte workbench surface. That is the division of
// labour between the two modes: Studio for judging planar faces and exact
// silhouettes, MatCap for reading subtle curvature and sculpt deformation, where
// a broader sheen and a stronger rim expose form a matte surface hides.
//
// The Studio Solid counterparts live in shaders/surface.frag. If one set is
// retuned, look at the other: they are meant to stay siblings.
constexpr float kAlbedo[3] = {0.660f, 0.645f, 0.625f};  // neutral, faintly warm clay

const Vec3 kKeyDirection{-0.40f, 0.60f, 0.70f};
constexpr float kKeyIntensity = 1.00f;
constexpr float kKeyColor[3] = {1.00f, 0.98f, 0.94f};

// Lower-FRONT and weak, for the same reason Studio Solid's fill is: a fill
// opposite the key lifts precisely the planes the key leaves dark, and the form
// flattens. See the longer note in shaders/surface.frag.
const Vec3 kFillDirection{0.35f, -0.55f, 0.55f};
constexpr float kFillIntensity = 0.16f;
constexpr float kFillColor[3] = {0.82f, 0.87f, 1.00f};

// Hemispherical ambient: a cool "sky" above and a darker, warmer "ground"
// below, blended by the normal's height. This is what gives an unlit region a
// direction instead of a flat fog, and it costs one lerp. Pulled down from
// Studio Solid's, which is where most of the extra contrast comes from.
constexpr float kSkyAmbient[3] = {0.420f, 0.450f, 0.500f};
constexpr float kGroundAmbient[3] = {0.200f, 0.190f, 0.190f};

// Broader and brighter than Studio Solid's tight, dim lobe: on a sculpted
// surface a wide sheen is what makes a shallow bump visible at all.
//
// Capped below the level where the lobe's peak would clip. At 0.50 the
// highlight saturated to pure white across several degrees, and a clipped
// region carries no gradient — which on a form-reading view is exactly the
// information the sheen was added to provide.
constexpr float kSpecularIntensity = 0.38f;
constexpr float kSpecularExponent = 24.0f;

constexpr float kRimIntensity = 0.15f;
constexpr float kRimExponent = 3.0f;
constexpr float kRimColor[3] = {0.78f, 0.84f, 0.95f};

inline float clamp01(float v) {
    if (!(v > 0.0f)) return 0.0f;  // also catches NaN
    return v > 1.0f ? 1.0f : v;
}

inline uint8_t encode(float linear) {
    // The swapchain is R8G8B8A8_UNORM and the renderer writes shaded values to
    // it directly, so every colour in ForgeShape — the clear colour, the debug
    // vertex colours, Studio Solid and this MatCap — is authored in the SAME
    // display space with no transfer function applied. Introducing one here
    // alone would make MatCap and Studio Solid disagree about brightness. A
    // real linear workflow is a PBR-stage decision, taken for all of them at
    // once.
    return static_cast<uint8_t>(clamp01(linear) * 255.0f + 0.5f);
}

// Evaluates the formula for one view-space unit normal.
void shadeNormal(const Vec3& n, float outRgb[3]) {
    const Vec3 key = vec3Normalize(kKeyDirection);
    const Vec3 fill = vec3Normalize(kFillDirection);
    const Vec3 view{0.0f, 0.0f, 1.0f};  // orthographic view direction, by MatCap definition

    const float keyLambert = std::fmax(vec3Dot(n, key), 0.0f) * kKeyIntensity;
    const float fillLambert = std::fmax(vec3Dot(n, fill), 0.0f) * kFillIntensity;

    // Hemisphere ambient, indexed by height rather than by any light.
    const float sky = n.y * 0.5f + 0.5f;

    // Blinn-Phong against the key only. A second specular lobe would add cost
    // and a competing highlight without adding information about the form.
    const Vec3 half = vec3Normalize(vec3Add(key, view));
    const float specular =
        std::pow(std::fmax(vec3Dot(n, half), 0.0f), kSpecularExponent) * kSpecularIntensity;

    // Silhouette lift. n.z falls to 0 exactly at the rim of the disc.
    const float rim = std::pow(1.0f - clamp01(n.z), kRimExponent) * kRimIntensity;

    for (int c = 0; c < 3; ++c) {
        const float ambient = kGroundAmbient[c] + (kSkyAmbient[c] - kGroundAmbient[c]) * sky;
        const float diffuse = keyLambert * kKeyColor[c] + fillLambert * kFillColor[c];
        outRgb[c] = kAlbedo[c] * (ambient + diffuse) + specular + rim * kRimColor[c];
    }
}

}  // namespace

void generateMatCap(std::vector<uint8_t>* out) {
    if (out == nullptr) {
        return;
    }
    out->assign(kMatCapByteSize, 0);

    for (uint32_t y = 0; y < kMatCapSize; ++y) {
        for (uint32_t x = 0; x < kMatCapSize; ++x) {
            // Texel centres, mapped onto [-1, 1] so the disc is inscribed in
            // the square exactly the way the shader's uv = n.xy * 0.5 + 0.5
            // lookup expects to find it.
            const float nx = (static_cast<float>(x) + 0.5f) / kMatCapSize * 2.0f - 1.0f;
            const float ny = 1.0f - (static_cast<float>(y) + 0.5f) / kMatCapSize * 2.0f;

            const float radiusSq = nx * nx + ny * ny;

            Vec3 n;
            if (radiusSq >= 1.0f) {
                // Outside the disc there is no normal a visible surface could
                // have, but bilinear filtering WILL read these texels when a
                // fragment sits on the silhouette. Clamping them to the rim
                // value in the same direction keeps that edge clean instead of
                // bleeding whatever the initializer left behind.
                const float radius = std::sqrt(radiusSq);
                n = Vec3{nx / radius, ny / radius, 0.0f};
            } else {
                n = Vec3{nx, ny, std::sqrt(1.0f - radiusSq)};
            }

            float rgb[3];
            shadeNormal(n, rgb);

            const uint32_t offset = (y * kMatCapSize + x) * kMatCapBytesPerTexel;
            (*out)[offset + 0] = encode(rgb[0]);
            (*out)[offset + 1] = encode(rgb[1]);
            (*out)[offset + 2] = encode(rgb[2]);
            (*out)[offset + 3] = 255;
        }
    }
}

bool sampleMatCap(const std::vector<uint8_t>& texels, float nx, float ny, uint8_t outRgb[3]) {
    if (texels.size() < kMatCapByteSize || outRgb == nullptr) {
        return false;
    }
    if (!std::isfinite(nx) || !std::isfinite(ny) || nx * nx + ny * ny > 1.0f) {
        return false;
    }

    // The shader's lookup, on the CPU: uv = n.xy * 0.5 + 0.5, with y running
    // down the image.
    const float u = nx * 0.5f + 0.5f;
    const float v = 1.0f - (ny * 0.5f + 0.5f);

    int32_t x = static_cast<int32_t>(u * kMatCapSize);
    int32_t y = static_cast<int32_t>(v * kMatCapSize);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= static_cast<int32_t>(kMatCapSize)) x = kMatCapSize - 1;
    if (y >= static_cast<int32_t>(kMatCapSize)) y = kMatCapSize - 1;

    const uint32_t offset = (static_cast<uint32_t>(y) * kMatCapSize + static_cast<uint32_t>(x)) *
                            kMatCapBytesPerTexel;
    outRgb[0] = texels[offset + 0];
    outRgb[1] = texels[offset + 1];
    outRgb[2] = texels[offset + 2];
    return true;
}

}  // namespace forgeshape
