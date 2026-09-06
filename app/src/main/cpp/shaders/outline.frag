#version 450

// ForgeShape selection outline, COMPOSITE pass, fragment stage.
//
// The edge-extraction rule, and the GPU half of a pair: forgeshape_selection_
// outline.cpp holds the CPU REFERENCE implementation of exactly this rule, and
// the render-shading self-test proves the reference's properties
// deterministically. The two are mirrored the same way shaders/grid.vert
// mirrors kGridDepthNudge — constants and tap counts are stated in both places
// and must be changed in both.
//
// The rule: a pixel is outline when it is OUTSIDE the selected body's visible
// coverage and some point within the band's width of it is INSIDE.
//
//   * outside rather than inside, so a small or thin body is edged rather than
//     filled — the "no full-object fill" half of UI-OWNER-10;
//   * the mask is already depth-resolved by the mask pass, so a pixel where the
//     selected body is hidden behind another body is simply not in the mask.
//     Occlusion therefore needs no rule here and an x-ray outline is not
//     something this shader could draw even if it wanted to;
//   * every offset is in TEXELS scaled by the band radius the CPU computed from
//     the viewport size, so the band is screen-space stable: the camera is not
//     an input to this shader at all, and zooming cannot change the width.
//
// Two RINGS rather than a filled disc. For a solid region the answers agree
// everywhere except across features narrower than the inner ring, and a disc of
// radius 5 would cost 81 taps per pixel against these 12 — paid full-screen on
// every frame a body is selected.
layout(set = 0, binding = 0) uniform sampler2D maskTexture;

// Push constant budget
// --------------------
//   offset  0  vec4 color   rgb = the outline colour for this viewport ground,
//                           a   = the strength it is blended at
//   offset 16  vec4 params  xy = one texel in uv (1 / mask extent)
//                           z  = the band radius in PIXELS
//                           w  = unused, written as zero
//
// 32 bytes.
layout(push_constant) uniform PushConstants {
    vec4 color;
    vec4 params;
} pc;

layout(location = 0) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

// Mirrored from kSelectionOutlineOuterTaps / kSelectionOutlineInnerTaps /
// kSelectionOutlineInnerRingScale in forgeshape_selection_outline.h.
const int kOuterTaps = 8;
const int kInnerTaps = 4;
const float kInnerRingScale = 0.5;
const float kTwoPi = 6.28318530717958647692;

void main() {
    // The body's own visible pixels are never painted over. This is what keeps
    // the result an outline and not a highlight, and it is also the early-out
    // that makes the interior of a large selected body cost one texture fetch.
    if (texture(maskTexture, fragUv).r > 0.5) {
        discard;
    }

    float radius = pc.params.z;
    vec2 texel = pc.params.xy;
    float best = 0.0;

    for (int i = 0; i < kOuterTaps; ++i) {
        float angle = kTwoPi * float(i) / float(kOuterTaps);
        best = max(best, texture(maskTexture, fragUv + vec2(cos(angle), sin(angle)) * radius * texel).r);
    }
    // Offset by half a step so the inner taps fall BETWEEN the outer spokes
    // rather than along them, which is what stops a narrow feature slipping
    // through the gap between two rays.
    float innerRadius = radius * kInnerRingScale;
    for (int i = 0; i < kInnerTaps; ++i) {
        float angle = kTwoPi * (float(i) + 0.5) / float(kInnerTaps);
        best = max(best, texture(maskTexture, fragUv + vec2(cos(angle), sin(angle)) * innerRadius * texel).r);
    }

    if (best <= 0.0) {
        discard;  // nothing selected within the band: the overwhelming majority
    }

    // The sampler filters linearly and clamps to an opaque-black border, so
    // `best` is a fractional coverage at the band's outer limit — an
    // anti-aliased edge for free — and a body touching the window edge grows no
    // band along the window edge, because the border reads as 0.
    outColor = vec4(pc.color.rgb, pc.color.a * clamp(best, 0.0, 1.0));
}
