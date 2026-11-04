#include "vector_icon.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace fsui {

namespace {

bool attribute(const std::string& element, const char* name, std::string* value) {
    const std::string key = std::string(name) + "=\"";
    const size_t at = element.find(key);
    if (at == std::string::npos) return false;
    const size_t start = at + key.size();
    const size_t end = element.find('"', start);
    if (end == std::string::npos) return false;
    *value = element.substr(start, end - start);
    return true;
}

float attributeFloat(const std::string& element, const char* name, float fallback) {
    std::string v;
    if (!attribute(element, name, &v)) return fallback;
    return std::strtof(v.c_str(), nullptr);
}

// The alpha of an Android colour literal (#RGB, #ARGB, #RRGGBB, #AARRGGBB);
// a reference (@color/..., ?attr/...) is opaque.
float colorAlpha(const std::string& color) {
    if (color.empty()) return 0.0f;
    if (color[0] != '#') return 1.0f;
    const std::string hex = color.substr(1);
    if (hex.size() == 8) return std::strtoul(hex.substr(0, 2).c_str(), nullptr, 16) / 255.0f;
    if (hex.size() == 4) return std::strtoul(hex.substr(0, 1).c_str(), nullptr, 16) / 15.0f;
    return 1.0f;
}

class PathParser {
public:
    explicit PathParser(const std::string& d) : d_(d) {}

    void parse(VectorPath* path) {
        char command = 0;
        Point current{0, 0}, start{0, 0}, lastControl{0, 0};
        char lastCommand = 0;
        std::vector<Point> contour;
        const auto flush = [&](bool closed) {
            if (contour.size() >= 2) {
                path->contours.push_back(contour);
                path->closed.push_back(closed);
            }
            contour.clear();
        };
        while (skipSeparators()) {
            if (std::isalpha(static_cast<unsigned char>(d_[i_]))) {
                command = d_[i_++];
            } else if (command == 0) {
                return;
            }
            const bool rel = std::islower(static_cast<unsigned char>(command)) != 0;
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
            const Point base = rel ? current : Point{0, 0};
            switch (c) {
                case 'M': {
                    float x, y;
                    if (!number(&x) || !number(&y)) return;
                    flush(false);
                    current = {base.x + x, base.y + y};
                    start = current;
                    contour.push_back(current);
                    command = rel ? 'l' : 'L';  // implicit lineto after a moveto
                    break;
                }
                case 'L': {
                    float x, y;
                    if (!number(&x) || !number(&y)) return;
                    current = {base.x + x, base.y + y};
                    contour.push_back(current);
                    break;
                }
                case 'H': {
                    float x;
                    if (!number(&x)) return;
                    current.x = base.x + x;
                    contour.push_back(current);
                    break;
                }
                case 'V': {
                    float y;
                    if (!number(&y)) return;
                    current.y = base.y + y;
                    contour.push_back(current);
                    break;
                }
                case 'C': case 'S': {
                    float x1, y1, x2, y2, x, y;
                    if (c == 'C') {
                        if (!number(&x1) || !number(&y1)) return;
                    }
                    if (!number(&x2) || !number(&y2) || !number(&x) || !number(&y)) return;
                    Point p1;
                    if (c == 'C') {
                        p1 = {base.x + x1, base.y + y1};
                    } else {
                        const bool smooth = lastCommand == 'C' || lastCommand == 'S';
                        p1 = smooth ? Point{2 * current.x - lastControl.x, 2 * current.y - lastControl.y}
                                    : current;
                    }
                    const Point p2{base.x + x2, base.y + y2};
                    const Point p3{base.x + x, base.y + y};
                    if (contour.empty()) contour.push_back(current);
                    flattenCubic(current, p1, p2, p3, 12, &contour);
                    lastControl = p2;
                    current = p3;
                    break;
                }
                case 'Q': case 'T': {
                    float x1 = 0, y1 = 0, x, y;
                    if (c == 'Q') {
                        if (!number(&x1) || !number(&y1)) return;
                    }
                    if (!number(&x) || !number(&y)) return;
                    Point p1;
                    if (c == 'Q') {
                        p1 = {base.x + x1, base.y + y1};
                    } else {
                        const bool smooth = lastCommand == 'Q' || lastCommand == 'T';
                        p1 = smooth ? Point{2 * current.x - lastControl.x, 2 * current.y - lastControl.y}
                                    : current;
                    }
                    const Point p2{base.x + x, base.y + y};
                    if (contour.empty()) contour.push_back(current);
                    flattenQuadratic(current, p1, p2, 10, &contour);
                    lastControl = p1;
                    current = p2;
                    break;
                }
                case 'A': {
                    float rx, ry, rotation, large, sweep, x, y;
                    if (!number(&rx) || !number(&ry) || !number(&rotation) || !flag(&large)
                        || !flag(&sweep) || !number(&x) || !number(&y)) {
                        return;
                    }
                    const Point end{base.x + x, base.y + y};
                    if (contour.empty()) contour.push_back(current);
                    arc(current, end, rx, ry, rotation, large != 0.0f, sweep != 0.0f, &contour);
                    current = end;
                    break;
                }
                case 'Z': {
                    flush(true);
                    current = start;
                    break;
                }
                default:
                    return;
            }
            lastCommand = c;
        }
        flush(false);
    }

private:
    bool skipSeparators() {
        while (i_ < d_.size() && (std::isspace(static_cast<unsigned char>(d_[i_])) || d_[i_] == ',')) {
            ++i_;
        }
        return i_ < d_.size();
    }

    bool number(float* out) {
        skipSeparators();
        if (i_ >= d_.size()) return false;
        const char* begin = d_.c_str() + i_;
        // A number ends at a second '.' too ("1.5.5" is 1.5 and .5).
        size_t j = i_;
        if (d_[j] == '-' || d_[j] == '+') ++j;
        bool dot = false;
        while (j < d_.size()) {
            const char ch = d_[j];
            if (std::isdigit(static_cast<unsigned char>(ch))) { ++j; continue; }
            if (ch == '.' && !dot) { dot = true; ++j; continue; }
            if ((ch == 'e' || ch == 'E') && j + 1 < d_.size()) {
                ++j;
                if (d_[j] == '-' || d_[j] == '+') ++j;
                while (j < d_.size() && std::isdigit(static_cast<unsigned char>(d_[j]))) ++j;
            }
            break;
        }
        if (j == i_) return false;
        *out = std::strtof(std::string(begin, j - i_).c_str(), nullptr);
        i_ = j;
        return true;
    }

    // An arc flag is one character and may be written with no separator.
    bool flag(float* out) {
        skipSeparators();
        if (i_ >= d_.size() || (d_[i_] != '0' && d_[i_] != '1')) return false;
        *out = d_[i_] == '1' ? 1.0f : 0.0f;
        ++i_;
        return true;
    }

    // SVG endpoint arc to points (the standard centre conversion).
    static void arc(Point p0, Point p1, float rx, float ry, float rotationDeg, bool large, bool sweep,
                    std::vector<Point>* out) {
        rx = std::fabs(rx);
        ry = std::fabs(ry);
        if (rx < 1e-6f || ry < 1e-6f) {
            out->push_back(p1);
            return;
        }
        const float phi = rotationDeg * 3.14159265f / 180.0f;
        const float cp = std::cos(phi), sp = std::sin(phi);
        const float dx = (p0.x - p1.x) * 0.5f, dy = (p0.y - p1.y) * 0.5f;
        const float x1 = cp * dx + sp * dy;
        const float y1 = -sp * dx + cp * dy;
        float lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
        if (lambda > 1.0f) {
            const float s = std::sqrt(lambda);
            rx *= s;
            ry *= s;
        }
        float num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
        const float den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
        num = num < 0.0f ? 0.0f : num;
        float coef = den > 0.0f ? std::sqrt(num / den) : 0.0f;
        if (large == sweep) coef = -coef;
        const float cxp = coef * rx * y1 / ry;
        const float cyp = -coef * ry * x1 / rx;
        const float cx = cp * cxp - sp * cyp + (p0.x + p1.x) * 0.5f;
        const float cy = sp * cxp + cp * cyp + (p0.y + p1.y) * 0.5f;
        const auto angle = [](float ux, float uy, float vx, float vy) {
            return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        };
        const float theta = angle(1, 0, (x1 - cxp) / rx, (y1 - cyp) / ry);
        float delta = angle((x1 - cxp) / rx, (y1 - cyp) / ry, (-x1 - cxp) / rx, (-y1 - cyp) / ry);
        if (!sweep && delta > 0) delta -= 6.2831853f;
        if (sweep && delta < 0) delta += 6.2831853f;
        const int steps = std::max(4, static_cast<int>(std::ceil(std::fabs(delta) / 0.2f)));
        for (int i = 1; i <= steps; ++i) {
            const float t = theta + delta * static_cast<float>(i) / steps;
            const float ex = rx * std::cos(t), ey = ry * std::sin(t);
            out->push_back({cp * ex - sp * ey + cx, sp * ex + cp * ey + cy});
        }
    }

    const std::string& d_;
    size_t i_ = 0;
};

}  // namespace

bool parseVectorDrawable(const std::string& xml, VectorIcon* out) {
    const size_t vector = xml.find("<vector");
    if (vector == std::string::npos) return false;
    const size_t vectorEnd = xml.find('>', vector);
    const std::string root = xml.substr(vector, vectorEnd - vector);
    out->viewportWidth = attributeFloat(root, "android:viewportWidth", 24.0f);
    out->viewportHeight = attributeFloat(root, "android:viewportHeight", 24.0f);
    out->paths.clear();
    size_t at = vectorEnd;
    while ((at = xml.find("<path", at)) != std::string::npos) {
        const size_t end = xml.find("/>", at);
        if (end == std::string::npos) break;
        const std::string element = xml.substr(at, end - at);
        at = end;
        std::string data;
        if (!attribute(element, "android:pathData", &data)) continue;
        VectorPath path;
        std::string fill, stroke, cap;
        if (attribute(element, "android:fillColor", &fill)) {
            path.fillAlpha = colorAlpha(fill) * attributeFloat(element, "android:fillAlpha", 1.0f);
        }
        if (attribute(element, "android:strokeColor", &stroke)) {
            path.strokeAlpha = colorAlpha(stroke) * attributeFloat(element, "android:strokeAlpha", 1.0f);
            path.strokeWidth = attributeFloat(element, "android:strokeWidth", 0.0f);
        }
        path.roundCaps = attribute(element, "android:strokeLineCap", &cap) && cap == "round";
        PathParser(data).parse(&path);
        out->paths.push_back(std::move(path));
    }
    return !out->paths.empty();
}

void rasterizeIcon(const VectorIcon& icon, int size, std::vector<uint8_t>* out) {
    out->assign(static_cast<size_t>(size) * size, 0);
    const float scale = static_cast<float>(size) / std::max(icon.viewportWidth, icon.viewportHeight);
    CoverageRaster raster;
    for (const VectorPath& path : icon.paths) {
        std::vector<std::vector<Point>> scaled;
        for (const std::vector<Point>& contour : path.contours) {
            std::vector<Point> s;
            s.reserve(contour.size());
            for (const Point& p : contour) s.push_back({p.x * scale, p.y * scale});
            scaled.push_back(std::move(s));
        }
        if (path.fillAlpha > 0.0f) {
            raster.reset(size, size);
            for (const std::vector<Point>& contour : scaled) raster.polygon(contour);
            raster.resolveOver(out->data(), path.fillAlpha);
        }
        if (path.strokeAlpha > 0.0f && path.strokeWidth > 0.0f) {
            raster.reset(size, size);
            for (size_t i = 0; i < scaled.size(); ++i) {
                strokePolyline(scaled[i], path.closed[i], path.strokeWidth * scale, path.roundCaps,
                               &raster);
            }
            raster.resolveOver(out->data(), path.strokeAlpha);
        }
    }
}

}  // namespace fsui
