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

const char* viewportBackgroundName(ViewportBackground background) {
    switch (background) {
        case ViewportBackground::NeutralDark: return "NeutralDark";
        case ViewportBackground::WarmLight: return "WarmLight";
    }
    return "Unknown";
}

bool viewportBackgroundFromIndex(int index, ViewportBackground* out) {
    if (index < 0 || index >= kViewportBackgroundCount || out == nullptr) {
        return false;
    }
    *out = static_cast<ViewportBackground>(index);
    return true;
}

int viewportBackgroundIndex(ViewportBackground background) {
    return static_cast<int>(background);
}

void viewportBackgroundColor(ViewportBackground background, float* outRgb) {
    if (outRgb == nullptr) {
        return;
    }
    switch (background) {
        case ViewportBackground::WarmLight:
            // #E6E1D9. Warm by about ten points of red over blue: paper rather
            // than screen, and far short of beige. Deliberately not #FFFFFF — a
            // stark white ground makes a neutral clay render read as grey.
            outRgb[0] = 0.902f;
            outRgb[1] = 0.882f;
            outRgb[2] = 0.851f;
            return;
        case ViewportBackground::NeutralDark:
            break;
    }
    // #0E121B, unchanged from every release before the light theme existed.
    outRgb[0] = 0.055f;
    outRgb[1] = 0.070f;
    outRgb[2] = 0.105f;
}

ViewportDisplaySettings DisplaySettingsStore::snapshot() const {
    ViewportDisplaySettings out;
    out.shading = shadingModel();
    out.surface = surfaceShading();
    out.background = viewportBackground();
    out.gridVisible = gridVisible();
    out.reducedMotion = reducedMotion();
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

ViewportBackground DisplaySettingsStore::viewportBackground() const {
    ViewportBackground background = kDefaultViewportBackground;
    viewportBackgroundFromIndex(background_.load(std::memory_order_relaxed), &background);
    return background;
}

bool DisplaySettingsStore::setViewportBackground(ViewportBackground background) {
    const int next = viewportBackgroundIndex(background);
    const int previous = background_.exchange(next, std::memory_order_relaxed);
    if (previous == next) {
        return false;
    }
    changeCount_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool DisplaySettingsStore::gridVisible() const {
    return gridVisible_.load(std::memory_order_relaxed);
}

bool DisplaySettingsStore::setGridVisible(bool visible) {
    const bool previous = gridVisible_.exchange(visible, std::memory_order_relaxed);
    if (previous == visible) {
        return false;
    }
    // Counted, unlike reduced motion: this IS a display setting the user chose,
    // exactly like the shading model beside it, and the display suite proves a
    // real transition happened by watching this counter move.
    changeCount_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool DisplaySettingsStore::reducedMotion() const {
    return reducedMotion_.load(std::memory_order_relaxed);
}

bool DisplaySettingsStore::setReducedMotion(bool reduced) {
    const bool previous = reducedMotion_.exchange(reduced, std::memory_order_relaxed);
    if (previous == reduced) {
        return false;
    }
    // Deliberately NOT counted in changeCount_: that counter exists so the
    // display suite can prove a real display transition happened, and reduced
    // motion is an accessibility preference arriving from the platform rather
    // than a display setting the user chose here.
    return true;
}

DisplaySettingsStore& displaySettings() {
    static DisplaySettingsStore store;
    return store;
}

}  // namespace forgeshape
