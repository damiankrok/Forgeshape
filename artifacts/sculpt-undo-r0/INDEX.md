# SCULPT-UNDO-R0 — evidence index

**Result: `PASS-SCULPT-UNDO-R0-OWNER-RETEST-READY`.**

Sculpt strokes can be undone and redone through the two real chrome controls, for
a Construction-derived and an Imported-Mesh-derived sculpt alike, using a
history that is completely separate from the project/object history and is never
serialized.

| | |
| --- | --- |
| Baseline HEAD | `04f28df49f5000e863628571cdf5f8f076dac7e0` |
| Owner authority | `ARCH-OWNER-12` |
| Native suites | 17/17, **2712 checks**, zero failures (84 new `SCUNDO_*`) |
| JVM | 70/70 |
| Focused device suite | `SculptUndoTest` — **OK (10 tests)** |
| Authoritative aggregate | **`FULL_SHARDED_SUITE_PASS`** — 34 classes / 482 tests / 5 shards, missing = duplicates = unexpected = 0 |
| `.forge` corpus | all twelve fixtures and all seven digests **unchanged** — no schema change, no version bump |
| Device | `emulator-5580`, confirmed `ForgeShape_Stage006`. `emulator-5554` never contacted. |

## Files

| File | What it holds |
| --- | --- |
| `INDEX.md` | this |
| `ARCHITECTURE.md` | the two histories, ownership, the entry format, the JNI surface, threading |
| `STROKE_TRANSACTION.md` | where an entry begins and ends, and the four ways a stroke closes |
| `MEMORY_BOUNDS.md` | the constants, how they were chosen, measured entry sizes, eviction |
| `TEST_RESULTS.md` | the `SCUNDO-01..24` table, builds, guards, corpus, and the one shared helper that changed |
| `DEVICE_E2E.md` | the `E2E-SCUNDO-01..10` table and what each case asserts |
| `FULL_SHARDED.txt` | the full aggregate transcript, including `DISCOVERY_PASS` and every `SHARD_RESULT` |
| `NATIVE_SELFTEST_SCUNDO.txt` | a clean debug launch: all seventeen `*_SELFTEST_OK` tokens, the 84 `SCUNDO_*` passes, and the seven `.forge` golden digests |
| `FOCUSED_SCULPT_UNDO.txt` | the focused `SculptUndoTest` run — Construction and Imported sculpt undo/redo |
| `DEVICE_SCULPT_HISTORY_TRACE.txt` | `FORGESHAPE_SCULPT_HISTORY` and `..._STROKE_BEGIN` lines from that run: the monotonic-revision and `hasEdits` contracts, and the measured affected-vertex counts |

No `.forge` format change was made, and no fixture was added.

## Preflight and C1 cleanup

`git rev-parse HEAD` matched the required baseline exactly. The only dirty paths
were the five untracked files under `artifacts/import-01b-c1/`, all of them that
cancelled run's own output (`INDEX.md`, `OWNER_FAILURE.md`, `ROOT_CAUSE.md`,
`TEST_RESULTS.md`, `DEVICE_E2E.md`) and none an owner or user file. That
directory alone was removed with a targeted `rm -rf`; no `git clean` was used,
broad or otherwise. The worktree was confirmed clean before any code was written.

Worth recording: that run's `ROOT_CAUSE.md` concluded there was **no defect** —
Undo was absent in Sculpt because four documents and three layers of production
code said it should be — and named this stage as the deferred feature that would
be needed, requiring owner approval to invert the "Sculpt has no undo" rule. This
stage is that approval, and the implementation follows the boundary it
identified: the project history still never stores a sculpt vertex.

## The one design decision worth the owner's attention

**A cancelled stroke that moved geometry IS recorded.**

The brief's §2 says an entry is finalized "at stroke end/cancel"; its
`SCUNDO-04` row says "no-op/cancelled stroke creates no entry". Those read two
ways, and the product's own standing rule settles it: a cancelled stroke **keeps
the positions it already wrote** — "a stroke that stopped, not one that is rolled
back", stated in `ARCHITECTURE.md` since the brush existed. Since that
deformation stands and the user can see it, excluding it from the history would
make it the one deformation in the product that cannot be taken back.

So `cancelStroke` records the same single entry `endStroke` does, and a cancel
that moved nothing records nothing. What the brief forbids — a PARTIAL entry — is
guaranteed structurally instead: `buildDelta` builds an entry in one pass from
the whole affected set or not at all, so no code path can write half of one.
`SCUNDO-04` is tested as the case both readings agree on. Full reasoning in
`STROKE_TRANSACTION.md`.

## Deviations and remaining debt

1. **The cancelled-stroke reading above.** Deliberate, documented, and the
   product-consistent choice. If the owner wants a cancelled stroke's
   deformation to be un-undoable instead, that is a one-line change in
   `SculptSession::cancelStroke` and a test update — but it would leave visible
   geometry the user cannot reverse.

2. **`hasEdits` stopped deriving from the revision.** Required, not optional:
   `SCUNDO-11` and `SCUNDO-12` cannot both hold while "has edits" is
   `revision > kFrozenSculptRevision` and the revision must stay monotonic. It is
   now an explicit flag whose only history-facing writer is
   `restoreEditedFlag()`. `.forge` already stored it as a boolean, so the format
   is untouched. Every pre-existing behaviour is preserved because
   `advanceRevision()` sets the flag and is still called exactly where it was.

3. **One shared test helper changed, and it closed a real gap.**
   `WorkspaceTestSupport.sculptTheViewport` bypassed the `SurfaceView` and so
   never reached `onViewportGestureSettled` — the only moment the Android layer
   learns a stroke happened. The first device run failed on exactly that: Undo
   worked when clicked while its enabled state stayed stale. The helper now
   dispatches real `MotionEvent`s through the production path. Six suites share
   it and all were re-run; the aggregate covers all of them.

4. **Delete keeps the history through a Delete/Undo round trip.** The brief
   permitted either behaviour and asked for whichever is safe at zero extra
   complexity. Here it is literally zero: `holdDetachedBody` holds the whole
   body, so its stacks come back with it, and nothing was serialized or added to
   a history step. When no step names the body any more it is released and the
   history dies with it. Documented and tested as implemented (`SCUNDO-18`).

5. **No screenshots.** Every claim in this stage is byte-exact geometry, control
   enabled state, stack depth or document bytes — a screenshot would state each
   less precisely than the assertions do. The one visual change is asserted as
   `View.VISIBLE` at fourteen points.

6. **Not attempted, by scope.** No brush framework change, no sculpt history
   panel, no named steps, no keyboard shortcut, no `.forge` change, no BRIDGE-R1,
   no CAD work, no new object command. `E2E-IMP01A-12` and `E2E-IMP01B-12` remain
   `OWNER_REAL_FILE_*_RETEST_PENDING` on unchanged terms — the owner's own
   `1 lowpoly.glb` is not in this environment and no user folder was searched
   for it.

## Exactly one next step

Return this report to the ForgeShape coordinator for the **owner's real-device
Sculpt Undo/Redo retest**. No product stage may begin here, and CAD is not
started.
