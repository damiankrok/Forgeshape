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
        if (a.bodies[i].representation != b.bodies[i].representation) {
            return false;
        }
        if (!sameConstructionPlacement(a.bodies[i].transform, b.bodies[i].transform)) {
            return false;
        }
        // Shape is compared only where there is one. An Imported Mesh's
        // geometry cannot change without a new import, and an import creates a
        // body rather than reshaping one, so identity plus placement is the
        // whole of what a step can say about it.
        if (a.bodies[i].representation == BodyRepresentation::Construction
            && !sameConstructionShape(a.bodies[i].construction, b.bodies[i].construction)) {
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
        captured.representation = body.representation();
        captured.transform = body.transform().values();
        if (const ConstructionObject* source = body.constructionOrNull()) {
            captured.construction = source->captureState();
        }
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
            // Held since whatever took it out of the scene -- an undone
            // creation, or a Delete -- so it comes back with its own Frozen
            // Sculpt Mesh and its own Imported Mesh intact rather than as a new
            // object wearing the same id.
            for (auto& candidate : detached_) {
                if (candidate && candidate->objectId() == wanted.objectId) {
                    body = std::move(candidate);
                    break;
                }
            }
        }
        if (!body) {
            if (wanted.representation != BodyRepresentation::Construction) {
                // An Imported Mesh cannot be fabricated from a step: its
                // geometry is not derived from anything a step holds. It can
                // only come back as the object that was taken out, and
                // `pruneDetachedBodies` keeps exactly the ones some step still
                // names -- so reaching here would mean the step and the
                // detached pool disagreed. Skip rather than invent an empty
                // body wearing an imported object's identity.
                continue;
            }
            body = scene_.makeBody(wanted.objectId);
        }

        const bool isConstruction = body->hasConstructionSource()
            && wanted.representation == BodyRepresentation::Construction;
        const ConstructionObjectState current =
            isConstruction ? body->construction().captureState() : ConstructionObjectState{};
        const bool shapeDiffers =
            isConstruction && !sameConstructionShape(current, wanted.construction);
        const bool placementDiffers =
            !sameConstructionPlacement(body->transform().values(), wanted.transform);
        // A body that has never published anything must, whatever its
        // parameters are — but a body coming BACK from the history still holds
        // its own published revision, and republishing identical geometry to
        // put it back on screen would be exactly the redundant work a
        // transaction boundary exists to remove.
        const bool mustPublish =
            shapeDiffers || body->meshStore().currentRevision() == kNoMeshRevision;

        if (shapeDiffers) {
            body->construction().restoreState(wanted.construction);
        }
        if (placementDiffers) {
            // The placement is the BODY's, so it is restored for an Imported
            // Mesh exactly as for a Construction Body — which is the whole
            // point of hoisting it out of the Construction Source.
            body->transform().setValues(wanted.transform);
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
            // for a body whose shape actually differs or that has never
            // published. A placement-only change publishes nothing, exactly as
            // an ordinary transform Apply publishes nothing.
            //
            // Dispatched per representation: a Construction Body regenerates
            // from its parameters, an Imported Mesh republishes the geometry it
            // already owns. Neither reads the other's truth.
            publishSceneObject(*body);
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

void ConstructionHistory::holdDetachedBody(std::unique_ptr<SceneObject> body) {
    if (!body) {
        return;
    }
    detached_.push_back(std::move(body));
}

void ConstructionHistory::pruneDetachedBodies() {
    // Whether any step in a stack names this body, on EITHER side.
    //
    // Both sides matter, and each for its own act: a redo restores an undone
    // creation from a step's AFTER state, and an undo restores a deleted body
    // from a step's BEFORE state. Asking only the forward side was correct
    // while an undone creation was the only way a body could leave the scene.
    const auto namedBy = [](const std::deque<HistoryEntry>& stack, ObjectId id) {
        for (const HistoryEntry& entry : stack) {
            for (const BodyConstructionState& state : entry.before.bodies) {
                if (state.objectId == id) {
                    return true;
                }
            }
            for (const BodyConstructionState& state : entry.after.bodies) {
                if (state.objectId == id) {
                    return true;
                }
            }
        }
        return false;
    };
    detached_.erase(
        std::remove_if(detached_.begin(), detached_.end(),
                       [this, &namedBy](const std::unique_ptr<SceneObject>& body) {
                           if (!body) {
                               return true;
                           }
                           return !namedBy(undoStack_, body->objectId())
                                  && !namedBy(redoStack_, body->objectId());
                       }),
        detached_.end());
}

ConstructionHistory& constructionHistory() {
    static ConstructionHistory history(constructionScene());
    return history;
}

}  // namespace forgeshape
