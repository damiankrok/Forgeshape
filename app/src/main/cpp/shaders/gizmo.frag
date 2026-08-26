#version 450

// ForgeShape Construction gizmo, fragment stage.
//
// A handle is a tool, not a surface: it has no normal, takes no light, is
// unaffected by the shading model and never consults the MatCap — which is why
// the gizmo pipeline, like the grid's, needs no descriptor set at all. There is
// nothing to compute here; the vertex stage has already resolved which axis
// this is and how strongly it is drawn.
layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Straight (non-premultiplied) alpha, matching the SRC_ALPHA /
    // ONE_MINUS_SRC_ALPHA colour blend the gizmo pipeline declares.
    outColor = fragColor;
}
