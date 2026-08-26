#version 450

// ForgeShape Construction transform gizmo, vertex stage.
//
// The gizmo is a TOOL drawn in the viewport, not model geometry: it has no
// ObjectId, no MeshRevision, is never published, is never picked by pickScene
// and is never exported. It reaches the GPU through its own tiny pipeline for
// the same reason the grid does — so that nothing about it can consult the
// MatCap, the shading model or a body transform.
//
// Push constant budget
// --------------------
// Exactly 128 bytes, the smallest maxPushConstantsSize Vulkan guarantees, the
// same budget the surface and grid stages work inside. That leaves precisely
// four vec4s after the matrix, so the three axis colours carry the remaining
// scalars in their otherwise-wasted w components — the same technique the
// surface stage uses to carry its shading model. Each is documented here and
// packed in exactly one place on the CPU (GizmoPush, forgeshape_renderer.cpp).
//
//   offset   0  mat4 mvp        canonical gizmo space -> clip. The vertices are
//                               authored ONCE at reference-unit size around the
//                               origin; this matrix carries the pivot, the
//                               world/local BASIS and the camera-derived uniform
//                               scale that keeps the handles a near-constant size
//                               on screen. The body own scale is deliberately not
//                               in it: stretching a body must not stretch the
//                               instrument used to stretch it.
//   offset  64  vec4 axisX      rgb = X colour, w = neutral grey level
//   offset  80  vec4 axisY      rgb = Y colour, w = which HANDLE is held
//                               (0 none, 1..3 axis, 4..6 plane, 7 uniform)
//   offset  96  vec4 axisZ      rgb = Z colour, w = alpha multiplier for the
//                               handles that are NOT held
//   offset 112  vec4 highlight  rgb = the colour a held handle is drawn in,
//                               w   = the base alpha every handle draws at
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 axisX;
    vec4 axisY;
    vec4 axisZ;
    vec4 highlight;
} pc;

layout(location = 0) in vec3 inPosition;
// Which axis gives this vertex its HUE: 0 neutral, 1 X, 2 Y, 3 Z. A plane handle
// carries the axis perpendicular to it, so the XY square is the blue one.
layout(location = 1) in float inAxis;
// Which HANDLE this vertex belongs to, as a GizmoHandle code. Separate from the
// hue on purpose: without it, holding the Z axis would light the XY plane too,
// because the two legitimately share a colour.
layout(location = 2) in float inHandle;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    vec3 base = pc.axisX.rgb;
    if (inAxis < 0.5) {
        // No axis at all — the uniform-scale cube. Drawn at the neutral grey the
        // appearance owns, because giving it an axis hue would say it belonged
        // to an axis.
        base = vec3(pc.axisX.w);
    } else if (inAxis > 2.5) {
        base = pc.axisZ.rgb;
    } else if (inAxis > 1.5) {
        base = pc.axisY.rgb;
    }

    float heldHandle = pc.axisY.w;
    float held = (heldHandle > 0.5 && abs(inHandle - heldHandle) < 0.5) ? 1.0 : 0.0;

    // The held handle is stated TWICE over, by hue and by weight, and the other
    // handles drop away rather than the held one merely brightening. Which
    // handle a thing IS stays carried by its geometry — a shaft that points one
    // way, a ring in one plane, a square in one plane, a cube on one shaft — so
    // a reader who can separate neither hue still has both the shape and the
    // weight to go on.
    float weight = (heldHandle < 0.5) ? 1.0 : mix(pc.axisZ.w, 1.0, held);
    vec3 rgb = mix(base, pc.highlight.rgb, held);

    fragColor = vec4(rgb, pc.highlight.w * weight);
}
