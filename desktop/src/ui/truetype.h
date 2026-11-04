// ForgeShape's own TrueType reader (`DESKTOP-R1`): the outlines of a system
// font (Segoe UI on Windows, DejaVu Sans on a Linux runner), read straight from
// the file -- cmap formats 4 and 12, loca/glyf with composite glyphs, hmtx --
// and rasterised by raster.h. No third-party font library.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "raster.h"

namespace fsui {

class TrueTypeFont {
public:
    bool loadFile(const std::string& path);
    bool load(std::vector<uint8_t> bytes);
    bool valid() const { return glyf_ != 0; }

    uint32_t glyphIndex(uint32_t codepoint) const;
    // Font units.
    int unitsPerEm() const { return unitsPerEm_; }
    int ascender() const { return ascender_; }
    int descender() const { return descender_; }
    int lineGap() const { return lineGap_; }
    int advance(uint32_t glyph) const;

    // The glyph's closed contours in FONT units (y up), curves flattened.
    bool outline(uint32_t glyph, float flatness, std::vector<std::vector<Point>>* contours) const;

private:
    uint16_t u16(size_t at) const;
    int16_t s16(size_t at) const { return static_cast<int16_t>(u16(at)); }
    uint32_t u32(size_t at) const;
    size_t glyphOffset(uint32_t glyph, size_t* length) const;
    bool appendOutline(uint32_t glyph, const float m[6], float flatness, int depth,
                       std::vector<std::vector<Point>>* contours) const;

    std::vector<uint8_t> data_;
    size_t cmap_ = 0;
    size_t loca_ = 0;
    size_t glyf_ = 0;
    size_t hmtx_ = 0;
    int unitsPerEm_ = 1000;
    int ascender_ = 800;
    int descender_ = -200;
    int lineGap_ = 0;
    int numGlyphs_ = 0;
    int numHMetrics_ = 0;
    int indexToLocFormat_ = 0;
    size_t cmapSubtable_ = 0;
    int cmapFormat_ = 0;
};

}  // namespace fsui
