#include "forgeshape_sculpt_history.h"

#include <cmath>
#include <utility>

namespace forgeshape {
namespace {

bool finite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace

size_t SculptStrokeDelta::payloadBytes() const {
    const size_t n = vertexIndices.size();
    return n * (sizeof(uint32_t) + 2 * sizeof(Vec3)) + kSculptHistoryEntryOverheadBytes;
}

bool SculptStrokeDelta::valid() const {
    const size_t n = vertexIndices.size();
    if (n == 0) {
        return false;
    }
    if (beforePositions.size() != n || afterPositions.size() != n) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        // Strictly increasing proves sorted AND unique in one pass. A repeated
        // index would make an entry's own application order-dependent, which is
        // the one thing a delta must never be.
        if (i > 0 && vertexIndices[i] <= vertexIndices[i - 1]) {
            return false;
        }
        if (!finite(beforePositions[i]) || !finite(afterPositions[i])) {
            return false;
        }
    }
    return true;
}

const char* SculptHistory::recordOutcomeName(RecordOutcome outcome) {
    switch (outcome) {
        case RecordOutcome::Recorded:
            return "recorded";
        case RecordOutcome::NothingChanged:
            return "nothing_changed";
        case RecordOutcome::NotRetained:
            return "not_retained";
    }
    return "unknown";
}

SculptHistory::RecordOutcome SculptHistory::record(SculptStrokeDelta delta) {
    if (!delta.valid()) {
        // Nothing moved, or the caller handed over something malformed. Neither
        // is a state change, so the redo stack stands: the user's next Redo
        // still means what it meant.
        return RecordOutcome::NothingChanged;
    }

    // A completed stroke is a new present, and every redo entry describes a
    // future that followed from the old one. This happens for a retained AND an
    // unretained stroke, because the geometry moved either way.
    redo_.clear();

    if (delta.payloadBytes() > kMaxSculptHistoryEntryBytes) {
        ++notRetained_;
        recomputeBytes();
        return RecordOutcome::NotRetained;
    }

    undo_.push_back(std::move(delta));
    evictToFit();
    return RecordOutcome::Recorded;
}

void SculptHistory::commitUndo() {
    if (undo_.empty()) {
        return;
    }
    redo_.push_back(std::move(undo_.back()));
    undo_.pop_back();
    recomputeBytes();
}

void SculptHistory::commitRedo() {
    if (redo_.empty()) {
        return;
    }
    undo_.push_back(std::move(redo_.back()));
    redo_.pop_back();
    // Moving an entry back cannot exceed a cap that already held it, but the
    // caps are re-enforced anyway rather than reasoned about: the invariant is
    // "the retained set is inside the caps", and the cheapest way to keep an
    // invariant true is to assert it at every mutation rather than at the ones
    // that seem to need it.
    evictToFit();
}

void SculptHistory::clear() {
    undo_.clear();
    redo_.clear();
    bytes_ = 0;
    // `evicted_` and `notRetained_` are session-lifetime diagnostics, not
    // contents, so they deliberately survive a clear.
}

void SculptHistory::evictToFit() {
    recomputeBytes();
    // Never evicts the last entry: a single stroke that fits the per-entry cap
    // is always retainable, whatever the total cap says, and an empty stack
    // with a stroke that just landed would be a worse answer than one entry
    // slightly over budget. `kMaxSculptHistoryEntryBytes` is a quarter of the
    // total, so the loop can never leave more than a quarter over.
    while (undo_.size() > 1
           && (undo_.size() > kMaxSculptHistoryEntries || bytes_ > kMaxSculptHistoryBytes)) {
        undo_.pop_front();
        ++evicted_;
        recomputeBytes();
    }
}

void SculptHistory::recomputeBytes() {
    size_t total = 0;
    for (const SculptStrokeDelta& d : undo_) {
        total += d.payloadBytes();
    }
    for (const SculptStrokeDelta& d : redo_) {
        total += d.payloadBytes();
    }
    bytes_ = total;
}

}  // namespace forgeshape
