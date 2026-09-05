# Test and build results — DEEP-AUDIT-R1

All on the final tree `d783d4b`, device `emulator-5580` = `ForgeShape_Stage006` (confirmed by `emu avd name`); `emulator-5554` never contacted; `logcat -G 64M` set before capture.

## Native self-tests (device launch)

20 suites, **2981 checks, 0 failures**, then `FORGESHAPE_NATIVE_VIEWPORT_OK` (first frame presented). Full list in `DEVICE_STARTUP.txt`. New this audit: `FORGESHAPE_SKETCH_UX_SELFTEST_OK (52)` includes `DAR1_01` ×2; `FORGESHAPE_PROJECT_SELFTEST_OK (251)` includes `DAR1_02` ×4; `FORGESHAPE_GLTF_IMPORT_SELFTEST_OK (189)` includes `DAR1_03` ×5. `FORGESHAPE_STARTUP_NO_PROJECT bodies=0` (Home). All five golden-digest lines (`GOLDEN_SHA256`, `…_IMPORTED`, `…_IMPORTED_SCULPT`, `…_CAD`, `…_CAD_V2`) match the corpus.

## Standalone native runner (NDK clang, `emulator-5580`)

18 platform-neutral suites, **2628 checks, 0 failures** (`STANDALONE_RUNNER.txt`). The same runner built against the PRE-FIX production files fails the seven new checks (`STANDALONE_RUNNER_RED.txt`): `DAR1_01` ×2, `DAR1_02` ×2 reachable without the render-shading suite (the 4-check group reduces to 2 in the runner harness), `DAR1_03` ×3. This is the red/green proof that the three fixes are load-bearing.

## JVM suite

`:app:testDebugUnitTest` — **70/70, 0 failures** (8 classes).

## Builds

debug + release (unsigned), both ABIs, all four `.so`s BUILD SUCCESSFUL. 16 KB `LOAD` alignment 0x4000 on every one. Sizes in `BASELINE.md`/`BUILD_RELEASE_ABI.md`.

## Device guards

`scripts\verify-device-guards.ps1`: **`DEV2-01..07` and `DEV3-01..06` all PASS** (`DEVICE_GUARDS.txt`), no device contacted.

## `.forge` corpus

`scripts\build-forge-corpus.ps1 -VerifyOnly`: all 28 fixture digests match the device print and the committed spec (`CORPUS_VERIFYONLY.txt`). No fixture changed — the fingerprint fix is not serialized.

## Representative workflows (device launch + suites)

Home (no project) → New CAD → immediate XY sketch → navigator plane/flip/rotate → line/arc/spline → line dimension typed → Extrude (first body + project) → Edit Sketch → Finish (one Undo): covered by `HomeFlowTest`, `SketchExtrudeTest`, `SpatialSketchTest`, `SketchUxTest`. Save/Open/recovery: `ProjectAutosaveRecoveryTest`, `ProjectProcessDeathTest`. Sculpt/Import/Delete: `SculptUndoTest`, `ImportedMeshDurableTest`, `ImportedMeshSculptTest`, `ObjectsDeleteTest`, `GlbExportTest`, `GlbImportExternalR1Test`. All are in the authoritative aggregate below.

## Authoritative aggregate (`-FullSharded`)

See `FULL_SHARDED.txt`. Discovery 39 classes / 519 tests, missing=0 duplicates=0 unexpected=0; 5 deterministic shards; every shard PASS with its exact expected count; `FULL_SHARDED_SUITE_PASS` emitted on the final runtime/test tree. Run once from shard 1 on `d783d4b` after all runtime and test files were final.

## Optional analyses

Lint / sanitizer / static analysis: NOT AVAILABLE (see `AUDIT_COVERAGE_MATRIX.md`) — none claimed PASS.
