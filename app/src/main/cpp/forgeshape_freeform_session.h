// The Freeform edit session (`MODELING-FOUNDATIONS-R1` B): what a finger on a
// Freeform body's control cage means while the body is being edited.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan. Volatile in the
// strongest sense: the element mode, the selection, the multi-select toggle
// and the transform mode are PRESENTATION -- never serialized, never a history
// step, never in the fingerprint. The CAGE is the only truth, and every act
// that changes it is ONE `ScopedConstructionEdit` (one Undo), whether it is a
// typed Push/Pull or a whole gizmo drag of a thousand samples.
//
// Picking is SEMANTIC: a tap resolves to a cage vertex, edge or face by its
// stable id -- vertices and edges by their projected distance in pixels, a face
// through the derived surface's per-triangle CONTROL face attribution. A
// derived triangle index is never an identity.
//
// The transform instrument is the gizmo's own (`GizmoSnapshot`, its hit test,
// its pure solvers), placed at the selection's centroid and pointing along the
// body's own axes -- the axes the cage and its symmetry planes are written in.
// One captured pointer, a basis frozen at the down, a second pointer or a
// Cancel restoring the pre-drag cage and recording nothing, a degenerate
// sample holding the last good cage.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_freeform.h"
#include "forgeshape_freeform_subdivision.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch_overlay.h"

namespace forgeshape {

// The pick radius for a cage vertex or edge, in gizmo reference units (the
// 48 dp touch floor's half).
constexpr float kFreeformPickRadiusUnits = 24.0f;

// How far a finger may travel and still be a TAP, in reference units: the
// sketch's own tap slop.
constexpr float kFreeformTapSlopUnits = 24.0f;

// The overlay's own revision space, so a frame that switches from another
// producer's overlay to this one can never be mistaken for one already
// uploaded.
constexpr uint64_t kFreeformOverlayRevisionBase = 0xF0F0000000000000ull;

// Semantic picking over a cage, as a pure function. `model` is the body's world
// model; `derived` (may be null) is the smooth surface faces are picked
// through. Returns false when nothing is within reach.
bool pickFreeformElement(const FreeformCage& cage, const FreeformMesh* derived, const Mat4& model,
                         const CameraSnapshot& camera, float screenX, float screenY,
                         int viewportWidth, int viewportHeight, FreeformElement element,
                         float radiusPixels, uint32_t* outId);

// The cage drawn over the smooth surface: every edge, the vertices as small
// crosses in Vertex mode, and the selection (with its mirror images) in the
// highlight colour. A pure function over values.
SketchOverlayPtr buildFreeformCageOverlay(const FreeformCage& cage, const Mat4& model,
                                          FreeformElement element,
                                          const std::vector<uint32_t>& selection,
                                          float worldPerUnit, uint64_t revision);

// What one touch event did, for the shell's arbitration and its logs.
struct FreeformTouchResult {
    // The event belonged to the session: neither the camera nor the selection
    // may see it.
    bool consumed = false;
    bool tapResolved = false;   // a tap was resolved (it may have selected nothing)
    bool dragBegan = false;
    bool dragMoved = false;
    bool dragCommitted = false;  // a step was recorded
    bool dragCancelled = false;
};

class FreeformEditSession {
public:
    FreeformEditSession(ConstructionScene& scene, ConstructionHistory& history)
        : scene_(scene), history_(history) {}

    FreeformEditSession(const FreeformEditSession&) = delete;
    FreeformEditSession& operator=(const FreeformEditSession&) = delete;

    // Opens editing on `id`, which must be a visible, unlocked Freeform body.
    // Opening resets the element mode to Face, the transform to Move and the
    // selection to empty, every time: a tool that reopened in whatever state it
    // was last left in would make the first tap a guess.
    FreeformStatus begin(ObjectId id);
    // Closes editing, cancelling any drag in progress.
    void end();
    bool active() const { return active_; }
    ObjectId bodyId() const { return bodyId_; }

    // Re-validates the session against the scene: the body gone, no longer
    // Freeform, hidden or locked, or another body made active, ends it; ids the
    // cage no longer has leave the selection. Called on every read and before
    // every act, so an Undo or a Delete from anywhere can never leave the
    // session naming something that is not there.
    void reconcile();

    FreeformStatus setElement(FreeformElement element);
    FreeformElement element() const { return element_; }
    void setMultiSelect(bool multi);
    bool multiSelect() const { return multiSelect_; }
    FreeformStatus setTransformMode(GizmoMode mode);
    GizmoMode transformMode() const { return mode_; }

    const std::vector<uint32_t>& selection() const { return selection_; }
    // Replaces the selection with `ids` of the current element kind; unknown ids
    // are refused by name and change nothing.
    FreeformStatus select(const std::vector<uint32_t>& ids);
    void clearSelection();

    // A tap at a pixel: picks the element under it and selects it (or, with
    // multi-select on, toggles it). A tap on nothing clears a single selection.
    FreeformStatus tap(const CameraSnapshot& camera, float screenX, float screenY,
                       int viewportWidth, int viewportHeight, bool* outHit = nullptr);

    // The whole touch rule (see the file comment): a Down on a gizmo handle
    // captures it; any other single-finger Down arms a tap that a travel past
    // the slop disarms into navigation; two fingers navigate and cancel.
    FreeformTouchResult onTouch(TouchAction action, int32_t actionPointerId,
                                const TouchPointer* pointers, int count,
                                const CameraSnapshot& camera, int viewportWidth,
                                int viewportHeight);
    bool tapArmed() const { return tapArmed_; }

    // The instrument: visible with a non-empty selection.
    GizmoSnapshot gizmoSnapshot(const CameraSnapshot& camera, int viewportWidth,
                                int viewportHeight) const;
    bool beginDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight);
    bool updateDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX, float screenY,
                    int viewportWidth, int viewportHeight);
    bool commitDrag();
    void cancelDrag();
    bool capturing() const { return capturing_; }
    uint64_t committedDragCount() const { return committedDrags_; }

    // The typed tools, each ONE transaction over the current selection.
    FreeformStatus pushPull(double distance);
    FreeformStatus extrude(double distance);
    FreeformStatus insertLoop(double ratio);
    FreeformStatus setCrease(double weight);
    FreeformStatus deleteFaces();
    FreeformStatus setSymmetry(uint8_t symmetry);
    FreeformStatus setLevel(int level);

    // The status of the last act the session refused or applied.
    FreeformStatus lastStatus() const { return lastStatus_; }

    // The overlay for this frame, rebuilt only when what it draws changed.
    SketchOverlayPtr overlay(float worldPerUnit);

    // The body being edited, or null.
    SceneObject* body() const;

    // The selection's centroid in body-local space; false with no selection.
    bool selectionCentroid(DVec3* out) const;

    // The body's world model; false when there is none.
    bool modelMatrix(Mat4* out) const;

private:
    // Applies `next` to the body inside the CURRENT edit (opened by the caller)
    // and republishes; returns the cage's own refusal.
    FreeformStatus applyLocked(const FreeformCage& next, bool* outChanged = nullptr);
    // One whole act: opens one edit, runs `tool` on the current cage, applies.
    template <typename Tool>
    FreeformStatus runTool(Tool tool);
    FreeformStatus usable() const;
    void pruneSelection();

    ConstructionScene& scene_;
    ConstructionHistory& history_;

    bool active_ = false;
    ObjectId bodyId_ = kNoObject;
    FreeformElement element_ = FreeformElement::Face;
    bool multiSelect_ = false;
    GizmoMode mode_ = GizmoMode::Move;
    std::vector<uint32_t> selection_;
    FreeformStatus lastStatus_ = FreeformStatus::Ok;

    // --- the armed tap ---------------------------------------------------
    bool tapArmed_ = false;
    int32_t tapPointerId_ = -1;
    float tapDownX_ = 0.0f;
    float tapDownY_ = 0.0f;
    CameraSnapshot tapCamera_{};

    // --- the drag, valid only while capturing_ -----------------------------
    bool capturing_ = false;
    int32_t pointerId_ = -1;
    GizmoHandle handle_ = GizmoHandle::None;
    GizmoMode dragMode_ = GizmoMode::Move;
    std::shared_ptr<const FreeformCage> startCage_;
    std::vector<uint32_t> dragSelection_;
    FreeformElement dragElement_ = FreeformElement::Face;
    DVec3 localPivot_{0.0, 0.0, 0.0};
    Vec3 pivot_{0.0f, 0.0f, 0.0f};
    Vec3 axes_[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    double bodyScale_[3] = {1.0, 1.0, 1.0};
    float startAxisT_ = 0.0f;
    Vec3 startPlaneHit_{0.0f, 0.0f, 0.0f};
    Vec3 ringNormal_{0.0f, 1.0f, 0.0f};
    float lastRingAngle_ = 0.0f;
    float accumulatedAngle_ = 0.0f;
    float scaleDownX_ = 0.0f;
    float scaleDownY_ = 0.0f;
    float scaleDirX_ = 1.0f;
    float scaleDirY_ = 0.0f;
    float scaleReferencePixels_ = 1.0f;
    uint64_t committedDrags_ = 0;

    // --- the overlay cache -------------------------------------------------
    SketchOverlayPtr overlay_;
    std::shared_ptr<const FreeformCage> overlayCage_;
    Mat4 overlayModel_ = mat4Identity();
    FreeformElement overlayElement_ = FreeformElement::Face;
    std::vector<uint32_t> overlaySelection_;
    float overlayWorldPerUnit_ = 0.0f;
    uint64_t overlayRevision_ = kFreeformOverlayRevisionBase;
};

// The one process-scoped session, over the one process-scoped scene and history.
FreeformEditSession& freeformEditSession();

// Creates a Freeform body of a creation form (0 Box, 1 Plane, 2 Cylinder) as
// ONE transaction and makes it active. Inside the session-initialization
// bracket it seeds a new project instead, recording nothing.
FreeformStatus createFreeformBody(ConstructionScene& scene, ConstructionHistory& history, int form,
                                  ObjectId* outId = nullptr);

}  // namespace forgeshape
