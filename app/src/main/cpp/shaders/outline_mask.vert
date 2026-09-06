#version 450

// ForgeShape selection outline, MASK pass, vertex stage.
//
// This is the pass that makes the outline a TRUE geometry-derived silhouette
// rather than a screen-space approximation of one. It rasterises the scene's
// bodies exactly as the surface pass does — the same device-local vertex and
// index buffers, the same model transform, the same camera — and writes a
// single coverage value instead of a shaded colour. Every body is drawn so that
// the depth buffer resolves occlusion for free: what survives in the mask is
// the part of the selected body that is actually VISIBLE, which is why the
// composite pass needs no occlusion rule of its own and cannot draw an x-ray.
//
// It consumes ONLY position. The render mesh's normal and colour channels are
// present in the buffer and deliberately not declared here: a mask has no
// shading, and the pipeline's vertex input state declares one attribute over
// the same RenderVertex stride so nothing is read that is not used.
//
// Push constant budget
// --------------------
//   offset   0  mat4 mvp     model -> clip, composed by the CPU exactly as the
//                            surface pass composes it, so the mask and the
//                            shaded body cannot land on different pixels
//   offset  64  vec4 params  x = the coverage this body writes: 1 for the
//                            selected body, 0 for every other one. y, z, w are
//                            unused and written as zero.
//
// 80 bytes, well inside the guaranteed 128.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 params;
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
}
