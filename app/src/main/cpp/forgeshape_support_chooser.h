// The spatial "Choose Sketch Support" mode (`CAD-A3`, `UI-OWNER-46`).
//
// Viewport-first support selection: the three principal world planes are drawn
// as large touchable targets at the origin, and -- for an existing CAD project
// -- the planar faces of CAD bodies are eligible too. The user hovers (stylus)
// or presses (finger) a target and taps to select it; a text list is never the
// primary path. A selection yields a support the sketch session can begin on:
// a world plane, or a producer face resolved into a world frame with its
// TopoRef.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan. It produces a line
// overlay (drawn through the same path as the sketch overlay) and resolves a
// tap against the planes and the scene; it renders nothing itself and owns no
// project state. Session-only, like the sketch session: nothing here is a body,
// an ObjectId or a .forge byte until the sketch it starts is committed.
#pragma once

#include <memory>

#include "forgeshape_camera.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_overlay.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

// What a tap resolved to.
struct ChosenSupport {
    enum class Kind { None, WorldPlane, Face };
    Kind kind = Kind::None;
    Workplane plane = Workplane::XY;   // when WorldPlane
    TopoRef faceRef{};                 // when Face
    SketchFrame worldFrame{};          // when Face: the resolved world frame
};

// The half-size of a drawn world-plane target square, in metres.
constexpr float kSupportPlaneHalfMeters = 3.0f;

class SupportChooser {
public:
    // Enters the mode. `allowFaces` is true for New Sketch inside an existing
    // CAD project; false for a brand-new CAD project (no bodies to sketch on).
    void begin(bool allowFaces);
    void cancel();
    bool active() const { return active_; }
    bool allowFaces() const { return allowFaces_; }

    // Resolves a screen point to a support WITHOUT committing to it: used for
    // stylus hover, which highlights but never selects. Updates the overlay.
    ChosenSupport hover(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                        int viewportHeight, const ConstructionScene& scene);

    // Resolves and SELECTS a support: the tap that chooses. Updates the overlay
    // to show the selection and returns it; None when the tap missed every
    // target.
    ChosenSupport select(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                         int viewportHeight, const ConstructionScene& scene);

    // The currently selected support, or Kind::None.
    const ChosenSupport& selected() const { return selected_; }

    // The line overlay: the three plane targets, the eligible/hovered/selected
    // highlights and, when a face is the target, its outline. Cached by
    // revision like the sketch overlay.
    std::shared_ptr<const SketchOverlay> overlay();

private:
    ChosenSupport resolve(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                          int viewportHeight, const ConstructionScene& scene) const;
    void rebuildOverlay();

    bool active_ = false;
    bool allowFaces_ = false;
    ChosenSupport hovered_;
    ChosenSupport selected_;
    std::shared_ptr<SketchOverlay> overlay_;
    uint32_t revision_ = 1;
    bool dirty_ = true;
};

SupportChooser& supportChooser();

}  // namespace forgeshape
