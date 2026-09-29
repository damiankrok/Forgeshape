# D1 after the fix: Import GLB is refused in Sculpt, and nothing moves

**Result: D1 CLOSED on the tested candidate `f29f482`.** The same three device
cases that were red on the baseline (`BEFORE.md`) are green:

- Import is withdrawn in Sculpt.
- A picker result that arrives in Sculpt is refused with no change.
- The bytes entry and the JNI entry point both refuse by name with no change.
- After Back to Construction, the same file imports exactly as before.

## Run

| Item | Value |
| --- | --- |
| Candidate | `f29f482` (product + test); everything above it is docs-only `[skip ci]` |
| Run | `CI DEVICE` `36643101662`, **attempt 2**, job `109663937803` — `PASS` |
| Startup | capture 1: 23/23 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines, 0 liblog drops |
| Instrumentation | `OK (31 tests)`, 0 failed, 211 s. `ImportGlbSculptGuardTest` 3 (listed FIRST), `ImportedMeshSculptTest` 9, `GlbImportExternalR1Test` 14, `JniBoundaryHardeningTest` 5 |
| Attempt 1 | same run, job `109659734380`: `DEVICE_STARTUP_UNRESOLVED`, no test ran (see `TEST_EVIDENCE.md`) |
| Evidence kept | `after-run-36643101662-attempt2/` (summary, startup capture, instrumentation output, buffer size) |

## Criterion by criterion

| # | What is proved | How |
| --- | --- | --- |
| D1-02 | `Import GLB…` and its `Import a mesh` label are not shown in Sculpt, and both are shown again in Construction | `d1_02_…`, which asserts `isShown()` and `isEnabled()` on the real popover opened through `project_actions_button` in each mode |
| D1-03 | A valid GLB delivered through `onOpenGlbDocumentChosen` (the Activity's picker-result path) in Sculpt changes nothing | `d1_03_…`: `assertEquals` of the full snapshot string before and after |
| D1-04 | The bytes entry (`applyImportedGlbBytes`, below the picker guard) changes nothing, and the JNI entry `importGlbDurable` called directly answers `RefusedInSculpt`, a PROJECT refusal (≥ `IMPORT_COMMIT_BASE`), and changes nothing | `d1_04_…`, with three assertions on the snapshot and the token |
| D1-05 | The active body and the sculpt target are unchanged | Snapshot fields `active`, `sculptTarget` |
| D1-06 | The body ids and body count are unchanged | Snapshot field `ids` (the whole id list) |
| D1-07 | Construction undo depth and sculpt undo depth are unchanged | Snapshot fields `constructionUndo`, `sculptUndo` |
| D1-08 | The fingerprint and the encoded project bytes are unchanged | Snapshot fields `fingerprint`, `project` (length + FNV-1a 64 of `encodeProject()`) |
| D1-09 | The sculpt revision, has-edits, and the active body's visibility and lock are unchanged; the mode stays Sculpt | Snapshot fields `sculptRevision`, `sculptHasEdits`, `activeVisible`, `activeLocked`, `mode` |
| D1-10 | After the real Back to Construction control, the same file through the same picker path adds six bodies after the existing ones; the first imported body is active and is an Imported Mesh; it costs exactly one Construction step; the mode is Construction | `assertLegalImportAfterBackToConstruction`, which ends `d1_03` and `d1_04` |

Each refusal was measured after one real stroke, so the sculpt side had
something to lose: sculpt undo ≥ 1 is a precondition.

## The neutrality table

The state before a refusal is the same deterministic journey the baseline ran.
The baseline run printed its values (`BEFORE.md`), for example
`mode=1 active=1 sculptTarget=1 ids=[1] sculptRevision=8 sculptHasEdits=1
sculptUndo=1 constructionUndo=1 activeVisible=true activeLocked=false`. On the
candidate, after each of the three refusals, the test asserted that every field
is **identical** to the value it had just before.

| Field | Baseline, after the import in Sculpt | Candidate, after the refusal |
| --- | --- | --- |
| mode | Sculpt | Sculpt: identical (asserted) |
| active body | moved 1 → 2 | identical (asserted) |
| body ids / count | `[1]` → 7 bodies | identical (asserted) |
| sculpt target | 1 → 0 | identical (asserted) |
| sculpt revision | 8 → 0 | identical (asserted) |
| sculpt undo depth | 1 → 0 | identical (asserted) |
| Construction undo depth | 1 → 2 | identical (asserted) |
| fingerprint | changed | identical (asserted) |
| project bytes | encode FAILED (`null`) | identical length + FNV-1a 64 (asserted) |
| active visible / locked | unchanged | identical (asserted) |

**Stated gap.** The candidate's own per-field values are not in the kept log.
The logcat dump starts at 23:25:47, after the D1 class, which was listed
first, had already run (`test-logcat-window-start.txt`). The buffer was 64 MiB
(`logcat-buffer-size.txt`), but the dump is taken once at the end of all four
classes. The claim therefore rests on the passing equality assertions and not
on a printed table of candidate values. No second device attempt was spent to
re-print numbers that the assertions already proved.

## Scope held

- **No format change.** No `.forge`, `CADB`, `SCNE`, GLB or corpus byte
  changed. CI FAST's corpus parity step is green on the candidate.
- **No change to what a legal import does.** Its active-body rule, its one
  Undo step and its persistence are the same; `D1-10` asserts them.
- **No automatic exit from Sculpt and no after-the-fact repair.** The import
  does not happen.
- **No Java-owned mode.** `showImportAvailable` and
  `importRefusedWhileSculpting` read `NativeViewport.productMode()` on each
  call. The popover remembers nothing.
