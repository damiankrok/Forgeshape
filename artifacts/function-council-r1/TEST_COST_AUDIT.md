# Test-cost audit (FUNCTION-COUNCIL-R1)

Read-only. No test, script or workflow was changed, and nothing is recommended
for deletion. Every time below is **measured** unless marked `est.`

## 1. The answer

- **The 65-minute aggregate is 629 device tests at about 6.2 s each, run one
  after another on ONE emulator.** The five shards do not run in parallel; the
  aggregate's wall time is the sum of the shards (`FULLSHARDED_C4.md` §6).
- **Almost half of it is the Android chrome.** The 21 `EditorWorkspace*`
  chrome classes other than Sculpt retention are 328 tests and **29.5 min (45 %)** of measured test time. They
  assert view ids, visibility, measured rectangles, resolved colours and rail
  composition. None of them needs a GPU, and their own class comments say so.
- **About 13 % is not a pass/fail product gate at all.** Six visual-evidence and
  audit classes (5.2 min) produce frames for the OWNER, and two classes (3.2 min)
  drive the Imported Mesh Preview, a diagnostic path with no user-facing control.
- **Every failure the last three aggregates found was a test defect.** C1–C3
  stopped on 9 failing tests: 2 stale expectations, 4 observation-seam defects,
  2 isolation defects and 1 fixed-delay capture race. None was a product defect
  (`FULLSHARDED_C1.md`..`C3.md`).
- **Each subsystem's real device proof is small.** The 2–4 classes that prove a
  subsystem's user path take 1.5–3.3 min of test time. With the fixed ~8 min of
  a `CI DEVICE` job, that is a 10–12 min focused answer
  (`LEAN_VALIDATION_POLICY.md`).

## 2. Measured facts

| Fact | Value | Evidence |
| --- | --- | --- |
| Inventory | 57 androidTest classes / 629 tests; 11 JVM classes / 104 tests; 23 native suites / 3757 checks | `grep -c @Test`; `PROJECT_STATUS.md` head |
| Aggregate test time | **3925 s = 65.4 min** summed over 629 start/finish pairs; the runner's shard total is 3939 s | C4 `logcat-final.txt` (`TestRunner: started/finished`, 5 execution processes, 0 `failed`) |
| Aggregate job | **72 min 59 s** (`CI FULL SHARDED` `36607747079`, 17:50:10 → 19:03:09) | GitHub run |
| Shards | 710.3 / 909.4 / 811 / 751.5 / 757 s, sequential; partition greedy by **test count**, class-atomic | runner output; `scripts/instrumented-sharding.ps1:102-113` |
| `CI FAST` | **4 min 26 s** job: build + JVM + four APKs 3 min 36 s; corpus parity 7 s; runner and guard checks 14 s | run `36603387133` step timings |
| `CI DEVICE` fixed part | **~8 min**: SDK + emulator image 39 s, APK build 2 min 9 s, AVD create + boot + settle + 23-token proof + a 1-test class 4 min 37 s | run `36547148093` (8 min 14 s job) |
| `CI DEVICE` jobs, measured | 1 test: 8 min 14 s and 9 min 24 s; 43 tests: 15 min 16 s; 57 tests: 18 min 23 s; 126 tests: 26 min 3 s | runs `36547148093`, `36603390799`, `36465122847`, `36462677660`, `36604540195` |
| Focused vs aggregate | a focused dispatch ran its classes at about 1.1–1.3× their aggregate time | run `36462677660` (628 s) vs the same six classes in C4 (479 s) |
| `settleLayout()` | `waitForIdleSync` + **250 ms sleep** + `waitForIdleSync`, at 723 call sites | `WorkspaceTestSupport.java` `settleLayout`; grep |
| Native suites in `CI FAST` | **none** — FAST runs JVM tests and builds; the native suites run only at debug startup on a device | `.github/workflows/ci-fast.yml:122-129` |

## 3. Test layers

| Layer | Protects | Cost | Flake / order / timing risk | Tier |
| --- | --- | --- | --- | --- |
| Static guards (`git diff --check`, `verify-device-guards.ps1`, runner self-tests) | hygiene, no bare `adb`, runner and partition invariants | seconds (14 s in FAST) | none | 0 |
| 23 native suites, host (`host-native-selftests.sh [filter]`) | all domain arithmetic, the codec and 36 pinned digests + the 8 v5 digests (`CADVS_IO_22`), the kernel, regions and features, solvers, sculpt kernel and history, recovery policy | est. 2–3 min first build, seconds per rerun | none: each suite builds its own scene and camera | 1 |
| 23 native suites, device (startup tokens) | the same on the Android ABI | inside every `CI DEVICE` job | liblog drop handled by relaunch (`ci-device-smoke.sh`) | 2/3 |
| JVM, 104 tests | preference rules, theme roles, HUD glyph/hit floors, motion, diagnostics ring, display indices, UI-state refusals, units, pointer map, sketch chrome policy, layout decision | inside FAST's 3 min 36 s build step | none | 1 |
| Corpus parity, 44 fixtures | an independent encoder writes the committed bytes | 7 s | none | 1 (in 3) |
| `CI FAST` | build health on the pinned toolchain + the above | 4 min 26 s | none | 3 |
| `CI DEVICE` default (`Ui3dStateCorrectionTest`) | Vulkan bring-up, 23 tokens, one real-`MotionEvent` class across three modes | ~10 min | low | 3 |
| `CI DEVICE` with a class list | one subsystem's user path on a device | 8 min + 1.1–1.3 × class time | class ORDER matters (§6) | 2 |
| Visual-evidence and audit classes | frames for the OWNER; one class also counts band pixels | 22–49 s each; 5.2 min in all | four of six wait a FIXED delay before capture (§6.3) | 4 |
| `CI FULL SHARDED` | the exhaustive union with a fingerprint | 73 min job | inherits every order dependency | 5 |

## 4. Where the aggregate's time goes (measured)

| Family (disjoint) | Classes | Tests | Minutes | Share |
| --- | ---: | ---: | ---: | ---: |
| `EditorWorkspace*` chrome classes (all but `EditorWorkspaceSculptRetentionTest`) | 21 | 328 | 29.5 | 45.1 % |
| CAD (`CadVerticalSlice`, `SketchUx`, `SpatialSketch`, `CadCanvasExtrude`, `CadExtrudeExtent`, `SketchExtrude`) | 6 | 52 | 7.4 | 11.2 % |
| Sculpt (`SculptUndo`, `SculptBrushStage025`, `SculptHistoryNavigator`, `Stage027SculptWorkflow`, `ImportedMeshSculpt`, `EditorWorkspaceSculptRetention`) | 6 | 42 | 5.8 | 8.8 % |
| Visual evidence and audit — OWNER material (`UiPrefVisual`, `SelectionOutlineVisual`, `SketchUxVisual`, `CadA3Visual`, `Ui3dStateAudit`, `Ui3dDimensionVisibility`) | 6 | 15 | 5.2 | 7.9 % |
| Imported Mesh Preview diagnostic (`GlbImportPreview`, `GlbImportExternalR1`) | 2 | 37 | 3.2 | 4.9 % |
| Persistence (`ProjectAutosaveRecovery`, `ProjectProcessDeath`, `ProjectTransfer`) | 3 | 35 | 2.8 | 4.3 % |
| Import / export (`ImportedMeshDurable`, `GlbExport`) | 2 | 36 | 2.8 | 4.3 % |
| Renderer (`SelectionOutline`, `RendererCounterContinuity`, `DiagnosticsAndRendererLoss`) | 3 | 29 | 2.7 | 4.1 % |
| The rest (`SettingsPreferences`, `Ui3dStateCorrection`, `HomeFlow`, `ObjectsDelete`, `JniBoundaryHardening`, `BodyDimensionsSmoke`, `ObjectCommands`, `MirrorSmoke`) | 8 | 55 | 6.1 | 9.3 % |
| **Total** | **57** | **629** | **65.4** | 100 % |

## 5. Per-class table — all 57 classes

Sorted by measured time. Risk legend: **Rst** calls `resetToBaselineConstruction`;
**Rot** rotates or recreates; **Dbg** reads a `NativeViewport.debug*` seam;
**Scr** takes screenshots; **Sett** `settleLayout` calls; **Slp** raw sleep.
Tier: 1 contract, 2 focused device, 3 FAST/DEVICE smoke, 4 OWNER review,
5 aggregate only. "2 (X)" means "in subsystem X's Tier 2 list".

| Class | Tests | Measured s (C4) | Mean s/test | Unique contract | Overlap | Isolation / order risk | Tier | OWNER review? |
| --- | ---: | ---: | ---: | --- | --- | --- | --- | --- |
| EditorWorkspaceGizmoTest | 46 | 240.7 | 5.2 | gizmo drags through MotionEvent (69), refusals, world/local | GIZMO suite (172) owns every solver | Rst 9 (re-baselines inside cases); Rot 8; Dbg 5; largest class | 2 (Objects) only when gizmo/pointer code changes, else 5 | no |
| EditorWorkspaceCorrectionTest | 37 | 199.2 | 5.4 | UI-R4B visual/motion corrections; status-line hold | Motion, Legibility | Rst; **4 wall-clock sleeps on `STATUS_HOLD_MS`** (`:358-401`, ≈ 3×hold + 4 s); stale-expectation history (`C2.md:45`) | 5 | no |
| EditorWorkspaceThemeTest | 19 | 191.6 | 10.1 | five palettes actually change resolved colours; bars flip | AppThemeTest (JVM); UiPrefVisualEvidence | Rst; Rot 5; 100 ms poll | 5 | frames only via UiPref |
| Ui3dStateAuditTest | 9 | 143.9 | 16.0 | audit LEDGER (records, does not assert; 79 recorder calls, 25 captures) | Ui3dStateCorrectionTest asserts the same family | Rst; MotionEvent 23 | 4 | **yes** (it is an audit by its own javadoc) |
| EditorWorkspaceLegibilityTest | 20 | 129.7 | 6.5 | trailing cluster squeeze, semantic contrast, gizmo touch contract | Foundation, ChromeComposition | Rst; 3 polls (`:1271,1298,1315`); Rot 7 | 5 | no |
| GlbImportPreviewTest | 23 | 118.5 | 5.2 | **diagnostic-only** preview (`NativeViewport.importGlbPreview`, `:614`) — no user control exists (CLAUDE.md "Imported Mesh Preview is a diagnostic") | GLTF_IMPORT suite; ImportedMeshDurableTest for the real feature | Rst; Scr 4 | 5 | no |
| SelectionOutlineTest | 19 | 110.6 | 5.8 | renderer ran mask+composite for each representation; toggle stops it | RENDER_SHADING `selectionOutlineCoverage` reference | Rst 5; closeProject 3; **6 raw sleeps** (`:555,739,748,944,994,996`: 100–400 ms windows around counter reads); Dbg 4; C2 seam + isolation history | 2 (renderer) | no |
| SettingsPreferencesTest | 15 | 109.2 | 7.3 | Settings page, persisted prefs, handedness mirror, byte-identical project | AppPreferencesTest (JVM 13) | Rst; Rot 4; 100 ms poll | 2 (preferences) | no |
| EditorWorkspaceControlsTest | 23 | 104.9 | 4.6 | control ids exist and do what they say; `ui06` starts sculpting on body 1 | Objects, Display | Rst; **leaves a Frozen Sculpt Mesh on body 1 that the reset cannot undo** (`C3.md:64-66`) | 5 | no |
| CadVerticalSliceTest | 7 | 103.5 | 14.8 | regions, Add/Cut, HUD, feature list through chrome; asserts volume/shells from regenerated solid; waits for 6 presented frames before each of 20 captures | CAD_FEATURE (142) owns regions/chain/codec | Rst; presented-frame wait (`:1180` 50 ms poll) is event-based; Scr 20 | 2 (CAD) | frames only |
| GlbExportTest | 24 | 102.9 | 4.3 | exported bytes read by independent `GlbDocument` (39 uses) | GLTF_EXPORT suite (94) owns the bake/refusals | Rst; closeProject 1 | 2 (import/export) | no |
| SketchUxTest | 14 | 94.7 | 6.8 | orientation navigator, line dimension editor, Arc/Spline tools, Edit Sketch | SKETCH_UX suite (106) | Rst | 2 (CAD) | no |
| EditorWorkspaceHistoryTest | 20 | 93.2 | 4.7 | Undo/Redo from the product side; chrome controls | CONSTRUCTION_HISTORY suite (148) | Rst; Rot 3 | 5 | no |
| EditorWorkspaceUnifiedRightHostTest | 18 | 89.6 | 5.0 | static geometry of the right host | RightHostPlacement, Architecture | Rst; **`sleep(1700)`** (`:188`) + 100 ms; Rot 3; stale-expectation candidate (History navigator added a surface) | 5 | no |
| EditorWorkspaceRightHostPlacementTest | 12 | 81.8 | 6.8 | trailing edge kept in every window/state | UnifiedRightHost, Layout | Rst; Rot 3 | 5 | no |
| SculptBrushStage025Test | 8 | 81.5 | 10.2 | Flatten/Crease/Mask through the viewport | SCULPT_BRUSH_KERNEL (598) | Rst 4; `awaitIdle` | 2 (Sculpt) | no |
| SculptUndoTest | 10 | 80.5 | 8.0 | one stroke = one entry, from the product side | SCULPT suite | Rst 4; `awaitIdle` 2 | 2 (Sculpt) | no |
| SculptHistoryNavigatorTest | 9 | 80.4 | 8.9 | navigator rows/jump through chrome | SCULPT suite cursor model | Rst | 2 (Sculpt) when navigator touched | no |
| EditorWorkspaceDisplayTest | 17 | 78.0 | 4.6 | display popover ids; presentation-only (native snapshot unchanged) | RENDER_SHADING suite; DisplaySettingsContractTest (JVM) | Rst; Rot 1 | 5 | no |
| Ui3dStateCorrectionTest | 12 | 74.7 | 6.2 | anchored chrome follows native projection in all three modes | Ui3dStateAuditTest | Rst; MotionEvent 26 | 3 (the `CI DEVICE` default smoke) | no |
| GlbImportExternalR1Test | 14 | 74.0 | 5.3 | widened preview subset through the preview seam; 3 captures | GLTF_IMPORT suite (164) | Rst; Dbg 4; closeProject 3 | 5 | no |
| EditorWorkspaceMobileTest | 15 | 73.8 | 4.9 | what is absent at rest | Composition, Layout | Rst | 5 | no |
| HomeFlowTest | 12 | 72.4 | 6.0 | Home/New Project/CAD bootstrap/Open File/unsaved guard through real chrome | ProjectProcessDeath (recovery), SketchUxTest | `showHomeAsFirstLaunchForTest` 3; closeProject; Rot 2; `awaitIdle` | 2 (project shell) | no |
| ProjectAutosaveRecoveryTest | 14 | 68.7 | 4.9 | checkpoint coalescing, temp+fsync+rename, recovery question; **no sleeps, `awaitIdle` only** (`:1-5` javadoc) | PROJECT suite (codec) | Rst; Rot 4 | 2 (persistence); device-only | no |
| ImportedMeshDurableTest | 12 | 67.1 | 5.6 | durable import as one Undo, `.forge` round trip, `IMPT` | PROJECT + GLTF_IMPORT suites (codec/parse) | Rst 5; closeProject 2; `awaitIdle` 2 | 2 (import/export) | no |
| CadCanvasExtrudeTest | 9 | 66.7 | 7.4 | real drag on the extrude arrow through MotionEvent (18), frozen basis, second-pointer cancel | CAD suite owns anchors/scale/drag arithmetic | Rst; Sett 21 | 2 (CAD) | no |
| SketchExtrudeTest | 9 | 64.4 | 7.2 | sketch→CAD body through chrome and MotionEvents (8) | CAD suite | Rst | 2 (CAD) | no |
| EditorWorkspaceChromeCompositionTest | 12 | 63.5 | 5.3 | UI-R4C composition defects (abbreviated labels, concentric corners) | Legibility, UnifiedRightHost | Rst; Rot 5 | 5 | no |
| CadExtrudeExtentTest | 5 | 60.6 | 12.1 | extent selector + Flip absent/present on device | CAD_FEATURE/CAD suites own extent policy | Rst | 2 (CAD) when extent touched | no |
| ProjectTransferTest | 13 | 59.1 | 4.5 | `Uri`→bytes→document through the production handlers; nothing of storage reaches the project | PROJECT suite; corpus parity | Rst | 2 (persistence); device-only | no |
| ObjectsDeleteTest | 8 | 56.9 | 7.1 | Delete is one transaction; last-body refusal; Undo restores same object | SCENE suite | Rst; `awaitIdle` 2; MotionEvent 2 | 2 (Objects) | no |
| EditorWorkspaceArchitectureTest | 12 | 53.9 | 4.5 | trailing-host ownership/parity (rail entries, hosts) | ChromeComposition, UnifiedRightHost | Rst; Rot 2; **stale-expectation history** (`C2.md:44`) | 5 | no |
| ImportedMeshSculptTest | 9 | 53.0 | 5.9 | sculpt over an Imported Mesh; arrays immutable | SculptUndoTest | Rst 4; closeProject 2 | 5 | no |
| SpatialSketchTest | 8 | 51.3 | 6.4 | spatial support chooser by real taps; stylus hover never commits; dependent survives producer edit and reopen | CAD_A3 suite (67) | Rst; Dbg 5; MotionEvent 19; **known suite-isolation defect** (2 cases fail only in a full shard 5) | 2 (CAD) | no |
| EditorWorkspacePointerTest | 13 | 50.9 | 3.9 | tool type / pressure / tilt survive MotionEvent→JNI (`debugLastPointerEvent`) | PointerSemanticsTest (JVM) map | Rst; MotionEvent 63 | 2 (UI chrome / input); device-only | no |
| EditorWorkspaceMotionTest | 10 | 50.3 | 5.0 | resting states after motion | ChromeMotionTest (JVM) owns decisions | Rst; 2 sleeps (30/50 ms) | 5 | no |
| EditorWorkspaceLayoutTest | 10 | 48.9 | 4.9 | unoccluded viewport fraction per window | RightHostPlacement, Mobile | Rst; Rot 8 (each rotation rebuilds the swapchain) | 5 | no |
| UiPrefVisualEvidenceTest | 1 | 48.8 | 48.8 | 20 frames across five palettes/handedness | EditorWorkspaceThemeTest, SettingsPreferencesTest | Rst 5; `sleep(400)` fixed (`:406`); Rot 4 | 4 | **yes** |
| EditorWorkspaceCompositionTest | 10 | 46.3 | 4.6 | UI-R2 role separation of surfaces | Mobile, Controls | Rst | 5 | no |
| SelectionOutlineVisualEvidenceTest | 1 | 44.8 | 44.8 | 12 frames, band pixel counts per ground family; waits for 6 presented frames, FAILS before capture on timeout (`C4.md:70`) | SelectionOutlineTest | Rst 5; closeProject; frame-poll sleep only (`:714`) | 4 | **yes** |
| DiagnosticsAndRendererLossTest | 9 | 42.5 | 4.7 | injected device loss (`debugInjectDeviceLoss`), rebuild counter, RestartRequired, diagnostics file | RENDER_RECOVERY suite owns the policy | Rst; 100 ms poll (`:367`); Dbg 6; `awaitIdle` | 2 (renderer); device-only | no |
| EditorWorkspaceFoundationTest | 8 | 41.7 | 5.2 | visual foundation + Tool Rail defect | Legibility | Rst; Rot 4; MotionEvent 5 | 5 | no |
| EditorWorkspaceProjectActionsTest | 7 | 41.1 | 5.9 | project surface; refused Open costs nothing | HomeFlowTest, ProjectTransferTest | Rst | 5 | no |
| EditorWorkspaceObjectsTest | 10 | 40.8 | 4.1 | Objects rows, viewport tap selects body | ObjectCommandsTest, ObjectsDeleteTest | **no baseline reset by design** (`:41-49`): "the scene accumulates bodies across a run"; MotionEvent 8 | 5 | no |
| ProjectProcessDeathTest | 8 | 39.9 | 5.0 | cold-launch recovery after real process death (sidecar files, scripted halves) | Autosave | Rst 9; `awaitIdle` 2; runs in two halves | 2 (persistence); device-only | no |
| Stage027SculptWorkflowTest | 4 | 39.4 | 9.9 | Isolate/Hide in Sculpt; pixel reads (Bitmap 18) after **`sleep(600)`** (`:880`) | SCENE suite (restriction) | Rst; Scr 3; Dbg 6; fixed-delay capture race class | 2 (Sculpt) — captures → 4 | partly |
| EditorWorkspaceGestureTest | 6 | 32.1 | 5.3 | gesture consumption ownership; IME survival | Pointer | Rst | 5 | no |
| SketchUxVisualEvidenceTest | 1 | 29.4 | 29.4 | 12 frames of the sketch UX journey | SketchUxTest | Rst; `sleep(400)` fixed (`:395`); Scr 15 | 4 | **yes** |
| CadA3VisualEvidenceTest | 1 | 22.9 | 22.9 | 10 frames of the A3 + Home journey | SpatialSketchTest, HomeFlowTest | Rst; `sleep(400)` fixed before capture (`:310`); Scr 13; Dbg 3 | 4 | **yes** |
| JniBoundaryHardeningTest | 5 | 21.3 | 4.3 | F-08/F-11: no rebind outside lock, no JNI call with pending exception (`debugActiveBodyMisuseCount`) | none (device-only property) | Rst 4; Dbg 5; MotionEvent 7 | 2 (any JNI change); device-only | no |
| Ui3dDimensionVisibilityTest | 2 | 20.3 | 10.2 | dimension overlay pixels present (UI3D-F-005); **`sleep(500)`** before capture (`:472`) | SketchUxTest | Rst; Rot 2; fixed-delay capture race class | 4 | **yes** |
| EditorWorkspaceLifecycleTest | 3 | 17.3 | 5.8 | HOME/resume rebuild of shell from native state | RendererCounterContinuity | Rst | 2 (project shell); device-only | no |
| BodyDimensionsSmokeTest | 1 | 12.0 | 12.0 | one dimensions journey through chrome, native placement read back | BODY_DIMENSIONS suite (78) owns the arithmetic (class javadoc says so) | Rst; no sleep | 2 (Objects/Construction) | no |
| ObjectCommandsTest | 1 | 11.5 | 11.5 | one journey over Rename/Hide/Lock/Duplicate | SCENE/HISTORY suites; ObjectsTest | Rst | 2 (Objects) | no |
| EditorWorkspaceSculptRetentionTest | 2 | 11.3 | 5.6 | Start/Back/Resume loses no sculpt work | Stage027SculptWorkflowTest | Rst | 2 (Sculpt) | no |
| RendererCounterContinuityTest | 1 | 9.5 | 9.5 | process-lifetime counters survive a render-thread restart (C2 mirror fix) | SelectionOutlineTest relies on the same mirrors | manual `ActivityScenario.launch` twice (`:64,82`); polls `POLL_MS` (`:119,154`) | 2 (renderer); device-only | no |
| MirrorSmokeTest | 1 | 7.8 | 7.8 | one Mirror journey | MIRROR suite (63) owns arithmetic | Rst 4 + explicit closeProject since C3 (`C3.md:74-80`) — the one class with a correct hard reset | 2 (Objects) | no |

Notes on the table:

- **Contract and overlap text** is condensed from each class's own header
  comment and test names; the counts in parentheses are `grep` counts of the
  thing named (for example `MotionEvent` constructions or native checks).
- **`EditorWorkspaceObjectsTest` is the only class that does not call the
  baseline reset, deliberately** ("the scene accumulates bodies across a run",
  `EditorWorkspaceObjectsTest.java:41-43`).

## 6. Findings

### 6.1 Too expensive for what they prove

1. **The 21 `EditorWorkspace*` chrome classes, 29.5 min.** They need a real `View`
   tree but not a GPU and not a user. Two of C1's seven failures were stale
   literal counts in this family (`uiar110` rail entries, `uir4b08` anchored
   surfaces). Moving them to a JVM layout runner is a DEPENDENCY decision
   (Robolectric or an inflate-only harness; `app/build.gradle` has neither), so
   short of that they belong in Tier 5 and are not re-run for a task that
   touches no chrome.
2. **`GlbImportPreviewTest` + `GlbImportExternalR1Test`, 37 tests, 3.2 min**,
   drive a path with no user-facing control (`CLAUDE.md`, "The Imported Mesh
   Preview is a diagnostic"). The parser is the `GLTF_IMPORT` suite's; the
   user-facing import is `ImportedMeshDurableTest`'s.
3. **`EditorWorkspaceGizmoTest`, 46 tests, 4.0 min**, re-proves solver
   behaviour the `GIZMO` suite owns; its device-only value is the real drag
   and the locked-body refusal. Tier 2 only when gizmo, pointer or transform
   code changes.
4. **`EditorWorkspaceThemeTest`, 19 tests, 3.2 min (10.1 s per test, the
   highest mean of any chrome class)** re-launches for each palette; the role
   resolution is `AppThemeTest`'s on the JVM.
5. **`Ui3dStateAuditTest`, 9 tests, 2.4 min**, is "an AUDIT and not a product
   gate" by its own comment (`Ui3dStateAuditTest.java:34`): it records rather
   than asserts. It is OWNER material, and it sits in shard 2's pass/fail path.

### 6.2 Device tests that duplicate native or JVM contracts

| Device class | Already proven by | What only the device adds |
| --- | --- | --- |
| `ImportedMeshDurableTest`, `ProjectTransferTest`, `ProjectAutosaveRecoveryTest` (codec halves) | `PROJECT` suite + 44 digests (36 in `PROJECT`, 8 in `CADVS_IO_22`) + corpus parity | the SAF `Uri` boundary, the autosave worker thread, the recovery question |
| `EditorWorkspaceGizmoTest` | `GIZMO` | `MotionEvent` → JNI → transform write |
| `EditorWorkspaceHistoryTest` | `CONSTRUCTION_HISTORY` | which history a chrome tap means in each mode |
| `CadCanvasExtrudeTest`, `CadExtrudeExtentTest` (arithmetic halves) | `CAD`, `CAD_FEATURE`; the 48 dp floor is `CadHudPresentationTest` | the drag through the surface; HUD presence |
| `SketchUxTest` (arc, spline, dimension halves) | `SKETCH_UX` | the navigator and editor chrome |
| `SculptUndoTest`, `SculptHistoryNavigatorTest`, `SculptBrushStage025Test` (kernel halves) | `SCULPT_BRUSH_KERNEL` | strokes through the surface; multi-touch never mutates |
| `EditorWorkspaceThemeTest`, `EditorWorkspaceMotionTest`, `SettingsPreferencesTest` (model half) | `AppThemeTest`, `ChromeMotionTest`, `AppPreferencesTest` | resolved colours and bars on a real Activity; `SharedPreferences` before the theme |
| `GlbImportPreviewTest`, `GlbImportExternalR1Test` | `GLTF_IMPORT` | nothing user-visible |

`BodyDimensionsSmokeTest` and `MirrorSmokeTest` already have the right shape:
one device journey over a native suite that owns the arithmetic.

### 6.3 Needs isolation, not retries

1. **The process is the fixture.** Classes inherit the previous class's scene,
   history and camera. `resetToBaselineConstruction` re-applies a box to the
   first Construction body, cannot un-freeze (`WorkspaceTestSupport.java:72-79`)
   and closes the project only when it has NO Construction body (`:99-103`).
   That is the C3 Mirror defect: `EditorWorkspaceControlsTest.ui06` froze body 1
   and `MirrorSmokeTest` inherited it. The fix C3 applied to Mirror alone
   (cancel, close, open fresh) is the general remedy; `EditorWorkspaceObjectsTest`
   is the one class that should keep inheriting, and says so.
2. **`SpatialSketchTest` is latent, not fixed.** Its minimal reproduction is
   the pair `GlbImportPreviewTest,SpatialSketchTest` (2 of 31 fail), which
   reproduced at a clean baseline: an inherited scene puts another body under
   the aiming tap (`artifacts/sel-out-r1/PREEXISTING_SHARD5_FAILURE.md`). Its
   `setUp` is unchanged. C4 passed because today's partition puts it in
   shard 1 and `GlbImportPreviewTest` in shard 5, so the next partition change
   can bring it back. `PROJECT_STATUS.md` calls it unfixed, which is right; its
   prediction that it "will block the next `-FullSharded` run" did not hold for
   C4, for this reason.
3. **Fixed-delay screenshots** (the C3 race class): `CadA3VisualEvidenceTest:310`
   400 ms, `SketchUxVisualEvidenceTest:395` 400 ms, `UiPrefVisualEvidenceTest:406`
   400 ms, `Stage027SculptWorkflowTest:880` 600 ms, `Ui3dDimensionVisibilityTest:472`
   500 ms. The CI emulator presents about three frames a second through a
   four-image FIFO swapchain, so these can capture a frame from before the
   change. `SelectionOutlineVisualEvidenceTest` and `CadVerticalSliceTest`
   already wait for six presented frames; `Stage027SculptWorkflowTest` and
   `Ui3dDimensionVisibilityTest` ASSERT pixels behind a fixed delay.
4. **Wall-clock status sleeps:** `EditorWorkspaceCorrectionTest:358,363,372,401`
   sleep 3.5 + 2.5 + 5.0 + 6.5 = **17.5 s** against `STATUS_HOLD_MS = 5000`
   (`GlobalToolbarView.java:923`); `EditorWorkspaceUnifiedRightHostTest:188`
   sleeps 1.7 s. `SelectionOutlineTest` brackets counter reads with six sleeps
   of 30–400 ms (`:555,739,748,944,994,996`).
5. **`settleLayout()`'s 250 ms** is correct but blind; a wait for one layout
   pass would remove a floor paid at 723 call sites.
6. **No test orchestrator and no package clearing** (`app/build.gradle`), so
   every class in a shard or a `CI DEVICE` list shares the native process.
7. **A device-global leak on failure.** `EditorWorkspaceLegibilityTest` sets
   `animator_duration_scale 0` (`:783`) and restores it in the test body
   (`:798`), not in its `@After`; a failure between the two leaves reduced
   motion on for every later class. The other classes that change it restore
   it in `@After`.

### 6.3a CI does not do what the docs say on a task branch

`CI FAST` and `CI DEVICE` run on pushes to `main` and `infra/ci-cloud-r1`, on
PRs to `main` and on dispatch (`ci-fast.yml:12-17`, `ci-device.yml:10-15`). A
push to a task branch runs neither, while `docs/CI_CLOUD.md` ("Push it; GitHub
Actions tests it") and `CLAUDE.md` ("push the task branch and let GitHub
Actions test it") say it does. In practice every recent gate was dispatched by
hand. Each `CI DEVICE` job also rebuilds the APKs `CI FAST` already built
(2 min 9 s), and the tablet (EXPANDED) branches of the layout classes never run
on the phone AVD.


### 6.4 Must stay on a device

- **Real input into JNI:** `EditorWorkspacePointerTest`,
  `JniBoundaryHardeningTest`, the CAD drag and tap classes, the Sculpt stroke
  classes, and "a gesture that becomes multi-touch never mutates".
- **Vulkan:** `DiagnosticsAndRendererLossTest` (the only supported device-loss
  path), `RendererCounterContinuityTest`, `SelectionOutlineTest` (the passes
  actually recorded), the rotation classes (identity `preTransform`).
- **Process death and recovery:** `ProjectProcessDeathTest`,
  `ProjectAutosaveRecoveryTest`. **SAF:** `ProjectTransferTest`.
  **Lifecycle:** `EditorWorkspaceLifecycleTest`, `HomeFlowTest`.
- **Physical device only, never the emulator:** stylus, pressure, hover
  (`STYLUS-G1` is still BLOCKED on hardware), hardware GPU, 16 KB-page ARM64,
  and real-device performance — the CI emulator renders through SwiftShader.

### 6.5 Recorded aggregate failures, C1–C4

| Aggregate | Failing tests | Classification | Product defect? |
| --- | ---: | --- | --- |
| C1 (`36472045843`) | 7 | 2 `STALE_TEST_EXPECTATION`, 4 `TEST_OBSERVATION_SEAM_DEFECT`, 1 `TEST_ISOLATION_DEFECT` | no |
| C2 (`36494252771`) | 1 | `MirrorSmokeTest` isolation (inherited a Frozen Sculpt Mesh) | no |
| C3 (`36567175533`) | 1 | fixed-delay capture race in `SelectionOutlineVisualEvidenceTest` | no |
| C4 (`36607747079`) | 0 | `FULL_SHARDED_SUITE_PASS` | — |

### 6.6 Workflow split

- **Pre-merge, cheap:** Tier 0 + Tier 1 locally; `CI FAST` and one Tier 2
  `CI DEVICE` list in parallel on the task branch.
- **Rare release validation:** `CI FULL SHARDED` on the named triggers in
  `LEAN_VALIDATION_POLICY.md` §2.
- **OWNER material out of the pass/fail path:** the six visual/audit classes as
  their own on-demand dispatch whose artifact is `test-evidence/` — a stale frame
  then costs one re-dispatch rather than a 73-minute aggregate (C3 was exactly
  this). A tiering change, not a deletion.
- **Keep `Ui3dStateCorrectionTest` as the `CI DEVICE` default smoke.** It is 12
  tests of real `MotionEvent` input through the live camera in all three modes.
  It proves no CAD regeneration and no persistence; the Tier 2 lists do that.
- **Balancing shards by measured time buys nothing today**, because shards run
  one after another. It only pays together with a fan-out of the shards to
  parallel jobs sharing one set of APK bytes (`LEAN_VALIDATION_POLICY.md` §4).
