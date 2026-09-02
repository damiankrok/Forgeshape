#include "forgeshape_body_delete.h"

#include <memory>
#include <utility>

namespace forgeshape {

const char* deleteBodyStatusName(DeleteBodyStatus status) {
    switch (status) {
        case DeleteBodyStatus::Ok: return "Ok";
        case DeleteBodyStatus::UnknownBody: return "UnknownBody";
        case DeleteBodyStatus::RefusedLastBody: return "RefusedLastBody";
        case DeleteBodyStatus::RefusedEditInProgress: return "RefusedEditInProgress";
    }
    return "unknown";
}

DeleteBodyStatus deleteSceneBody(ObjectId id, ConstructionScene& scene,
                                 ConstructionHistory& history, DeleteBodyReport* outReport) {
    DeleteBodyReport report;
    report.activeBodyId = scene.activeBodyId();
    report.bodyCount = scene.bodyCount();
    const auto refuse = [&](DeleteBodyStatus status) {
        if (outReport != nullptr) {
            *outReport = report;
        }
        return status;
    };

    // Every refusal is asked BEFORE an edit is opened, so a refused delete does
    // not even leave an empty transaction behind.
    if (history.editInProgress()) {
        return refuse(DeleteBodyStatus::RefusedEditInProgress);
    }
    const size_t index = scene.indexOfBody(id);
    if (index == scene.bodyCount()) {
        return refuse(DeleteBodyStatus::UnknownBody);
    }
    if (scene.bodyCount() <= 1) {
        return refuse(DeleteBodyStatus::RefusedLastBody);
    }

    // Resolved from the list as it is NOW, while the deleted body is still in
    // it, because the rule is about the row that takes its place. After the
    // detach the index would name a different body and the "or the one before"
    // half would have nothing left to mean.
    ObjectId nextActive = scene.activeBodyId();
    if (nextActive == id) {
        nextActive = (index + 1 < scene.bodyCount()) ? scene.bodyAt(index + 1).objectId()
                                                     : scene.bodyAt(index - 1).objectId();
    }

    // ONE transaction, and always this scope's own: an edit already in progress
    // was refused above, so this can never be the joining case
    // `ScopedConstructionEdit` also supports. One Delete is therefore exactly
    // one step, recorded when this scope closes.
    {
        ScopedConstructionEdit edit(history);
        std::unique_ptr<SceneObject> removed = scene.detachBody(id);
        // Handed over rather than destroyed: the undo step recorded when this
        // scope closes names this body, and its Imported Mesh and its Frozen
        // Sculpt Mesh are the two things a step cannot rebuild.
        history.holdDetachedBody(std::move(removed));
        // detachBody's own fallback puts the selection on the first body when
        // the active one is what left; the product rule is the next row, so it
        // is stated here, inside the transaction, and the step therefore
        // carries it and an undo puts the original selection back.
        scene.setActiveBody(nextActive);
    }

    report.removedBodyId = id;
    report.removedIndex = index;
    report.activeBodyId = scene.activeBodyId();
    report.bodyCount = scene.bodyCount();
    if (outReport != nullptr) {
        *outReport = report;
    }
    return DeleteBodyStatus::Ok;
}

}  // namespace forgeshape
