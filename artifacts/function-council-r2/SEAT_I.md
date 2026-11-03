# Seat I — Test / CI / evidence / build integrity

FUNCTION-COUNCIL-R2. Audited tree `70a6c14` (product tree `f3db953`). Research
only: nothing under `app/`, `testdata/`, `scripts/`, `.github/`, Gradle files or
the owner documents was modified. Scratch diagnostics live in
`/tmp/claude-0/fc2-scratch/I/` and are not part of any build.

Companion files: `TEST_COVERAGE_MATRIX.md`, `VALIDATION_PLAN.md`,
`work/req_I.tsv` (75 rows, TST-001..TST-075).

## Scope

Every host suite, JVM class, androidTest class and golden fixture; the three CI
workflows and the scripts they call; the release self-test guard; debug/release
differences; false-green mechanisms; stale counts in docs; re-evaluation of
FUNCTION-COUNCIL-R1's `TEST_COST_AUDIT.md` and `LEAN_VALIDATION_POLICY.md` on
this tree.

## Sources read

- `artifacts/function-council-r2/BEFORE.md` (`9cfc3f5`).
- `CLAUDE.md`, `README.md`, `PROJECT_STATUS.md` (head, Self-test suite
  :3079-3128, Android UI suites :3362-3523, Known Issues, Next Stage :4851),
  `DATA_PACKAGE_SPEC.md` :1740-1816, `docs/CI_CLOUD.md` — all at `70a6c14`.
- `.github/workflows/ci-fast.yml`, `ci-device.yml`, `ci-full-sharded.yml`;
  `scripts/ci-device-smoke.sh`, `ci-release-selftest-guard.sh`,
  `host-native-selftests.sh`, `host-forge-corpus-verdicts.sh`.
- `artifacts/function-council-r1/TEST_COST_AUDIT.md`, `LEAN_VALIDATION_POLICY.md`
  (against `a1b50f2`; re-checked, not reused).
- `artifacts/cad-r2-c2-picking-seam/TESTS.md`, `SUMMARY.md`;
  `artifacts/cad-r2-c1-quality-gate/TESTS.md`;
  `artifacts/cad-r2-retained-sketch-parametric-ux/TESTS.md`, `SUMMARY.md`.
- `artifacts/function-council-r2/SEAT_H.md` (H-03, H-13 cross-referenced).
- Fresh outputs: scratchpad `fc2/host_selftests.txt`, `fc2/release_guard.txt`,
  `fc2/run_status.txt`, `fc2/gradle.txt`, `fc2/gradle_unit_rerun.txt`,
  `fc2/host_corpus_verdicts.txt`, `fc2/ci/fast` (`CI FAST` `40796992679`),
  `fc2/ci/dev1` (`CI DEVICE` `40797031704`); `app/build/test-results/testDebugUnitTest/*.xml`.

## Inspected code paths

- `forgeshape_jni.cpp:2083-2126` `NativeViewport_start` — launches the render
  thread and runs the 25 `run*SelfTestsAndLog` on the CALLING thread under
  `g_viewport.mutex`; re-entry guard is only `g_viewport.thread.joinable()`.
- `ForgeShapeActivity.java:25-45` (`onCreate` → `NativeViewport.start()`, comment
  "Started once per PROCESS"), `:570-584` (`onDestroy` → `NativeViewport.stop()`
  unless `isChangingConfigurations()`).
- `forgeshape_jni.cpp:452-1060` the 25 runners and their caps (`kMax*Checks`);
  `:608` the CAD_FEATURE performance `FS_LOGI`.
- `Recorder::check` in each `*_selftest.cpp`, e.g.
  `forgeshape_project_selftest.cpp:21-32` (`if (n < max)`; no overflow flag).
- `forgeshape_jni.cpp:9074-9086`, `:7967-7972`, `:2267-2271`, `:7914-7924` —
  release stubs of the debug seams.
- `app/src/main/cpp/CMakeLists.txt:118-160` — `FORGESHAPE_SELFTEST_SOURCES`
  (all 35 `*selftest*.cpp` present; no product `.cpp` missing).
- `scripts/ci-device-smoke.sh` — token list, `FAIL_PATTERN`, `capture_startup`,
  settle loop :293-301, status-code parser :430-470.
- `WorkspaceTestSupport.java:72-118` (`resetToBaselineConstruction`), `:417-421`
  (`settleLayout`), `:480-515` (`dragConsumed`/`send`);
  `SketchTestSupport.java:31-125` (`tapTapWorld`, `tapViewport`, `dragSketch`);
  `ModelingOwnerRig.java:94-160, 210-255` (`press`, `touchView`, `realGesture`).
- `forgeshape_surface.h:332-361`, `forgeshape_project_document.h:290-311, 580`.

## Test coverage of the domain

See `TEST_COVERAGE_MATRIX.md` for the full tables. In short:

- **Host suites (E1).** 25 suites, 4392 checks, 0 failed; per-suite counts on
  the device identical (`CI DEVICE` `40797031704`). Check-ID families per suite
  extracted by seat-I `names_dump` (e.g. PROJECT = FSR1A 120, IMP01A 51, IMP01B
  36, CADA3 17, FSR1B 12, CADR0 11, OBJ018A 5, DAR1 4; CAD_FEATURE includes 17
  `R2_OR` independent geometry oracles). Golden digests are ASSERTED against hex
  literals (e.g. `FF_20_the_frfm_corpus_matches_the_independent_builder`,
  `SURF_22_the_surf_corpus_matches_the_independent_builder`,
  `CADEXT_10_c..h`), not only printed.
- **JVM (E1).** 20 classes / 196 tests / 0 failures: presentation and policy
  rules with no Android types (HUD, drafting chrome, History, drawer, Surface,
  Freeform, preferences, theme, layout, units, pointer map).
- **androidTest (E2 counts).** 70 classes / 723 `@Test`. What they assert:
  native truth read back over JNI after a UI act; measured view bounds; resolved
  colours; renderer counters; pixels for a few visual cases; debug tap tokens
  read back from logcat in 6 CAD classes. Input reaches the app by
  `performClick()` (482 sites), viewport-direct `MotionEvent` (170 sites) or the
  decor-root rig with an occlusion check (112 sites).
- **Fixtures (E1).** 72; CI FAST parity 72/72; host verdicts 44 Ok / 28 refused,
  identical to the last recorded capture.

## Confirmed findings

### I-01 — P2 — A device suite silently drops checks past its result cap; PROJECT is at 256/256

- **Where.** `forgeshape_project_selftest.cpp:21-32` (`Recorder::check` records
  only `if (n < max)`), the same pattern in all 25 suites;
  `forgeshape_jni.cpp:821` `kMaxProjectChecks = 256`.
- **Repro (E1).** `/tmp/claude-0/fc2-scratch/I/cap_probe.cpp`, linked against
  the host objects of this tree:
  `PROJECT cap=8192 recorded=256`, `cap=256 recorded=256`,
  `cap=255 recorded=255 failed=0 verdict=OK`, `cap=200 recorded=200 verdict=OK`.
  The device reports `FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)`. CAMERA is
  119/128.
- **Impact (latent).** PROJECT has 0 headroom, but no check is lost on this
  tree: the host harness (cap 8192) also records exactly 256. The defect is
  that an overflow would be SILENT: the next PROJECT check would run on the
  host and be skipped on the device — the only CI path that executes native checks (I-05).
  Precedent: CAD_FEATURE lost 49 checks at cap 512
  (`artifacts/cad-r2-retained-sketch-parametric-ux/TESTS.md:76`, E3).
  Other suites close to their caps (host count / device cap, E1/E2): CAMERA
  119/128, CONSTRUCTION_BOX 100/128 (`forgeshape_jni.cpp:923`), DYNAMIC_MESH
  91/128 (`:902`), MIRROR 62/128 (`:567`).
- **Verdict.** DEFECT-CONFIRMED (test infrastructure). Same fact as SEAT_H H-13.
- **Fix.** Make `count == cap` a suite failure; raise PROJECT and CAMERA caps.

### I-02 — P2 — The 25 suites re-run on every Activity start, ~8.4 s each on the CI emulator, once per instrumented test (PARTIAL + DOC-DIVERGENCE; revised in round 2)

- **Where.** `forgeshape_jni.cpp:2083-2126` (suites run synchronously in
  `start()` whenever the render thread is not joinable);
  `ForgeShapeActivity.java:582` stops it on every non-configuration destroy;
  the comment at `ForgeShapeActivity.java:33-36` says "Started once per PROCESS".
- **Repro (E1).** `CI DEVICE` `40797031704`, `test-logcat.txt`: 32
  `native viewport started` lines and 32 full suite runs for 32 tests; each run
  8.9–9.9 s (sum 304.4 s of the 667 s test time); `ActivityTaskManager:
  Displayed ... +9s848ms / +9s480ms / +9s816ms`. Startup capture: suites begin
  `01:01:49.526`, MIRROR OK `49.975`, CAD_FEATURE OK `57.492` (7.5 s),
  SURFACE OK `57.941`.
- **Cause (revised).** The per-start re-run is not new: the same
  `NativeViewport.stop()` stood at `fa0b6b4` (`ForgeShapeActivity.java:581-582`),
  whose 629-test aggregate took 65.65 min. It is the MULTIPLIER; the growth is
  suite time — CAD_FEATURE alone takes 7.5 s of the ~8.4 s on the device
  (141 checks in the old status table, 638 now).
- **Impact.** About 46 % of device test time in this run is self-test
  re-execution (304.4 s of 667 s). The 32 tests are CAD owner classes, so their
  20.8 s per test is not representative. Projection (E5, assumes every test
  cold-starts the Activity, unverified for the whole inventory): suite
  re-execution alone is 723 × 9.51 s (304.4/32) ≈ 6 877 s ≈ 114.6 min, 95 % of
  the 120-min hard stop; the aggregate then exceeds it if the non-suite time
  over all 723 tests exceeds ≈ 5.4 min (≈ 0.45 s per test). In this run the
  non-suite time was 362.6 s / 32 ≈ 11.3 s per test. In a debug build, any
  cold Activity creation in a live process blocks the main thread ~9 s. Release
  is unaffected (suites compiled out).
- **Verdict.** PARTIAL (test cost, debug-path latency). CLAUDE.md requires only
  "run once from `NativeViewport.start()` … never per frame", and the code meets
  that. DOC-DIVERGENCE for the `ForgeShapeActivity.java:33-36` comment ("Started
  once per PROCESS"). Consistent with `TST-007`.
- **Fix.** A process-static "suites ran" flag checked in `start()`; optionally
  move CAD_FEATURE's performance and oracle blocks host-only.

### I-03 — P2 — A task-branch push runs no CI, contrary to CLAUDE.md and docs/CI_CLOUD.md

- **Where.** `ci-fast.yml:12-17`, `ci-device.yml:10-15` (`push: [main,
  infra/ci-cloud-r1]`, `pull_request: [main]`, `workflow_dispatch`);
  `CLAUDE.md` Hard rules ("push the task branch and let GitHub Actions test it");
  `docs/CI_CLOUD.md` "The flow" step 2.
- **Repro (E2/E1).** Trigger lists above; this audit's FAST run reports
  `event=workflow_dispatch` (`toolchain-manifest.txt`).
- **Impact.** An undispatched task branch with no open PR to `main` has no CI
  evidence while the docs say it does; a task branch with an open PR to `main`
  IS tested (`pull_request: [main]`). `docs/CI_CLOUD.md` is inconsistent with
  itself: `:12` "Push it; GitHub Actions tests it" against its own table at
  `:22-23` ("every push to `main`, every PR to `main`"), which is correct. Re-confirms FC-R1 `TEST_COST_AUDIT.md` §6.3a, still unresolved.
- **Verdict.** DOC-DIVERGENCE.
- **Fix.** OWNER chooses: add task-branch globs to `push`, or change both texts
  to "dispatch".

### I-04 — P3 — Stale counts in six places

- **Repro (E1/E2).**
  - `docs/CI_CLOUD.md`: "all 57 fixtures" (72), "all **23** `*_SELFTEST_OK`
    tokens" and "the 23 startup tokens" (25).
  - `README.md:194`: "Seven of the fifty-seven fixtures" (72 committed).
  - `scripts/ci-device-smoke.sh:57`: "The twenty-four startup suites" above a
    25-token list.
  - `.github/workflows/ci-device.yml:2`: header comment "(23 self-test
    tokens…)" (25) — added in round 2 (seat B).
  - `PROJECT_STATUS.md` Self-test suite (:3079-3128): "Twenty-three ... 3757
    checks"; PICKING 174 (185), SKETCH_UX 105 (118), CAD_FEATURE 141 (638); no
    FREEFORM/SURFACE rows — while the file's head says 4392.
  - `PROJECT_STATUS.md` Android UI suites: 4 count mismatches
    (AppPreferencesTest 10→13, CadCanvasExtrudeTest 9→11,
    EditorWorkspaceCorrectionTest 36→37, LengthUnitTest 5→6) and 23 classes
    absent (15 androidTest incl. `CadRetainedSketchOwnerTest`,
    `FreeformOwnerTest`, `SurfaceOwnerTest`; 8 JVM).
- **Verified correct (E1).** CLAUDE.md's and README's 25-token lists equal the
  device emission order; "seventy-two" equals the committed corpus.
- **Verdict.** DOC-DIVERGENCE.
- **Fix.** Regenerate the numbers from host output and `@Test` counts.

### I-05 — P2 — CI FAST does not run the native suites or the corpus-verdict diff

- **Where.** `ci-fast.yml:122-206` — Gradle, release guard, corpus parity,
  runner/guard checks; no `host-native-selftests.sh` or
  `host-forge-corpus-verdicts.sh` step.
- **Impact.** 4392 native checks reach CI only through a dispatched `CI DEVICE`
  debug startup, behind the device cap of I-01. FC-R1 found the same; still true.
- **Evidence.** E2. **Verdict.** PARTIAL (gap).
- **Fix.** Add both scripts to FAST (g++ on the runner; no new framework).

### I-06 — P2 — No exhaustive aggregate covers the current tree; 54 of 70 device classes have no run on the current product tree

- **Repro.** Last `FULL_SHARDED_SUITE_PASS`: `36607747079` on `fa0b6b4`, 57
  classes / 629 tests (`PROJECT_STATUS.md:420-430`, E3). On product `f3db953`:
  11 classes in C2's `40776968165` + `40782861799` (126/126, E3) and 5 in this
  audit's `40797031704` (32/32, E1). The other 54 — including
  `Ui3dStateCorrectionTest` (the CI DEVICE default), persistence, sculpt, gizmo,
  import/export, `FreeformOwnerTest`, `SurfaceOwnerTest` — last ran on older
  product trees (48 product commits since `main`).
- **Requirement (revised).** The source is the Tier-5 gate before merging to
  `main` (`PROJECT_STATUS.md` Next Stage :4851-4865, Tier-5 policy :2545), H1 —
  not CLAUDE.md's documentation rule.
- **Construction-domain classes with no run on `f3db953`:**
  `EditorWorkspaceGizmoTest` (46), `EditorWorkspaceHistoryTest` (20),
  `EditorWorkspaceObjectsTest` (10), `ObjectsDeleteTest` (8),
  `ObjectCommandsTest`, `MirrorSmokeTest`, `BodyDimensionsSmokeTest` (1 each).
- **Verdict.** PARTIAL. Not a defect: the PROJECT_STATUS records state focused
  unions honestly, and the Tier-5 run before the v6→main merge is still pending
  (BLOCKED-OWNER).
- **Fix.** Tier-2 subsystem lists per change; Tier 5 at the OWNER gate after I-02.

### I-07 — P3 — The device `FORGESHAPE_CAD_FEATURE_PERFORMANCE` line is truncated

- **Where.** `forgeshape_jni.cpp:608`.
- **Repro (E1).** In `40797031704` the line ends `parametric_ch`;
  `grep -c CAD_R2_ORACLE_PASS startup-forgeshape.txt` = 0; the host line is
  complete (`CAD_R2_ORACLE_PASS=17`).
- **Verdict.** DEFECT-CONFIRMED. Same fact as SEAT_H H-03.
- **Fix.** Several lines < 1000 chars.

### I-08 — P2 — Most device "touches" cannot detect chrome standing over the target

- **Where.** `View.performClick()` — 482 call sites in 67 files;
  `SketchTestSupport.tapViewport/dragSketch/tapSketch` dispatch to the viewport
  view itself (`SketchTestSupport.java:79-121`) — 170 call sites in 26 classes.
  Decor-root, hit-checked input exists in 8 test classes (corrected in round 2):
  4 through `ModelingOwnerRig` (`touchView`/`touchId` check the hit target,
  `:119-149`; `realGesture`/`realDrag` enter at the decor root, `:215-257`) —
  `CadParametricHistoryOwnerTest`, `CadRetainedSketchOwnerTest`,
  `FreeformOwnerTest`, `SurfaceOwnerTest` — and 4 with their own decor-root
  `realGesture` — `CadFillPickR2Test`, `CadMultiFaceOwnerTest`,
  `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`. About 106 call sites by
  re-count. The rig's `press()` is `performClick()` and is unchecked.
  `EditorWorkspaceGizmoTest` dispatches drags to the viewport view (`:2126`,
  `:2138`).
- **Impact.** The CLAUDE.md rule "a surface may never partially cover another
  live control" and "the nearest chooser target owns a tap" are only provable by
  those 8 classes. No gizmo test could detect chrome standing over a gizmo
  handle. The Edit Sketch chip taking a face tap
  (`MODELING-R1-OWNER-CORRECTION`) and the font-1.5 History overlap (`CAD-R2-C1`)
  are the defect class a viewport-direct tap cannot see (E2).
- **Verdict.** PARTIAL (coverage gap, not a product defect).
- **Fix.** Route the SketchTestSupport helpers through the decor root and assert
  the hit target; changing the harness is a Tier-5 trigger.

### I-09 — P3 — Fixed sleeps remain where FC-R1 found them

- **Where (E2).** `settleLayout` = settle + `sleep(250)` + settle
  (`WorkspaceTestSupport.java:417-421`), 884 call sites; 49 raw sleep sites;
  `EditorWorkspaceCorrectionTest:358,363,372,401` 17.5 s on `STATUS_HOLD_MS`;
  fixed delays before capture/pixel reads at `CadA3VisualEvidenceTest:310`,
  `SketchUxVisualEvidenceTest:395`, `UiPrefVisualEvidenceTest:406`,
  `Stage027SculptWorkflowTest:880`, `Ui3dDimensionVisibilityTest:472`.
- **Positive.** Autosave tests have no sleeps (`ProjectAutosaveRecoveryTest`
  only the javadoc at :31; `ProjectProcessDeathTest` 0).
- **Verdict.** PARTIAL (risk).

### I-10 — P3 — The release APK is never executed by any CI tier

- **Where.** The androidTest APK targets debug only; the release guard
  (`ci-release-selftest-guard.sh`) proves ABSENCE of self-test symbols/strings.
  13 `debug*` JNI exports remain in the release `.so` (seat-I `llvm-readelf`),
  each `#ifdef NDEBUG`-stubbed in source (four inspected).
- **Verdict.** IMPLEMENTED-UNTESTED (release runtime).
- **Fix.** Optional: a release-variant launch smoke as a dispatch input (needs a
  signing decision; not a framework).

### I-11 — P3 — `EditorWorkspaceLegibilityTest` leaks `animator_duration_scale 0` on failure

- **Where.** Set at `:783`, restored at `:798` in the test body; `@After`
  (`:86-96`) restores font scale and orientation only.
- **Evidence.** E2 (FC-R1 §6.3.7, still present).
- **Verdict.** DEFECT-CONFIRMED (test hygiene).
- **Fix.** Restore in `@After`.

### I-12 — see Hypotheses (H-I-1).

### I-13 — P3 — A green `summary.json` does not carry the audit ledger's verdicts

- **Where.** `Ui3dStateAuditTest` records rather than asserts
  (javadoc :34-47); `ci-device-smoke.sh` reports only JUnit pass/fail.
- **Repro (E1).** In `40797031704` the ledger is clean (`VISIBILITY.tsv` 119
  PASS; `SPATIAL_ATTACHMENTS.tsv` 68 PASS, 3 CLAMPED, 1 NOT_MEASURED), so no
  false green occurred in this run.
- **Verdict.** PARTIAL (risk).
- **Fix.** Copy the ledger verdict counts into `summary.json`.

### I-14 — INFO — No CI step compares per-suite check counts

- **Where.** `ci-device-smoke.sh` checks token presence and order only. A suite
  that loses checks (an early `return r.n`, a deleted block) stays green.
- **Evidence.** E2. **Verdict.** PARTIAL.
- **Fix.** Committed expected-count table; fail on any decrease.

## Hypotheses (unproven)

- **H-I-1 (was I-12).** An assumption-failed test can be reported as passing.
  `ci-device-smoke.sh` maps statuses -3/-4 to `<skipped/>` and requires
  `OK (n tests)`; `DiagnosticsAndRendererLossTest#e2er1b07` uses `assumeTrue`
  (:245). Whether AndroidJUnitRunner prints `OK (n)` with n including the skip
  was not reproduced here. E5.
- **H-I-2.** In a debug build, a cold Activity creation with input pending could
  ANR during the ~9 s synchronous self-test run (I-02). Not reproduced. E5.
- **H-I-3.** `SpatialSketchTest`'s pair-order failure
  (`artifacts/sel-out-r1/PREEXISTING_SHARD5_FAILURE.md`) is still latent: its
  `setUp` is unchanged. Not re-run here. E3/E5.

## Adversarial counterexample attempts

1. **Order dependence of native suites.** Ran the 25 suites in reverse order,
   then CAD_FEATURE and PROJECT a second time (`rev_order.cpp`). Outcome: every
   count identical, 0 failures — no order dependence found (E1).
2. **Silent truncation at the device cap.** Ran PROJECT with caps 256/255/200
   (`cap_probe.cpp`). Outcome: the suite reports OK with 255 or 200 checks —
   counterexample SUCCEEDED (I-01).
3. **Corrupt fixture refused for the wrong reason.** `cmp` of four CADB pairs
   and the FRFM pair: 5–8 differing bytes each (the field + CRC). For
   `surface_bad_ref_v1`, which is not a byte-patch of a sibling, rebuilt the
   state from the decoded `surface_patch_extrude_v1` (`surf_attr.cpp`): Trim
   naming feature 7 → `UnknownFeature` (failed feature 3); the same Trim naming
   feature 1 → `Ok`. Outcome: refusals are attributable — counterexample FAILED.
4. **Self-test strings leaking into release under newer prefixes.** Scanned the
   release x86_64 `.so` for `CADV6|PICK_C2|FF_n|SURF_|CADR2|C1_CH|_ORACLE|
   FORGESHAPE_CAD_HUD_TOUCH|PERFORMANCE|GOLDEN`: 0 matches — FAILED to find a leak.
5. **Hidden ledger failures under a green device run.** Read the audit ledger of
   `40797031704`: all rows PASS/CLAMPED/NOT_MEASURED — FAILED to find one.
6. **"Run once" claim.** Counted suite runs in `test-logcat.txt`: 32 for 32
   tests — SUCCEEDED (I-02).

## Positive claims with their evidence

| Claim | Evidence |
| --- | --- |
| 25 suites, 4392 checks, 0 failures on the host; identical per-suite counts on the device | `fc2/host_selftests.txt`; `40797031704` startup (E1) |
| Suite results are order- and repetition-independent WITH AN EMPTY PROCESS SCENE (host) | seat-I `rev_order` (E1). Untested: `forgeshape_primitive_selftest.cpp:124-128` short-circuits to true when `constructionScene().hasProject()`, so on a warm Activity restart with a project open (I-02) it passes vacuously (E2) |
| 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines; CLAUDE.md and README lists equal the emission order | `40797031704` (E1); seat-I diff (E1) |
| Release `.so` has 0 self-test symbols/strings on both ABIs; debug control > 0 | `CI FAST` `40796992679` (E1); all 35 self-test TUs only in the debug CMake list (E2). The local run is supporting only: `run_status.txt` records `guard_exit=2` (precondition-failure path, `ci-release-selftest-guard.sh:29/37/58`) at 00:54:48; `release_guard.txt` (mtime 00:57) says PASS, but the rerun's command and exit code were not recorded |
| 72/72 fixtures byte-identical to the independent PowerShell builder | `CI FAST` `40796992679` (E1) |
| Corpus verdicts unchanged since the last recorded capture; corrupt fixtures are refused for their documented reason | seat-I diff, `cmp`, `surf_attr` (E1) |
| 196/196 JVM tests | `app/build/test-results` (E1) |
| Runner fingerprint/resume/exactly-once logic self-tested | TESTRUNTIME-01..26, THR1-01..10 in `40796992679` (E1) |
| Device guards hold (no bare adb, 5554 refused) | DEV2-01..07, DEV3-01..06 in `40796992679` (E1) |
| Dropped startup captures are proven, not assumed | `ci-device-smoke.sh` (E2); C1 record `liblog_dropped=35,39` then a clean third capture (E3) |
| "fails" in passing check names is never read as failure | `startup-failure-lines-1.txt` empty while `invalid_revision_fails_closed` is logged (E1) |

## Journeys proposed for END_TO_END_JOURNEYS

1. **J-I-1 Fresh debug startup proves the native domain** (positive). Install the
   debug APK on a fresh API 36 AVD, `logcat -G 64M`, launch. Expected: 25 tokens
   in CLAUDE.md order, each `(<n> checks)` equal to the host count, 0
   `_SELFTEST_FAIL`/`_FAIL:` lines, `NATIVE_VIEWPORT_OK`. Tests: `CI DEVICE`
   startup phase. Status: PASS-REPRODUCED in `40797031704` (counts compared by
   this seat, not by CI).
2. **J-I-2 A corrupt project file changes nothing** (negative / fail-closed).
   From Home, Open File… on `cad_bad_two_sides_v4.forge`, then on
   `surface_bad_ref_v1.forge`. Expected: refused by name, Home still standing,
   no body, no history, no checkpoint written. Tests: `HomeFlowTest` (corrupt /
   missing file case), PROJECT/SURF_22 host checks, host corpus verdicts.
   Status: host E1; device E3 (C2 run).
3. **J-I-3 The shipped release binary carries no self-test** (negative). Build
   `:app:assembleRelease`, run `scripts/ci-release-selftest-guard.sh`. Expected:
   release 0/0 both ABIs, debug > 0/ > 0, `RELEASE_SELFTEST_GUARD=PASS`. Status:
   PASS-REPRODUCED (local + `40796992679`).
4. **J-I-4 A task branch is gated before the OWNER sees an APK** (process).
   Dispatch `CI FAST` and one Tier-2 `CI DEVICE` list on the task branch; record
   both run ids and that `git diff --stat <run-sha> HEAD -- app scripts .github
   testdata` is empty. Expected: both green on the same tree. Status: practised
   by C2 (E3); not automated.

## Owner decisions needed

1. I-03: change the `push` triggers to cover task branches, or change CLAUDE.md
   and `docs/CI_CLOUD.md` to say "dispatch".
2. I-05: approve a host-suite step in `CI FAST`.
3. I-02: authorise a (debug-path) product change so the suites run once per
   process — it is the precondition for an affordable Tier-5 run.
4. When to run the Tier-5 aggregate (required once before the v6 line merges to
   `main`), and whether the diagnostic preview classes stay in it.
5. Whether a release-variant runtime smoke (I-10) is wanted, and with what signing.

## Unresolved dependencies and open questions

- Physical-device matters (stylus, pressure, hover, hardware GPU, 16 KB pages,
  real-device performance, the physical-phone startup capture of
  `PROJECT_STATUS.md:4158`) are OWNER-PHYSICAL-UNVERIFIED.
- The canonical master plan and OWNER decision files are not available; horizons
  in `work/req_I.tsv` come from CLAUDE.md, PROJECT_STATUS.md and the dossiers.
- Local corpus parity was not run (no `pwsh`); the CI FAST run supplies it.
- `CI DEVICE` job wall time for `40797031704` was not read from GitHub; the
  phase timings come from its artifact.
- H-I-1..3 need a device run to settle.

**Completeness: COMPLETE** for the seat's scope on this tree — every suite, JVM
class, androidTest class, fixture family, workflow and guard was inventoried and
the material counts re-derived; physical-device and OWNER-visual matters are
outside automation by rule and are listed as OWNER-PHYSICAL-UNVERIFIED rather
than audited.

## Round 2 — objections from seat B and responses

Seat B (Construction / objects / scene / transforms) raised 21 objections. Each
was re-checked on this tree. Commands for the re-checks are listed at the end.
SEAT_I.md and `work/req_I.tsv` were corrected where an objection was conceded.

Totals:
- 14 conceded, in full or in part: 1, 2, 3, 4, 6, 8, 11, 13, 14, 16, 18, 19, 20, 21.
- 0 rebutted. Two concessions carry a counter-point: on 2, the projection's
  arithmetic is shown and still points the same way; on 8, seat B's "4" is
  also short.
- 7 had nothing to answer: 5, 7, 9, 10, 12, 15, 17.
- 3 remain UNRESOLVED. All three need the coordinator, because they touch
  files outside this seat's writable set:
  - 11: provenance of the local guard rerun.
  - 8: the stale "112 in 9 classes" line in `TEST_COVERAGE_MATRIX.md`.
  - 21: the gizmo coverage note in `TEST_COVERAGE_MATRIX.md`.

| # | Objection | Re-check | Response | Status |
| --- | --- | --- | --- | --- |
| 1 | I-01 impact worded too strongly; add the near-cap Construction suites | Host output: PROJECT records 256 with cap 8192, so no check is lost today. Caps confirmed: `forgeshape_jni.cpp:567` MIRROR 128, `:902` mesh 128, `:923` construction 128. Host counts: 62, 91, 100. | CONCEDED. I-01 now reads "latent: 0 headroom, no current loss, overflow would be silent". The three caps are added beside CAMERA. Verdict stays DEFECT-CONFIRMED, P2 (`TST-005`). | Resolved |
| 2 | I-02 verdict, cause and projection overstated | `start()` guard is only `thread.joinable()` (`forgeshape_jni.cpp:2083-2088`). `git show fa0b6b4:…ForgeShapeActivity.java` has the same `stop()` at :582, and that tree's aggregate took 65.65 min (`PROJECT_STATUS.md:420-426`). CLAUDE.md says "run once from `NativeViewport.start()` … never per frame", which the code meets. | CONCEDED. Regraded to PARTIAL (test cost and debug-path latency) plus DOC-DIVERGENCE for `ForgeShapeActivity.java:33-36`. Cause: the per-start re-run is the multiplier, which pre-dates this tree, and suite growth is the cost (CAD_FEATURE 7.5 s on device). Projection arithmetic is now in I-02: 723 × 9.51 s ≈ 114.6 min of suite re-execution alone, about 95 % of the 120-min stop. It overflows if non-suite time exceeds ≈ 0.45 s per test; this run measured 11.3 s per test. It is still E5, because it assumes every test cold-starts the Activity. `SEAT_I.md` and `TST-007` now agree. | Resolved |
| 3 | I-03: cite `CI_CLOUD.md:12` against `:22-23`, and mention the PR route | Read `docs/CI_CLOUD.md:8-26`. Both workflows have `pull_request: [main]`. | CONCEDED. Added to I-03 and `TST-027`. Verdict kept. | Resolved |
| 4 | I-04 misses `ci-device.yml:2` | `.github/workflows/ci-device.yml:2` reads "(23 self-test tokens, …)". | CONCEDED. I-04 now lists six places; new row `TST-077`. | Resolved |
| 5 | I-05 confirmed | — | Nothing to answer. | — |
| 6 | `TST-040` cites the wrong requirement; list the B-domain classes | `PROJECT_STATUS.md:4851-4865` (merge to `main` only after Tier-5) and `:2545` (Tier-5 policy). `TEST_COVERAGE_MATRIX.md:141-176` marks all seven B classes "older". | CONCEDED. `TST-040` source is now the Next Stage Tier-5 gate, horizon H1, P2 kept. The seven classes are listed in I-06 and `TST-040`. | Resolved |
| 7 | I-07 confirmed | — | Nothing to answer. | — |
| 8 | I-08 class count is 4, not 9; add the gizmo | `ModelingOwnerRig` users: 4 test classes. Classes with their OWN decor-root `realGesture`: `CadFillPickR2Test`, `CadMultiFaceOwnerTest`, `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`. The "9" counted the rig file itself. `EditorWorkspaceGizmoTest.java:2126,2138` dispatch to the viewport. | CONCEDED IN PART. The correct count is 8 test classes: 4 through the rig and 4 with their own helpers, so seat B's 4 is also short. The re-count is ≈ 106 call sites, replacing 112. The rig's `press()` is `performClick` and unchecked. Gizmo impact added. `SEAT_I.md` and `TST-044` corrected. `TEST_COVERAGE_MATRIX.md:184` still says "112 in 9 classes" and needs the coordinator. | UNRESOLVED (coordinator: matrix line) |
| 9 | I-11 confirmed | — | Nothing to answer. | — |
| 10 | I-09, I-10, I-13, I-14 consistent | — | Nothing to answer. | — |
| 11 | Local release guard PASS not disclosed in `SEAT_I.md` | `run_status.txt`: `guard_exit=2` at 00:54:48. `release_guard.txt` mtime is 00:57 and says PASS. No rerun command or exit code is recorded anywhere this seat can read. | CONCEDED. The positive-claims row now rests on `CI FAST` `40796992679` (E1). The local run is labelled supporting-only, with its provenance incomplete; `TST-014` is annotated. This seat cannot supply the rerun command, because it did not issue it. | UNRESOLVED (coordinator: rerun provenance) |
| 12 | 4392 checks and identical device counts verified | — | Nothing to answer. | — |
| 13 | Order-independence claim overstated | `forgeshape_primitive_selftest.cpp:122-128` reads `constructionScene().hasProject()` and short-circuits. A grep for process accessors over `*_selftest.cpp` finds no other process-scoped read. | CONCEDED. The claim is now restricted to "with an empty process scene". The vacuous pass on a warm restart is noted as untested (E2) in the positive-claims table and `TST-006`. | Resolved |
| 14 | `TST-072` caveat rests on a stale comment | `forgeshape_mesh_selftest.cpp:147,370` and `forgeshape_construction_selftest.cpp:265,334` build local `MeshStore`s. The `forgeshape_jni.cpp:2120-2125` comment says the suites publish into the store. | CONCEDED. `TST-072` now says the only process-scoped read is the read-only check of item 13, and records the comment as DOC-DIVERGENCE, INFO. | Resolved |
| 15 | Goldens and 72/72 parity: no objection | — | Nothing to answer. | — |
| 16 | `TST-036` uses the non-vocabulary verdict `HYPOTHESIS` | Confirmed in the TSV. | CONCEDED. Now UNDETERMINED-SOURCE-GAP, still listed as H-I-1. | Resolved |
| 17 | Horizons correct | — | Nothing to answer. `TST-040` moved H0 → H1 under item 6. | — |
| 18 | Face-supported transform refusal has no test; `TST-065` overclaims | `isFaceSupportedCad` outside self-tests: `forgeshape_body_commands.cpp:205` (Duplicate) and `forgeshape_jni.cpp:7679` (seam). Its androidTest callers (`HomeFlowTest`, `CadA3VisualEvidenceTest`, `OwnerProjectShellSketchSupportTest`, `SpatialSketchTest`) only assert that a body IS face-supported. 0 hits in the Gizmo, ObjectCommands and GlbExport tests. Re-ran seat B's binary `/tmp/claude-0/fc2-scratch/seatB/diag_face_supported`: `gizmo visible=1`, `applyStatus=0 undoDepth 0->1 fingerprintChanged=1`, resolved model still `(0,0,3)` (E1, this response). | CONCEDED. New row `TST-076`: H0 (CAD-A3), no test, DEFECT-CONFIRMED per SEAT_B B-01, P2. `TST-065` annotated. | Resolved |
| 19 | `TST-060` says nothing about face-supported export | `forgeshape_gltf_export.cpp:177-179` reads `body.transform()`. No face-supported case in `forgeshape_gltf_export_selftest.cpp`. The same diagnostic run shows `export body 2 translation=(0,0,0)`, with the dependent drawn at `(0,0,3)`. | CONCEDED. `TST-060` annotated ("bake rule asserted for independent bodies only; face-supported placement untested and defective, B-02"). The missing GLTF_EXPORT check is added as a proposed coverage addition. | Resolved |
| 20 | `TST-066` names the wrong suite | `forgeshape_cad_a3_selftest.cpp:883` asserts `RefusedFaceSupportedCad`; it is not in the SCENE suite. | CONCEDED. Attribution corrected to CAD_A3 (66). | Resolved |
| 21 | B-04 and B-06 gizmo states not in the coverage tables | `ObjectCommandsTest` has 1 test. No test covers the gizmo after a select onto a locked body, after Undo of Unlock, or over a hidden active body. | CONCEDED. The note is added to `TST-065`, with verdicts left as SEAT_B grades them. `TEST_COVERAGE_MATRIX.md` is outside this seat's writable set, so the coordinator must add the same note there. | UNRESOLVED (coordinator: matrix note) |

**Proposed coverage additions (round 2):**
- A host check that a face-supported CAD body refuses a transform write and gizmo activation (`TST-076`).
- A GLTF_EXPORT check that a face-supported dependent exports at its resolved world placement (`TST-060`).
- Device or host checks for the gizmo after a select onto a locked body, after an Undo of Unlock, and over a hidden active body (`TST-065`).

**Re-check commands (round 2):**
- `grep -n "kMax[A-Za-z]*Checks *=" app/src/main/cpp/forgeshape_jni.cpp`, then `sed -n` on lines 560-570, 898-905 and 918-926.
- `git show fa0b6b4:app/src/main/java/com/forgeshape/app/ForgeShapeActivity.java | grep -n "stop()"`
- `sed -n 2080,2128p app/src/main/cpp/forgeshape_jni.cpp`
- `grep -l ModelingOwnerRig app/src/androidTest/java/com/forgeshape/app/*.java`
- `grep -l "touchView\|realGesture" app/src/androidTest/java/com/forgeshape/app/*.java`
- `grep -nE "(^|[^.:a-zA-Z_])(constructionScene|constructionTransform|meshStore|sketchSession)\(\)" app/src/main/cpp/*_selftest.cpp`
- `/tmp/claude-0/fc2-scratch/seatB/diag_face_supported`: seat B's prebuilt binary, run read-only.
- Edits were made by `/tmp/claude-0/fc2-scratch/I-vs-B/edit_tsv.py` and `/tmp/claude-0/fc2-scratch/I-vs-B/edit_seat.py`.
