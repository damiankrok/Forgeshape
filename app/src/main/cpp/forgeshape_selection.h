// ForgeShape selection ownership.
//
// This module is the single source of truth for "what is selected". Neither the
// renderer nor the CameraController owns selected identity: the renderer is told
// only whether to draw the highlight, and the camera is told nothing at all.
//
// It also owns the tap-versus-navigation decision, because that decision is the
// only thing that may turn a touch gesture into a selection change. It contains
// no JNI, Android or Vulkan types.
//
// Stage 005 is a selection FOUNDATION: there is exactly one selectable object,
// no hierarchy, no scene graph and no multi-select.
#pragma once

#include <cstdint>

#include "forgeshape_camera.h"
#include "forgeshape_input.h"
#include "forgeshape_math.h"
#include "forgeshape_object_id.h"

namespace forgeshape {

// `ObjectId`, `kNoObject` and `kDemoCubeObjectId` live in
// forgeshape_object_id.h so the mesh layer can name an identity without
// depending on camera or input types. The RULES below are still owned here.
//
// Identity is minted by this layer and nothing else: it is NOT a pointer, NOT a
// list index, NOT a Vulkan or renderer handle and NOT a display name, so it
// stays valid when GPU buffers are recreated, when the Surface is destroyed and
// rebuilt, and when a new mesh revision replaces the geometry entirely.

// A single-finger gesture stays a tap candidate only while its total travel
// from the ORIGINAL down position stays inside this radius. Beyond it the
// gesture is orbit-only and can no longer select or clear.
constexpr float kTapSlopPixels = 24.0f;

// Result of resolving a screen point against the selectable scene.
struct SceneHit {
    bool hit = false;
    ObjectId objectId = kNoObject;
    int triangleIndex = -1;
    float distance = 0.0f;
    Vec3 position{0.0f, 0.0f, 0.0f};
};

// Resolves a view-local screen point against everything selectable, using CPU
// ray/triangle picking and the canonical front-face convention (so only surfaces
// the rasterizer actually draws can be hit) — EXCEPT for the one bounded,
// explicitly named case a flat, zero-thickness Construction Plane needs: it has
// no "inside" a two-sided pick could wrongly reach, unlike a closed solid, so
// the active primitive being Plane picks both its front and its back. Every
// other primitive keeps the ordinary front-face-only rule.
//
// The ray is carried into the object's local space by the current Construction
// transform and intersected against the unchanged local mesh, so what is
// pickable is the box where it actually appears. `position` is reported back in
// WORLD space; `distance` needs no conversion because the transform is rigid.
SceneHit pickScene(const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight);

// Same, against an explicitly supplied transform pair and an explicit
// front-face-only choice, instead of the process-scoped ConstructionTransform
// and ConstructionObject::kind() the overload above reads. Exists so
// self-tests that already publish their own known mesh into the (still
// process-scoped) MeshStore can pick it without also depending on whatever
// transform or primitive kind a prior UI test or Activity recreation left
// live. `frontFacesOnly` defaults to true — the ordinary closed-solid rule —
// so existing callers are unaffected.
SceneHit pickScene(const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight, const Mat4& model,
                   const Mat4& inverseModel, bool frontFacesOnly = true);

class SelectionController {
public:
    // Feeds the same platform-neutral event the CameraController receives.
    //
    // Returns true only when this event completes a valid tap and the caller
    // should therefore resolve a pick at (*outX, *outY). It never returns true
    // for a gesture that orbited past the slop radius, ever had two or more
    // pointers, or was cancelled.
    bool onTouch(TouchAction action, int32_t actionPointerId,
                 const TouchPointer* pointers, int count, float* outX, float* outY);

    // Drops tap candidacy without touching the selected identity. Called on
    // ACTION_CANCEL and whenever the Surface goes away.
    void resetGesture();

    // Applies the outcome of a resolved tap: a hit selects, a miss clears.
    // Returns true when the selected identity actually changed.
    bool applyPick(const SceneHit& hit);

    bool setSelected(ObjectId id);
    bool clearSelection();

    ObjectId selected() const { return selected_; }
    bool hasSelection() const { return selected_ != kNoObject; }
    bool isSelected(ObjectId id) const { return id != kNoObject && id == selected_; }

    // --- introspection (logging and self-tests only) ---
    bool tapCandidateActive() const { return tapValid_; }
    uint32_t changeCount() const { return changeCount_; }

private:
    void beginCandidate(const TouchPointer& p);
    void cancelCandidate();
    // Returns nullptr when the tracked pointer is absent from this event.
    const TouchPointer* findTracked(const TouchPointer* pointers, int count) const;

    ObjectId selected_ = kNoObject;
    uint32_t changeCount_ = 0;

    bool tapValid_ = false;
    int32_t tapPointerId_ = -1;
    float downX_ = 0.0f;  // ORIGINAL down position; displacement is measured
    float downY_ = 0.0f;  // from here, never from the previous move
    float lastX_ = 0.0f;
    float lastY_ = 0.0f;
};

}  // namespace forgeshape
