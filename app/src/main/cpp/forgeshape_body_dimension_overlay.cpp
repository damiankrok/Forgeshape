#include "forgeshape_body_dimension_overlay.h"

#include <cmath>
#include <memory>
#include <vector>

namespace forgeshape {
namespace {

void pushLine(std::vector<GizmoVertex>* out, const Vec3& a, const Vec3& b, float handle) {
    // Hue tag 0 throughout, exactly as the sketch's own dimension range uses:
    // an annotation is not an axis handle and must not borrow an axis colour,
    // or a reader would take the leader for something they can grab.
    out->push_back(GizmoVertex{{a.x, a.y, a.z}, 0.0f, handle});
    out->push_back(GizmoVertex{{b.x, b.y, b.z}, 0.0f, handle});
}

Vec3 add(const Vec3& a, const Vec3& b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }

Vec3 scaled(const Vec3& v, double k) {
    return Vec3{v.x * static_cast<float>(k), v.y * static_cast<float>(k),
                v.z * static_cast<float>(k)};
}

bool finiteVec(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace

SketchOverlayPtr buildBodyDimensionOverlay(const LocalBounds& bounds,
                                           const TransformValues& placement, int activeAxis,
                                           float worldPerUnit, uint64_t revision,
                                           BodyDimensionLabelAnchors* outAnchors) {
    auto built = std::make_shared<SketchOverlay>();
    built->revision = revision;
    if (outAnchors != nullptr) {
        *outAnchors = BodyDimensionLabelAnchors{};
    }
    if (!localBoundsValid(bounds) || !(worldPerUnit > 0.0f) || !std::isfinite(worldPerUnit)) {
        return built;
    }

    // The body's own basis, as three UNIT world directions, and its origin.
    // Taken from the rotation matrix rather than from the model matrix, so the
    // body's scale does not reach the annotation's stand-off: how large a body
    // is must not change how far its leaders sit from it.
    const Mat4 rotation = rotationMatrixFromEuler(eulerOf(placement));
    Vec3 axisDir[kBodyAxisCount];
    for (int a = 0; a < kBodyAxisCount; ++a) {
        axisDir[a] = Vec3{rotation.m[a * 4 + 0], rotation.m[a * 4 + 1], rotation.m[a * 4 + 2]};
        if (!finiteVec(axisDir[a])) {
            return built;
        }
    }
    const Vec3 origin{static_cast<float>(placement.positionX),
                      static_cast<float>(placement.positionY),
                      static_cast<float>(placement.positionZ)};
    const double scale[kBodyAxisCount] = {placement.scaleX, placement.scaleY, placement.scaleZ};

    // One LOCAL point, carried into the world by the body's own placement:
    // P = T + R * S * p, the stated convention and no other.
    const auto world = [&](double lx, double ly, double lz) {
        Vec3 p = origin;
        const double local[kBodyAxisCount] = {lx, ly, lz};
        for (int a = 0; a < kBodyAxisCount; ++a) {
            p = add(p, scaled(axisDir[a], local[a] * scale[a]));
        }
        return p;
    };

    const double unit = static_cast<double>(worldPerUnit);
    const double offset = kBodyDimensionOffsetUnits * unit;
    const double gap = kBodyDimensionExtensionGapUnits * unit;
    const double overshoot = kBodyDimensionOvershootUnits * unit;
    const double tick = kBodyDimensionTickUnits * unit;

    // Two ranges, both weights the renderer already draws: the active axis in
    // the annotation highlight, the other two in the neutral entity weight.
    std::vector<GizmoVertex> neutral;
    std::vector<GizmoVertex> highlighted;
    BodyDimensionLabelAnchors anchors;
    anchors.valid = true;

    for (int a = 0; a < kBodyAxisCount; ++a) {
        std::vector<GizmoVertex>* out = (a == activeAxis) ? &highlighted : &neutral;
        const int b = (a + 1) % kBodyAxisCount;
        const int c = (a + 2) % kBodyAxisCount;

        // Each axis is measured along the body edge that meets at the corner
        // with the two other axes at their MINIMUM, and its annotation is
        // pushed outward along both of those. Three different edges, so the
        // three leaders never lie on top of one another.
        double from[kBodyAxisCount] = {0.0, 0.0, 0.0};
        double to[kBodyAxisCount] = {0.0, 0.0, 0.0};
        from[a] = bounds.min(a);
        to[a] = bounds.max(a);
        from[b] = to[b] = bounds.min(b);
        from[c] = to[c] = bounds.min(c);

        const Vec3 cornerFrom = world(from[0], from[1], from[2]);
        const Vec3 cornerTo = world(to[0], to[1], to[2]);
        // The stand-off, in WORLD units along the body's own two other axes,
        // away from the body.
        const Vec3 away = add(scaled(axisDir[b], -1.0), scaled(axisDir[c], -1.0));
        const double awayLength = std::sqrt(static_cast<double>(away.x) * away.x
                                            + static_cast<double>(away.y) * away.y
                                            + static_cast<double>(away.z) * away.z);
        if (!(awayLength > 0.0) || !std::isfinite(awayLength)) {
            return std::make_shared<SketchOverlay>(SketchOverlay{revision, {}, {}});
        }
        const Vec3 awayUnit = scaled(away, 1.0 / awayLength);

        // Extension lines: from just clear of each body corner, out past the
        // dimension line. They never touch the geometry, which is what keeps
        // the annotation legible against the body it measures.
        pushLine(out, add(cornerFrom, scaled(awayUnit, gap)),
                 add(cornerFrom, scaled(awayUnit, offset + overshoot)),
                 a == activeAxis ? 1.0f : 0.0f);
        pushLine(out, add(cornerTo, scaled(awayUnit, gap)),
                 add(cornerTo, scaled(awayUnit, offset + overshoot)),
                 a == activeAxis ? 1.0f : 0.0f);

        const Vec3 lineFrom = add(cornerFrom, scaled(awayUnit, offset));
        const Vec3 lineTo = add(cornerTo, scaled(awayUnit, offset));
        pushLine(out, lineFrom, lineTo, a == activeAxis ? 1.0f : 0.0f);

        // The two end ticks: a slash at 45 degrees to the dimension line, the
        // draughting convention the sketch annotation already uses.
        const Vec3 slash = add(scaled(axisDir[a], 0.70710678118654752),
                               scaled(awayUnit, 0.70710678118654752));
        for (const Vec3& at : {lineFrom, lineTo}) {
            pushLine(out, add(at, scaled(slash, -tick)), add(at, scaled(slash, tick)),
                     a == activeAxis ? 1.0f : 0.0f);
        }

        // The label anchor: the midpoint of the dimension line, which is the
        // only thing the chrome label is told.
        anchors.axis[a] = Vec3{(lineFrom.x + lineTo.x) * 0.5f, (lineFrom.y + lineTo.y) * 0.5f,
                               (lineFrom.z + lineTo.z) * 0.5f};
        if (!finiteVec(anchors.axis[a])) {
            anchors.valid = false;
        }
    }

    built->vertices.reserve(neutral.size() + highlighted.size());
    SketchOverlayRange neutralRange;
    neutralRange.firstVertex = 0;
    neutralRange.vertexCount = static_cast<uint32_t>(neutral.size());
    neutralRange.style = SketchOverlayStyle::Entities;
    built->vertices.insert(built->vertices.end(), neutral.begin(), neutral.end());
    built->ranges.push_back(neutralRange);

    SketchOverlayRange activeRange;
    activeRange.firstVertex = static_cast<uint32_t>(built->vertices.size());
    activeRange.vertexCount = static_cast<uint32_t>(highlighted.size());
    activeRange.style = SketchOverlayStyle::Dimension;
    built->vertices.insert(built->vertices.end(), highlighted.begin(), highlighted.end());
    built->ranges.push_back(activeRange);

    if (built->vertices.size() > kMaxSketchOverlayVertices) {
        // Cannot happen -- three axes are 24 vertices -- and refused rather
        // than trusted, on the sketch overlay's own terms.
        built->vertices.clear();
        built->ranges.clear();
        anchors.valid = false;
    }
    if (outAnchors != nullptr) {
        *outAnchors = anchors;
    }
    return built;
}

void BodyDimensionSession::open() {
    active_ = true;
    activeAxis_ = kNoDimensionAxis;
    anchor_ = ResizeAnchor::Center;
    built_ = false;
}

void BodyDimensionSession::close() {
    active_ = false;
    activeAxis_ = kNoDimensionAxis;
    anchor_ = ResizeAnchor::Center;
    overlay_.reset();
    labelAnchors_ = BodyDimensionLabelAnchors{};
    built_ = false;
}

bool BodyDimensionSession::setActiveAxis(int axis) {
    if (axis != kNoDimensionAxis && (axis < 0 || axis >= kBodyAxisCount)) {
        return false;
    }
    activeAxis_ = axis;
    return true;
}

bool BodyDimensionSession::setAnchor(ResizeAnchor anchor) {
    anchor_ = anchor;
    return true;
}

SketchOverlayPtr BodyDimensionSession::overlay(const LocalBounds& bounds,
                                               const TransformValues& placement,
                                               float worldPerUnit) {
    if (!active_) {
        overlay_.reset();
        labelAnchors_ = BodyDimensionLabelAnchors{};
        built_ = false;
        return overlay_;
    }
    const bool sameInputs = built_ && builtWorldPerUnit_ == worldPerUnit && builtAxis_ == activeAxis_
                         && builtBounds_.minX == bounds.minX && builtBounds_.maxX == bounds.maxX
                         && builtBounds_.minY == bounds.minY && builtBounds_.maxY == bounds.maxY
                         && builtBounds_.minZ == bounds.minZ && builtBounds_.maxZ == bounds.maxZ
                         && sameConstructionPlacement(builtPlacement_, placement);
    if (sameInputs && overlay_) {
        return overlay_;
    }
    ++revision_;
    overlay_ = buildBodyDimensionOverlay(bounds, placement, activeAxis_, worldPerUnit, revision_,
                                         &labelAnchors_);
    builtBounds_ = bounds;
    builtPlacement_ = placement;
    builtWorldPerUnit_ = worldPerUnit;
    builtAxis_ = activeAxis_;
    built_ = true;
    return overlay_;
}

BodyDimensionSession& bodyDimensionSession() {
    static BodyDimensionSession session;
    return session;
}

}  // namespace forgeshape
