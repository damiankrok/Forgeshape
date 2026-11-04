#version 450
// One pipeline for the whole desktop interface (see forgeshape_ui_frame.h):
// atlas coverage for text and icons, and rounded rectangles, outlines and
// soft shadows drawn from their own signed distance, so every edge is
// anti-aliased at any size without a texture.
layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec2 vLocal;
layout(location = 2) in vec2 vHalf;
layout(location = 3) in vec3 vShape;
layout(location = 4) in vec4 vColor;

layout(location = 0) out vec4 outColor;

float roundedBox(vec2 p, vec2 halfSize, float radius) {
    vec2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main() {
    float mode = vShape.y;
    float coverage;
    if (mode < 0.5) {
        coverage = texture(atlas, vUv).r;
    } else if (mode < 1.5) {
        coverage = clamp(0.5 - roundedBox(vLocal, vHalf, vShape.x), 0.0, 1.0);
    } else if (mode < 2.5) {
        float d = roundedBox(vLocal, vHalf, vShape.x);
        float w = vShape.z;
        coverage = clamp(0.5 - (abs(d + 0.5 * w) - 0.5 * w), 0.0, 1.0);
    } else if (mode < 3.5) {
        float d = roundedBox(vLocal, vHalf, vShape.x);
        coverage = 1.0 - smoothstep(-vShape.z, vShape.z, d);
    } else {
        // A segment (the view cube's axes): vLocal is relative to its first
        // end, vHalf is the vector to the second, vShape.x the half width.
        vec2 ba = vHalf;
        float h = clamp(dot(vLocal, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
        float d = length(vLocal - ba * h) - vShape.x;
        coverage = clamp(0.5 - d, 0.0, 1.0);
    }
    outColor = vec4(vColor.rgb, vColor.a * coverage);
}
