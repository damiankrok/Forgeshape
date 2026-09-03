# MEMORY_BOUNDS — SCULPT-UNDO-R0

## The constants, in production code

`app/src/main/cpp/forgeshape_sculpt_history.h`:

```cpp
constexpr size_t kSculptHistoryEntryOverheadBytes = 128;
constexpr size_t kMaxSculptHistoryEntries         = 32;
constexpr size_t kMaxSculptHistoryBytes           = 4u * 1024u * 1024u;   // 4 MiB
constexpr size_t kMaxSculptHistoryEntryBytes      = 1u * 1024u * 1024u;   // 1 MiB
```

All three are **per body**, because the history is per body. The same limits
apply to Construction-derived and Imported-derived sculpt: there is one
`SculptHistory` type and one `record` path, so there is nowhere for a second set
of limits to live.

## The cost measure

```
payloadBytes = n * (4 + 12 + 12) + 128
             = 28 n + 128
```

`4` is the `uint32_t` index; the two `12`s are the before and after `Vec3`. The
`128` is a deliberate over-estimate of three `std::vector` headers (24 bytes each
on a 64-bit target), the two flags and allocator slack — the cap is reached
before the real footprint does, not after.

Asserted bit-exactly rather than described:
`SCUNDO_22_one_vertex_costs_what_the_measure_says` checks
`syntheticDelta(1).payloadBytes() == 28 + kSculptHistoryEntryOverheadBytes`.

The total is **recomputed from the contents** on every mutation rather than
tracked incrementally. A running total that drifts from what is actually held is
exactly how a byte cap stops being a cap, and both stacks are bounded by 32
entries so the sum is over at most 32 small headers.

## Measured entry sizes

The **affected vertex counts below are measured** on `emulator-5580`
(`ForgeShape_Stage006`), read from
`FORGESHAPE_SCULPT_STROKE_BEGIN:<tool>:<affectedVertexCount>` — a log line the
touch path already emitted before this stage; see
`DEVICE_SCULPT_HISTORY_TRACE.txt`. The **byte figures are derived** from those
counts by the `28 n + 128` formula above, which is itself asserted bit-exactly.
Where a bound is asserted directly rather than derived, it says so.

| Fixture | Brush | Affected vertices | Entry bytes | % of the 1 MiB per-entry cap |
| --- | --- | --- | --- | --- |
| Construction Sphere, diameter 2 m (482 vertices) | 300 px, strength 1.0 | **482** — the whole mesh | 13 624 B (13.3 KiB) | 1.3 % |
| the same, 40 consecutive strokes | 300 px | 482 falling to **140** | 13 624 B down to 4 048 B | 1.3 % to 0.4 % |
| the same, glancing stroke near the silhouette | 300 px | **6** | 296 B | 0.03 % |
| Imported Mesh (`construction_sentinel.glb`, one body) | 300 px, strength 1.0 | **127–130** | 3 684 - 3 768 B | 0.35 % |

The 300 px brush at this framing captures the ENTIRE 482-vertex sphere, so
13.3 KiB is the worst case for that fixture, not a typical one — a brush cannot
capture more vertices than the mesh has. `buildDelta` then keeps only the
vertices that actually moved, so a stored entry is this size or smaller.

The 40-stroke run's descending sequence (482, 436, 430, 425, 419, ... 148, 140)
is the brush working as designed rather than anything about the history: each
stroke pulls surface away from the brush centre, so fewer vertices fall inside
the next stroke's world-metric radius. It means the average retained entry is
comfortably smaller than the first one.

The imported body is the more representative measurement — a real file's
geometry under a real brush at roughly **3.7 KiB per stroke**, which is 0.09 % of
one body's whole 4 MiB budget.

**Ten-stroke sequence — asserted, not derived.**
`SCUNDO_22_and_ten_of_them_stay_far_inside_the_total` drives ten real
default-brush strokes and asserts the retained total is
`< kMaxSculptHistoryBytes / 4` (1 MiB) with a depth of exactly 10. Derived from
the measured counts, ten strokes of the worst shape above would be ~133 KiB —
about 3 % of the 4 MiB budget.

**One stroke — asserted.**
`SCUNDO_22_a_default_brush_stroke_costs_far_less_than_the_per_entry_cap` asserts
one real default-brush stroke is non-zero and `< kMaxSculptHistoryEntryBytes / 8`
(128 KiB).

A full 32-entry stack of whole-mesh strokes on this fixture is
`32 x 13 624 = 435 968 B` ≈ **426 KiB** (derived), about 10 % of the budget. The
budget is therefore not the binding constraint for ordinary work — the step cap
is, which is the intended order.

## How the numbers were chosen

Not picked round. At 28 bytes per vertex:

- **1 MiB per entry** is ~37 449 vertices in ONE stroke. Every sculpt source in
  the product is far smaller than that, so an ordinary stroke can never be
  refused; only a large brush on a dense import can reach it.
- **4 MiB per body** holds a full 32-stroke session of ordinary strokes with an
  order of magnitude to spare (above), while staying small enough that ten
  sculpted bodies cost tens of megabytes rather than hundreds on a device that
  has neither to spare.
- **32 entries** is the visible product promise — the number of strokes a user
  can walk back — and is what actually binds in normal use.

The per-entry cap is deliberately a QUARTER of the total, which is what bounds
the one place eviction stops: see below.

## Both caps are enforced, and why both are needed

Either alone is escapable:

- a step cap alone lets 32 whole-mesh strokes on a dense import hold hundreds of
  megabytes;
- a byte cap alone lets an unbounded NUMBER of one-vertex strokes accumulate an
  unbounded number of allocations.

`evictToFit()` runs on every `record` and every `commitRedo`, and enforces both.

## Eviction

- **Deterministic and oldest-first.** `undo_` is a `std::deque` and eviction is
  `pop_front()`.
- **Never the newest entry.** The loop stops at `undo_.size() > 1`: the stroke
  that just landed is the one the user is most likely to want back, and an empty
  stack immediately after a stroke would be a worse answer than one entry
  slightly over budget. Because the per-entry cap is a quarter of the total, that
  can never leave more than a quarter over.
- **Never the current sculpt mesh.** The history holds deltas only; the mesh is
  owned by `FrozenSculpt` beside it and no eviction path can reach it.
- **Never the redo stack.** Entries only move between stacks, never appear in
  redo, so the two together can never exceed 32.
- Counted by `evictedEntries()`, exposed as `sculptHistoryEvictedCount()`.

`SCUNDO_22_the_step_cap_is_reached_without_eviction`,
`SCUNDO_22_the_next_stroke_evicts_exactly_one`,
`SCUNDO_22_and_it_was_the_oldest`,
`SCUNDO_22_the_byte_cap_bites_before_the_step_cap`,
`SCUNDO_22_the_newest_entry_is_never_the_one_evicted`.

The byte-cap case is driven with 20 000-vertex synthetic deltas (560 128 B each);
twelve of them would be 6.4 MiB, so the total cap bites while the step count is
still under 32 — which is what the check asserts.

## A stroke too large to retain

`kMaxSculptHistoryEntryBytes` is checked BEFORE the entry is stored.

- The **deformation still applies**. Refusing to sculpt because the history is
  full would be the tail wagging the dog.
- Nothing is stored, so there is **no unbounded allocation**: the delta is moved
  in, measured, and dropped.
- The **redo stack is still cleared**, because it must be — the geometry moved,
  and those entries describe a future that no longer follows from the present.
- It is named, not silent: `RecordOutcome::NotRetained`, counted by
  `notRetainedStrokes()`, exposed as `sculptHistoryNotRetainedCount()` and
  `HISTORY_ENTRY_NOT_RETAINED`, and reported to the user once via
  `status_sculpt_stroke_not_retained` ("That stroke was too large to keep in
  sculpt history and cannot be undone.").
- The history **still works afterwards**: the next ordinary stroke records
  normally.

`SCUNDO_23_an_oversized_stroke_is_refused_by_name`,
`SCUNDO_23_it_is_not_stored`,
`SCUNDO_23_but_the_stale_redo_branch_is_still_dropped`,
`SCUNDO_23_and_the_history_still_works_afterwards`.

## Device bound

`E2E-SCUNDO-10` drives **40 real strokes** through the whole touch path and then
asserts, on the device:

- `sculptUndoDepth() <= 32`
- `sculptHistoryBytes() <= 4 MiB`
- `sculptHistoryEvictedCount() > 0` — eviction genuinely happened rather than the
  cap never being reached
- the controls still work, and Undo/Redo still move geometry exactly, after
  eviction

## No snapshot fallback was needed

The brief allowed full position snapshots if correct deltas proved materially
unsafe. They did not: `SculptStroke` already captured the affected set and every
member's base position at pointer-down, so the delta's BEFORE side was already
present in the domain and the AFTER side is one read per vertex. All four tools
were confirmed to write only vertices in that set. Deltas are also bounded by the
BRUSH rather than the mesh — a small brush on a million-vertex import costs a few
hundred vertices, where a snapshot would cost 12 MB for the same stroke and would
have made the per-entry cap unreachable for every real import.

## What performance was and was not measured

This is not a performance stage and no optimization work was done. What the
acceptance criteria require is proved:

- **No unbounded growth** — both caps, above, plus the 40-stroke device run.
- **No per-pointer-event entries** — `SCUNDO-03`: four moves in one stroke,
  `undoDepth() == 1`, with the revision advancing more than once inside it to
  prove those really were separate batches.
- **No full project encode/decode in a step** — `undoStroke`/`redoStroke` touch
  the delta, `setVertexPosition`, `advanceRevision` and one
  `publishSculptRepresentation`. No codec, document, fingerprint or file is
  involved; `.forge` appears in this feature only in the tests, as the way
  geometry is OBSERVED.
- Undo/Redo apply cost is O(moved vertices) — 482 `setVertexPosition` calls at
  the fixture's worst case, plus one normal-cache invalidation and one
  publication, which is strictly less than the stroke that created it (that
  published once per move).
