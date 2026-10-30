# CAD-FACE-SUPPORTED-PLACEMENT-R1 — TESTS (every attempt, failed and passed)

**Product tree.** Frozen at `912b861` (the GREEN commit) for everything below.
Every later commit touches only `app/src/androidTest`, `artifacts/` and docs.
`git diff --name-only 912b861 HEAD -- app/src/main scripts testdata .github
DATA_PACKAGE_SPEC.md gradle build.gradle app/build.gradle` returns nothing.

**Runs and gates.**
- Every run here is on an emulator; none is physical-device evidence.
- No FullSharded run (OD-03 not authorised).
- Nothing was merged.

## 1. RED (on the unchanged product)

| What | Where | Result |
| --- | --- | --- |
| Host self-tests with the new checks on the base product (RED commit `8f72535`) | `evidence/red/host_selftests_RED.txt` | `HOST_SELFTESTS_FAIL (4423 checks, 18 failed)`: GLTF_EXPORT 14/115 (E01×3, E02×3, E03×3, E04×2, E06×1, E07×2), GIZMO 4/185 (T02×4) |
| Typed door (closeout CR-02 reproducer, process-scoped objects, base tree) | `evidence/red/cr02_typed_and_gizmo_RED.txt` | C1: `undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=0`, junk saved and decoded. C2: gizmo drawn at the origin 3 m from the body, drag recorded |
| `uir4a04` | closeout `CI DEVICE 40983965648` | `expected:<8> but was:<12>` |
| E05 corpus | closeout `diagnostics/CR-01` | 11 MISMATCH (every face-supported body), cap and side GLBs byte-identical |

## 2. GREEN — local (Linux x86_64, NDK 29.0.14206865)

Every exit code is recorded in `evidence/local/run_status.txt`.

| Command | Result |
| --- | --- |
| `bash scripts/host-native-selftests.sh` (first GREEN) | `evidence/host_selftests_GREEN_1.txt`: `HOST_SELFTESTS_OK (4434 checks, 0 failed)` |
| same, final | `HOST_SELFTESTS_OK (4434 checks, 0 failed)`, 25/25 suites, `CAD_R2_ORACLE_PASS=17`; GLTF_EXPORT 115, GIZMO 185, CAD_A3 77, SCENE 175, PROJECT 256 (unchanged: no check added) |
| `bash scripts/host-forge-corpus-verdicts.sh` | `CORPUS_FIXTURES 72` (decoder verdicts unchanged) |
| `./gradlew :app:testDebugUnitTest --rerun-tasks` | 196 tests, 0 skipped, 0 failures, 0 errors |
| `./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease` | exit 0 |
| `bash scripts/ci-release-selftest-guard.sh` | `RELEASE_SELFTEST_GUARD=PASS` |
| `git diff --check` | clean |
| E05: closeout CR-01 diagnostic re-run on this product | 0 MISMATCH / 23 MATCH (+1 Imported Mesh NO_SOURCE, translation equal); independent-only GLBs keep their sha256 (`evidence/e05-corpus`) |
| Host export benchmark, ABBA against `7c2fc91` | `evidence/perf` (see SUMMARY §Performance) |

**Device recorder capacity.** Each changed suite carries a check that it fits
its device recorder:

| Suite | Checks | Device recorder |
| --- | --- | --- |
| GLTF_EXPORT | 115 | 128 |
| GIZMO | 185 | 256 |
| CAD_A3 | 77 | 128 |

## 3. CI DEVICE (dispatched sequentially; API 36 x86_64 emulator, swiftshader)

| Run | Head | Classes | Result | Time | Artifact |
| --- | --- | --- | --- | --- | --- |
| `41078310302` | `71c8333` | `CadFaceSupportedPlacementTest` 2, `GlbExportTest` 24, `EditorWorkspaceGizmoTest` 46, `EditorWorkspaceMobileTest` 15 | **86/87 — FAIL** | 1810 s | `12991615050` (`sha256:810d9448…d5c8`) |
| `41084823234` | `f997032` | `CadFaceSupportedPlacementTest` 2, `CadPlanarFaceRuntimeTest` 6, `CadPlanarFaceOwnerCorrectionTest` 5, `CadRetainedSketchOwnerTest` 22, `SpatialSketchTest` 8, `MirrorSmokeTest` 1, `ObjectCommandsTest` 1, `OwnerProjectShellSketchSupportTest` 5 | **50/50 PASS**, 0 skipped | 1645 s | `12992928070` (`sha256:777b191d…7a8d`) |
| `41090517111` | `f997032` | `GlbExportTest` 24, `EditorWorkspaceGizmoTest` 46, `EditorWorkspaceMobileTest` 15, `ImportedMeshDurableTest` 12, `HomeFlowTest` 12 | **109/109 PASS**, 0 skipped | 2221 s | `12996588088` (`sha256:5a1a69a9…cb01`) |

**Startup checks.** Every run boots with 25/25 self-test tokens in order,
`NATIVE_VIEWPORT_OK` and 0 failure-token lines.
- In `41090517111` the first two startup captures each read 24/25, with
  `liblog_dropped` 117 and 17: the log ring buffer dropped a line, the case
  CLAUDE.md warns about.
- The CI script's third capture read 25/25 in order. That is the capture the
  run used.

**The `41078310302` failure.**
`CadFaceSupportedPlacementTest#aFaceSupportedBodyIsNotTransformableAndExportsOnItsFace`
failed with `world AABB[0] [-0.4, -0.4, 2.0, 0.4, 0.4, 2.5] expected:<-0.5>`.
- **It was a test assumption, not a product fault.** The dependent was
  exported on the far cap (z [2.0, 2.5]), but 0.8 wide: the 1.0 drag on the
  face sketch snaps to the view-adaptive grid.
- **Fix, test only (`f997032`).** The footprint is now measured. The test
  asserts placement:
  - the base lies exactly on the cap, centred in it and inside it;
  - after a typed producer move and after save/reopen, the box is exactly
    the first box shifted by the move.
- **Rerun.** `41084823234` passed. It measured producer
  `[-1,-1,0]..[1,1,2]` and dependent `[-0.4,-0.4,2]..[0.4,0.4,2.5]`.
- **Retest.** The other 86 tests of that run passed, `uir4a04` included. All
  three of those classes ran again green in `41090517111` on the final test
  APK.

**Classes on the final test APK (`f997032`), each green at least once:**
- `CadFaceSupportedPlacementTest` (2: E2E and orientation × font 1.0/1.5)
- `GlbExportTest`
- `EditorWorkspaceGizmoTest`
- `EditorWorkspaceMobileTest`
- `CadPlanarFaceRuntimeTest`
- `CadPlanarFaceOwnerCorrectionTest`
- `CadRetainedSketchOwnerTest`
- `SpatialSketchTest`
- `MirrorSmokeTest`
- `ObjectCommandsTest`
- `OwnerProjectShellSketchSupportTest`
- `ImportedMeshDurableTest`
- `HomeFlowTest`

That is 13 classes and 159 tests, with no failures on that APK. Together they
are a union of bounded runs, **not** a full-suite result.

Job logs: `evidence/device-run-*/job.log`.

## 4. CI FAST (last, on the frozen product)

### Attempt 1: `41097660891` on `ce1a375` — FAILURE

The job stopped at its first gate, `git diff --check <main merge-base> HEAD`,
and never reached the build.
- **What it flagged.** Trailing whitespace in raw evidence: what this branch
  inherited from the two audit branches, plus this stage's two CI job logs.
- **Not this stage's fault.** The base `7c2fc91` fails the same check, while
  C2 `70a6c14` passes.
- **Fix (`7ee9fa0`).** A `.gitattributes` with `-whitespace` on
  `artifacts/**/{evidence,diagnostics,sources}/**`. No evidence byte was
  rewritten, and product, tests, scripts and docs stay checked.
- **Log:** `evidence/fast-run-41097660891/job.log`.

### Attempt 2: `41098028076` on `7ee9fa0` (product = `912b861`) — SUCCESS

| Gate | Result |
| --- | --- |
| Whitespace check | clean |
| Build | `BUILD SUCCESSFUL` |
| JVM tests | 196 / 0 skipped / 0 failed (from the artifact's XML) |
| Release self-test guard | `RELEASE_SELFTEST_GUARD=PASS` |
| Corpus parity | `FORGE_CORPUS_PARITY=PASS (72/72 byte-identical)` |
| Runtime and device guards | `TESTRUNTIME-01..12` and the device guards PASS |

The artifact and APK digests are in `evidence/fast-run-41098028076/ARTIFACT.txt`.

## 5. Not executed

| Item | Reason |
| --- | --- |
| `-FullSharded` / CI FULL SHARDED | forbidden without a new OD-03 authorisation |
| Physical device, stylus, hover, TalkBack, ARM64 timing | no device: `OWNER-PHYSICAL-UNVERIFIED`, see `OWNER_LATER_TEST_PACK.md` |
| Local `pwsh` corpus builder parity | no `pwsh` in the container; covered by CI FAST's corpus parity |
| The 58 of 71 instrumented classes not in the runs above | bounded scope; product change touches export, transform and gizmo only |
