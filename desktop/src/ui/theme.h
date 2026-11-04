// The desktop interface's theme tokens (`DESKTOP-R1`). The colour ROLES are
// the Android attrs.xml roles, so a palette means the same thing on both
// shells; the default values are Warm Graphite's. A user theme file
// (theme.json in the per-user settings folder) overrides any token by name --
// colours as "#RRGGBB" / "#AARRGGBB", sizes as numbers in density-independent
// pixels -- read by the project's own JSON reader. Presentation only: never a
// project byte.
#pragma once

#include <cstdint>
#include <string>

namespace fsui {

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    uint32_t packed() const {
        return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8)
               | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(a) << 24);
    }
    Color alpha(float f) const {
        Color c = *this;
        c.a = static_cast<uint8_t>(a * (f < 0 ? 0 : f > 1 ? 1 : f));
        return c;
    }
    static Color hex(uint32_t argb) {
        return Color{static_cast<uint8_t>(argb >> 16), static_cast<uint8_t>(argb >> 8),
                     static_cast<uint8_t>(argb), static_cast<uint8_t>(argb >> 24)};
    }
};

struct Theme {
    // Colour roles (attrs.xml names without the fs prefix).
    Color viewportBackground = Color::hex(0xFF302E2B);
    Color chromeSurface = Color::hex(0xFF3A3835);
    Color surfaceFloating = Color::hex(0xF246433F);
    Color surfacePrecision = Color::hex(0xFF403D39);
    Color chromeHairline = Color::hex(0xFF565149);
    Color controlSurface = Color::hex(0xFF45423D);
    Color controlSurfacePressed = Color::hex(0xFF565249);
    Color controlBorder = Color::hex(0xFF565149);
    Color fieldSurface = Color::hex(0xFF34322F);
    Color accent = Color::hex(0xFF4C8FD6);
    Color accentFill = Color::hex(0xFF52504A);
    Color primaryFillPressed = Color::hex(0xFF6BA3DF);
    Color textOnPrimary = Color::hex(0xFF12110F);
    Color textPrimary = Color::hex(0xFFF4F1EB);
    Color textSecondary = Color::hex(0xFFB6B1A9);
    Color textDisabled = Color::hex(0xFF7A746C);
    Color textSuccess = Color::hex(0xFF7FB388);
    Color textError = Color::hex(0xFFE8A093);
    Color textMeasure = Color::hex(0xFFE0A84B);
    Color shadow = Color::hex(0x66000000);

    // Sizes, in density-independent pixels.
    float fontSize = 13.5f;
    float smallFontSize = 12.0f;
    float titleFontSize = 12.0f;
    float iconSize = 20.0f;
    float controlHeight = 32.0f;
    float rowHeight = 30.0f;
    float radius = 8.0f;
    float panelRadius = 12.0f;
    float spacing = 8.0f;
    float toolbarHeight = 48.0f;
    float statusHeight = 28.0f;
    float browserWidth = 264.0f;
    float propertiesWidth = 300.0f;
    float panelInset = 10.0f;
    float shadowBlur = 16.0f;

    // Applies a theme file's overrides; unknown names and bad values are
    // ignored, so a typo costs that one token and nothing else.
    bool loadOverrides(const std::string& path, std::string* report);
};

}  // namespace fsui
