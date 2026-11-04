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
    // Where the control with `id` stood in the frame last built, so an
    // automated check finds a control by its semantic id, never by a
    // coordinate. False when it was not drawn.
    bool widgetRect(const std::string& id, Rect* out) const;

    // --- drawing primitives ---
    void rect(const Rect& r, Color c, float radiusPx);
    void border(const Rect& r, Color c, float radiusPx, float widthPx);
    void shadow(const Rect& r, Color c, float radiusPx, float blurPx);
    void icon(const std::string& name, const Rect& box, Color c);
    // An anti-aliased segment `widthPx` wide with round ends, and a disc.
    void line(float x0, float y0, float x1, float y1, float widthPx, Color c);
    void circle(float cx, float cy, float radiusPx, Color c);
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
    // A panel DOCKED to a window edge: square, unshadowed, opaque, with one
    // hairline on the side it meets the model (0 none, 1 right, 2 left, 3 top).
    void dockedPanel(const Rect& r, int hairlineSide);
    void separator(float x, float y, float w);
    bool button(const std::string& id, const Rect& r, const std::string& text,
                const std::string& iconName = std::string(), bool primary = false,
                bool enabled = true);
    bool iconButton(const std::string& id, const Rect& r, const std::string& iconName,
                    const std::string& tooltip, bool active = false, bool enabled = true);
    // A ribbon command: its icon over its caption, the caption ALWAYS drawn
    // (a ribbon says what each command is, as SolidCraft's and Shapr3D's do).
    // `primary` is the screen's ONE commit (Finish, Extrude): the accent fill.
    bool ribbonButton(const std::string& id, const Rect& r, const std::string& iconName,
                      const std::string& caption, bool active = false, bool enabled = true,
                      const std::string& tip = std::string(), bool primary = false);
    // A ribbon tab: its label, underlined when chosen.
    bool tab(const std::string& id, const Rect& r, const std::string& text, bool active);
    // A History step: an icon and a label in a bordered chip. Clickable when
    // `enabled`; a step being edited takes the selected treatment.
    bool stepChip(const std::string& id, const Rect& r, const std::string& iconName,
                  const std::string& text, bool editing, bool enabled, Color ink, Color border,
                  const std::string& tip = std::string());
    // A start tile on Home: a large target with its icon, a title and one
    // secondary line, as the start pages of desktop CAD tools carry.
    bool tile(const std::string& id, const Rect& r, const std::string& iconName,
              const std::string& title, const std::string& line, bool primary = false);
    bool listRow(const std::string& id, const Rect& r, const std::string& text,
                 const std::string& iconName, bool selected, const std::string& detail = "");
    bool toggle(const std::string& id, const Rect& r, const std::string& text, bool* value);
    // A number field typed into; returns true on the frame its value is
    // committed (Enter, Tab or a click elsewhere) AND the text differs from
    // what it showed at focus, with the parsed number -- so focusing a field
    // and leaving it writes nothing. The value is drawn at twelve significant
    // digits, the same noise-free rule as Android's fields (`decimals` is
    // kept for call sites and ignored).
    bool numberField(const std::string& id, const Rect& r, double* value, int decimals,
                     const std::string& unit);
    // A value as a field draws it: <= 12 significant digits, no exponent,
    // trailing zeros stripped, -0 as 0.
    static std::string formatFieldNumber(double value);
    bool textField(const std::string& id, const Rect& r, std::string* value);
    void tooltip(const std::string& text);

private:
    bool hot(const std::string& id, const Rect& r);
    bool clicked(const std::string& id, const Rect& r, bool enabled);
    void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
              float mode, float cx, float cy, float hw, float hh, float radius, float param,
              Color c);
    void flushCommand();
    // The one selected treatment: the selected-at-rest fill and a textPrimary
    // indicator bar, under a horizontal item or on a vertical one's leading edge.
    void selectedMark(const Rect& r, bool leadingBar);
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
    std::vector<std::pair<std::string, Rect>> widgetRects_;
    std::vector<Rect> previousBlockers_;
    std::string pressedId_;
    std::string hotId_;
    std::string editId_;
    std::string editBuffer_;
    size_t editCursor_ = 0;
    bool editCommitPending_ = false;
    bool editSelectAll_ = false;    // the first keystroke replaces the text
    std::string editInitial_;       // the text shown when the field took focus
    std::string tooltipText_;
    float tooltipX_ = 0, tooltipY_ = 0;
};

}  // namespace fsui
