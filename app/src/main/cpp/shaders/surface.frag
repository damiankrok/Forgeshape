#version 450

// ForgeShape surface fragment stage: Studio Solid, MatCap, and a debug-only
// source-colour path.
//
// This is deliberately NOT a PBR shader. There is no metalness, no roughness,
// no environment probe, no shadow map, no ambient occlusion and no tone-mapping
// stack. Its whole job is to make form READABLE while modelling: planar faces
// distinguishable from curved ones, hard edges visible as hard, and sculpt
// deformation legible as it happens.

// Mirrors the vertex stage's block from offset 64 onward. The mat4 at offset 0
// is skipped because this stage never needs the MVP.
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 normalRow0;  // w = shading model
    vec4 normalRow1;
    vec4 normalRow2;
    vec4 selectionTint;                   // rgb = tint, a = how much to mix in
} pc;

// The one ForgeShape-owned MatCap, generated procedurally at device init by
// forgeshape_matcap.cpp. There is exactly one preset and no library.
layout(set = 0, binding = 0) uniform sampler2D matcapTexture;

layout(location = 0) in vec3 fragViewNormal;
layout(location = 1) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

// Must match ShadingModel in forgeshape_display.h.
const int kShadingStudioSolid = 0;
const int kShadingMatCap = 1;
const int kShadingDebugSourceColor = 2;

// ---------------------------------------------------------------------------
// Studio Solid lighting model
// ---------------------------------------------------------------------------
//
// Evaluated in VIEW space, so the lights FOLLOW THE CAMERA. That is a
// deliberate product decision, not an implementation convenience: while
// modelling, the user orbits constantly, and world-fixed lights would swing a
// face from lit to unlit purely because the viewpoint moved, which reads as the
// shape changing. Camera-relative light keeps a given surface orientation
// looking the same from every angle, so a change in shading always means a
// change in the MODEL. Every sculpting and CAD viewport that is pleasant to
// orbit in makes the same choice, and it is the same space the MatCap works in,
// so the two modes agree about where the light comes from and switching between
// them does not relight the object.
//
// Tuned FLATTER and more matte than the MatCap: Studio Solid is the neutral
// modelling default, where an even, low-drama surface makes planar faces and
// exact silhouettes easiest to judge. The MatCap is the higher-contrast, glossier
// form-reading view. The two are intentionally different in feel while sharing
// this geometry of light.
const vec3 kStudioAlbedo = vec3(0.660, 0.645, 0.625);  // neutral, faintly warm clay

const vec3 kStudioKeyDirection = vec3(-0.40, 0.60, 0.70);  // upper-left, toward the viewer
const float kStudioKeyIntensity = 0.95;
const vec3 kStudioKeyColor = vec3(1.00, 0.98, 0.94);

// Lower-FRONT, not lower-right, and deliberately weak.
//
// This direction is the one tuning decision that most affects whether a box
// reads as a box. A fill placed opposite the key (lower-right) lifts exactly
// the faces the key leaves dark, so a left-facing and a right-facing plane end
// up almost the same value and the form flattens out — which is what a
// symmetric two-light rig actually did here before it was measured. Pushing the
// fill DOWNWARD instead lets it do its real job, keeping undersides legible,
// without competing with the key across the vertical faces.
//
// Measured on the default 2 x 1 x 0.5 m box, view-space face values:
// left 0.49, right 0.29 — a 1.7x separation, where the previous rig gave
// 0.44 vs 0.40 and the two side faces were nearly indistinguishable.
const vec3 kStudioFillDirection = vec3(0.35, -0.55, 0.55);
const float kStudioFillIntensity = 0.18;
const vec3 kStudioFillColor = vec3(0.82, 0.87, 1.00);

// Hemispherical ambient: what keeps an unlit region oriented instead of flat,
// and what carries most of the shadow-side legibility now that the fill is
// weak. A surface facing straight away from the key still lands near 0.22
// rather than at black.
const vec3 kStudioSkyAmbient = vec3(0.460, 0.490, 0.540);
const vec3 kStudioGroundAmbient = vec3(0.260, 0.250, 0.240);

// Restrained on purpose: enough to separate a curved surface from a planar one,
// not enough to read as a material.
const float kStudioSpecularIntensity = 0.22;
const float kStudioSpecularExponent = 48.0;

// A small silhouette lift so the object separates from the dark viewport
// background without glowing.
const float kStudioRimIntensity = 0.08;
const float kStudioRimExponent = 3.0;
const vec3 kStudioRimColor = vec3(0.78, 0.84, 0.95);

// Safe normalization.
//
// A zero-length normal is a legitimate, documented value: it is what the render
// mesh builder emits for a corner that only degenerate triangles touch, and it
// is honest — that surface has no defined direction. Facing it at the viewer
// keeps such a fragment finite and neutral instead of black, NaN or a spike.
// The comparison is written inverted so a NaN component takes the fallback too.
vec3 safeViewNormal(vec3 n) {
    float lengthSquared = dot(n, n);
    if (!(lengthSquared > 1e-12)) {
        return vec3(0.0, 0.0, 1.0);
    }
    return n * inversesqrt(lengthSquared);
}

vec3 studioSolid(vec3 n) {
    vec3 key = normalize(kStudioKeyDirection);
    vec3 fill = normalize(kStudioFillDirection);
    vec3 view = vec3(0.0, 0.0, 1.0);

    float keyLambert = max(dot(n, key), 0.0) * kStudioKeyIntensity;
    float fillLambert = max(dot(n, fill), 0.0) * kStudioFillIntensity;

    vec3 ambient = mix(kStudioGroundAmbient, kStudioSkyAmbient, n.y * 0.5 + 0.5);

    // `halfVector`, not `half`: GLSL reserves `half` as a future type keyword.
    vec3 halfVector = normalize(key + view);
    float specular =
        pow(max(dot(n, halfVector), 0.0), kStudioSpecularExponent) * kStudioSpecularIntensity;

    float rim = pow(1.0 - clamp(n.z, 0.0, 1.0), kStudioRimExponent) * kStudioRimIntensity;

    vec3 diffuse = keyLambert * kStudioKeyColor + fillLambert * kStudioFillColor;
    return kStudioAlbedo * (ambient + diffuse) + vec3(specular) + rim * kStudioRimColor;
}

// The MatCap lookup, in full: a view-space unit normal's xy IS the texture
// coordinate. Everything else about the appearance was baked into the texture
// by forgeshape_matcap.cpp, which is why this mode costs one sample and no
// lighting arithmetic at all.
//
// The y flip matches the generator's row order (row 0 is n.y = +1).
vec3 matcap(vec3 n) {
    vec2 uv = vec2(n.x * 0.5 + 0.5, 1.0 - (n.y * 0.5 + 0.5));
    return texture(matcapTexture, uv).rgb;
}

void main() {
    vec3 n = safeViewNormal(fragViewNormal);
    int model = int(pc.normalRow0.w + 0.5);

    vec3 shaded;
    if (model == kShadingMatCap) {
        shaded = matcap(n);
    } else if (model == kShadingDebugSourceColor) {
        // DEBUG ONLY. Reproduces the pre-shading appearance exactly, so a
        // before/after comparison is one control away and needs no second build.
        shaded = fragColor;
    } else {
        shaded = studioSolid(n);
    }

    // Selection stays a whole-object tint rather than an outline, unchanged
    // from before this stage. Mixing AFTER shading rather than tinting the
    // albedo is what keeps the highlight equally obvious in Studio Solid, in
    // MatCap and in the debug mode, without any of them needing its own rule.
    outColor = vec4(mix(shaded, pc.selectionTint.rgb, pc.selectionTint.a), 1.0);
}
