// The Construction transaction boundary and the Undo/Redo history.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type, no
// UI type. The Android shell can ask whether an undo is available and can ask
// for one; it holds no history of its own and no copy of any rule here.
//
// What a transaction is
// ---------------------
// One user act. `beginEdit()` captures the Construction-domain state of the
// whole scene; ordinary domain mutations then run — one of them for a typed
// Apply, two for Add Primitive, and as many as a drag produces once direct
// manipulation exists; `commitEdit()` compares the state afterwards with the
// state at begin and records exactly one history step if, and only if, they
// differ. There is deliberately no separate "update" call: an update IS an
// ordinary mutation made while an edit is open, which is what keeps the live
// state authoritative for the renderer and the picker throughout a drag instead
// of being buffered somewhere the rest of the product cannot see.
//
// `cancelEdit()` puts the captured state back and records nothing, which is the
// operation a dragged handle needs and a numeric field does not.
//
// What a history step holds, and what it deliberately does not
// ------------------------------------------------------------
// A bounded before/after copy of the scene's CONSTRUCTION-DOMAIN state: for
// every body, its ObjectId, which primitive is active, all six primitives'
// remembered parameters and its placement, plus the scene's order and which
// body was active. That is on the order of twenty doubles per body.
//
// It holds no vertices and no indices. Geometry is DERIVED from the parameters
// by the same generator the product already uses, so copying a published mesh
// into every step would be storing a product of the truth beside the truth, at
// hundreds of kilobytes a step. It holds no sculpt vertex, no SculptRevision
// and no stroke: sculpt strokes have their OWN per-body history
// (forgeshape_sculpt_history.h, `ARCH-OWNER-12`), and Construction history
// must never be able to move a sculpted vertex.
//
// Snapshot rather than a typed reversible command per operation: a drag emits a
// continuous stream of mutations, and a per-operation inverse would have to be
// written, tested and kept correct for every mutation and every composition of
// them. The cost is that a step is proportional to the SCENE rather than to the
// edit -- a few hundred kilobytes in the worst case at this capacity -- and it
// buys atomicity for creation and deletion, whose "inverse" spans the body
// list, its order and the selection, for free.
#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// How many undo steps are retained.
//
// In-memory only, for the life of the process. A step is bounded by the scene
// (see the file comment), so this is a ceiling of roughly a few hundred
// kilobytes for a scene of a hundred bodies and far less for anything real.
// Beyond it the OLDEST undo step is dropped, deterministically, one per commit;
// the current state and the redo stack are unaffected, because dropping a step
// only removes how far BACK the user can go.
constexpr size_t kConstructionHistoryCapacity = 64;

// One body's history-domain state, with the identity it belongs to.
//
// Placement is here for EVERY body, because every body has one. The
// Construction shape is here only for a body that has a Construction Source; an
// Imported Mesh's geometry is never copied into a step — it is not derived from
// anything, so a step could not hold it cheaply and must not try. An undone
// import's geometry survives inside the detached body the history keeps, the
// same way a Frozen Sculpt Mesh already does.
struct BodyConstructionState {
    ObjectId objectId = kNoObject;
    BodyRepresentation representation = BodyRepresentation::Construction;
    // Meaningful only when `representation` is Construction. Left at its
    // default for an Imported Mesh and never compared for one.
    ConstructionObjectState construction{};
    // Meaningful only when `representation` is Cad: the sketch and the
    // extrusion, which are the whole of a CAD Body's truth and are BOUNDED
    // (see kMaxSketchEntities). Never a vertex: the mesh is regenerated from
    // exactly this, so a step holds the truth and not a product of it.
    CadBodyState cad{};
    TransformValues transform{};
    // Stage 018A: the three representation-neutral facts about a body that are
    // project truth and are not geometry.
    //
    // They are here for EVERY body, on exactly the terms `transform` is: a body
    // has a name, a visibility and a lock because it is a body, whatever
    // generates its geometry. Carrying them in the step is the whole of their
    // Undo/Redo -- there is no per-command inverse, because the history is a
    // snapshot and a snapshot that includes them restores them.
    //
    // Bounded like the rest of a step: a name is capped by the domain's own
    // name rule (kMaxImportedMeshNameBytes) and the two flags are one byte
    // each, so a step stays proportional to the scene and holds no geometry.
    std::string name;
    bool visible = true;
    bool locked = false;
};

// The whole scene's Construction-domain state: which bodies exist, in what
// order, in what state, and which one the user was on.
struct SceneConstructionState {
    std::vector<BodyConstructionState> bodies;
    ObjectId activeBodyId = kNoObject;
};

// True when the two states are identical in every respect a history step cares
// about: the same bodies, in the same order, of the same representation, in the
// same shape and placement. `activeBodyId` is deliberately NOT compared —
// selection is carried by a step so a restore can land the user somewhere, but
// it is not itself an edit, so changing it alone must record nothing (see the
// definition for why).
bool sameSceneConstructionState(const SceneConstructionState& a,
                                const SceneConstructionState& b);

// Reads the scene's Construction-domain state. Publishes nothing and mutates
// nothing.
SceneConstructionState captureSceneConstructionState(const ConstructionScene& scene);

// What one restore actually had to do, so a caller can log it and a test can
// assert that a transaction boundary added no geometry work.
struct ConstructionRestoreReport {
    // Bodies whose SHAPE differed and therefore published a new mesh revision.
    int republishedBodies = 0;
    // Bodies whose placement differed. These publish nothing: a placement is a
    // derived matrix, exactly as an ordinary transform Apply is.
    int replacedPlacements = 0;
    int removedBodies = 0;
    int restoredBodies = 0;
    bool activeBodyChanged = false;
};

// THE owner of Construction history.
//
// Bound to one scene by reference, rather than reaching for the process-scoped
// one, so the self-tests can drive a whole history against a scene of their own
// and never depend on what a previous test or a live session left behind.
//
// Not internally synchronised, exactly like ConstructionScene: callers hold the
// one existing state mutex.
class ConstructionHistory {
public:
    explicit ConstructionHistory(ConstructionScene& scene) : scene_(scene) {}

    ConstructionHistory(const ConstructionHistory&) = delete;
    ConstructionHistory& operator=(const ConstructionHistory&) = delete;

    // ---------------------------------------------------------------------
    // The transaction boundary
    // ---------------------------------------------------------------------

    // Opens an edit and captures what it may have to go back to.
    //
    // Returns false, changing nothing, when one is already open. There is no
    // nesting: a composite user act (Add Primitive is one) opens ONE edit
    // around the mutations it is made of, and the inner mutations join it —
    // see ScopedConstructionEdit, which is how that is expressed at the call
    // sites without any of them having to know whether they are the outer one.
    bool beginEdit();

    bool editInProgress() const { return editOpen_; }

    // Closes an open edit and records at most one step.
    //
    // Returns true when a step was actually recorded, which happens only when
    // the scene's Construction state genuinely differs from what `beginEdit`
    // captured. A rejected mutation, a no-op Apply and an edit that was opened
    // and never used all end here with nothing recorded — and, crucially, with
    // the redo stack untouched, so a refused Apply after an Undo does not throw
    // the redo away.
    bool commitEdit();

    // Closes an open edit by putting the captured state back.
    //
    // Records nothing and leaves the redo stack alone. This is the operation a
    // cancelled drag needs: the live state has been moving with the finger and
    // has to return to where the finger went down.
    void cancelEdit(ConstructionRestoreReport* outReport = nullptr);

    // ---------------------------------------------------------------------
    // The session initialization boundary
    // ---------------------------------------------------------------------
    //
    // What a NEW SESSION does to reach the state the user starts working from
    // is not something the user did, and it must never be their first Undo.
    //
    // The product asks one question at launch, and answering "Sculpt" runs two
    // real Construction acts to get there — the body is shaped into a sphere and
    // then frozen. Those go through the ordinary mutation entry points, on
    // purpose, so that nothing about Freeze is duplicated or re-validated; but
    // the shape change IS a genuine Construction difference, so an ordinary
    // commit would record it and the user's very first Undo would rewind a
    // decision they never made.
    //
    // This is the production boundary that says so. Between `begin` and `end`,
    // commits still CLOSE their edit and the live state still moves — the
    // renderer and the picker see the seeded model exactly as before — but
    // nothing is recorded and no redo is thrown away. `end` then states the
    // postcondition outright: an empty history, both ways.
    //
    // It is deliberately NOT a general "clear the history" act. It is scoped to
    // bootstrap and is driven from the one place a session is seeded; Back to
    // Construction, a rotation, a resume and every ordinary edit are entirely
    // outside it and none of them may reach it. The debug-only `clear()` remains
    // what a TEST uses to fabricate a fresh-session observation inside a process
    // that has already run other cases.
    void beginSessionInitialization();

    // Closes the boundary and establishes the postcondition by construction: no
    // undo step, no redo step, no edit left open.
    void endSessionInitialization();

    // ---------------------------------------------------------------------
    // Holding a body the scene no longer contains
    // ---------------------------------------------------------------------

    // Takes ownership of a body an act removed from the scene, so a step that
    // names it can put THAT object back rather than a fresh one wearing its id.
    //
    // Undo already produced detached bodies -- undoing a creation takes one out
    // -- and kept them alive inside `applyState`. `IMPORT-01B`'s Delete is the
    // first act that removes a body while it is still named by an undo step's
    // BEFORE state, and it has nowhere else to put it: an Imported Mesh's
    // geometry and any body's Frozen Sculpt Mesh are not derived from anything
    // a step holds, so a step could not rebuild them.
    //
    // Ignores a null. Records nothing and touches neither stack: holding a body
    // is not a transaction, and the caller's own edit scope is what makes the
    // removal one.
    void holdDetachedBody(std::unique_ptr<SceneObject> body);

    bool sessionInitializing() const { return sessionInitializing_; }

    // ---------------------------------------------------------------------
    // Undo and redo
    // ---------------------------------------------------------------------

    bool canUndo() const { return !undoStack_.empty() && !editOpen_; }
    bool canRedo() const { return !redoStack_.empty() && !editOpen_; }

    size_t undoDepth() const { return undoStack_.size(); }
    size_t redoDepth() const { return redoStack_.size(); }

    // Steps one entry back. Returns false, changing nothing, when there is
    // nothing to undo or while an edit is open.
    bool undo(ConstructionRestoreReport* outReport = nullptr);

    // Steps one entry forward. Returns false, changing nothing, when there is
    // nothing to redo or while an edit is open.
    bool redo(ConstructionRestoreReport* outReport = nullptr);

    // Drops every step and every body being held for a redo. The scene itself
    // is untouched: forgetting how to go back is not going back.
    void clear();

private:
    struct HistoryEntry {
        SceneConstructionState before;
        SceneConstructionState after;
    };

    // Makes the scene match `target`, doing the least work that can.
    void applyState(const SceneConstructionState& target, ConstructionRestoreReport* outReport);

    // A detached body can only ever come back through a step that NAMES it, so
    // anything no step in either stack names is unreachable and is released
    // here. Both stacks and both sides of every step are asked: a redo restores
    // an undone creation from its AFTER state, and an undo restores a deleted
    // body from its BEFORE state.
    void pruneDetachedBodies();

    ConstructionScene& scene_;

    // Newest last. Bounded by kConstructionHistoryCapacity; the oldest is
    // dropped from the front.
    std::deque<HistoryEntry> undoStack_;
    std::deque<HistoryEntry> redoStack_;

    // Bodies an act took out of the scene -- an undone creation, or a Delete --
    // kept whole, Frozen Sculpt Mesh and Imported Mesh included, so putting one
    // back restores the SAME object rather than a fresh one wearing its
    // ObjectId. A body no step in either stack still names can never come back
    // and is released; see pruneDetachedBodies.
    std::vector<std::unique_ptr<SceneObject>> detached_;

    bool editOpen_ = false;
    SceneConstructionState editPreState_;

    // True only between beginSessionInitialization and endSessionInitialization.
    // It suppresses RECORDING, never mutation — see the boundary's comment.
    bool sessionInitializing_ = false;
};

// Opens an edit if none is open, and closes only the one it opened.
//
// This is what lets a single Apply be one history step on its own AND be
// absorbed into the creation transaction around it, without either caller
// testing which case it is in. A mutation entry point declares one of these; if
// an outer act already opened an edit, this one does nothing at all and the
// outer commit is still the only boundary.
class ScopedConstructionEdit {
public:
    explicit ScopedConstructionEdit(ConstructionHistory& history)
        : history_(history), owned_(history.beginEdit()) {}

    ~ScopedConstructionEdit() {
        if (owned_) {
            history_.commitEdit();
        }
    }

    ScopedConstructionEdit(const ScopedConstructionEdit&) = delete;
    ScopedConstructionEdit& operator=(const ScopedConstructionEdit&) = delete;

    // True when this scope is the one that opened the edit and will close it.
    bool owned() const { return owned_; }

private:
    ConstructionHistory& history_;
    bool owned_;
};

// The one process-scoped history, over the one process-scoped scene.
ConstructionHistory& constructionHistory();

}  // namespace forgeshape
