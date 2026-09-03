// The Sculpt Undo/Redo history: one bounded, volatile, per-body stack of
// completed strokes.
//
// `ARCH-OWNER-12`. This is the SECOND history in the product and it is
// deliberately not the first one. `ConstructionHistory` records project/object
// truth — parameters, placements, creation, deletion — and a step there holds
// bounded Construction-domain state and never one mesh byte. A sculpt stroke's
// entire effect IS mesh bytes, so it could never be a step there without
// inverting that rule. The answer is two histories that never merge:
//
//     ConstructionHistory   project truth, serialized nowhere but implied by
//                           the document, never holds a sculpt vertex
//     SculptHistory         THIS: sculpt stroke geometry, runtime-only, never
//                           serialized, never consulted outside Sculpt mode
//
// WHY IT IS VOLATILE
// ------------------
// A `.forge` document stores what cannot be recomputed. A Frozen Sculpt Mesh's
// CURRENT positions are exactly that and are stored; the PATH the user took to
// reach them is not project truth by any reading — it is a property of the
// editing session, like the camera or the held brush. So nothing here reaches
// the codec, the checkpoint or the document, and reopening a project starts a
// fresh history over the geometry the file restored.
//
// WHY IT KNOWS NOTHING ABOUT A MESH
// --------------------------------
// This file includes `forgeshape_math.h` and nothing else from the domain. It
// is a bounded stack of position deltas: it decides what is retained and what
// is evicted, and it never writes a vertex. `SculptSession` owns the one place
// a delta is applied, exactly as it owns the one place a stroke is recorded, so
// there is no second path that could move a sculpt vertex.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no renderer, no
// filesystem.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

#include "forgeshape_math.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// One completed stroke
// ---------------------------------------------------------------------------
//
// A DELTA, not a snapshot: the vertices one stroke actually moved, where they
// were before it and where it left them. A stroke's affected set is captured at
// pointer-down and fixed for the stroke's life (see `SculptStroke`), so this is
// bounded by the brush, not by the mesh — a small brush on a million-vertex
// import costs a few hundred vertices, where a snapshot would cost twelve
// megabytes for the same stroke.
//
// The three vectors are PARALLEL and always the same length. `vertexIndices` is
// sorted and unique, so applying an entry is deterministic and an entry can be
// compared without normalizing it first.
//
// Normals are deliberately absent: they are derived data with exactly one
// producer (`SculptMesh`'s normal cache, recomputed on the next read after any
// accepted position write), so storing them would be storing an answer the mesh
// regenerates for free and could disagree with.
struct SculptStrokeDelta {
    std::vector<uint32_t> vertexIndices;
    std::vector<Vec3> beforePositions;
    std::vector<Vec3> afterPositions;

    // The user-semantic edited flag on both sides of the stroke.
    //
    // Carried rather than derived, and that is the whole reason it exists.
    // "Has this frozen mesh been sculpted?" is what the destructive-reset
    // confirmation asks and what `.forge` stores, and it is NOT `undoDepth >
    // 0`: a project loaded with an already-edited sculpt mesh starts with an
    // empty history and must still report edits, so undoing the one new stroke
    // taken since must land on `true`, while undoing the first stroke after a
    // fresh Freeze must land on `false`. Only the value captured at the stroke's
    // own start answers both.
    bool beforeHasEdits = false;
    bool afterHasEdits = true;

    size_t vertexCount() const { return vertexIndices.size(); }

    // A conservative estimate of what this entry costs, used to enforce the
    // byte budget. Deliberately an over-estimate: it charges the payload plus a
    // fixed per-entry allowance for the three vector headers and their
    // allocator slack, so the cap is reached before the real footprint does
    // rather than after.
    size_t payloadBytes() const;

    // Well-formed: three parallel vectors, at least one vertex, indices sorted
    // and unique, every position finite. A malformed delta is refused by
    // `SculptHistory::record` rather than stored and applied later.
    bool valid() const;
};

// Bytes charged for one entry beyond its payload: three `std::vector` headers
// (24 bytes each on a 64-bit target), the two flags, and allocator slack. It
// only has to be an over-estimate, and it is.
constexpr size_t kSculptHistoryEntryOverheadBytes = 128;

// ---------------------------------------------------------------------------
// The bounds
// ---------------------------------------------------------------------------
//
// Two independent caps, because either alone is escapable: a step cap alone
// lets thirty-two whole-mesh strokes hold hundreds of megabytes, and a byte cap
// alone lets a hundred thousand one-vertex strokes hold an unbounded number of
// allocations. Both are enforced on every record.
//
// The numbers are per BODY, not per session, because the history is per body —
// see `FrozenSculpt`. They were chosen against measured entry sizes rather than
// picked round: at 28 payload bytes per vertex (4 index + 12 before + 12
// after), the default 120 px brush on the product's own sculpt sources touches
// on the order of a few hundred to a few thousand vertices, which is tens of
// kilobytes per stroke. `kMaxSculptHistoryBytes` therefore holds a full
// thirty-two-stroke session of ordinary strokes with room to spare, while
// staying small enough that ten sculpted bodies cost tens of megabytes rather
// than hundreds on a device that has neither to spare.

// The most completed strokes one body retains. The thirty-third evicts the
// oldest.
constexpr size_t kMaxSculptHistoryEntries = 32;

// The most one body's retained strokes may cost in total.
constexpr size_t kMaxSculptHistoryBytes = 4u * 1024u * 1024u;  // 4 MiB

// The most ONE stroke may cost and still be retained.
//
// A stroke larger than this still APPLIES — refusing to sculpt because the
// history is full would be the tail wagging the dog — but it is not retained,
// and it says so by name rather than silently. See
// `SculptHistory::RecordOutcome::NotRetained`.
constexpr size_t kMaxSculptHistoryEntryBytes = 1u * 1024u * 1024u;  // 1 MiB

// ---------------------------------------------------------------------------
// The history
// ---------------------------------------------------------------------------
//
// One per body, living inside `FrozenSculpt` beside the mesh it describes, so
// "which body's history is this" is answered by ownership rather than by a key
// somebody has to keep in step. Two bodies sculpted in one session hold two of
// these and neither can reach the other's; `sculptSession()` rebinds to the
// active body's on every call, so switching bodies switches history with no
// lookup at all.
//
// Not copyable: a duplicated history would be a second answer to "what can be
// undone", which is exactly what one history exists to prevent.
class SculptHistory {
public:
    SculptHistory() = default;
    SculptHistory(const SculptHistory&) = delete;
    SculptHistory& operator=(const SculptHistory&) = delete;
    SculptHistory(SculptHistory&&) = default;
    SculptHistory& operator=(SculptHistory&&) = default;

    // What recording one completed stroke did.
    enum class RecordOutcome {
        // Retained. It is now the top of the undo stack and the redo stack is
        // empty.
        Recorded,
        // The delta was empty or malformed, so there was nothing to record.
        // Not a failure: a stroke that moved no vertex is a stroke that did
        // nothing, and recording it would put an entry in the stack that undoes
        // to itself.
        NothingChanged,
        // The stroke is larger than `kMaxSculptHistoryEntryBytes`. The
        // deformation stands and the history stays coherent, but this one
        // stroke cannot be taken back.
        //
        // The redo stack is still cleared, because it must be: the geometry has
        // moved on and those entries describe a future that no longer follows
        // from the present.
        NotRetained,
    };

    static const char* recordOutcomeName(RecordOutcome outcome);

    // Records one completed stroke, clearing the redo stack.
    RecordOutcome record(SculptStrokeDelta delta);

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }

    // The entry an Undo would apply. Only valid while `canUndo()`.
    const SculptStrokeDelta& undoTop() const { return undo_.back(); }
    // The entry a Redo would apply. Only valid while `canRedo()`.
    const SculptStrokeDelta& redoTop() const { return redo_.back(); }

    // Moves the top entry across, AFTER its caller has applied it.
    //
    // Two calls rather than one, because this class cannot write a vertex and
    // will not pretend to: the caller reads `undoTop()`, applies it to the
    // mesh, and then commits. `SculptSession::undoStroke` is the ONE caller of
    // either, so the pairing has exactly one place to be right.
    void commitUndo();
    void commitRedo();

    // Forgets everything, in both directions.
    //
    // Called when the retained sculpt mesh is REPLACED — a Freeze or a
    // destructive Reset from source — because every entry describes positions
    // in a mesh that no longer exists. Undo deliberately cannot cross that
    // boundary: the reset is the user saying the previous sculpt is gone.
    void clear();

    size_t undoDepth() const { return undo_.size(); }
    size_t redoDepth() const { return redo_.size(); }

    // What the retained entries cost right now, by the same conservative
    // measure the cap is enforced with.
    size_t payloadBytes() const { return bytes_; }

    // --- introspection (logging, diagnostics and self-tests only) ---

    // How many entries have been dropped to keep inside the caps, and how many
    // strokes were too large to retain at all. Both are session-lifetime
    // counters and neither is project truth.
    uint64_t evictedEntries() const { return evicted_; }
    uint64_t notRetainedStrokes() const { return notRetained_; }

private:
    // Enforces both caps by dropping from the OLDEST end of the undo stack.
    // Never touches the redo stack and never drops the newest entry: the entry
    // just recorded is the one the user is most likely to want back.
    void evictToFit();

    // Recomputed rather than tracked incrementally, because a drift between a
    // running total and the real contents is exactly how a byte cap stops
    // being a cap. Both stacks are bounded by `kMaxSculptHistoryEntries`, so
    // the sum is over at most sixty-four small headers.
    void recomputeBytes();

    // `deque` rather than `vector`: eviction pops the FRONT, which is O(1)
    // here and O(n) memory-moves there.
    std::deque<SculptStrokeDelta> undo_;
    std::deque<SculptStrokeDelta> redo_;
    size_t bytes_ = 0;
    uint64_t evicted_ = 0;
    uint64_t notRetained_ = 0;
};

}  // namespace forgeshape
