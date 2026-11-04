#include "raster.h"

#include <algorithm>
#include <cmath>

namespace fsui {

void CoverageRaster::reset(int width, int height) {
    width_ = width > 0 ? width : 0;
    height_ = height > 0 ? height : 0;
    // One spare cell per row end and a few at the end: an edge touching the
    // right border writes its carry into the next cell.
    accumulation_.assign(static_cast<size_t>(width_) * height_ + 4, 0.0f);
}

void CoverageRaster::line(Point p0, Point p1) {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }
    const float maxX = static_cast<float>(width_ - 1);
    p0.x = std::min(std::max(p0.x, 0.0f), maxX);
    p1.x = std::min(std::max(p1.x, 0.0f), maxX);
    if (std::fabs(p0.y - p1.y) <= 1e-6f) {
        return;
    }
    float dir = 1.0f;
    if (p0.y > p1.y) {
        std::swap(p0, p1);
        dir = -1.0f;
    }
    const float dxdy = (p1.x - p0.x) / (p1.y - p0.y);
    float x = p0.x;
    if (p0.y < 0.0f) {
        x -= p0.y * dxdy;
    }
    const int yStart = std::max(0, static_cast<int>(p0.y));
    const int yEnd = std::min(height_, static_cast<int>(std::ceil(p1.y)));
    for (int y = yStart; y < yEnd; ++y) {
        const size_t row = static_cast<size_t>(y) * width_;
        const float dy = std::min(static_cast<float>(y + 1), p1.y) - std::max(static_cast<float>(y), p0.y);
        const float xNext = x + dxdy * dy;
        const float d = dy * dir;
        const float x0 = std::min(x, xNext);
        const float x1 = std::max(x, xNext);
        const float x0Floor = std::floor(x0);
        const int x0i = static_cast<int>(x0Floor);
        const float x1Ceil = std::ceil(x1);
        const int x1i = static_cast<int>(x1Ceil);
        if (x1i <= x0i + 1) {
            const float xmf = 0.5f * (x + xNext) - x0Floor;
            accumulation_[row + x0i] += d - d * xmf;
            accumulation_[row + x0i + 1] += d * xmf;
        } else {
            const float s = 1.0f / (x1 - x0);
            const float x0f = x0 - x0Floor;
            const float a0 = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
            const float x1f = x1 - x1Ceil + 1.0f;
            const float am = 0.5f * s * x1f * x1f;
            accumulation_[row + x0i] += d * a0;
            if (x1i == x0i + 2) {
                accumulation_[row + x0i + 1] += d * (1.0f - a0 - am);
            } else {
                const float a1 = s * (1.5f - x0f);
                accumulation_[row + x0i + 1] += d * (a1 - a0);
                for (int xi = x0i + 2; xi < x1i - 1; ++xi) {
                    accumulation_[row + xi] += d * s;
                }
                const float a2 = a1 + static_cast<float>(x1i - x0i - 3) * s;
                accumulation_[row + x1i - 1] += d * (1.0f - a2 - am);
            }
            accumulation_[row + x1i] += d * am;
        }
        x = xNext;
    }
}

void CoverageRaster::polygon(const std::vector<Point>& loop) {
    const size_t n = loop.size();
    if (n < 3) {
        return;
    }
    for (size_t i = 0; i < n; ++i) {
        line(loop[i], loop[(i + 1) % n]);
    }
}

void CoverageRaster::resolveOver(uint8_t* dst, float alpha) const {
    float sum = 0.0f;
    const size_t count = static_cast<size_t>(width_) * height_;
    for (size_t i = 0; i < count; ++i) {
        sum += accumulation_[i];
        const float c = std::min(1.0f, std::fabs(sum)) * alpha;
        if (c <= 0.0f) {
            continue;
        }
        const float existing = dst[i] / 255.0f;
        const float out = existing + c * (1.0f - existing);
        dst[i] = static_cast<uint8_t>(std::lround(std::min(1.0f, out) * 255.0f));
    }
}

void flattenQuadratic(Point p0, Point p1, Point p2, int segments, std::vector<Point>* out) {
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / segments;
        const float u = 1.0f - t;
        out->push_back({u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x,
                        u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y});
    }
}

void flattenCubic(Point p0, Point p1, Point p2, Point p3, int segments, std::vector<Point>* out) {
    for (int i = 1; i <= segments; ++i) {
        const float t = static_cast<float>(i) / segments;
        const float u = 1.0f - t;
        const float a = u * u * u;
        const float b = 3 * u * u * t;
        const float c = 3 * u * t * t;
        const float d = t * t * t;
        out->push_back({a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                        a * p0.y + b * p1.y + c * p2.y + d * p3.y});
    }
}

namespace {

float signedArea(const std::vector<Point>& loop) {
    float area = 0.0f;
    for (size_t i = 0; i < loop.size(); ++i) {
        const Point& a = loop[i];
        const Point& b = loop[(i + 1) % loop.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return 0.5f * area;
}

// Every stroke piece is added with POSITIVE area, so where two pieces overlap
// their coverage adds and clamps instead of cancelling.
void addPositive(std::vector<Point> loop, CoverageRaster* raster) {
    if (signedArea(loop) < 0.0f) {
        std::reverse(loop.begin(), loop.end());
    }
    raster->polygon(loop);
}

void addDisc(Point c, float r, CoverageRaster* raster) {
    const int segments = std::max(8, static_cast<int>(std::ceil(r * 3.0f)));
    std::vector<Point> loop;
    loop.reserve(segments);
    for (int i = 0; i < segments; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / segments;
        loop.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
    addPositive(loop, raster);
}

}  // namespace

void strokePolyline(const std::vector<Point>& points, bool closed, float width, bool roundCaps,
                    CoverageRaster* raster) {
    if (points.size() < 2 || width <= 0.0f) {
        return;
    }
    const float h = 0.5f * width;
    const size_t n = points.size();
    const size_t segments = closed ? n : n - 1;
    for (size_t i = 0; i < segments; ++i) {
        const Point a = points[i];
        const Point b = points[(i + 1) % n];
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) {
            continue;
        }
        const float nx = -dy / len * h;
        const float ny = dx / len * h;
        addPositive({{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny},
                     {a.x - nx, a.y - ny}},
                    raster);
    }
    // Round joins at every interior vertex, and round caps at an open
    // polyline's ends when asked for (Android's strokeLineCap="round").
    for (size_t i = 0; i < n; ++i) {
        const bool end = !closed && (i == 0 || i == n - 1);
        if (end && !roundCaps) {
            continue;
        }
        addDisc(points[i], h, raster);
    }
}

}  // namespace fsui
