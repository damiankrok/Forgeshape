# FUNCTION-COUNCIL-R2 — TESTS (executed vs not executed, raw records)

Audited product tree: `70a6c14` (= `f3db953` for `app/src/main`). The audit
branch adds only `artifacts/function-council-r2/**`; every CI run below ran on
an audit-branch head whose product, test, script and workflow trees are
byte-identical to `70a6c14`. Raw records live under `evidence/`.

## 1. Executed fresh in this audit

### 1.1 Local (Linux container, x86_64, NDK 29.0.14206865)

| # | Command | Result | Record |
| --- | --- | --- | --- |
| L1 | `bash scripts/host-native-selftests.sh` | **`HOST_SELFTESTS_OK (4392 checks, 0 failed)`** — 25/25 `HOST_SUITE_OK` lines; `CAD_R2_ORACLE_PASS=17` | `evidence/local/host_selftests.txt` |
| L2 | `bash scripts/host-forge-corpus-verdicts.sh` | `CORPUS_FIXTURES 72`, every fixture decoded to a verdict (valid ones `Ok`, refusal fixtures refused by name, e.g. `UnknownRequiredSection`, `UnsupportedMajor`, `Truncated`) | `evidence/local/host_corpus_verdicts.txt` |
| L3 | `./gradlew :app:testDebugUnitTest :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease` | BUILD SUCCESSFUL (124 tasks; the assemble tasks were up to date from the identical C2 tree) | `evidence/local/gradle_build_tail.txt` |
| L4 | `./gradlew :app:testDebugUnitTest --rerun-tasks` (forces a fresh JVM run) | **196 tests, 0 skipped, 0 failures, 0 errors** over 20 result files (22 tasks executed) | `evidence/local/gradle_unit_rerun_tail.txt` |
| L5 | `bash scripts/ci-release-selftest-guard.sh` | first attempt `ERROR ANDROID_HOME/ANDROID_SDK_ROOT is not set` (environment, `run_status.txt` `guard_exit=2`); rerun with `ANDROID_HOME=/root/android-sdk` → **`RELEASE_SELFTEST_GUARD=PASS`**: release x86_64 3,256,192 B / arm64-v8a 2,916,104 B with 0 `SelfTests` symbols and 0 self-test strings; debug 35 / 398 on both ABIs | `evidence/local/release_guard.txt` |
| L6 | Seat host diagnostics (scratch, never built into the product) | per seat: commands and outputs recorded in `SEAT_*.md` (E1 rows) | `SEAT_B/C/D/E/G/H/J.md` |

### 1.2 GitHub Actions (dispatched by this audit; emulator, never physical)

| Run | Workflow | Head | Scope | Result | Artifact (id, digest) |
| --- | --- | --- | --- | --- | --- |
| `40796992679` | CI FAST | `9cfc3f5` | build, JVM, debug + androidTest + release APKs, whitespace, release guard, `.forge` corpus parity (pwsh builder), device-free runner guards | **success**; `FORGE_CORPUS_PARITY=PASS (72/72 byte-identical)`, `RELEASE_SELFTEST_GUARD=PASS`, 20 JVM result files | `12863779125`, `sha256:7f2d7168…3afb` (downloaded ZIP hash equal) |
| `40797031704` | CI DEVICE | `9cfc3f5` | `Ui3dStateAuditTest`, `SketchUxVisualEvidenceTest`, `UiPrefVisualEvidenceTest`, `SelectionOutlineVisualEvidenceTest`, `EditorWorkspaceLegibilityTest` | **32/32**, 0 skipped, 667 s; 104 PNG kept | `12865436338`, `sha256:49ec9a06…0053` |
| `40800569636` | CI DEVICE | `9cfc3f5` | `SurfaceOwnerTest`, `FreeformOwnerTest`, `Stage027SculptWorkflowTest`, `SculptHistoryNavigatorTest`, `SculptUndoTest`, `SculptBrushStage025Test`, `ImportGlbSculptGuardTest`, `ImportedMeshSculptTest`, `CadVerticalSliceTest`, `CadExtrudeExtentTest`, `SketchUxTest` | **77/77**, 0 skipped, 1972 s; 40 PNG kept | `12868000918`, `sha256:f729d344…930f` |
| `40805804385` | CI DEVICE | `7c05748` | `ProjectAutosaveRecoveryTest`, `ProjectTransferTest`, `ProjectProcessDeathTest`, `ImportedMeshDurableTest`, `ObjectsDeleteTest`, `ObjectCommandsTest`, `MirrorSmokeTest`, `BodyDimensionsSmokeTest`, `EditorWorkspaceLifecycleTest`, `JniBoundaryHardeningTest`, `DiagnosticsAndRendererLossTest`, `SettingsPreferencesTest` | **90/90**, 0 skipped, 1892 s | `12870141144`, `sha256:103f0189…f2cf4` |

Every DEVICE run: `system-images;android-36;google_apis;x86_64`,
`medium_phone`, `swiftshader_indirect` (Vulkan through ranchu), emulator
37.2.12.0, AVD name confirmed `ForgeShape_CI_API36` on `emulator-5580`,
**25/25 startup self-test tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure
token lines** (`evidence/device-run-*/summary.json`,
`selftest-tokens-found.txt`, `environment-manifest.txt`,
`instrumentation-junit.xml`). Fresh device total: **199/199** across 28
classes. Screenshots are copied into `evidence/device-run-*/screens/` with
`SHA256SUMS.txt` (the GitHub artifacts expire 2026-11-12).

APK note: the CI FAST debug APK of this audit is 17,582,551 bytes, SHA-256
`824cc1ff…acb6`; C2's FAST APK (same product tree) is the same size with a
different hash (`262076926e…be9e`). The debug build is not byte-reproducible
across CI runs; this audit makes no claim about the APK beyond that.

## 2. Historical evidence relied on (not re-executed here)

| Record | What it proves | Used as |
| --- | --- | --- |
| C2 DEVICE `40776968165` (59/59, `CadRetainedSketchOwnerTest` D01..D22 + `EditorWorkspaceCorrectionTest`) and `40782861799` (67/67, nine CAD/Home/project-action classes), product tree identical | CAD retained-sketch, chooser, fill-pick, HUD, Revolve, drafting, history journeys | E3 |
| C1 DEVICE `40752389411`, `40760227555`; R2 `40683528533`, `40689211847` | earlier trees | E3, superseded where re-run |
| `FULL_SHARDED` aggregates on older trees (e.g. `36607747079` on `fa0b6b4`) | exhaustive suite on an older tree only | E3, never cited as current |

## 3. Not executed (with reason and impact)

| Item | Reason | Impact | Next step |
| --- | --- | --- | --- |
| `-FullSharded` aggregate | Not authorised by the prompt; no general permission | Of the 70 instrumented classes with `@Test` methods, 39 ran on this product tree (28 fresh here + 11 in C2's two runs); the other **31** (§4) have only historical (E3) runs on older trees; `REQUIRES-FULLSHARDED-OWNER-GATE` for any claim of the whole suite | OWNER gate (see `OWNER_DECISIONS_REQUIRED.md`) |
| Local corpus builder (`scripts/build-forge-corpus.ps1`) | No `pwsh` in the container | none — CI FAST ran it on Linux `pwsh` (72/72) | — |
| Physical Android device, real stylus/pressure/hover, hardware GPU, 16 KB pages, thermals, TalkBack with a real user | No device; emulator only | All such items `OWNER-PHYSICAL-UNVERIFIED` | OWNER physical review |
| Font scale 1.3/1.5, landscape, tablet/foldable sizes in this audit's runs | The bounded runs used the stock `medium_phone` profile; font-scale cases exist in `CadRetainedSketchOwnerTest` (C1/C2 E3) but were not re-run here | UI matrix rows for those configurations are E3 or UNKNOWN | focused CI DEVICE run, or OWNER device |
| MSVC / Windows compile of the domain | Not available | Desktop portability claims for MSVC are E5 | `DESKTOP_POC_GATE.md` |
| Vulkan validation layers | Not fetched (session-scoped tool per `CLAUDE.md`) | renderer hazard hypotheses stay hypotheses | separate debug session |

## 4. Instrumented classes with no run on this product tree (31 of 70)

`CadA3VisualEvidenceTest`, `CadCanvasExtrudeTest`,
`CadPlanarFaceOwnerCorrectionTest`, `CadPlanarFaceRuntimeTest`,
`EditorWorkspaceArchitectureTest`, `EditorWorkspaceChromeCompositionTest`,
`EditorWorkspaceCompositionTest`, `EditorWorkspaceControlsTest`,
`EditorWorkspaceDisplayTest`, `EditorWorkspaceFoundationTest`,
`EditorWorkspaceGestureTest`, `EditorWorkspaceGizmoTest`,
`EditorWorkspaceHistoryTest`, `EditorWorkspaceLayoutTest`,
`EditorWorkspaceMobileTest`, `EditorWorkspaceMotionTest`,
`EditorWorkspaceObjectsTest`, `EditorWorkspacePointerTest`,
`EditorWorkspaceRightHostPlacementTest`, `EditorWorkspaceSculptRetentionTest`,
`EditorWorkspaceThemeTest`, `EditorWorkspaceUnifiedRightHostTest`,
`GlbExportTest`, `GlbImportExternalR1Test`, `GlbImportPreviewTest`,
`RendererCounterContinuityTest`, `SelectionOutlineTest`, `SketchExtrudeTest`,
`SpatialSketchTest`, `Ui3dDimensionVisibilityTest`, `Ui3dStateCorrectionTest`.

Computed by listing every `app/src/androidTest/**/*.java` with at least one
`@Test` and subtracting the 39 classes named in the five run lists above.
