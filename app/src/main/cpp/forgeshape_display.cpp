#include "forgeshape_display.h"

namespace forgeshape {

const char* shadingModelName(ShadingModel model) {
    switch (model) {
        case ShadingModel::StudioSolid: return "StudioSolid";
        case ShadingModel::MatCap: return "MatCap";
        case ShadingModel::DebugSourceColor: return "DebugSourceColor";
    }
    return "Unknown";
}

bool shadingModelFromIndex(int index, ShadingModel* out) {
    if (index < 0 || index >= kShadingModelCount || out == nullptr) {
        return false;
    }
    *out = static_cast<ShadingModel>(index);
    return true;
}

int shadingModelIndex(ShadingModel model) {
    return static_cast<int>(model);
}

bool surfaceShadingFromIndex(int index, SurfaceShading* out) {
    if (out == nullptr) {
        return false;
    }
    switch (index) {
        case 0: *out = SurfaceShading::Smooth; return true;
        case 1: *out = SurfaceShading::Faceted; return true;
        default: return false;
    }
}

int surfaceShadingIndex(SurfaceShading shading) {
    return shading == SurfaceShading::Faceted ? 1 : 0;
}

ViewportDisplaySettings DisplaySettingsStore::snapshot() const {
    ViewportDisplaySettings out;
    out.shading = shadingModel();
    out.surface = surfaceShading();
    return out;
}

ShadingModel DisplaySettingsStore::shadingModel() const {
    ShadingModel model = kDefaultShadingModel;
    // Only a validated index is ever stored, so this cannot fail; the checked
    // conversion is kept anyway so a future writer that forgets to validate
    // degrades to the product default rather than to an undefined enumerator.
    shadingModelFromIndex(shading_.load(std::memory_order_relaxed), &model);
    return model;
}

SurfaceShading DisplaySettingsStore::surfaceShading() const {
    SurfaceShading shading = kDefaultSurfaceShading;
    surfaceShadingFromIndex(surface_.load(std::memory_order_relaxed), &shading);
    return shading;
}

bool DisplaySettingsStore::setShadingModel(ShadingModel model) {
    const int next = shadingModelIndex(model);
    const int previous = shading_.exchange(next, std::memory_order_relaxed);
    if (previous == next) {
        return false;
    }
    changeCount_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool DisplaySettingsStore::setSurfaceShading(SurfaceShading shading) {
    const int next = surfaceShadingIndex(shading);
    const int previous = surface_.exchange(next, std::memory_order_relaxed);
    if (previous == next) {
        return false;
    }
    changeCount_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

DisplaySettingsStore& displaySettings() {
    static DisplaySettingsStore store;
    return store;
}

}  // namespace forgeshape
