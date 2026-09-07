# UI-3D-STATE-AUDIT-R1 — Evidence Index

**Start HEAD:** `dfcab1afd1361d37b6dfe4607772a5597f31d043`
**Device:** `ForgeShape_Stage006` / `emulator-5580` (identity confirmed with
`adb -s emulator-5580 emu avd name` before any interaction). `emulator-5554` was
never contacted; it was not even attached.
**Runner:** `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass
com.forgeshape.app.Ui3dStateAuditTest` — `MODE=FOCUSED_SUBSET`, so **no
aggregate marker is emitted in either direction** and none is claimed.

## Machine-readable ledgers

| file | rows | what it holds |
| --- | --- | --- |
| `CONTRACT_MATRIX.tsv` | 117 | every executed expectation: case, state, surface, expected, the binding contract it comes from, and the measured result |
| `VISIBILITY.tsv` | 117 | the show/hide ledger: expected vs. actual vs. verdict |
| `SPATIAL_ATTACHMENTS.tsv` | 72 (60 measured, 12 not measurable) | the attachment ledger: the semantic anchor, the actual attachment point, the error in px and dp, and the verdict |
| `NOTES.tsv` | 28 | the diagnostic reads taken at each measurement — mode state, per-axis anchor validity, chip visibility, the overlay container's own padding, drag outcomes, camera scale |

## Screenshots

All 59 frames were captured with `UiAutomation.takeScreenshot()`, which captures
the **composed display**, so the Vulkan viewport is in every one of them.

**Mechanical overlays.** Where a spatial surface was measurable, the frame
carries the overlay UI3D-15 requires:

- **green ring** — the semantic anchor native reports for that surface at that
  instant;
- **red cross** — where the surface actually attached;
- **yellow line** — the two joined, so the error is legible without reading the
  TSV.

| # | file | what it shows | cited by |
| --- | --- | --- | --- |
| 01 | `ui3d03_01_home.png` | Home with the whole editor withdrawn | UI3D-03 |
| 02 | `ui3d03_02_new_project.png` | the New Project chooser | UI3D-03 |
| 03 | `ui3d03_03_first_sketch.png` | the CAD bootstrap sketch over an empty scene | UI3D-03 |
| 04 | `ui3d03_04_staged_one_side.png` | the staged One Side extrusion | UI3D-03, UI3D-08 |
| 05 | `ui3d03_05_symmetric.png` | Symmetric: Flip withdrawn, one value | UI3D-03, UI3D-09 |
| 06 | `ui3d03_06_two_sides.png` | Two Sides: two values, Flip withdrawn | UI3D-03, UI3D-09 |
| 07 | `ui3d03_07_committed_body.png` | the committed body with the `Edit Sketch` chip | UI3D-03, UI3D-10 |
| 08 | `ui3d03_08_edit_sketch.png` | the re-opened sketch | UI3D-03, UI3D-10 |
| 09 | `ui3d03_09_after_cancel.png` | after cancelling the edit | UI3D-03 |
| 10 | `ui3d04_01_dimensions_open.png` | **UI3D-F-003**: Dimensions open, leaders drawn, numbers absent | UI3D-04 |
| 11 | `ui3d04_02_after_move.png` | dimensions after a Move | UI3D-04 |
| 12 | `ui3d04_03_after_rotate.png` | dimensions after a Rotate | UI3D-04 |
| 13 | `ui3d04_04_after_scale.png` | dimensions after a Scale | UI3D-04 |
| 14 | `ui3d04_05_after_typed_dimension.png` | after an exact dimension typed on a one-sided anchor | UI3D-04 |
| 15 | `ui3d04_06_after_undo.png` | after Undo of that resize | UI3D-04 |
| 16 | `ui3d05_01_before_camera.png` | the reference frame before camera motion | UI3D-05 |
| 17 | `ui3d05_02_after_orbit.png` | after a real one-finger orbit | UI3D-05 |
| 18 | `ui3d05_03_after_chrome_sync.png` | the same view after one unrelated chrome act — the labels reappear | UI3D-05, UI3D-F-002 |
| 19 | `ui3d05_04_after_pan.png` | after a two-finger pan | UI3D-05 |
| 20 | `ui3d05_05_after_zoom_out.png` | **UI3D-F-002, the headline frame**: the body is a sliver near the centre and `2 m` / `1 m` / `0.5 m` float far below-left, unattached | UI3D-05 |
| 21 | `ui3d05_06_after_zoom_in.png` | after zooming back in | UI3D-05 |
| 22 | `ui3d06_01_body_b.png` | Dimensions opened on body B | UI3D-06 |
| 23 | `ui3d06_02_body_a.png` | **UI3D-F-007**: after switching to body A, the numbers stand at body B's anchors (282 dp) | UI3D-06 |
| 24 | `ui3d06_03_hidden.png` | **UI3D-F-004**: the active body hidden, a dimension label still drawn | UI3D-06 |
| 25 | `ui3d06_04_locked.png` | the active body locked: no gizmo, no labels | UI3D-06 |
| 26 | `ui3d06_05_after_delete.png` | after Delete | UI3D-06 |
| 27 | `ui3d06_06_after_undo.png` | after Undo of the Delete | UI3D-06 |
| 28 | `ui3d07_01_line_selected.png` | **UI3D-F-005, the adjudicating frame**: a selected straight Line, `1.6 m` chip drawn, and **no extension lines, dimension line or ticks anywhere** | UI3D-07 |
| 29 | `ui3d07_02_length_applied.png` | after the length was typed | UI3D-07 |
| 30 | `ui3d07_03_after_pan.png` | the label tracking a pan correctly | UI3D-07 |
| 31 | `ui3d07_04_tool_changed.png` | after changing tool | UI3D-07 |
| 32 | `ui3d08_01_one_side.png` | the staged One Side cluster with its anchor overlay | UI3D-08 |
| 33 | `ui3d08_02_after_drag.png` | after a real arrow drag (depth 1.0 → 1.514 m) | UI3D-08 |
| 34 | `ui3d08_03_after_orbit.png` | the cluster after an orbit | UI3D-08 |
| 35 | `ui3d08_04_after_zoom_out.png` | the cluster at the 0.80 scale floor | UI3D-08 |
| 36 | `ui3d08_05_after_zoom_in.png` | the cluster back at 0.95 | UI3D-08 |
| 37 | `ui3d09_01_symmetric.png` | Symmetric, one number, two arrows | UI3D-09 |
| 38 | `ui3d09_02_two_sides.png` | Two Sides, Side A | UI3D-09 |
| 39 | `ui3d09_03_two_sides_side_b.png` | Two Sides, Side B at its own anchor | UI3D-09 |
| 40 | `ui3d09_04_after_side_b_drag.png` | after dragging Side B (1.514 → 2.125 m) | UI3D-09 |
| 41 | `ui3d09_05_side_b_zero.png` | Side B typed to 0 while Side A carries the extent | UI3D-09 |
| 42 | `ui3d09_06_back_to_one_side.png` | back to One Side: no stale second value | UI3D-09 |
| 43 | `ui3d10_01_committed.png` | **UI3D-F-001, the clearest frame**: the anchor ring on the retained sketch, the `Edit Sketch` chip one status bar below it | UI3D-10 |
| 44 | `ui3d10_02_after_move.png` | the chip after a Move | UI3D-10 |
| 45 | `ui3d10_03_after_rotate.png` | after a Rotate | UI3D-10 |
| 46 | `ui3d10_04_after_scale.png` | after a Scale | UI3D-10 |
| 47 | `ui3d10_05_after_gizmo_drag.png` | after a real gizmo axis drag | UI3D-10 |
| 48 | `ui3d10_06_after_orbit.png` | after an orbit — the chip tracks | UI3D-10 |
| 49 | `ui3d10_07_after_zoom.png` | after a zoom out — the chip tracks and the scale floors at 0.80 | UI3D-10 |
| 50 | `ui3d10_08_edit_sketch.png` | Edit Sketch: the chip withdrawn, the gizmo withdrawn | UI3D-10 |
| 51 | `ui3d10_09_finish_again.png` | Finish again restores the feature preview for the same body | UI3D-10 |
| 52 | `ui3d11_01_sculpt.png` | **UI3D-F-004, the decisive frame**: Sculpt entered, the renderer has dropped the leaders, and all THREE `1.2 m` chips are still drawn — one squarely on top of the sphere being sculpted | UI3D-11 |
| 53 | `ui3d11_02_navigator.png` | the Sculpt History navigator open | UI3D-11 |
| 54 | `ui3d11_03_mask.png` | after a real Mask stroke | UI3D-11 |
| 55 | `ui3d11_04_back_to_construction.png` | Back to Construction: brush controls and navigator gone | UI3D-11 |
| 56 | `ui3d11_05_resume.png` | Resume Sculpt resurrects nothing | UI3D-11 |
| 57 | `ui3d13_01_one_primary.png` | one primary surface at a time | UI3D-13 |
| 58 | `ui3d13_02_relative_scale.png` | Relative Scale closes Dimensions | UI3D-13 |
| 59 | `ui3d13_03_shape_tool.png` | the Shape entry withdraws the Transform members | UI3D-13 |

## Run logs

| file | run |
| --- | --- |
| `logs/run-01.log` | attempt 1 — 9 cases, 101.4 s, `OK (9 tests)`. Two of its rows turned out to be matrix errors of this audit's own, corrected for attempt 2 and recorded in `FINDINGS.md`. |
| `logs/run-02.log` | attempt 2 — 9 cases, 98.5 s, `OK (9 tests)`. **The authoritative run**; every ledger in this directory is its output. |

Two complete attempts, which is the limit §7 sets. Total automated runtime
**3 min 20 s**, against a 20-minute target and a 30-minute hard stop.

## Harness

| file | role |
| --- | --- |
| `app/src/androidTest/java/com/forgeshape/app/Ui3dStateAuditTest.java` | the nine audit cases |
| `app/src/androidTest/java/com/forgeshape/app/Ui3dAuditRecorder.java` | the two ledgers, the centre/clamp geometry and the annotated capture |

**No observability seam was added.** Every anchor comes from a read-only debug
seam the repository already ships: `bodyDimensionLabelPoint`,
`cadExtrudeToolState`, `cadBodySketchAnchor`, `sketchLineDimension`,
`sketchScreenPoint`, `debugProjectWorld`, `debugCameraPose`, `gizmoHandlePoint`,
`gizmoState`, `bodyDimensionsState`. No production translation unit was touched,
so the release surface is unchanged by construction — see UI3D-16 in
`SUMMARY.md`.
