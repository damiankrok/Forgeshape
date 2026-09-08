// The dimension leaders drawn around a Construction Body in Dimensions mode
// (Stage 020M, `UI-OWNER-33B`).
//
// Presentation in the strongest sense the project has, on exactly the sketch
// overlay's terms: it produces a WORLD-space line list with no ObjectId, is
// never published through a MeshStore, is never picked, never exported, never
// reaches a `.forge` byte, mints no revision and moves no dirty flag. Nothing
// downstream may read a dimension back out of it — the vertices are already
// world positions and the number itself is not in them at all.
//
// It reuses the SKETCH overlay's structure and the renderer's existing line
// pipeline rather than adding a second overlay path: the annotation for a
// selected sketch Line and the annotation around a body are the same kind of
// drawing, and the renderer already knows how to weight both of its ranges.
// That is why the renderer needed no change for this stage.
//
// The geometry is the body's OWN local oriented bounds carried through the
// SceneObject transform — never a world axis-aligned box, which would swell and
// turn as the body rotates and would be measuring the wrong thing. The OFFSET
// that stands the annotation clear of the body is applied in WORLD units along
// the body's own unit axes, so a large scale does not push the leaders away and
// a small one does not bury them.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan type.
#pragma once

#include "forgeshape_body_dimensions.h"
#include "forgeshape_sketch_overlay.h"
#include "forgeshape_transform.h"

namespace forgeshape {

// How far the dimension line stands off the body, in gizmo REFERENCE units, so
// it reads at the same size at any zoom exactly as the sketch annotation does.
// The numbers are the sketch's, because the two annotations must read as one
// drawing convention rather than as two.
constexpr double kBodyDimensionOffsetUnits = 30.0;
constexpr double kBodyDimensionExtensionGapUnits = 6.0;
constexpr double kBodyDimensionOvershootUnits = 8.0;
constexpr double kBodyDimensionTickUnits = 7.0;

// Where the numeric label for one axis belongs: the midpoint of that axis's
// dimension line, in world space. The label is chrome — an Android view over
// the viewport — and this is the only thing it is allowed to know, so its
// position is derived from the true annotation and never guessed.
struct BodyDimensionLabelAnchors {
    bool valid = false;
    Vec3 axis[kBodyAxisCount];
};

// Builds the whole annotation for one body.
//
// `activeAxis` is 0, 1 or 2 for the axis whose value is being read or edited,
// or -1 for none: that axis is emitted in the `Dimension` range (the highlight
// weight) and the other two in `Entities` (the neutral weight), which are both
// weights the renderer already draws.
//
// Returns an empty overlay for invalid bounds, a non-finite placement or a
// non-positive `worldPerUnit`. `revision` is the caller's — the renderer
// re-uploads only when it changes, so the caller advances it when the body, the
// placement, the active axis or the camera scale moved and not once per frame.
SketchOverlayPtr buildBodyDimensionOverlay(const LocalBounds& bounds,
                                           const TransformValues& placement, int activeAxis,
                                           float worldPerUnit, uint64_t revision,
                                           BodyDimensionLabelAnchors* outAnchors = nullptr);

// The three label anchors alone, for the chrome that positions the numbers.
//
// A READ, and that is the whole point of it existing beside the builder: it
// derives the anchors for the body, the placement and the camera scale it is
// GIVEN, touching no session, no cached overlay and no revision. The chrome can
// therefore ask where a label belongs at any instant without waiting for a
// frame to have cached an answer -- which is what left the labels absent on the
// frame Dimensions opened, and standing on the PREVIOUS body after a switch.
//
// The active axis is not a parameter because an anchor does not depend on one:
// it is the midpoint of that axis's dimension line, and which range the line is
// emitted in changes how it is DRAWN and not where it is. A self-test pins that,
// because this function is only sound while it stays true.
//
// False for invalid bounds, a non-finite placement or a non-positive scale, on
// exactly the builder's terms; `out` is cleared either way.
bool bodyDimensionLabelAnchors(const LocalBounds& bounds, const TransformValues& placement,
                               float worldPerUnit, BodyDimensionLabelAnchors* out);

// The Dimensions INTERACTION state: whether the mode is open, which axis is
// being read or edited, and which side a resize holds.
//
// Session-only and native-owned, on the display settings' terms: none of it is
// project truth, none of it reaches a `.forge` byte, a checkpoint, the
// fingerprint or a history step, and no Java flag mirrors it — the shell asks
// this for its answer on every refresh, so a rotation and a resume land where
// native truth says. Nothing here can move one coordinate: the acts that CAN
// live in `forgeshape_body_dimensions.h` and are reached only by an explicit
// user Apply.
class BodyDimensionSession {
public:
    bool active() const { return active_; }
    // 0, 1 or 2, or kNoDimensionAxis when no value is being read.
    int activeAxis() const { return activeAxis_; }
    ResizeAnchor anchor() const { return anchor_; }

    // Entering resets the axis and the anchor. A mode the user re-enters must
    // not silently carry the last visit's choices: the anchor decides which way
    // the body grows, and inheriting one nobody chose this time is exactly the
    // surprise a reset prevents.
    void open();
    void close();

    // False for an out-of-range axis. kNoDimensionAxis clears the selection.
    bool setActiveAxis(int axis);
    bool setAnchor(ResizeAnchor anchor);

    // The overlay for one body's bounds and placement, at one camera scale.
    //
    // REBUILT ONLY WHEN SOMETHING IT DRAWS CHANGED, so a resting frame costs no
    // transfer: the revision advances on a real difference and on nothing else,
    // which is the same contract the sketch overlay gives the renderer.
    SketchOverlayPtr overlay(const LocalBounds& bounds, const TransformValues& placement,
                             float worldPerUnit);

    // There is deliberately NO cached label anchor here. The chrome asks
    // `bodyDimensionLabelAnchors` for the instant it is refreshing, so a
    // session cannot hold an anchor belonging to a body it no longer measures
    // or to a camera that has since moved (`UI3D-F-003`, `UI3D-F-007`).

private:
    bool active_ = false;
    int activeAxis_ = -1;
    ResizeAnchor anchor_ = ResizeAnchor::Center;

    SketchOverlayPtr overlay_;
    LocalBounds builtBounds_{};
    TransformValues builtPlacement_{};
    float builtWorldPerUnit_ = 0.0f;
    int builtAxis_ = -1;
    uint64_t revision_ = 0;
    bool built_ = false;
};

// No axis is being read. Not an enum, because it travels to the shell as an
// index and back, and -1 is what "none" already means at that boundary.
constexpr int kNoDimensionAxis = -1;

// The one process-scoped Dimensions session, beside the camera, the mesh store
// and the sketch session, so the mode survives a Surface recreation and a
// resume exactly as every other piece of native interaction state does.
BodyDimensionSession& bodyDimensionSession();

}  // namespace forgeshape
