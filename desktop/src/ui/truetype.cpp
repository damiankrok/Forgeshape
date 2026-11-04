#include "truetype.h"

#include <cmath>
#include <cstdio>

namespace fsui {

bool TrueTypeFont::loadFile(const std::string& path) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    std::vector<uint8_t> bytes;
    uint8_t buffer[65536];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + read);
    }
    std::fclose(file);
    return load(std::move(bytes));
}

uint16_t TrueTypeFont::u16(size_t at) const {
    if (at + 2 > data_.size()) return 0;
    return static_cast<uint16_t>((data_[at] << 8) | data_[at + 1]);
}

uint32_t TrueTypeFont::u32(size_t at) const {
    if (at + 4 > data_.size()) return 0;
    return (static_cast<uint32_t>(data_[at]) << 24) | (static_cast<uint32_t>(data_[at + 1]) << 16)
           | (static_cast<uint32_t>(data_[at + 2]) << 8) | data_[at + 3];
}

bool TrueTypeFont::load(std::vector<uint8_t> bytes) {
    data_ = std::move(bytes);
    glyf_ = 0;
    if (data_.size() < 12) return false;
    const uint32_t version = u32(0);
    if (version != 0x00010000u && version != 0x74727565u /* 'true' */) {
        return false;  // a CFF (.otf) outline is not read here
    }
    const int tables = u16(4);
    size_t head = 0, hhea = 0, maxp = 0;
    for (int i = 0; i < tables; ++i) {
        const size_t record = 12 + static_cast<size_t>(i) * 16;
        const uint32_t tag = u32(record);
        const size_t offset = u32(record + 8);
        switch (tag) {
            case 0x636D6170: cmap_ = offset; break;  // cmap
            case 0x6C6F6361: loca_ = offset; break;  // loca
            case 0x676C7966: glyf_ = offset; break;  // glyf
            case 0x686D7478: hmtx_ = offset; break;  // hmtx
            case 0x68656164: head = offset; break;   // head
            case 0x68686561: hhea = offset; break;   // hhea
            case 0x6D617870: maxp = offset; break;   // maxp
            default: break;
        }
    }
    if (!cmap_ || !loca_ || !glyf_ || !hmtx_ || !head || !hhea || !maxp) {
        glyf_ = 0;
        return false;
    }
    unitsPerEm_ = u16(head + 18);
    indexToLocFormat_ = s16(head + 50);
    ascender_ = s16(hhea + 4);
    descender_ = s16(hhea + 6);
    lineGap_ = s16(hhea + 8);
    numHMetrics_ = u16(hhea + 34);
    numGlyphs_ = u16(maxp + 4);
    // The best Unicode subtable: format 12 (full range) over format 4 (BMP).
    const int subtables = u16(cmap_ + 2);
    for (int i = 0; i < subtables; ++i) {
        const size_t record = cmap_ + 4 + static_cast<size_t>(i) * 8;
        const int platform = u16(record);
        const int encoding = u16(record + 2);
        const size_t offset = cmap_ + u32(record + 4);
        const int format = u16(offset);
        const bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
        if (!unicode) continue;
        if (format == 12 || (format == 4 && cmapFormat_ != 12)) {
            cmapSubtable_ = offset;
            cmapFormat_ = format;
        }
    }
    if (unitsPerEm_ <= 0 || cmapFormat_ == 0) {
        glyf_ = 0;
        return false;
    }
    return true;
}

uint32_t TrueTypeFont::glyphIndex(uint32_t cp) const {
    if (cmapFormat_ == 12) {
        const uint32_t groups = u32(cmapSubtable_ + 12);
        for (uint32_t i = 0; i < groups; ++i) {
            const size_t g = cmapSubtable_ + 16 + static_cast<size_t>(i) * 12;
            const uint32_t start = u32(g);
            const uint32_t end = u32(g + 4);
            if (cp >= start && cp <= end) return u32(g + 8) + (cp - start);
        }
        return 0;
    }
    if (cmapFormat_ == 4 && cp <= 0xFFFF) {
        const int segX2 = u16(cmapSubtable_ + 6);
        const size_t ends = cmapSubtable_ + 14;
        const size_t starts = ends + segX2 + 2;
        const size_t deltas = starts + segX2;
        const size_t ranges = deltas + segX2;
        for (int s = 0; s < segX2 / 2; ++s) {
            const uint32_t end = u16(ends + s * 2);
            if (cp > end) continue;
            const uint32_t start = u16(starts + s * 2);
            if (cp < start) return 0;
            const int delta = s16(deltas + s * 2);
            const int rangeOffset = u16(ranges + s * 2);
            if (rangeOffset == 0) return static_cast<uint16_t>(cp + delta);
            const size_t at = ranges + s * 2 + rangeOffset + (cp - start) * 2;
            const uint32_t glyph = u16(at);
            return glyph == 0 ? 0 : static_cast<uint16_t>(glyph + delta);
        }
    }
    return 0;
}

int TrueTypeFont::advance(uint32_t glyph) const {
    if (numHMetrics_ <= 0) return unitsPerEm_ / 2;
    const uint32_t index = glyph < static_cast<uint32_t>(numHMetrics_) ? glyph : numHMetrics_ - 1;
    return u16(hmtx_ + index * 4);
}

size_t TrueTypeFont::glyphOffset(uint32_t glyph, size_t* length) const {
    if (glyph >= static_cast<uint32_t>(numGlyphs_)) {
        *length = 0;
        return 0;
    }
    size_t a, b;
    if (indexToLocFormat_ == 0) {
        a = static_cast<size_t>(u16(loca_ + glyph * 2)) * 2;
        b = static_cast<size_t>(u16(loca_ + glyph * 2 + 2)) * 2;
    } else {
        a = u32(loca_ + glyph * 4);
        b = u32(loca_ + glyph * 4 + 4);
    }
    *length = b > a ? b - a : 0;
    return glyf_ + a;
}

bool TrueTypeFont::outline(uint32_t glyph, float flatness,
                           std::vector<std::vector<Point>>* contours) const {
    contours->clear();
    const float identity[6] = {1, 0, 0, 1, 0, 0};
    return appendOutline(glyph, identity, flatness, 0, contours);
}

bool TrueTypeFont::appendOutline(uint32_t glyph, const float m[6], float flatness, int depth,
                                 std::vector<std::vector<Point>>* contours) const {
    if (depth > 8) return false;
    size_t length = 0;
    const size_t at = glyphOffset(glyph, &length);
    if (length == 0) return true;  // a space
    const int contourCount = s16(at);
    const auto map = [m](float x, float y) {
        return Point{m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]};
    };
    if (contourCount < 0) {
        // Composite: each component is another glyph under a 2x2 and an offset.
        size_t p = at + 10;
        for (;;) {
            const uint16_t flags = u16(p);
            const uint16_t component = u16(p + 2);
            p += 4;
            float dx = 0, dy = 0;
            if (flags & 1) {
                dx = s16(p);
                dy = s16(p + 2);
                p += 4;
            } else {
                dx = static_cast<int8_t>(data_[p]);
                dy = static_cast<int8_t>(data_[p + 1]);
                p += 2;
            }
            float a = 1, b = 0, c = 0, d = 1;
            if (flags & 0x8) {
                a = d = s16(p) / 16384.0f;
                p += 2;
            } else if (flags & 0x40) {
                a = s16(p) / 16384.0f;
                d = s16(p + 2) / 16384.0f;
                p += 4;
            } else if (flags & 0x80) {
                a = s16(p) / 16384.0f;
                b = s16(p + 2) / 16384.0f;
                c = s16(p + 4) / 16384.0f;
                d = s16(p + 6) / 16384.0f;
                p += 8;
            }
            if (!(flags & 2)) {
                dx = dy = 0;  // point-matched components are not placed here
            }
            // child = m · [a c dx; b d dy]
            const float child[6] = {m[0] * a + m[2] * b,      m[1] * a + m[3] * b,
                                    m[0] * c + m[2] * d,      m[1] * c + m[3] * d,
                                    m[0] * dx + m[2] * dy + m[4], m[1] * dx + m[3] * dy + m[5]};
            appendOutline(component, child, flatness, depth + 1, contours);
            if (!(flags & 0x20)) break;
        }
        return true;
    }
    // Simple glyph.
    std::vector<int> endPoints(contourCount);
    for (int i = 0; i < contourCount; ++i) endPoints[i] = u16(at + 10 + i * 2);
    const int pointCount = contourCount > 0 ? endPoints.back() + 1 : 0;
    size_t p = at + 10 + contourCount * 2;
    p += 2 + u16(p);  // instructions
    std::vector<uint8_t> flags(pointCount);
    for (int i = 0; i < pointCount;) {
        const uint8_t f = data_[p++];
        flags[i++] = f;
        if (f & 8) {
            int repeat = data_[p++];
            while (repeat-- > 0 && i < pointCount) flags[i++] = f;
        }
    }
    std::vector<float> xs(pointCount), ys(pointCount);
    int v = 0;
    for (int i = 0; i < pointCount; ++i) {
        if (flags[i] & 2) {
            v += (flags[i] & 16) ? data_[p] : -data_[p];
            p += 1;
        } else if (!(flags[i] & 16)) {
            v += s16(p);
            p += 2;
        }
        xs[i] = static_cast<float>(v);
    }
    v = 0;
    for (int i = 0; i < pointCount; ++i) {
        if (flags[i] & 4) {
            v += (flags[i] & 32) ? data_[p] : -data_[p];
            p += 1;
        } else if (!(flags[i] & 32)) {
            v += s16(p);
            p += 2;
        }
        ys[i] = static_cast<float>(v);
    }
    int start = 0;
    for (int c = 0; c < contourCount; ++c) {
        const int end = endPoints[c];
        const int n = end - start + 1;
        if (n < 2) {
            start = end + 1;
            continue;
        }
        // Rotate so the walk begins on an on-curve point (or a synthetic
        // midpoint when every point is off-curve).
        std::vector<Point> pts(n);
        std::vector<bool> on(n);
        for (int i = 0; i < n; ++i) {
            pts[i] = {xs[start + i], ys[start + i]};
            on[i] = (flags[start + i] & 1) != 0;
        }
        int first = -1;
        for (int i = 0; i < n; ++i) if (on[i]) { first = i; break; }
        Point startPoint;
        if (first < 0) {
            startPoint = {(pts[0].x + pts[n - 1].x) * 0.5f, (pts[0].y + pts[n - 1].y) * 0.5f};
            first = 0;
        } else {
            startPoint = pts[first];
        }
        std::vector<Point> loop;
        loop.push_back(map(startPoint.x, startPoint.y));
        Point current = startPoint;
        const int steps = std::max(2, static_cast<int>(flatness));
        for (int k = 1; k <= n; ++k) {
            const int i = (first + k) % n;
            if (on[i]) {
                loop.push_back(map(pts[i].x, pts[i].y));
                current = pts[i];
                continue;
            }
            const int j = (i + 1) % n;
            Point next = on[j] ? pts[j]
                               : Point{(pts[i].x + pts[j].x) * 0.5f, (pts[i].y + pts[j].y) * 0.5f};
            std::vector<Point> flat;
            flattenQuadratic(map(current.x, current.y), map(pts[i].x, pts[i].y),
                             map(next.x, next.y), steps, &flat);
            loop.insert(loop.end(), flat.begin(), flat.end());
            current = next;
            if (on[j]) {
                ++k;  // the next on-curve point is this curve's end
            }
        }
        contours->push_back(std::move(loop));
        start = end + 1;
    }
    return true;
}

}  // namespace fsui
