# Validation plan, Tiers 0–5 (FUNCTION-COUNCIL-R2, seat I)

**Status: PROPOSAL.** It changes no workflow, script, test or rule. It is a
re-statement of FUNCTION-COUNCIL-R1's `LEAN_VALIDATION_POLICY.md` against tree
`70a6c14`, with every number re-measured here. No new framework: JUnit, the
AndroidX runner, the existing scripts and the three existing workflows only.
Seat I's findings are referenced as I-NN (`SEAT_I.md`).

## 0. What changed since FUNCTION-COUNCIL-R1 (its figures are obsolete)

| Fact | FC-R1 (`a1b50f2`) | Now (`70a6c14`) | Grade |
| --- | --- | --- | --- |
| Native suites / checks | 23 / 3757 | **25 / 4392** | E1 |
| JVM | 11 classes / 104 | **20 / 196** | E1 |
| androidTest | 57 classes / 629 | **70 / 723** | E2 |
| Fixtures | 44 | **72** | E1 |
| Debug self-test time per Activity start, CI emulator | not measured | **~8.4 s** (CAD_FEATURE 7.5 s of it); `Displayed +9.5–9.8 s` | E1 (`40797031704`) |
| Device time per test | 6.2 s (aggregate mean) | **~20.8 s** (32 tests, 667 s) in `40797031704`; 27.6 s and 25.9 s in C2's two runs | E1 / E3 |
| Self-test re-runs inside a device run | not noticed | **once per test** (32 starts for 32 tests; ~304 s of the 667 s) | E1 |

The last point dominates cost: `NativeViewport.start()` re-runs all 25 suites
whenever the render thread is not alive (`forgeshape_jni.cpp:2083-2126`), and
`ForgeShapeActivity.onDestroy` stops it on every non-configuration destroy
(`ForgeShapeActivity.java:582`), which `ActivityScenario` does after every test
(I-02). An exhaustive aggregate of 723 tests would therefore spend on the order
of 723 × ~9 s ≈ 1.8 h re-running self-tests alone (E5, extrapolated from E1). A
Tier-5 run is not affordable inside the runner's 120-minute hard stop until I-02
is addressed.

## 1. Evidence rules (all tiers)

1. **Evidence names a tree.** A run counts for a tree only when
   `git diff --stat <run-sha> <tree> -- app scripts .github testdata` is empty.
   Docs-only commits after a run do not invalidate it; any product or test edit
   does.
2. **Branch.** Normal work: a task branch from the current base. `CI FAST` and
   `CI DEVICE` do **not** run on a task-branch push (`ci-fast.yml:12-17`,
   `ci-device.yml:10-15`); they are dispatched (`workflow_dispatch`) on the task
   branch, and the run id plus its head sha are recorded (I-03).
3. **Emulator is not a phone.** No `CI DEVICE` or `CI FULL SHARDED` result is
   cited for stylus, pressure, hover, a hardware GPU, 16 KB pages, real-device
   performance or visual quality; those are OWNER-PHYSICAL-UNVERIFIED until Tier 4.
4. **A red is investigated, never retried to green.** One focused re-run is
   allowed only to separate an order dependence from a product defect.
5. **Visual-evidence and audit classes** (`SelectionOutlineVisualEvidenceTest`,
   `UiPrefVisualEvidenceTest`, `SketchUxVisualEvidenceTest`,
   `CadA3VisualEvidenceTest`, `Ui3dStateAuditTest`, `Ui3dDimensionVisibilityTest`)
   are Tier-4 material. A green run of them proves the journey executed, not
   that the frames are right; `Ui3dStateAuditTest`'s verdicts are in its
   `VISIBILITY.tsv` / `SPATIAL_ATTACHMENTS.tsv`, which must be read (I-13).

## 2. Tiers

### Tier 0 — static, < 1 minute, local, every change

```
git diff --check origin/main...HEAD          # or the task base
git diff --stat <base>...HEAD -- app scripts .github testdata
```

Expected: no whitespace error; the stat lists only what the task meant to touch.
When a script under `scripts/` changed, add (needs `pwsh`; otherwise Tier 3 runs them):

```
pwsh -NoProfile -File scripts/verify-device-guards.ps1         # All device-guard checks PASS
pwsh -NoProfile -File scripts/test-instrumented-runtime.ps1    # TESTRUNTIME-01..26 PASS
pwsh -NoProfile -File scripts/test-instrumented-sharding.ps1   # THR1-01..10 PASS
```

### Tier 1 — contracts, ≤ 5 minutes, local, every change touching native or Java logic

```
bash scripts/host-native-selftests.sh                # all 25; or one substring filter
bash scripts/host-forge-corpus-verdicts.sh artifacts/function-council-r2/evidence/local/host_corpus_verdicts.txt
./gradlew :app:testDebugUnitTest                     # only when no other Gradle process runs
```

Expected outcomes and measured cost:

| Command | Expected | Cost (E1 unless marked) |
| --- | --- | --- |
| host suites | `HOST_SELFTESTS_OK (4392 checks, 0 failed)` or a higher total that the task explains; per-suite counts never lower than the table in `TEST_COVERAGE_MATRIX.md` §2 | ~2.3 s of suite execution (sum of the 25 `ms` fields); first build compiles the kernel once (E4 2–3 min) |
| corpus verdicts | output equals the committed capture exactly (72 lines) | seconds |
| JVM | 196 tests (or more), 0 failures | 13 s Gradle wall in this audit (`gradle_unit_rerun.txt`) |

Filters: `CAD` (CAD, CAD_A3, CAD_FEATURE), `SKETCH_UX`, `SCULPT`, `SCENE`,
`CONSTRUCTION` (five suites), `GIZMO`, `PROJECT`, `RENDER` (two), `GLTF` (two),
`FREEFORM`, `SURFACE`, `MIRROR`, `BODY_DIMENSIONS`, `PICKING`, `CAMERA`.
A change in `forgeshape_jni.cpp` or `forgeshape_renderer.cpp` has no Tier-1
answer (both are excluded from the host build) and goes to Tier 2.

When a suite gains checks, compare its new count with its device cap
(`TEST_COVERAGE_MATRIX.md` §2). **PROJECT is at 256/256 today**: any new
PROJECT check is silently skipped on the device until the cap moves (I-01).

### Tier 2 — one focused `CI DEVICE` dispatch, target 20 min, hard stop 35 min

GitHub → Actions → `CI DEVICE` → Run workflow on the task branch, `test_class`
= a comma-separated list. One dispatch per task (the workflow cancels an
in-progress run on the same ref). Expected `summary.json`: `"result": "PASS"`,
`selftest_tokens_found: 25`, `selftest_tokens_in_order: true`,
`native_viewport_ok: true`, `failure_token_lines: 0`, `tests_failed: 0`.
Also confirm the 25 `(<n> checks)` values in `startup-forgeshape.txt` equal the
host counts (no CI step does this yet; I-14).

Cost model on this tree: fixed part ≈ boot 41 s + settle up to 181 s + startup
10 s + APK build (E1 for the first three); then **≈ 9 s of self-test re-run per
test** plus the test's own time (I-02). Lists below are sized for ≤ ~40 tests.

| Subsystem touched | `test_class` list | Tests |
| --- | --- | ---: |
| CAD retained sketch / History / chooser | `CadRetainedSketchOwnerTest,CadParametricHistoryOwnerTest,OwnerProjectShellSketchSupportTest` | 30 |
| CAD fill / multi-face / HUD | `CadFillPickR2Test,CadMultiFaceOwnerTest,CadHud3dOwnerTest,CadPlanarFaceOwnerCorrectionTest` | 22 |
| CAD Revolve / drafting | `CadRevolveOwnerTest,CadSketchDraftingOwnerTest` | 23 |
| CAD legacy paths (support, extrude, extent) | `SpatialSketchTest,SketchExtrudeTest,CadCanvasExtrudeTest,CadExtrudeExtentTest,SketchUxTest` | 47 |
| Sculpt | `SculptUndoTest,SculptBrushStage025Test,SculptHistoryNavigatorTest,EditorWorkspaceSculptRetentionTest` | 29 |
| Objects / Construction | `ObjectsDeleteTest,ObjectCommandsTest,MirrorSmokeTest,BodyDimensionsSmokeTest,EditorWorkspaceHistoryTest` | 31 |
| Gizmo / transform / pointer | `EditorWorkspaceGizmoTest` (46; dispatch alone) or `EditorWorkspacePointerTest,Ui3dStateCorrectionTest` | 46 / 25 |
| Persistence | `ProjectAutosaveRecoveryTest,ProjectProcessDeathTest,ProjectTransferTest` | 35 |
| Import / export | `ImportedMeshDurableTest,GlbExportTest,ImportGlbSculptGuardTest` | 39 |
| Renderer / outline / loss | `SelectionOutlineTest,RendererCounterContinuityTest,DiagnosticsAndRendererLossTest` | 29 |
| Project shell | `HomeFlowTest,EditorWorkspaceProjectActionsTest,EditorWorkspaceLifecycleTest` | 22 |
| Freeform / Surface | `FreeformOwnerTest,SurfaceOwnerTest` | 6 |
| Preferences | `SettingsPreferencesTest` | 15 |
| Any JNI signature change | add `JniBoundaryHardeningTest` | +5 |

Order inside a list: the class proving the new behaviour first (classes share
the process; `resetToBaselineConstruction` cannot un-freeze,
`WorkspaceTestSupport.java:72-118`). When a red is investigated and it
disappears alone, record it as order coupling, not as fixed.

### Tier 3 — `CI FAST`, ~5 min, dispatched on the task branch, every task

GitHub → Actions → `CI FAST` → Run workflow on the task branch. Expected:
success; `forge-corpus-parity.txt` = `committed=72 generated=72 mismatches=0`;
`release-selftest-guard.md` ends `RELEASE_SELFTEST_GUARD=PASS` with release
0/0 and debug > 0/ > 0 on both ABIs; `test-instrumented-runtime.txt`,
`test-instrumented-sharding.txt`, `verify-device-guards.txt` all PASS; JUnit
reports 0 failures. Run it in parallel with Tier 2. The run id, its head sha and
the APK SHA-256 handed to the OWNER are recorded in the closeout.

What Tier 3 does **not** cover today: the 25 host suites and the corpus-verdict
diff (I-05). Until a FAST step runs `bash scripts/host-native-selftests.sh`
(proposal P-1 below), Tier 1 must be run and its output recorded by the task.

### Tier 4 — OWNER review, physical device

Required for: anything visual, ergonomic or about feel; stylus, pressure, hover,
palm rejection; a hardware GPU; 16 KB-page ARM64; real-device performance; the
physical-phone startup capture (`PROJECT_STATUS.md:4158` records dropped lines
there). Input: the debug APK from the Tier-3 run (artifact id + SHA-256), the
Tier-4 frames from the visual classes (dispatched as their own Tier-2 list), and
the task's checklist. Output: an OWNER verdict recorded in `PROJECT_STATUS.md`.
Automation never substitutes for it.

### Tier 5 — `CI FULL SHARDED`, OWNER gate only

Run only with explicit OWNER authorisation, and only for: a release or beta
candidate; the integration of `feature/cad-v6-sketch-face-r1` into `main`
(`PROJECT_STATUS.md` Next Stage requires it once); a `.forge` section/version or
corpus change; a shared vertex/pipeline/JNI layout change; the kernel boundary;
the test harness or runner; evidence of cross-class coupling.

```
# GitHub → Actions → CI FULL SHARDED, on the candidate branch:
mode=plan                     # first; no instrumentation, prints the partition
mode=fresh                    # one counted attempt; at most two per fingerprint
mode=shard shard=<n>          # focused follow-up, never an aggregate
```

Expected: `FULL_SHARDED_SUITE_PASS`, missing = duplicates = unexpected = 0, every
shard on one fingerprint. Budget: the runner warns at 90 and stops at 120
minutes. **Prerequisite on this tree:** I-02 (self-tests re-run per test) or a
plan whose partition fits the budget, because 723 tests at the measured ~20 s
per test is ~4 h (E5). The last aggregate is `36607747079` on `fa0b6b4`
(57 classes / 629 tests, E3); no aggregate covers the current 70 classes.

## 3. Which evidence closes which claim

| Claim | Minimum evidence |
| --- | --- |
| Native arithmetic / codec / kernel | Tier 1 output on the tree + Tier 2 startup tokens with counts equal to host |
| Format / fixture | Tier 1 + Tier 3 corpus parity on the same tree |
| A user journey through chrome | Tier 2 class on the same tree whose touches go through the decor root (`ModelingOwnerRig`), not `performClick` or a viewport-direct tap (I-08) |
| Release binary hygiene | Tier 3 guard (absence only; no release runtime exists — I-10) |
| Anything physical or visual | Tier 4 |
| "The whole suite is green" | Tier 5 only |

## 4. Proposed changes (each a separate authorised task; none is a new framework)

| Id | Change | Fixes | Cost / risk |
| --- | --- | --- | --- |
| P-1 | Add `bash scripts/host-native-selftests.sh` and `bash scripts/host-forge-corpus-verdicts.sh <committed expected>` to `ci-fast.yml` | I-05 | +1–3 min FAST; low |
| P-2 | Fail a device suite when `count == cap`; raise PROJECT (and CAMERA) caps | I-01 | debug-only C++; low |
| P-3 | Run the 25 suites once per PROCESS (a static flag in `start()`), or move CAD_FEATURE's performance/oracle blocks host-only | I-02 | product source (debug path); halves Tier-2 time |
| P-4 | `ci-device-smoke.sh`: compare each `(<n> checks)` with a committed expected table; fail on -3/-4 statuses; record `settle_timed_out`; copy audit-ledger verdict counts into `summary.json` | I-12, I-13, I-14 | script only |
| P-5 | Either add task-branch patterns to the `push` triggers or change `CLAUDE.md`/`docs/CI_CLOUD.md` to "dispatch" | I-03 | OWNER choice |
| P-6 | Route `SketchTestSupport.tapViewport/dragSketch/tapSketch` through the decor root and assert the hit target | I-08 | test harness = Tier-5 trigger |
| P-7 | Replace `settleLayout`'s 250 ms and the five fixed capture delays with traversal / presented-frame waits; move `EditorWorkspaceLegibilityTest`'s animator restore into `@After` | I-09, I-11 | test-only |
| P-8 | Update stale counts in `docs/CI_CLOUD.md`, `README.md:194`, `ci-device-smoke.sh:57`, and the two `PROJECT_STATUS.md` tables | I-04 | docs |
| P-9 | Split `FORGESHAPE_CAD_FEATURE_PERFORMANCE` into lines < 1000 chars | I-07 | debug-only |
