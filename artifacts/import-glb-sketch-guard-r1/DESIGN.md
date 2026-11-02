# IMPORT-GLB-SKETCH-GUARD-R1 — DESIGN

## Why the guard sits at commit time

The import has two halves:
- **Parse:** pure computation over a private buffer, done OUTSIDE
  `g_stateMutex` so a multi-megabyte decode does not stall the render thread.
- **Commit:** the scene mutation, done under the lock.

A mode can change between the two. A sketch can begin while the parse runs,
and the system picker can answer long after the request. So the authoritative
question is asked under the SAME lock, immediately before
`commitImportedGlbScene`. There is no earlier native pre-check to rely on, and
none is needed: refusing after a parse costs a parse and nothing else.

The decision lives in the domain (`forgeshape_import_commit`):
- `importModeRefusal(ImportModeFacts)`
- `commitImportedGlbSceneInMode(parsed, scene, history, facts, report)`

It takes two plain facts rather than the sessions themselves. That keeps the
module independent of what it protects, and lets the host self-tests drive the
exact decision JNI runs, with facts read from real `SketchSession` /
`SculptSession` objects. JNI fills the facts under the lock:
`sculptSession().inSculptMode()` and `sketchSession().active()`.

## Status map and precedence

| Wire | Ordinal | Name | Change |
| --- | --- | --- | --- |
| 0 | 0 | `Ok` | unchanged |
| 1001 | 1 | `NothingToImport` | unchanged |
| 1002 | 2 | `TooManyObjects` | unchanged |
| 1003 | 3 | `RejectedGeometry` | unchanged |
| 1004 | 4 | `RefusedEditInProgress` | unchanged |
| 1005 | 5 | `RefusedInSculpt` | unchanged |
| **1006** | **6** | **`RefusedInSketch`** | **appended** |

- **Base.** `kImportCommitStatusBase` stays 1000. File (parse) refusals keep
  their own `GlbImportStatus` ordinals below 1000, so a malformed file is
  never reported as a mode refusal, nor the reverse.
- **Order.** Sculpt is asked FIRST, then a sketch, then the commit's own
  checks (edit in progress, empty, capacity, geometry). Sculpt and a sketch
  cannot coexist; the order keeps 1005 exactly what it was.
- **Native helpers.**
  - `glbCommitStatusToken`'s upper bound is moved to `RefusedInSketch`.
  - `glbCommitStatusCategory` puts it in the default category. That category
    is never shown, because Java branches on the status first.
- **Java.**
  - `NativeViewport.IMPORT_REFUSED_IN_SKETCH = IMPORT_COMMIT_BASE + 6`.
  - `applyImportedGlbBytes` maps 1006 to
    `status_glb_import_refused_in_sketch` ("Import is not available while a
    sketch is open. Your project and your sketch are unchanged."). It never
    uses the corrupt-file message.

## What "a sketch is open" means

`sketchSession().active()` means state Editing or Ready. One process-scoped
session carries every flavour:
- a new sketch and a face sketch;
- a staged Edit Sketch and Edit Feature;
- a reused or edited retained sketch;
- Revolve (a Ready sub-mode);
- a Surface-purpose sketch (its purpose is set only after `begin` succeeds);
- the CAD bootstrap's first sketch.

All are refused. Refusing reads nothing from the draft and writes nothing to
it. The same sketch can then be finished, committed or cancelled, which host
`IMPORT_SK_03` and device `sk03` / `sk04` exercise.

## Race analysis

| Path | Window | Outcome |
| --- | --- | --- |
| Row tap | The row is drawn from `ProjectDrawerPolicy.importShown(sculpting, sketching)` on every `syncFromNative` and again when the drawer opens. The chooser's confirm tap can begin a sketch without an immediate refresh. | `onImportGlbRequested` re-reads native before launching the picker. |
| Late picker answer | The user begins a sketch while the picker (another app) is up. | `onOpenGlbDocumentChosen` re-reads native before reading the file. |
| Bytes seam / direct JNI | No Java guard in front. | The lock-held native check refuses with 1006. |
| Native parse ↔ commit | A sketch begins on another thread between the parse and the lock. | The facts are read under the lock, so they cannot change before the commit. |

No Java flag mirrors the sketch mode. `isSketching()` and `isSculpting()`
ask native on every call.

## Held open (OWNER), recorded and not decided

- **CR-03 Q2:** the support chooser up without a sketch. The import is still
  accepted there, as before. The Freeform guard refuses on the chooser, but
  adopting that for import is a semantic decision. No chooser input exists in
  `ImportModeFacts` or `importShown`.
- **CR-03 Q3:** a direct call with no project (Home). No `!hasProject()`
  refusal was added. The CAD bootstrap with its first sketch open is a live
  sketch, so it IS refused, as a consequence of the sketch rule and not as a
  bootstrap rule. A chooser-only bootstrap and Home are unchanged and are
  unreachable from the UI, because the mark is withdrawn.
- **`.gitattributes`:** OWNER-REVIEW-PENDING. This stage does not change it
  (see `BEFORE.md`).
- **Open OWNER decisions:** OD-01 (A2), OD-03, OD-05/06, OD-16, OD-22, OD-27,
  OD-29.
