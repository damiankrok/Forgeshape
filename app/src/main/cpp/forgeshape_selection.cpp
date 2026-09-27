#include "forgeshape_selection.h"

#include <cmath>

// Deliberately does NOT include forgeshape_construction.h: selection picking
// reads published meshes, never a Construction primitive.
#include "forgeshape_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_picking.h"
#include "forgeshape_transform.h"

namespace forgeshape {

SceneHit pickScene(const CameraSnapshot& camera, float screenX, float screenY,
                   int viewportWidth, int viewportHeight) {
    // The viewport's one list -- the SAME call the renderer draws -- so a body
    // the Sculpt Isolate leaves out is not pickable either, with no predicate
    // of its own here.
    return pickSceneSnapshot(camera, screenX, screenY, viewportWidth, viewportHeight,
                             viewSceneSnapshot());
}

SceneHit pickSceneSnapshot(const CameraSnapshot& camera, float screenX, float screenY,
                           int viewportWidth, int viewportHeight, const SceneSnapshot& scene) {
    SceneHit nearest{};

    for (const SceneDrawItem& item : scene) {
        // The bounded two-sided exception, read from THIS body's active
        // published mesh rather than from any Construction primitive.
        //
        // A flat sheet has no interior a back-face hit could wrongly reach, so
        // it alone picks from both sides; every closed solid keeps the
        // front-face-only rule. Asking `constructionObject().kind()` for this
        // was wrong before Stage 017 because a Frozen Sculpt Mesh outlives the
        // Source it was frozen from, and it would be wrong twice over now: the
        // active body's kind says nothing whatsoever about a DIFFERENT body's
        // geometry. Each item answers for itself.
        const bool frontFacesOnly = !(item.mesh != nullptr && item.mesh->renderBothSides());
        const SceneHit hit = pickMesh(camera, screenX, screenY, viewportWidth, viewportHeight,
                                      item.mesh, item.model, item.inverseModel, frontFacesOnly);
        if (!hit.hit) {
            continue;
        }
        // Nearest positive hit across the whole scene. Distances are directly
        // comparable between bodies because every Construction transform is
        // rigid, so each body's local `t` is already a world distance. Ties keep
        // the earlier body in scene order, which makes the result deterministic
        // rather than dependent on iteration accidents.
        if (!nearest.hit || hit.distance < nearest.distance) {
            nearest = hit;
        }
    }

    return nearest;
}

SceneHit pickMesh(const CameraSnapshot& camera, float screenX, float screenY, int viewportWidth,
                  int viewportHeight, const RuntimeMeshPtr& mesh, const Mat4& model,
                  const Mat4& inverseModel, bool frontFacesOnly) {
    SceneHit result{};

    Ray worldRay{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &worldRay)) {
        return result;
    }

    if (!mesh) {
        return result;  // nothing published yet: a miss, not a crash
    }

    // The published mesh is the body's LOCAL geometry, so the ray comes to it
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
