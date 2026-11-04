#version 450

// ForgeShape feature edges, vertex stage (`WORKBENCH-UX-R1`).
//
// A body's feature edges are an index range appended to its OWN index buffer
// (forgeshape_feature_edges.h), drawn as a line list over the very vertex
// buffer the body's shaded draw binds, so the line lies exactly on the
// surface it outlines. Position only, over the RenderVertex stride.
//
// Push constants -- the outline mask's 80-byte layout, reused:
//   offset  0  mat4 mvp     model -> clip, composed as the surface pass does
//   offset 64  vec4 colour  the edge ink, straight alpha
//
// The line is nudged a hair toward the eye, the grid's own size (1e-4 of clip
// w), so under a LESS_OR_EQUAL test it wins against the faces it borders while
// an edge BEHIND another face still fails and stays hidden.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 colour;
} pc;

layout(location = 0) in vec3 inPosition;

const float kEdgeDepthNudge = 0.0001;

void main() {
    vec4 clip = pc.mvp * vec4(inPosition, 1.0);
    clip.z -= kEdgeDepthNudge * clip.w;
    gl_Position = clip;
}
