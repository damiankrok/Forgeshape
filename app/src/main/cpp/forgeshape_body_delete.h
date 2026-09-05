// Removing one body from the project (`UI-OWNER-45`).
//
// Its own module rather than a method on either collaborator: a delete is a
// decision ABOUT the project that needs both the scene (which detaches) and the
// history (which records), and neither owns the other.
//
// Invariants:
//   * REPRESENTATION-NEUTRAL. Nothing here asks what a body is. A Construction
//     Body, an Imported Mesh, a CAD Body, and any of them carrying a Frozen
//     Sculpt Mesh leave as one whole object -- identity, representation,
//     placement, published mesh and sculpt state together -- so no per-kind
//     path can bring one back missing a part.
//   * THE BODY IS NOT DESTROYED. It is handed to the history, which holds it
//     while any step names it: an Imported Mesh's geometry and a Frozen Sculpt
//     Mesh cannot be rebuilt from step state, so Undo must restore the SAME
//     object and Redo remove that same object again.
//   * ONE TRANSACTION. One Delete is one `ScopedConstructionEdit` that OWNS
//     the edit; an edit already open is refused (`RefusedEditInProgress`), as
//     `loadProjectDocument` and `commitImportedGlbScene` refuse it.
//   * THE LAST BODY IS REFUSED (`RefusedLastBody`). A project is never empty:
//     an empty scene is Home (`APP-H1`), reached only by closing the project,
//     and `validateProjectDocument` refuses a zero-body file. Nothing invents
//     a replacement primitive.
//   * A PRODUCER WITH FACE-SUPPORTED DEPENDENTS IS REFUSED
//     (`RefusedHasDependents`), never cascaded (`CAD-A3`).
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no renderer, no
// filesystem.
#pragma once

#include "forgeshape_history.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Why a delete did not happen. Every one is a refusal, and a refusal changes
// nothing: no body leaves the scene, no history step is recorded, no ObjectId
// moves and the project fingerprint is unchanged.
enum class DeleteBodyStatus {
    Ok,
    // No body in the scene carries that id.
    UnknownBody,
    // The scene holds exactly one body. See the header: this product has no
    // empty project, and inventing a replacement is not an answer.
    RefusedLastBody,
    // A Construction edit is open. Removing a body underneath a half-finished
    // user act would leave that act's captured pre-state describing a scene it
    // never saw -- the same rule `loadProjectDocument` and
    // `commitImportedGlbScene` already apply.
    RefusedEditInProgress,
    // The body is a producer with face-supported CAD dependents (`CAD-A3`).
    // Refused rather than cascaded: delete the dependents first.
    RefusedHasDependents,
};

const char* deleteBodyStatusName(DeleteBodyStatus status);

// What a delete actually did, so a caller can log it and a test can assert
// against it rather than against a bare boolean.
struct DeleteBodyReport {
    ObjectId removedBodyId = kNoObject;
    // Where it sat in scene order, which is what makes the replacement rule
    // below checkable.
    size_t removedIndex = 0;
    // The body left active afterwards. Equal to whatever was active when the
    // deleted body was not it.
    ObjectId activeBodyId = kNoObject;
    // How many bodies remain.
    size_t bodyCount = 0;
};

// Removes one body from the scene, as one history transaction.
//
// SELECTION, and the whole rule: if the deleted body was not the active one,
// the active one does not change. If it was, the selection falls to the NEXT
// body in scene order, or -- when the deleted body was last -- to the one
// before it. Scene order is the Objects list's order, so what the user sees
// selected afterwards is the row that took the deleted row's place, which is
// the only answer that does not look arbitrary in a list.
//
// Callers hold the one existing state mutex, exactly as for every other scene
// mutation.
DeleteBodyStatus deleteSceneBody(ObjectId id, ConstructionScene& scene,
                                 ConstructionHistory& history,
                                 DeleteBodyReport* outReport = nullptr);

}  // namespace forgeshape
