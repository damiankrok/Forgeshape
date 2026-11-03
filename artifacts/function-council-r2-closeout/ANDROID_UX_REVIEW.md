# FUNCTION-COUNCIL-R2-CLOSEOUT-R1: Android UX review

This is an audit only. Product tree `70a6c14` (`app/src/main` = `f3db953`). Nothing was fixed and nothing was built.

## Inputs

**Council R2 dossier (read-only):**

- `ANDROID_UI_UX_AUDIT.md`
- `UI_SCREEN_STATE_MATRIX.tsv`
- `SEAT_F.md`
- `work/VERIFICATION.md`

**Emulator evidence from three CI DEVICE runs:**

| Label | Run | Tests |
| --- | --- | --- |
| A | `40797031704` | 32/32 |
| B | `40800569636` | 77/77 |
| C | `40805804385` | 90/90 |

All three runs belong to the previous dossier (`artifacts/function-council-r2/evidence/`). No capture or rect was re-run in this closeout, so every screenshot and recorded rect cited below is **E3** (historic emulator run) however closely it was re-read. E1 is used only for arithmetic or diagnostics actually executed in this closeout (`diagnostics/UX-REVIEW/rect_overlap.py`, and CR-06-S3's `diagnostics/CR-06/cr06_native.out`).

All three ran on API 36 x86_64 with SwiftShader, at 1080x2400 and 420 dpi (2.625 px/dp).

**Source read:** the Java views, `res/values/ids*.xml` and `forgeshape_jni.cpp`.

**Diagnostics:** `diagnostics/UX-REVIEW/` holds `rect_overlap.py`, `rect_overlap.out` and `CMD.txt`.

## Evidence rules used here

- **Emulator only.** Every runtime fact is emulator evidence. None of it is a physical-device result.
- **What a screenshot proves.** A static screenshot is cited only for layout, clipping and occlusion. It is never cited for:
  - TalkBack,
  - hardware or stylus hover,
  - comfort or reach,
  - animation,
  - touch routing.
- **Font scale.** No manifest records the font scale. `ANDROID_UI_UX_AUDIT.md:222` says 1.0, so it is graded E4 here.
- **"Verified" in a chain** means one of:
  - an executed test in A, B or C that drives or asserts that step, or
  - a captured view rect or screenshot.

  A step with no such evidence is **UNVERIFIED**. A chain with any unverified step is not a PASS.
- **`performClick()` is not a touch.** Many UI tests "press" a control by calling `performClick()` on the view (for example `SketchUxTest.java:586-595`). That proves the click handler. It does **not** prove that a finger at that spot reaches the view.
- **The coordinator's focused run is not cited.** A focused CI DEVICE run (GlbExport / Sketch / CadPlanarFace classes) is held by the coordinator. A folder `evidence/device-run-40979514270/` exists in this closeout tree. Per the task, this seat does not interpret it or claim its result.

## 1. The ten largest frictions, ranked

The ranking uses three factors:

- cost to the user: data loss or leaving the app ranks above silent wrong input, which ranks above a silent refusal or hidden state, which ranks above cosmetic issues;
- how often the act is used;
- how strong the evidence is.

"Taps / backs" counts the minimum to reach the friction from the stated state, plus the cost to recover.

### UX-1. System Back in the CAD bootstrap throws away the drawn sketch even when a panel is open (F-03)

- **State:** CAD bootstrap (no project), sketch with geometry. One of these is open:
  - the precision surface (Property Inspector),
  - the Modify palette (`sketch_actions_palette`),
  - the extrude dock palette (`cad_extrude_actions_palette`).

  Portrait, any IME state.
- **Taps / backs:** 1 tap opens the panel, then **1 Back**. The whole sketch is gone. Recovery means redrawing everything: there is no Undo, because the sketch is volatile by design. The path the user meant is 1 tap on the same toggle.
- **Failure:** in `EditorWorkspaceView.dismissTopmostSurface` (`EditorWorkspaceView.java:1149-1227`), the bootstrap branch (`:1177-1184`) runs before:
  - the row strip (`:1199`),
  - the anchored-surface branch (`:1202-1226`).

  Neither sketch palette is in that chain at all.

  The bootstrap branch calls `onCancelSketchRequested`, which calls `NativeViewport.sketchCancel()` (`:4930`). The status line then reads "Back at Home. No project was created." (`strings.xml:56`).
- **Target size:** not size-related.
- **JNI act:** `sketchCancel`.
- **Orientation / IME / font:** all of them; the order is unconditional.
- **Evidence:**
  - E2: source lines above.
  - Screen A `cad-a3-c2-sketch-ux-r1/03_immediate_flat_sketch.png` shows the state: precision toggle (sliders icon) on the rail, Modify under the navigator.
  - No test drives Back here. `uilr106` is the only platform-key Back test, and it covers Add Primitive in Construction only.
- **Verdict:** SOURCE-CONFIRMED-NOT-REPRODUCED.

### UX-2. The selected Line's dimension label and its length editor sit under the orientation navigator and the Tool Rail (new in this review)

- **State:** sketch Editing, a Line selected, in portrait. This review saw it in the bootstrap.
- **Taps / backs:**
  1. Select tool (1 tap).
  2. Tap the line (1 tap).
  3. Tap the value (1 tap).
  4. Type.
  5. Apply (1 tap).

  The dimension label `sketch_dimension_label` (rect `[800,583][914,709]`, recorded by `SketchUxVisualEvidenceTest` as `bounds(..., R.id.sketch_dimension_label)`; its reading chip `sketch_dimension_value` is a child inside it) and the editor field `field_sketch_line_length` (rect `[638,583][890,709]`) lie partly inside the navigator (`[442,336][859,691]`). Measured overlap (`diagnostics/UX-REVIEW/rect_overlap.out`):

  | View | Overlap with the navigator | Centre inside the navigator? | Overlap with the rail's x-span |
  | --- | --- | --- | --- |
  | Label | 59x108 px = 44.4% of the label | yes | 18 px |
  | Field | 221x108 px = 75.2% of the field | yes | — |
  | Editor | — | — | 147 px |

- **What the screenshots show:**
  - In `07_line_dimension.png` only "m" of "5 m" is visible.
  - In `08_length_editor_open.png`:
    - the field's focus outline disappears under the navigator's −90°/+Z/+90° row;
    - Apply covers the rail where Polyline stood;
    - the rail is cut short, and its Cancel ✕ floats outside the capsule.
- **Failure:** this breaks two rules in `CLAUDE.md`:
  - "No surface ... may partially cover another live control."
  - "The navigator may stand on the drawing; it may never stand on the Tool Rail."

  The rule's intent is also broken: the label should be hidden rather than drawn under chrome.

  Z-order in source:
  - The navigator has `setElevation(elevation_floating)` (`SketchOrientationNavigatorView.java:81`).
  - The dimension view is added after it but has no elevation (`EditorWorkspaceView.java:921-924`).

  The navigator therefore draws over the label and editor, and most likely takes the touches there. Which navigator chip a finger at the label's centre (857, 646) would hit is E5.

  `SketchUxTest.cadUxR1_17..22` and `SketchUxVisualEvidenceTest` pass because they `performClick()` the view and assert `isShown()`. Neither tests what is on top.
- **Target size:**
  - label `sketch_dimension_label` 43.43x48 dp (114x126 px at density 420; below the 48 dp width floor, F-01);
  - field 96x48 dp;
  - editor 172.6x56.4 dp.
- **JNI act:** `sketchApplyLineLength` (`EditorWorkspaceView.java:4081` → `forgeshape_jni.cpp:6700` → `SketchSession::applyLineLength`).
- **Orientation / IME / font:** portrait, font scale 1.0 (E4). With the IME up (capture 08), the rail is truncated.
- **Evidence:**
  - E3: the rects are recorded runtime facts from run A.
  - E1: the overlap arithmetic over those recorded rects, run in this closeout (`rect_overlap.py`, exit 0). This is the only E1 here: it proves the arithmetic, not a fresh layout.
  - E3: the visual occlusion, observed in the historic emulator captures A 07 and 08 (run `40797031704`, previous dossier). No capture was re-run in this closeout.
  - E2: z-order and touch routing.
- **Verdict:** SOURCE-CONFIRMED-NOT-REPRODUCED. The geometric and visual occlusion is observed in a historic run (E3) and its z-order is source-derived (E2); nothing was re-captured here. Touch misrouting is SOURCE-CONFIRMED-NOT-REPRODUCED (E2, chip identity E5).

### UX-3. Back during an in-project Revolve leaves the app on API 33 and later (F-04)

- **State:** project open, Ready, Revolve started (`revolve_sketch`). API 36 (targetSdk 36).
- **Taps / backs:** 1 Back and the Activity goes to the background. The user must relaunch from recents.

  Below API 33 the same Back returns to Extrude (`dismissTopmostSurface:1170-1174`).
- **Failure:** `hasDismissibleSurface` (`EditorWorkspaceView.java:1112-1117`) leaves out the revolve state. So the back callback is not armed when only a Revolve is in progress.
- **JNI act:** none; the act is lost. On the legacy route: `sketchEndRevolve`, via `onRevolveEndRequested`.
- **Orientation / IME / font:** all.
- **Evidence:** E2. No fresh capture exists: `CadRevolveOwnerTest` is in none of runs A, B or C, so revolve layout is E3 history only.
- **Verdict:** SOURCE-CONFIRMED-NOT-REPRODUCED.

### UX-4. A comma decimal is dropped silently in three canvas editors (F-05)

- **Affected fields:**
  - sketch line length (`SketchDimensionLabelView.java:112`),
  - extrude depth / second side (`CadExtrudeCanvasView.java:644`),
  - body dimension (`BodyDimensionLabelsView.java:127`).

  All three use `DigitsKeyListener("0123456789.-")`. Four other editors accept `,`:
  - `NumericPropertyRow.java:109`,
  - `CadRevolveAngleLabelView.java:87`,
  - `SketchDimensionLabelsView.java:126`,
  - `SketchModifyView.java:208`.
- **Taps:** typing `2,5` takes 3 key taps and yields `25`. There is no error.
- **Visual:** in capture A `08_length_editor_open.png` the emulator's numeric IME shows a `,` key next to `.` in this very field. Whether a given IME's decimal key sends `,` is not tested.
- **JNI act:** `sketchApplyLineLength` / `sketchSetExtrudeSide` / `applyBodyDimension`, each with the wrong number.
- **Orientation / IME / font:** IME-dependent; a hardware keyboard always can type `,`.
- **Evidence:** E2, plus an E3 visual of the `,` key in the historic capture A 08 (not re-captured here).
- **Verdict:** SOURCE-CONFIRMED-NOT-REPRODUCED. The wrong value was not typed in any run.

### UX-5. A failed staged Surface edit is named, but its verdict and Fix fall outside the visible sheet (new in this review)

- **State:** Surface body. History is open and a Thicken row is selected; Thickness is set to 0. Bottom sheet in portrait.
- **Taps:**
  1. History (1 tap).
  2. Row (1 tap).
  3. Type `0`.

  Apply is withdrawn (correct). The verdict "Thicken 2 cannot be rebuilt with this value." (B `modeling-surface/facts-*.txt`, `surf02.staged_failure`) and the Fix control exist in the view tree: the test reads `surface_edit_verdict` and asserts `surface_edit_fix` is shown (`SurfaceOwnerTest.java:200-212`).

  But capture B `modeling-surface/04_staged_failure.png` shows neither. The sheet shows Modify / Thickness / "Edit Thicken 2" / Value, with the next field clipped at the sheet's bottom edge.

  The resting chips show "Edit Thicken 2". They show no failure.

  The body is drawn mostly off the window's right edge. Bringing it back takes at least 1 scroll plus a pan.
- **JNI act:** `surfaceTimeline(... editTarget, editId, value ...)` for the staged evaluation. No commit happens.
- **Orientation / IME / font:** portrait, 1.0. With the IME up the sheet would be shorter. That case is not captured.
- **Evidence:** E3: the historic emulator capture B `modeling-surface/04` (run `40800569636`, previous dossier) and its recorded facts; re-reading it here does not make it new evidence. `isShown()` in the test is not visibility in the frame.
- **Verdict:** Observed in historic run (E3); not reproduced in this closeout. No source derivation places the verdict out of frame, so this is not SOURCE-CONFIRMED either.

### UX-6. Transform over a locked body gives no feedback (new in this review)

- **State:** Construction. Active body locked through the Objects row strip. Transform is chosen on the rail.
- **Taps / backs:** 1 tap on `tool_rail_place`. The rail highlights Transform, but:
  - no gizmo appears,
  - no Move / Rotate / Scale selector appears,
  - nothing is written to the status line.

  Recovery takes 4 taps to unlock, then a re-tap of Transform:
  1. Objects capsule.
  2. Row overflow `object_row_more`.
  3. `object_row_lock`.
  4. Close.
- **What causes it:**
  - The gizmo and selectors are withdrawn over a locked body (`EditorWorkspaceView.java:3026-3033`, `:3045-3049`), as Stage 018A intends.
  - The rail handler deliberately writes no status (`:5186-5210`, comment "Nothing is written for this").
  - The only lock cue lives inside the Objects popover row. The resting capsule shows "Body #3" with no lock glyph.
- **Visual:** capture A `ui-3d-state-audit-r1/ui3d06_04_locked.png` shows Transform held, no gizmo, an empty status line, and a capsule reading only "Body #3".
- **JNI act:** `setGizmoActive(false)`. Nothing else is sent.
- **Evidence:**
  - E3 visual: the historic emulator capture A `ui3d06_04` (run `40797031704`), not re-captured here.
  - E2 source.
  - `ObjectCommandsTest.e2eObj018a01` (C) asserts the gizmo is withdrawn and the write refused. It does not assert any feedback.
- **Verdict:** SOURCE-CONFIRMED-NOT-REPRODUCED for the missing cue (E2 source, E3 historic capture). Whether a cue is required is **OWNER-SEMANTICS-REQUIRED**, because "chrome reports, it does not instruct" also applies here.

### UX-7. The orientation navigator's Flip and ±90° change the label but not the camera (F-02 / C-01)

- **State:** sketch Editing, empty sketch.
- **Taps:** 1 tap on `sketch_navigator_flip` or `sketch_navigator_rotate_cw`. The label changes ("XY · Along +Z · 0°" becomes "… 90°"). The view does not.
- **What the screenshots can and cannot prove:** captures A `04_navigator_default_plus_z.png` and `06_navigator_rotated_plus_90.png` differ only in the label. But a square grid looks the same after a 90° turn, so these two images **cannot** discriminate a camera change.

  The finding rests on:
  - source: `forgeshape_jni.cpp:313-320` `beginSketchView` frames `frame()`, and `viewFrame` has no production caller (`work/VERIFICATION.md` row F-02);
  - Seat C's host diagnostic.

  `SketchUxTest.cadUxR1_08_09_10` and `_11_12_13` (B) assert state bits only.
- **Target size:** Flip is 36.2x48 dp, below the width floor.
- **JNI act:** `sketchSetViewFlipped` / `sketchRotateView` (`EditorWorkspaceView.java:4061`, `:4067`).
- **Evidence:** E2, plus the R2 host diagnostic, which is a historic recorded run (E3). In this closeout the native leg was re-run as CR-06-S3 (`diagnostics/CR-06/cr06_native.out`: `camera_view_identical_to_B0=1` for Flip, +90 and −90), which is E1 for the native camera only. The screenshots are non-discriminating, and the on-screen tap path was not run.
- **Verdict:** REPRODUCED at the native layer (CR-06-S3, E1). The Android tap-to-camera path is SOURCE-CONFIRMED-NOT-REPRODUCED (E2).

### UX-8. Several chips are narrower than the 48 dp floor (F-01)

Arithmetic run in this closeout (`rect_overlap.out`, E1) over rects recorded in historic run A `40797031704` (E3), density 420:

| Control | Size (dp) |
| --- | --- |
| `sketch_navigator_flip` | 36.19x48 |
| `view_selection_outline_on` | 36.95x48 |
| `view_selection_outline_off` | 39.24x48 |
| `sketch_dimension_label` | 43.43x48 |
| `finish_sketch` (within the test's tolerance) | 112.38x47.62 |

- `view_grid_on/off` is built the same way (`EditorControlStyles.chip`, `:366`) but was not measured.
- **Taps:** 1 each. Precision risk only.
- **States:** sketch Editing (Flip, value) and the Display popover (outline chips). Portrait, 1.0.
- **Screens:**
  - A `cad-a3-c2-sketch-ux-r1/04`, `07`;
  - A `sel-out-r1/11_left_handed_view_overlay.png`, where the Grid and Selection Outline On/Off chips are visibly narrower than the Shading chips.
- **JNI acts:** `sketchSetViewFlipped`, `setSelectionOutlineVisible`, `setGridVisible`.
- **Cause in source:** `EditorControlStyles.chip` (`:366-382`) sets a 48 dp minimum height and 10 dp horizontal padding but no minimum width, so a short label yields a chip under 48 dp wide (E2).
- **Evidence:** E3 rects (historic run A, not re-run), E1 arithmetic over them, E2 source. Physical comfort is not assessed.
- **Verdict:** SOURCE-CONFIRMED-NOT-REPRODUCED, as `CRITICAL_REPRO_MATRIX.tsv` CR-06-S6 grades the same F-01 measurement.

### UX-9. With the IME up in the bootstrap, a ghost "Body #0" capsule appears and the rail is cut short (new in this review)

- **State:** CAD bootstrap (no project), line length editor open, IME up, portrait.
- **What is seen:** capture A `08_length_editor_open.png` shows the Objects capsule reading **"Body #0"** at bottom left. That is an ObjectId of 0, which means no object. Capture 03, the same bootstrap without the IME, has no such capsule.

  Source says the bottom row is GONE without a project (`EditorWorkspaceView.java:2884`). But a second writer, `applyPrimarySurfaceChromePolicy` (`:1107`), sets it `VISIBLE` whenever the inspector is not a bottom sheet, and it does not check `projectOpen`. That is a plausible cause, graded **E5** and not reproduced.

  In the same capture:
  - the rail is cut off above Rectangle;
  - Polyline is hidden behind Apply;
  - the Cancel ✕ floats below the rail capsule;
  - the precision toggle is not visible.
- **Taps:** tapping the capsule would open Objects over a non-project. The effect is UNVERIFIED.
- **JNI act:** none observed.
- **Evidence:** E3 for what is drawn (historic emulator capture A 08, run `40797031704`, not re-captured here); E5 for the cause.
- **Verdict:** Observed in historic run (E3); not reproduced in this closeout. The cause (the `applyPrimarySurfaceChromePolicy` writer) is an E5 inference and is not reproduced.

### UX-10. The Sculpt status pill sits under the brush column (F-06)

- **State:** Sculpt resting, just entered, portrait 1.0.
- **What is seen:** in capture A `ui-3d-state-audit-r1/ui3d11_01_sculpt.png`, the status text "… the Construction sphere is kept." is covered at its start by the Radius / Strength column `brush_edge_controls`. Its first words cannot be read.
- **Taps:** 0. It is a reading problem only.
- **JNI act:** `enterSculptMode` / `freezeToSculpt`, which produce the message.
- **Evidence:** E3 visual: the historic emulator capture A `ui3d11_01` (run `40797031704`), re-read here but not re-captured.
- **Verdict:** Observed in historic run (E3); not reproduced in this closeout.

### Considered and left out of the ten

| Finding | Why it was left out |
| --- | --- |
| F-11: Settings Back at y = 3409 px, 384 dp below the fold | System Back works, so the cost is low |
| F-07: TalkBack selected state and live verdicts | Source-only (E2). A screenshot cannot show TalkBack |
| F-08: Light Charcoal focus ring at 2.48:1 | Recommendation-grade |
| Ready / dock icons have no captions while Tool Labels is OFF (B `05_compact_hud_extent_palette.png`) | By decision (Tool Labels preference); discoverability only |

## 2. Function chains (42 rows)

**Reading the table:**

- Steps are joined by →.
- **V** = verified, with its evidence in brackets. **U** = UNVERIFIED.
- Test runs: A = `40797031704`, B = `40800569636`, C = `40805804385`. All are emulator runs.
- A test that only checks `undoDepth` grew has verified the step was **recorded**, not that an **Undo was executed**.
- JNI sites are `forgeshape_jni.cpp:<line>`; Java sites are `<File>.java:<line>`.
- Domain functions were confirmed by grep of the JNI body unless marked (E4).

| # | Function | User intent | Control (view id) | JNI method | Domain function | UI feedback | Undo / persistence | Chain verdict |
|---|---|---|---|---|---|---|---|---|
| 1 | New CAD project | start a CAD project | `home_new_project` → `new_project_cad` V [`SketchUxTest.cadUxR1_04` B; rects A facts] | `sketchBegin(XY)` (EWV:2365 → jni:4110) V [`cadUxR1_05_06_07` B reads native sketch state] | `SketchSession::begin` + `beginSketchView` V [same] | flat XY sketch, rail, navigator V [A `03_immediate_flat_sketch.png`] | none, volatile by design V [`cadUxR1_01_02_03` B: no body] | VERIFIED |
| 2 | Leave the bootstrap | abandon before creating | `back_to_home` V [screen A 03] | `sketchCancel` (EWV:4930) U | `SketchSession::cancel` U | Home + "Back at Home…" U | nothing created U | UNVERIFIED: no test in A/B/C drives `back_to_home` (HomeFlowTest not run) |
| 3 | Draw a rectangle | sketch a profile | `tool_rail_rectangle` + viewport drag V [`CadVerticalSliceTest.owner_rectangle_circle_region` B] | `sketchSetTool`, `touchEvent` (jni:9503) V [same] | `SketchSession` entity add V [same, region count asserted] | stroke drawn V [B `01_rectangle_circle_two_regions.png`] | volatile until commit V [design + test] | VERIFIED |
| 4 | Finish Sketch | stop drawing, see the extrusion | `finish_sketch` V [B tests] | `sketchFinish` (EWV:4522 → jni:4400) V | `SketchSession::finish` + `beginExtrudeFeatureView` V [`CadVerticalSliceTest.compact_extrude_hud` B] | Ready HUD, arrow, dock V [A `ui3d03_04_staged_one_side.png`] | none V | VERIFIED |
| 5 | First Extrude creates the project | make the first body | `extrude_sketch` V [`Ui3dStateAuditTest.ui3d03` A] | `sketchCommit` (EWV:4797 → jni:4487) V | `commitFirstCadProject` (jni:4507) V [ui3d03 asserts a body] | editor, body V [A `ui3d03_07_committed_body.png`] | empty history by design: U in A/B/C; save/reopen of an extent V [`CadExtrudeExtentTest.cadext12` B] | UNVERIFIED (the empty-history step) |
| 6 | Type an exact depth | set the extrusion length | `cad_extrude_depth_value` → `field_cad_extrude_depth` → `apply_cad_extrude_depth` V [`CadExtrudeExtentTest.cadext06b` B] | `sketchSetExtrudeSide` (EWV:4125 → jni:6283) V | `SketchSession::setExtrudeSide` V | value above the leader V [A `ui3d08_01_one_side.png`] | staged until commit V; comma dropped (UX-4) | VERIFIED (with UX-4 caveat) |
| 7 | Drag the extrude arrow | set depth by hand | `cad_extrude_canvas` (viewport drag) V [`cadext06c` B] | `touchEvent` → manipulator V | `CadExtrudeManipulator` → `setExtrude` (E4) V [depth asserted] | arrow grows V [A `ui3d08_02_after_drag.png`] | staged V | VERIFIED |
| 8 | Change the extent | Symmetric / Two Sides | dock → `cad_extrude_extent_symmetric` V [`cadext06a` B] | `sketchSetExtrudeExtent` (EWV:4147 → jni:6258) V | `SketchSession::setExtrudeExtent` V | two leaders V [A `ui3d09_01_symmetric.png`] | save/open preserves V [`cadext12` B] | VERIFIED |
| 9 | Flip the extrusion | grow on the other side | `cad_extrude_flip` U (no fresh test names it) | `sketchFlipExtrudeDirection` (EWV:4172 → jni:6301) U | `flipExtrudeDirection` U | arrow reverses U | U | UNVERIFIED |
| 10 | Choose a region | pick what to extrude | Ready tap on a cell V [`CadVerticalSliceTest.owner_rectangle_two_circles_union` B] | `sketchToggleRegion` (jni:4839) V | `toggleRegion` / `togglePlanarFace` V | hatched selection V [B `02_ring_selected_hatched.png`] | stored as a region ref V [B tests encode] | VERIFIED |
| 11 | Add to the same body | union a feature | face sketch → Add in the dock V [`new_body_then_add_same_body` B] | `sketchCommit` (non-first path) V | `SketchSession::commit` → `CadBody::applyState` → kernel Union (E4) V [volume asserted] | preview tint, then body V [B `08_add_preview`, `09_add_committed`] | one step V; Undo executed V [`CadVerticalSliceTest` undo calls, 14 sites] | VERIFIED |
| 12 | Cut | remove material | Cut in the dock V [`same_body_cut` B] | `sketchCommit` V | kernel Difference V [`cut.volume_m3` in B facts] | B `10_cut_preview`, `11_cut_committed` V | Undo V [`CadVerticalSliceTest`] | VERIFIED |
| 13 | Refused operation | understand why a Cut fails | Cut that misses V [`operation_refusal` B] | `sketchCommit` (refused) V | `CutNoIntersection` V | badge / status named V [B `12_cut_misses_preview.png`] | nothing written V | VERIFIED |
| 14 | Edit a feature | change an earlier feature | feature row / Edit V [`feature_edit_roundtrip` B] | `sketchBeginEditFeature` / `sketchCommitEdit` (EWV:4904 → jni:7273) V | `SketchSession::commitEdit` V | B `13_feature_edit_open`, `14_feature_chain_reopened` V | one Finish = one Undo V [A `cad-a3-c2…/facts.txt`: `one_finish_is_one_undo=true`] | VERIFIED |
| 15 | Edit Sketch (retained) | reopen a committed sketch | `cad_canvas_edit_sketch` V [`Ui3dStateAuditTest.ui3d10` A] | `sketchBeginEdit` (EWV:4854 → jni:7137) V | `SketchSession::beginEdit` V | A `ui3d10_08_edit_sketch.png` V | Cancel restores V [ui3d10 A] | VERIFIED |
| 16 | Exact line length | dimension a line | `sketch_dimension_value` → `field_sketch_line_length` → `apply_sketch_line_length` V via `performClick` only [`cadUxR1_17…22` B] | `sketchApplyLineLength` (EWV:4081 → jni:6700) V | `applyLineLength` V | label "5 m" V, but **occluded** (UX-2) [A 07/08] | volatile V | **UNVERIFIED as a touch path**: the controls are partly under the navigator and the test never touches them |
| 17 | Rotate / flip the sketch view | look at the sketch another way | `sketch_navigator_rotate_cw`, `sketch_navigator_flip` V [`cadUxR1_11_12_13` B] | `sketchRotateView` / `sketchSetViewFlipped` (jni:6630 / 6612) V | `rotateView` / `setViewFlipped` V | label changes V; camera change **U** (F-02, UX-7) | not truth V | UNVERIFIED (feedback step) |
| 18 | Change the sketch plane | XZ instead of XY | `sketch_navigator_plane_xz` V [`cadUxR1_14_15` B] | `sketchSetSupportPlane` (EWV:4045 → jni:6592) V | `setSupportPlane` V | A `05_navigator_other_plane.png` V; refused once the sketch has geometry V [chips greyed in A 07] | V | VERIFIED |
| 19 | Revolve | turn a profile | `revolve_sketch` U (CadRevolveOwnerTest not in A/B/C) | `sketchBeginRevolve` / `sketchSetRevolveAngle` (EWV:4620 / 4668 → jni:6391) U | `setRevolveAngle` U | ring + angle label U (no fresh capture) | Back on API 36 leaves the app (UX-3, E2) | UNVERIFIED |
| 20 | Undo / Redo (Construction) | take back an act | `undo_action` / `redo_action` V [`ObjectsDeleteTest.imp01b18and19` C] | `historyUndo` / `historyRedo` (EWV:3592 → jni:8266) V | `ConstructionHistory` via `runHistoryStep` V | body returns V [A `ui3d06_06_after_undo.png`] | V | VERIFIED |
| 21 | Delete a body | remove an object | `object_row_delete` V [`imp01b15and23` C] | `sceneDeleteBody` (ObjectsSectionView:424 → jni:3311) V | `deleteSceneBody` (jni:3323) V | row gone, selection falls V [`imp01b20` C; A `ui3d06_05_after_delete.png`] | one Undo restores V; absent from save / export V [`imp01b22` C] | VERIFIED |
| 22 | Rename | name a body | `object_row_more` → `object_row_rename` → `object_rename_field` / `object_rename_commit` V [`ObjectCommandsTest.e2eObj018a01` C] | `sceneRenameBody` (OSV:820 → jni:3413) V | `renameSceneBody` (jni:3429) V | name asserted V; visual U (no capture of the strip) | history step U (not asserted for rename); bytes U | UNVERIFIED (Undo step) |
| 23 | Hide / Show | hide a body | `object_row_visibility` V [e2eObj018a01 C] | `sceneSetBodyVisible` (OSV:736 → jni:3462) V | `setSceneBodyVisible` (jni:3471) V | hidden in viewport V [A `ui3d06_03_hidden.png`] | Undo U | UNVERIFIED (Undo step) |
| 24 | Lock | protect placement | `object_row_lock` V [e2eObj018a01 C] | `sceneSetBodyLocked` (OSV:751 → jni:3515) V | `setSceneBodyLocked` (jni:3524) V | gizmo withdrawn V; **no resting cue** (UX-6) [A `ui3d06_04_locked.png`] | one step recorded V [`ObjectCommandsTest.java:134`]; Undo executed U | UNVERIFIED (feedback, Undo execution) |
| 25 | Duplicate | copy a body | `object_row_duplicate` V [e2eObj018a01 C] | `sceneDuplicateBody` (OSV:765 → jni:3554) V | `duplicateSceneBody` (jni:3563) V | new row, copy active V [asserted] | one step recorded V; Undo executed U | UNVERIFIED (Undo execution) |
| 26 | Mirror | reflect a body | `object_row_mirror` → `object_mirror_plane_xy` V [`MirrorSmokeTest.e2eMirror0101` C] | `sceneMirrorBody` (OSV:798 → jni:3621) V | `mirrorSceneBody` (jni:3638) V | new body V; plane chooser visual U (no capture) | one step V; Undo executed V [`MirrorSmokeTest.java:227`] | UNVERIFIED (chooser visual only) |
| 27 | Exact body dimension | resize to a size | `body_dimension_label_x` → `field_body_dimension` → `apply_body_dimension` V [`BodyDimensionsSmokeTest` C] | `applyBodyDimension` (EWV:5064 → jni:2713) V | body-dimension solver (E4: `forgeshape_body_dimensions`) V [value asserted] | labels V [A `ui3d04_05_after_typed_dimension.png`] | one step V [`BodyDimensionsSmokeTest.java:106`]; Undo V [A `ui3d04_06_after_undo.png`]; comma dropped (UX-4) | VERIFIED (with UX-4 caveat) |
| 28 | Relative Scale | scale by a factor | relative-scale editor V [same test C] | `applyBodyRelativeScale` (RelativeScaleEditorView:148 → jni:2753) V | same solver V | A `ui3d13_02_relative_scale.png` V | one step V [`:123`] | VERIFIED |
| 29 | Exact transform | type position / rotation / scale | `property_inspector` fields → `apply_transform` V [`EditorWorkspaceLegibilityTest.uilr107–110,114` A] | `applyBoxTransform` (ConstructionPlacementEditorView:269 → jni:2538) V | `applyBoxTransform` (domain) V | values shown V [A `ui-pref-r1/16_left_handed_exact_open.png`] | Undo U in A/B/C for this path | UNVERIFIED (Undo) |
| 30 | Gizmo mode | Move / Rotate / Scale | `transform_mode_rotate` V [`uilr112/113` A] | `setGizmoMode` (EWV:3119 → jni:3870) V | `TransformGizmo::setMode` (E4) V | A `ui-pref-r1/20_gizmo_largest_bold_rotate.png` V | not truth V | VERIFIED |
| 31 | Start Sculpting | sculpt a body | `freeze_to_sculpt` V [`SculptUndoTest.e2eScundo01` B] | `freezeToSculpt` (EWV:5268 → jni:2871) + `enterSculptMode` (jni:3007) V | `buildSculptSourceMesh` + `SculptMesh::freezeFrom` (E4) V | Sculpt chrome V; status pill occluded (UX-10) [A `ui3d11_01_sculpt.png`] | sculpt mesh saved V [`ImportedMeshSculptTest.imp01b11and13` B] | VERIFIED (with UX-10 caveat) |
| 32 | Sculpt brush stroke + Undo | deform and take it back | `tool_rail_grab..mask` + stroke + `undo_action` V [`SculptBrushStage025Test.e2eFcm01`, `SculptUndoTest.e2eScundo01` B] | `setSculptTool` (jni:9179), `touchEvent`, `historyUndo` (sculpt branch) V | `SculptSession` / `SculptHistory` V | geometry changes V | sculpt history never serialized V [`scundo20and21` B] | VERIFIED |
| 33 | Clear Mask | release protected area | `clear_mask` V [`e2eFcm04and05` B] | `sculptClearMask` (SculptContextView:296 → jni:8191) V | `clearMask` V | A `ui3d11_03_mask.png` V | one Undo V [same test] | VERIFIED |
| 34 | Sculpt History jump | go back several strokes | `history_navigator_action` → `history_navigator_row` V [`SculptHistoryNavigatorTest.e2eSchnav02/03` B] | `sculptJumpToHistoryCursor` (EWV:3561 → jni:8145) V | `jumpToHistoryCursor` V | rows, current by shape V [A `ui3d11_02_navigator.png`] | records nothing V [`e2eSchnav07` B] | VERIFIED |
| 35 | Save project | keep the work | `project_actions_button` → `project_save` V [`ProjectAutosaveRecoveryTest`, `ProjectProcessDeathTest` C] | `encodeProject` (EWV:5459 → jni:8387) V | `captureProjectDocument` + codec V | status U (no capture) | reopen V [`ProjectProcessDeathTest` C] | UNVERIFIED (visual feedback) |
| 36 | Open File… | load a `.forge` | `project_open_file` / `home_open_file` V [`ProjectTransferTest.fsr1b11_openFileAppliesAValidDocument` C] | `loadProject` (EWV:5489 → jni:8440) V | `decodeProject` + `loadProjectDocument` (jni:8455 / 8477) V | damaged file refused V [`fsr1b11_openFileRefusesADamagedDocumentAndChangesNothing` C] | fresh history; manual slot untouched V [`fsr1b12` C] | VERIFIED (functional) |
| 37 | Import GLB | bring in a mesh | `import_glb` U (no fresh test drives the drawer row) | `importGlbDurable` (EWV:5882 → jni:8560) V [`ImportedMeshDurableTest` C] | `importGlb` + `commitImportedGlbScene` V | U | one Undo for all objects U in this review; saved V [`ImportedMeshDurableTest` C] | UNVERIFIED (control, feedback) |
| 38 | Export GLB | hand geometry to another tool | `export_action` U (focused run not cited) | `exportGlb` (EWV:5750 → jni:8506) U | `exportSceneAsGlb` U | U | is a read (design) U | UNVERIFIED in this review |
| 39 | Display toggles | turn the outline or grid off | `view_selection_outline_off`, `view_grid_on` V [`SelectionOutlineVisualEvidenceTest` A] | `setSelectionOutlineVisible` / `setGridVisible` (EWV:6304 / 6283 → jni:9327 / 9299) V | display store V | band gone V [A `sel-out-r1/02`, `measured_band_matched_pixels=0`] | session-only, not truth V | VERIFIED (chips below 48 dp, UX-8) |
| 40 | Palette / handedness | change appearance | `home_settings` / `project_settings` → `appearance_warm_light`, `handedness_left` V [`SettingsPreferencesTest` C; `UiPrefVisualEvidenceTest` A] | none (Java `AppPreferencesStore`) V | not domain V | A `ui-pref-r1/10..15` V | project bytes identical V [`uiprefr1_09_10_11_37` C] | VERIFIED |
| 41 | Freeform Push / Pull | shape the cage | `freeform_push_pull` V [`FreeformOwnerTest.devFf02` B] | `freeformPushPull` (FreeformEditorView:134 → jni:5594) V | freeform `pushPull` V | B `modeling-freeform/03_loop_and_crease.png` V (sheet content scrolled and clipped) | one Undo each V [devFf02]; save / reopen V [devFf03] | VERIFIED |
| 42 | Surface staged edit | change an earlier surface feature | `feature_history_action` → `feature_history_row` → `surface_edit_apply` / `surface_edit_fix` V [`SurfaceOwnerTest.devSurf02` B] | `surfaceTimeline` (staged) V | surface regeneration V | verdict exists V; **not visible in the frame** (UX-5) | one Undo V; Cancel writes nothing V [`SurfaceOwnerTest.java:189-213`] | VERIFIED functionally, UNVERIFIED visually |

**Tally:** 42 rows.

- **VERIFIED end to end: 26.** Rows 1, 3, 4, 6, 7, 8, 10, 11, 12, 13, 14, 15, 18, 20, 21, 27, 28, 30, 31, 32, 33, 34, 36, 39, 40, 41.
  - 6, 27, 31 and 39 carry a stated caveat.
- **At least one UNVERIFIED step: 15.** Rows 2, 5, 9, 16, 17, 19, 22, 23, 24, 25, 26, 29, 35, 37, 38.
- **Split: 1.** Row 42 is verified functionally and unverified visually.

## 3. What the screenshots show and what they cannot show

### Shown (layout, clipping, occlusion; E3: historic emulator captures from runs A, B and C, re-read in this closeout but not re-captured)

**Start pages.** Home and New Project are opaque full-window pages. New Project lists CAD, Sculpt, Freeform and Surface, each about 90–108 dp tall, with Back below (A `ui3d03_02_new_project.png`). Settings is opaque. Its Back row is 384 dp below the fold (A `ui-pref-r1/02_settings_page.png` plus the recorded rect).

**Bootstrap sketch** (A `cad-a3-c2…/03`):

- Finish Sketch and Back to Home share one capsule.
- The status line carries an instruction ("Draw a closed profile, then Extrude…"), which is the R2 F-12 doc divergence.
- The navigator stands left of the rail and does not overlap it at rest (`navigator_overlaps_tool_rail=false`).
- Modify sits under the navigator.

**Line selected and editor open** (A 07 / 08):

- the label is occluded by the navigator, with only "m" visible;
- the editor field is mostly under the navigator;
- Apply stands over the rail's Polyline;
- with the IME up the rail is truncated, ✕ floats outside it, and a ghost "Body #0" capsule appears (UX-2, UX-9);
- the plane chips are greyed once the sketch has geometry, which is consistent with `SketchNotEmpty`.

**Ready** (A `ui3d03_04`, B `16`):

- the rail is reduced to three icon-only controls (✕, ←, sliders);
- the value "1 m" stands on the leader;
- the dock badge stands beside the body;
- with the precision surface on request, the bottom sheet hides the Objects and history capsules (by design, `EditorWorkspaceView.java:1107`), while the toolbar Extrude stays.

**Dock palette** (B `05_compact_hud_extent_palette.png`): it opens flush at the window's left edge (x ≈ 0) over the model. It holds icon-only extent choices plus Flip; no captions, because Tool Labels is OFF.

**Display popover** (A `sel-out-r1/11`): it stands on the model and not on a control. The Grid and Selection Outline On/Off chips are visibly narrower than the Shading, Surface and Projection chips.

**Sculpt** (A `ui3d11_01`): the brush column covers the start of the status pill.

**Freeform cage sheet** (B `modeling-freeform/03`):

- the sheet's content is scrolled, with a field clipped at its top edge;
- Crease is clipped at the bottom;
- the Objects and history capsules float above the sheet, over the model;
- a small dot glyph stands beside the ForgeShape mark. Its meaning cannot be told from the image (UNVERIFIED).

**Surface staged failure** (B `modeling-surface/04`): the verdict and Fix are not in the frame, and the body is mostly off the right edge (UX-5).

**Locked body + Transform** (A `ui3d06_04`): no gizmo, no selector, no status, no lock glyph on the capsule (UX-6).

### Not shown, and not claimed here

| Area | Why the screenshots cannot answer it |
| --- | --- |
| Touch routing at an occluded point (UX-2) | Z-order is E2 only. The tests use `performClick`, which bypasses hit testing. |
| Navigator Flip / ±90° camera effect (UX-7) | A square grid looks the same after a turn, so A 04 vs 06 cannot discriminate. |
| TalkBack (F-07) | Selected state, live-region announcements and focus order are not visible in a PNG. |
| Hover | Hardware or stylus hover (chooser highlight); stylus pressure. |
| Physical use | Comfort, reach and thumb zones; real-device GPU, frame pacing and performance. |
| Font scale 1.3 / 1.5 / 2.0, landscape (except ui-pref 16), tablet / foldable / multi-window, mouse | No fresh capture in A, B or C. |
| Animations and transitions | No timing evidence (for example the anchored-surface exit motion). |
| Missing captures | Revolve ring, History surface, row command strip, mirror plane chooser, unsaved / recovery questions and Add Primitive. Their visuals stay E3 or UNKNOWN. |
| Window Back route | No screenshot proves where Back goes (UX-1, UX-3). |

## Summary of new and carried items

| Item | Status | Verdict | Evidence grade |
| --- | --- | --- | --- |
| UX-2: dimension label and editor occluded | **new** | SOURCE-CONFIRMED-NOT-REPRODUCED (occlusion observed in historic run) | E3 captures and rects; E1 arithmetic only; E2 z-order |
| UX-5: Surface failure verdict out of frame | **new** | Observed in historic run; not reproduced | E3 |
| UX-6: Transform over a locked body is silent | **new** | SOURCE-CONFIRMED-NOT-REPRODUCED for the missing cue; whether a cue is required is OWNER-SEMANTICS-REQUIRED | E2 + E3 |
| UX-9: ghost "Body #0" capsule and truncated rail with the IME in the bootstrap | **new** | Observed in historic run; not reproduced; cause not reproduced | E3; cause E5 |
| UX-1 (F-03) | carried from R2 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 |
| UX-3 (F-04) | carried from R2 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 |
| UX-4 (F-05) | carried from R2 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 |
| UX-7 (F-02) | carried from R2 | REPRODUCED at the native layer (CR-06-S3); tap path SOURCE-CONFIRMED-NOT-REPRODUCED | E1 native; E2 tap path |
| UX-8 (F-01) | carried from R2 | SOURCE-CONFIRMED-NOT-REPRODUCED (= CR-06-S6) | E3 rects + E2; E1 arithmetic only |
| UX-10 (F-06) | carried from R2 | Observed in historic run; not reproduced | E3 |

**Function chains:** 42 rows; 26 are verified end to end, 15 have at least one UNVERIFIED step, and 1 is split. The commonest gaps are:

- an Undo that is recorded but never executed by a test (rename, hide, duplicate, exact transform);
- controls exercised only through `performClick`, never a real touch;
- the Revolve, Export and Import controls, which are absent from runs A, B and C.
