#include "ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace fsui {

namespace {

// UTF-8 to code points; malformed bytes become U+FFFD.
std::u32string decode(const std::string& s) {
    std::u32string out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = 0xFFFD;
        size_t n = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 6 && i + 1 < s.size()) {
            cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            n = 2;
        } else if ((c >> 4) == 14 && i + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            n = 3;
        } else if ((c >> 3) == 30 && i + 3 < s.size()) {
            cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6)
                 | (s[i + 3] & 0x3F);
            n = 4;
        }
        out.push_back(cp);
        i += n;
    }
    return out;
}

std::string encode(const std::u32string& s) {
    std::string out;
    for (char32_t cp : s) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

}  // namespace

int Ui::fontPx(float dp) const {
    return std::max(6, static_cast<int>(std::lround(dp * scale_)));
}

void Ui::begin(int width, int height, float scale, const InputState& input) {
    width_ = width;
    height_ = height;
    scale_ = scale > 0.25f ? scale : 1.0f;
    input_ = input;
    frame_ = std::make_shared<forgeshape::UiFrame>();
    clips_.clear();
    clips_.push_back({0, 0, static_cast<float>(width), static_cast<float>(height)});
    commandStart_ = 0;
    previousBlockers_ = blockers_;
    blockers_.clear();
    widgetRects_.clear();
    hotId_.clear();
    tooltipText_.clear();
    if (input_.mouseReleased[0] && pressedId_ != "") {
        // A press that ended anywhere is over; `clicked` reads it below.
    }
}

forgeshape::UiFramePtr Ui::end() {
    if (!tooltipText_.empty()) {
        const float size = theme_.smallFontSize;
        const float w = textWidth(tooltipText_, size) + px(16);
        const float h = px(26);
        float x = std::min(tooltipX_, width_ - w - px(4));
        float y = tooltipY_ + px(22);
        if (y + h > height_) y = tooltipY_ - h - px(8);
        const Rect r{x, y, w, h};
        shadow(r, theme_.shadow, px(6), px(10));
        rect(r, theme_.chromeSurface, px(6));
        border(r, theme_.chromeHairline, px(6), 1.0f);
        label({x + px(8), y, w - px(16), h}, tooltipText_, theme_.textPrimary, size);
    }
    flushCommand();
    if (input_.mouseReleased[0]) pressedId_.clear();
    frame_->atlas = atlas_.image();
    std::shared_ptr<const forgeshape::UiFrame> out = frame_;
    frame_.reset();
    return out;
}

bool Ui::pointerCaptured() const {
    if (!pressedId_.empty()) return true;
    for (const Rect& r : previousBlockers_) {
        if (r.contains(input_.mouseX, input_.mouseY)) return true;
    }
    for (const Rect& r : blockers_) {
        if (r.contains(input_.mouseX, input_.mouseY)) return true;
    }
    return false;
}

void Ui::block(const Rect& r) { blockers_.push_back(r); }

void Ui::flushCommand() {
    const uint32_t count = static_cast<uint32_t>(frame_->indices.size()) - commandStart_;
    if (count == 0) return;
    const Rect& clip = clips_.back();
    forgeshape::UiDrawCommand cmd;
    cmd.firstIndex = commandStart_;
    cmd.indexCount = count;
    cmd.clipX = static_cast<int32_t>(std::floor(clip.x));
    cmd.clipY = static_cast<int32_t>(std::floor(clip.y));
    cmd.clipWidth = static_cast<uint32_t>(std::max(0.0f, std::ceil(clip.w)));
    cmd.clipHeight = static_cast<uint32_t>(std::max(0.0f, std::ceil(clip.h)));
    frame_->commands.push_back(cmd);
    commandStart_ = static_cast<uint32_t>(frame_->indices.size());
}

void Ui::pushClip(const Rect& r) {
    flushCommand();
    const Rect& p = clips_.back();
    const float x0 = std::max(p.x, r.x), y0 = std::max(p.y, r.y);
    const float x1 = std::min(p.right(), r.right()), y1 = std::min(p.bottom(), r.bottom());
    clips_.push_back({x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)});
}

void Ui::popClip() {
    flushCommand();
    if (clips_.size() > 1) clips_.pop_back();
}

void Ui::quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1,
              float mode, float cx, float cy, float hw, float hh, float radius, float param,
              Color c) {
    const uint32_t base = static_cast<uint32_t>(frame_->vertices.size());
    const uint32_t packed = c.packed();
    const float xs[2] = {x0, x1}, ys[2] = {y0, y1}, us[2] = {u0, u1}, vs[2] = {v0, v1};
    for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
            forgeshape::UiVertex v;
            v.x = xs[i];
            v.y = ys[j];
            v.u = us[i];
            v.v = vs[j];
            v.localX = xs[i] - cx;
            v.localY = ys[j] - cy;
            v.halfWidth = hw;
            v.halfHeight = hh;
            v.radius = radius;
            v.mode = mode;
            v.param = param;
            v.color = packed;
            frame_->vertices.push_back(v);
        }
    }
    for (uint32_t idx : {0u, 1u, 2u, 2u, 1u, 3u}) frame_->indices.push_back(base + idx);
}

void Ui::rect(const Rect& r, Color c, float radiusPx) {
    if (c.a == 0 || r.w <= 0 || r.h <= 0) return;
    const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
    const float rad = std::min(radiusPx, std::min(r.w, r.h) * 0.5f);
    quad(r.x - 1, r.y - 1, r.right() + 1, r.bottom() + 1, 0, 0, 0, 0, 1.0f, cx, cy, r.w * 0.5f,
         r.h * 0.5f, rad, 0, c);
}

void Ui::border(const Rect& r, Color c, float radiusPx, float widthPx) {
    if (c.a == 0 || r.w <= 0 || r.h <= 0) return;
    const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
    const float rad = std::min(radiusPx, std::min(r.w, r.h) * 0.5f);
    quad(r.x - 1, r.y - 1, r.right() + 1, r.bottom() + 1, 0, 0, 0, 0, 2.0f, cx, cy, r.w * 0.5f,
         r.h * 0.5f, rad, widthPx, c);
}

void Ui::shadow(const Rect& r, Color c, float radiusPx, float blurPx) {
    if (c.a == 0) return;
    const float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f + blurPx * 0.25f;
    quad(r.x - blurPx * 2, r.y - blurPx * 2, r.right() + blurPx * 2, r.bottom() + blurPx * 2, 0, 0,
         0, 0, 3.0f, cx, cy, r.w * 0.5f, r.h * 0.5f, radiusPx, blurPx, c);
}

void Ui::line(float x0, float y0, float x1, float y1, float widthPx, Color c) {
    if (c.a == 0) return;
    const float pad = widthPx * 0.5f + 1.5f;
    quad(std::min(x0, x1) - pad, std::min(y0, y1) - pad, std::max(x0, x1) + pad,
         std::max(y0, y1) + pad, 0, 0, 0, 0, 4.0f, x0, y0, x1 - x0, y1 - y0, widthPx * 0.5f, 0, c);
}

void Ui::circle(float cx, float cy, float radiusPx, Color c) {
    rect({cx - radiusPx, cy - radiusPx, radiusPx * 2.0f, radiusPx * 2.0f}, c, radiusPx);
}

void Ui::icon(const std::string& name, const Rect& box, Color c) {
    const int size = static_cast<int>(std::lround(std::min(box.w, box.h)));
    const AtlasEntry& e = atlas_.icon(name, size);
    if (!e.valid) return;
    const float x = std::round(box.x + (box.w - size) * 0.5f);
    const float y = std::round(box.y + (box.h - size) * 0.5f);
    const float inv = 1.0f / atlas_.width();
    quad(x, y, x + e.width, y + e.height, e.x * inv, e.y * inv, (e.x + e.width) * inv,
         (e.y + e.height) * inv, 0.0f, 0, 0, 0, 0, 0, 0, c);
}

float Ui::text(float x, float baseline, const std::string& utf8, Color c, float sizeDp, bool bold) {
    const int size = fontPx(sizeDp);
    const float inv = 1.0f / atlas_.width();
    float pen = std::round(x);
    const float base = std::round(baseline);
    for (char32_t cp : decode(utf8)) {
        const AtlasEntry& e = atlas_.glyph(cp, size, bold);
        if (e.valid) {
            const float gx = std::round(pen) + e.offsetX;
            const float gy = base + e.offsetY;
            quad(gx, gy, gx + e.width, gy + e.height, e.x * inv, e.y * inv, (e.x + e.width) * inv,
                 (e.y + e.height) * inv, 0.0f, 0, 0, 0, 0, 0, 0, c);
        }
        pen += e.advance;
    }
    return pen - x;
}

float Ui::textWidth(const std::string& utf8, float sizeDp, bool bold) {
    const int size = fontPx(sizeDp);
    float w = 0;
    for (char32_t cp : decode(utf8)) w += atlas_.glyph(cp, size, bold).advance;
    return w;
}

void Ui::label(const Rect& r, const std::string& utf8, Color c, float sizeDp, bool bold,
               bool centred) {
    const int size = fontPx(sizeDp);
    std::string shown = utf8;
    float w = textWidth(shown, sizeDp, bold);
    if (w > r.w) {
        // Shortened with an ellipsis rather than spilling out of its box.
        std::u32string cps = decode(utf8);
        const std::string dots = "\xE2\x80\xA6";
        while (!cps.empty()) {
            cps.pop_back();
            shown = encode(cps) + dots;
            w = textWidth(shown, sizeDp, bold);
            if (w <= r.w) break;
        }
    }
    const float ascent = atlas_.ascent(size, bold);
    const float descent = atlas_.descent(size, bold);
    const float baseline = r.y + (r.h + ascent - descent) * 0.5f;
    const float x = centred ? r.x + (r.w - w) * 0.5f : r.x;
    text(x, baseline, shown, c, sizeDp, bold);
}

void Ui::panel(const Rect& r) {
    const float radius = px(theme_.panelRadius);
    shadow(r, theme_.shadow, radius, px(theme_.shadowBlur));
    rect(r, theme_.surfaceFloating, radius);
    border(r, theme_.chromeHairline.alpha(0.8f), radius, 1.0f);
    block(r);
}

void Ui::dockedPanel(const Rect& r, int hairlineSide) {
    rect(r, theme_.chromeSurface, 0);
    switch (hairlineSide) {
        case 1: rect({r.right() - 1.0f, r.y, 1.0f, r.h}, theme_.chromeHairline, 0); break;
        case 2: rect({r.x, r.y, 1.0f, r.h}, theme_.chromeHairline, 0); break;
        case 3: rect({r.x, r.y, r.w, 1.0f}, theme_.chromeHairline, 0); break;
        default: break;
    }
    block(r);
}

void Ui::separator(float x, float y, float w) {
    rect({x, std::round(y), w, 1.0f}, theme_.chromeHairline, 0);
}

bool Ui::hot(const std::string& id, const Rect& r) {
    const Rect& clip = clips_.back();
    const bool over = r.contains(input_.mouseX, input_.mouseY)
                      && clip.contains(input_.mouseX, input_.mouseY);
    if (over && (pressedId_.empty() || pressedId_ == id)) {
        hotId_ = id;
        return true;
    }
    return false;
}

bool Ui::widgetRect(const std::string& id, Rect* out) const {
    for (const auto& entry : widgetRects_) {
        if (entry.first == id) {
            if (out != nullptr) *out = entry.second;
            return true;
        }
    }
    return false;
}

bool Ui::clicked(const std::string& id, const Rect& r, bool enabled) {
    widgetRects_.push_back({id, r});
    if (!enabled) return false;
    const bool over = hot(id, r);
    if (over && input_.mousePressed[0]) pressedId_ = id;
    return over && input_.mouseReleased[0] && pressedId_ == id;
}

bool Ui::button(const std::string& id, const Rect& r, const std::string& textValue,
                const std::string& iconName, bool primary, bool enabled) {
    const bool click = clicked(id, r, enabled);
    const bool over = enabled && hotId_ == id;
    const bool down = over && pressedId_ == id;
    const float radius = px(theme_.radius);
    Color fill = primary ? theme_.accent : theme_.controlSurface;
    if (down) fill = primary ? theme_.primaryFillPressed : theme_.controlSurfacePressed;
    else if (over) fill = primary ? theme_.primaryFillPressed : theme_.controlSurfacePressed;
    if (!enabled) fill = fill.alpha(0.5f);
    rect(r, fill, radius);
    if (!primary) border(r, theme_.controlBorder.alpha(0.7f), radius, 1.0f);
    const Color ink = !enabled ? theme_.textDisabled : primary ? theme_.textOnPrimary : theme_.textPrimary;
    float x = r.x + px(10);
    if (!iconName.empty()) {
        const float s = px(theme_.iconSize) * 0.85f;
        icon(iconName, {x, r.y + (r.h - s) * 0.5f, s, s}, ink);
        x += s + px(8);
    }
    if (iconName.empty()) {
        label({r.x + px(8), r.y, r.w - px(16), r.h}, textValue, ink, theme_.fontSize, primary, true);
    } else {
        label({x, r.y, r.right() - x - px(8), r.h}, textValue, ink, theme_.fontSize, false);
    }
    return click;
}

void Ui::selectedMark(const Rect& r, bool leadingBar) {
    rect(r, theme_.accentFill, px(theme_.radius));
    if (leadingBar) {
        const float h = std::min(px(16), r.h - px(8));
        rect({r.x + px(2), r.y + (r.h - h) * 0.5f, std::max(2.0f, px(2.5f)), h}, theme_.textPrimary,
             px(1.5f));
    } else {
        const float w = std::min(px(16), r.w - px(8));
        rect({r.x + (r.w - w) * 0.5f, r.bottom() - px(4), w, std::max(2.0f, px(2.0f))},
             theme_.textPrimary, px(1.0f));
    }
}

bool Ui::iconButton(const std::string& id, const Rect& r, const std::string& iconName,
                    const std::string& tip, bool active, bool enabled) {
    const bool click = clicked(id, r, enabled);
    const bool over = enabled && hotId_ == id;
    const float radius = px(theme_.radius);
    if (active) {
        selectedMark(r, false);
    } else if (over) {
        rect(r, theme_.controlSurfacePressed.alpha(0.85f), radius);
    }
    const float s = px(theme_.iconSize);
    const Color ink = !enabled ? theme_.textDisabled : theme_.textPrimary;
    icon(iconName, {r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f - (active ? px(1) : 0.0f), s, s},
         ink);
    if (over && !tip.empty()) tooltip(tip);
    return click;
}

bool Ui::ribbonButton(const std::string& id, const Rect& r, const std::string& iconName,
                      const std::string& caption, bool active, bool enabled,
                      const std::string& tip, bool primary) {
    const bool click = clicked(id, r, enabled);
    const bool over = enabled && hotId_ == id;
    const float radius = px(theme_.radius);
    Color ink = !enabled ? theme_.textDisabled : theme_.textPrimary;
    if (primary && enabled) {
        rect(r, over ? theme_.primaryFillPressed : theme_.accent, radius);
        ink = theme_.textOnPrimary;
    } else if (active) {
        selectedMark(r, false);
    } else if (over) {
        rect(r, theme_.controlSurfacePressed.alpha(0.85f), radius);
    }
    const float s = px(24);
    icon(iconName, {r.x + (r.w - s) * 0.5f, r.y + px(6), s, s}, ink);
    label({r.x + px(2), r.y + px(33), r.w - px(4), px(16)}, caption, ink, theme_.smallFontSize, false,
          true);
    if (over && !tip.empty()) tooltip(tip);
    return click;
}

bool Ui::tab(const std::string& id, const Rect& r, const std::string& textValue, bool active) {
    const bool click = clicked(id, r, true);
    const bool over = hotId_ == id;
    if (active) {
        selectedMark(r, false);
    } else if (over) {
        rect(r, theme_.controlSurfacePressed.alpha(0.7f), px(theme_.radius));
    }
    label({r.x, r.y - (active ? px(1) : 0.0f), r.w, r.h}, textValue,
          active ? theme_.textPrimary : theme_.textSecondary, theme_.fontSize, active, true);
    return click;
}

bool Ui::listRow(const std::string& id, const Rect& r, const std::string& textValue,
                 const std::string& iconName, bool selected, const std::string& detail) {
    const bool click = clicked(id, r, true);
    const bool over = hotId_ == id;
    const float radius = px(theme_.radius);
    if (selected) {
        selectedMark(r, true);
    } else if (over) {
        rect(r, theme_.controlSurfacePressed.alpha(0.6f), radius);
    }
    float x = r.x + px(10);
    if (!iconName.empty()) {
        const float s = px(theme_.iconSize) * 0.8f;
        icon(iconName, {x, r.y + (r.h - s) * 0.5f, s, s},
             selected ? theme_.textPrimary : theme_.textSecondary);
        x += s + px(8);
    }
    float detailW = 0;
    if (!detail.empty()) {
        detailW = std::min(textWidth(detail, theme_.smallFontSize) + px(4), r.w * 0.45f);
        label({r.right() - detailW - px(8), r.y, detailW, r.h}, detail, theme_.textSecondary,
              theme_.smallFontSize);
    }
    // A selected row keeps its weight: the fill and the bar carry the state,
    // so the label does not jump wider when it is chosen.
    label({x, r.y, r.right() - x - detailW - px(12), r.h}, textValue, theme_.textPrimary,
          theme_.fontSize, false);
    return click;
}

bool Ui::toggle(const std::string& id, const Rect& r, const std::string& textValue, bool* value) {
    const bool click = clicked(id, r, true);
    if (click) *value = !*value;
    const float tw = px(36), th = px(20);
    const Rect track{r.right() - tw - px(4), r.y + (r.h - th) * 0.5f, tw, th};
    // ON: a textPrimary thumb at the trailing end on the selected fill; OFF: a
    // textSecondary thumb on the field well. Shape and position carry the
    // state, never the accent.
    rect(track, *value ? theme_.accentFill : theme_.fieldSurface, th * 0.5f);
    border(track, theme_.fieldBorder, th * 0.5f, 1.0f);
    const float knob = th - px(6);
    const float kx = *value ? track.right() - knob - px(3) : track.x + px(3);
    rect({kx, track.y + px(3), knob, knob}, *value ? theme_.textPrimary : theme_.textSecondary,
         knob * 0.5f);
    label({r.x, r.y, r.w - tw - px(12), r.h}, textValue, theme_.textPrimary, theme_.fontSize);
    return click;
}

bool Ui::editCommit(const std::string& id, std::string* committed) {
    if (editId_ != id) return false;
    bool commit = false;
    bool cancel = false;
    for (Key k : input_.keys) {
        if (k == Key::Enter || k == Key::Tab) commit = true;
        if (k == Key::Escape) cancel = true;
    }
    if (editCommitPending_) {
        commit = true;
        editCommitPending_ = false;
    }
    if (cancel) {
        editId_.clear();
        return false;
    }
    if (commit) {
        *committed = editBuffer_;
        editId_.clear();
        return true;
    }
    return false;
}

bool Ui::textField(const std::string& id, const Rect& r, std::string* value) {
    const float radius = px(theme_.radius);
    const bool over = hot(id, r);
    const bool editing = editId_ == id;
    if (input_.mousePressed[0]) {
        if (over && !editing) {
            editId_ = id;
            editBuffer_ = *value;
            editInitial_ = *value;
            editCursor_ = decode(editBuffer_).size();
            // Focus selects the whole value, so the first keystroke replaces
            // it -- the Android fields' rule.
            editSelectAll_ = true;
        } else if (!over && editing) {
            editCommitPending_ = true;
        }
    }
    if (editId_ == id) {
        std::u32string cps = decode(editBuffer_);
        if (editSelectAll_) {
            bool replaces = false;
            for (char32_t ch : input_.text) replaces |= ch >= 0x20 && ch != 0x7F;
            bool erases = false, moves = false;
            for (Key k : input_.keys) {
                erases |= k == Key::Backspace || k == Key::Delete;
                moves |= k == Key::Left || k == Key::Right || k == Key::Home || k == Key::End;
            }
            if (replaces || erases) {
                cps.clear();
                editCursor_ = 0;
                editSelectAll_ = false;
            } else if (moves) {
                editSelectAll_ = false;
            }
        }
        for (char32_t ch : input_.text) {
            if (ch >= 0x20 && ch != 0x7F) {
                cps.insert(cps.begin() + std::min(editCursor_, cps.size()), ch);
                ++editCursor_;
            }
        }
        for (Key k : input_.keys) {
            if (k == Key::Backspace && editCursor_ > 0 && !cps.empty()) {
                cps.erase(cps.begin() + (editCursor_ - 1));
                --editCursor_;
            } else if (k == Key::Delete && editCursor_ < cps.size()) {
                cps.erase(cps.begin() + editCursor_);
            } else if (k == Key::Left && editCursor_ > 0) {
                --editCursor_;
            } else if (k == Key::Right && editCursor_ < cps.size()) {
                ++editCursor_;
            } else if (k == Key::Home) {
                editCursor_ = 0;
            } else if (k == Key::End) {
                editCursor_ = cps.size();
            }
        }
        editBuffer_ = encode(cps);
    }
    rect(r, theme_.fieldSurface, radius);
    border(r, editId_ == id ? theme_.accent : over ? theme_.textSecondary : theme_.fieldBorder,
           radius, editId_ == id ? 2.0f : 1.0f);
    const Rect inner{r.x + px(8), r.y, r.w - px(16), r.h};
    pushClip(inner);
    const std::string shown = editId_ == id ? editBuffer_ : *value;
    if (editId_ == id && editSelectAll_ && !shown.empty()) {
        const float sw = textWidth(shown, theme_.fontSize);
        rect({inner.x - px(1), r.y + px(6), sw + px(2), r.h - px(12)}, theme_.accent.alpha(0.45f), 0);
    }
    label(inner, shown, theme_.textPrimary, theme_.fontSize);
    if (editId_ == id && !editSelectAll_) {
        const std::u32string cps = decode(editBuffer_);
        const float caret = inner.x + textWidth(encode(cps.substr(0, editCursor_)), theme_.fontSize);
        rect({caret, r.y + px(7), std::max(1.0f, px(1.2f)), r.h - px(14)}, theme_.accent, 0);
    }
    popClip();
    std::string committed;
    if (editCommit(id, &committed)) {
        *value = committed;
        return true;
    }
    return false;
}

std::string Ui::formatFieldNumber(double value) {
    if (!std::isfinite(value)) return "0";
    if (value == 0.0) return "0";  // and -0 is 0
    // Twelve significant digits keep every digit a person types and drop the
    // binary noise a derived double carries below them (1.0000000000000002).
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.11e", value);
    // buffer: [-]d.ddddddddddde[+-]XX
    std::string digits;
    bool negative = false;
    const char* p = buffer;
    if (*p == '-') {
        negative = true;
        ++p;
    }
    for (; *p != '\0' && *p != 'e'; ++p) {
        if (*p != '.') digits.push_back(*p);
    }
    const int exponent = (*p == 'e') ? std::atoi(p + 1) : 0;
    // The decimal point stands after digit (exponent + 1).
    const int point = exponent + 1;
    std::string intPart, fracPart;
    if (point <= 0) {
        intPart = "0";
        fracPart = std::string(static_cast<size_t>(-point), '0') + digits;
    } else if (point >= static_cast<int>(digits.size())) {
        intPart = digits + std::string(static_cast<size_t>(point - static_cast<int>(digits.size())), '0');
    } else {
        intPart = digits.substr(0, static_cast<size_t>(point));
        fracPart = digits.substr(static_cast<size_t>(point));
    }
    while (!fracPart.empty() && fracPart.back() == '0') fracPart.pop_back();
    std::string out = negative ? "-" : "";
    out += intPart;
    if (!fracPart.empty()) out += "." + fracPart;
    if (out == "-0") out = "0";
    return out;
}

bool Ui::numberField(const std::string& id, const Rect& r, double* value, int decimals,
                     const std::string& unit) {
    (void)decimals;
    std::string text = formatFieldNumber(*value);
    const float unitW = unit.empty() ? 0.0f : textWidth(unit, theme_.smallFontSize) + px(10);
    const Rect field{r.x, r.y, r.w - unitW, r.h};
    const bool wasEditing = editId_ == id;
    const std::string initial = wasEditing ? editInitial_ : text;
    const bool committed = textField(id, field, &text);
    if (!unit.empty()) {
        label({field.right() + px(6), r.y, unitW, r.h}, unit, theme_.textSecondary,
              theme_.smallFontSize);
    }
    // Untouched text is not a statement: Tab, Enter or a click away from a
    // field that still reads what it showed submits nothing.
    if (!committed || text == initial) return false;
    std::string normalised = text;
    std::replace(normalised.begin(), normalised.end(), ',', '.');
    char* end = nullptr;
    const double parsed = std::strtod(normalised.c_str(), &end);
    if (end == normalised.c_str() || !std::isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

void Ui::tooltip(const std::string& textValue) {
    tooltipText_ = textValue;
    tooltipX_ = input_.mouseX;
    tooltipY_ = input_.mouseY;
}

}  // namespace fsui
