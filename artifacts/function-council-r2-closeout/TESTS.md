# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — TESTS (executed vs not executed)

Product tree for every run below: `70a6c14` (= `f3db953` for `app/src/main`).
Every audit-branch head used by CI was checked with
`git diff --name-only 70a6c14 <head> -- app scripts testdata .github gradle
build.gradle settings.gradle gradle.properties DATA_PACKAGE_SPEC.md` → **0
files** (`9cfc3f5`, `7c05748`, `2e5d8fc`, `f749a58`, `642e47f`).

## 1. Executed in this closeout

### 1.1 Local (Linux x86_64, NDK 29.0.14206865), 2026-10-30

Records in `evidence/local/` (`run_status.txt` holds every exit code).

| Command | Exit | Result |
| --- | --- | --- |
| `bash scripts/host-native-selftests.sh` | 0 | `HOST_SELFTESTS_OK (4392 checks, 0 failed)`; 25/25 `HOST_SUITE_OK`; `CAD_R2_ORACLE_PASS=17` |
| `bash scripts/host-forge-corpus-verdicts.sh` | 0 | `CORPUS_FIXTURES 72` (decoder verdicts; not the pwsh encoder-parity check) |
| `./gradlew :app:testDebugUnitTest --rerun-tasks` | 0 | 22 tasks executed; **196 tests, 0 skipped, 0 failures, 0 errors** (20 XML files) |
| `./gradlew :app:assembleDebug :app:assembleRelease` | 0 | BUILD SUCCESSFUL |
| `bash scripts/ci-release-selftest-guard.sh` (`ANDROID_HOME` set) | 0 | `RELEASE_SELFTEST_GUARD=PASS` |
| Closeout diagnostics (CR-01..CR-07, RECONCILE, ALIAS, POSCLAIM, UX-REVIEW, desktop, panel probes) | per `diagnostics/*/CMD.txt` | audit-only programs, never built into the product; outputs quoted in `CRITICAL_REPROS.md` |

### 1.2 Focused CI DEVICE (dispatched by this closeout; emulator, never physical)

Both runs: `system-images;android-36;google_apis;x86_64`, `medium_phone`,
`swiftshader_indirect`, AVD `ForgeShape_CI_API36` confirmed, **25/25
startup self-test tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure-token
lines**. Budget estimated before dispatch at ≈25 s/test against the 2700 s
instrumentation cap; both stayed below it. These are two bounded runs of
named classes, not an aggregate.

| Run | Head | Classes | Result | Time | Artifact (digest) |
| --- | --- | --- | --- | --- | --- |
| `40979514270` | `f749a58` | `GlbExportTest` 24, `SpatialSketchTest` 8, `SketchExtrudeTest` 9, `CadCanvasExtrudeTest` 11, `CadPlanarFaceRuntimeTest` 6, `CadPlanarFaceOwnerCorrectionTest` 5 | **63/63 PASS**, 0 skipped | 1365 s | `12946055768` (`sha256:b358d930…f2d`) |
| `40983965648` | `642e47f` | `EditorWorkspaceGizmoTest` 46, `Ui3dStateCorrectionTest` 12, `EditorWorkspaceMobileTest` 15 | **72/73 — 1 FAIL**, 0 skipped | 1440 s | `12948105153` (`sha256:f77dfa18…fe3`) |

The failure: `EditorWorkspaceMobileTest#uir4a04_addPrimitiveOffersTheSixRealPrimitivesAndNothingElse`
(`EditorWorkspaceMobileTest.java:274`, `expected:<8> but was:<12>`). Triage in
`evidence/device-run-40983965648/TRIAGE.md`: the Add palette gained three
Freeform tiles and one Surface tile on 2026-10-06 (`d1a0671`, `7b4c7a4`,
authorised by `MODELING-FOUNDATIONS-R1`); the test was last edited 2026-09-27
and the last aggregate (`fa0b6b4`, 2026-09-29) predates the tiles. **Stale
test assertion (TEST_HARNESS_DEFECT), not a product regression.** It was not
fixed (audit only).

What the green runs do and do not prove:
- `GlbExportTest` 24/24 green while CR-01 reproduces at E1: the class contains
  no CAD Body and no face-supported case.
- `EditorWorkspaceGizmoTest` 46/46 green while CR-02 reproduces at E1: no case
  uses a face-supported CAD body.
- The five sketch / planar-face classes closed the coverage gap C-06 named
  (emulator).

### 1.3 GitHub metadata re-verified (previous dossier runs)

| Run | Workflow | Head | Product diff vs `70a6c14` | Conclusion / count |
| --- | --- | --- | --- | --- |
| `40796992679` | CI FAST | `9cfc3f5` | 0 files | success; corpus parity 72/72 (`artifacts/function-council-r2/evidence/fast-run-40796992679/`) |
| `40797031704` | CI DEVICE | `9cfc3f5` | 0 files | 32/32 |
| `40800569636` | CI DEVICE | `9cfc3f5` | 0 files | 77/77 |
| `40805804385` | CI DEVICE | `7c05748` | 0 files | 90/90 |

## 2. Device coverage on this product tree after the closeout

70 instrumented classes have `@Test` methods. Before the closeout, 39 had a
run on this product tree. The closeout added 9 more (6 in `40979514270`, 3 in
`40983965648`), so **48 of 70** have now run on it. The other **22** have
only historical runs on older trees:
`CadA3VisualEvidenceTest`, `EditorWorkspaceArchitectureTest`,
`EditorWorkspaceChromeCompositionTest`, `EditorWorkspaceCompositionTest`,
`EditorWorkspaceControlsTest`, `EditorWorkspaceDisplayTest`,
`EditorWorkspaceFoundationTest`, `EditorWorkspaceGestureTest`,
`EditorWorkspaceHistoryTest`, `EditorWorkspaceLayoutTest`,
`EditorWorkspaceMotionTest`, `EditorWorkspaceObjectsTest`,
`EditorWorkspacePointerTest`, `EditorWorkspaceRightHostPlacementTest`,
`EditorWorkspaceSculptRetentionTest`, `EditorWorkspaceThemeTest`,
`EditorWorkspaceUnifiedRightHostTest`, `GlbImportExternalR1Test`,
`GlbImportPreviewTest`, `RendererCounterContinuityTest`,
`SelectionOutlineTest`, `Ui3dDimensionVisibilityTest`.

The union of bounded runs is **not** a full-suite result, and nothing here may
be cited as `FULL_SHARDED_SUITE_PASS`. The `uir4a04` find shows why: a class
not run for weeks can carry a stale assertion that only an aggregate would
reveal.

## 3. Not executed

| Item | Reason | Impact |
| --- | --- | --- |
| `-FullSharded` aggregate | not authorised | 22 classes unrun on this tree; whole-suite claims stay `REQUIRES-FULLSHARDED-OWNER-GATE` |
| CI FAST re-dispatch | not needed: product tree unchanged since the verified FAST `40796992679` | encoder parity 72/72 is historical (verified metadata), decoder verdicts are fresh |
| Local `pwsh` corpus builder | no `pwsh` in the container | covered by FAST above |
| Physical Android device, real stylus/hover, hardware GPU, ARM64 timing, thermals, TalkBack | no device | everything physical stays `OWNER-PHYSICAL-UNVERIFIED` |
| MSVC / Windows compile | no MSVC | no Windows claim (`DESKTOP_SCOPE_REVIEW.md`) |
| Device-side repro of CR-05 (recreation / SAF picker / process-death races) | needs new androidTest cases (forbidden here) or a physical device | those symptoms stay SOURCE-CONFIRMED-NOT-REPRODUCED or BLOCKED |
