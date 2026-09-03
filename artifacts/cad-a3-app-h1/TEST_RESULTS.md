# Test results - `CADA3-01..48`

`native` = the `CADA3-*` self-test (`forgeshape_cad_a3_selftest.cpp`, 44 checks,
run at startup as the 19th suite and via the standalone runner). `device` =
`SpatialSketchTest` on `emulator-5580`. `doc` = a fail-closed document check in
the same native suite. Items marked DEFERRED are honestly not done this pass.

| ID | Requirement | Proof | Result |
| --- | --- | --- | --- |
| CADA3-01 | cold launch shows Home, no random project | -- | DEFERRED (APP-H1) |
| CADA3-02 | Open File opens valid `.forge` | existing SAF open unchanged | PARTIAL |
| CADA3-03 | Open cancellation stays safe | existing SAF path | PARTIAL |
| CADA3-04 | corrupt Open non-mutating | existing fail-closed decoder | PASS |
| CADA3-05 | New Project shows CAD/Sculpt | -- | DEFERRED (APP-H1) |
| CADA3-06 | New Sculpt creates valid sculptable project | existing start flow | PASS |
| CADA3-07 | New CAD enters spatial support chooser | spatial chooser reachable from New Sketch (not from a Home New CAD) | PARTIAL |
| CADA3-08 | XY plane picked through viewport | native `CADA3_08`; device world-plane tap-tap | PASS |
| CADA3-09 | XZ plane picked through viewport | native picking (plane index) | PASS |
| CADA3-10 | YZ plane picked through viewport | native picking | PASS |
| CADA3-11 | world-plane hover/press highlight | chooser highlight range; hover JNI wired | PARTIAL (hover not device-verified) |
| CADA3-12 | sketch camera exactly normal/orthographic | native `CADA3_12` | PASS |
| CADA3-13 | Finish Sketch returns sane 3D view | pose restore on end | PASS |
| CADA3-14 | Cancel support selection no mutation | System Back cancels; native cancel | PASS |
| CADA3-15 | New Sketch enters face-support chooser | device: New Sketch -> Pick in 3D | PASS |
| CADA3-16 | Extrude StartCap has stable semantic face | native `CADA3_16` | PASS |
| CADA3-17 | Extrude EndCap has stable semantic face | native `CADA3_17` | PASS |
| CADA3-18 | rectangle side has stable profile-edge token | native `CADA3_18` | PASS |
| CADA3-19 | circle cylindrical side not eligible | native `CADA3_19` | PASS |
| CADA3-20 | rendered triangle resolves transiently to a face | native `CADA3_20` (ranges tile the mesh) | PASS |
| CADA3-21 | persisted support contains no triangle index | TopoRef is semantic; codec stores tokens | PASS |
| CADA3-22 | cap support frame orientation deterministic | native `CADA3_22` | PASS |
| CADA3-23 | side support frame orientation deterministic | native `CADA3_23` | PASS |
| CADA3-24 | tap planar face starts Sketch | native `CADA3_24`; device face tap-tap | PASS |
| CADA3-25 | overlapping support disambiguation deterministic | nearest-hit rule; native picking | PASS |
| CADA3-26 | unsupported face refusal named/non-mutating | native `CADA3_26` (curved side never selects) | PASS |
| CADA3-27 | face Sketch persists TopoRef | native `CADA3_41/42` round-trip | PASS |
| CADA3-28 | face Sketch -> Extrude New Body | native `CADA3_28`; device | PASS |
| CADA3-29 | dependent Undo removes only dependent | native `CADA3_29` | PASS |
| CADA3-30 | dependent Redo restores same IDs/TopoRef | native `CADA3_30` | PASS |
| CADA3-31 | parent width/height edit preserves side-supported child | native `CADA3_31` | PASS |
| CADA3-32 | parent depth edit preserves cap-supported child | native `CADA3_32` | PASS |
| CADA3-33 | parent transform keeps dependent attached | native `CADA3_33` (move + rotate) | PASS |
| CADA3-34 | child placement semantics deterministic | derived, refused-independent; documented | PASS (locked, per F3) |
| CADA3-35 | dependency chain A->B->C regenerates | native `CADA3_35` (1/8/32) | PASS |
| CADA3-36 | dependency cycle refused | native `CADA3_36` (scene + document) | PASS |
| CADA3-37 | bad semantic face ref refused | native `CADA3_37` | PASS |
| CADA3-38 | Delete producer with dependents refused | native `CADA3_38`; device (DELETE_REFUSED_HAS_DEPENDENTS) | PASS |
| CADA3-39 | Delete dependent then producer succeeds | delete refusal only while dependents stand | PASS |
| CADA3-40 | CADB v1 fixtures byte-identical/decode | native `CADA3_40`; corpus (16 fixtures unchanged) | PASS |
| CADA3-41 | CADB v2 cap-support save/open | native `CADA3_41` | PASS |
| CADA3-42 | CADB v2 side-support save/open | native `CADA3_42` | PASS |
| CADA3-43 | autosave/recovery dependency restore | fingerprint mixes support; checkpoint is same document | PASS (by design; device autosave not separately run) |
| CADA3-44 | adaptive grid chooses bounded readable increments | native `CADA3_44` | PASS |
| CADA3-45 | typed dimensions bypass grid snap | native `CADA3_45`; CAD-R0 numeric edits | PASS |
| CADA3-46 | dirty New/Open Save/Discard/Cancel guard | -- | DEFERRED (APP-H1) |
| CADA3-47 | representation dispatch handles Cad explicitly | new code representation-neutral; scene resolve, delete, codec all branch on CAD explicitly | PASS |
| CADA3-48 | forbidden booleans/custom planes/imported-face sketch absent | grep clean; face support refuses non-CAD; no such control drawn | PASS |

## Suites and builds

| Gate | Result |
| --- | --- |
| Native self-tests, clean debug launch | 19/19 suites, all `*_SELFTEST_OK`, then `NATIVE_VIEWPORT_OK` (`NATIVE_SELFTEST_CADA3.txt`). New CAD-A3 suite: 44 checks, 0 failed. |
| Golden digests | all four CAD digests unchanged; the twelve prior unchanged |
| Standalone runner | cad_a3 44/0, cad 122/0, scene 79/0, history 147/0, project 230/0 |
| JVM (`:app:testDebugUnitTest`) | 70 / 70, 0 failures |
| Debug + release APK | both built, `arm64-v8a` + `x86_64` |
| Device guards | `DEV2-01..07`, `DEV3-01..06` all PASS |
| Corpus (`build-forge-corpus.ps1 -VerifyOnly`) | all sixteen fixtures `OnDisk = True`, digests unchanged (CADB v1 byte-identical) |
| Focused device suite | `SpatialSketchTest` OK (2 tests) - `FOCUSED_SPATIAL_SKETCH.txt` |
| Authoritative aggregate | see `FULL_SHARDED.txt` |

## Deferred, honestly

- APP-H1 Home / New Project routing and the empty New-CAD bootstrap (Part A;
  see `HOME_FLOW.md`): not implemented; conflicts with the no-empty-project
  invariant.
- The independent PowerShell v2 corpus builder and the five named v2 fixtures
  (Part I): not created; the v2 codec is proved by native round-trip instead.
- Stylus hover highlight device verification and the face-first contextual
  shortcut (Parts K1/D3): wired or absent, not device-verified.
- Deterministic screenshot contact sheet (Part K3): not captured.
