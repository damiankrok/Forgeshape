// Build-time tool (`DESKTOP-R1`): writes ForgeShape.ico from the Android
// adaptive launcher icon's own two layers (ic_launcher_background and
// ic_launcher_foreground), so the Windows icon and the Android one are one
// drawing. Each size is the adaptive icon's visible 72-unit square, rounded,
// stored as a PNG entry (Windows Vista and later read those).
//
//   make_app_icon <drawable dir> <out.ico>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "png_writer.h"
#include "ui/vector_icon.h"

namespace {

bool load(const std::string& path, fsui::VectorIcon* icon) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::stringstream text;
    text << file.rdbuf();
    return fsui::parseVectorDrawable(text.str(), icon);
}

void put16(std::vector<uint8_t>* out, uint16_t v) {
    out->push_back(static_cast<uint8_t>(v));
    out->push_back(static_cast<uint8_t>(v >> 8));
}

void put32(std::vector<uint8_t>* out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out->push_back(static_cast<uint8_t>(v >> (8 * i)));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: make_app_icon <drawable dir> <out.ico>\n");
        return 2;
    }
    const std::string dir = argv[1];
    fsui::VectorIcon background, foreground;
    if (!load(dir + "/ic_launcher_background.xml", &background)
        || !load(dir + "/ic_launcher_foreground.xml", &foreground)) {
        std::fprintf(stderr, "make_app_icon: launcher layers not found in %s\n", dir.c_str());
        return 1;
    }
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    std::vector<std::vector<uint8_t>> pngs;
    for (int size : sizes) {
        std::vector<uint8_t> rgba(static_cast<size_t>(size) * size * 4, 0);
        // The visible square is the central 72 of the 108-unit viewport.
        const float scale = static_cast<float>(size) / 72.0f;
        const float offset = -18.0f * scale;
        fsui::compositeIconRgba(background, size, size, scale, offset, offset, &rgba);
        fsui::compositeIconRgba(foreground, size, size, scale, offset, offset, &rgba);
        // A rounded square, anti-aliased by distance.
        const float half = size * 0.5f;
        const float radius = size * 0.22f;
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                const float px = std::fabs(x + 0.5f - half) - (half - radius);
                const float py = std::fabs(y + 0.5f - half) - (half - radius);
                const float d = std::sqrt(std::max(px, 0.0f) * std::max(px, 0.0f) +
                                          std::max(py, 0.0f) * std::max(py, 0.0f)) +
                                std::min(std::max(px, py), 0.0f) - radius;
                const float a = std::min(1.0f, std::max(0.0f, 0.5f - d));
                uint8_t& alpha = rgba[(static_cast<size_t>(y) * size + x) * 4 + 3];
                alpha = static_cast<uint8_t>(std::lround(alpha * a));
            }
        }
        pngs.push_back(fsdesktop::encodePng(rgba.data(), size, size, 4));
    }
    std::vector<uint8_t> ico;
    put16(&ico, 0);
    put16(&ico, 1);
    put16(&ico, static_cast<uint16_t>(pngs.size()));
    uint32_t offset = 6 + 16 * static_cast<uint32_t>(pngs.size());
    for (size_t i = 0; i < pngs.size(); ++i) {
        const int size = sizes[i];
        ico.push_back(static_cast<uint8_t>(size >= 256 ? 0 : size));
        ico.push_back(static_cast<uint8_t>(size >= 256 ? 0 : size));
        ico.push_back(0);
        ico.push_back(0);
        put16(&ico, 1);
        put16(&ico, 32);
        put32(&ico, static_cast<uint32_t>(pngs[i].size()));
        put32(&ico, offset);
        offset += static_cast<uint32_t>(pngs[i].size());
    }
    for (const auto& png : pngs) ico.insert(ico.end(), png.begin(), png.end());
    std::FILE* out = std::fopen(argv[2], "wb");
    if (out == nullptr) return 1;
    std::fwrite(ico.data(), 1, ico.size(), out);
    std::fclose(out);
    // A preview PNG beside it, for review.
    const std::string preview = std::string(argv[2]) + ".png";
    std::FILE* p = std::fopen(preview.c_str(), "wb");
    if (p != nullptr) {
        std::fwrite(pngs.back().data(), 1, pngs.back().size(), p);
        std::fclose(p);
    }
    return 0;
}
