// ForgeShape selection outline — how a SELECTED body's silhouette is drawn,
// never which body is selected and never what its geometry is.
//
// Platform-independent: no JNI, no Android, no Vulkan and no UI type appears
// here. Nothing in this file is truth about anything. It holds no ObjectId,
// mints no MeshRevision, moves no vertex, rebuilds no render mesh, reaches no
// `.forge` byte and cannot be observed by picking. Everything it computes is a
// colour, a width in pixels and a per-pixel coverage the fragment stage blends
// with.
//
// WHY A SEPARATE FILE FROM forgeshape_selection_pulse.h
// -----------------------------------------------------
// The two answer different questions about the same fact. The pulse answers
// "has this body JUST become selected", and its whole output is one alpha that
// decays to nothing. The outline answers "which body is selected RIGHT NOW",
// and it persists for as long as the answer does. `UI-OWNER-10` split them
// deliberately: the acknowledgement is allowed to be loud because it is brief,
// and the persistent state is a thin edge because the body underneath it is
// what the user is actually working on. Keeping them in one file would invite
// the next reader to fold one into the other, which is exactly the whole-object
// glow this stage removed.
//
// WHY THE KERNEL LIVES HERE AND IN A SHADER
// -----------------------------------------
// `selectionOutlineCoverage` is the REFERENCE implementation of the
// edge-extraction rule; `shaders/outline.frag` is the one the GPU runs, and it
// mirrors this file's tap counts and radius policy exactly, the same way
// shaders/grid.vert mirrors kGridDepthNudge. The reference exists so the rule
// can be proven deterministically on the CPU in microseconds — that an interior
// pixel is never painted, that a pixel far from the silhouette is never
// painted, and that the band that IS painted is the width the policy asked for
// — rather than being judged only from a screenshot.
#pragma once

#include "forgeshape_display.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// Width
// ---------------------------------------------------------------------------
//
// The outline is measured in SCREEN pixels and never in world units, which is
// what makes it stable under zoom: the band around a body 200 mm away and the
// band around the same body 20 m away are the same thickness, because neither
// is a function of the camera at all. A world-space shell would have been the
// other choice and would have grown and shrunk with the dolly.
//
// The pixel count is derived from the VIEWPORT's short side rather than fixed,
// for the same reason the gizmo is sized in reference units against the
// viewport height: a constant 3 px is a firm line on a 1080-wide phone and a
// hairline on a 1440-wide one. It is then CLAMPED at both ends, because the
// point of the band is to be legible without obscuring small geometry — the
// floor keeps it visible on a small window and the ceiling keeps it from
// swallowing a 12-pixel feature on a large one.
constexpr float kSelectionOutlineWidthFraction = 0.0028f;
constexpr float kSelectionOutlineMinPixels = 2.0f;
constexpr float kSelectionOutlineMaxPixels = 5.0f;

// The band's half-width in screen pixels for a viewport of this size.
//
// A degenerate or nonsensical viewport answers the floor rather than zero or a
// NaN: an outline that vanished on a 0x0 window would be a second failure
// stacked on the first, and the renderer already refuses to record a frame it
// has no extent for.
float selectionOutlineWidthPixels(int viewportWidth, int viewportHeight);

// ---------------------------------------------------------------------------
// Colour
// ---------------------------------------------------------------------------
//
// Two authored values, chosen by ONE question — `viewportBackgroundIsLight` —
// exactly as every other per-ground tool colour in the product asks it, so
// adding a sixth ground touches the palettes and never a switch in a renderer.
//
// The hue is the selection accent the acknowledgement pulse already uses, so
// the flash and the edge that follows it read as one act rather than as two
// unrelated pieces of feedback. What is NOT shared is the amount: the pulse
// floods the whole body for 220 ms and then leaves, and this is a band a few
// pixels wide that stays. That distinction is `UI-OWNER-10` in one sentence.
//
// Light Charcoal (#3C3F41) takes the DARK answer even though the gizmo's
// palette lumps it with the light ones. The gizmo's split is about saturation
// against a mid ground; this one is about luminance contrast against the pixels
// immediately outside a silhouette, and measured against #3C3F41 the bright
// amber clears 6.1:1 while the burnt one would sit at 1.4:1. The question the
// outline asks is genuinely "is this ground light", so it asks exactly that.
//
// Measured WCAG contrast against each of the five grounds is recorded in
// artifacts/sel-out-r1/VISUAL_EVIDENCE.md and pinned by the render-shading
// self-test, so a future palette edit that made the outline illegible fails a
// check rather than a screenshot review.
void selectionOutlineColor(ViewportBackground background, float* outRgb);

// The relative luminance of an sRGB triple, and the WCAG contrast ratio between
// two of them. Exposed because the self-test measures the authored colours
// against the authored grounds with the same arithmetic the evidence document
// quotes, rather than with a second formula that could drift from it.
//
// The viewport's swapchain format is UNORM rather than sRGB, so the values the
// renderer writes ARE the sRGB values the panel shows; treating them as sRGB
// here is therefore the measurement and not an approximation of one.
float srgbRelativeLuminance(const float* rgb);
float srgbContrastRatio(const float* a, const float* b);

// ---------------------------------------------------------------------------
// Edge extraction
// ---------------------------------------------------------------------------
//
// The rule, stated once: a pixel is part of the outline when it is OUTSIDE the
// selected body's visible coverage and some point within the band's width of it
// is INSIDE. Outside rather than inside on purpose — an inner band would sit on
// the body's own pixels and, on a small or thin body, would cover the whole of
// it, which is the "full-object fill" this stage exists to avoid. The cost of
// choosing outside is that a selected body reads a few pixels larger than it
// is, which is what every tool that draws one accepts.
//
// The mask the rule reads is the body's VISIBLE coverage — already depth
// resolved — so occlusion needs no rule of its own here: a pixel where the
// selected body is hidden behind another body is simply not in the mask, and
// the band therefore traces the visible contour and never an x-ray silhouette.
//
// The neighbourhood is sampled as two RINGS rather than a filled disc. For a
// solid region the two give the same answer everywhere except across features
// thinner than the inner ring, and a disc of radius 5 costs 81 taps per pixel
// against 12 — a full-screen difference that would be paid on every frame a
// body is selected.
constexpr int kSelectionOutlineOuterTaps = 8;
constexpr int kSelectionOutlineInnerTaps = 4;
constexpr float kSelectionOutlineInnerRingScale = 0.5f;

// Reference edge extraction for ONE pixel of an explicitly supplied mask.
//
// `mask` is `width * height` coverage values in [0, 1], row major, with row 0
// at the top — the layout the renderer's mask attachment has. Samples that fall
// outside the image read as 0, which is what the GPU sampler's opaque-black
// border does and is why a body touching the window edge does not grow an
// outline along the window edge.
//
// Returns the coverage to blend the outline colour at: 0 for an interior pixel,
// 0 for a pixel with nothing within the band, and the strongest neighbouring
// coverage otherwise.
float selectionOutlineCoverage(const float* mask, int width, int height, int x, int y,
                               float radiusPixels);

}  // namespace forgeshape
