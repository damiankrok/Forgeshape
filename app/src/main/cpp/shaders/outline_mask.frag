#version 450

// ForgeShape selection outline, MASK pass, fragment stage.
//
// The whole shader. Unselected bodies still run it — they must, because they
// are what writes the depth that occludes the selected one — and they write 0,
// which is what the attachment was cleared to. Only the selected body writes 1.
//
// The attachment is a single-channel R8_UNORM image, so this costs one byte per
// pixel and no blending: the mask is a coverage question with two answers, not
// an image.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 params;
} pc;

layout(location = 0) out float outMask;

void main() {
    outMask = pc.params.x;
}
