#version 450

// ForgeShape selection outline, COMPOSITE pass, vertex stage.
//
// A full-screen triangle generated from gl_VertexIndex, with no vertex buffer,
// no index buffer and nothing to upload or free. One triangle rather than two:
// it covers the same area with no diagonal seam, which is where a two-triangle
// quad makes neighbouring fragments come from different primitives and can
// disturb derivative-based filtering.
//
// The mapping is deliberately the identity between the composite's texture
// coordinate and the mask's texel grid. uv.y = 0 is clip y = -1, which is the
// TOP row of a Vulkan framebuffer, and it is also texel row 0 of the mask image
// the previous pass rendered into with the same projection. So no flip exists
// anywhere in this path, and a flip added "to correct" it would put the outline
// on the mirror image of the selected body.
layout(location = 0) out vec2 fragUv;

void main() {
    // (0,0), (2,0), (0,2) -> a triangle whose inscribed [0,1] square is the
    // whole viewport.
    vec2 uv = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    fragUv = uv;
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
