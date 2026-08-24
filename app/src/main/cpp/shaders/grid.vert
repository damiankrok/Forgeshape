#version 450

// ForgeShape world reference grid, vertex stage.
//
// Push constant budget
// --------------------
// Exactly 128 bytes, the smallest maxPushConstantsSize Vulkan guarantees — the
// same budget shaders/surface.vert works inside, and for the same reason. The
// four tier colours fill it precisely, which is also why a fifth line weight is
// not a thing this grid can grow: it would need a descriptor, and a floor does
// not deserve one.
//
//   offset   0  mat4 viewProj      world -> clip. There is no model matrix:
//                                  the grid IS world space.
//   offset  64  vec4 minorColor    rgb + the alpha it blends at
//   offset  80  vec4 majorColor
//   offset  96  vec4 axisXColor
//   offset 112  vec4 axisZColor
layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec4 minorColor;
    vec4 majorColor;
    vec4 axisXColor;
    vec4 axisZColor;
} pc;

layout(location = 0) in vec3 inPosition;
// GridLineTier's numeric value: 0 minor, 1 major, 2 X axis, 3 Z axis. The TIER
// travels in the vertex buffer and the COLOUR does not, which is what makes
// switching appearance cost four push-constant writes and zero uploads.
layout(location = 1) in float inTier;

layout(location = 0) out vec4 fragColor;
// The world-space position of this fragment on the grid plane, interpolated
// along the line. The fragment stage needs it to fade by distance from the
// origin — see the note in grid.frag for why that cannot be done here.
layout(location = 1) out vec2 fragPlanePosition;

// Mirrored from kGridDepthNudge in forgeshape_renderer.cpp. See there for why
// the coplanar case is settled here rather than with VkPipelineRasterizationState's
// depth bias, which is defined for polygon fragments and would do nothing to a
// line list.
const float kDepthNudge = 1.0e-4;

void main() {
    gl_Position = pc.viewProj * vec4(inPosition, 1.0);

    // Push the grid a hair FURTHER from the eye, so that where it is exactly
    // coplanar with real geometry — a Construction Plane lying on the world
    // floor — it loses the depth test cleanly instead of fighting for the
    // pixel. NDC depth is z/w, so scaling the nudge by w applies it after the
    // perspective divide and makes it mean the same thing in both projections
    // (in Orthographic w is 1 and this is simply an additive offset).
    gl_Position.z += kDepthNudge * gl_Position.w;

    vec4 color = pc.minorColor;
    if (inTier > 2.5) {
        color = pc.axisZColor;
    } else if (inTier > 1.5) {
        color = pc.axisXColor;
    } else if (inTier > 0.5) {
        color = pc.majorColor;
    }
    fragColor = color;

    fragPlanePosition = inPosition.xz;
}
