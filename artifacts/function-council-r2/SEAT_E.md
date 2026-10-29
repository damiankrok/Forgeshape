# FUNCTION-COUNCIL-R2 — Seat E: Document / I/O / lifecycle / failure containment

## Scope

- The `.forge` codec: header, section framing, CRC, every section and version
  (`SCNE` v1/v2, `CONS`, `IMPT`, `SCUL`, `CADB` v1..v8, `FRFM` v1, `SURF` v1),
  the conditional "lowest version" writers, semantic validation, version
  refusal, corrupt and out-of-range input.
- The 72-fixture corpus and its byte identity: `testdata/forge/v1`,
  `scripts/build-forge-corpus.ps1`, `scripts/host-forge-corpus-verdicts.sh`.
- `projectSemanticFingerprint`.
- Android persistence: the manual slot, Save, Save Copy, Open Saved Project,
  Open File (`ProjectTransfer`, SAF), autosave/checkpoint/recovery
  (`AutosaveController`, `ProjectCheckpoint`), the unsaved-changes question,
  process death, rotation/resume/recreation, background races, temp+fsync+rename.
- History reset on project replacement, project establishing/closing paths and
  the process-scoped reset list.
- GLB import/export bounds and refusals.
- Re-check of the old Council findings D1..D7 and the "import during sketch"
  observation on the current code.

Out of scope: rendering, CAD modelling semantics beyond what the codec
validates, UI layout. No product, test, fixture, script, workflow or doc file
was modified.

## Sources read (exact paths, revisions)

The audited product tree is `70a6c14` (HEAD `9cfc3f5` adds only
`artifacts/function-council-r2/BEFORE.md`; `git diff --stat 70a6c14 HEAD --
app scripts testdata` is empty).

- `artifacts/function-council-r2/BEFORE.md`
- `artifacts/function-council-r1/COUNCIL_FINDINGS.md` (§1 D1..D7),
  `OWNERSHIP_MAP.md:111`
- `artifacts/function-council-c1-d1/SUMMARY.md` (D1 fix; "Import GLB during an
  open sketch" debt)
- `CLAUDE.md` (the `.forge`, Autosave, recovery, Uri, GLB, APP-H1, Stage 018A,
  CAD-A3, CADB v2..v8, FRFM/SURF, Export, ARCH-OWNER-07 rules)
- `PROJECT_STATUS.md` §Owner Decision Baseline (2486+), §Known Issues (4018+),
  §Technical Debt (4216-4262, incl. D2..D7 and the Stage027 GUARD-2 debt),
  §Android UI suites (3379-3419), §Next Stage (4851+)
- `PRODUCT.md` §Starting a project (131+), §Saving and reopening (1988-2016),
  §When ForgeShape closes before you saved (2018-2039), §Moving a project
  (2041-2056), §Sending the model (2058+), §Importing a GLB file (2099+)
- `DATA_PACKAGE_SPEC.md` §7c (515-580) and the fixture table
- Fresh run outputs (not fabricated, read from disk):
  `/tmp/claude-0/-home-user-Forgeshape/2f5454c6-63b1-52b8-b92c-5f8b69c420c6/scratchpad/fc2/`
  `host_selftests.txt`, `host_corpus_verdicts.txt`, `gradle.txt`,
  `gradle_unit_rerun.txt`, `release_guard.txt`, `run_status.txt`; JUnit XML in
  `app/build/test-results/testDebugUnitTest/`.

## Inspected code paths

Native codec and project state:

- `forgeshape_project_document.cpp`
  - `validateProjectDocument` 716-1185: empty or too large → `ImpossibleCount`
    (718); duplicate or zero ids; `nextObjectId` ≤ highest (747); active not
    found (758); exactly-one-representation `covered[]`; `SCUL` refused over
    CADB/FRFM/SURF (1142); CAD regeneration during validation, which SKIPS
    PlanarFaces bodies (934-940).
  - `encodeProjectV1` 1416; `encodeProjectV1Unchecked` 1428-1718: SCNE v2
    predicate (1430, 473-480); CADB lowest version (1526-1534, 1694-1704);
    IMPT, CADB, FRFM and SURF always required (1687-1716).
  - `decodeProjectV1` 2775-2958: section header, the CRC before meaning
    (2815), unknown required (2833), duplicate tag (2849), version gate
    (2856-2904), header-flag truth (2948), semantic validation (2952).
  - `decodeProject` 2960-3025: magic, major (2994), header size, kind, flags,
    file length (3013).
- `forgeshape_project_freeform.cpp:17-125`, `forgeshape_project_surface.cpp`.
- `forgeshape_project_state.cpp`: `runtimeCanEvaluateProject` 118-133;
  `captureProjectDocument` 135-245; `loadProjectDocument` 247-441 (refused
  while an edit is open at 250; stage; commit with `cancelStroke` 403,
  `reserveObjectIdsThrough` 415, enter mode 421-425, `history.clear()` 430);
  `projectSemanticFingerprint` 745-853.
- `forgeshape_project_state.h:46-63`. This comment is stale: it says
  PlanarFaces is not loadable.
- `forgeshape_project_bootstrap.cpp:17-94` (`commitFirstCadProject`, through
  `loadProjectDocument`).
- `forgeshape_sketch.cpp:723-749` (`cadFaceTokenCode`, `sameCadFaceToken`:
  only the low 16 bits of `edgeLocalIndex` are used).

JNI (`forgeshape_jni.cpp`): `encodeProject` 8387; `loadProject` 8440-8500
(resets the grab, the stroke and the sketch only); `exportGlb` 8506;
`importGlbDurable` 8560-8620 (Sculpt guard 8592; no sketch guard);
`validateProject` 8898-8925; `projectFingerprint` 8936; `projectOpen` 8957;
`closeProject` 8968-8990 (no `editInProgress` check); `beginSketchView`
313-320 (reads `frame()`).

Other native files:

- `forgeshape_import_commit.cpp:61-181`
- `forgeshape_gltf_export.cpp:155-317` (`captureGlbExportScene` uses
  `body.transform()` at 176-185); `encodeGlb` 320-365
- `forgeshape_scene.cpp:277-330` (`resolveWorldModel`)
- `forgeshape_freeform_session.cpp:299-330` (`reconcile`)
- `forgeshape_history.cpp:221-227` (`clear` drops `editOpen_`)
- `forgeshape_sketch_session.cpp:648-668` (`viewFrame`, no production
  caller), 2904-2935, 3757-3781 (`overlay` revision bump)
- `forgeshape_sculpt_history.cpp:80-102`
- `forgeshape_sculpt.cpp:1687-1690` (`canClearMask`)

Android:

- `ProjectSlot.java:77-155`
- `ProjectCheckpoint.java:85-185`
- `ProjectTransfer.java:89, 172-240`
- `AutosaveController.java` (`performCheckpoint`, `noteProjectPersisted`,
  `awaitIdle`)
- `EditorWorkspaceView.java`: 271-272 (`persistedFingerprint`/`everPersisted`
  view fields); 1019-1029 (the recovery offer in the constructor); 2403-2465
  (New Sculpt / Freeform / Surface); 2597-2700 (dirty guard, Discard); 2823
  (`showImportAvailable(!sculpting)`); 5440-5512 (Save / Open slot);
  5579-5700 (recovery); 5706-5803 (Save Copy, Export); 5835-5935 (Import);
  6084-6118 (Open File)
- `EditorUiState.java:41-64, 151, 235-254`
- `ForgeShapeActivity.java:268-280` (`requestTheme` → `recreate`), 351-360
  (`onStop`), 569-584 (`onDestroy`)
- `AndroidManifest.xml:31` (`configChanges`)

## Test coverage of the domain (what it actually asserts)

**Host native suites (fresh, E1).** `HOST_SELFTESTS_OK (4392 checks, 0
failed)`, from `fc2/host_selftests.txt`.

| Suite | Checks | What it asserts |
| --- | --- | --- |
| `PROJECT` | 256 / 0 | Encode/decode/validate; committed digests `FSR1A_12_*` and `IMP01A_19_*` (`forgeshape_project_selftest.cpp:2330-2360`) |
| `GLTF_EXPORT` | 93 / 0 | Bake, determinant refusals, determinism |
| `GLTF_IMPORT` | 189 / 0 | Bounded subset, named refusals |
| `RENDER_RECOVERY` | 24 / 0 | Recovery policy |
| `SCENE` | 175 / 0 | Scene behaviour |
| `CONSTRUCTION_HISTORY` | 147 / 0 | History, including the session-initialization bracket |
| `FREEFORM`, `SURFACE` | — | Print `FRFM`/`SURF` golden digests; seat E compared all six with `sha256sum` of the committed fixtures: 6/6 equal |

No host check covers GLB export of a CAD body, of a face-supported body, of a
Freeform body or of a Surface body. `forgeshape_gltf_export_selftest.cpp`
contains no CAD case.

**Corpus verdicts (fresh, E1).** `scripts/host-forge-corpus-verdicts.sh` →
`CORPUS_FIXTURES 72`. 44 decode Ok, and every Ok CAD project reports
`cad_bodies == regenerated`. 28 are refused by name:

- `UnsupportedMajor`, `Truncated`, `ChecksumMismatch`, `UnknownRequiredSection`
- `BadPayload` (`object_state_bad_flags_v2`)
- `UnresolvedReference` (`cad_dependency_cycle_v2`)
- `InvalidSemanticValue` for every other `bad_*`, `noncanonical`,
  `unresolved`, `overlap`, `conflict` and `surface_bad_ref` fixture

**Corpus parity with the PowerShell builder.** NOT RUN locally (no `pwsh`).
E3: CI FAST `40787465884` step ".forge corpus parity" (72/72), as recorded in
`BEFORE.md`.

**JVM (fresh, E1).** 20 classes, 196 tests, 0 failures, 0 errors (JUnit XML).
`DiagnosticLogTest` (11) is the only class in this domain. No JVM test covers
`AutosaveController`, `ProjectSlot` or `ProjectCheckpoint`.

**androidTest (historic, E3; not run in this audit).** Counts from
`PROJECT_STATUS.md` 3379-3419:

| Class | Tests | Cases in this domain |
| --- | --- | --- |
| `ProjectAutosaveRecoveryTest` | 14 | `fsr1b01..09` |
| `ProjectTransferTest` | 13 | `fsr1b10..13`, `fsr1b18` |
| `ProjectProcessDeathTest` | 8 | Stages 1-8, run through `scripts/run-project-persistence-e2e.ps1` |
| `EditorWorkspaceProjectActionsTest` | — | `e2er1a01/04/05/06` |
| `HomeFlowTest` | 12 | `e2eAppH1_01..12` |
| `ImportedMeshDurableTest` | — | — |
| `ImportGlbSculptGuardTest` | 3 | `d1_02..04`; CI DEVICE `36643101662` attempt 2 |
| `GlbExportTest` | — | — |
| `GlbImportExternalR1Test` | — | — |
| `DiagnosticsAndRendererLossTest` | 9 | — |

No androidTest asserts any of the following:

- recovery behaviour after a recreation in a process that had no candidate;
- `persistedFingerprint` across a palette change;
- Import while a sketch is open;
- the export placement of a face-supported body.

**Seat-E diagnostics (fresh, E1).** Scratch only, never in the build. Each
program is compiled against the host objects the suite script built:

```
g++ -std=c++17 -O1 -Iapp/src/main/cpp -Iapp/src/main/cpp/third_party/manifold/include \
  -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM \
  /tmp/claude-0/fc2-scratch/E/<prog>.cpp $(ls build/host-selftests/obj/*.o | grep -v _selftest.o) \
  build/host-selftests/kernel/libmanifold.a -o /tmp/claude-0/fc2-scratch/E/<prog>
```

| Program | Input | Output |
| --- | --- | --- |
| `codec_diag` / `codec_diag3` | all 72 fixtures | `codec_diag.out`, `codec_diag3.out` |
| `faceidx` | `cad_face_sketch_side_v2` | `faceidx.out` |
| `pf_gap` | `cad_mixed_selection_v6`, `cad_fragment_support_v6` | `pf_gap.out` |
| `freeform_load` | `freeform_box_v1`, `freeform_crease_symmetry_v1` | `freeform_load.out` |
| `hidden_sculpt` | the three `*sculpt*` fixtures | `hidden_sculpt.out` |
| `export_face` | `cad_face_sketch_cap_v2`, `side_v2`, `chain_v2`, `cad_rectangle_v1` | `export_face.out` |

## Confirmed findings

### E-01 — P1 — GLB export puts a face-supported CAD body in the wrong place

- **Location.** `app/src/main/cpp/forgeshape_gltf_export.cpp:176-185`,
  `captureGlbExportScene`.
- **What is wrong.** The export splits `body.transform()` into node
  translation and baked linear part. A face-supported body's drawn placement is
  `ConstructionScene::resolveWorldModel` (`forgeshape_scene.cpp:277-330`),
  which composes the producer's world model with the face frame. Its own
  transform is the stored identity.
- **Repro.** `export_face` loads a face fixture, calls
  `captureGlbExportScene`, and compares each body's export translation with its
  resolved world model:

  | Fixture | Dependent | Exported translation | Drawn translation |
  | --- | --- | --- | --- |
  | `cad_face_sketch_cap_v2.forge` | body 2 | `(0,0,0)` | `(1.5,0.5,0)` |
  | `cad_face_sketch_side_v2.forge` | body 2 | `(0,0,0)` | `(1,0,1)`, plus the side face's rotation |
  | `cad_face_chain_v2.forge` | bodies 2 and 3 | `(0,0,0)` | `y=1.0` and `y=1.5` |

  Independent bodies match exactly.
- **Impact.** A face-supported body is exported at the origin, unrotated,
  without any error. That breaks CLAUDE.md "it exports EVERY body" and
  PRODUCT.md "each in the place, at the angle … you put it". Any New Body made
  from a face sketch is face-supported (`SketchSession::operationAvailable`
  2926 and `candidateState` 2838-2840), so normal OWNER-accepted CAD-A3 work
  reaches this path.
- **Grade and verdict.** E1. DEFECT-CONFIRMED.
- **Fix.** In `captureGlbExportScene`, take the model from
  `scene.resolveWorldModel(id, &model)`. Put its translation on the node and
  bake its linear 3×3 into the vertices, with normals by inverse-transpose, as
  today. If it does not resolve, fail by name. Add one host check per face
  fixture.

### E-02 — P2 — The recovery question can reappear over the live project after an Activity recreation

- **Location.** `EditorWorkspaceView.java:1027-1029` and `5579-5582`;
  `EditorUiState.java:151`.
- **What is wrong.** `recoveryResolved` is a static flag. It is set only when a
  candidate existed and was answered or quarantined. A process that started
  with no candidate never sets it. Later, autosave writes `recovery.forge` for
  the live session, and any recreation runs the constructor's
  `offerRecoveryIfPresent()` again. It offers the session's own checkpoint over
  the open project.
- **Recreation triggers.** A palette change (`requestTheme` → `recreate()`,
  `ForgeShapeActivity.java:268-280`). Any configuration change not in
  `configChanges` (`AndroidManifest.xml:31` lacks `uiMode`, `locale`,
  `fontScale`).
- **Repro.**
  1. Cold launch with no `recovery.forge`.
  2. Create a project and edit it; wait at least 0.9 s, or background the app.
  3. Open Settings… and change the palette, or toggle system dark mode while
     the app is in the background.
  4. Return. The prompt "Recover" / "Discard" stands over the open project.
- **Existing test that shows the mechanism.** `fsr1b04` does
  `askRecoveryAgain()` + `recreate()`, which is exactly the state a
  no-candidate process is in.
- **Impact.**
  - It contradicts PRODUCT.md "it asks — once" and CLAUDE.md "Nothing replaces
    the live project without the user saying so".
  - Recover reloads the checkpoint with no dirty question and clears the live
    Construction Undo history. If the checkpoint was written before the
    newest edits, those edits are lost: on a foreground recreate, `onStop`'s
    immediate checkpoint runs on the old worker while the new constructor
    reads the file.
- **Grade and verdict.** E2, plus the E3 mechanism in `fsr1b04`.
  DEFECT-CONFIRMED.
- **Fix.** Call `uiState.recordRecoveryResolved()` after the first
  `offerRecoveryIfPresent()` in a process, whatever its outcome. Or offer only
  when `!NativeViewport.projectOpen()`.

### E-03 — P2 — Import GLB is reachable and unguarded while a sketch is open (CONFIRMED-OPEN)

- **Location.** `forgeshape_jni.cpp:8586-8604`, which asks only
  `inSculptMode()`; `EditorWorkspaceView.java:2823`,
  `showImportAvailable(!sculpting)`.
- **What is wrong.** The ForgeShape mark stays drawn while a sketch is open in
  a project; it is withdrawn only in the bootstrap. Nothing withdraws or
  refuses Import there. `commitImportedGlbScene` records a
  `ScopedConstructionEdit` and calls `setActiveBody`
  (`forgeshape_import_commit.cpp:150, 175`).
- **Impact.** CLAUDE.md says creation, body switching and Construction
  Undo/Redo are "refused below JNI and withdrawn above it" while a sketch is
  drawn. Import does both creation and body switching.
- **Grade and verdict.** E2. The runtime effect was not reproduced.
  DEFECT-CONFIRMED.
- **Fix.** Refuse in `importGlbDurable` when `sketchSession().active() ||
  supportChooser().active()`, under the same lock. Add `&& !sketching` to the
  chrome predicate.

### E-04 — P2 — A palette change forgets that the project was saved (D3, CONFIRMED-OPEN)

- **Location.** `persistedFingerprint` and `everPersisted` are view fields
  (`EditorWorkspaceView.java:271-272`). They are not carried by
  `EditorUiState.carryAcrossRecreation`.
- **Repro.** Save, change the palette, then New Project…. The unsaved-changes
  question appears for a saved project. The same happens on an un-handled
  configuration change.
- **Grade and verdict.** E2. DEFECT-CONFIRMED.
- **Fix.** Move both fields into the process-scoped `EditorUiState` statics.

### E-05 — P3 — A PlanarFaces body whose later feature does not regenerate passes decode/validate but fails load

- **Location.** `forgeshape_project_document.cpp:934-940` skips
  `generateCadMesh` for `cadBodyStateUsesPlanarFaces` bodies. Its own comment
  at 927-930 (and `forgeshape_project_state.h:59-63`) still says "the runtime
  refuses to load it", but `runtimeCanEvaluateProject` returns true since
  `CAD-V6-S2` (`forgeshape_project_state.cpp:128-132`).
- **Repro.** `pf_gap` takes `cad_mixed_selection_v6` and
  `cad_fragment_support_v6` and flips the later feature's operation or
  direction:

  | Step | Result |
  | --- | --- |
  | `regenerateCadBody` | `CutNoIntersection` / `AddNoEffect` |
  | `decodeProject` of the re-encoded bytes | Ok |
  | JNI `validateProject` equivalent | Ok |
  | `loadProjectDocument` | `InvalidSemanticValue` |

- **Impact.** A recovery candidate can be offered and then fail to recover:
  it is quarantined and reported. Open File reports "invalid". Both stay
  fail-closed. Only a crafted file, or a corrupt one with a valid CRC, can
  carry such a body.
- **Grade and verdict.** E1. DEFECT-CONFIRMED.
- **Fix.** Regenerate every chain with later features in validation, and
  correct the two comments.

### E-06 — P3 — The codec accepts two encodings of one semantic face

- **Location.** `forgeshape_sketch.cpp:734-749`. `cadFaceTokenCode` and
  `sameCadFaceToken` use `edgeLocalIndex & 0xFFFF`. The decoder never refuses
  a value above 0xFFFF.
- **Repro.** `faceidx` sets `faceEdgeLocalIndex = 0x00080001` in
  `cad_face_sketch_side_v2.forge` (CADB payload offset 101) and fixes the CRC.

  | Check | Result |
  | --- | --- |
  | decode | Ok |
  | load | Ok |
  | fingerprint | equal (`e5ebec3e274d33d4`) |
  | `sameProjectDocument` | 1 |
  | re-save | keeps the non-canonical bytes |

  The fuzz found 12 equal-fingerprint / different-byte cases in CADB payloads across seven CAD fixtures. In every one, the first differing byte was 0x00 in the original, which is consistent with the high bytes of a face-token index. Only `cad_face_sketch_side_v2` was verified field by field.
- **Impact.** It breaks the codec's one-solid-one-encoding discipline, and the
  fingerprint cannot see the difference. The product never writes such a
  value.
- **Grade and verdict.** E1. DEFECT-CONFIRMED.
- **Fix.** Refuse `faceEdgeLocalIndex > 0xFFFF` in every face-token reader
  (v2 TopoRef, v5 support, v6 token), and state the bound in
  `DATA_PACKAGE_SPEC.md` §7c.

### E-07 — P3 — The Freeform edit session survives a project load with a stale selection

- **Location.** `loadProject` (`forgeshape_jni.cpp:8460-8476`) and
  `closeProject` do not end `freeformEditSession()`. `reconcile`
  (`forgeshape_freeform_session.cpp:299-308`) keeps the session when the new
  active body has the same ObjectId and is Freeform. It prunes the selection
  only by id existence.
- **Repro.** `freeform_load`:
  1. Load `freeform_box_v1` and begin editing.
  2. Select face 6.
  3. Load `freeform_crease_symmetry_v1`.
  4. Call `reconcile`.

  The result is `active=1 body=1 selection=[6]`: face 6 of a cage the user
  never selected.
- **Impact.** UI reachability is UNDETERMINED: it depends on the Shape tool
  staying held across Open. The selection is drawn, so it is visible.
- **Grade and verdict.** E1 at the native boundary. DEFECT-CONFIRMED.
- **Fix.** Call `freeformEditSession().end()` in `loadProject` and
  `closeProject`.

### E-08 — P3 — Discard, then a cancelled or refused Open, leaves the live dirty project without crash protection

- **Location.** `onUnsavedDiscardRequested` calls `ProjectCheckpoint.clear`
  before `proceedToLeave(LEAVE_OPEN_FILE / LEAVE_OPEN_SLOT)`
  (`EditorWorkspaceView.java:2680-2690`). If the picker is cancelled or the
  file is refused, the project stays live (by design, 2640-2645). Autosave's
  `checkpointedFingerprint` still equals it, so no new checkpoint is written
  until the next edit.
- **Grade and verdict.** E2. DEFECT-CONFIRMED.
- **Fix.** Retire the checkpoint only after the replacing project is live, or
  reset the autosave baseline.

### E-09 — P3 — A Sculpt project loads into Sculpt on a hidden active body (Stage027 GUARD-2 debt, CONFIRMED-OPEN)

- **Location.** `forgeshape_project_state.cpp:421-425`.
- **Repro.** `hidden_sculpt`: for the three sculpt fixtures with the active
  body hidden, encode, decode and load all return Ok, then `inSculptMode=1`
  with `activeVisible=0`.
- **Impact.** Only a crafted file reaches it, because Sculpt refuses Hide.
- **Grade and verdict.** E1. DEFECT-CONFIRMED.
- **Fix.** Load into Construction, or refuse, when the active body is hidden.

### E-10 — P3 — `closeProject` does not refuse an open Construction edit (D7, CONFIRMED-OPEN)

- **Location.** `forgeshape_jni.cpp:8968-8990`. `ConstructionHistory::clear`
  sets `editOpen_ = false` (`forgeshape_history.cpp:221-227`), while
  `loadProjectDocument` refuses at `forgeshape_project_state.cpp:250`.
- **Impact.** No shell path reaches it mid-drag today.
- **Grade and verdict.** E2. DEFECT-CONFIRMED.

### E-11 — INFO — Documentation divergences

1. **D5 is closed in code but still listed as open.** `PROJECT_STATUS.md`
   §Technical Debt (4245) lists D5. The code fixed it in `5da5d85`
   (`forgeshape_sketch_session.cpp:3768-3779`), and the host check
   `CADFC1_S4_a_a_zoom_rebuild_advances_the_overlay_revision` passes in the
   fresh `SKETCH_UX` 118/0. The listing is stale.
2. **PRODUCT.md describes the old project control.** `PRODUCT.md:1990-1992`
   still says "a project control beside Export … the two things that work".
   `PRODUCT.md:180` and the code put this in the ForgeShape mark's drawer.
3. **Stale PlanarFaces comments.** The comments at
   `forgeshape_project_state.h:59-63` and
   `forgeshape_project_document.cpp:927-930` (see E-05).

Grade E2. Verdict DOC-DIVERGENCE.

## Debt recheck: old Council D1..D7 and import-during-sketch on the current tree

| Item | Status now | Evidence |
| --- | --- | --- |
| D1, Import in Sculpt | **CLOSED** | `forgeshape_jni.cpp:8592` (`RefusedInSculpt`), `EditorWorkspaceView.java:2823/5837/5856/5884`. Tests `ImportGlbSculptGuardTest` d1_02..04 (CI DEVICE `36643101662`, attempt 2, E3) and GLTF_IMPORT host 189/0 (E1) |
| D2, navigator Flip/±90° do not move the camera | **CONFIRMED-OPEN** | `beginSketchView` reads `sketchSession().frame()` (`forgeshape_jni.cpp:318-319`); `SketchSession::viewFrame()` (`forgeshape_sketch_session.cpp:648`) has no production caller (`grep viewFrame() *.cpp`). `sketchSetViewFlipped` and `sketchRotateView` (6611-6645) call `beginSketchView`. E2 |
| D3, palette change forgets the saved baseline | **CONFIRMED-OPEN** | E-04: `EditorWorkspaceView.java:271-272`, `ForgeShapeActivity.java:268-280`. E2 |
| D4, Undo after an over-cap stroke | **CONFIRMED-OPEN** | `SculptHistory::record` clears redo (`forgeshape_sculpt_history.cpp:91`) and returns `NotRetained` while keeping the undo stack (93-97). Entries hold absolute positions. E2 |
| D5, stale sketch overlay after zoom | **CLOSED** | `forgeshape_sketch_session.cpp:3768-3779` bumps `overlayRevision_` on `cameraChanged` (commit `5da5d85`, CAD-FOUNDATION-C1). Host check `CADFC1_S4_a_a_zoom_rebuild_advances_the_overlay_revision` in SKETCH_UX 118/0 (E1). `PROJECT_STATUS.md:4245` still lists it open (DOC-DIVERGENCE, E-11) |
| D6, Clear Mask drawn then refused | **CONFIRMED-OPEN** | `canClearMask` (`forgeshape_sculpt.cpp:1687-1690`) still has no entry-size test. E2 |
| D7, `closeProject` vs an open edit | **CONFIRMED-OPEN** | E-10: `forgeshape_jni.cpp:8968-8990`. E2 |
| Import GLB during an open sketch | **CONFIRMED-OPEN** (source; runtime not reproduced) | E-03: `forgeshape_jni.cpp:8586-8604`, `EditorWorkspaceView.java:2823`. E2 |

## Hypotheses (unproven)

- **H-E1. An autosave write can land after Save clears it.** A checkpoint
  write in flight on the autosave worker can complete AFTER Save's
  `ProjectCheckpoint.clear()` (`EditorWorkspaceView.java:5467`). There is no
  shared lock between `write` and `clear`. That would leave a `recovery.forge`
  of already-saved work; with no further edit, the fingerprint is unchanged
  and the file is never rewritten. The next cold launch would offer a
  redundant recovery. Not reproduced. E5.
- **H-E2. A large project can be saved but not reopened.** The writer has no
  byte ceiling. `ProjectSlot.read`, `ProjectCheckpoint.read` and
  `ProjectTransfer.readFrom` refuse files over 256 MiB. An imported project
  can exceed that: per mesh, up to 4 M vertices and 24 M indices is about
  190 MB, and up to 4096 meshes are allowed. Such a project would save but
  never reopen, and its checkpoint would be quarantined. Java heap limits may
  stop the encode first. `kMaxImportedGlbBytes` (512 MiB) is unreachable
  through the 256 MiB reader. E2 for the asymmetry, E5 for reachability.
- **H-E3. A crash may undo a rename.** No parent-directory fsync follows the
  rename, so a power loss could revert it. The old file survives, so this is a
  durability, not a corruption, risk. E5.
- **H-E4. Save can stall the UI thread.** Save encodes and fsyncs on the UI
  thread (`saveProjectToSlot`), which could jank or ANR on large sculpt or
  imported projects. E5.
- **H-E5. Import during a sketch may misplace the next commit.** A face
  sketch's Finish after an import that switched the active body could commit
  against an unexpected target. Not traced to a concrete wrong outcome. E5.

## Adversarial counterexample attempts

1. **Fuzzing the decoder past its CRC.**
   - **Method.** 400 random 1-3-byte payload mutations per fixture, with the
     section CRC recomputed so the semantic reader is exercised, plus
     truncation at every 7th length: 34,615 inputs.
   - **Expected failure.** A crash, an Ok decode that the load refuses, or a
     refused load that mutates the live scene.
   - **Outcome.** Every input was refused by name or loaded Ok:
     - no crash;
     - 9,089 Ok decodes, all of which loaded;
     - 0 fail-closed violations (fingerprint and body count unchanged on every
       refused load).

   The positive claim survives. Side result: 346 equal-fingerprint /
   different-byte pairs.
   - 334 are in `IMPT`/`SCUL` payloads, which are documented proxies.
   - 12 are in CADB payloads, which became E-06.
2. **Validate/load drift.** Attempted to make `validateProject` accept what
   `loadProjectDocument` refuses. Succeeded for PlanarFaces + later feature
   (E-05).
3. **Placement truth in a derived consumer.** Attempted to find a consumer of
   placement that bypasses `resolveWorldModel`. Succeeded for GLB export
   (E-01).
4. **"Asked once" across a recreation.** Attempted to re-trigger the recovery
   question across a recreation. Confirmed from source (E-02).
5. **Re-encoding the corpus.** Attempted to break canonical byte identity by
   decode → encode, and by decode → load → capture → encode, over every valid
   fixture. Failed: 43/43 are byte-identical; `unknown_optional_v1` correctly
   drops its skipped optional section. The positive claim survives.

## Positive claims with evidence

- **The decoder fails closed.** No crash, and no partial load, on 34,615
  adversarial inputs (E1, `codec_diag.out`).
- **The writer is canonical.**
  - Every valid committed fixture (43) re-encodes byte-identically, and so
    does a load → capture → encode round trip (E1).
  - The PowerShell builder parity is 72/72 in CI FAST `40787465884` (E3).
- **Every refusal fixture is refused by its expected name** (28 of 28), and
  every valid CAD fixture regenerates (E1, `host_corpus_verdicts.txt`).
- **Load fails closed, and a successful load starts a fresh history.**
  - Atomic staging is at `forgeshape_project_state.cpp:265-400`; the commit
    cannot fail; `history.clear()` is at line 430 (E2).
  - Fuzz shows 0 violations (E1).
- **D1 (Import in Sculpt) is CLOSED.**
  - Native guard at `forgeshape_jni.cpp:8592`; chrome withdrawal at
    `EditorWorkspaceView.java:2823`; picker guard at 5856 (E2).
  - `ImportGlbSculptGuardTest` d1_02..04 passed in CI DEVICE `36643101662`
    attempt 2 (E3).
- **Release builds carry no self-tests.** `RELEASE_SELFTEST_GUARD=PASS`,
  0 symbols and 0 strings in release on both ABIs (E1). `run_status.txt`
  records `guard_exit=2` despite that PASS line; see open questions.

## Journeys proposed for END_TO_END_JOURNEYS

| Id | Kind | Initial state | Steps | Expected |
| --- | --- | --- | --- | --- |
| J-E1 | positive | — | Save → kill process → Open Saved Project | Exact ids, placements and kind; empty history (ProjectProcessDeathTest stages 1-2) |
| J-E2 | negative, fail-closed | A project open with an Undo depth | Open File → pick `cad_bad_two_sides_v4.forge` | Refused as invalid; scene, fingerprint and Undo depth unchanged |
| J-E3 | negative, regression for E-02 | Cold launch, no candidate | Edit → wait for checkpoint → Settings → change palette | NO recovery prompt; project not dirty after an earlier Save (E-04) |
| J-E4 | positive, regression for E-01 | — | CAD body → New Sketch on its top cap → New Body extrude → Export | Each node's translation plus baked geometry equals the drawn world AABB of every body |
| J-E5 | negative, regression for E-03 | A project open | New Sketch (Editing) → open the ForgeShape mark | No Import GLB row; a direct `importGlbDurable` answers a sketch refusal and records no step |

## Owner decisions needed

1. **A face-supported body in export (E-01).** Should the export bake the face
   frame, as the derived placement says, or should face-supported bodies be
   refused from export? The former is consistent with CLAUDE.md.
2. **The recovery offer (E-02).** Should it be strictly once per process, even
   if no candidate existed, or only at Home, with no project open?
3. **Import during a sketch (E-03).** Withdraw it, as the sketch-mode rule
   implies, or allow it as the end of the sketch? Seat E proposes withdrawing
   it.
4. **The size limit (H-E2).** Should there be a write-side byte limit equal to
   the 256 MiB read limit, refused at Save with a named status?

## Unresolved dependencies and open questions

- Corpus parity with the PowerShell builder was not run locally. CI FAST is the
  only evidence.
- No androidTest was run in this audit. Device behaviour of Save, Open,
  recovery and process death is E3 historic, not physical-device evidence.
  These remain OWNER-PHYSICAL-UNVERIFIED.
- `run_status.txt` says `guard_exit=2` while `release_guard.txt` ends
  `RELEASE_SELFTEST_GUARD=PASS`. The script exits 2 only for missing tools or
  APKs, before printing. This is a runner-capture question for the orchestrator
  (seat-independent).
- E-07's UI reachability depends on whether the Shape tool and the Freeform
  context survive an Open in the shell. That needs a device case.
- D2, D4 and D6 belong to other seats. They were assessed from source only (see
  debt recheck).

## Completeness

COMPLETE for the seat's assigned scope (codec, corpus, fingerprint, Android
persistence/lifecycle, GLB bounds, D1..D7 plus the sketch-import re-check).
Device-runtime claims are E3 historic by environment, and corpus parity with
the PowerShell builder is E3 (CI FAST) because there is no `pwsh`.
