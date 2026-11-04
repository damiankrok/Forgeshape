// One frame of the DESKTOP shell's own interface, as the renderer draws it
// (`DESKTOP-R1`). Plain data, platform-neutral: the desktop UI kit builds it,
// the renderer draws it after everything else in the main pass, and nothing in
// it is project truth -- no ObjectId, no geometry, no `.forge` byte.
//
// Every quad is drawn by ONE pipeline whose fragment stage chooses by `mode`:
//   0  textured: the atlas's coverage times `color` (text and icons),
//   1  a rounded rectangle filled with `color`, anti-aliased by its own
//      signed distance (`hw`, `hh` half size, `radius`),
//   2  the same rectangle's outline, `param` pixels wide,
//   3  a soft shadow of the rectangle, blurred over `param` pixels.
// so a panel, its border, its shadow, a label and an icon cost one bind.
//
// The Android build never makes one: its interface is Android views.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace forgeshape {

struct UiVertex {
    float x = 0.0f;  // pixels, top-left origin
    float y = 0.0f;
    float u = 0.0f;  // atlas texel coordinates normalised to [0, 1]
    float v = 0.0f;
    float localX = 0.0f;  // position relative to the rectangle's centre
    float localY = 0.0f;
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
    float radius = 0.0f;
    float mode = 0.0f;
    float param = 0.0f;
    uint32_t color = 0;  // R, G, B, A bytes in memory order
};
static_assert(sizeof(UiVertex) == 48, "UiVertex is a fixed 48-byte vertex");

struct UiDrawCommand {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int32_t clipX = 0;
    int32_t clipY = 0;
    uint32_t clipWidth = 0;
    uint32_t clipHeight = 0;
};

// The single-channel coverage atlas every glyph and icon is drawn from.
// Re-uploaded only when `revision` changes.
struct UiAtlasImage {
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t revision = 0;
    std::vector<uint8_t> pixels;
};

struct UiFrame {
    std::vector<UiVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<UiDrawCommand> commands;
    std::shared_ptr<const UiAtlasImage> atlas;
};

using UiFramePtr = std::shared_ptr<const UiFrame>;

}  // namespace forgeshape
