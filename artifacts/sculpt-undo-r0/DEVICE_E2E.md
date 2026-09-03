# DEVICE_E2E — SCULPT-UNDO-R0

All device work on **`emulator-5580`**, confirmed **`ForgeShape_Stage006`** by
`adb -s emulator-5580 emu avd name` before any install. The reserved
`emulator-5554` was attached throughout and **never contacted**.

Every case drives the **real controls**: `workspace.undoAction().performClick()`
and `workspace.redoAction().performClick()` — the views a user presses. Nothing
in this suite calls `sculptUndo`/`sculptRedo` directly, because the dispatch is
as much the subject as the history. Strokes go through
`WorkspaceTestSupport.sculptTheViewport`, which dispatches real `MotionEvent`s to
the viewport `SurfaceView`, so the whole production path runs: surface ->
`NativeViewport.touchEvent` -> native arbitration -> brush -> the
settled-gesture edge the chrome depends on.

**No coordinate locates a control.** Every control is reached by its semantic id
(`R.id.freeze_to_sculpt`, `R.id.back_to_construction`, `R.id.resume_sculpt`,
`R.id.project_actions_button`) or by its accessor (`undoAction()`,
`redoAction()`, `historyGroup()`). The one place coordinates appear is inside the
viewport gesture itself, which is the repository's existing touch harness
injecting viewport input — nothing is inferred FROM the coordinate.

**No `Thread.sleep` for a product condition.** `awaitIdle` is the autosave
barrier, as the debounce rule requires. `settleLayout()` is the existing shared
layout barrier and is used exactly as every other suite uses it.

## The table

| ID | Scenario | Case | Result |
| --- | --- | --- | --- |
| E2E-SCUNDO-01 | Construction -> Start Sculpting -> stroke -> actual Undo button -> geometry reverts -> Redo -> returns | `e2eScundo01_theRealControlsUndoAndRedoAConstructionSculptStroke` | PASS |
| E2E-SCUNDO-02 | Imported Mesh -> Start Sculpting -> the same real UI sequence | `e2eScundo02_theRealControlsUndoAndRedoAnImportedSculptStroke` | PASS |
| E2E-SCUNDO-03 | two strokes -> Undo twice -> Redo twice through actual controls | `e2eScundo03and04_twoStrokesWalkInOrderAndANewStrokeDropsTheRedoBranch` | PASS |
| E2E-SCUNDO-04 | Undo once -> new stroke -> Redo disabled | same case | PASS |
| E2E-SCUNDO-05 | Back to source -> Resume Sculpt -> history controls retain prior runtime state | `e2eScundo05_backAndResumeKeepTheRuntimeSculptHistory` | PASS |
| E2E-SCUNDO-06 | switch between two bodies with retained Sculpt histories -> each controls only itself | `e2eScundo06_eachBodysControlsActOnlyOnItsOwnHistory` | PASS |
| E2E-SCUNDO-07 | Save after Undo -> reopen -> geometry preserved, history buttons have empty state | `scundo20and21_neitherSaveNorAutosaveEverSerializesTheSculptHistory` | PASS |
| E2E-SCUNDO-08 | existing project/object transform Undo outside Sculpt still works unchanged | `scundo19_theProjectHistoryIsUntouchedBySculptAndStillWorksAfterwards` | PASS |
| E2E-SCUNDO-09 | Reset/restart-from-source clears Sculpt history and Undo cannot cross reset | `e2eScundo09_aResetFromSourceClearsTheHistoryAndUndoCannotCrossIt` | PASS |
| E2E-SCUNDO-10 | history memory-bound device sanity — no crash or runaway allocation | `e2eScundo10_aLongRunOfRealStrokesStaysBounded` | PASS |

`OK (10 tests)` — `SculptUndoTest`, run focused on `emulator-5580`, and again as
part of shard 2 of the authoritative aggregate.

## What each case actually asserts

**E2E-SCUNDO-01.** The `SCUL` bytes before the stroke, after the stroke, after
Undo and after Redo, compared bit-exactly: Undo lands on the pre-stroke bytes and
Redo lands on the post-stroke bytes. Around them, the two controls' enabled state
is asserted at four points — both inert before any stroke, Undo-only after it,
Redo-only after the Undo, Undo-only again after the Redo. `SCULPT_HAS_EDITS` is
asserted `0` after the Undo and non-zero after the Redo, which is `SCUNDO-11` on
a device.

**E2E-SCUNDO-02.** The same sequence over a body imported from
`construction_sentinel.glb`, plus the invariant that matters most for an import:
the document's `IMPT` section is byte-identical after the stroke, the Undo and
the Redo. No sculpt history step touched immutable source truth.

**E2E-SCUNDO-03/04.** Three geometry states (seed, after A, after B) walked
backwards and forwards through the real buttons in order, with the enabled state
asserted at the ends of the stack. Then Undo once and sculpt: Redo goes inert and
stays inert.

**E2E-SCUNDO-05.** Depths and geometry captured mid-stack (one entry on each
side), then *Back to Construction* — where the pair is asserted to be the
Construction history again — then *Resume Sculpt*: same depths, same geometry
bytes, both controls live.

**E2E-SCUNDO-06.** Body A sculpted once, body B added and sculpted twice. B's
history starts EMPTY with both controls inert, which is the per-body claim
stated positively. B is walked back to its seed, then Undo is pressed on an
exhausted stack — and A's entry is still there afterwards, untouched. Returning
to A finds its single entry, with only Undo live.

**E2E-SCUNDO-07** (in the persistence case). After an Undo, the document is
encoded; the autosave checkpoint is byte-compared against it (`assertArrayEquals`)
to show the same canonical document; the file is reopened through the ordinary
atomic load. Geometry equals what was on screen; both sculpt depths are `0`;
re-encoding the reopened project reproduces the saved bytes exactly — an encoder
that leaked one history entry could not. Then a new stroke is taken and is
asserted to be entry **one** of a new runtime history.

**E2E-SCUNDO-08** (in the project-history case). A sphere is applied to build a
Construction step; a stroke, a Sculpt Undo and a Sculpt Redo are taken; the
Construction depth is unchanged. `constructionUndo()` is called directly and
returns `HISTORY_REFUSED_IN_SCULPT`, changing nothing. Then, out of Sculpt, a
real placement change and the real Undo button restores the placement to nine
values compared with `assertArrayEquals`.

**E2E-SCUNDO-09.** A stroke, then the ordinary destructive freeze the
confirmation runs. Both depths `0`, both controls inert, the mesh is the source
bytes again, `SCULPT_HAS_EDITS` is `0`, and pressing Undo moves nothing.

**E2E-SCUNDO-10.** Forty real strokes through the whole touch path. Depth
`<= 32`, bytes `<= 4 MiB`, eviction count `> 0` (so the cap was genuinely
reached rather than never approached), and Undo/Redo still move geometry exactly
afterwards. No crash, no ANR, no runaway allocation.

## The invariants, visible in the device log

The new `FORGESHAPE_SCULPT_HISTORY` line makes the two subtlest contracts
readable rather than only asserted. From the focused run:

```
FORGESHAPE_SCULPT_HISTORY:undo sculptRevision=9  meshRevision=23 edits=0 undo=0 redo=1
FORGESHAPE_SCULPT_HISTORY:redo sculptRevision=10 meshRevision=24 edits=1 undo=1 redo=0
FORGESHAPE_SCULPT_HISTORY_REFUSED:undo:nothing_to_do
```

The first line is `SCUNDO-11` and `SCUNDO-13` together: the sculpt revision went
UP (8 -> 9) on a step that moved geometry BACK to its unedited seed, and
`edits=0` says so. The second is the Redo, revision up again, `edits=1`. The
third is a clean refusal at the oldest state — a named condition, not a generic
failure.

`FORGESHAPE_SCULPT_STROKE_BEGIN:grab:482` is the measured affected-vertex count
the memory evidence is built from.

## A first-run failure worth recording

The first device run had Undo and Redo *working when clicked* while their
**enabled state stayed stale**. The cause was not in the feature: the shared
stroke helper called `NativeViewport.touchEvent` directly and so never reached
`onViewportGestureSettled`, the only moment the Android layer learns a stroke
happened. Making the helper dispatch through the real `SurfaceView` fixed it and
closed a real gap in the harness — a helper that stops one call short of the
production path passes while the actual control is grey. See `TEST_RESULTS.md`.

## Owner retest

Nothing here substitutes for the owner's own device and files. The two standing
`OWNER_REAL_FILE_*_RETEST_PENDING` items are unchanged by this stage; the sculpt
Undo/Redo flow over the owner's own `1 lowpoly.glb` is the retest this report is
returned for.

## No screenshots

None were taken, and none is claimed. Every assertion in this stage is about
byte-exact geometry, control enabled state, stack depth or a document's bytes —
all of which a screenshot would state less precisely than the assertions already
do, and none of which is a visual question. The one visual change (the pair is
drawn in Sculpt) is asserted as `View.VISIBLE` on `historyGroup()` at fourteen
points, plus the pre-existing layout suites that measure that capsule's bounds.
