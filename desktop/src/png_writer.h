// A minimal PNG writer for the desktop shell (`DESKTOP-R1`): "Save view as
// image" and the headless check's evidence frame. Uncompressed (stored)
// deflate blocks, so it needs no compression library; a PNG reader accepts it
// exactly like any other.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fsdesktop {

// `channels` is 1 (grey) or 4 (RGBA); rows are top first, tightly packed.
std::vector<uint8_t> encodePng(const uint8_t* pixels, uint32_t width, uint32_t height,
                               int channels);
bool writePng(const std::string& path, const uint8_t* pixels, uint32_t width, uint32_t height,
              int channels);

}  // namespace fsdesktop
