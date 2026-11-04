#version 450

// ForgeShape feature edges, fragment stage: one ink per ground family, chosen
// on the CPU through viewportBackgroundIsLight.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 colour;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = pc.colour;
}
