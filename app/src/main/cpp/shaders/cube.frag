#version 450

// Offset 64 sits just past the vertex stage's mat4 MVP in the same push
// constant block. rgb is the selection tint, a is how much of it to mix in;
// a == 0 means "not selected".
layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 selectionTint;
} pc;

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 base = mix(fragColor, pc.selectionTint.rgb, pc.selectionTint.a);
    outColor = vec4(base, 1.0);
}
