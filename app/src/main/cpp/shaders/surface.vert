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

layout(location = 0) out vec3 fragViewNormal;
layout(location = 1) out vec3 fragColor;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);

    // Normals are transformed into VIEW space by the upper-left 3x3 of
    // (view * model), applied directly rather than as an inverse-transpose.
    //
    // That shortcut is valid because BOTH factors are rigid: the camera's view
    // matrix is a look-at (rotation + translation) and ConstructionTransform is
    // documented as rotation + translation with NO scale, so the product is
    // orthonormal and its inverse-transpose is itself.
    //
    // If scale is ever added to the transform, this is one of the two places
    // that silently becomes wrong (the other is picking's "local distance is
    // world distance" shortcut). The CPU must then send a real inverse-
    // transpose; the shader itself would not have to change.
    fragViewNormal = vec3(dot(pc.normalRow0.xyz, inNormal),
                          dot(pc.normalRow1.xyz, inNormal),
                          dot(pc.normalRow2.xyz, inNormal));

    // Carried through for the debug-only source-colour mode. Studio Solid and
    // MatCap both ignore it.
    fragColor = inColor;
}
