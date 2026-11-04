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
// The SELECTED body is drawn last and nudged a hair toward the eye
// (`WORKBENCH-UX-R1`), so where it shares a plane with another body the tie
// resolves to it every pixel; two meshes on one plane interpolate depths a
// unit apart, and under a plain test the band broke into speckle there. The
// nudge is the grid's own size (kGridDepthNudge, 1e-4 of clip w), far below
// any real separation between surfaces.
//
// 80 bytes, well inside the guaranteed 128.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 params;
} pc;

layout(location = 0) in vec3 inPosition;

const float kSelectedDepthNudge = 0.0001;

void main() {
    vec4 clip = pc.mvp * vec4(inPosition, 1.0);
    if (pc.params.x > 0.5) {
        clip.z -= kSelectedDepthNudge * clip.w;
    }
    gl_Position = clip;
}
