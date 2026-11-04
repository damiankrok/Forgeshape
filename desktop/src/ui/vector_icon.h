// The Android vector drawables, read and drawn on the desktop (`DESKTOP-R1`):
// an icon has ONE source, `app/src/main/res/drawable/ic_*.xml`, embedded into
// the desktop build at configure time and rasterised here by ForgeShape's own
// path parser and raster.h. The subset the product's icons use: <vector> with
// its viewport, <path> with pathData (M L H V C S Q T A Z, absolute and
// relative), fill and stroke colours' alpha, fillAlpha, strokeWidth and round
// caps. Colour is a TINT the interface chooses, so only coverage is kept.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "raster.h"

namespace fsui {

struct VectorPath {
    std::vector<std::vector<Point>> contours;  // viewport units
    std::vector<bool> closed;
    float fillAlpha = 0.0f;    // 0: no fill
    float strokeAlpha = 0.0f;  // 0: no stroke
    float strokeWidth = 0.0f;
    bool roundCaps = false;
};

struct VectorIcon {
    float viewportWidth = 24.0f;
    float viewportHeight = 24.0f;
    std::vector<VectorPath> paths;
};

// Parses one drawable's XML text. False when it is not a <vector>.
bool parseVectorDrawable(const std::string& xml, VectorIcon* out);

// Rasterises into a size x size coverage bitmap (row-major, top row first).
void rasterizeIcon(const VectorIcon& icon, int size, std::vector<uint8_t>* out);

}  // namespace fsui
