# FUNCTION-COUNCIL-R2 — Seat J: adversarial integration, data loss and user trust

Seat J looks for places where two features meet and the result loses work,
silently changes a value, or breaks a stated trust rule. Each claim carries an
evidence grade from E1 to E5, defined in `BEFORE.md`. Verdicts use the council
vocabulary. Results produced in this seat came from host programs in
`/tmp/claude-0/fc2-scratch/J/`. The exact commands are in
`/tmp/claude-0/fc2-scratch/J/COMMANDS.txt`. None of these programs is part of
the build.

## Scope

The seat covers the trust and integrity rules that cut across domains:

- data-loss prevention: Save, the checkpoint, recovery, and leaving a project;
- all-or-nothing commits and refusal by name;
- that each history domain stays separate (Construction, Sculpt, CAD,
  Freeform, Surface);
- that support references are never retargeted;
- stale controls after a state change;
- unsavable or unloadable states;
- wrong-unit edits;
- hidden or locked bodies reached through a side path;
- malformed input at the JNI boundary;
- deterministic replay;
- functions that only test seams can reach.

Each domain's own feature rules belong to Seats A–H. This seat cites their
findings by ID instead of repeating them.

## Sources read

| Item | Detail |
| --- | --- |
| Tree | `HEAD 7c05748` on the audit branch. `git diff --stat 70a6c14 HEAD -- app/ testdata/` is empty, so the product tree equals the C2 head `70a6c14`. |
| Project rules | `CLAUDE.md`: APP-H1; the autosave, recovery, `.forge`, Uri and history rules; Stage 018A; Stage027; MIRROR-01; CAD-A3; SKETCH-UX-R1 F; CAD-V6-S1; MODELING-FOUNDATIONS-R1 B/C. |
| `PROJECT_STATUS.md` | *Known Issues / Blockers* (4018–4215), *Technical Debt* (4216–4700), including ARCH-HEALTH-01 `nextObjectId` and DEEP-AUDIT F-07/F-09. |
| `PRODUCT.md` | Lines 476–583 (rename, hide, lock, duplicate) and 1397/1459 (dimension refusals). |
| `artifacts/function-council-r2/` | `BEFORE.md` and `SEAT_A.md` … `SEAT_H.md`, read so this seat does not duplicate them. |
| Fresh audit outputs (not regenerated here) | `scratchpad/fc2/host_selftests.txt` (`HOST_SELFTESTS_OK (4392 checks, 0 failed)`), `host_corpus_verdicts.txt` (72 fixtures), `release_guard.txt` (`RELEASE_SELFTEST_GUARD=PASS`), `run_status.txt`, and `evidence/device-run-40797031704/summary.json` (32 tests, none in this seat's classes). Added at repair: `evidence/device-run-40805804385/summary.json` (90/90, emulator only), which DID run this seat's persistence, recovery, process-death, object-command and JNI-hardening classes; see the androidTest note below. |

## Inspected code paths

- **Persistence boundary**
  - `forgeshape_project_state.cpp`: `runtimeCanEvaluateProject` (118-133),
    `captureProjectDocument` (135-), `loadProjectDocument`, and
    `projectSemanticFingerprint`, which mixes `nextObjectId` at 753.
  - `forgeshape_project_state.h:55-64`. This comment is stale; see E-05.
- **JNI project calls**
  - `encodeProject` (`forgeshape_jni.cpp:8387-8425`) holds no
    `editInProgress` guard.
  - Also read: `loadProject` (8427-) and `projectFingerprint` (8936).
- **Android persistence**
  - `ProjectSlot.write` (`ProjectSlot.java:77-123`): temp file, fsync, rename,
    with a delete-and-retry fallback. `ProjectSlot.read` (130-150).
  - `ProjectCheckpoint` write, `clear` and quarantine (`ProjectCheckpoint.java:89-185`).
  - `AutosaveController.performCheckpoint` (146-175).
- **Leaving a project** (`EditorWorkspaceView.java`)
  - `projectDirty` (2597-2602) and `leaveProjectFor` / `proceedToLeave` /
    `onUnsavedSaveRequested` (2622-2678).
  - `saveProjectToSlot` (5458-5471) and `openSavedProjectSlot` (5483-).
  - Read to re-verify A-01.
- **Hidden and locked guards**
  - Hidden is enforced in `ConstructionScene::snapshot` (`forgeshape_scene.cpp:328-345`).
  - Sculpt guards: `forgeshape_jni.cpp:2894` (Start) and 3015 (Resume).
  - Dimensions guard: `forgeshape_body_dimensions.cpp:438-446`.
  - Freeform: `FreeformEditSession::begin` (`forgeshape_freeform_session.cpp:273-279`)
    and `reconcile` (299-308).
  - CAD edit entry points have **no visibility or lock check**:
    `sketchBeginEdit` (`forgeshape_jni.cpp:7137-7193`) and
    `sketchBeginEditFeature` (7205-7250).
  - Surface entry points have no check either (5843-6043).
  - The CAD preview substitutes only into snapshot items
    (`applyCadOperationPreviewLocked`, 1727-1766).
  - The History control is shown without asking visibility:
    `FeatureHistoryPresentation.controlShown` (208-211) and
    `EditorWorkspaceView.refreshFeatureHistory` (3214-3218).
- **Units**
  - `LengthUnit` (whole file).
  - `onDisplayUnitRequested` (`EditorWorkspaceView.java:6403-6416`) converts
    only four editors.
  - `FreeformEditorView.java:123-145,282-296`.
  - `SurfaceEditorView.java:106-112,152,181,486-528`.
- **JNI input checks**
  - `freeformSetElement` (5530-5536), `freeformSelect` (5557-5585),
    `freeformElementScreenPoint` (5730-5733), `sketchSetModifyMode` (6782-6795)
    and `sketchAddDimension` (6898-6910). All range-check their inputs.
  - The debug seams `debugSetCameraPose` (7913-) and
    `debugResetConstructionHistory` (7966-7972) are under `#ifndef NDEBUG`.
- **Commands with refusals**
  - `deleteSceneBody`, `duplicateSceneBody`, `renameSceneBody`,
    `setSceneBodyVisible`, `setSceneBodyLocked`, `mirrorSceneBody`, and the
    shared mode guard `objectCommandsBlockedByMode` (`forgeshape_jni.cpp:3408-3410`).
- **Revolve axis pick**
  - JNI `sketchBeginRevolveAxisPick` (6363) and its touch path (9745-9789).
  - Java `EditorWorkspaceView.onRevolveAxisPickRequested` (4645).
- **JNI usage census**
  - 275 `static native` declarations in `NativeViewport.java`, each matched
    against callers in `app/src/main/java`, `app/src/androidTest` and
    `app/src/test` (`native_usage.txt`).

## Test coverage of the domain

These are what the tests actually assert:

- **Host suites.** A fresh audit run reports 4392 checks and 0 failures
  (`scratchpad/fc2/host_selftests.txt`, E1).
  - **PROJECT.** Codec refusals by name; corpus digests; all-or-nothing load
    (`RefusedEditInProgress` at `forgeshape_project_selftest.cpp:2491`); and a
    fresh history after load.
  - **SCENE.** `obj018a_*`: lock and hide keep the body drawn or pickable as
    specified; locking twice records nothing; a hidden, locked body still
    deletes (`forgeshape_scene_selftest.cpp:739-1036`).
  - **CAD_A3.** Producer dependents (`forgeshape_cad_a3_selftest.cpp:252-256,895-904`).
    The host suites never assert the `RefusedHasDependents` *status* (grep
    finds none); this seat's J4 does.
  - **SKETCH_UX.** `DependentFaceLost` (`forgeshape_sketch_ux_selftest.cpp:721`).
  - **FREEFORM.** Hidden and locked refusals.
  - **REVOLVE.** Axis-pick domain state (`forgeshape_cad_revolve_selftest.cpp:768-964`).
- **Corpus verdicts.** 72 fixtures, each refusal fixture refused by its
  expected name (fresh, E1, `host_corpus_verdicts.txt`). CI FAST
  `40796992679` has `forge-corpus-parity.txt` (E3).
- **JVM.** The fresh Gradle run exited 0 (`run_status.txt`). No JVM class
  covers `ProjectSlot`, `ProjectCheckpoint` or unit seeding in the Freeform
  and Surface editors.
- **androidTest.** None of these classes ran in this audit's fresh device run
  `40797031704`, which ran 32 UI tests in other classes. `HomeFlowTest` and
  `EditorWorkspaceProjectActionsTest` ran in C2 DEVICE `40782861799` at
  `30b561b`, 67/67 (`artifacts/cad-r2-c2-picking-seam/TESTS.md:75`), whose
  `app/src/main` and `app/src/androidTest` equal this tree's (`git diff --stat
  30b561b 70a6c14 -- app/src/main app/src/androidTest` is empty) (E3). For the
  other classes below the most recent aggregate is `FULL_SHARDED_SUITE_PASS`
  `36607747079` on `fa0b6b4`, an **older tree** (E3). (Corrected in Round 2,
  objection C-12.)
  **Superseded at repair by CI DEVICE `40805804385` (90/90, 25/25 startup
  tokens in order, emulator only; `evidence/device-run-40805804385/`, recorded
  in `TESTS.md`).** On the audited tree it ran `ProjectAutosaveRecoveryTest`
  (14), `ProjectProcessDeathTest` (stage1..stage8), `ObjectCommandsTest` (1),
  `JniBoundaryHardeningTest` (5), `ObjectsDeleteTest` (8), `ProjectTransferTest`
  (13), `ImportedMeshDurableTest` (12), `MirrorSmokeTest`,
  `BodyDimensionsSmokeTest`, `EditorWorkspaceLifecycleTest`,
  `DiagnosticsAndRendererLossTest` and `SettingsPreferencesTest`, all passing
  (counts from `instrumentation-junit.xml`). `HomeFlowTest` and
  `EditorWorkspaceProjectActionsTest` were not in that run and keep their E3
  `40782861799`. The `fa0b6b4` dating above no longer applies to the classes
  this run covered.
  - **`ProjectAutosaveRecoveryTest`** (FSR1B-01..09): checkpoint isolation from
    the slot; one write per burst; a failed write leaves the previous
    checkpoint; quarantine; Recover restores values and a fresh history.
  - **`ProjectProcessDeathTest`** (E2ER1A/B): runs across a real process
    death. It needs `scripts/run-project-persistence-e2e.ps1`.
  - **`EditorWorkspaceProjectActionsTest`**.
  - **`JniBoundaryHardeningTest`** (PAH-R1-03..06, 12..13). It covers only
    lock-scoped concurrent reads and short `touchEvent` arrays. It sends no
    malformed *values* (NaN, out-of-range enums, ids).
  - **`ObjectCommandsTest`, `ObjectsDeleteTest`, `HomeFlowTest`,
    `SpatialSketchTest`**: refusal codes, including `RefusedHasDependents`.
- **Fixtures.** The 28 refusal fixtures in `testdata/forge/v1`.
- **Gaps.**
  - ~~No test drives the real revolve-axis pick (J-05).~~ Withdrawn in Round 2
    (C-1): `CadRevolveOwnerTest` drives it with real `MotionEvent`s.
  - No test hides or locks a CAD or Surface body and then edits it (J-01).
  - No test checks Freeform or Surface fields against the display unit (J-02).

## Confirmed findings

### J-01 — P3 — A hidden CAD or Surface body can be edited through History and Edit; no solid candidate is drawn for it (Round 2: BLOCKED-OWNER OD-J1 for the refusal question, PARTIAL for the missing solid preview)

- **Where.**
  - `forgeshape_jni.cpp:7137-7193` (`sketchBeginEdit`) and 7205-7250
    (`sketchBeginEditFeature`) check project, Sculpt and edit-in-progress, but
    not `visible()` or `locked()`.
  - The Surface JNI at 5843-6043 has no such check either.
  - `applyCadOperationPreviewLocked` (1758-1765) replaces the target's
    `SceneDrawItem` only when the target is already in the snapshot. A hidden
    body is left out of the snapshot by `forgeshape_scene.cpp:332-344`, so no
    candidate is drawn.
  - `FeatureHistoryPresentation.controlShown` (208-211) draws the History
    control without regard to visibility.
- **Minimal repro.** Run `j1_hidden_edit` on `cad_rectangle_v1.forge`:
  - `[HIDDEN] inSnapshot=0 beginEdit=Ok finish=Ok setExtrude=Ok candidate=Ok targetIsBody=1`
  - `[HIDDEN] commitEdit=Ok depth 1.5000 -> 3.0000 undoSteps 1 -> 2 visible=0`
  - `[LOCKED] ... commitEdit=Ok depth 1.5000 -> 3.0000 ... locked=1`
  - `[FREEFORM] beginHidden=BodyHidden beginLocked=BodyLocked`
  - On the device: hide the active CAD body, tap History, open its Extrude
    row, and drag or type a depth. No solid candidate is drawn (only a target
    already in the snapshot is substituted); the sketch overlay — profile,
    hatch and extrude arrow — is still drawn (`forgeshape_jni.cpp:1988-1992`).
    The commit lands on a body that is not drawn.
- **Impact.** The user edits a solid whose result is not drawn. No repo rule
  requires CAD or Surface edits to refuse a hidden body (Stage027 GUARD-2 is
  Sculpt-only; the Dimensions refusal states its own reason; `PRODUCT.md:530-535`
  says a hidden body can still be selected from its row), so this is a
  consistency question for the owner, not a rule breach. Sibling paths that do
  refuse:
  - Freeform refuses (`BodyHidden`).
  - Sculpt Start and Resume refuse (Stage027 GUARD-2).
  - Dimensions refuse (`RefusedHidden`), "because the leaders are read off
    geometry that is not drawn".

  The locked half is NOT a defect: `PRODUCT.md:537-542` says a locked body
  refuses only being moved and "Everything else still works", and CLAUDE.md
  Stage 018A names exactly two lock guards, so CAD/Surface commits on a locked
  body are the documented behaviour (PASS-SOURCE, E2). The outlier is
  Freeform's `BodyLocked` (`forgeshape_freeform_session.cpp:277-278`), which no
  owner document states (DOC-DIVERGENCE, OD-J1). Nothing is lost, because the
  commit is one Undo.
- **Evidence and verdict.** E1 for the domain composition (`j1_hidden_edit.out`).
  E2 for the JNI, the preview and the Java gating. Round 2 (objections C-3,
  C-4): **BLOCKED-OWNER (OD-J1), horizon H2-pending-decision** for whether a
  hidden body's CAD/Surface edit should be refused; **PARTIAL, P3** for the
  missing solid candidate (CAD-VERTICAL-SLICE-R1 "the renderer draws that
  evaluation" does not happen for a target outside the snapshot). Formerly
  DEFECT-CONFIRMED.
- **Isolated fix.**
  - Refuse `sketchBeginEdit`, `sketchBeginEditFeature`,
    `sketchBeginEditSketch` and the Surface edit entry points with a named
    `BodyHidden` status.
  - Pass `activeBodyVisible` into `FeatureHistoryPresentation.controlShown`.

  Either fix is one file. The owner chooses whether locked bodies are also
  refused (OD-J1).

### J-02 — P3 — Freeform and Surface length fields are seeded with unitless literals and read in the session display unit

- **Where.**
  - `FreeformEditorView.java:123-125` sets `distanceRow` to `"0.25"`.
  - `SurfaceEditorView.java` sets `distanceRow` to `"0.5"` (108), `offsetRow`
    to `"0"` (152) and `thicknessRow` to `"0.1"` (181).
  - Their captions are a bare "Distance", "Offset" and "Thickness"
    (`strings_modeling.xml:55,106,111,114`), with no unit.
  - `readLength` and `quietLength` convert the text with
    `host.uiState().displayUnit().toMeters(...)` (`FreeformEditorView.java:282-285`,
    `SurfaceEditorView.java:499-516`).
  - `onDisplayUnitRequested` (`EditorWorkspaceView.java:6403-6416`) converts
    only the shape, placement, sketch and CAD editors. Its own comment says
    this is "so a draft does not silently change meaning while it is away".
- **Minimal repro.** The E1 arithmetic is `j3_unit_probe.out`, run through the
  product's own `LengthUnit`:

  | Seeded text | mm | cm | m |
  | --- | --- | --- | --- |
  | `"0.25"` (Freeform distance) | 2.5E-4 m | 0.0025 m | 0.25 m |
  | `"0.1"` (Surface thickness) | 1.0E-4 m | 0.001 m | 0.1 m |

  The UI steps (E2) are:
  1. In any Construction placement editor, tap the `mm` chip.
  2. Open the Freeform context surface.
  3. Tap Extrude with the seeded "0.25".

  The face moves 0.25 mm, not 0.25 m. Any text typed while the unit was `m`
  also changes meaning when the unit changes.
- **Impact.** A wrong-unit edit with no visible unit, off by a factor of 1000.
  It can be undone, but the user is given no reason to suspect it.
- **Evidence and verdict.** E1 for the arithmetic, E2 for the wiring.
  **DEFECT-CONFIRMED.**
- **Isolated fix.**
  1. Give each length row a unit caption (`label() + " (" + unit.label() + ")"`).
  2. Seed the rows through `unit.format(meters)`.
  3. Add `convertDisplayUnit` to `FreeformEditorView` and `SurfaceEditorView`,
     and call it from `onDisplayUnitRequested`.
  4. (Narrowed in Round 2, C-16.) The `SketchModifyView` offset field is
     seeded correctly from native metres through `displayUnit().format(...)`
     on entering Offset mode (`SketchModifyView.java:280-282`); only a unit
     change made while Offset mode is already open is not converted (E2).

### J-03 — P3 — Creating a body and undoing it leaves the project "unsaved" and checkpointed, though nothing changed

- **Where.**
  - `projectSemanticFingerprint` mixes `scene.nextObjectId()`
    (`forgeshape_project_state.cpp:753`).
  - The allocator is never rolled back, by the CLAUDE.md history rule.
  - `projectDirty()` compares fingerprints (`EditorWorkspaceView.java:2597-2602`).
  - `AutosaveController.performCheckpoint` writes whenever the fingerprint
    differs (165-173).
- **Minimal repro.** `j4_replay` on `mixed_cad_face_v2.forge`. It runs
  Duplicate, then Freeform create, then Undo back across both. Every undone
  state is `semantic(ignoring allocator)=MATCH`, but the fingerprint is
  `DIFF`, with `nextObjectId` 7 against 5 or 6.

  The user flow: Open, Add Primitive, Undo, then New Project. The
  unsaved-changes question appears, and a checkpoint is written for a project
  whose content is identical to the saved file.
- **Impact.** The user is asked to save when there is nothing to save. A
  needless recovery candidate also exists after a crash. This erodes trust in
  the dirty indicator. No data is lost.
- **Evidence and verdict.** E1 (`j4_replay.out`) and E2. Round 2 (C-15):
  **BLOCKED-OWNER (OD-J3), horizon H2-pending-decision**, formerly PARTIAL.
  The behaviour follows the stated rules: the `.forge` rule stores the id
  allocator's high-water mark, so a Save after create + Undo produces
  different bytes, and APP-H1 asks by FINGERPRINT. No repo record states the
  opposite expectation. Does not apply to CAD's `nextSketchId` /
  `nextFeatureId`, which Undo restores (CAD-V6-S1-C1).
- **Isolated fix.** Either exclude `nextObjectId` from the *dirty* comparison
  while keeping it in the codec, or document it in `PRODUCT.md`. This needs
  owner decision OD-J3.

### J-04 — P3 — The Save fallback keeps the "only copy" of a project in a pending file that nothing ever reads

- **Where.**
  - `ProjectSlot.write` (`ProjectSlot.java:98-123`): when the first
    `renameTo` fails, it deletes the slot, retries, and keeps
    `project.forge.pending` as the "ONLY copy of the project left".
  - `ProjectSlot.read` (130-150) reads only `project.forge`.
  - `grep -rn "project.forge.pending\|PENDING_FILE_NAME" app/src/main/java`
    finds only the constant and its write path.
- **Minimal repro.** Source only. On a filesystem where `renameTo` refuses to
  replace and the retry also fails, Save reports failure and the slot is gone.
  Open Saved Project then says there is no project. The fsync'd bytes sit
  orphaned until the next Save overwrites them.
- **Impact.** On this narrow path, which ext4 and f2fs should not take, the
  previous slot copy is lost and the newer bytes sit orphaned in
  `.pending`. Current work stays in memory and in the checkpoint, because
  `saveProjectToSlot` returns before `ProjectCheckpoint.clear` on a failed
  write (`EditorWorkspaceView.java:5459-5467`). The code comment's "ONLY copy"
  is never read back. (Reworded in Round 2, C-17.)
- **Evidence and verdict.** E2. **DEFECT-CONFIRMED** for the recovery gap.
  How likely the path is is E5.
- **Isolated fix.** In `ProjectSlot.read`, fall back to a validated
  `project.forge.pending` when the slot is missing. Alternatively, never
  delete the slot before the pending file is in place: write to
  `slot.forge.new`, then use `Files.move` with `ATOMIC_MOVE` and
  `REPLACE_EXISTING`.

### J-05 — P3 — A few device tests reach state through JNI seams the product never calls (narrowed in Round 2)

- **Where.** The JNI census in `native_usage.txt` covers 275 natives.
  - **66 are never called from product Java.**
  - **Seams used as setup in device tests** (Round 2, C-1/C-2: restricted to
    these):
    - `sketchSetRevolveAxis` as setup in `CadParametricHistoryOwnerTest.java:241`
      and `CadRetainedSketchOwnerTest.java:1262`;
    - `sketchToggleRegion` in `CadPlanarFaceRuntimeTest.java:529`
      (`toggleFace`). `CadFillPickR2Test.nativeToggle` (`:933-940`) also calls
      it, but only as setup; that class drives its acceptance taps with
      `realGesture` (9 call sites).
  - **Withdrawn (Round 2, C-1).** The earlier claim that the real revolve-axis
    pick is untested on the device was wrong. `CadRevolveOwnerTest` opens
    Revolve with `pressInSketchEditor(R.id.sketch_revolve_begin)` (`:555-566`),
    asserts `REVOLVE_AXIS_PICKING == 1` (`:129-131`), chooses the axis with
    `tapAxis` → `realGesture`, which dispatches real `MotionEvent` DOWN/UP
    through the decor view (`:619-636`, `:725-747`), and `devRev05` presses
    `R.id.sketch_revolve_change_axis` and taps again (`:309-321`). That class
    passed in C2 DEVICE `40782861799` at `30b561b`, 67/67, on a product and
    test tree equal to this one (E3). `CadSketchDraftingOwnerTest` chooses
    dimension targets with real taps (`:222-237`); its `sketchSetDimensionTarget`
    block (`:344-354`) is bulk setup.
  - **DOC-DIVERGENCE (test javadoc).** `CadPlanarFaceRuntimeTest.toggleFace`
    (`:521-529`) says it "adds a face through the precision surface's row
    path". No product Java calls `sketchToggleRegion` (only its declaration,
    `NativeViewport.java:2835`), and the real row makes the tapped region the
    ONE region (`SketchEditorView.java:538-545`). The javadoc describes a
    product path that does not exist.
  - **Eight natives are called by neither product nor tests:**
    - `supportChooserSelect`, `supportChooserConfirm`;
    - `freeformSelect`, `freeformClearSelection`;
    - `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`;
    - `sketchToggleSelectEntity`.
- **Impact.** Where a seam is setup, the device evidence after it proves the
  domain, not the gesture that would have produced that state. The eight
  orphan exports are untested ABI surface.
- **Evidence and verdict.** E2. **PARTIAL** for seam-as-setup coverage; the
  orphans are recorded only (IMPLEMENTED-UNTESTED). The real axis pick is
  PASS-SOURCE-AND-RECORDED-RUNTIME (E3 `40782861799`).
- **Isolated fix.**
  - Correct the `toggleFace` javadoc.
  - Either delete the eight orphan natives or cover them.
  - Note in each seam-using test which UI act it stands for.

### Cross-seat re-verification (by source, on this tree)

- **A-01** (P1, Save and continue before Open Saved Project overwrites the
  slot it then opens) is **confirmed**. `onUnsavedSaveRequested` (2665-2677)
  calls `saveProjectToSlot`, which calls `ProjectSlot.write` and
  `ProjectCheckpoint.clear` (5458-5467). It then calls
  `proceedToLeave(LEAVE_OPEN_SLOT)`, which calls `openSavedProjectSlot` (2656)
  and reads back the slot just written: an **unannounced overwrite** of the
  project about to be opened (the prompt says "Save the project to this device
  first", `strings.xml:63`, but not that it replaces that project). E2.
- **E-05 and E-06** match this seat's codec observations. Seat J's fuzz did
  not exercise operation flips, so it neither adds to nor contradicts them.
- **A-04 / C-02 / D-02 / E-03** (Import during a sketch) are not re-run here.
  The `objectCommandsBlockedByMode` guard (`forgeshape_jni.cpp:3408`) covers
  object commands, not `importGlbDurable` (8560; it guards Sculpt only,
  8594-8600, while `sceneAddBody` refuses `in_sketch`, 3246-3265). The rule is
  the delivered **H0** CAD-R0-A1A2 guard; E1 is Seat A's
  `SEATA_IMPORT_DURING_SKETCH=ACCEPTED` (`SEAT_A.md:134`). (Round 2, C-5.)

## Hypotheses (unproven)

- **HJ-1. The checkpoint can capture an uncommitted mid-drag transform.**
  - `encodeProject` (`forgeshape_jni.cpp:8387-8410`) does not check
    `constructionHistory().editInProgress()`, while `loadProject` refuses
    `RefusedEditInProgress`.
  - A gizmo or Freeform drag writes the transform live. If the finger holds
    still for more than `DEBOUNCE_MILLIS` (900 ms) and the process dies, the
    recovered placement is one the user cancelled or never committed.
  - The guard's absence is E2. The user consequence is E5.
  - Scope (Round 2, C-18): only gizmo and Freeform drags can be caught. CAD
    extrude and revolve drags are session-only and write no `CadBodyState`
    until commit (CAD-UX-S1), so they never reach a checkpoint.
- **HJ-2. The rename may not be durable on power loss.**
  - Neither `ProjectSlot.write` nor `ProjectCheckpoint.write` fsyncs the
    parent directory after `renameTo`.
  - On some filesystems a power loss can revert the rename. E5.
- **HJ-3. A locked face-supported dependent moves with its producer.**
  - Lock "refuses being moved" (PRODUCT.md:537-542). A dependent's placement
    is derived through `resolveWorldModel`, and nothing consults its lock.
  - Moving or editing the producer therefore moves a locked body. It is
    consistent with the derived-placement rule; whether lock should mean
    anything here is OD-J2. E2.

## Adversarial counterexample attempts

| # | Attempt (command) | Expected failure | Outcome |
| --- | --- | --- | --- |
| 1 | **J2: memory-safety fuzz of the decoder and load.** ASan and UBSan build of all 98 platform-neutral TUs. Every byte offset of every section payload of the 72 fixtures is overwritten with a 4-byte count-like value (0xFFFFFFFF, 0x7FFFFFFF, 1<<20, 1<<16, 0), the CRC is repaired, the input is decoded, and an Ok decode is loaded over a live project. | A crash, `bad_alloc`, a sanitizer report, an Ok decode that the load refuses, or a refused load that mutates the project. | **Failed to break it.** `J2_SUMMARY runs=167875 decodeOk=48587 loadOk=48587 loadRefused=0 failClosedViolations=0 badAlloc=0`, with 0 sanitizer lines and a worst decode of 30.26 ms under ASan. This complements Seat E's random-mutation fuzz, which ran without sanitizers. |
| 2 | **J4: cross-representation replay.** On `mixed_cad_face_v2`: four refusals (delete producer, duplicate dependent, empty rename, mirror CAD), then rename with an emoji, lock dependent, hide producer, duplicate producer, Freeform create, Freeform extrude, delete dependent and delete producer. After each step: encode, reopen in a fresh scene, compare fingerprints. Then Undo-all and Redo-all, comparing captured documents. | An unsavable state, a reopen mismatch, a refusal that changes something, or Undo/Redo landing on a state that never existed. | **Failed to break integrity.** Refusals: `RefusedHasDependents`, `RefusedFaceSupportedCad`, `RefusedInvalidName`, `RefusedNotMirrorable`, each with changed=0. Every step encodes and reopens with an equal fingerprint. Every Undo and Redo is a semantic MATCH (`J4_SUMMARY failures=0`). Side result: J-03. |
| 3 | **J5: non-finite and absurd values.** NaN, ±inf and ±1e308 sent through Freeform push/pull, extrude, crease and level; Surface apply-value for features and sketches 1..4, and Thicken; CAD `setExtrude`, `setExtrudeSide` and `setRevolveAngle`. | A clamp, a partial write, or a silent acceptance. | **Failed to break it.** `J5_SUMMARY probes=93 violations=0`. Every probe was refused by name (`NonFinite`, `InvalidDistance`, `ThickenInvalidThickness`, `InvalidExtrudeDepth`, `InvalidSubdivisionLevel`, …) with the fingerprint and undo depth unchanged. |
| 4 | **J1: hidden or locked side path.** | The edit is refused, as Freeform does. | **Succeeded:** J-01. |
| 5 | **J3: wrong-unit edit.** | The seeded value is unit-correct. | **Succeeded:** J-02. |
| 6 | **Lock bypass through Freeform.** Begin a Freeform edit on a locked body, and on a hidden one. | The tools act on a locked body. | **Failed.** `BodyLocked` / `BodyHidden` (j1 output). `reconcile` also ends the session when the body becomes hidden, locked or inactive. |
| 7 | **JNI malformed enums and arrays**, by source. | An unchecked `static_cast` of a Java int to an enum, or an unbounded array. | **Failed.** Every enum cast found is range-checked first (5531, 5732, 6789, 6906). `freeformSelect` bounds the length (`kMaxFreeformEdges`), checks for an exception and refuses `id <= 0`. |
| 8 | **Re-verify A-01** by source. | — | **Confirmed** (above). |

## Positive claims with their evidence

1. **Decoding and loading is memory-safe and fail-closed** under 167,875
   structural mutations with sanitizers (E1, J2). On 34,615 random mutations
   it also fails closed (Seat E, E1).
2. **The four object-command refusals probed (`RefusedHasDependents`,
   `RefusedFaceSupportedCad`, `RefusedInvalidName`, `RefusedNotMirrorable`,
   J4) and the 93 numeric refusals (J5) change nothing**: fingerprint and undo
   depth equal (E1). Delete-last-body, refusals in Sculpt or in a sketch, and
   lock/hide in a sketch were not probed. (Narrowed in Round 2, C-10.)
3. **Undo and Redo walk exactly the recorded semantic states** across a
   rename, a lock, a hide, a CAD duplicate, a Freeform create and extrude,
   and two deletes, including a producer and its dependent. Each state saves
   and reopens to an equal fingerprint (E1, J4).
4. **The live product cannot reach an unsavable state through these commands.**
   Every state in J4 encoded and reopened (E1). This holds for the commands
   exercised, not exhaustively.
5. **Saves and checkpoints are written temp-then-fsync-then-rename**
   (`ProjectSlot.java:83-98`, `ProjectCheckpoint.java:91-104`, E2). A failed
   checkpoint keeps the previous one (FSR1B-03, E3 `36607747079` on the
   older tree `fa0b6b4`; superseded at repair: `ProjectAutosaveRecoveryTest`
   passed 14/14 on this tree in CI DEVICE `40805804385`,
   `evidence/device-run-40805804385/instrumentation-junit.xml`, emulator only.
   The FSR1B-03 method name is not separately cross-checked in this repair).
6. **The JNI enum and array inputs checked here are validated before
   use** (E2). The debug-only seams are **inert in release**, not compiled
   out: they remain exported with no-op bodies (`debugResetConstructionHistory`
   `#ifndef NDEBUG` inside the body, `forgeshape_jni.cpp:7967-7972`;
   `debugCameraPose` the same, `:7893-7911`; `debugInjectDeviceLoss` returns
   `JNI_FALSE`, `:9083`) (PASS-SOURCE, E2; the release guard does not probe
   them). Release builds carry no self-tests (`RELEASE_SELFTEST_GUARD=PASS`,
   E1) — from a rerun at 00:57; `run_status.txt` records the first attempt as
   `guard_exit=2` at 00:54 and does not record the rerun. (Round 2, C-9.)
7. **Freeform refuses hidden or locked bodies and prunes a stale selection**
   (`forgeshape_freeform_session.cpp:273-330`, E1 in j1 output).
8. **Host self-tests pass 4392/0 and the corpus gives 72 named verdicts**
   (fresh audit run, E1).

## Journeys proposed for END_TO_END_JOURNEYS

- **JR-J1 (negative, fail-closed).**
  - *Start:* a CAD project with one body, hidden through the Objects row.
  - *Steps:* tap History, then the Extrude row, then type a depth.
  - *Expected (after the fix):* the History control is absent, or the edit is
    refused by name, and the body state is unchanged.
  - *Today:* the edit begins and commits with no preview (J-01).
  - *Undo/reopen:* after any commit, one Undo restores the depth; reopening
    the file shows the restored depth.
- **JR-J2 (positive, integrity).**
  - *Start:* a mixed project (CAD producer, a face-supported dependent, a
    Freeform body).
  - *Steps:* rename, lock, hide, duplicate, delete the dependent, then delete
    the producer. Save, kill the process, and reopen.
  - *Expected:* every state equals its pre-kill fingerprint. Undo-all in the
    session walks back exactly.
  - *Undo/reopen:* covered by the steps (host-proven by J4; device journey
    proposed).
- **JR-J3 (negative).**
  - *Start:* an open, saved project.
  - *Steps:* Open File… a `.forge` with a CRC-repaired count bomb (J2
    generator).
  - *Expected:* a named refusal. The live project, the slot and the
    checkpoint are untouched.
- **JR-J4 (unit).**
  - *Start:* display unit set to mm, a Freeform box open.
  - *Steps:* Extrude with the seeded distance.
  - *Expected:* the field shows its unit, and the extrusion equals the
    displayed quantity.
  - *Undo/reopen:* one Undo.
- **JR-J5 (trust).**
  - *Start:* a project just opened from the slot.
  - *Steps:* Add Primitive, Undo, then New Project….
  - *Expected (pending OD-J3):* no unsaved-changes question.

## Owner decisions needed

- **OD-J1.** Should CAD and Surface edits (Edit Sketch, History rows, Keep
  Sketch, Add/Cut, Surface value and Thicken) be refused over a hidden body,
  as Freeform, Sculpt and Dimensions are? And is Freeform's undocumented
  `BodyLocked` refusal intended, given `PRODUCT.md:537-542` ("Everything else
  still works")? (Seat D's D-09 covers lock.)
- **OD-J2.** Does Lock on a face-supported dependent constrain anything when
  its producer moves (HJ-3)?
- **OD-J3.** Is an allocator-only difference "unsaved changes" (J-03)?
- **OD-J4.** Should `encodeProject` defer a checkpoint while a Construction
  edit is in progress (HJ-1)?

## Unresolved dependencies and open questions

- ~~No device run in this audit exercised the persistence, recovery or JNI
  hardening classes. … for `ProjectAutosaveRecoveryTest`,
  `JniBoundaryHardeningTest` and `ObjectCommandsTest` the newest aggregate is
  from the older tree `fa0b6b4` (`36607747079`).~~ **Superseded by CI DEVICE
  `40805804385` (90/90, emulator only, `evidence/device-run-40805804385/`,
  recorded in `TESTS.md`)**, which ran all three on the audited tree
  (`ProjectAutosaveRecoveryTest` 14, `JniBoundaryHardeningTest` 5,
  `ObjectCommandsTest` 1, all passing) together with `ProjectProcessDeathTest`
  stage1..stage8. `HomeFlowTest` and `EditorWorkspaceProjectActionsTest` still
  rest on C2 `40782861799` on an identical product/test tree (E3).
- Whether `renameTo` ever fails to replace on a shipped Android filesystem
  (J-04 likelihood, HJ-2) needs a device or filesystem test.
- The ProjectProcessDeathTest flow needs the PowerShell persistence script.
  `pwsh` is not available in this environment. (Updated at repair: its eight
  stages passed in CI DEVICE `40805804385`. Whether that CI run killed the
  process between stages, as the script does, is not established by the
  evidence files read here (`summary.json`, `instrumentation-junit.xml`), so
  the real-process-death claim stays unverified by this seat.)
- J-01 for Surface rests on source evidence only (E2). The host diagnostic
  ran CAD and Freeform.

**Completeness: PARTIAL.**

- The host-reproducible scope is complete: 5 diagnostics, 8 attempts and
  every cross-cutting rule rowed in `work/req_J.tsv`.
- Three parts have no fresh run and rest on source or historic evidence (E2
  or E3):
  - device-level journeys (partly closed at repair: CI DEVICE `40805804385`
    ran the persistence, recovery, object-command and JNI-hardening classes,
    emulator only);
  - the process-death and filesystem-failure paths (the process-death stages
    passed in `40805804385`, but a between-stage process kill is not evidenced
    here; the filesystem-failure path still has no run);
  - Surface hidden-body edits.
- The canonical master plan and owner-decision sources are not available,
  per `BEFORE.md`.

## Round 2 — objections from seat C and responses

Each objection was re-checked by read-only source and test inspection at
`70a6c14`, plus `git diff --stat 30b561b 70a6c14 -- app/src/main app/src/androidTest`
(empty). No new binaries were run. Corrections are applied above in this file
and in `work/req_J.tsv`. Unless a row says otherwise, nothing remains UNRESOLVED.

| # | Objection (seat C) | Re-verification | Response | Status |
| --- | --- | --- | --- | --- |
| C-1 | ADV-063 / J-05: the real revolve-axis pick IS device-tested; withdraw | `CadRevolveOwnerTest.java:555-566` presses `R.id.sketch_revolve_begin`; `:129-131` asserts `REVOLVE_AXIS_PICKING`; `tapAxis` (`:619-636`) → `realGesture` (`:725-747`) dispatches real `MotionEvent` DOWN/UP through the decor view and asserts the tapped entity is the axis; `devRev05` presses `sketch_revolve_change_axis` (`:314`). Class is in `40782861799` 67/67 at `30b561b` (`cad-r2-c2-picking-seam/TESTS.md:75`), and the tree is identical | **CONCEDED.** ADV-063 → PASS-SOURCE-AND-RECORDED-RUNTIME (E3 `40782861799`). The "No androidTest file references it" sentence and the Gaps bullet are withdrawn. My grep searched only the JNI name. | Resolved |
| C-2 | J-05 / ADV-064 overstated; restrict the seam claim; add the stale javadoc | `CadFillPickR2Test` has 9 `realGesture` call sites. `nativeToggle` (`:933-940`) is setup. `CadSketchDraftingOwnerTest:222-237` chooses dimension targets with real taps. `sketchToggleRegion` has no product caller (only `NativeViewport.java:2835`). `CadPlanarFaceRuntimeTest.toggleFace` javadoc (`:521-529`) names a "precision surface's row path"; the real row selects ONE region (`SketchEditorView.java:538-545`) | **CONCEDED.** J-05 stays P3. The seam claim is restricted to `sketchSetRevolveAxis` (`CadParametricHistoryOwnerTest:241`, `CadRetainedSketchOwnerTest:1262`) and `CadPlanarFaceRuntimeTest:529`. The stale javadoc is added as DOC-DIVERGENCE. | Resolved |
| C-3 | J-01 / ADV-051 hidden-body edit is not a rule breach → BLOCKED-OWNER; the "no preview" wording is too strong | JNI gaps confirmed (`forgeshape_jni.cpp:7137-7193`, `7205-7250`). `PRODUCT.md:530-535` lets a hidden body be selected from its row. GUARD-2 is Sculpt-only. `applyCadOperationPreviewLocked` (`:1758-1765`) substitutes only a target already in the snapshot. The sketch overlay is set regardless (`:1988-1992`) | **CONCEDED.** ADV-051 → BLOCKED-OWNER (OD-J1), H2-pending-decision. The missing SOLID candidate is recorded as PARTIAL P3. That observation stands on CAD-VERTICAL-SLICE-R1 "the renderer draws that evaluation", which does not happen for a target outside the snapshot. The wording is now "no solid candidate is drawn; the sketch overlay and extrude arrow are". | Resolved |
| C-4 | Locked half of J-01 / ADV-053 points the wrong way | `PRODUCT.md:537-542` ("Everything else still works"). CLAUDE.md Stage 018A names two lock guards. Freeform `BodyLocked` (`forgeshape_freeform_session.cpp:278`) appears in no owner doc; the only `BodyLocked` hit is ARCHITECTURE.md:4990, which is about the transform rejection | **CONCEDED.** CAD/Surface lock behaviour → PASS-SOURCE (E2). ADV-053 → DOC-DIVERGENCE (Freeform outlier), and OD-J1 is reworded to ask about it. | Resolved |
| C-5 | ADV-046: horizon H1→H0, grade E2→E1 (Seat A) | `importGlbDurable` guards only Sculpt (`:8594-8600`). `sceneAddBody` refuses `in_sketch` (`:3246-3265`). The rule is CAD-R0-A1A2 (delivered). `SEAT_A.md:134` `SEATA_IMPORT_DURING_SKETCH=ACCEPTED` | **CONCEDED.** H0, E1 (Seat A's diagnostic). P2 DEFECT-CONFIRMED is kept. | Resolved |
| C-6 | ADV-027: "silent loss" → "unannounced overwrite" | `EditorWorkspaceView.java:2665-2678`, `5458-5471`, `2656`. `strings.xml:62-63` | **CONCEDED.** Reworded in the A-01 re-verification and in the ADV-027 expected behaviour. P1 is kept. | Resolved |
| C-7 | ADV-026: H1→H0; add DOC-DIVERGENCE for `PRODUCT.md:193-194` | `onRecoverRequested` (`:5675-5679`) clears the checkpoint and notes the project persisted. `PRODUCT.md:193-194` "recovered is not asked about" | **CONCEDED.** H0 (CLAUDE.md Autosave rule). The DOC-DIVERGENCE note is added to the dependencies column. | Resolved |
| C-8 | ADV-067: wrong cause and wrong fix | `ExactExpression.java:35` maps `,`→`.`. The three canvas editors parse with `LengthUnit.parse` (`CadExtrudeCanvasView:1284`, `SketchDimensionLabelView:264`, `BodyDimensionLabelsView:302`). Their key listeners are `"0123456789.-"` (`:644`, `:112`, `:127`). `CadRevolveAngleLabelView:87` and `NumericPropertyRow:109` allow `,` | **CONCEDED.** The cause is now the key-listener allowed set, and the fix is "add `,` to the three key listeners". E2, DEFECT-CONFIRMED, owned by F-05. J3 now evidences only that the parser works. | Resolved |
| C-9 | ADV-061: seams are inert, not compiled out; the guard does not probe them; the guard run had a rerun | `ci-release-selftest-guard.sh:11-17` probes `SelfTests` symbols and token strings only. No-op bodies at `forgeshape_jni.cpp:7967-7972` and `:9083`. `run_status.txt` records `guard_exit=2` at 00:54:48. `release_guard.txt` mtime is 00:57:29 and reads PASS | **CONCEDED.** Self-test absence: E1 PASS-REPRODUCED, stated as coming from the unrecorded rerun. Debug seams: PASS-SOURCE (E2), "inert in release". | Resolved; the rerun's own exit status is not recorded in `run_status.txt` and stays an open provenance note |
| C-10 | Positive claim 2 over-generalised | `j4_replay.out` has exactly four refusals. J5 probes numeric values | **CONCEDED.** Reworded to the four probed refusals plus the 93 numeric refusals. The unprobed refusals are listed. | Resolved |
| C-11 | ADV-001: J2 never reached a refused load | `J2_SUMMARY … loadRefused=0`. FSR1A_07 checks at `forgeshape_project_selftest.cpp:1834-1843` and FSR1A_13 at `:1845`. The fresh PROJECT suite is 256/0 (`host_selftests.txt:14`); that file prints suite totals, not check names | **CONCEDED.** The E1 now cites FSR1A_07 and FSR1A_13 through the fresh PROJECT suite pass. J2 is kept only for memory safety and decode/load agreement. | Resolved |
| C-12 | Device dating for HomeFlowTest / EditorWorkspaceProjectActionsTest | `TESTS.md:75` lists both in `40782861799` 67/67. Product and androidTest trees are identical to `70a6c14` | **CONCEDED.** ADV-012, ADV-028, ADV-071, ADV-041 (and ADV-037, which also cites HomeFlowTest) now carry E3 `40782861799`. ADV-041 keeps its E1 from J4 and adds E3. "No device run on this tree" now applies only to `ProjectAutosaveRecoveryTest`, `JniBoundaryHardeningTest` and `ObjectCommandsTest` (superseded at repair: all three passed in CI DEVICE `40805804385`, 90/90, emulator only). | Resolved |
| C-13 | ADV-039: J1 never cancels | `CADUXR1_33_edit_sketch_cancel_is_exact_non_mutation` (`forgeshape_sketch_ux_selftest.cpp:638`). Fresh SKETCH_UX is 118/0 | **CONCEDED.** That check is added as the Cancel evidence. The verdict stands. | Resolved |
| C-14 | Claims 3, 4, 5, 7, 8 hold | — | Acknowledged. | Resolved |
| C-15 | ADV-018 / J-03 has no repo source for the expectation | `projectSemanticFingerprint` mixes `nextObjectId` (`forgeshape_project_state.cpp:753`). The `.forge` rule stores the allocator mark. APP-H1 asks by fingerprint | **CONCEDED.** ADV-018 → BLOCKED-OWNER (OD-J3), H2-pending-decision. A note is added that CAD id marks are restored by Undo. | Resolved |
| C-16 | J-02 / ADV-066: the sketch Offset field is seeded correctly at entry | `SketchModifyView.java:280-282` formats native metres in the display unit on entering Offset. `onDisplayUnitRequested` (`EditorWorkspaceView.java:6403-6416`) does not convert it | **CONCEDED.** Offset is narrowed to "not converted on a mid-mode unit change" (E2). Whether a unit chip is reachable while Offset mode is open was not checked (E5). Freeform and Surface stand as stated. | Resolved |
| C-17 | J-04 / ADV-013 impact text | `saveProjectToSlot` returns before `ProjectCheckpoint.clear` on a failed write (`EditorWorkspaceView.java:5459-5463`) | **CONCEDED.** The impact is reworded: the previous slot copy is lost, the newer bytes sit orphaned in `.pending`, and current work stays in memory and in the checkpoint. P3 DEFECT-CONFIRMED is kept. | Resolved |
| C-18 | HJ-1 scope note | CAD-UX-S1: extrude and revolve drags are session-only until commit | **CONCEDED.** Scope note added: gizmo and Freeform drags only. | Resolved |
| C-19 | Revolve contradiction with SEAT_C | Same as C-1 | **CONCEDED** (withdrawn). | Resolved |
| C-20 | Gap in SEAT_C (J-01, F-05 cross-refs) | This is a demand on seat C, not on seat J | No action for seat J. Seat J agrees that seat C should cross-reference J-01 (BLOCKED-OWNER, OD-J1) and F-05. | Seat C's to close |
| C-21 | No contradiction on ADV-038/040/060, ADV-003/005 | — | Acknowledged. | Resolved |

**Tally:** 18 substantive objections conceded (C-1 to C-13 and C-15 to C-19), 0 rebutted. C-14 and C-21 raise no objection, and C-20 is addressed to seat C. Seat J's net severity changes:
- J-01 goes from DEFECT-CONFIRMED to BLOCKED-OWNER, plus a PARTIAL P3 for the missing solid preview.
- J-03 goes from PARTIAL to BLOCKED-OWNER.
- J-05 is narrowed, and its revolve half is withdrawn.
- ADV-067 moves to DEFECT-CONFIRMED with the corrected cause.
- ADV-026 and ADV-046 move to H0.
