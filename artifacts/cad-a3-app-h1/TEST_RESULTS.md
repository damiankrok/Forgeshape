# Test results - `CADA3-01..51` after `CAD-A3-C1`

`native` = the `CADA3-*` self-test (`forgeshape_cad_a3_selftest.cpp`, 57 checks,
the 19th suite at startup and in the standalone runner) or the project suite
(`CADA3-46..51`, 247 checks). `device` = `HomeFlowTest` / `SpatialSketchTest` /
`CadA3VisualEvidenceTest` on `emulator-5580` (`DEVICE_E2E.md`). `corpus` = the
independent PowerShell encoder (`CORPUS.md`). Nothing is marked deferred any
more.

| ID | Requirement | Proof | Result |
| --- | --- | --- | --- |
| CADA3-01 | cold launch shows Home, no random project | native `CADA3_BOOT_01/03`; device `E2E-APPH1-01` | PASS |
| CADA3-02 | Open File opens valid `.forge` | device `E2E-APPH1-05` (from Home, a dependency project) | PASS |
| CADA3-03 | Open cancellation stays safe | device `E2E-APPH1-06` | PASS |
| CADA3-04 | corrupt Open non-mutating | device `E2E-APPH1-07`; corpus `cad_bad_face_ref_v2` / `cad_dependency_cycle_v2` refused | PASS |
| CADA3-05 | New Project shows CAD/Sculpt | device `E2E-APPH1-02` | PASS |
| CADA3-06 | New Sculpt creates valid sculptable project | device `E2E-APPH1-04`; native `CADA3_BOOT_13` (seed records nothing) | PASS |
| CADA3-07 | New CAD enters spatial support chooser directly | device `E2E-APPH1-03` | PASS |
| CADA3-08 | XY plane picked through viewport | native `CADA3_08`; device `everyWorldPlaneIsSelectableSpatially` | PASS |
| CADA3-09 | XZ plane picked through viewport | device (same test), evidence capture 04 | PASS |
| CADA3-10 | YZ plane picked through viewport | device (same test), by-name fallback too | PASS |
| CADA3-11 | world-plane hover/press highlight | device `stylusHoverHighlightsATargetWithoutCommitting` (synthesized stylus hover through the real dispatch; hardware hover not claimed) and the aiming tap in every plane case | PASS |
| CADA3-12 | sketch camera exactly normal/orthographic | native `CADA3_12`; evidence capture 05 (`projection_mode=1`) | PASS |
| CADA3-13 | Finish Sketch returns sane 3D view | pose restore on end; evidence capture 09 | PASS |
| CADA3-14 | Cancel support selection no mutation | device `E2E-APPH1-11` (Back one step, nothing created) | PASS |
| CADA3-15 | New Sketch enters face-support chooser | device `faceSupportedSketch…` (tile lands in the chooser, cap aimed as a face) | PASS |
| CADA3-16..23 | semantic face tokens, frames, ranges, lineage | native `CADA3_16..23` (unchanged) | PASS |
| CADA3-24 | tap planar face starts Sketch | native `CADA3_24`; device cap and side taps | PASS |
| CADA3-25 | overlapping support disambiguation deterministic | nearest-hit rule; native picking | PASS |
| CADA3-26 | unsupported face refusal named/non-mutating | native `CADA3_26`; device `aCylindricalSideIsRefusedAsASupport` | PASS |
| CADA3-27 | face Sketch persists TopoRef | native `CADA3_41/42`; corpus round-trips | PASS |
| CADA3-28 | face Sketch → Extrude New Body | native `CADA3_28`; device | PASS |
| CADA3-29/30 | dependent Undo/Redo | native `CADA3_29/30` | PASS |
| CADA3-31/32 | parent size / depth edit preserves child | native `CADA3_31/32`; device `E2E-CADA3-16` (depth edit on device) | PASS |
| CADA3-33/34/35 | parent transform, child placement, chain | native `CADA3_33/34/35`; corpus `cad_face_chain_v2` | PASS |
| CADA3-36 | dependency cycle refused | native `CADA3_36`; corpus `cad_dependency_cycle_v2` refused `UnresolvedReference` (`CADA3_51`) | PASS |
| CADA3-37 | bad semantic face ref refused | native `CADA3_37`; corpus `cad_bad_face_ref_v2` refused `InvalidSemanticValue` (`CADA3_50`) | PASS |
| CADA3-38/39 | producer Delete refused / dependent then producer | native; device (three suites) | PASS |
| CADA3-40 | CADB v1 fixtures byte-identical | corpus: all sixteen v1 fixtures `OnDisk True`, digests unchanged; `CADA3_46` reads v1 off the files | PASS |
| CADA3-41/42 | CADB v2 cap / side support save/open | native round-trip; corpus `cad_face_sketch_cap_v2` / `_side_v2` byte-identical on both encoders (`CADA3_46/47`) | PASS |
| CADA3-43 | autosave/recovery dependency restore | fingerprint mixes support; device `E2E-APPH1-05` reopen | PASS |
| CADA3-44 | adaptive grid bounded readable increments | native `CADA3_44`; device grid test (two real pinches) | PASS |
| CADA3-45 | typed dimensions bypass grid snap | native `CADA3_45`; device (typed 1.2345 exact after a snapped drag) | PASS |
| CADA3-46 | dirty New/Open Save/Discard/Cancel guard | device `E2E-APPH1-08/09/10/11` | PASS |
| CADA3-47 | representation dispatch handles Cad explicitly | unchanged | PASS |
| CADA3-48 | forbidden scope absent | no boolean, custom plane, imported/sculpt-face sketch, CAD→Sculpt or independently movable dependent added | PASS |
| BOOT-01..13 | Home is not a project; the first commit | native `CADA3_BOOT_01..13` | PASS |
| CORPUS | independent v2 encoder, six fixtures, lineage as a format field | `CADA3_46..51`; `build-forge-corpus.ps1 -VerifyOnly` | PASS (after the FNV basis fix, `CORPUS.md`) |

## Suites and builds

| Gate | Result |
| --- | --- |
| Native self-tests, clean debug launch | 19/19 suites, all `*_SELFTEST_OK`, then `NATIVE_VIEWPORT_OK`, plus `FORGESHAPE_STARTUP_NO_PROJECT bodies=0` and `FORGESHAPE_PROJECT_GOLDEN_SHA256_CAD_V2` (`NATIVE_SELFTEST_CADA3.txt`) |
| Standalone runner (all 17 platform-neutral suites) | camera 119, picking 174, mesh 91, construction 100, transform 121, primitive 126, sphere 105, cone_capsule 163, sculpt 494, scene 79, history 147, gizmo 145, project 247, gltf_export 93, gltf_import 184, cad 122, cad_a3 57 — 2567 checks, 0 failed |
| Golden digests | eleven v1 digests unchanged; six v2 digests pinned, PowerShell = C++ |
| JVM (`:app:testDebugUnitTest`) | 70 / 70, 0 failures (three start-question cases became New-Project-chooser cases) |
| Debug + release APK | both built, `arm64-v8a` + `x86_64` |
| Device guards | `DEV2-01..07`, `DEV3-01..06` all PASS (the new evidence script included in the scan) |
| Corpus (`build-forge-corpus.ps1 -VerifyOnly`) | twenty-two fixtures `OnDisk = True` |
| Focused device suites | see `DEVICE_E2E.md` |
| Authoritative aggregate | `FULL_SHARDED_SUITE_PASS`: 37 classes / 504 tests / 5 shards (100 + 104 + 99 + 101 + 100), missing = duplicates = unexpected = execution_missing = 0 (`FULL_SHARDED.txt`) |
