# SKETCH-NAVIGATOR-OUTCOME-R1 — TESTS (every attempt)

## Branch

`feature/sketch-navigator-outcome-r1` from `8a797ff0f57af0b01caf5cc7608c09209273a71a`.

| Commit | What |
| --- | --- |
| `d17adef` | RED: BEFORE, the device test, the CR-06 host re-run. Product unchanged. |
| `a746d29` | GREEN: product + host checks |
| `28cb315` | Device-test fix (nav06 targets). Product unchanged since `a746d29`. |
| `1c85498`, `c527967`, `3b60086`, `1b950a4`, … | docs / evidence only |

The product and test tree is frozen at `28cb315`.
`git diff --name-only 28cb315 HEAD -- app scripts testdata .github gradle build.gradle`
returns nothing.

All runs are on the emulator. There was no FullSharded run, nothing was merged
and no PR was opened.

## RED

| What | Result |
| --- | --- |
| CR-06 native diagnostic on `8a797ff` | Flip, +90 and −90 each leave the view identical to B0 (see `RED.md`) |
| `CI DEVICE` `41507903710` on `d17adef` | `SketchNavigatorCameraTest` **7/9 FAIL**, all product-caused. Startup 25/25 in order. nav01 and nav09 pass (already-correct behaviour). |
| Host mutations (GREEN tree) | the camera on `frame()` fails 6; Back keeping flip/roll fails 1; face flip allowed fails 2 |

## Local, GREEN (Linux x86_64, NDK 29.0.14206865)

Exit codes are in `evidence/local/run_status.txt`.

| Command | Result |
| --- | --- |
| `bash scripts/host-native-selftests.sh SKETCH_UX` (targeted, first) | 136/136 |
| `bash scripts/host-native-selftests.sh` | `HOST_SELFTESTS_OK (4472 checks, 0 failed)`; 25/25 `HOST_SUITE_OK`; `CAD_R2_ORACLE_PASS=17`; SKETCH_UX 136 (was 118; device cap 256); PROJECT 256 (unchanged, nothing added) |
| `./gradlew :app:testDebugUnitTest --rerun-tasks` | 199 / 0 skipped / 0 failures / 0 errors. No Java logic changed, so no JVM test was added. |
| `./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease` | exit 0 |
| `bash scripts/ci-release-selftest-guard.sh` | `RELEASE_SELFTEST_GUARD=PASS` |
| `bash scripts/host-forge-corpus-verdicts.sh` | `CORPUS_FIXTURES 72` |
| `git diff --check` | clean |

## CI DEVICE (API 36 x86_64 emulator, swiftshader)

| Run | Head | Classes (new class first) | Result | Time | Artifact |
| --- | --- | --- | --- | --- | --- |
| `41507903710` | `d17adef` | `SketchNavigatorCameraTest` 9 | **2/9: expected RED** | 212 s | `13173151620` |
| `41509947297` | `a746d29` | `SketchNavigatorCameraTest` 9, `SketchUxTest` 14, `SpatialSketchTest` 8, `SketchExtrudeTest` 9, `CadFaceSupportedPlacementTest` 2, `ImportGlbSketchGuardTest` 5, `CadRetainedSketchOwnerTest` 22 | **68/69: FAIL**, test-harness assumption | 2099 s | `13175821326` |
| `41517008402` | `28cb315` | same 7 classes, 69 tests | **69/69 PASS**, 0 failed, none `@Ignore`d | 2097 s | `13178083933` (`sha256:f6a30019…d752`) |

**The `41509947297` failure** (`nav06`): the line drawn after +90 and Flip
landed at (0.4,0.2)–(1.4,1.0) for targets (0.5,0.25)–(1.5,1.0). Each end
moved to the 0.2 m view-adaptive snap grid, within 0.1 m of the touch:
correct placement, and a mapping error would be metres off. The test
assumed no snapping.
- **Fixed in the test only (`28cb315`):** the drag now goes between the
  whole-metre points (1,0)–(2,1). The tolerance is unchanged.
- **Result in `41517008402`:** exactly (1.0, 0.0)–(2.0, 1.0).

**Startup.**
- `41507903710` and `41509947297`: 25/25 on the first capture.
- `41517008402`: the first capture read 23/25 (`liblog_dropped=184`, the log
  ring buffer dropped lines). The script's second capture read 25/25 in order,
  `liblog_dropped=0`, and that is the evidence the run used.
- `NATIVE_VIEWPORT_OK` and 0 failure lines in all three.

## CI FAST (last, frozen tree)

`41524092732` on `3b60086` (product/test = `28cb315`): **success**. Details are
in `evidence/fast-run-41524092732/ARTIFACT.txt`:
- release guard on x86_64 and arm64-v8a;
- corpus parity 72/72;
- `TESTRUNTIME-01..26` PASS;
- JVM 199/0;
- the ZIP digest matches GitHub's;
- `app-debug.apk` 17755951 bytes, SHA-256
  `f8cbb67de8e1b0541ebf4d5cbd79d203eb82689d6456c195fc93022c0aaf4194`.

## Not executed

| Item | Reason |
| --- | --- |
| FullSharded | OD-03 not authorised |
| Physical device, stylus, hover, TalkBack, ARM64 timing, MSVC / desktop | no such target here: UNVERIFIED |
| Comparative performance timing | no per-frame path changed: the framing runs only when a view is (re)installed |
