// ForgeShape's own anti-aliased coverage rasteriser (`DESKTOP-R1`): what turns
// a TrueType glyph and an Android vector drawable into atlas pixels, with no
// third-party library. Signed-area accumulation (the technique a coverage
// rasteriser needs and no more): every closed contour adds its signed area to
// the cells it crosses, a running sum across each row gives the winding
// coverage, and its magnitude clamped to 1 is the pixel's ink.
#pragma once

#include <cstdint>
#include <vector>

namespace fsui {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

class CoverageRaster {
public:
    void reset(int width, int height);
    // One edge of a closed contour, in pixel coordinates (y down).
    void line(Point a, Point b);
    // A closed polygon: every edge, including the closing one.
    void polygon(const std::vector<Point>& loop);
    // Composites this raster's coverage, times `alpha`, OVER `dst`
    // (width x height bytes): dst = dst + c·(1 − dst).
    void resolveOver(uint8_t* dst, float alpha) const;

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<float> accumulation_;
};

// Appends the flattened points of a quadratic and a cubic Bezier (the start
// point is the caller's), in `segments` straight pieces.
void flattenQuadratic(Point p0, Point p1, Point p2, int segments, std::vector<Point>* out);
void flattenCubic(Point p0, Point p1, Point p2, Point p3, int segments, std::vector<Point>* out);

// Strokes a polyline into the raster as a union of quads and round joins and
// caps, every piece wound the same way so overlaps add rather than cancel.
void strokePolyline(const std::vector<Point>& points, bool closed, float width, bool roundCaps,
                    CoverageRaster* raster);

}  // namespace fsui
