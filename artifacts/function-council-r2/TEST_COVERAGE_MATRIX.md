# Test coverage matrix (FUNCTION-COUNCIL-R2, seat I)

Audited tree: `70a6c14` (product tree `app/src/main` = `f3db953`). Every count
below was re-derived on this tree; nothing is copied from FUNCTION-COUNCIL-R1
(which counted 57 classes / 629 tests / 23 suites / 3757 checks on `a1b50f2`).

Evidence grades: **E1** = produced in this audit (fresh local run, fresh CI run,
or a seat-I scratch diagnostic); **E2** = source; **E3** = a recorded historic
run; **E4** = a document claim; **E5** = inference.

Fresh runs this matrix rests on (durable copies under
`artifacts/function-council-r2/evidence/`; run ids and results are as recorded
in `TESTS.md`, which also records each CI run's head SHA):

| Run | Tree | Result | Where |
| --- | --- | --- | --- |
| Host suites, local | `70a6c14` | `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 suites, 17 oracles (`CAD_R2_ORACLE_PASS=17`) | `evidence/local/host_selftests.txt` (E1) |
| JVM, local (`--rerun-tasks`) | `70a6c14` | 20 classes, 196 tests, 0 failures, 0 skipped | `evidence/local/gradle_unit_rerun_tail.txt` (BUILD SUCCESSFUL); per-class counts from `app/build/test-results/testDebugUnitTest/*.xml` (E1) |
| Release guard, local | `70a6c14` | `RELEASE_SELFTEST_GUARD=PASS` (first attempt in `run_status.txt` exited 2; the rerun file says PASS, its command and exit code not recorded) | `evidence/local/release_guard.txt`, `evidence/local/run_status.txt` (E1, supporting; `CI FAST` is the authoritative guard run) |
| Corpus verdicts, local | `70a6c14` | `CORPUS_FIXTURES 72`: 44 Ok, 28 refused | `evidence/local/host_corpus_verdicts.txt` (E1) |
| `CI FAST` `40796992679` | see `TESTS.md` (app, scripts, workflows, testdata identical to `70a6c14`) | success; corpus parity 72/72, guard PASS, TESTRUNTIME-01..26, THR1-01..10, DEV2/DEV3 PASS | `evidence/fast-run-40796992679/` (E1) |
| `CI DEVICE` `40797031704` (**FC2-D1**) | `70a6c14` | 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines; 5 classes (visual-evidence and legibility), 32/32 | `evidence/device-run-40797031704/` (E1, emulator) |
| `CI DEVICE` `40800569636` (**FC2-D2**) | see `TESTS.md` | 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines; 11 classes (Surface, Freeform, Sculpt, Import-sculpt, CAD vertical slice, extent, SketchUx), 77/77 | `evidence/device-run-40800569636/` (E1, emulator) |
| `CI DEVICE` `40805804385` (**FC2-D3**) | see `TESTS.md` | 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines; 12 classes (persistence, transfer, process death, objects, Mirror, dimensions, lifecycle, JNI hardening, diagnostics/renderer loss, settings), 90/90 | `evidence/device-run-40805804385/` (E1, emulator) |
| Seat-I scratch | `70a6c14` objects | cap probe, reverse-order run, Surface fixture attribution, release `.so` scan | session scratch `/tmp/claude-0/fc2-scratch/I/` — **no durable copy under `evidence/`**; results quoted from `SEAT_I.md` (E1 at the time, not re-checkable from this directory) |

All three device runs are emulator-only (API 36 x86_64, SwiftShader, AVD
`ForgeShape_CI_API36`, per each run's `summary.json`). They close no
physical-device gate (stylus, pressure, hover, hardware GPU, 16 KB pages,
real-device performance).

## 1. Inventory

| Layer | Count on this tree | Grade |
| --- | --- | --- |
| Native suites | 25 suites, 4392 checks (host = device per suite) | E1 |
| Self-test translation units | 35 `*selftest*.cpp`, all in the debug-only `FORGESHAPE_SELFTEST_SOURCES` CMake list | E2 |
| JVM | 20 classes, 196 tests | E1 |
| androidTest | 76 files; 70 classes with tests; **723 `@Test`**; 6 support files (`WorkspaceTestSupport`, `SketchTestSupport`, `ModelingOwnerRig`, `Ui3dAuditRecorder`, `GlbDocument`, `CadLeaderHudChecks`) | E2 |
| `.forge` fixtures | 72 in `testdata/forge/v1` (44 decode Ok, 28 refused: the 23 corrupt fixtures written since `CADB` v2, `cad_bad_plane_v1`, and 4 envelope fixtures) | E1 |
| CI workflows | `ci-fast.yml`, `ci-device.yml`, `ci-full-sharded.yml` | E2 |
| CI/guard scripts | `ci-device-smoke.sh`, `ci-release-selftest-guard.sh`, `host-native-selftests.sh`, `host-forge-corpus-verdicts.sh`, `run-instrumented-tests.ps1`, `instrumented-sharding.ps1`, `instrumented-runtime.ps1`, `test-instrumented-runtime.ps1`, `test-instrumented-sharding.ps1`, `verify-device-guards.ps1`, `build-forge-corpus.ps1` | E2 |

## 2. Native suites

Device cap = the result array `forgeshape_jni.cpp` hands the suite. Every suite's
`Recorder::check` drops a record silently once the array is full, so headroom
below 0 would make the device report `..._OK (cap checks)` while skipping the
rest (seat-I `cap_probe`: PROJECT at cap 200 records 200 and reports OK; E1).

| Suite (token) | Checks host / device | Device cap | Headroom | Check-ID families (count) | Domain | What it asserts | Gaps |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CAMERA | 119 / 119 | 128 (`jni:454`) | 9 | camproj01..14, orbit/pan/pinch/pitch | camera, projection | orbit/pan/zoom limits, ortho/persp transition, degenerate inputs | headroom 9 |
| PICKING | 185 / 185 | 256 | 71 | PICK_C2=11, camproj09/10, ray, tap, stylus/pressure/tilt | CPU picking, pointer semantics | ray/triangle incl. C2 double-precision seam, scene pick, pointer sanitising | stylus only as synthetic values |
| DYNAMIC_MESH | 91 / 91 | 128 | 37 | capacity, fixture, store, validate | mesh store | revisioned publish, capacity, coalescing | — |
| CONSTRUCTION_BOX | 100 / 100 | 128 | 28 | apply, reject, runtime | Construction box | parameter validation, winding, counts | — |
| CONSTRUCTION_TRANSFORM | 121 / 121 | 192 | 71 | model, inverse, rotate_*, reject | transform | `Model = T·Rz·Ry·Rx·S`, Euler bridge, scale refusals | — |
| CONSTRUCTION_PRIMITIVE | 126 / 126 | 256 | 130 | cylinder, plane, box, reject | primitives | counts, bounds, winding, round trip | — |
| CONSTRUCTION_SPHERE | 105 / 105 | 192 | 87 | sphere, typed | sphere | Euler characteristic, no degenerate triangles | — |
| CONE_CAPSULE | 163 / 163 | 256 | 93 | cone, capsule | cone, capsule | same family | — |
| SCULPT_BRUSH_KERNEL | 655 / 655 | 768 | 113 | FCM 93+15, SCUNDO 84, SCHNAV 39, s020r3 62, IMP01B 32, side01..09 | sculpt kernel, mask, sculpt history, navigator | seven brushes, mask exact ends, one stroke = one entry, caps, jump = repeated Undo | multi-touch half is device-only |
| RENDER_SHADING | 412 / 412 | 512 | 100 | seloutr1 71, r1c2 60, display 43, nor01..10 | shading, outline policy, display indices | normals, crease split, outline width/contrast/edge rule (CPU reference of `outline.frag`) | GPU passes only on device |
| SCENE | 175 / 175 | 256 | 81 | s17 79, obj018a 76, s027 20 | scene, object commands, isolate | delete/rename/hide/lock/duplicate, view restriction | — |
| CONSTRUCTION_HISTORY | 147 / 147 | 256 | 109 | IMP01B 33, creation, shape, transaction | Construction history | one act = one step, held bodies, no-op commits | — |
| GIZMO | 176 / 176 | 256 | 80 | uipref 21, overlay, scale, cancel | gizmo | handles, placement scale, stroke bundles, drag/cancel | — |
| PROJECT | 256 / 256 | **256** (`jni:821`) | **0** | FSR1A 120, IMP01A 51, IMP01B 36, CADA3 17, FSR1B 12, CADR0 11, OBJ018A 5, DAR1 4 | `.forge` codec | encode/decode, envelopes, golden digests, IMPT/SCUL/SCNE v2 | **at cap** (finding I-01) |
| RENDER_RECOVERY | 24 / 24 | 128 | 104 | FSR1B 24 | GPU-loss policy | bounded rebuilds, RestartRequired | — |
| GLTF_EXPORT | 93 / 93 | 128 | 35 | FSR1C 65, FSR1C_C1 28 | GLB export | bake `L`, `T` on node, refusals | — |
| GLTF_IMPORT | 189 / 189 | 256 | 67 | GLBIR0 85, GLBIR1 54, IMP01A 45, DAR1 5 | GLB import | subset accepted, every refusal by name | — |
| CAD | 155 / 155 | 256 | 101 | CADR0 122, CADEXT 33 | CAD R0, extent | profiles, extent modes, v1..v4 digests | — |
| CAD_A3 | 66 / 66 | 128 | 62 | CADA3 44, CADA3_BOOT 13, obj018a 9 | face-supported sketch, bootstrap | TopoRef, lineage, cycles, Home bootstrap | — |
| SKETCH_UX | 118 / 118 | 256 | 138 | CADUXR1 50, CADUXS1 40, CADFC1 13, CADUXS1C1 13 | sketch UX, arc/spline, extrude tool | view vs plane, dimension, anchors, feature view | — |
| BODY_DIMENSIONS | 102 / 102 | 256 | 154 | DIM020M 102 | dimensions | derived size, anchors, refusals | — |
| MIRROR | 62 / 62 | 128 | 66 | MIRROR01 62 | body Mirror | identity, one Undo | — |
| CAD_FEATURE | 638 / 638 | 1024 | 386 | CADVS* ~150, CADV6* ~70, DR_* 84, REV 31+9, PFS1 39, FILL 44, R2_* 74, R2_OR 17 oracles, MF 19, OSS 17, PAR 16, HUD3D 9, PICK_R2 9, C1_CH 6, PICK_C2 6 | regions, chain, Add/Cut, v6, Revolve, drafting, R2 retained sketch | kernel results measured, codec v5..v8, 17 `R2_OR` geometry oracles reported passing on host (`CAD_R2_ORACLE_PASS=17`); "the whole CAD domain passes" was judged overstated by the panel (`work/VERIFICATION.md`, seat C claim), so this is a host check count, not proof of independence or coverage | perf line truncated on device (I-07); oracle line absent on device (`CAD_R2_ORACLE_PASS` count 0) |
| FREEFORM | 74 / 74 | 256 | 182 | FF_01..22, FF_SESSION | Freeform cage | Catmull-Clark counts, crease, symmetry, FRFM v1 | — |
| SURFACE | 40 / 40 | 128 | 88 | SURF_01..24, SURF_AUTH_01..08 | Surface | patch/extrude/revolve/loft/trim/stitch/thicken and refusals, SURF v1 | 40 checks for seven kinds |

Ordering: the 25 suites in reverse order, with CAD_FEATURE and PROJECT run twice
more, give identical counts and 0 failures (seat-I `rev_order`, E1) — **with an
empty process scene only**: `forgeshape_primitive_selftest.cpp:122-128` reads
`constructionScene().hasProject()` and short-circuits, so a warm-restart run
can pass vacuously (untested, E2; seat I R2 concession 13).

## 3. JVM classes (196 tests, run in `CI FAST`)

| Class | Tests | Domain | What it asserts |
| --- | ---: | --- | --- |
| `AppPreferencesTest` | 13 | preferences | defaults, missing/unknown keys, clamp and non-finite rules, no handle style |
| `AppThemeTest` | 10 | palettes | five appearances, JNI index contract |
| `CadHud3dPresentationTest` | 6 | CAD HUD dock | dock read whole or not at all, homography on native corners, touch claim |
| `CadHudPresentationTest` | 22 | CAD HUD | value text precision, 48 dp floors independent of the band |
| `CadHudTouchAttributionTest` | 3 | debug attribution | token names the consuming surface, silent otherwise |
| `CadRevolvePresentationTest` | 8 | Revolve chrome | controls drawn from one state read, angle formatting, handle proxy |
| `ChromeMotionTest` | 8 | motion | durations, reduced motion = 0 |
| `DiagnosticLogTest` | 11 | diagnostics | bounded ring, no model data |
| `DisplaySettingsContractTest` | 6 | display indices | shading/surface/background indices |
| `EditorUiStateTest` | 10 | UI state | what the UI refuses to remember |
| `FeatureHistoryPresentationTest` | 9 | History | rows by durable id, failing row, glyphs |
| `FreeformPresentationTest` | 6 | Freeform chrome | tools drawn only when they can succeed |
| `LengthUnitTest` | 6 | units | exact mm/cm/m |
| `PointerSemanticsTest` | 6 | input | tool-type mapping |
| `ProjectDrawerPolicyTest` | 9 | project drawer | rows, order, close-first |
| `SketchBrowserPresentationTest` | 17 | CAD-R2 Sketches | acts offered as native bits say |
| `SketchChromePolicyTest` | 4 | sketch chrome | Ready withdraws rail/navigator/dimension |
| `SketchDraftingPresentationTest` | 20 | drafting chrome | palette entries, label stand-off and hiding |
| `SurfacePresentationTest` | 7 | Surface chrome | kinds drawn only when the candidate commits |
| `WorkspaceLayoutModeTest` | 15 | layout | breakpoints, dock decision |

## 4. androidTest classes (723 `@Test`)

Input path: **D** = through the window's decor root by `ModelingOwnerRig`
(`touchView` checks the control is the hit target at its centre,
`ModelingOwnerRig.java:119-149`); **V** = a `MotionEvent` dispatched straight to
the viewport view (`SketchTestSupport.tapViewport/dragSketch/tapSketch`), which
skips any chrome standing over the point; **P** = `View.performClick()`, which
skips hit-testing and occlusion; **N** = the act is a JNI call. Current-tree
evidence: **C2** = in `CI DEVICE` `40776968165`/`40782861799` on product
`f3db953` (126/126, E3); **FC2-D1** = this audit's `40797031704` (5 classes,
32/32, E1); **FC2-D2** = `40800569636` (11 classes, 77/77, E1); **FC2-D3** =
`40805804385` (12 classes, 90/90, E1); **older** = last device evidence
predates the CAD-R2 product changes. Per-class test counts below equal the
`testcase` counts in each run's `instrumentation-junit.xml`.

Split (recomputed from the tags below): **28** of the 70 classes have an FC2
run (5 + 11 + 12, 199 tests), **11** a C2 run, **31** rest on older runs.
`FEATURE_INVENTORY.md` §0 cites this split. A green FC2 run means the class
passed on the emulator on this tree; it does NOT upgrade a claim the panel
judged overstated (`work/VERIFICATION.md`: 10 of 12 strongest positive claims
overstated, typically because the test calls a native seam or `syncFromNative()`
rather than the real UI path) — the Input column says which path a class uses.

| Class | Tests | Domain / journey | Input | Asserts | Current-tree evidence |
| --- | ---: | --- | --- | --- | --- |
| CadRetainedSketchOwnerTest | 22 | CAD-R2 retained sketches, History, chooser D01..D22 | D, V | native state after real touches, 48 dp acts, rotation/background survival | C2 |
| EditorWorkspaceCorrectionTest | 37 | UI-R4B corrections, status hold | P | view state; 17.5 s of wall-clock status sleeps | C2 |
| OwnerProjectShellSketchSupportTest | 5 | mark + drawer, New Sketch on faces, pinch split | D, V | native face kind, one solid | C2 |
| CadMultiFaceOwnerTest | 6 | 24-cell fill selection, first Extrude | D, V | selection set, commit | C2 |
| CadFillPickR2Test | 6 | fill set toggle, shaft vs head tap | D, V | selection diff, depth, debug tap tokens | C2 |
| CadHud3dOwnerTest | 5 | action dock in world space | D, V | dock corners vs native, hide reasons | C2 |
| CadRevolveOwnerTest | 9 | Revolve New Body | D, V | angle, axis ref, commit | C2 |
| CadSketchDraftingOwnerTest | 14 | drafting palette | D | Trim/Extend/Offset/Mirror/dimensions | C2 |
| CadParametricHistoryOwnerTest | 3 | History edit of earlier step | D, V | rebuild, failing row | C2 |
| HomeFlowTest | 12 | Home, New Project, bootstrap, Open File, unsaved guard | V, P, N | no active-body read, empty history | C2 |
| EditorWorkspaceProjectActionsTest | 7 | project surface | P | refused Open costs nothing | C2 |
| Ui3dStateAuditTest | 9 | UI/3D audit ledger | V, P | records rather than asserts (javadoc :34-47); ledger 119 PASS + 68 PASS/3 CLAMPED/1 NOT_MEASURED | FC2-D1 |
| EditorWorkspaceLegibilityTest | 20 | trailing cluster, contrast, gizmo touch contract | P | measured bounds | FC2-D1 |
| SketchUxVisualEvidenceTest | 1 | OWNER frames | V | frames + facts; fixed 400 ms before capture | FC2-D1 |
| UiPrefVisualEvidenceTest | 1 | OWNER frames, palettes | P | 20 frames; fixed 400 ms | FC2-D1 |
| SelectionOutlineVisualEvidenceTest | 1 | OWNER frames, outline | V, P | waits presented frames; band pixel counts | FC2-D1 |
| SpatialSketchTest | 8 | support chooser, hover, dependents | V | native kinds; latent order dependence (E3) | older |
| SketchUxTest | 14 | navigator, dimension, arc/spline, Edit Sketch | V | native sketch state bits only (`:199-233`); does NOT cover the navigator Flip/±90° camera (C-01 = F-02, `COUNCIL_DEBT_RECHECK.md` D2) | FC2-D2 |
| SketchExtrudeTest | 9 | sketch to body | V, P | body is CAD | older |
| CadVerticalSliceTest | 9 | regions, Add/Cut, HUD | V, P | volume/shells from regenerated solid | FC2-D2 |
| CadCanvasExtrudeTest | 11 | arrow drag | V, MotionEvent | frozen basis, cancel | older |
| CadExtrudeExtentTest | 5 | extent selector, Flip | V, P | presence/absence | FC2-D2 |
| CadPlanarFaceRuntimeTest | 6 | planar faces J1..J6 | V | native faces | older |
| CadPlanarFaceOwnerCorrectionTest | 5 | every cell by window tap | window MotionEvent | cells | older |
| CadA3VisualEvidenceTest | 1 | OWNER frames | V | fixed 400 ms | older |
| Ui3dStateCorrectionTest | 12 | anchored chrome follows projection (CI DEVICE default) | MotionEvent | anchor error | older |
| Ui3dDimensionVisibilityTest | 2 | dimension pixels | V | pixels after fixed 500 ms | older |
| SurfaceOwnerTest | 3 | Surface journey | D, P | native feature list, Undo | FC2-D2 |
| FreeformOwnerTest | 3 | Freeform journey | D | cage ids, Undo | FC2-D2 |
| EditorWorkspaceGizmoTest | 46 | gizmo drags, refusals | MotionEvent, P | transform written | older |
| EditorWorkspaceHistoryTest | 20 | Undo/Redo from chrome | P, N | depths | older |
| EditorWorkspaceControlsTest | 23 | control ids | P | ids act; leaves a frozen mesh (E3 C3) | older |
| EditorWorkspaceDisplayTest | 17 | display popover | P | presentation-only | older |
| EditorWorkspaceThemeTest | 19 | palettes on a real Activity | P | resolved colours, bars | older |
| EditorWorkspaceUnifiedRightHostTest | 18 | right host geometry | P | rects; `sleep(1700)` | older |
| EditorWorkspaceMobileTest | 15 | absent at rest | P | visibility | older |
| EditorWorkspacePointerTest | 13 | tool type/pressure/tilt into JNI | MotionEvent | `debugLastPointerEvent` | older |
| EditorWorkspaceArchitectureTest | 12 | host ownership | P | ids | older |
| EditorWorkspaceChromeCompositionTest | 12 | composition | P | labels, corners | older |
| EditorWorkspaceRightHostPlacementTest | 12 | trailing edge | P | rects | older |
| EditorWorkspaceCompositionTest | 10 | role separation | P | — | older |
| EditorWorkspaceLayoutTest | 10 | viewport floor | P | fractions | older |
| EditorWorkspaceMotionTest | 10 | resting states | P | — | older |
| EditorWorkspaceObjectsTest | 10 | rows by id, viewport pick | MotionEvent | no reset by design | older |
| EditorWorkspaceFoundationTest | 8 | rail defects | P, MotionEvent | — | older |
| EditorWorkspaceGestureTest | 6 | gesture ownership, IME | MotionEvent to chrome | consumption | older |
| EditorWorkspaceLifecycleTest | 3 | HOME/resume | lifecycle | shell rebuilt from native | FC2-D3 |
| EditorWorkspaceSculptRetentionTest | 2 | Start/Back/Resume | P | no sculpt loss | older |
| SculptUndoTest | 10 | one stroke = one entry | MotionEvent | depths | FC2-D2 |
| SculptBrushStage025Test | 8 | Flatten/Crease/Mask | MotionEvent | vertices | FC2-D2 |
| SculptHistoryNavigatorTest | 9 | navigator jump | P | cursor | FC2-D2 |
| Stage027SculptWorkflowTest | 4 | Isolate/Hide | V | pixels after fixed 600 ms | FC2-D2 |
| ImportedMeshSculptTest | 9 | sculpt over imported | P | arrays immutable | FC2-D2 |
| ImportGlbSculptGuardTest | 3 | Import not a Sculpt act | P, N | withdrawn + refused | FC2-D2 |
| ImportedMeshDurableTest | 12 | durable import, IMPT | N, P | one Undo, round trip, 7 packaged fixtures load | FC2-D3 |
| GlbExportTest | 24 | export | N | independent `GlbDocument` reader | older |
| GlbImportPreviewTest | 23 | diagnostic preview (no user control) | N | — | older |
| GlbImportExternalR1Test | 14 | diagnostic preview subset | N | — | older |
| ProjectAutosaveRecoveryTest | 14 | autosave, recovery | N, P | `awaitIdle`, no sleeps | FC2-D3 |
| ProjectProcessDeathTest | 8 | real process death | lifecycle | recovery | FC2-D3 |
| ProjectTransferTest | 13 | SAF `Uri` boundary | N | nothing of storage reaches the document | FC2-D3 |
| ObjectsDeleteTest | 8 | Delete | MotionEvent, P | one Undo, last-body refusal | FC2-D3 |
| ObjectCommandsTest | 1 | rename/hide/lock/duplicate | P | — | FC2-D3 |
| MirrorSmokeTest | 1 | Mirror | P | — | FC2-D3 |
| BodyDimensionsSmokeTest | 1 | Dimensions | P | native placement | FC2-D3 |
| SelectionOutlineTest | 19 | outline passes | V, P | renderer counters | older |
| RendererCounterContinuityTest | 1 | counters across restart | lifecycle | — | older |
| DiagnosticsAndRendererLossTest | 9 | injected device loss | N | `assumeTrue` can skip e2er1b07 (:245); in `40805804385` the junit has 0 `<skipped/>` | FC2-D3 |
| JniBoundaryHardeningTest | 5 | JNI misuse | N, MotionEvent | `debugActiveBodyMisuseCount` | FC2-D3 |
| SettingsPreferencesTest | 15 | Settings page | P | byte-identical project | FC2-D3 |

Totals by input path (call sites, E2): `performClick()` 482 in 67 files;
viewport-direct helpers 170 in 26 classes; decor-root rig acts and gestures 112 in
9 classes.

## 5. Fixtures

| Family | Count | Pinned by | Parity | Attribution of corrupt ones |
| --- | ---: | --- | --- | --- |
| Envelope (`corrupt_crc`, `truncated`, `unknown_*`, `unsupported_major`) + Construction/Sculpt/Imported | 12 | PROJECT (FSR1A, IMP01A/B) | FAST 72/72 (E1) | 4 refused, distinct statuses (ChecksumMismatch, Truncated, UnknownRequiredSection, UnsupportedMajor) (E1). Refusal total 21 + 1 + 1 + 1 + 4 = 28; 22 of the 28 are `InvalidSemanticValue` |
| `SCNE` v2 | 2 | PROJECT (OBJ018A) | E1 | 1 refused, BadPayload (E1) |
| `CADB` v1..v8 (incl. `mixed_cad*`) | 52 | CAD, CAD_A3, CAD_FEATURE (hex literals) | E1 | 21 refused: 20 `InvalidSemanticValue`, 1 `UnresolvedReference` (`cad_dependency_cycle_v2`). These are codec status CLASSES, not per-fixture expected names — no expected-verdict file exists, and "refused by name" claims (seat C 39 CAD fixtures, seat E 28/28) were judged overstated (`work/VERIFICATION.md`); the four sampled CADB pairs (two_sides, bad_plane, revolve bad_axis, bad_operation) differ from their parent by 5–8 bytes (seat-I `cmp`, E1); the rest not sampled |
| `FRFM` v1 | 3 | FF_20 | E1 | 1 refused, `InvalidSemanticValue`; 6-byte delta (E1) |
| `SURF` v1 | 3 | SURF_22 | E1 | 1 refused, `InvalidSemanticValue`; parent + Trim naming feature 7 → `UnknownFeature`; the same Trim naming feature 1 → Ok (seat-I `surf_attr`, E1) |

## 6. Domain × layer summary

Every one of the 70 androidTest classes in §4 is counted in exactly one row
(column totals: C2 11, FC2 28, older 31; JVM 20). Host checks sum to 4392.

| Domain | Host | JVM | Device C2 (E3, product `f3db953`) | Device FC2 (E1, this tree, emulator) | Device older | Physical |
| --- | --- | --- | --- | --- | --- | --- |
| CAD sketch/feature/R2 | 977 checks (CAD, CAD_A3, SKETCH_UX, CAD_FEATURE) | 8 | 8 (CadRetainedSketchOwner, OwnerProjectShellSketchSupport, CadMultiFaceOwner, CadFillPickR2, CadHud3dOwner, CadRevolveOwner, CadSketchDraftingOwner, CadParametricHistoryOwner) | 4 (D2: SketchUx, CadVerticalSlice, CadExtrudeExtent; D1: SketchUxVisualEvidence) | 7 (SpatialSketch, SketchExtrude, CadCanvasExtrude, CadPlanarFaceRuntime, CadPlanarFaceOwnerCorrection, CadA3VisualEvidence, Ui3dDimensionVisibility) | OWNER-PHYSICAL-UNVERIFIED |
| Home / project shell / lifecycle | within CAD_A3 (`CADA3_BOOT` 13) | 2 (ProjectDrawerPolicy, EditorUiState) | 2 (HomeFlow, EditorWorkspaceProjectActions) | 1 (D3: EditorWorkspaceLifecycle) | — | — |
| Sculpt (incl. sculpt over Imported Mesh) | 655 | — | — | 6 (D2: SculptUndo, SculptBrushStage025, SculptHistoryNavigator, Stage027SculptWorkflow, ImportedMeshSculpt, ImportGlbSculptGuard) | 1 (EditorWorkspaceSculptRetention) | stylus/pressure OWNER-PHYSICAL-UNVERIFIED |
| Construction/objects/gizmo/camera/picking | 1672 | 2 (PointerSemantics, LengthUnit) | — | 5 (D3: ObjectsDelete, ObjectCommands, MirrorSmoke, BodyDimensionsSmoke, JniBoundaryHardening) | 6 (EditorWorkspaceGizmo, …History, …Controls, …Pointer, …Objects, …Gesture) | — |
| Persistence (`.forge`, autosave, transfer, process death) | 256 + 72 fixtures | — | — | 3 (D3: ProjectAutosaveRecovery, ProjectProcessDeath, ProjectTransfer) | — | — |
| Import/export | 282 | — | — | 1 (D3: ImportedMeshDurable) | 3 (GlbExport, GlbImportPreview, GlbImportExternalR1) | — |
| Renderer / diagnostics | 436 | 2 (DisplaySettingsContract, DiagnosticLog) | — | 2 (D1: SelectionOutlineVisualEvidence; D3: DiagnosticsAndRendererLoss) | 2 (SelectionOutline, RendererCounterContinuity) | hardware GPU OWNER-PHYSICAL-UNVERIFIED (SwiftShader only) |
| Freeform | 74 | 1 | — | 1 (D2: FreeformOwner, 3 tests) | — | — |
| Surface | 40 | 1 | — | 1 (D2: SurfaceOwner, 3 tests) | — | — |
| Settings / preferences / theme | within GIZMO (`uipref` 21) | 2 (AppPreferences, AppTheme) | — | 2 (D1: UiPrefVisualEvidence; D3: SettingsPreferences) | 1 (EditorWorkspaceTheme) | OWNER visual |
| Chrome/layout | — | 2 (ChromeMotion, WorkspaceLayoutMode) | 1 (EditorWorkspaceCorrection) | 2 (D1: Ui3dStateAudit, EditorWorkspaceLegibility) | 11 (Ui3dStateCorrection, EditorWorkspace Display, UnifiedRightHost, Mobile, Architecture, ChromeComposition, RightHostPlacement, Composition, Layout, Motion, Foundation) | OWNER visual |
| Release binary | guard only | — | never executed | never executed | never executed | — |

No full-sharded aggregate (`FULL_SHARDED_SUITE_PASS`) exists on this tree; the
31 `older` classes are the gap.

## 7. False-green register

| Id | Mechanism | Status on this tree | Grade |
| --- | --- | --- | --- |
| FG-1 | Device `Recorder` drops checks past the cap and still prints OK | PROJECT exactly at 256/256 | E1 |
| FG-2 | CI DEVICE checks token presence/order, never per-suite counts | no expected counts anywhere | E2 |
| FG-3 | `performClick` and viewport-direct taps cannot see chrome occlusion | 652 call sites | E2 |
| FG-4 | Audit ledger rows are not in `summary.json` | ledger clean in FC2-D1 | E1 |
| FG-5 | Ignored/assumption statuses are `<skipped/>`, not failures | one `assumeTrue` site; it did not fire in FC2-D3 (0 `<skipped/>` in `evidence/device-run-40805804385/instrumentation-junit.xml`) | E1 for that run, E2 otherwise |
| FG-6 | Fixed delays before pixel asserts can read a stale frame | 5 capture sites | E2 |
| FG-7 | Generic codec status cannot say WHY a fixture is refused | mitigated by minimal-delta construction | E1 |
| FG-8 | Device perf line truncated at 1023 chars | CAD-R2 keys lost on device | E1 |
