// The four object commands of Stage 018A: Rename, Show/Hide, Lock/Unlock and
// Duplicate (`UI-OWNER-40`).
//
// Its own module rather than methods on either collaborator, for exactly the
// reason `forgeshape_body_delete.h` is one: each of these is a decision ABOUT
// the project that needs both the scene (which holds the bodies) and the
// history (which records the transaction), and neither owns the other. Delete
// stays where it is and is not touched here.
//
// Invariants shared by all four:
//
//   * REPRESENTATION-NEUTRAL. Nothing here asks what a body is in order to
//     decide WHETHER it can be renamed, hidden or locked -- a Construction
//     Body, an Imported Mesh and a CAD Body answer identically, and any of them
//     carrying a Frozen Sculpt Mesh answers identically again. Duplicate is the
//     one that must dispatch, because it has to COPY the representation, and it
//     dispatches through the existing `BodyRepresentation` rather than through
//     a new boolean.
//   * ONE ACT IS ONE TRANSACTION. Each opens ONE `ScopedConstructionEdit` that
//     OWNS the edit; an edit already open is refused (`RefusedEditInProgress`)
//     on the same terms `deleteSceneBody` and `loadProjectDocument` refuse one.
//     One Rename is one Undo; so is one toggle and one Duplicate.
//   * A COMMIT THAT FINDS NOTHING DIFFERENT RECORDS NOTHING. Renaming a body to
//     the name it already has, or hiding one that is already hidden, is a no-op
//     and leaves the redo stack alone -- that is `commitEdit`'s existing rule
//     and none of these restate it.
//   * A REFUSAL CHANGES NOTHING AT ALL. No body is renamed, no flag moves, no
//     ObjectId is minted, no step is recorded and the project fingerprint does
//     not shift.
//   * NO GEOMETRY WORK. Rename, Show/Hide and Lock/Unlock publish nothing, mint
//     no MeshRevision, rebuild no CAD mesh, upload nothing and move no sculpt
//     vertex. Duplicate publishes exactly once, for the body it created.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no renderer, no
// filesystem.
#pragma once

#include <string>

#include "forgeshape_history.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Why an object command did not happen.
//
// One enum for all four rather than one per command: the first three refusals
// are shared word for word, and a caller that had to translate four nearly
// identical enums into one status line would be the place they drifted apart.
enum class BodyCommandStatus {
    Ok,
    // No body in the scene carries that id.
    UnknownBody,
    // A Construction edit is open. Mutating a body underneath a half-finished
    // user act would leave that act's captured pre-state describing a scene it
    // never saw.
    RefusedEditInProgress,
    // The requested name is not one this product would store: empty, or empty
    // once sanitized. Refused rather than silently replaced by a fallback --
    // the user typed something and is entitled to know it was not taken.
    RefusedInvalidName,
    // Duplicate only: the body is a FACE-SUPPORTED CAD Body (`CAD-A3`).
    //
    // Its world placement is DERIVED from the producer's face frame and is not
    // stored, so a copy would stand exactly where the original stands, forever,
    // with no way for the user to move it apart -- `isFaceSupportedCad` is
    // precisely the predicate that refuses independent transformation. A
    // duplicate the user can neither see as separate nor separate is not a
    // duplicate, so it is refused BY NAME rather than created. Nothing is
    // retargeted, nothing is detached from its TopoRef and no dependency is
    // rewritten: the refusal is the whole behaviour.
    //
    // A world-plane CAD Body duplicates normally, and so does a PRODUCER that
    // has dependents -- the copy is simply a producer of its own with none,
    // because a single-object Duplicate copies one body and never a graph.
    RefusedFaceSupportedCad,
    // Duplicate only: the copy did not build or did not regenerate. A CAD state
    // is re-validated by the scene's own `addCadBody` before an id is minted,
    // so a refusal here costs no ObjectId either.
    RefusedNotDuplicable,
};

const char* bodyCommandStatusName(BodyCommandStatus status);

// Renames one body, as one history transaction.
//
// The name is put through the DOMAIN's existing rule --
// `sanitizeImportedMeshName` -- and not through a second one written here. That
// rule already trims, drops malformed UTF-8, replaces control characters and
// cuts on a UTF-8 boundary at `kMaxImportedMeshNameBytes`, and it is
// idempotent, which is what lets the `.forge` decoder state its check as "the
// stored name is what this would produce". Rename is a new way to reach an
// existing policy, never a second policy: one bound, one sanitizer, one
// storability predicate.
//
// A name that sanitizes to empty is REFUSED (`RefusedInvalidName`) and the body
// keeps the name it had. Empty is the UI's signal for the ObjectId-derived
// fallback label and must never be something a user can type their way into by
// accident.
//
// Publishes nothing and mints no revision: a name is truth about identity.
BodyCommandStatus renameSceneBody(ObjectId id, const std::string& requestedName,
                                  ConstructionScene& scene, ConstructionHistory& history);

// Shows or hides one body, as one history transaction.
//
// Hidden means NOT DRAWN and NOT PICKED, enforced in the single place both read
// from (`ConstructionScene::snapshot`). It does not mean deleted: the body
// stays in the scene, stays in the Objects list, stays selectable from its row,
// stays saved in the `.forge` file and stays exported.
//
// Hiding the ACTIVE body is allowed and the selection does NOT move. That is
// the smallest coherent behaviour: the row stays selected so the user can
// immediately show it again, and everything that reads the viewport already
// copes with an active body that is not in the snapshot -- the gizmo is
// withdrawn above JNI and refused below it, and the outline has nothing to
// trace because the mask pass rasterises the snapshot.
BodyCommandStatus setSceneBodyVisible(ObjectId id, bool visible, ConstructionScene& scene,
                                      ConstructionHistory& history);

// Locks or unlocks one body, as one history transaction.
//
// A locked body stays visible and stays pickable; what it refuses is being
// MOVED. The guards are two and both are named: the gizmo is not activated over
// one, and a transform write against one is rejected. Rename, Show/Hide,
// Duplicate and Unlock all remain available, and Delete is deliberately
// UNCHANGED by lock -- Delete's semantics are `UI-OWNER-45`'s and this stage
// does not redefine them.
BodyCommandStatus setSceneBodyLocked(ObjectId id, bool locked, ConstructionScene& scene,
                                     ConstructionHistory& history);

// What a duplicate produced, so a caller can log it and a test can assert
// against it rather than against a bare boolean.
struct DuplicateBodyReport {
    ObjectId sourceBodyId = kNoObject;
    // Freshly minted, never reused and never derived from the source's.
    ObjectId newBodyId = kNoObject;
    // Where the copy landed: the END of scene order, like every other creation
    // in this product. The Objects list is insertion-ordered and has no
    // reordering, so appending is the one position that needs no new rule.
    size_t newIndex = 0;
    size_t bodyCount = 0;
    // Whether the copy carries its own Frozen Sculpt Mesh, cloned from the
    // source's CURRENT positions.
    bool clonedSculptMesh = false;
};

// Duplicates one body, as one history transaction.
//
// WHAT IS COPIED: the source representation's own truth (a Construction
// Source's six remembered parameter sets and active kind, an Imported Mesh's
// positions, normals, indices and submesh batches, or a CAD Body's sketch and
// extrusion), the placement, the name with a deterministic copy suffix, the
// visibility, the lock, and the Frozen Sculpt Mesh's current positions and
// topology when the source owns one.
//
// WHAT IS DELIBERATELY NOT COPIED:
//   * the ObjectId. A fresh one is minted, which is the whole point.
//   * the SculptHistory. It is a bounded, volatile, per-body Undo over strokes
//     made on THIS mesh (`ARCH-OWNER-12`); the copy has made none, so it starts
//     empty exactly as a freshly frozen mesh does. Its `hasEdits` fact IS
//     carried, because that is a stored fact about the geometry rather than a
//     stack depth.
//   * every renderer resource, published revision and GPU handle. The copy
//     publishes its own geometry once, through the one `publishSceneObject`
//     dispatch, and gets its own MeshStore by construction.
//   * the Construction history. A duplicate is one step in it, not a copy of
//     it.
//   * the selection pulse and the selection outline, which are per-frame
//     presentation and hold nothing.
//
// The copy becomes the ACTIVE body, because a creation the user asked for is
// one they are about to work on -- the same answer Add Primitive and Import
// already give.
BodyCommandStatus duplicateSceneBody(ObjectId id, ConstructionScene& scene,
                                     ConstructionHistory& history,
                                     DuplicateBodyReport* outReport = nullptr);

// The name a duplicate of `sourceName` gets, given the names already in `scene`.
//
// Deterministic and exposed so it can be proven directly: `name` becomes
// `name copy`, and if that is taken, `name copy 2`, `name copy 3` and so on.
// A body with no stored name produces no stored name -- the copy falls back to
// its own ObjectId label exactly as the source does, because fabricating
// "Body #3 copy" would freeze a label that is DERIVED, not authored, into
// project truth.
//
// The suffix is applied and then sanitized, so a name already at the byte cap
// is truncated on a UTF-8 boundary by `sanitizeImportedMeshName` rather than
// being rejected. A truncated copy name that collides is then disambiguated by
// the same loop, so the result is always storable.
std::string duplicateBodyName(const std::string& sourceName, const ConstructionScene& scene);

}  // namespace forgeshape
