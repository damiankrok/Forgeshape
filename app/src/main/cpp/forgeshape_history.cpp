#include "forgeshape_history.h"

#include <algorithm>
#include <utility>

#include "forgeshape_mesh.h"

namespace forgeshape {

bool sameSceneConstructionState(const SceneConstructionState& a,
                                const SceneConstructionState& b) {
    // `activeBodyId` is deliberately NOT compared.
    //
    // It is carried by a step so that a restore can put the user back on the
    // body the step was about — undoing a creation has to leave a valid
    // selection, and redoing one has to re-select what came back. But selection
    // is not itself an edit: picking a different body in the Objects list
    // changes nothing about the model, and if it were part of this comparison
    // every tap on the list would commit a history step the user could then
    // "undo" to no visible effect.
    if (a.bodies.size() != b.bodies.size()) {
        return false;
    }
    for (size_t i = 0; i < a.bodies.size(); ++i) {
        // Order is compared positionally on purpose: the Objects list is
        // insertion-ordered and a step that reordered the scene would be a real
        // change even if every body's own state matched.
        if (a.bodies[i].objectId != b.bodies[i].objectId) {
            return false;
        }
        if (!sameConstructionShape(a.bodies[i].construction, b.bodies[i].construction)
            || !sameConstructionPlacement(a.bodies[i].construction, b.bodies[i].construction)) {
            return false;
        }
    }
    return true;
}

SceneConstructionState captureSceneConstructionState(const ConstructionScene& scene) {
    SceneConstructionState state;
    state.bodies.reserve(scene.bodyCount());
    for (size_t i = 0; i < scene.bodyCount(); ++i) {
        const SceneObject& body = scene.bodyAt(i);
        BodyConstructionState captured;
        captured.objectId = body.objectId();
        captured.construction = body.construction().captureState();
        state.bodies.push_back(captured);
    }
    state.activeBodyId = scene.activeBodyId();
    return state;
}

// ---------------------------------------------------------------------------
// The transaction boundary
// ---------------------------------------------------------------------------

bool ConstructionHistory::beginEdit() {
    if (editOpen_) {
        return false;
    }
    editPreState_ = captureSceneConstructionState(scene_);
    editOpen_ = true;
    return true;
}

bool ConstructionHistory::commitEdit() {
    if (!editOpen_) {
        return false;
    }
    editOpen_ = false;
    if (sessionInitializing_) {
        // Seeding a session is not a user act. The edit is CLOSED — so nothing
        // is left open for the first real edit to collide with — and the live
        // scene keeps whatever the seed just did to it, because suppressing a
        // recording is not undoing a mutation.
        editPreState_ = SceneConstructionState{};
        return false;
    }
    SceneConstructionState after = captureSceneConstructionState(scene_);
    if (sameSceneConstructionState(editPreState_, after)) {
        // Nothing happened, so nothing is recorded and — the part that matters —
        // the redo stack is left exactly as it was. A refused Apply is not a new
        // branch of history.
        return false;
    }

    HistoryEntry entry;
    entry.before = std::move(editPreState_);
    entry.after = std::move(after);
    undoStack_.push_back(std::move(entry));
    while (undoStack_.size() > kConstructionHistoryCapacity) {
        undoStack_.pop_front();  // deterministic: oldest first, one per commit
    }

    // A real new edit is what invalidates the forward branch, and the only
    // thing that does.
    redoStack_.clear();
    pruneDetachedBodies();
    return true;
}

void ConstructionHistory::cancelEdit(ConstructionRestoreReport* outReport) {
    if (!editOpen_) {
        return;
    }
    editOpen_ = false;
    applyState(editPreState_, outReport);
}

// ---------------------------------------------------------------------------
// The session initialization boundary
// ---------------------------------------------------------------------------

void ConstructionHistory::beginSessionInitialization() {
    sessionInitializing_ = true;
}

void ConstructionHistory::endSessionInitialization() {
    sessionInitializing_ = false;
    // A seed that left an edit open would hand the first user act a transaction
    // it did not open. Close it the way a cancelled one closes — recording
    // nothing — rather than leaving the flag to decide.
    editOpen_ = false;
    editPreState_ = SceneConstructionState{};
    // The postcondition, stated rather than assumed: a session that has just
    // started has nothing to go back to and nothing to go forward to. The scene
    // itself is untouched; forgetting how to go back is not going back.
    undoStack_.clear();
    redoStack_.clear();
    detached_.clear();
}

// ---------------------------------------------------------------------------
// Undo and redo
// ---------------------------------------------------------------------------

bool ConstructionHistory::undo(ConstructionRestoreReport* outReport) {
    if (!canUndo()) {
        return false;
    }
    HistoryEntry entry = std::move(undoStack_.back());
    undoStack_.pop_back();
    // Moved onto the redo stack BEFORE the restore runs, because the restore is
    // what detaches the bodies this step's forward state still names — and a
    // detached body is kept alive by exactly that reference.
    redoStack_.push_back(std::move(entry));
    applyState(redoStack_.back().before, outReport);
    return true;
}

bool ConstructionHistory::redo(ConstructionRestoreReport* outReport) {
    if (!canRedo()) {
        return false;
    }
    HistoryEntry entry = std::move(redoStack_.back());
    redoStack_.pop_back();
    undoStack_.push_back(std::move(entry));
    applyState(undoStack_.back().after, outReport);
    while (undoStack_.size() > kConstructionHistoryCapacity) {
        undoStack_.pop_front();
    }
    return true;
}

void ConstructionHistory::clear() {
    undoStack_.clear();
    redoStack_.clear();
    detached_.clear();
    editOpen_ = false;
    editPreState_ = SceneConstructionState{};
}

// ---------------------------------------------------------------------------
// Restore
// ---------------------------------------------------------------------------

void ConstructionHistory::applyState(const SceneConstructionState& target,
                                     ConstructionRestoreReport* outReport) {
    ConstructionRestoreReport report;
    const ObjectId activeBefore = scene_.activeBodyId();

    // Take every body out first, then put back exactly the ones the target
    // names, in the target's order. Rebuilding the order rather than trying to
    // patch it is what makes scene position exact for free — including the case
    // where a body has to reappear in the middle of the list.
    std::vector<std::unique_ptr<SceneObject>> held;
    held.reserve(scene_.bodyCount());
    while (scene_.bodyCount() > 0) {
        held.push_back(scene_.detachBody(scene_.bodyAt(0).objectId()));
    }

    for (size_t i = 0; i < target.bodies.size(); ++i) {
        const BodyConstructionState& wanted = target.bodies[i];
        std::unique_ptr<SceneObject> body;
        bool wasInScene = false;

        for (auto& candidate : held) {
            if (candidate && candidate->objectId() == wanted.objectId) {
                body = std::move(candidate);
                wasInScene = true;
                break;
            }
        }
        if (!body) {
            // Held for a redo since the undo that took it out of the scene, so
            // it comes back with its own Frozen Sculpt Mesh rather than as a
            // new object wearing the same id.
            for (auto& candidate : detached_) {
                if (candidate && candidate->objectId() == wanted.objectId) {
                    body = std::move(candidate);
                    break;
                }
            }
        }
        if (!body) {
            body = scene_.makeBody(wanted.objectId);
        }

        const ConstructionObjectState current = body->construction().captureState();
        const bool shapeDiffers = !sameConstructionShape(current, wanted.construction);
        const bool placementDiffers = !sameConstructionPlacement(current, wanted.construction);
        // A body that has never published anything must, whatever its
        // parameters are — but a body coming BACK from the history still holds
        // its own published revision, and republishing identical geometry to
        // put it back on screen would be exactly the redundant work a
        // transaction boundary exists to remove.
        const bool mustPublish =
            shapeDiffers || body->meshStore().currentRevision() == kNoMeshRevision;

        if (shapeDiffers || placementDiffers) {
            body->construction().restoreState(wanted.construction);
        }
        if (shapeDiffers) {
            // The same stale-source bookkeeping an ordinary Construction edit
            // performs, and for the same reason: a Frozen Sculpt Mesh is never
            // re-derived or replaced behind the user's back, it is only marked
            // as having been frozen from a source that has since moved.
            body->frozenSculpt().markSourceStale();
            ++report.republishedBodies;
        }
        if (placementDiffers) {
            ++report.replacedPlacements;
        }
        if (!wasInScene) {
            ++report.restoredBodies;
        }
        if (mustPublish) {
            // Publication is the ONLY geometry work a restore does, and only
            // for a body whose shape actually differs. A placement-only change
            // publishes nothing, exactly as an ordinary transform Apply
            // publishes nothing.
            publishConstructionObject(body->construction(), body->meshStore());
        }

        scene_.insertBody(std::move(body), i);
    }

    for (auto& leftover : held) {
        if (leftover) {
            ++report.removedBodies;
            detached_.push_back(std::move(leftover));
        }
    }
    // Anything still in `detached_` that no redo step names again can never
    // come back, so it is released rather than accumulating for the session.
    pruneDetachedBodies();

    scene_.setActiveBody(target.activeBodyId);
    report.activeBodyChanged = scene_.activeBodyId() != activeBefore;

    if (outReport != nullptr) {
        *outReport = report;
    }
}

void ConstructionHistory::pruneDetachedBodies() {
    detached_.erase(
        std::remove_if(detached_.begin(), detached_.end(),
                       [this](const std::unique_ptr<SceneObject>& body) {
                           if (!body) {
                               return true;
                           }
                           for (const HistoryEntry& entry : redoStack_) {
                               for (const BodyConstructionState& state : entry.after.bodies) {
                                   if (state.objectId == body->objectId()) {
                                       return false;
                                   }
                               }
                           }
                           return true;
                       }),
        detached_.end());
}

ConstructionHistory& constructionHistory() {
    static ConstructionHistory history(constructionScene());
    return history;
}

}  // namespace forgeshape
