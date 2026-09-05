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
// Exactly one body is selected at a time (`kNoObject` for none): no hierarchy,
// no scene graph and no multi-select.
#pragma once

#include <cstdint>

#include "forgeshape_camera.h"
#include "forgeshape_input.h"
#include "forgeshape_math.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"

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

// Intersects ONE explicitly supplied published mesh. The shared core all three
// entry points above ultimately run, exposed so scene picking can drive it once
// per body without re-reading any process-scoped state.
SceneHit pickMesh(const CameraSnapshot& camera, float screenX, float screenY, int viewportWidth,
                  int viewportHeight, const RuntimeMeshPtr& mesh, const Mat4& model,
                  const Mat4& inverseModel, bool frontFacesOnly = true);

// Nearest positive hit across an explicitly supplied scene snapshot.
//
// Each item is intersected with ITS OWN transform and ITS OWN sidedness, and
// the nearest hit wins; ties keep the earlier body in scene order. Taking the
// snapshot as a parameter is what lets a self-test pick a scene it built itself
// and keeps the caller free to take the snapshot under the state mutex and then
// scan triangles with that mutex released.
SceneHit pickSceneSnapshot(const CameraSnapshot& camera, float screenX, float screenY,
                           int viewportWidth, int viewportHeight, const SceneSnapshot& scene);

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
