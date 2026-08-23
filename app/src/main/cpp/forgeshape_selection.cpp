#include "forgeshape_selection.h"

#include <cmath>

// Deliberately does NOT include forgeshape_construction.h: selection picking
// reads the active published mesh, never the Construction primitive.
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_transform.h"

namespace forgeshape {

SceneHit pickScene(const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight) {
    // The bounded two-sided exception, read from the ACTIVE PUBLISHED MESH
    // rather than from the current Construction primitive.
    //
    // A flat sheet has no interior a back-face hit could wrongly reach, so it
    // alone picks from both sides; every closed solid keeps the front-face-only
    // rule. The distinction is a fact about the geometry actually on screen, and
    // asking `constructionObject().kind()` for it was wrong in both directions
    // once a Frozen Sculpt Mesh can outlive the Source it was frozen from: a
    // frozen solid would start picking from behind the moment the Source was
    // changed to a Plane, and a frozen Plane would stop picking from behind the
    // moment the Source was changed to a solid — in neither case did the mesh
    // being picked change at all. Reading the published RuntimeMesh keeps
    // picking, rendering and the Sculpt hit-test on one answer.
    const RuntimeMeshPtr active = meshStore().current();
    const bool twoSided = (active != nullptr) && active->renderBothSides();
    return pickScene(camera, screenX, screenY, viewportWidth, viewportHeight,
                     constructionTransform().modelMatrix(),
                     constructionTransform().inverseModelMatrix(), !twoSided);
}

SceneHit pickScene(const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight, const Mat4& model,
                   const Mat4& inverseModel, bool frontFacesOnly) {
    SceneHit result{};

    Ray worldRay{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &worldRay)) {
        return result;
    }

    // Picking reads the CURRENT CPU mesh revision, never Vulkan buffer memory.
    // Holding the snapshot keeps that revision alive for the whole intersection,
    // even if a newer revision is published concurrently.
    const RuntimeMeshPtr mesh = meshStore().current();
    if (!mesh) {
        return result;  // nothing published yet: a miss, not a crash
    }

    // The published mesh is the box's LOCAL geometry, so the ray comes to it
    // rather than the geometry going to the ray. This is what lets a transform
    // change cost no mesh revision: the vertices the picker reads are the same
    // vertices the GPU already has.
    Ray localRay{};
    if (!transformRayToLocal(worldRay, inverseModel, &localRay)) {
        return result;
    }

    // Front faces only, so picking sees exactly the surfaces the rasterizer
    // draws under VK_CULL_MODE_BACK_BIT — except when the caller has passed the
    // bounded two-sided exception for a flat Construction Plane. The rigid
    // transform preserves winding (it has no reflection), so front-facing is
    // the same fact in either space.
    const TriangleHit hit = pickTriangleMesh(localRay, mesh->triangleView(), frontFacesOnly);
    if (!hit.hit) {
        return result;
    }

    result.hit = true;
    // Identity comes from the store, not from the mesh geometry or from any GPU
    // resource, so replacing the mesh cannot change what gets selected.
    result.objectId = mesh->objectId();
    result.triangleIndex = hit.triangleIndex;
    // The transform is rigid, so the local distance is already the world
    // distance; only the hit POINT has to be carried back for reporting.
    result.distance = hit.t;
    result.position = mat4TransformPoint(model, hit.position);
    return result;
}

// ---------------------------------------------------------------------------
// Tap versus navigation
//
// The rules are deliberately one-way: candidacy can only ever be revoked during
// a gesture, never restored. A gesture that orbited past the slop radius, or
// that was ever multi-touch, or that was cancelled, cannot select or clear on
// release. Only a fresh Down starts a new candidate.
// ---------------------------------------------------------------------------

void SelectionController::beginCandidate(const TouchPointer& p) {
    tapValid_ = true;
    tapPointerId_ = p.id;
    downX_ = p.x;
    downY_ = p.y;
    lastX_ = p.x;
    lastY_ = p.y;
}

void SelectionController::cancelCandidate() {
    tapValid_ = false;
    tapPointerId_ = -1;
}

const TouchPointer* SelectionController::findTracked(const TouchPointer* pointers,
                                                     int count) const {
    // Pointer ID is authoritative; MotionEvent array order is not.
    for (int i = 0; i < count; ++i) {
        if (pointers[i].id == tapPointerId_) {
            return &pointers[i];
        }
    }
    return nullptr;
}

void SelectionController::resetGesture() {
    cancelCandidate();
    downX_ = downY_ = lastX_ = lastY_ = 0.0f;
}

bool SelectionController::onTouch(TouchAction action, int32_t actionPointerId,
                                  const TouchPointer* pointers, int count,
                                  float* outX, float* outY) {
    if (action == TouchAction::Cancel) {
        cancelCandidate();
        return false;
    }
    if (pointers == nullptr || count <= 0) {
        cancelCandidate();
        return false;
    }

    // Any moment at which two or more pointers are present makes the whole
    // gesture navigation-only, and PointerDown/PointerUp only ever happen in
    // multi-touch gestures.
    if (count >= 2 || action == TouchAction::PointerDown || action == TouchAction::PointerUp) {
        cancelCandidate();
        return false;
    }

    switch (action) {
        case TouchAction::Down: {
            const TouchPointer& p = pointers[0];
            if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                cancelCandidate();
                return false;
            }
            beginCandidate(p);  // a new gesture always starts from a clean slate
            return false;
        }

        case TouchAction::Move: {
            if (!tapValid_) {
                return false;
            }
            const TouchPointer* p = findTracked(pointers, count);
            if (p == nullptr || !std::isfinite(p->x) || !std::isfinite(p->y)) {
                cancelCandidate();
                return false;
            }
            lastX_ = p->x;
            lastY_ = p->y;
            // Displacement is measured from the ORIGINAL down position, so a
            // slow drag that never exceeds the slop per-move still cancels.
            const float dx = p->x - downX_;
            const float dy = p->y - downY_;
            if ((dx * dx + dy * dy) > (kTapSlopPixels * kTapSlopPixels)) {
                cancelCandidate();
            }
            return false;
        }

        case TouchAction::Up: {
            const bool wasValid = tapValid_;
            const TouchPointer* p = findTracked(pointers, count);
            const bool samePointer = (p != nullptr) && (actionPointerId < 0 ||
                                                        actionPointerId == tapPointerId_);
            float x = lastX_;
            float y = lastY_;
            if (p != nullptr && std::isfinite(p->x) && std::isfinite(p->y)) {
                x = p->x;
                y = p->y;
                const float dx = x - downX_;
                const float dy = y - downY_;
                if ((dx * dx + dy * dy) > (kTapSlopPixels * kTapSlopPixels)) {
                    cancelCandidate();
                    return false;
                }
            }
            cancelCandidate();  // the gesture is over either way
            if (!wasValid || !samePointer) {
                return false;
            }
            if (outX != nullptr) *outX = x;
            if (outY != nullptr) *outY = y;
            return true;
        }

        default:
            cancelCandidate();
            return false;
    }
}

// ---------------------------------------------------------------------------
// Selected identity
// ---------------------------------------------------------------------------

bool SelectionController::setSelected(ObjectId id) {
    if (id == selected_) {
        return false;
    }
    selected_ = id;
    ++changeCount_;
    return true;
}

bool SelectionController::clearSelection() { return setSelected(kNoObject); }

bool SelectionController::applyPick(const SceneHit& hit) {
    // A valid tap that misses selectable geometry clears the selection.
    return setSelected(hit.hit ? hit.objectId : kNoObject);
}

}  // namespace forgeshape
