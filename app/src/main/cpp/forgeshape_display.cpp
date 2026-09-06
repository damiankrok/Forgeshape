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
        case ViewportBackground::WarmGraphite: return "WarmGraphite";
        case ViewportBackground::NeutralCharcoal: return "NeutralCharcoal";
        case ViewportBackground::LightCharcoal: return "LightCharcoal";
        case ViewportBackground::WarmLight: return "WarmLight";
        case ViewportBackground::CoolLight: return "CoolLight";
    }
    return "Unknown";
}

bool viewportBackgroundIsLight(ViewportBackground background) {
    return background == ViewportBackground::WarmLight ||
           background == ViewportBackground::CoolLight;
}

const char* gizmoStrokeWeightName(GizmoStrokeWeight weight) {
    switch (weight) {
        case GizmoStrokeWeight::Thin: return "Thin";
        case GizmoStrokeWeight::Regular: return "Regular";
        case GizmoStrokeWeight::Bold: return "Bold";
    }
    return "Unknown";
}

bool gizmoStrokeWeightFromIndex(int index, GizmoStrokeWeight* out) {
    if (index < 0 || index >= kGizmoStrokeWeightCount || out == nullptr) {
        return false;
    }
    *out = static_cast<GizmoStrokeWeight>(index);
    return true;
}

int gizmoStrokeWeightIndex(GizmoStrokeWeight weight) {
    return static_cast<int>(weight);
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
        case ViewportBackground::NeutralCharcoal:
            // #26282A. The coolest of the three: blue leads red by four points,
            // which is what separates it from Warm Graphite at a glance without
            // either reading as tinted.
            outRgb[0] = 0.149f;
            outRgb[1] = 0.157f;
            outRgb[2] = 0.165f;
            return;
        case ViewportBackground::LightCharcoal:
            // #3C3F41. The lightest ground the set allows. Past roughly this
            // value a neutral clay render stops reading as lit and starts
            // reading as washed out, which is the same failure a stark white
            // canvas causes from the other direction.
            outRgb[0] = 0.235f;
            outRgb[1] = 0.247f;
            outRgb[2] = 0.255f;
            return;
        case ViewportBackground::WarmLight:
            // #EDE7DC. A cream-biased light canvas (UI-PREF-R1, UI-OWNER-42):
            // red leads blue by seventeen points, so it reads as paper rather
            // than as a grey that has merely been turned up.
            outRgb[0] = 0.929f;
            outRgb[1] = 0.906f;
            outRgb[2] = 0.863f;
            return;
        case ViewportBackground::CoolLight:
            // #E4E8EC. A steel-biased light canvas: blue leads red by eight
            // points, which separates it from Warm Light at a glance the same
            // way Neutral Charcoal is separated from Warm Graphite.
            outRgb[0] = 0.894f;
            outRgb[1] = 0.910f;
            outRgb[2] = 0.925f;
            return;
        case ViewportBackground::WarmGraphite:
            break;
    }
    // #302E2B. The product default: red leads blue by five points, so the
    // ground is warm enough to keep a neutral clay body from looking cold and
    // far short of brown.
    outRgb[0] = 0.188f;
    outRgb[1] = 0.180f;
    outRgb[2] = 0.169f;
}

ViewportDisplaySettings DisplaySettingsStore::snapshot() const {
    ViewportDisplaySettings out;
    out.shading = shadingModel();
    out.surface = surfaceShading();
    out.background = viewportBackground();
    out.gridVisible = gridVisible();
    out.reducedMotion = reducedMotion();
    out.gizmoStrokeWeight = gizmoStrokeWeight();
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

GizmoStrokeWeight DisplaySettingsStore::gizmoStrokeWeight() const {
    GizmoStrokeWeight weight = kDefaultGizmoStrokeWeight;
    gizmoStrokeWeightFromIndex(gizmoStrokeWeight_.load(std::memory_order_relaxed), &weight);
    return weight;
}

bool DisplaySettingsStore::setGizmoStrokeWeight(GizmoStrokeWeight weight) {
    const int next = gizmoStrokeWeightIndex(weight);
    const int previous = gizmoStrokeWeight_.exchange(next, std::memory_order_relaxed);
    if (previous == next) {
        return false;
    }
    // Counted like the grid: a display setting the user chose, whose one
    // consequence is a re-upload of the gizmo's canonical vertex list.
    changeCount_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

DisplaySettingsStore& displaySettings() {
    static DisplaySettingsStore store;
    return store;
}

}  // namespace forgeshape
