#version 450

// ForgeShape surface vertex stage.
//
// Push constant budget
// --------------------
// This block is exactly 128 bytes, which is the SMALLEST maxPushConstantsSize
// Vulkan guarantees. Staying inside that guarantee is why the normal matrix is
// carried as three rows rather than a mat4, and why the shading model rides in
// an otherwise-wasted w component instead of taking a fifth 16-byte slot. Adding
// one more vec4 here would make the pipeline layout invalid on conformant
// implementations that expose only the minimum.
//
//   offset   0  mat4 mvp
//   offset  64  vec4 normalRow0   xyz = row 0 of the view-space normal matrix
//                                 w   = shading model (see ShadingModel)
//   offset  80  vec4 normalRow1   xyz = row 1
//   offset  96  vec4 normalRow2   xyz = row 2
//   offset 112  vec4 selectionTint rgb = tint, a = how much to mix in
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 normalRow0;
    vec4 normalRow1;
    vec4 normalRow2;
    vec4 selectionTint;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
// The Sculpt Mask weight, already clamped into [0, 1] on the CPU. Zero for
// every representation but a masked Frozen Sculpt Mesh.
layout(location = 3) in float inMask;

layout(location = 0) out vec3 fragViewNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out float fragMask;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    // Normals are transformed into VIEW space by a matrix the CPU has already
    // built as a real inverse-transpose: view * (R * S^-1), where S is the
    // body's local scale. See ConstructionTransform::normalMatrix.
    //
    // It is NOT the upper-left 3x3 of (view * model). The two agree exactly for
    // every unscaled body and part company the moment a body carries a
    // non-uniform scale, where the model would stretch a normal the same way it
    // stretches a position and tilt it off the surface it belongs to.
    //
    // The result is deliberately left UNNORMALISED here: a non-uniform scale
    // changes a normal's length as well as its direction, and the fragment
    // stage normalises once per fragment anyway (see safeViewNormal).
    fragViewNormal = vec3(dot(pc.normalRow0.xyz, inNormal),
                          dot(pc.normalRow1.xyz, inNormal),
                          dot(pc.normalRow2.xyz, inNormal));

    // Carried through for the debug-only source-colour mode. Studio Solid and
    // MatCap both ignore it.
    fragColor = inColor;

    // Interpolated across the triangle, which is what makes a mask painted with
    // a smooth falloff read as a soft edge rather than a per-vertex stipple.
    fragMask = inMask;
}
