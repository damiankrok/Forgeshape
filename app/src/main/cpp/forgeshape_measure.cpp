#include "forgeshape_measure.h"

#include <cmath>
#include <memory>

namespace forgeshape {

void measureRecordPoint(MeasureState* state, const Vec3& point, bool snapped) {
    if (state == nullptr) {
        return;
    }
    if (state->count == 1) {
        state->b = point;
        state->snappedB = snapped;
        state->count = 2;
        return;
    }
    state->a = point;
    state->snappedA = snapped;
    state->b = Vec3{0.0f, 0.0f, 0.0f};
    state->snappedB = false;
    state->count = 1;
}

MeasureResult measureResult(const MeasureState& state) {
    MeasureResult result;
    if (state.count < 2) {
        return result;
    }
    result.dx = static_cast<double>(state.b.x) - static_cast<double>(state.a.x);
    result.dy = static_cast<double>(state.b.y) - static_cast<double>(state.a.y);
    result.dz = static_cast<double>(state.b.z) - static_cast<double>(state.a.z);
    result.distance = std::sqrt(result.dx * result.dx + result.dy * result.dy + result.dz * result.dz);
    result.complete = std::isfinite(result.distance);
    return result;
}

Vec3 measureSnapPoint(const CameraSnapshot& camera, int viewportWidth, int viewportHeight,
                      float tapX, float tapY, const RuntimeMesh& mesh, const Mat4& model,
                      int triangle, const Vec3& hit, bool* snapped) {
    if (snapped != nullptr) {
        *snapped = false;
    }
    if (triangle < 0 || static_cast<uint32_t>(triangle) >= mesh.triangleCount()) {
        return hit;
    }
    float best = kMeasureSnapPixels;
    Vec3 chosen = hit;
    bool found = false;
    for (int k = 0; k < 3; ++k) {
        const uint32_t index = mesh.indices()[static_cast<uint32_t>(triangle) * 3u + static_cast<uint32_t>(k)];
        if (index >= mesh.vertexCount()) {
            continue;
        }
        const float* p = mesh.vertices()[index].position;
        const Vec3 world = mat4TransformPoint(model, Vec3{p[0], p[1], p[2]});
        float sx = 0.0f;
        float sy = 0.0f;
        if (!projectWorldToScreen(camera, world, viewportWidth, viewportHeight, &sx, &sy)) {
            continue;
        }
        const float d = std::hypot(sx - tapX, sy - tapY);
        if (d <= best) {
            best = d;
            chosen = world;
            found = true;
        }
    }
    if (snapped != nullptr) {
        *snapped = found;
    }
    return chosen;
}

SketchOverlayPtr measureOverlay(const MeasureState& state, float markerSize) {
    auto built = std::make_shared<SketchOverlay>();
    std::vector<GizmoVertex>& v = built->vertices;
    const float h = std::isfinite(markerSize) && markerSize > 0.0f ? 0.5f * markerSize : 0.0f;
    // The measure's own hue (the Z axis's blue), apart from a finish highlight.
    const auto line = [&v](const Vec3& p, const Vec3& q) {
        v.push_back(GizmoVertex{{p.x, p.y, p.z}, 3.0f, 1.0f});
        v.push_back(GizmoVertex{{q.x, q.y, q.z}, 3.0f, 1.0f});
    };
    const auto cross = [&](const Vec3& c) {
        line(Vec3{c.x - h, c.y, c.z}, Vec3{c.x + h, c.y, c.z});
        line(Vec3{c.x, c.y - h, c.z}, Vec3{c.x, c.y + h, c.z});
        line(Vec3{c.x, c.y, c.z - h}, Vec3{c.x, c.y, c.z + h});
    };
    if (state.count >= 1) {
        cross(state.a);
    }
    if (state.count >= 2) {
        cross(state.b);
        line(state.a, state.b);
    }
    SketchOverlayRange range;
    range.firstVertex = 0;
    range.vertexCount = static_cast<uint32_t>(v.size());
    range.style = SketchOverlayStyle::Entities;
    built->ranges.push_back(range);
    uint64_t hash = 0xCBF29CE484222325ull;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(v.data());
    for (size_t i = 0; i < v.size() * sizeof(GizmoVertex); ++i) {
        hash ^= bytes[i];
        hash *= 0x100000001B3ull;
    }
    built->revision = hash | 0x8000000000000000ull;
    return built;
}

}  // namespace forgeshape
