#version 450

// ForgeShape Construction Move / Rotate gizmo, vertex stage.
//
// The gizmo is a TOOL drawn in the viewport, not model geometry: it has no
// ObjectId, no MeshRevision, is never published, is never picked by pickScene
// and is never exported. It reaches the GPU through its own tiny pipeline for
// the same reason the grid does — so that nothing about it can consult the
// MatCap, the shading model or a body's transform.
//
// Push constant budget
// --------------------
// Exactly 128 bytes, the smallest maxPushConstantsSize Vulkan guarantees, the
// same budget the surface and grid stages work inside.
//
//   offset   0  mat4 mvp        canonical gizmo space -> clip. The gizmo's
//                               vertices are authored ONCE at unit size around
//                               the origin; this matrix carries the pivot and
//                               the camera-derived uniform scale that keeps the
//                               handles a near-constant size on screen.
//   offset  64  vec4 axisXColor rgb + the alpha it blends at
//   offset  80  vec4 axisYColor
//   offset  96  vec4 axisZColor
//   offset 112  vec4 control    x = which axis is held (0 none, 1 X, 2 Y, 3 Z)
//                               y = alpha multiplier for the axes NOT held
//                               z = alpha multiplier for the one that IS
//                               w = unused
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 axisXColor;
    vec4 axisYColor;
    vec4 axisZColor;
    vec4 control;
} pc;

layout(location = 0) in vec3 inPosition;
// Which axis this vertex belongs to: 1 X, 2 Y, 3 Z. It travels in the vertex
// buffer and the COLOUR does not, so switching appearance — or highlighting a
// held handle — rewrites push constants and re-uploads nothing at all.
layout(location = 1) in float inAxis;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    vec4 color = pc.axisXColor;
    if (inAxis > 2.5) {
        color = pc.axisZColor;
    } else if (inAxis > 1.5) {
        color = pc.axisYColor;
    }

    // The held handle is stated by weight rather than by hue, so which axis a
    // handle IS and whether it is currently held are two independent readings
    // and neither has to be inferred from the other.
    float held = abs(inAxis - pc.control.x) < 0.5 ? 1.0 : 0.0;
    float weight = mix(pc.control.y, pc.control.z, held);
    // No axis is dimmed while nothing is held: control.x is 0, which matches no
    // axis, so without this the resting gizmo would draw entirely at the
    // "not held" weight.
    if (pc.control.x < 0.5) {
        weight = 1.0;
    }

    fragColor = vec4(color.rgb, color.a * weight);
}
