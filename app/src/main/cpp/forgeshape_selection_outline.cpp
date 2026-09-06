#include "forgeshape_selection_outline.h"

#include <cmath>

namespace forgeshape {
namespace {

// Authored against the three DARK grounds (#302E2B, #26282A, #3C3F41), where it
// measures 7.84:1, 8.56:1 and 6.14:1. Lifted and de-saturated a little from the
// pulse's own 1.00/0.62/0.10 because a two-to-five pixel band needs more
// luminance to read than a flood of the same hue over a whole body does.
const float kOutlineOnDark[3] = {1.00f, 0.72f, 0.26f};

// Authored against the two LIGHT grounds (#EDE7DC, #E4E8EC), where it measures
// 6.25:1 and 6.25:1, and against a lit clay surface (0.83, 0.81, 0.78) where it
// still measures 4.94:1 — which matters because the band can fall on another
// body as well as on the ground. The direction is inverted from the dark
// answer, exactly as the grid's is: over paper a selection edge sinks darker
// instead of lifting brighter.
const float kOutlineOnLight[3] = {0.58f, 0.20f, 0.00f};

float channelLuminance(float c) {
    // The sRGB electro-optical transfer function, as WCAG states it.
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Nearest-neighbour read with an opaque-black border, matching the GPU
// sampler's VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER. Clamp-to-edge would have
// smeared the last row outward and drawn a band along the window edge for any
// body that reaches it.
float sampleMask(const float* mask, int width, int height, int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return 0.0f;
    }
    return mask[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
}

}  // namespace

float selectionOutlineWidthPixels(int viewportWidth, int viewportHeight) {
    const int shortSide = (viewportWidth < viewportHeight) ? viewportWidth : viewportHeight;
    if (shortSide <= 0) {
        // A window with no extent. The floor rather than zero: the caller is
        // about to be refused a frame anyway, and answering 0 here would make
        // a later divide look like a working outline of no width.
        return kSelectionOutlineMinPixels;
    }
    const float derived = static_cast<float>(shortSide) * kSelectionOutlineWidthFraction;
    if (!(derived > kSelectionOutlineMinPixels)) {
        return kSelectionOutlineMinPixels;  // inverted so a NaN lands on the floor
    }
    if (derived > kSelectionOutlineMaxPixels) {
        return kSelectionOutlineMaxPixels;
    }
    return derived;
}

void selectionOutlineColor(ViewportBackground background, float* outRgb) {
    if (outRgb == nullptr) {
        return;
    }
    const float* source = viewportBackgroundIsLight(background) ? kOutlineOnLight : kOutlineOnDark;
    for (int i = 0; i < 3; ++i) {
        outRgb[i] = source[i];
    }
}

float srgbRelativeLuminance(const float* rgb) {
    if (rgb == nullptr) {
        return 0.0f;
    }
    return 0.2126f * channelLuminance(rgb[0]) + 0.7152f * channelLuminance(rgb[1]) +
           0.0722f * channelLuminance(rgb[2]);
}

float srgbContrastRatio(const float* a, const float* b) {
    const float la = srgbRelativeLuminance(a);
    const float lb = srgbRelativeLuminance(b);
    const float hi = (la > lb) ? la : lb;
    const float lo = (la > lb) ? lb : la;
    return (hi + 0.05f) / (lo + 0.05f);
}

float selectionOutlineCoverage(const float* mask, int width, int height, int x, int y,
                               float radiusPixels) {
    if (mask == nullptr || width <= 0 || height <= 0) {
        return 0.0f;
    }
    if (sampleMask(mask, width, height, x, y) > 0.5f) {
        // Interior. The body's own visible pixels are never painted over, which
        // is what keeps this an outline rather than a highlight.
        return 0.0f;
    }
    if (!(radiusPixels > 0.0f)) {
        return 0.0f;
    }

    constexpr float kTwoPi = 6.28318530717958647692f;
    float best = 0.0f;
    for (int i = 0; i < kSelectionOutlineOuterTaps; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) /
                            static_cast<float>(kSelectionOutlineOuterTaps);
        const int sx = x + static_cast<int>(std::lround(std::cos(angle) * radiusPixels));
        const int sy = y + static_cast<int>(std::lround(std::sin(angle) * radiusPixels));
        const float v = sampleMask(mask, width, height, sx, sy);
        if (v > best) best = v;
    }
    // The inner ring is offset by half a step so its taps fall BETWEEN the
    // outer ones rather than along the same spokes, which is what keeps a
    // narrow feature from slipping through the gap between two rays.
    const float innerRadius = radiusPixels * kSelectionOutlineInnerRingScale;
    for (int i = 0; i < kSelectionOutlineInnerTaps; ++i) {
        const float angle = kTwoPi * (static_cast<float>(i) + 0.5f) /
                            static_cast<float>(kSelectionOutlineInnerTaps);
        const int sx = x + static_cast<int>(std::lround(std::cos(angle) * innerRadius));
        const int sy = y + static_cast<int>(std::lround(std::sin(angle) * innerRadius));
        const float v = sampleMask(mask, width, height, sx, sy);
        if (v > best) best = v;
    }
    return (best > 1.0f) ? 1.0f : best;
}

}  // namespace forgeshape
