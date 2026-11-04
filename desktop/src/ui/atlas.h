// The desktop interface's one coverage atlas (`DESKTOP-R1`): every glyph at
// every size and every icon at every size it is drawn at, rasterised once on
// first use and packed in shelves. A full atlas is cleared and refilled -- a
// session's glyph set is small, and a clear costs one re-upload.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "forgeshape_ui_frame.h"
#include "truetype.h"
#include "vector_icon.h"

namespace fsui {

struct AtlasEntry {
    // Texel rectangle in the atlas.
    int x = 0, y = 0, width = 0, height = 0;
    // Where the bitmap sits relative to the pen (glyphs) or the icon box.
    float offsetX = 0.0f, offsetY = 0.0f;
    float advance = 0.0f;
    bool valid = false;
};

class Atlas {
public:
    Atlas();
    void setFont(std::shared_ptr<TrueTypeFont> regular, std::shared_ptr<TrueTypeFont> bold);
    void addIconSource(const std::string& name, const std::string& xml);

    // A glyph of `pixelSize` (the em size, in pixels). Never fails: a missing
    // glyph is a zero-size entry with an advance.
    const AtlasEntry& glyph(uint32_t codepoint, int pixelSize, bool bold);
    const AtlasEntry& icon(const std::string& name, int pixelSize);
    bool hasIcon(const std::string& name) const { return icons_.count(name) != 0; }

    float ascent(int pixelSize, bool bold) const;
    float descent(int pixelSize, bool bold) const;
    float lineHeight(int pixelSize, bool bold) const;
    const TrueTypeFont* font(bool bold) const;

    std::shared_ptr<const forgeshape::UiAtlasImage> image();
    int width() const { return kSize; }
    int height() const { return kSize; }

private:
    static constexpr int kSize = 2048;
    bool place(int w, int h, int* x, int* y);
    void clear();

    std::shared_ptr<TrueTypeFont> regular_;
    std::shared_ptr<TrueTypeFont> bold_;
    std::map<std::string, VectorIcon> icons_;
    std::map<std::tuple<uint32_t, int, bool>, AtlasEntry> glyphs_;
    std::map<std::pair<std::string, int>, AtlasEntry> iconEntries_;
    std::vector<uint8_t> pixels_;
    int shelfX_ = 1, shelfY_ = 1, shelfHeight_ = 0;
    uint64_t revision_ = 1;
    bool dirty_ = true;
    std::shared_ptr<forgeshape::UiAtlasImage> image_;
};

}  // namespace fsui
