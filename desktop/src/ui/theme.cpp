#include "theme.h"

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include "forgeshape_json.h"

namespace fsui {

namespace {

bool parseColor(const std::string& text, Color* out) {
    if (text.size() != 7 && text.size() != 9) return false;
    if (text[0] != '#') return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(text.c_str() + 1, &end, 16);
    if (end == nullptr || *end != 0) return false;
    *out = Color::hex(text.size() == 7 ? (0xFF000000u | static_cast<uint32_t>(v))
                                       : static_cast<uint32_t>(v));
    return true;
}

}  // namespace

bool Theme::loadOverrides(const std::string& path, std::string* report) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    std::string text;
    char buffer[4096];
    size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, n);
    std::fclose(file);
    forgeshape::JsonDocument json;
    if (json.parse(text.data(), text.size()) != forgeshape::JsonStatus::Ok
        || json.root().type != forgeshape::JsonType::Object) {
        if (report) *report = "theme file is not a JSON object";
        return false;
    }
    const std::vector<std::pair<const char*, Color*>> colors = {
        {"viewportBackground", &viewportBackground}, {"chromeSurface", &chromeSurface},
        {"surfaceFloating", &surfaceFloating},       {"surfacePrecision", &surfacePrecision},
        {"chromeHairline", &chromeHairline},         {"controlSurface", &controlSurface},
        {"controlSurfacePressed", &controlSurfacePressed},
        {"controlBorder", &controlBorder},           {"fieldSurface", &fieldSurface},
        {"accent", &accent},                         {"accentFill", &accentFill},
        {"primaryFillPressed", &primaryFillPressed}, {"textOnPrimary", &textOnPrimary},
        {"textPrimary", &textPrimary},               {"textSecondary", &textSecondary},
        {"textDisabled", &textDisabled},             {"textSuccess", &textSuccess},
        {"textError", &textError},                   {"textMeasure", &textMeasure},
        {"shadow", &shadow}};
    const std::vector<std::pair<const char*, float*>> sizes = {
        {"fontSize", &fontSize},         {"smallFontSize", &smallFontSize},
        {"titleFontSize", &titleFontSize}, {"iconSize", &iconSize},
        {"controlHeight", &controlHeight}, {"rowHeight", &rowHeight},
        {"radius", &radius},             {"panelRadius", &panelRadius},
        {"spacing", &spacing},           {"toolbarHeight", &toolbarHeight},
        {"statusHeight", &statusHeight}, {"browserWidth", &browserWidth},
        {"ribbonHeight", &ribbonHeight}, {"historyHeight", &historyHeight},
        {"propertiesWidth", &propertiesWidth}, {"panelInset", &panelInset},
        {"shadowBlur", &shadowBlur}};
    int applied = 0;
    for (const auto& entry : colors) {
        std::string value;
        Color c;
        if (json.stringMember(json.root(), entry.first, &value) && parseColor(value, &c)) {
            *entry.second = c;
            ++applied;
        }
    }
    for (const auto& entry : sizes) {
        double value = 0.0;
        // Bounded so a typo cannot make the interface unusable.
        if (json.numberMember(json.root(), entry.first, &value) && value >= 0.0 && value <= 2000.0) {
            *entry.second = static_cast<float>(value);
            ++applied;
        }
    }
    if (report) *report = std::to_string(applied) + " theme tokens applied";
    return true;
}

}  // namespace fsui
