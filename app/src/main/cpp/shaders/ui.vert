#version 450
// The desktop shell's interface (`DESKTOP-R1`): pixel positions in, clip space
// out; everything else passes through to the fragment stage.
layout(push_constant) uniform UiPush {
    vec2 viewport;  // pixels
} pc;

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec2 inLocal;
layout(location = 3) in vec2 inHalf;
layout(location = 4) in vec3 inShape;  // radius, mode, param
layout(location = 5) in vec4 inColor;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec2 vLocal;
layout(location = 2) out vec2 vHalf;
layout(location = 3) out vec3 vShape;
layout(location = 4) out vec4 vColor;

void main() {
    gl_Position = vec4(inPos / pc.viewport * 2.0 - 1.0, 0.0, 1.0);
    vUv = inUv;
    vLocal = inLocal;
    vHalf = inHalf;
    vShape = inShape;
    vColor = inColor;
}
