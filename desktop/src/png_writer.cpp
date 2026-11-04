#include "png_writer.h"

#include <cstdio>

namespace fsdesktop {

namespace {

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        built = true;
    }
    for (size_t i = 0; i < size; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

void put32(std::vector<uint8_t>* out, uint32_t v) {
    out->push_back(static_cast<uint8_t>(v >> 24));
    out->push_back(static_cast<uint8_t>(v >> 16));
    out->push_back(static_cast<uint8_t>(v >> 8));
    out->push_back(static_cast<uint8_t>(v));
}

void chunk(std::vector<uint8_t>* out, const char type[4], const std::vector<uint8_t>& body) {
    put32(out, static_cast<uint32_t>(body.size()));
    std::vector<uint8_t> typed(type, type + 4);
    typed.insert(typed.end(), body.begin(), body.end());
    out->insert(out->end(), typed.begin(), typed.end());
    put32(out, crc32(typed.data(), typed.size()) ^ 0xFFFFFFFFu);
}

}  // namespace

std::vector<uint8_t> encodePng(const uint8_t* pixels, uint32_t width, uint32_t height,
                               int channels) {
    std::vector<uint8_t> raw;
    const size_t stride = static_cast<size_t>(width) * channels;
    raw.reserve((stride + 1) * height);
    for (uint32_t y = 0; y < height; ++y) {
        raw.push_back(0);  // filter: none
        raw.insert(raw.end(), pixels + y * stride, pixels + (y + 1) * stride);
    }
    std::vector<uint8_t> z{0x78, 0x01};
    size_t at = 0;
    while (at < raw.size() || raw.empty()) {
        const size_t n = std::min<size_t>(65535, raw.size() - at);
        const bool last = at + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<uint8_t>(n));
        z.push_back(static_cast<uint8_t>(n >> 8));
        z.push_back(static_cast<uint8_t>(~n));
        z.push_back(static_cast<uint8_t>((~n) >> 8));
        z.insert(z.end(), raw.begin() + at, raw.begin() + at + n);
        at += n;
        if (raw.empty()) break;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    put32(&z, (b << 16) | a);

    std::vector<uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> header;
    put32(&header, width);
    put32(&header, height);
    header.push_back(8);
    header.push_back(channels == 4 ? 6 : 0);
    header.push_back(0);
    header.push_back(0);
    header.push_back(0);
    chunk(&png, "IHDR", header);
    chunk(&png, "IDAT", z);
    chunk(&png, "IEND", {});
    return png;
}

bool writePng(const std::string& path, const uint8_t* pixels, uint32_t width, uint32_t height,
              int channels) {
    const std::vector<uint8_t> png = encodePng(pixels, width, height, channels);
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
    std::fclose(file);
    return ok;
}

}  // namespace fsdesktop
