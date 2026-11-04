#include "atlas.h"

#include <algorithm>
#include <cmath>

namespace fsui {

Atlas::Atlas() { clear(); }

void Atlas::clear() {
    pixels_.assign(static_cast<size_t>(kSize) * kSize, 0);
    glyphs_.clear();
    iconEntries_.clear();
    shelfX_ = 1;
    shelfY_ = 1;
    shelfHeight_ = 0;
    ++revision_;
    dirty_ = true;
}

void Atlas::setFont(std::shared_ptr<TrueTypeFont> regular, std::shared_ptr<TrueTypeFont> bold) {
    regular_ = std::move(regular);
    bold_ = bold ? std::move(bold) : regular_;
    clear();
}

void Atlas::addIconSource(const std::string& name, const std::string& xml) {
    VectorIcon icon;
    if (parseVectorDrawable(xml, &icon)) {
        icons_[name] = std::move(icon);
    }
}

const TrueTypeFont* Atlas::font(bool bold) const {
    const TrueTypeFont* f = bold ? bold_.get() : regular_.get();
    return f != nullptr && f->valid() ? f : nullptr;
}

float Atlas::ascent(int pixelSize, bool bold) const {
    const TrueTypeFont* f = font(bold);
    return f ? f->ascender() * static_cast<float>(pixelSize) / f->unitsPerEm() : pixelSize * 0.8f;
}

float Atlas::descent(int pixelSize, bool bold) const {
    const TrueTypeFont* f = font(bold);
    return f ? -f->descender() * static_cast<float>(pixelSize) / f->unitsPerEm() : pixelSize * 0.2f;
}

float Atlas::lineHeight(int pixelSize, bool bold) const {
    return ascent(pixelSize, bold) + descent(pixelSize, bold);
}

bool Atlas::place(int w, int h, int* x, int* y) {
    if (w + 2 > kSize || h + 2 > kSize) return false;
    if (shelfX_ + w + 1 > kSize) {
        shelfX_ = 1;
        shelfY_ += shelfHeight_ + 1;
        shelfHeight_ = 0;
    }
    if (shelfY_ + h + 1 > kSize) return false;
    *x = shelfX_;
    *y = shelfY_;
    shelfX_ += w + 1;
    shelfHeight_ = std::max(shelfHeight_, h);
    return true;
}

const AtlasEntry& Atlas::glyph(uint32_t codepoint, int pixelSize, bool bold) {
    const auto key = std::make_tuple(codepoint, pixelSize, bold);
    auto found = glyphs_.find(key);
    if (found != glyphs_.end()) return found->second;
    AtlasEntry entry;
    const TrueTypeFont* f = font(bold);
    if (f == nullptr) {
        entry.advance = pixelSize * 0.55f;
        return glyphs_[key] = entry;
    }
    const float scale = static_cast<float>(pixelSize) / f->unitsPerEm();
    uint32_t index = f->glyphIndex(codepoint);
    if (index == 0 && codepoint != ' ') index = f->glyphIndex('?');
    entry.advance = f->advance(index) * scale;
    std::vector<std::vector<Point>> contours;
    f->outline(index, pixelSize > 20 ? 6.0f : 4.0f, &contours);
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& c : contours) {
        for (auto& p : c) {
            const float x = p.x * scale;
            const float y = -p.y * scale;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (contours.empty() || maxX <= minX || maxY <= minY) {
        return glyphs_[key] = entry;
    }
    const int ox = static_cast<int>(std::floor(minX)) - 1;
    const int oy = static_cast<int>(std::floor(minY)) - 1;
    const int w = static_cast<int>(std::ceil(maxX)) - ox + 2;
    const int h = static_cast<int>(std::ceil(maxY)) - oy + 2;
    int ax = 0, ay = 0;
    if (!place(w, h, &ax, &ay)) {
        clear();
        if (!place(w, h, &ax, &ay)) return glyphs_[key] = entry;
    }
    CoverageRaster raster;
    raster.reset(w, h);
    for (auto& c : contours) {
        std::vector<Point> t;
        t.reserve(c.size());
        for (auto& p : c) t.push_back({p.x * scale - ox, -p.y * scale - oy});
        raster.polygon(t);
    }
    std::vector<uint8_t> bitmap(static_cast<size_t>(w) * h, 0);
    raster.resolveOver(bitmap.data(), 1.0f);
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            // A gentle gamma lift: thin strokes read as solid on a dark ground.
            const float c = bitmap[yy * w + xx] / 255.0f;
            pixels_[static_cast<size_t>(ay + yy) * kSize + ax + xx] =
                static_cast<uint8_t>(std::lround(std::pow(c, 0.8f) * 255.0f));
        }
    }
    entry.x = ax;
    entry.y = ay;
    entry.width = w;
    entry.height = h;
    entry.offsetX = static_cast<float>(ox);
    entry.offsetY = static_cast<float>(oy);
    entry.valid = true;
    dirty_ = true;
    return glyphs_[key] = entry;
}

const AtlasEntry& Atlas::icon(const std::string& name, int pixelSize) {
    const auto key = std::make_pair(name, pixelSize);
    auto found = iconEntries_.find(key);
    if (found != iconEntries_.end()) return found->second;
    AtlasEntry entry;
    auto source = icons_.find(name);
    if (source == icons_.end() || pixelSize <= 0) return iconEntries_[key] = entry;
    std::vector<uint8_t> bitmap;
    rasterizeIcon(source->second, pixelSize, &bitmap);
    int ax = 0, ay = 0;
    if (!place(pixelSize, pixelSize, &ax, &ay)) {
        clear();
        if (!place(pixelSize, pixelSize, &ax, &ay)) return iconEntries_[key] = entry;
    }
    for (int yy = 0; yy < pixelSize; ++yy) {
        std::copy(bitmap.begin() + yy * pixelSize, bitmap.begin() + (yy + 1) * pixelSize,
                  pixels_.begin() + static_cast<size_t>(ay + yy) * kSize + ax);
    }
    entry.x = ax;
    entry.y = ay;
    entry.width = pixelSize;
    entry.height = pixelSize;
    entry.valid = true;
    dirty_ = true;
    return iconEntries_[key] = entry;
}

std::shared_ptr<const forgeshape::UiAtlasImage> Atlas::image() {
    if (dirty_ || !image_) {
        auto next = std::make_shared<forgeshape::UiAtlasImage>();
        next->width = kSize;
        next->height = kSize;
        next->revision = ++revision_;
        next->pixels = pixels_;
        image_ = next;
        dirty_ = false;
    }
    return image_;
}

}  // namespace fsui
