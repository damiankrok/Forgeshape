# FUNCTION-COUNCIL-C1: D1 Import GLB in Sculpt, summary

**Result: `PASS-FUNCTION-COUNCIL-C1-D1`.**

| Item | Value |
| --- | --- |
| Baseline | `origin/main` = `a1b50f261cfb761d0919a0f3b1d70f055c263329`, confirmed before any change |
| Branch | `fix/function-council-d1-import-sculpt-guard-r1`, created from exactly `origin/main` |
| Reproduction commit | `6ee8557` (baseline + the D1 test only) |
| Tested candidate | **`f29f482`** (product + test) |
| Above the candidate | docs-only `[skip ci]` commits; no source file changes after `f29f482` |
| FullSharded | **NOT RUN** |

## What was wrong

In Sculpt, `Import GLB…` was offered, and nothing below it asked about the
mode. Importing a valid six-node file on the baseline did all of the
following (`BEFORE.md`, run `36641798987`):

- created six bodies and made the first one active;
- dropped the sculpt target, revision and undo to 0;
- recorded a Construction step, which Sculpt otherwise refuses;
- left the project **unencodable** (`FORGESHAPE_PROJECT_ENCODE_FAIL:UnresolvedReference`),
  so Save and autosave could not write it.

The mode stayed Sculpt throughout, and the JNI entry answered `Ok`.

## Root cause

The import path was the one scene-creating path without the Sculpt question.

- **Every other path asks.** Add, delete, the object commands and
  Construction Undo/Redo ask `sculptSession().inSculptMode()` under the state
  lock at the JNI boundary.
- **Import did not.** `importGlbDurable` went straight to
  `commitImportedGlbScene`, and that domain function is platform-neutral and
  cannot see the mode.
- **The chrome did not either.** It never withdrew the row.

## The fix: three layers, one owner of the mode

1. **Chrome.** `ProjectActionsPopoverView.showImportAvailable(!sculpting)` runs
   from `syncFromNative`, so the `Import GLB…` row and its section label are
   ABSENT in Sculpt and back in Construction. The surface stays
   `View`-stateless.
2. **Java callback.** `onImportGlbRequested` and `onOpenGlbDocumentChosen`
   refuse in Sculpt before the file is read, and `applyImportedGlbBytes` maps
   the native refusal. Each reads `NativeViewport.productMode()` per call;
   there is no Java mode field. The status line says "Import is not available
   while sculpting. Your project is unchanged." and the diagnostics ring gets
   `GLB_IMPORT_REFUSED RefusedInSculpt`.
3. **Native.** `importGlbDurable` asks `inSculptMode()` under the SAME lock as
   the commit and answers the new, appended
   `ImportCommitStatus::RefusedInSculpt` (ordinal 5, stable token
   `RefusedInSculpt`) without touching the scene or either history. The token
   bound in `glbCommitStatusToken` moved to the new last value.

Nothing exits Sculpt automatically, and nothing is repaired after the fact:
the import does not happen.

## Evidence

- **Tier 1 (host).** `GLTF` 282, `SCENE` 175 and `PROJECT` 256 checks, all 0
  failed on both the baseline and the candidate.
- **CI FAST.** `36643103708` on `f29f482` was green in 3 min 50 s, with corpus
  parity 44/44.
- **CI DEVICE.** `36643101662` on `f29f482`. Attempt 1 was
  `DEVICE_STARTUP_UNRESOLVED`, a dropped startup capture before any test.
  Attempt 2, the one allowed re-run, **passed**: 23/23 tokens, `OK (31 tests)`,
  with the D1 class first.
- `AFTER.md` maps each of D1-02..D1-10 to its assertion. `TEST_EVIDENCE.md`
  has the tiers, the attempts and the infrastructure classification.

## Scope

- **Untouched:** D2–D7, CAD, Sculpt semantics, `.forge`/`CADB`/GLB formats,
  Manifold, the renderer, scripts, workflows and the test harness.
- **Legal import unchanged.** Import in Construction behaves as before and is
  asserted (D1-10).

## Recorded debt, not fixed here

- **Import GLB during an open sketch** (source-level, **not runtime-verified**).
  Inside an open project the Project button stays drawn while a sketch is
  open (`GlobalToolbarView.showBootstrap` withdraws it only for the bootstrap).
  Creation is withdrawn there (`showCreationAvailable(… && !sketching)`), and
  `sceneAddBody` refuses `in_sketch`, but import has no sketch question.
  Whether it misbehaves has not been reproduced. It is the same shape as D1,
  for another mode, and needs its own task.
- **Candidate-run values are not printed.** The candidate device run's logcat
  window began after the D1 class had run, so the candidate's per-field values
  exist only as passing equality assertions (`AFTER.md`, stated gap). A later
  harness change could dump logcat per class.
- **D2–D7** stay exactly as `artifacts/function-council-r1/COUNCIL_FINDINGS.md`
  records them.

## Council canonicalization

The eight `FUNCTION-COUNCIL-R1` artifacts are on this branch, copied by
content from `fdc3818` and byte-identical, with no merge or cherry-pick of the
audit branch. `PROJECT_STATUS.md` records:

- the audit, with its caveats (the hard-stop overrun and the partial seats);
- TEST-OWNER-04 lean tiers as **ADOPTED**;
- D1 closed by this task;
- D2–D7 as debt.
