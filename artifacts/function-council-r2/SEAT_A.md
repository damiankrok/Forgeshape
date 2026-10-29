# FUNCTION-COUNCIL-R2 — Seat A: Product, user journeys, plan reconstruction

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` = `f3db953`). Research only:
nothing under `app/`, `testdata/`, `scripts/`, `.github/`, Gradle files or the owner docs was
modified. No Gradle was run by this seat. Requirement rows: `work/req_A.tsv` (86 rows,
`APP-001`..`APP-086`). Sources and gaps: `SOURCE_REGISTER.md`.

## Scope

The app shell and the journeys that cross features: cold launch → Home → New Project (CAD,
Sculpt, Freeform, Surface) → edit → Save Project / Save Copy… / Open Saved Project / Open File… /
Export → leave → reopen → recover, and Import GLB → editable body. Also: the ForgeShape mark and
project drawer, the unsaved-changes and recovery questions, the Settings page, System Back,
Share Diagnostics, status-line wording, and Activity recreation as it affects these journeys.
Domain internals (CAD regeneration, sculpt, codec layout, renderer, input) belong to other
seats and were read only where a shell journey depends on them.

## Sources read

Exact paths, line counts and revisions are in `SOURCE_REGISTER.md`. Primary:
`CLAUDE.md` (`911f5c3`), `PROJECT_STATUS.md` (`70a6c14`; Owner Decision Baseline §2486, Known
Issues §4018, Next Stage §4851), `PRODUCT.md` (`9144169`; lines 131-202, 299-303, 1988-2090),
`artifacts/function-council-r2/BEFORE.md`, `artifacts/cad-a3-app-h1/HOME_FLOW.md`,
`artifacts/function-council-r1/COUNCIL_FINDINGS.md`, `artifacts/function-council-c1-d1/SUMMARY.md`,
`artifacts/cad-r2-c2-picking-seam/{SUMMARY,TESTS}.md`, `artifacts/cad-vertical-slice-r1/FULLSHARDED_C4.md`,
`artifacts/modeling-foundations-r1/SUMMARY.md`. **Not available:** Master v4.1.61, OWNER_DECISIONS_*,
MASTER_PLAN_HISTORY_*, PROJECT_STATE v4.1.62 addenda, Master v4.1.0. Horizons are reconstructed
(H0 = E2E-R1A/B/C, Stage 022/023 persistence/transfer/export, UI-OWNER-01/03; H1 = APP-H1,
SKETCH-UX-R1, MODELING-R1-OWNER-CORRECTION, MODELING-FOUNDATIONS-R1, UI-PREF-R1, IMPORT-01A/B;
H2 = explicitly "Not this stage" or absent by design).

## Inspected code paths

Java (`app/src/main/java/com/forgeshape/app/`):

| File:lines | What |
| --- | --- |
| `ForgeShapeActivity.java:25-60, 75-140, 268-280, 326-360, 402-430, 446-567, 569-584` | onCreate, Back dismissal (`onBackPressed`, `BackDismissal`), `requestTheme` → `recreate()`, onResume/onStop checkpoint, debug key hook, SAF transfer host, `onActivityResult`, onDestroy |
| `AndroidManifest.xml:31` | `configChanges="orientation|screenSize|smallestScreenSize|keyboardHidden|screenLayout|density"` |
| `StartPageView.java`, `HomeView.java:47-79`, `NewProjectChooserView.java:45-95` | start pages |
| `ProjectDrawerPolicy.java:68-169`, `ProjectActionsPopoverView.java:78-257` | drawer rows, `perform`, `markShown`, `newSketchShown`, `showSlotState`, `showImportAvailable` |
| `EditorWorkspaceView.java:271-279` | `persistedFingerprint`, `everPersisted`, `pendingLeave` (view fields) |
| `EditorWorkspaceView.java:1021-1030, 1112-1227` | recovery offer at construction; `hasDismissibleSurface`; `dismissTopmostSurface` |
| `EditorWorkspaceView.java:1999-2131` | `refreshShellPhase`, Home handlers, Settings open/close |
| `EditorWorkspaceView.java:2358-2590` | CAD / Sculpt / Freeform / Surface new-project handlers, `onBackToHomeRequested` |
| `EditorWorkspaceView.java:2597-2706` | `projectDirty`, `leaveProjectFor`, `proceedToLeave`, unsaved Save/Discard/Cancel |
| `EditorWorkspaceView.java:2796-2830` | drawer/toolbar availability (`showBootstrap`, `showImportAvailable(!sculpting)`) |
| `EditorWorkspaceView.java:5411-5523, 5579-5703` | drawer open, Save Project, `saveProjectToSlot`, Open Saved Project, recovery offer / Recover / Discard |
| `EditorWorkspaceView.java:5709-6153, 6190-6198, 6337-6346` | Save Copy, Open File, Export, Share Diagnostics, Import GLB, transfer result handlers, renderer restart, `projectSummary`, `showStatus` |
| `AutosaveController.java` (all) | `performCheckpoint`, `noteProjectPersisted`, coalescing |
| `ProjectSlot.java:43-63`, `ProjectTransfer.java:96-158` | slot in `getFilesDir()`, SAF intents (`ACTION_CREATE_DOCUMENT`) |

Native (`app/src/main/cpp/`): `forgeshape_jni.cpp` `encodeProject` 8387, `loadProject` 8440-8496
(sketch cancelled at 8478-8482 before `loadProjectDocument`), `exportGlb` 8506, `importGlbDurable`
8560-8606 (Sculpt guard only, 8590-8597), `validateProject` 8898, `projectFingerprint` 8936,
`projectOpen` 8957, `closeProject` 8968-8990, `sceneAddBody` in_sketch refusal 3246-3265,
`sceneDeleteBody` 3311; `forgeshape_import_commit.cpp:50-63` (`commitImportedGlbScene` guards only
`editInProgress`); `forgeshape_project_state.cpp:118-133, 247-262` (`runtimeCanEvaluateProject`,
`loadProjectDocument`); `forgeshape_gltf_export.cpp:205` (sculpt mesh only when kind == Sculpt).

## Test coverage of the domain (what it actually asserts)

| Layer | Tests | Asserts | Evidence |
| --- | --- | --- | --- |
| Host native | 25 suites; relevant: PROJECT (256), SCENE (175), CONSTRUCTION_HISTORY (147), GLTF_EXPORT (93), GLTF_IMPORT (189 incl. `IMP01A_*`), CAD_A3 (66 incl. `CADA3_BOOT_*`), RENDER_RECOVERY (24) | codec round trips, fail-closed decode, empty-scene bootstrap, durable import commit, export bake | **E1**: `fc2/host_selftests.txt` `HOST_SELFTESTS_OK (4392 checks, 0 failed)` at `70a6c14` |
| Corpus | 72 `.forge` fixtures | decode verdict per fixture (e.g. `truncated_v1` Truncated, `unsupported_major_v1` UnsupportedMajor) | **E1**: `fc2/host_corpus_verdicts.txt`, `CORPUS_FIXTURES 72` |
| JVM | `ProjectDrawerPolicyTest` (9), `AppPreferencesTest` (13), `AppThemeTest` (10), `EditorUiStateTest` (10), `DiagnosticLogTest` (11), `WorkspaceLayoutModeTest` (15) … 20 classes / 196 | drawer order, `perform` closes first, mark/New Sketch predicates; preference parsing; palette set | **E3**: XML results 2026-10-28T18:46Z, Gradle `:app:testDebugUnitTest UP-TO-DATE` on `70a6c14` (`fc2/gradle.txt`) |
| Device (this product tree) | `HomeFlowTest` E2E-APPH1-01..12, `EditorWorkspaceProjectActionsTest` (9), `OwnerProjectShellSketchSupportTest` devOss01..05 | cold Home, four-option chooser + Back, CAD bootstrap → first body, New Sculpt, Open File from Home, cancel/corrupt open at Home, unsaved Cancel/Discard/Save(Open File path), Back in Home/chooser/bootstrap/unsaved, recreation at Home/bootstrap; slot save/refusals/fresh history; mark → left drawer → New Sketch | **E3**: `CI DEVICE 40782861799` @`30b561b` (67/67) |
| Device (older trees) | `ProjectAutosaveRecoveryTest` FSR1B-01..09, `ProjectTransferTest` FSR1B-10..13/18, `ProjectProcessDeathTest` stage1-8, `SettingsPreferencesTest` UIPREFR1-*, `DiagnosticsAndRendererLossTest`, `ImportedMeshSculptTest`, `ImportGlbSculptGuardTest` | autosave policy, recovery offer/quarantine/recover/discard, SAF copy/open/cancel, real process death, preferences byte-identity | **E3**: `CI FULL SHARDED 36607747079` @`fa0b6b4` (older shell; Freeform/Surface New Project added later); `FreeformOwnerTest`/`SurfaceOwnerTest` in `CI DEVICE 37472535341` @`30833aa` |
| Release guard | `RELEASE_SELFTEST_GUARD` | — | **not obtained**: `fc2/release_guard.txt` = `ERROR ANDROID_HOME/ANDROID_SDK_ROOT is not set` (environment, not a product verdict) |

Coverage blind spots relevant to findings: no test drives Open Saved Project through the unsaved
question's Save (A-01); none leaves a project after Recover (A-02); none reopens the slot from a
cold Home (A-03); none imports during an open sketch (A-04); `SettingsPreferencesTest.uiprefr1_09`
asserts the dirty flag "did not move" across a palette recreation but starts from a never-saved
(already dirty) project, so it cannot see A-05; no test recreates the Activity while a picker is
open (A-06); `HomeFlowTest._11` covers Back only in shell phases, not inside an in-project sketch
(A-10); the `HomeFlowTest._12` name says the chooser survives a recreation but it asserts only Home
and the bootstrap.

## Confirmed findings

**A-01 — P1 — "Save and continue" before Open Saved Project destroys the saved project it is
about to open.** `EditorWorkspaceView.java:2665-2678` (`onUnsavedSaveRequested`) calls
`saveProjectToSlot()` (5458-5471: `ProjectSlot.write` 5461, `ProjectCheckpoint.clear` 5467) and
then `proceedToLeave(LEAVE_OPEN_SLOT)` → `openSavedProjectSlot()` (2656-2657, 5483), which reads
back the slot it just overwrote.
Repro: Save Project (slot = A) → edit or create another project B → drawer → Open Saved Project →
"Save and continue". Result: slot = B, checkpoint deleted, B reopened with history cleared; A is
gone. The prompt reads "nothing is replaced until you decide" (`strings.xml:62`).
Impact: silent loss of the user's only saved project on the most natural answer. Evidence E2;
no test (HomeFlowTest `_10` covers the Save path only for Open File). Verdict **DEFECT-CONFIRMED**.
Fix (isolated): when `pendingLeave == LEAVE_OPEN_SLOT`, do not offer Save-and-continue (offer Save
Copy… / Discard / Cancel), or refuse it by name.

**A-02 — P1 — Recovered work is protected nowhere.** `onRecoverRequested`
(`EditorWorkspaceView.java:5652-5687`) deletes the checkpoint (5675), sets the autosave baseline to
the recovered fingerprint (5676 → `AutosaveController.noteProjectPersisted`) and marks the project
persisted (5679). `AutosaveController.performCheckpoint` skips when `everCheckpointed &&
fingerprint == checkpointedFingerprint`, and `projectDirty()` (2597-2602) answers false.
Repro: edit, kill the process, relaunch, Recover → (a) New Project… asks nothing and
`closeProject` discards it, or (b) background the app (onStop writes nothing for the unchanged
fingerprint) and let the process die → the work is in neither the slot nor a checkpoint.
PRODUCT.md:193-194 documents "recovered is not asked about", but CLAUDE.md "Autosave protects
work" is violated by the combination. Evidence E2. Verdict **DEFECT-CONFIRMED**. Fix: on Recover keep
the checkpoint and call `noteProjectUnpersisted()` instead of the two persisted notes.

**A-03 — P1 — The saved project cannot be reopened from Home.** Home offers New Project and Open
File… only (`HomeView.java:47-79`, PRODUCT.md:133-138); the slot lives in `getFilesDir()`
(`ProjectSlot.java:63`), unreachable through SAF; Open Saved Project exists only in the drawer of an
open project. After a relaunch the path is: New Project → Sculpt/Freeform (a throwaway project) →
drawer → Open Saved Project → unsaved question (a never-stored project is dirty by definition,
2597-2602) → Discard (Save triggers A-01). `home_prompt` ("Open a saved project file…",
`strings.xml:28`) suggests otherwise. Evidence E2. Verdict **PARTIAL** (journey incomplete; Home's
"exactly two" is an APP-H1 decision). Fix: an Open Saved Project row on Home when
`ProjectSlot.exists`. Owner decision needed.

**A-04 — P2 — Import GLB is offered and accepted while a sketch is open inside a project.**
CLAUDE.md: while a sketch is drawn, creation and body switching are refused below JNI and withdrawn
above it. `EditorWorkspaceView.java:2823` draws the import row whenever `!sculpting`; the mark is
drawn (`ProjectDrawerPolicy.markShown`, `GlobalToolbarView.java:681-687`); `importGlbDurable`
guards Sculpt only (`forgeshape_jni.cpp:8590-8597`); `commitImportedGlbScene` guards only
`editInProgress` (`forgeshape_import_commit.cpp:61-63`) — while `sceneAddBody` does refuse
`in_sketch` (jni 3246-3265). **E1** host diagnostic (scratch, never in the build):

```
cd /tmp/claude-0/fc2-scratch/seatA
R=/home/user/Forgeshape; OBJS=$(ls $R/build/host-selftests/obj/*.o | grep -v _selftest.o | grep -v host_selftest_main)
g++ -std=c++17 -O1 -I$R/app/src/main/cpp -I$R/app/src/main/cpp/third_party/manifold/include \
    diag_import_during_sketch.cpp $OBJS $R/build/host-selftests/kernel/libmanifold.a -lpthread -o diag_import_during_sketch
./diag_import_during_sketch
glb_bytes=1648 export=0 parse=0 sketch_begin=0 sketch_active=1
commit=Ok bodies_before=1 bodies_after=2 active_before=1 active_after=2 undo_depth=1
SEATA_IMPORT_DURING_SKETCH=ACCEPTED
```

(objects from the audit's fresh host build of `70a6c14`; the diagnostic calls
`sketchSession().begin(Workplane::XY)` then the same `commitImportedGlbScene` the JNI calls.) This
re-checks the FUNCTION-COUNCIL-C1-D1 recorded debt: still present. Evidence E1 (domain) + E2 (JNI,
Java). Verdict **DEFECT-CONFIRMED** (rule violation); the consequence for the staged sketch is a
hypothesis (H-A2). Fix: refuse in `importGlbDurable` when `sketchSession().active() ||
supportChooser().active()`, and `showImportAvailable(!sculpting && !sketching && !chooser)`.

**A-05 — P2 — The "saved" baseline is lost on every Activity recreation (FUNCTION-COUNCIL-R1 D3,
re-checked, still present and wider).** `everPersisted` / `persistedFingerprint` are view fields
(`EditorWorkspaceView.java:271-272`) not carried by `EditorUiState.carryAcrossRecreation`. Triggers:
a palette change (`ForgeShapeActivity.requestTheme` 268-280 → `recreate()`), and system recreation
for configuration changes the manifest does not absorb (`AndroidManifest.xml:31` lacks `uiMode`,
`fontScale`, `locale`, `keyboard`, `navigation` — dark-mode toggle, font size, attaching a hardware
keyboard on a desktop/Chromebook window). Repro: Save Project → Settings → choose another palette →
New Project… → the unsaved question appears for a saved project. Evidence E2; the existing test is
blind (see coverage). Verdict **DEFECT-CONFIRMED**. Fix: move the baseline into `EditorUiState`
(process-scoped) or recompute it from the slot's fingerprint.

**A-06 — P2 — A Save Copy or Export whose destination was chosen can silently write nothing,
leaving an empty file.** `pendingCopyBytes` (6004) and `pendingGlbBytes` (5778) are view fields; if
the Activity is recreated while the system picker is open (process death, or an A-05 configuration
change), `onActivityResult` reaches a new workspace whose pending bytes are null and the result is
logged as a cancel (6050-6055, 5788-5793) with no status, while `ACTION_CREATE_DOCUMENT`
(`ProjectTransfer.java:96, 123`) has already created the empty document. The code's own comment
(5711-5713) states the intent to never let the user "watch an empty file appear there". Evidence
E2. Verdict **DEFECT-CONFIRMED** (mechanism proven from source; the trigger is a standard lifecycle
event, not reproduced on a device). Fix: keep the pending kind in `onSaveInstanceState`/process
scope and re-encode on return, or report "nothing was written" and delete the empty document.

**A-07 — P2 — Leaving a project discards an open sketch / staged edit without a question.**
`projectDirty()` is fingerprint-only (2597-2602); `closeProject` (jni 8977-8980) and `loadProject`
(8478-8482) cancel the sketch. Consistent with the "sketch is volatile" rule, but a long sketch in a
clean project is dropped by New Project… / Open… with no prompt. Evidence E2. Verdict **PARTIAL**;
owner decision.

**A-08 — P3 — Wrong failure text for an Open request.** When the picker cannot launch for Open
File, `EditorWorkspaceView.java:2090` and `:2652` show `status_project_copy_failed` ("Could not
write the copy…"). E2. **DEFECT-CONFIRMED**. Fix: a dedicated string.

**A-09 — P3 — Stale descriptions.** `home_new_project_description` (`strings.xml:29`) names only CAD
and Sculpt (the chooser offers four); `settings_home_description` (`strings.xml:316`) omits Tool
Labels; `NewProjectChooserView.java:26-27` javadoc says Back from an open project returns to the
workspace, but `proceedToLeave` (2636-2645) closes the project first (PRODUCT.md is right);
PRODUCT.md:1990-1992 still describes "a project control beside Export" with two actions;
PROJECT_STATUS.md:3379 describes HomeFlowTest's CAD journey as a "spatial plane tap-tap" (the test now
opens flat XY directly), and PROJECT_STATUS.md:3524 "Current evidence summary" still names
`CAD-UX-S1` as the latest run. E2. **DOC-DIVERGENCE**.

**A-10 — P3 — System Back is not one step inside an in-project sketch / mode, nor on the recovery
question.** `dismissTopmostSurface` (1149-1227) and `hasDismissibleSurface` (1112-1117) handle the
bootstrap and support chooser but not a sketch inside a project, the Freeform cage edit or
Dimensions with no anchored surface open, and not the recovery prompt; Back then goes to the
platform (app to background). PRODUCT.md:196 ("cancels the question you are reading") and :301-303
("leaves the app only when there is nothing open to close"). Not destructive (native state is
process-scoped). E2. **PARTIAL**.

**A-11 — P3 — Internal tokens and imprecise mode words in user text.** New Freeform refusal shows
`"Refused: %1$s"` with a raw `freeformStatusToken` (`EditorWorkspaceView.java:2448`,
`strings_modeling.xml:81`), and every non-Sculpt project is summarised "in Construction"
(`projectSummary` 6190-6198) for CAD/Freeform/Surface. E2. **PARTIAL**.

**A-12 — P3 — Export doc overstates the sculpt rule.** PRODUCT.md:2080-2082 says a sculpted body
exports its sculpted surface; `forgeshape_gltf_export.cpp:205` uses the sculpt mesh only while the
session is in Sculpt, so after Back to Construction the body exports its Construction shape (what is
on screen; recorded by `ImportedMeshSculptTest` -14). E2+E3. **DOC-DIVERGENCE**.

**A-13 — P3 — A stale recovery candidate can survive a clean leave.** A clean `LEAVE_NEW_PROJECT`
does not retire the checkpoint (2638-2646); Open File writes one for an unchanged external file
(6112, no autosave baseline); an edit-then-Undo back to the saved fingerprint rewrites one. The next
cold launch then asks "Unsaved work found" for work already stored. E2. **PARTIAL**.

## Hypotheses (unproven)

- **H-A1** `loadProject` cancels an open sketch (jni 8478-8482) BEFORE `loadProjectDocument`'s
  post-decode refusals (`editInProgress`, `runtimeCanEvaluateProject`, Surface/Imported
  regeneration, forgeshape_project_state.cpp:250-320). A refused Open over a sketch would then lose
  the sketch, against "a failed load changes nothing". Not shown reachable: `decodeProject` already
  validates (all 28 refused corpus fixtures are refused at decode in `fc2/host_corpus_verdicts.txt`).
- **H-A2** After A-04, a face-supported Add/Cut staged on body X commits while the active body is the
  imported one; whether the commit lands on the wrong body or is refused was not traced.
- **H-A3** On API < 31 System Back in an in-project sketch finishes the Activity and
  `NativeViewport.stop()` runs (`ForgeShapeActivity.java:569-584`); whether a later relaunch in the
  same process keeps the sketch was not verified.

## Adversarial counterexample attempts

1. **Old D1 (Import in Sculpt) still reachable?** Refuted: Java guard (`EditorWorkspaceView.java:2823,
   5925-5938`) plus native guard (jni 8590-8597) plus `ImportGlbSculptGuardTest` d1_02..04.
2. **Import while a sketch is open?** Succeeded — A-04, E1 output above.
3. **Can a refused Open File from Home write its verdict where nobody sees it?** Refuted: `showStatus`
   routes to Home's own line when Home is visible (6337-6346); `HomeFlowTest._07` recorded in
   `40782861799`.
4. **Does anything behind Home get saved or checkpointed?** Refuted: `encodeProject` returns null
   with no project (jni 8393-8400), fingerprint 0, autosave counts `skippedNoProject`.
5. **Is recovered work re-checkpointed on backgrounding?** Not refuted — A-02 (the skip rule in
   `performCheckpoint` with the baseline set by Recover).
6. **Can Save-and-continue ever overwrite the thing being opened?** Yes for the slot — A-01; for Open
   File it cannot (the external file is never written).

## Positive claims with evidence

- Home and the shell phase are derived from native `projectOpen()` on every refresh, and a
  recreation lands where native truth says (`EditorWorkspaceView.java:1999-2044`; HomeFlowTest
  `_01`, `_12` in `CI DEVICE 40782861799`) — E2+E3.
- A failed open is all-or-nothing: decode runs outside the lock and before any mutation;
  `loadProjectDocument` stages everything before touching the scene (jni 8440-8496;
  `forgeshape_project_state.cpp:247-266`); 72 corpus verdicts reproduced (E1); EditorWorkspaceProjectActionsTest
  `e2er1a04/05` (E3).
- New Sculpt and New Freeform seed inside the session-initialization bracket, so the first Undo is
  the user's (2402-2458; HomeFlowTest `_04`, FreeformOwnerTest `devFf01` asserts undo depth 0) — E2+E3.
- The drawer's order and "close first, then the one handler" are pure Java pinned on the JVM
  (`ProjectDrawerPolicyTest`, 9 tests; E3 at this tree).
- Import refusals change nothing and the native import commit is one Construction step
  (host `IMP01A_*` in GLTF_IMPORT 189/0 — E1).
- No Uri reaches the domain; the codec knows no filesystem (`ProjectTransfer` boundary;
  ProjectTransferTest fsr1b13 — E3 older tree).

## Journeys proposed for END_TO_END_JOURNEYS

| Id | Kind | Steps | Expected | Today |
| --- | --- | --- | --- | --- |
| J-A1 | positive | cold Home → New Project → CAD → rectangle → Finish → Extrude → drawer Save Project → force-stop → relaunch → reopen the saved project | the saved body returns with an empty history | no direct path from Home (A-03) |
| J-A2 | negative / fail-closed | Save Project (A) → edit to B → drawer Open Saved Project → "Save and continue" | A is not destroyed (Save either refused or writes elsewhere) | A destroyed (A-01) |
| J-A3 | negative | edit → kill → relaunch → Recover → background → kill → relaunch | recovery offered again (or work saved) | work lost (A-02) |
| J-A4 | negative | open project → New Sketch → drawer → Import GLB | row absent; native refuses `in_sketch` | imports, active body switches (A-04) |
| J-A5 | positive | Import GLB → Transform → Start Sculpting → stroke → Back to Imported Mesh → Save Copy… → Open File… | bodies, IMPT+SCUL and placement round-trip; one Undo for the import | supported (APP-049/050, E3 older tree) |
| J-A6 | negative | Save Project → Settings → change palette → New Project… | no unsaved question | question appears (A-05) |

## Owner decisions needed

1. Home: add Open Saved Project (or otherwise expose the slot at launch) vs APP-H1 "exactly two" (A-03).
2. Recover semantics: is recovered work "stored" (no question) or unpersisted until Save (A-02)?
3. Unsaved question before Open Saved Project: drop or redefine "Save and continue" (A-01).
4. Does an open sketch / staged edit count as unsaved work when leaving (A-07)?
5. System Back inside an in-project sketch and on the recovery question (A-10).
6. Whether Import GLB belongs on Home as "new project from GLB" (APP-051, H2).
7. Whether bootstrap guidance in the status line is an approved exception to "Chrome reports" (APP-073).

## Unresolved dependencies and open questions

- Canonical Master v4.1.61 / OWNER_DECISIONS are unavailable: onboarding, recent files and desktop
  keyboard expectations cannot be judged (APP-077..079).
- Device evidence for the persistence / transfer / settings classes is from `fa0b6b4` (older shell);
  no fresh device run was made by this seat.
- The release guard result for this tree was not obtained (environment error).
- Physical device, stylus and OWNER visual matters (palettes, start pages) remain
  OWNER-PHYSICAL-UNVERIFIED.

Completeness: **PARTIAL** — the shell and cross-feature journeys were traced end to end from source
with one fresh runtime reproduction (A-04) and fresh host/corpus evidence, but the canonical plan
sources are missing (PARTIAL-PLAN-SOURCE-GAP), the Java-layer defects (A-01, A-02, A-05, A-06) are
source-proven only (no device or Robolectric reproduction was possible here), and the release guard
could not run.
