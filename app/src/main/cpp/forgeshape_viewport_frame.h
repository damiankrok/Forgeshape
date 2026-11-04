// What one viewport frame is handed (`DESKTOP-R1`): the ONE composition both
// shells draw, so a body, a gizmo, a sketch, a chooser, a preview and a
// Dimensions leader stand in the same order, under the same rules and from the
// same instant on Android and on the desktop.
//
// It reads the scene, the camera and every editing session under
// `editorStateMutex()` and hands the renderer immutable snapshots; it draws
// nothing itself. The renderer's own `drawFrame` follows it.
#pragma once

#include <functional>

#include "forgeshape_editor_core.h"
#include "forgeshape_sketch_overlay.h"

namespace forgeshape {

class Renderer;

// The parts of a frame that still live in a shell, act by act until they move
// into the core: the one overlay slot's shell producers (Android's Fillet /
// Chamfer and Hole offers and its Measure reading). Called under the state
// mutex with whether a sketch session is active; an empty pointer means none.
struct ViewportFrameSources {
    std::function<SketchOverlayPtr(bool sketchActive)> shellOverlay;
};

void composeViewportFrame(Renderer& renderer, const ViewportFrameSources& sources);

}  // namespace forgeshape
