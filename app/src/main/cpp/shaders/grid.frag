#version 450

// ForgeShape world reference grid, fragment stage.
//
// The grid is a REFERENCE, not a surface: it has no normal, takes no light, is
// unaffected by the shading model and never consults the MatCap — which is why
// the grid pipeline needs no descriptor set at all. All this stage does is the
// radial fade, and then hand the colour over for blending.
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragPlanePosition;

layout(location = 0) out vec4 outColor;

// Where the radial fade begins, as a fraction of the half extent, and the half
// extent itself. Mirrored from forgeshape_grid.h; they are compile-time
// constants on both sides because nothing can move them at run time.
const float kHalfExtent = 20.0;
const float kFadeStart = 0.55;

void main() {
    // The grid has a FIXED extent, so without this it would end at a hard
    // square border 20 m out — which reads as a table top rather than as a
    // floor, and draws attention to exactly the arbitrary limit it should hide.
    // Fading radially by distance from the world origin lets a bounded grid
    // present as an open plane, and it also thins the far lines in Perspective
    // where they would otherwise converge into aliased noise.
    //
    // PER FRAGMENT, and that is not an optimisation choice — it is the only
    // place this can be computed at all. A grid line's only two vertices are
    // its far ENDPOINTS, both of which sit at the outer edge of the extent, so
    // a per-vertex fade evaluates to zero at both ends and the hardware then
    // interpolates zero across the entire line. Every line disappears, at every
    // radius. (This was written per-vertex first, and that is exactly what it
    // did.)
    float radius = length(fragPlanePosition) / kHalfExtent;
    float fade = 1.0 - smoothstep(kFadeStart, 1.0, radius);

    // Straight (non-premultiplied) alpha, matching the SRC_ALPHA /
    // ONE_MINUS_SRC_ALPHA colour blend the grid pipeline declares. Weight comes
    // from alpha alone, so a line can be made subtler without being made a
    // different colour — see the palette rules in forgeshape_grid.cpp.
    outColor = vec4(fragColor.rgb, fragColor.a * fade);
}
