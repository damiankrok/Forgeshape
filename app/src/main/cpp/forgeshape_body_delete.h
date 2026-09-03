// Removing one body from the project.
//
// `UI-OWNER-45`. Deliberately its own small module rather than a method on
// either collaborator, exactly like `forgeshape_import_commit.h`: deleting a
// body is a decision ABOUT the project that needs both the scene and the
// history, and neither of them owns the other. The scene knows how to detach a
// body and nothing about transactions; the history knows how to record one and
// nothing about which body should be selected afterwards.
//
// REPRESENTATION-NEUTRAL, AND WHY THAT IS THE WHOLE DESIGN
// -------------------------------------------------------
// Nothing here asks what a body IS. A Construction Body, an Imported Mesh, and
// either of them carrying a retained Frozen Sculpt Mesh are all removed by the
// same three lines, because a body is removed as a whole object: its identity,
// its representation, its placement, its published mesh and its sculpt state
// leave together and come back together. A per-representation delete path is
// exactly how one of them would eventually come back missing something.
//
// THE BODY IS NOT DESTROYED
// -------------------------
// It is detached and HANDED TO THE HISTORY, which holds it for as long as some
// step still names it. That is not an optimization: an Imported Mesh's geometry
// and any Frozen Sculpt Mesh are not derived from anything a history step
// holds, so a step could not rebuild them and an undo that had to would come
// back with an empty object wearing the right id. Undo restores the SAME
// object, and a redo removes that same object again.
//
// ONE TRANSACTION
// ---------------
// One Delete is exactly one Undo, through the ordinary `ScopedConstructionEdit`
// every other act uses -- and always as the scope that OWNS the edit, because an
// edit already in progress is refused outright (`RefusedEditInProgress`), the
// same rule `loadProjectDocument` and `commitImportedGlbScene` apply. Nothing in
// the product wraps a delete in a larger act, and refusing rather than joining
// keeps this operation's transaction boundary unambiguous.
//
// THE LAST BODY IS REFUSED
// ------------------------
// This product has no empty project. `ConstructionScene` creates a body
// eagerly, `activeBody()` returns a reference and every accessor built on it --
// `meshStore()`, `sculptSession()`, `activeConstructionOrNull()` -- assumes one
// exists, and `validateProjectDocument` refuses a `.forge` file with zero
// bodies. So deleting the only body is refused BY NAME
// (`RefusedLastBody`) and changes nothing at all, rather than being made to
// work by inventing a replacement primitive the user did not ask for.
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
