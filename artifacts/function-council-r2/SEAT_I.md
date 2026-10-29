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
- **Impact.** The next PROJECT check would run on the host and be silently
  skipped on the device — the only CI path that executes native checks (I-05).
  Precedent: CAD_FEATURE lost 49 checks at cap 512
  (`artifacts/cad-r2-retained-sketch-parametric-ux/TESTS.md:76`, E3).
- **Verdict.** DEFECT-CONFIRMED (test infrastructure). Same fact as SEAT_H H-13.
- **Fix.** Make `count == cap` a suite failure; raise PROJECT and CAMERA caps.

### I-02 — P2 — The 25 suites re-run on every Activity start, ~8.4 s each on the CI emulator, once per instrumented test

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
- **Impact.** About 46 % of device test time in this run is self-test
  re-execution; per-test cost is ~20.8 s against FC-R1's 6.2 s; a 723-test
  aggregate would not fit the 120-minute hard stop (E5). In a debug build, any
  cold Activity creation in a live process blocks the main thread ~9 s. Release
  is unaffected (suites compiled out).
- **Verdict.** DEFECT-CONFIRMED (debug path / test cost; the code contradicts its
  own "once per PROCESS" comment).
- **Fix.** A process-static "suites ran" flag checked in `start()`; optionally
  move CAD_FEATURE's performance and oracle blocks host-only.

### I-03 — P2 — A task-branch push runs no CI, contrary to CLAUDE.md and docs/CI_CLOUD.md

- **Where.** `ci-fast.yml:12-17`, `ci-device.yml:10-15` (`push: [main,
  infra/ci-cloud-r1]`, `pull_request: [main]`, `workflow_dispatch`);
  `CLAUDE.md` Hard rules ("push the task branch and let GitHub Actions test it");
  `docs/CI_CLOUD.md` "The flow" step 2.
- **Repro (E2/E1).** Trigger lists above; this audit's FAST run reports
  `event=workflow_dispatch` (`toolchain-manifest.txt`).
- **Impact.** An undispatched task branch has no CI evidence while the docs say
  it does. Re-confirms FC-R1 `TEST_COST_AUDIT.md` §6.3a, still unresolved.
- **Verdict.** DOC-DIVERGENCE.
- **Fix.** OWNER chooses: add task-branch globs to `push`, or change both texts
  to "dispatch".

### I-04 — P3 — Stale counts in five places

- **Repro (E1/E2).**
  - `docs/CI_CLOUD.md`: "all 57 fixtures" (72), "all **23** `*_SELFTEST_OK`
    tokens" and "the 23 startup tokens" (25).
  - `README.md:194`: "Seven of the fifty-seven fixtures" (72 committed).
  - `scripts/ci-device-smoke.sh:57`: "The twenty-four startup suites" above a
    25-token list.
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
  Only `ModelingOwnerRig.touchView` checks the control is the hit target
  (`ModelingOwnerRig.java:139-149`) and `realGesture` enters at the decor root
  (`:215-235`) — 112 sites in 9 classes.
- **Impact.** The CLAUDE.md rule "a surface may never partially cover another
  live control" and "the nearest chooser target owns a tap" are only provable by
  the 9 rig classes. The Edit Sketch chip taking a face tap
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
| Suite results are order- and repetition-independent | seat-I `rev_order` (E1) |
| 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines; CLAUDE.md and README lists equal the emission order | `40797031704` (E1); seat-I diff (E1) |
| Release `.so` has 0 self-test symbols/strings on both ABIs; debug control > 0 | local guard PASS; `CI FAST` `40796992679` (E1); all 35 self-test TUs only in the debug CMake list (E2) |
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
