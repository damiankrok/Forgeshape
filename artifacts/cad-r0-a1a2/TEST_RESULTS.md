# Test results - `CADR0-01..40`

## Where each requirement is proved

`native` = the CAD self-test (`forgeshape_cad_selftest.cpp`, 122 checks, all
named `CADR0_<nn>_*`), which builds its own sketches, scenes, histories and
camera. `project` = the project self-test (230 checks). `device` =
`SketchExtrudeTest` on `emulator-5580`. `corpus` = the independent PowerShell
encoder against the committed fixtures.

| ID | Requirement | Proof | Result |
| --- | --- | --- | --- |
| CADR0-01 | XY workplane mapping | native `CADR0_01_*` (3 checks: right-handed frame, exact axes, U right / V up on screen) | PASS |
| CADR0-02 | XZ workplane mapping | native `CADR0_02_*` (2) | PASS |
| CADR0-03 | YZ workplane mapping | native `CADR0_03_*` (2) | PASS |
| CADR0-04 | Line create/validate | native `CADR0_04_*` (6: mint, zero length, NaN, range, replace keeps id, low allocator refused; a tap with the Line tool places nothing) | PASS |
| CADR0-05 | Polyline create/validate | native `CADR0_05_*` (6, incl. a tapped polyline closing on its first vertex and a tool change ending one open) | PASS |
| CADR0-06 | Rectangle create/parameter validation | native `CADR0_06_*` (3) | PASS |
| CADR0-07 | Circle create/parameter validation | native `CADR0_07_*` (4, incl. a dragged circle) | PASS |
| CADR0-08 | Endpoint snapping | native `CADR0_08_*` (3): a line end snaps to a rectangle's OFF-GRID corner exactly, with the snap kind reported as Endpoint | PASS |
| CADR0-09 | Grid snapping | native `CADR0_09_*` (2): a dragged rectangle lands on exact 0.25 m multiples; an off-grid drag lands on the grid | PASS |
| CADR0-10 | Sketch entity selection | native `CADR0_10_*` (3): tap an edge, tap empty space, tap a circle's ring | PASS |
| CADR0-11 | Sketch entity Delete | native `CADR0_11_*` (2); device E2E-CADR0-14 | PASS |
| CADR0-12 | Numeric rectangle edit | native `CADR0_12_*` (5: applied, refused atomically, identical unchanged, wrong kind refused, typed exact never snapped); device E2E-CADR0-09 | PASS |
| CADR0-13 | Numeric circle edit | native `CADR0_13_*` (1); device E2E-CADR0-12 | PASS |
| CADR0-14 | Cancelled sketch leaves project unchanged | native `CADR0_14_*` (8: inactive refusals, regeneration deterministic, state that does not regenerate refused whole, second finger cancels, overlay cached, extrude preview, back to editing, cancel leaves fingerprint and scene untouched, camera pose restored); device E2E-CADR0-14 | PASS |
| CADR0-15 | Open profile refused | native `CADR0_15_*` (6: open polyline, open chain, forked chain, no closed profile refuses regeneration, an open sketch and an empty sketch refuse Finish by name); device E2E-CADR0-14 | PASS |
| CADR0-16 | Closed rectangle profile found | native `CADR0_16_*` | PASS |
| CADR0-17 | Closed circle profile found | native `CADR0_17_*` | PASS |
| CADR0-18 | Closed polyline profile found | native `CADR0_18_*` (4: closed flag, closed by snap, line chain anchored by lowest id, chain order and direction irrelevant) | PASS |
| CADR0-19 | Self-intersection refused | native `CADR0_19_*` (2) | PASS |
| CADR0-20 | Zero-area profile refused | native `CADR0_20_*` | PASS |
| CADR0-21 | Multiple profiles deterministic/selectable | native `CADR0_21_*` (7: ordered by anchor, no guess, nested outer refused / inner kept, overlapping kept, finish leaves the choice open, ambiguous commit refused, profile chosen by anchor) | PASS |
| CADR0-22 | Simple polygon triangulation deterministic | native `CADR0_22_*` (4, incl. the four performance sizes watertight) | PASS |
| CADR0-23 | Circle tessellation deterministic | native `CADR0_23_*` | PASS |
| CADR0-24 | Rectangle Extrude watertight | native `CADR0_24_*` | PASS |
| CADR0-25 | Circle Extrude watertight | native `CADR0_25_*` | PASS |
| CADR0-26 | Generic closed polyline Extrude watertight | native `CADR0_26_*` (2: a 7-gon, and a concave L by signed volume) | PASS |
| CADR0-27 | Correct winding/normals | native `CADR0_27_*` (4: canonical winding and extents, against-normal, XZ and YZ, invalid depth) | PASS |
| CADR0-28 | Create CAD body = one project history step | native `CADR0_28_*` (4); device E2E-CADR0-05/08 | PASS |
| CADR0-29 | Undo/Redo CAD body create | native `CADR0_29_*` (3); device E2E-CADR0-07/08 | PASS |
| CADR0-30 | Edit extrude depth + Undo/Redo | native `CADR0_30_*` (5); device E2E-CADR0-10 | PASS |
| CADR0-31 | Edit sketch dimension + Undo/Redo | native `CADR0_31_*` (3); device E2E-CADR0-09 | PASS |
| CADR0-32 | SceneObject transform survives CAD parameter edit | native `CADR0_32_*`; device E2E-CADR0-15 | PASS |
| CADR0-33 | Save/Open rectangle CAD body | native `CADR0_33_*` (4); project `CADR0_33_*` (5: rectangle and mixed fixtures encode, round-trip bit for bit, match the committed digests); corpus; device E2E-CADR0-11 | PASS |
| CADR0-34 | Save/Open circle CAD body | native `CADR0_34_*`; project `CADR0_34_*` (3); corpus | PASS |
| CADR0-35 | Autosave/recovery CAD body | native `CADR0_35_*` (the fingerprint follows a CAD creation and edit and returns on undo); device E2E-CADR0-11 (fingerprint and validated bytes); the checkpoint is the same document | PASS |
| CADR0-36 | Corrupt CAD persistence fails closed | native `CADR0_36_*` (8: open profile document refused at encode, zero depth, CONS+CADB claim, SCUL over CADB, bad plane code at decode with CRC repaired, truncated section, header flag announced, legacy project carries no CADB); project `CADR0_36_*` (2, the bad-plane fixture semantically refused and digest-pinned); corpus | PASS |
| CADR0-37 | Export uses regenerated current mesh | native `CADR0_37_*` (3: the export carries the regenerated mesh, reflects an edited depth WITHOUT a republish, encodes to GLB) | PASS |
| CADR0-38 | Objects row/selection/gizmo | device E2E-CADR0-06/15 | PASS |
| CADR0-39 | Existing Import/Sculpt/SculptHistory unaffected | native `CADR0_39_*` (a CAD Body offers no sculpt source); all seventeen prior native suites unchanged in count and green; device E2E-CADR0-16; the FullSharded aggregate | PASS |
| CADR0-40 | Forbidden booleans/fillets/solver/etc absent | a case-insensitive grep of the native and Java sources for boolean union/subtract/intersect, fillet, chamfer, revolve, loft, sweep, constraint solver, tangent constraint and spline finds only the gizmo self-test's `driveRingSweep` (a test helper that sweeps a ring drag, not a CAD sweep); no such control is drawn anywhere | PASS |

## Suites and builds

| Gate | Result |
| --- | --- |
| Native self-tests, clean debug launch | **18/18 suites**, all `*_SELFTEST_OK`, then `FORGESHAPE_NATIVE_VIEWPORT_OK` - see `NATIVE_SELFTEST_CADR0.txt`. CAD: 122 checks, 0 failed. Project: 230 (219 + 11 new). Every other suite at its previous count. |
| Golden digests | all eleven printed and asserted; the seven prior ones unchanged |
| Standalone native runner (NDK clang, x86_64 emulator) | cad 122/0, history 147/0, project 230/0, scene 79/0, gltf_export 93/0, camera 119/0 |
| JVM (`:app:testDebugUnitTest`) | **70 / 70**, 0 failures, 0 errors |
| Debug APK | built; `lib/arm64-v8a` and `lib/x86_64` present |
| Release APK (`:app:assembleRelease`) | built, unsigned; both ABIs present |
| Device guards (`verify-device-guards.ps1`) | `DEV2-01..07`, `DEV3-01..06` all PASS |
| Corpus (`build-forge-corpus.ps1 -VerifyOnly`) | all sixteen fixtures `OnDisk = True`; the four new digests identical between the PowerShell encoder and the C++ encoder |
| Focused device suite | `SketchExtrudeTest` **OK (9 tests)** - `FOCUSED_SKETCH_EXTRUDE.txt` |
| Authoritative aggregate | **`FULL_SHARDED_SUITE_PASS`** - 35 classes / 491 tests / 5 shards (100 + 98 + 99 + 97 + 97), assigned union 491 = executed union 491, missing = duplicates = unexpected = 0 (`FULL_SHARDED.txt`). A first run failed one pre-existing palette test on the new palette structure; that test was corrected and the aggregate rerun once from shard 1 on the final tree (`FULL_SHARDED_RUN1_FAIL.txt`, `INDEX.md`) |
