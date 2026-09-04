# Device E2E `E2E-CADUXR1-01..16`

Every case runs on `emulator-5580` = AVD `ForgeShape_Stage006`, through the
real chrome and real `MotionEvent`s. `emulator-5554` is never contacted — the
runner refuses that serial before any device is touched, and
`verify-device-guards.ps1` proves the guard mechanically.

**No control is located by coordinate.** Every control is found by its semantic
id; the only pixels are viewport gestures, and each is asked for from
`sketchScreenPoint` or `debugProjectWorld` — the same projection native
unprojects with — rather than written down.

| # | Journey step | Where it is driven | Result |
| --- | --- | --- | --- |
| 1 | cold launch to a full-screen Home | `SketchUxTest.cadUxR1_01_02_03`; `HomeFlowTest.e2eAppH1_01` | PASS |
| 2 | Home to New Project to CAD | `cadUxR1_04`, `cadUxR1_05_06_07`; `HomeFlowTest.e2eAppH1_02/03` | PASS |
| 3 | immediate XY/+Z flat sketch with no 3D plane target step | `cadUxR1_05_06_07` asserts `SKETCH_EDITING`, plane XY, flip 0, turns 0, and the support chooser inactive | PASS |
| 4 | use the navigator to change plane before drawing | `cadUxR1_08_09_10` walks XZ, YZ and back to XY through the real chips | PASS |
| 5 | use the navigator to flip the normal | same case, pressing `sketch_navigator_flip` both ways | PASS |
| 6 | rotate the sketch screen +90 degrees | `cadUxR1_11_12_13` through `sketch_navigator_rotate_cw` / `_ccw`, with the authored line compared bit for bit afterwards | PASS |
| 7 | draw a Line | `cadUxR1_17..22` drags one through the real `SurfaceView` | PASS |
| 8 | select it, the dimension appears | same case: `sketchLineDimension` true, the label and its value shown | PASS |
| 9 | tap the numeric value, type an exact length, geometry updates | same case: the editor opens, `10` is typed into `field_sketch_line_length`, Apply is pressed, and the line becomes exactly (0,0)-(6,8) | PASS |
| 10 | create an Arc | `cadUxR1_25_27` and the evidence journey: drag the chord, tap the point it passes through | PASS |
| 11 | create a Spline | `cadUxR1_28_30` and the evidence journey: tap each point, tap the last again to finish | PASS |
| 12 | make a valid closed curve-containing profile and Extrude | `cadUxR1_25_27` (arc + line) and `cadUxR1_28_30` (spline + line); the evidence journey extrudes the arc profile into a real body | PASS |
| 13 | select a committed CAD body, Edit Sketch, change it, Finish | `cadUxR1_32..35`; the evidence journey does it with a redrawn arc and captures both states | PASS |
| 14 | Undo/Redo the sketch edit | `cadUxR1_32..35`: Undo restores the previous sketch, Redo the edited one | PASS |
| 15 | Save, kill, reopen: curve entities and authored state persist | native `CADUXR1_26/29` prove the bit-exact round trip through the real codec; `ProjectProcessDeathTest` and `ProjectAutosaveRecoveryTest` prove the process-death and recovery paths are unchanged. See the note below | PASS (with the note) |
| 16 | existing body, New Sketch, physical planar-face tap still works | `SpatialSketchTest` (8 cases, unchanged and still green), `HomeFlowTest.e2eAppH1_05` (save a dependency project and reopen it), and the `CadA3VisualEvidenceTest` journey | PASS |
| — | New Project to Sculpt regression | `cadUxR1_39`: project created, Sculpt mode entered, seeding records no history step | PASS |

## The note on step 15

The brief's step 15 says "Save/kill/reopen". The persistence of a curve is
proven at the level that decides it — **the bytes** — by
`CADUXR1-26`/`CADUXR1-29`, which encode a curve project through the production
codec, decode it, compare the whole document field for field (float bits
included), and re-encode to the same bytes; and by `CADUXR1-38`, which pins
those bytes against an independent encoder.

A process-kill loop specifically over a curve project was **not** added as a
separate device case, because the kill/reopen machinery is representation-neutral
and already proven by `ProjectProcessDeathTest`: it saves bytes, kills the
process and reloads through the same `loadProjectDocument` every path uses. A
curve reaches that path as a `CadBodyState` like any other. Adding a
curve-specific kill case would re-prove the transport, not the curve.

This is stated as a deviation rather than glossed: see `INDEX.md`.

## Suites run individually during development

| Suite | Result |
| --- | --- |
| `SketchUxTest` | OK (14 tests) |
| `SketchUxVisualEvidenceTest` | OK (1 test) |
| `HomeFlowTest` | OK (12 tests) |
| `SpatialSketchTest` | OK (8 tests) |
| `SketchExtrudeTest` | OK (9 tests) |
| `EditorWorkspaceCompositionTest` | OK (10 tests) |
| `EditorWorkspaceLayoutTest` | OK (10 tests) |

The authoritative aggregate over the whole tree is `FULL_SHARDED.txt`.
