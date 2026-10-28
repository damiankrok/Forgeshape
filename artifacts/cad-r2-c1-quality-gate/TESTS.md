# CAD-R2-C1 — TESTS

Raw commands and results in the order run. Container: Linux x86_64, g++ 13.3.0
(host suites), OpenJDK driving the Gradle 8.14.3 wrapper, a local Android SDK
with the pinned NDK `29.0.14206865` (`local.properties` git-ignored). CI on
GitHub-hosted `ubuntu-24.04` (JDK 17), `docs/CI_CLOUD.md`.

## T0 — preflight

Remote refs re-verified (`main` `6c9f156`, R2 predecessor `636797e`, R2
`ea24d20`), clean tree, new branch `feature/cad-r2-c1-quality-gate-r1` from
exactly `ea24d20`. `git diff --check` clean before every commit. Startup
suites stay 25, no new token, none per frame.

## T1 — host native

`bash scripts/host-native-selftests.sh` → **`HOST_SELFTESTS_OK (4375 checks, 0
failed)`**, 25 suites (baseline 4369; CAD_FEATURE 626 → 632; every other suite
unchanged), `CAD_R2_ORACLE_PASS=17`, every R2_SK/HIST/PF/SUP/DATA check kept.

New checks (`forgeshape_cad_r2_selftest.cpp`, `testChooserOwnership`), each
for behaviour actually fixed or classified:

| Check | Asserts |
| --- | --- |
| C1_CH_01 | the curved-wall scenario has a world plane BEHIND the wall on the same ray |
| C1_CH_02 | two taps on the curved wall: `None` both times, chooser active, fingerprint unchanged |
| C1_CH_03 | a YZ plane point in front of a face: `WorldPlane YZ` exactly |
| C1_CH_04 | an eligible pocket floor in front of the XY plane: exact `TopoRef` (body, feature 3, `CapFar`, lineage), twice |
| C1_CH_05 | a revolved surface with planes behind: `None`, fingerprint unchanged |
| C1_CH_06 | hover over the curved wall: `None`, nothing selected |

Before the fix (fix reverted in the working tree): C1_CH_02, 05, 06 FAIL
(`CADR2_CHECK_FAILED …`); C1_CH_04 also failed on its first aim point, the
floor's centre on the triangle diagonal (pick crack, `HIT_OWNERSHIP.md`) —
re-aimed off the diagonal, it passes with and without the fix.

## T2 — JVM

`./gradlew :app:testDebugUnitTest` → **196 tests, 0 failures, 0 errors**
(unchanged count: the one Java product change is a visibility condition inside
a View, proven on the device by D17, which has no JVM seam).

## T3 — Android builds, release guard

`./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease`
→ BUILD SUCCESSFUL. `ANDROID_HOME=/root/android-sdk bash
scripts/ci-release-selftest-guard.sh` → `RELEASE_SELFTEST_GUARD=PASS`: release
x86_64 3,256,064 B and arm64-v8a 2,916,072 B, 0 `SelfTests` symbols and 0
self-test strings; debug control 35 symbols / 398 strings on both ABIs. No new
dependency, no binary format change. The perf harness lives under `artifacts/`
and is compiled by nothing in the build.

## T4 — DEVICE (CI DEVICE, API 36 x86_64 emulator, real decor-window touches)

Focused `CadRetainedSketchOwnerTest` attempts (all recorded):

| Run | Head | Result | Cause | Remediation |
| --- | --- | --- | --- | --- |
| 40740706307 | `9ff53b0` | 16/18 | D04/D15/D16 PASS (curved wall → kind −1, plane in front → YZ sketch). **D17 product**: at font 1.5 portrait `history_act_edit_sketch` did not own its centre. **D18 harness**: re-tapping the selected row deselects it (`FeatureHistoryView`) | D18 asserts the selection survives instead; D17 failure made to name the occluder (`68894b2`) |
| 40744870935 | `68894b2` | 17/18 | D17: under the act `LinearLayout cad_canvas_edit_sketch [477,1137 126x126]`, History surface `Rect(366, 896 - 1059, 2169)` — the retained-sketch chip stood on History's act | **product fix** `cf340f0`: the chip steps aside while History is open; D17 asserts its absence |
| **40748215407** | `cf340f0` | **PASS 18/18** (650 s) | | evidence `ci-device-evidence` 12844485594 |

Font scale facts from the passing run (px, 1080 × 2400 at 420 dpi, 48 dp =
126 px): every act 304 × 126 or 199 × 126 (tabs), all inside the window, apart:

| Scale | Portrait unused-sketch acts | Landscape unused-sketch acts |
| --- | --- | --- |
| 1.0 | (403,1415)…(1022,1678) | (1723,287)…(2342,550) |
| 1.3 | (403,1237)…(1022,1500) | (1723,300)…(2342,563) |
| 1.5 | (403,1139)…(1022,1402) | (1723,306)…(2342,569) |

Screenshots `d17_font_{1_0,1_3,1_5}_{0,1}.png`, `d17_journeys_font_1_5.png`,
`d04_curved_wall_refused.png`, `d15_plane_aimed.png` in that artifact.

### Final DEVICE union (bounded; two parts, the workflow's 2700 s cap)

The product tree is frozen at `cf340f0`; `git diff --stat cf340f0 e81639b`
touches only `artifacts/cad-r2-c1-quality-gate/TESTS.md`, so both parts test
the same `app/` tree. They are two runs, not one same-SHA aggregate.

| Part | Run | Head | Classes | Result | Evidence |
| --- | --- | --- | --- | --- | --- |
| A | **40752389411** | `cf340f0` | `OwnerProjectShellSketchSupportTest`, `CadMultiFaceOwnerTest`, `CadFillPickR2Test`, `CadHud3dOwnerTest`, `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`, `CadParametricHistoryOwnerTest`, `HomeFlowTest`, `EditorWorkspaceProjectActionsTest` | **run=67 failed=0** (2008 s) | `ci-device-evidence` 12846579227 |
| B | **40760227555** | `e81639b` (app tree = `cf340f0`) | `CadRetainedSketchOwnerTest` (D01..D18), `EditorWorkspaceCorrectionTest` | **run=55 failed=0** (1294 s) | `ci-device-evidence` 12849042735 |

Union **122 / 122**. Both parts: 25/25 startup tokens in order,
`NATIVE_VIEWPORT_OK`, 0 failure lines, `FORGESHAPE_CI_DEVICE_RESULT=PASS`.
Part B's first two startup captures lost log lines (`liblog_dropped=35`, `39`:
23/25 and 24/25 tokens, out of order, 0 failure lines) and the script's third
capture was clean (25/25 in order, 0 dropped) — a dropped capture as
`CLAUDE.md` describes, not a missing suite. Part A: capture 1 clean.

## T5 — persistence, CI FAST, APK

`CI FAST` run **40766013297** on `911f5c3` (docs only after `cf340f0`:
`git diff --stat cf340f0 911f5c3 -- app/` is empty): **success**. Rechecked in
the downloaded artifact: JVM 20 suites, **196 tests, 0 failures, 0 errors**;
`RELEASE_SELFTEST_GUARD=PASS` (release x86_64 3,256,064 B / arm64-v8a
2,916,072 B: 0 symbols, 0 strings; debug 35 / 398); **`FORGE_CORPUS_PARITY=PASS
(72/72 byte-identical)`**, `committed=72 generated=72 mismatches=0`; device-
guard and runner checks PASS.

**APK** — `ci-fast-evidence` artifact **12849379257** (zip SHA-256
`ddca52c843cbcd0516c760b153913a1a8bc124d8942e563b367dee8e28b8361b`, equal to
GitHub's recorded digest; expires 2026-11-11T20:52:13Z), path
`app/build/outputs/apk/debug/app-debug.apk`, **17,552,535 bytes**, SHA-256
**`fd7a11a9a7b1bbf7748b46e2d71c71a2a830b54f59e8bd7f9011113d968b1dd5`**, hashed
from the downloaded CI bytes; no local rebuild substituted. A debug APK, not a
signed production build.

No FullSharded run. No skipped or weakened test: D04's assertion became
stricter (exact `NONE` instead of `!= FACE`); D18's re-tap was a harness error
corrected to a stronger assertion (the selection survives).
