// ForgeShape's own desktop interface kit (`DESKTOP-R1`): an immediate-mode
// widget set drawn through ONE renderer pipeline (forgeshape_ui_frame.h), no
// third-party UI library. A frame is built from scratch every time it is
// drawn; what persists between frames is only which control is hot, pressed or
// being typed into -- presentation, never project truth.
//
// Every colour and size is a THEME token (theme.h), the same roles the Android
// attrs.xml names, and a user theme file overrides any of them, so the whole
// look can be restyled without a rebuild.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "atlas.h"
#include "forgeshape_ui_frame.h"
#include "theme.h"

namespace fsui {

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};

enum class Key { None, Enter, Escape, Backspace, Delete, Tab, Left, Right, Home, End };

struct InputState {
    float mouseX = -1, mouseY = -1;
    bool mouseDown[3] = {false, false, false};
    bool mousePressed[3] = {false, false, false};
    bool mouseReleased[3] = {false, false, false};
    float wheel = 0;
    std::u32string text;      // typed characters this frame
    std::vector<Key> keys;    // navigation keys pressed this frame
    bool ctrl = false;
    bool shift = false;
};

class Ui {
public:
    Ui(Atlas& atlas, Theme& theme) : atlas_(atlas), theme_(theme) {}

    void begin(int width, int height, float scale, const InputState& input);
    forgeshape::UiFramePtr end();

    float scale() const { return scale_; }
    float px(float dp) const { return dp * scale_; }
    int fontPx(float dp) const;
    const Theme& theme() const { return theme_; }
    const InputState& input() const { return input_; }

    // Whether the pointer is over any surface drawn this frame (blocking), or
    // a control owns the pointer -- the viewport then gets no mouse input.
    bool pointerCaptured() const;
    // Called by a surface that should swallow the pointer (panels, menus).
    void block(const Rect& r);
    bool typing() const { return !editId_.empty(); }

    // --- drawing primitives ---
    void rect(const Rect& r, Color c, float radiusPx);
    void border(const Rect& r, Color c, float radiusPx, float widthPx);
    void shadow(const Rect& r, Color c, float radiusPx, float blurPx);
    void icon(const std::string& name, const Rect& box, Color c);
    // Draws UTF-8 text with its baseline at `baseline`; returns the advance.
    float text(float x, float baseline, const std::string& utf8, Color c, float sizeDp,
               bool bold = false);
    float textWidth(const std::string& utf8, float sizeDp, bool bold = false);
    // Text vertically centred in `r`, left aligned (or centred), clipped with an
    // ellipsis to the rectangle's width.
    void label(const Rect& r, const std::string& utf8, Color c, float sizeDp, bool bold = false,
               bool centred = false);
    void pushClip(const Rect& r);
    void popClip();

    // --- surfaces and widgets ---
    void panel(const Rect& r);
    void separator(float x, float y, float w);
    bool button(const std::string& id, const Rect& r, const std::string& text,
                const std::string& iconName = std::string(), bool primary = false,
                bool enabled = true);
    bool iconButton(const std::string& id, const Rect& r, const std::string& iconName,
                    const std::string& tooltip, bool active = false, bool enabled = true);
    bool listRow(const std::string& id, const Rect& r, const std::string& text,
                 const std::string& iconName, bool selected, const std::string& detail = "");
    bool toggle(const std::string& id, const Rect& r, const std::string& text, bool* value);
    // A number field typed into; returns true on the frame its value is
    // committed (Enter, Tab or a click elsewhere), with the parsed number.
    bool numberField(const std::string& id, const Rect& r, double* value, int decimals,
                     const std::string& unit);
    bool textField(const std::string& id, const Rect& r, std::string* value);
    void tooltip(const std::string& text);

private:
    bool hot(const std::string& id, const Rect& r);
    bool clicked(const std::string& id, const Rect& r, bool enabled);
    void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
              float mode, float cx, float cy, float hw, float hh, float radius, float param,
              Color c);
    void flushCommand();
    bool editCommit(const std::string& id, std::string* committed);

    Atlas& atlas_;
    Theme& theme_;
    InputState input_;
    float scale_ = 1.0f;
    int width_ = 0, height_ = 0;
    std::shared_ptr<forgeshape::UiFrame> frame_;
    std::vector<Rect> clips_;
    uint32_t commandStart_ = 0;
    std::vector<Rect> blockers_;
    std::vector<Rect> previousBlockers_;
    std::string pressedId_;
    std::string hotId_;
    std::string editId_;
    std::string editBuffer_;
    size_t editCursor_ = 0;
    bool editCommitPending_ = false;
    std::string tooltipText_;
    float tooltipX_ = 0, tooltipY_ = 0;
};

}  // namespace fsui
