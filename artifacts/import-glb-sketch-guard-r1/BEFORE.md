# IMPORT-GLB-SKETCH-GUARD-R1 — BEFORE (written before any product edit)

NO CURRENT FULL MASTER PLAN COMPLIANCE CLAIM. The canonical v4.1.61 and the
current OWNER decision bodies are still absent.

## Refs

Verified by `git fetch` on 2026-11-02.

| Ref | SHA |
| --- | --- |
| Baseline `feature/cad-face-supported-placement-r1` | `8eb94ce77124b788e32a1f864b089f891aa576d7` (matches) |
| `main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` (matches) |
| `feature/cad-r2-c2-picking-seam-r1` | `70a6c14f703bfda2ab689b855126c1d12a541e41` (matches) |
| `audit/function-council-r2-closeout-r1` | `7c2fc913d8c7081ab36b2b0385904ee02d7e7245` (matches) |
| New branch | `feature/import-glb-sketch-guard-r1`, created from exactly `8eb94ce`; the name was free on origin |

`7c2fc91` and `70a6c14` are ancestors of the baseline. The tree was clean.

## Context

**CR-03** (Council R2 closeout, `CRITICAL_REPROS.md`) reproduced at E1:
- Import GLB is accepted while a sketch is open in a project.
- The import creates a body, makes it active, records a Construction step and
  moves the fingerprint.
- That step cannot be undone while the sketch stays open.
- The sketch itself is not corrupted.

The closeout also recorded two OWNER questions, which this stage keeps open:
- **Q2:** the support chooser open without a sketch.
- **Q3:** a direct call on Home or a bootstrap with no project.

**FUNCTION-COUNCIL-C1 D1** solved the same shape for Sculpt, in three layers:
`RefusedInSculpt` under the commit lock, both Java callbacks, and the row
withdrawn. This stage adds the sketch half beside it.

## The current path (lines read at `8eb94ce`)

| Step | Where | Sketch question asked today |
| --- | --- | --- |
| Drawer row | `ProjectActionsPopoverView.showImportAvailable` (`:202-206`), only caller `EditorWorkspaceView.java:2836` `showImportAvailable(!sculpting)`. Opening the drawer (`onProjectActionsRequested`, `:5430-5438`) does **not** refresh it. | none (Sculpt only) |
| Row tap | `ProjectDrawerPolicy.perform` → `onImportGlbRequested` (`EditorWorkspaceView.java:5853`): `importRefusedWhileSculpting()`, then `transferHost.requestOpenGlbDocument()` | none |
| Picker answer | `ForgeShapeActivity.onActivityResult` (`:510-539`, `REQUEST_OPEN_GLB_DOCUMENT` 4105) → `onOpenGlbDocumentChosen(Uri)` (`:5866`): cancel, Sculpt re-check, `ProjectTransfer.readFrom`, `applyImportedGlbBytes` | none |
| Bytes | `applyImportedGlbBytes` (`:5896`) → `NativeViewport.importGlbDurable`. Only 1005 (`IMPORT_REFUSED_IN_SCULPT`) has a branch; any other refusal shows the generic "could not import that file" with a category | none |
| JNI | `importGlbDurable` (`forgeshape_jni.cpp:8610`): parse outside the lock, then under `g_stateMutex`: `inSculptMode()` → `RefusedInSculpt`, else `commitImportedGlbScene` | **none** |
| Domain | `commitImportedGlbScene` (`forgeshape_import_commit.cpp`) refuses only `history.editInProgress()` | cannot see either session |

**Other doors.**
- `importGlbPreview` and the round-trip diagnostic create nothing.
- The manifest has no VIEW or SEND filter.
- Open / Open File / Recover load `.forge` only, after cancelling a sketch.

So `importGlbDurable` is the only route into the commit.

**`ImportCommitStatus` ordinals** (`forgeshape_import_commit.h:42-67`, wire
code `1000 + ordinal`):

| Ordinal | Name | Wire code |
| --- | --- | --- |
| 0 | `Ok` | — |
| 1 | `NothingToImport` | 1001 |
| 2 | `TooManyObjects` | 1002 |
| 3 | `RejectedGeometry` | 1003 |
| 4 | `RefusedEditInProgress` | 1004 |
| 5 | `RefusedInSculpt` | 1005 |

- `kImportCommitStatusBase = 1000` (`forgeshape_jni.cpp:8601`), mirrored by
  `NativeViewport.IMPORT_COMMIT_BASE` (`:1242`).
- `glbCommitStatusToken` bounds the ordinal at `RefusedInSculpt` (`:8672`).

**`SketchSessionState`** (`forgeshape_sketch_session.h:93-97`) is `Inactive`,
`Editing` or `Ready`. `active()` means `state_ != Inactive` (`:485`).
- One process-scoped `sketchSession()` serves every flavour: new sketch, face
  sketch, Edit Sketch (`beginEdit`), Edit Feature, reuse, retained-sketch
  edit, Revolve (a Ready sub-mode) and the CAD bootstrap.
- A Surface sketch's purpose is set only after `begin` succeeds.
- The support chooser is a SEPARATE object (`supportChooser().active()`), and
  `sketchSession().active()` is false while only the chooser is up.

**Precedents for refusing during a sketch** (all under `g_stateMutex`, keyed on
`sketchSession().active()`): `sceneSelectBody`, `sceneAddBody`,
`sceneDeleteBody`, object commands, Construction Undo/Redo, freeze. The
Freeform guard alone also refuses on the chooser.

**Invariants a refusal must keep** (the D1 contract restated): no change to:
- scene body ids or count, or the active body;
- the `ObjectId` allocator;
- Construction Undo/Redo, the fingerprint, or the encoded `.forge` bytes;
- Sculpt state;
- the sketch's state, draft, candidate or selection.

## RED on this baseline (reproducible)

**Host (E1).** The closeout's CR-03 diagnostic, rebuilt and re-run here
unchanged.
- It copies the JNI import body. That body is **byte-identical** between
  `70a6c14` and `8eb94ce`: a `diff` of the extracted function is empty, and
  `forgeshape_import_commit.*` and `forgeshape_sketch_session.h` did not
  change.
- Build: `bash artifacts/function-council-r2-closeout/diagnostics/CR-03/build.sh`
  prints `BUILD_OK`. Run: `cr03 A..E`, all exit 0. Outputs are in
  `evidence/red/cr03_scenario_{A..E}.out`.
- The GLB is a legitimate 1648-byte file written by the product's own
  `exportSceneAsGlb`.
- **Scenario A:** a sketch open in Editing with 1 rectangle, then the import.
  - `importGlbDurable_return=0`, so the import was accepted.
  - Bodies 1 → 2 and active 1 → 2.
  - Undo 0 → 1.
  - Fingerprint `22a55196…` → `937217cf…`.
  - The sketch is still Editing with 1 entity.
  - `jni_undo_while_sketch=REFUSED:in_sketch`.
- **D (Sculpt control):** refused with 1005.
- **C (chooser) and E (bootstrap):** the import is accepted. These are the
  OWNER questions and are recorded only.

**Device (real Android path).** `ImportGlbSketchGuardTest` is committed BEFORE
any product edit and run on `CI DEVICE` against this unchanged product.
- It covers:
  - the row seen through a real tap on the mark, in Editing and Ready;
  - a picker opened legally by real taps, then a sketch, then the picker's
    answer;
  - the bytes seam and the JNI entry, in Editing, in Ready and in a staged
    Edit Sketch;
  - orientation × font scale.
- The expected wire code is pinned as `IMPORT_COMMIT_BASE + 6`.
- The RED run id and results are in `TESTS.md`. The picker itself is replaced
  by a recording `ProjectTransferHost`, so the system picker UI is not shown.
  The CALLBACK is the one `ForgeShapeActivity.onActivityResult` calls.

## `.gitattributes` scope (OWNER-REVIEW-PENDING; not changed by this stage)

| Path | `git check-attr whitespace` |
| --- | --- |
| `artifacts/cad-face-supported-placement-r1/evidence/device-run-41084823234/job.log` (raw evidence) | unset |
| `artifacts/function-council-r2-closeout/diagnostics/CR-05/transcription_check.out` (raw output) | unset |
| `artifacts/function-council-r2-closeout/sources/FORGESHAPE_MASTER_PLAN_v4.1.0.md` (OWNER source copy) | unset |
| `artifacts/cad-face-supported-placement-r1/SUMMARY.md`, `artifacts/function-council-r2-closeout/SUMMARY.md` (authored dossiers) | unspecified, so checked |
| `PROJECT_STATUS.md`, `EditorWorkspaceView.java`, `forgeshape_jni.cpp`, this file | unspecified, so checked |
| `artifacts/function-council-r2/diagnostics/I-vs-B/edit_tsv.py`, `.../seatB/diag_math.cpp` (**authored** diagnostic sources) | **unset** |

CI FAST still checks product code, tests, scripts and authored dossiers.

**Recommendation (not applied).** `artifacts/**/diagnostics/**` also exempts
AUTHORED diagnostic sources, not only machine outputs. A narrower rule would
cover only output files (`*.out`, `*.txt`, `*.log`, `*.tsv`). That is a policy
change, so it is left to the OWNER. This stage's own evidence stays under
`evidence/`, and its authored files must pass `git diff --check` normally.

## Out of scope (stated before any edit)

- **Chooser-only import (Q2):** observed; no new semantics.
- **No-project / bootstrap import (Q3):** no `!hasProject()` refusal added.
  A bootstrap that has a SKETCH open is a live sketch, so it falls under this
  guard as a consequence. No bootstrap path is created.
- **Sculpt (D1):** unchanged, and it keeps precedence.
- **Formats and engine:** export, face placement, kernel, renderer, `.forge`,
  `CADB`, codec, fixtures, Manifold, Vulkan and build dependencies.
- **Open OWNER decisions:** OD-01, OD-03, OD-05, OD-06, OD-16, OD-22, OD-27,
  OD-29.
