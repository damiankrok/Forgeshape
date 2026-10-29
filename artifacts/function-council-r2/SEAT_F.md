# FUNCTION-COUNCIL-R2 — Seat F: Android UI/UX, interaction, responsive design, accessibility

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (branch `audit/function-council-r2-full-product-ux-platform-r1`, `app/src/main` = `f3db953`).

This seat did research only:

- Nothing under `app/`, `testdata/`, `scripts/`, `.github/`, the Gradle files or the owner documents was modified.
- No Gradle was run.
- Scratch work went in `/tmp/claude-0/fc2-scratch/F/`.

Seat outputs:

- `ANDROID_UI_UX_AUDIT.md`
- `UI_SCREEN_STATE_MATRIX.tsv` (36 states)
- `SCREEN_EVIDENCE_INDEX.md`
- `work/req_F.tsv` (87 rows, `UI-001`..`UI-087`)

## Scope

The Android presentation layer as a user meets it:

- **Screens and surfaces:**
  - the start pages (Home, New Project, Settings);
  - the Global Toolbar with the ForgeShape mark and project drawer;
  - the Objects capsule, its rows and the row command strip;
  - Add Primitive;
  - the spatial and by-name sketch support chooser;
  - the Tool Rail;
  - the orientation navigator;
  - the sketch actions palette;
  - the Line and persistent dimensions;
  - the extrusion HUD, action dock and palette;
  - the Revolve ring and angle label;
  - History (Steps/Sketches/Features);
  - the Property Inspector;
  - the Freeform and Surface surfaces;
  - Sculpt (brush column, History navigator, Mask);
  - Import/Export/Transfer entry points;
  - the unsaved and recovery questions.
- **Checks:**
  - 48 dp hit areas;
  - occlusion, hit ownership and z-order;
  - controls drawn where native refuses;
  - System Back;
  - content descriptions, TalkBack and focus;
  - IME and decimal input;
  - orientation and window classes;
  - font scale;
  - palettes and contrast;
  - handedness;
  - gesture conflicts;
  - stylus and mouse.

Out of scope here:

- native correctness of CAD, sculpt, codec and picking (seats B–E/G), cited only where the UI depends on it;
- shell journeys (Seat A).

## Sources read

- **Owner docs:**
  - `CLAUDE.md`: UI rules, the vocabulary, APP-H1, SKETCH-UX-R1 C/E, CAD-UX-S1, CAD-VERTICAL-SLICE-R1 HUD, UI-PREF-R1, SEL-OUT-R1, SCULPT-H1, Stage 018A, MIRROR-01, MODELING-FOUNDATIONS-R1.
  - `PRODUCT.md`: Starting a project, lines 131-202, including the System Back paragraph at 196-199.
- **Prior audits and run records:**
  - `artifacts/function-council-r2/BEFORE.md`;
  - `artifacts/function-council-r2/SEAT_A.md` and `SEAT_C.md`, to avoid duplicate ids and to cross-reference C-01 and A-05;
  - `artifacts/function-council-r1/COUNCIL_FINDINGS.md` (older tree `a1b50f2`, used as leads only);
  - `artifacts/function-council-c1-d1/SUMMARY.md`.
- **Contrast and evidence dossiers:**
  - `artifacts/ui-pref-r1/CONTRAST.md`, against `colors.xml`, which is unchanged since `fd88db8`;
  - the stage dossiers listed in `SCREEN_EVIDENCE_INDEX.md`;
  - `docs/ui/wireframes/WF-1..WF-4` (listed; SVG wireframes, not a measured spec).
- **Fresh evidence (this audit):**
  - `evidence/device-run-40797031704/{summary.json, environment-manifest.txt, instrumentation-junit.xml, screens/*/facts.txt, screens/ui-3d-state-audit-r1/*.tsv}`: CI DEVICE, 32/32, 25/25 tokens;
  - `evidence/local/{host_selftests.txt (HOST_SELFTESTS_OK 4392/0), release_guard.txt (RELEASE_SELFTEST_GUARD=PASS), run_status.txt}`;
  - `app/build/test-results/testDebugUnitTest/*.xml`: JVM 196 tests, 0 failures, timestamps 2026-10-29T00:57Z.
- **Code:** Java sources in `app/src/main/java/com/forgeshape/app/` (85 files), `res/values/{ids*,strings*,dimens*,colors,themes,attrs}.xml`, `AndroidManifest.xml`, `app/build.gradle` (min 26, target and compile 36).

## Inspected code paths

| File:line | Function | Why |
| --- | --- | --- |
| `ForgeShapeActivity.java:86-160` | `installBackDismissal`, `onBackPressed`, `BackDismissal` | Two Back routes (API < 33 override, API ≥ 33 dispatcher armed only while `hasDismissibleSurface()`) |
| `ForgeShapeActivity.java:402-430` | `onKeyDown` | Debug key hooks → `debugMeshCommand`, NDEBUG-guarded at `forgeshape_jni.cpp:2267-2271` |
| `EditorWorkspaceView.java:1112-1117` | `hasDismissibleSurface` | Arming predicate for API ≥ 33 |
| `EditorWorkspaceView.java:1149-1230` | `dismissTopmostSurface` | Ordered Back decision |
| `EditorWorkspaceView.java:2058-2068` | `bootstrapActive`, `bootstrapVisible` | Bootstrap = no project and (chooser or sketch) |
| `EditorWorkspaceView.java:2363-2375` | `beginCadBootstrap` | Status instruction string |
| `EditorWorkspaceView.java:4630-4642` | `onRevolveEndRequested` | Target of the Back revolve branch |
| `EditorWorkspaceView.java:4927-4945` | `onCancelSketchRequested` | Bootstrap cancel: no question |
| `EditorWorkspaceView.java:6534-6538` | `anchoredSurfaces` | The 8 surfaces Back knows; the two palettes are not among them |
| `EditorWorkspaceView.java:1240-1300` | inset listener, `applyChromeInsets` | IME and inset handling; full-bleed viewport |
| `EditorWorkspaceView.java:4308` | `announceForAccessibility` | The only announcement in the product (snap kind) |
| `EditorControlStyles.java:366-382` | `chip` | Minimum HEIGHT only |
| `EditorControlStyles.java:433-469` | `setChipActive`, `setChipEnabled` | `setActivated` only; disabled = alpha 0.45 |
| `EditorControlStyles.java:537-555` | `iconButton` | Content description required |
| `SketchOrientationNavigatorView.java:73-160, 166-200, 237-262` | constructor, `arrow`, `refreshFromNative`, `anchoredParams`, `onMeasure` | Weighted Flip chip; 184 dp cap |
| `SketchDimensionLabelView.java:79-146` | constructor | Value chip without a minimum width; field key listener `"0123456789.-"` (:112) |
| `SketchDimensionLabelsView.java:126, 174-177` | — | Persistent labels: minimum width 48 dp; field accepts `,` |
| `BodyDimensionLabelsView.java:125-128` | — | `"0123456789.-"` |
| `CadExtrudeCanvasView.java:480-505, 642-645, 1053-1140` | `IconControl`, field factory, palette open/close | HUD hit box; `"0123456789.-"`; palette state outside Back |
| `CadRevolveAngleLabelView.java:85-88` | — | Accepts `,` |
| `NumericPropertyRow.java:105-128` | — | Accepts `,` |
| `LengthUnit.java:129` | `parse` | Replaces `,` with `.` |
| `DisplaySettingsPopoverView.java:230-300, 318-338` | chips | Wrap chips, `setChipActive` |
| `SketchModifyView.java:97-136, 208, 254-287` | palette | `paletteOpen` outside Back; offset field accepts `,` |
| `GlobalToolbarView.java:446` | status | `statusText`: no live region |
| `ToolRailView.java:201, 242` | — | Rail description; active = `setActivated` |
| `ForgeShapeSurfaceView.java:84, 143-155` | pressure capture, `onHoverEvent` | Stylus hover; no `onGenericMotionEvent` (mouse wheel) |
| `PointerSemantics.java:54-66` | `neutralToolType` | Mouse mapped |
| `forgeshape_jni.cpp:313-320, 6611-6648` | `beginSketchView`, `sketchSetViewFlipped`, `sketchRotateView` | Camera framed from `frame()`, not `viewFrame()` |
| `forgeshape_sketch_session.cpp:648-668` | `viewFrame` | No production caller (grep) |
| `WorkspaceLayoutMode.java` | `forWindow` | 600 / 840 dp, 480 dp height |
| `AndroidManifest.xml:31` | `configChanges` | No recreation on rotation |

## Test coverage of the domain (what is actually asserted)

**Host suites.** 25 suites, 4392 checks, fresh. They assert native policy that the UI reads,
for example `SKETCH_UX` navigator algebra (`forgeshape_sketch_ux_selftest.cpp:490-593`:
`viewFrame` right-handedness). None asserts a camera pose after Flip.

**JVM**, fresh, 196/0. Pure presentation policies:

| Class | Tests | What it pins |
| --- | --- | --- |
| `CadHudPresentationTest` | 22 | Value precision, reading angle, 48 dp floor independent of scale |
| `CadHud3dPresentationTest` | 6 | Dock homography and hit quad |
| `CadHudTouchAttributionTest` | 3 | Touch attribution |
| `SketchChromePolicyTest` | 4 | What a sketch shows in each state |
| `ProjectDrawerPolicyTest` | 9 | Drawer closes first, then one handler |
| `WorkspaceLayoutModeTest` | 15 | 600/840/480 thresholds, inspector placement |
| `ChromeMotionTest` | 8 | Reduced motion |
| `DisplaySettingsContractTest` | 6 | — |
| `AppPreferencesTest` | 13 | Clamp/default rules |
| `AppThemeTest` | 10 | — |
| `LengthUnitTest` | 6 | Parser accepts `"1,5"` (`acceptsACommaDecimalSeparatorAndASignedValue`) |
| `SketchDraftingPresentationTest` | 20 | — |
| `FeatureHistoryPresentationTest` | 9 | — |
| `CadRevolvePresentationTest` | 8 | — |
| `FreeformPresentationTest` | 6 | — |
| `SurfacePresentationTest` | 7 | — |
| `PointerSemanticsTest` | 6 | — |
| `SketchBrowserPresentationTest` | 17 | — |
| `EditorUiStateTest` | 10 | — |
| `DiagnosticLogTest` | 11 | — |

**Instrumented, fresh in CI DEVICE `40797031704`** (emulator):

- `EditorWorkspaceLegibilityTest`, 20 tests:
  - `uilr101` compact portrait anchors and targets; `uilr102` short landscape;
  - `uilr103` six named trailing controls ≥ 48 dp at font 1.3; `uilr104` IME;
  - `uilr105`, `uilr115`: no displacement; `uilr106`: real `KEYCODE_BACK` closes Add Primitive and the app stays;
  - `uilr107`–`uilr110`: field behaviours; Apply reachable without scrolling;
  - `uilr111`: contrast; `uilr112`: gizmo handles; `uilr116`, `uilr117`: Back via each surface's own exit, primary exclusivity;
  - `uilr118`–`uilr120`.
- `Ui3dStateAuditTest`, 9 tests: visibility ledger 119/119 PASS; spatial ledger 68 PASS, 3 CLAMPED, 1 NOT_MEASURED.
- `SketchUxVisualEvidenceTest`, `UiPrefVisualEvidenceTest`, `SelectionOutlineVisualEvidenceTest`: 1 journey each, writing rect facts plus captures.

**Instrumented, historical** (C2 runs `40776968165` and `40782861799`, and earlier):

- `CadRetainedSketchOwnerTest`:
  - `d12` landscape/portrait acts ≥ 48 dp and own their centre;
  - `d17` History acts at font 1.3 and 1.5, portrait and landscape: shown, ≥ 48 dp, inside the window, own centre, no overlap;
  - `d15`/`d16`/`d19`–`d22` chooser hit ownership and stylus hover.
- `HomeFlowTest.e2eAppH1_11`: Back in each shell phase, with nothing open.
- `SettingsPreferencesTest`; `EditorWorkspaceThemeTest`; `EditorWorkspaceRightHostPlacementTest` (font 1.3); `EditorWorkspaceChromeCompositionTest`; `EditorWorkspaceMobileTest`; `SculptHistoryNavigatorTest.e2eSchnav09` (Back closes the navigator first); `CadHud3dOwnerTest`.
- `SketchUxTest.cadUxR1_11_12_13` asserts the navigator's **native bits only**.

**Not asserted anywhere:**

- a sweep over every clickable view for the 48 dp floor;
- Back with a surface or palette open inside the bootstrap;
- Back during an in-project Revolve on API 33 or later;
- the comma separator at the field level;
- TalkBack semantics, selected state, live regions or focus order;
- font scale ≥ 1.5 outside History;
- tablet or foldable windows;
- mouse input.

Fixtures: none in this domain. The UI writes no `.forge` bytes of its own.

## Confirmed findings

### F-01 — P2 — Several chrome controls fall below the 48 dp hit-area floor in width

- **Verdict:** DEFECT-CONFIRMED. **Grade:** E1 (rects measured in this audit's CI DEVICE run) + E2 (cause).
- **Location:** `EditorControlStyles.java:366-382`, where `chip()` sets only `setMinimumHeight(control_height)`. Instances:
  - **`sketch_navigator_flip`:** 95x126 px = **36.2 x 48 dp**. A weighted 0-width child, `SketchOrientationNavigatorView.java:134-137`. Source: `screens/cad-a3-c2-sketch-ux-r1/facts.txt` capture 04.
  - **`view_selection_outline_on`:** 97x126 px = **37.0 dp** wide. `view_selection_outline_off`: 103x126 px = **39.2 dp** wide. Wrap chips, `DisplaySettingsPopoverView.java:276-299`. Source: `screens/sel-out-r1/facts.txt`. Grid On/Off are built identically at `:230-250` but were not measured.
  - **`sketch_dimension_value`** (container `sketch_dimension_label`): 114x126 px = **43.4 dp** wide. Source: capture 07. The persistent dimension chips in the same feature do set a minimum width (`SketchDimensionLabelsView.java:177`); the selected-Line chip (`SketchDimensionLabelView.java:82-90`) does not.
- **Minimal repro:** run `SketchUxVisualEvidenceTest` or `SelectionOutlineVisualEvidenceTest`. Read the `width_px` of those ids and divide by 2.625.
- **Impact:** These are hit areas, not glyphs, and they fall below the floor `CLAUDE.md` makes contractual ("48 dp is the interactive floor … it is the HIT AREA"). Mis-taps are likely on the navigator, where Flip sits 4 dp from two 48 dp arrows.
- **Fix:** `chip.setMinimumWidth(dimen(control_height))` in `chip()`. For the navigator's weighted row, keep weight but give the Flip chip a minimum width, and let the navigator take up to its existing 184 dp cap. Then add a sweep assertion (F-09).

### F-02 — P2 — The orientation navigator's Flip and ±90° change no camera; only the label changes

- **Verdict:** DEFECT-CONFIRMED (also DOC-DIVERGENCE against `CLAUDE.md` SKETCH-UX-R1 C, "`SketchSession::viewFrame` derives the camera's frame"). The same defect as **Seat C C-01** and FUNCTION-COUNCIL-R1 D2, re-verified here on `70a6c14`.
- **Grade:** E2 + E1 (Seat C's host diagnostic) + E1 (screen comparison below).
- **Location:**
  - `forgeshape_jni.cpp:313-320`: `beginSketchView` frames `sketchSession().frame()`.
  - `:6611-6648`: `sketchSetViewFlipped` and `sketchRotateView` set state, then call `beginSketchView`.
  - `forgeshape_sketch_session.cpp:648`: `viewFrame()` has no production caller. `grep -rn viewFlipped|viewQuarterTurns` outside the session and selftests finds only `sketchViewState`.
- **UI evidence (this audit):**
  - Compared capture `04_navigator_default_plus_z.png` with `06_navigator_rotated_plus_90.png`, both from the fresh run. The navigator rectangle and the bottom gesture bar were blacked out, then compared with `compare -metric AE -fuzz 5%`.
  - Every remaining differing pixel lies in the box `921x259+32+32`: the toolbar and status line. None lies in the viewport.
  - For contrast, the plane change in capture `05` differs over `1080x980+0+220`, a re-framed viewport.
  - The grid alone is nearly symmetric under a 90° roll, so the image comparison is **consistent with** the source proof but is not proof by itself.
  - Commands are under Counterexample attempt 3.
- **Repro:** open a sketch, tap Flip. The label reads "Along −Z" while the view is unchanged.
- **Impact:** two drawn controls with no visible effect, against "a control that looks like it works and does not is worse than an absent one".
- **Fix:** frame `viewFrame()` in `beginSketchView`. `viewFrame()` equals `frame()` when no view state is set, so every other caller is unchanged. Add a camera-pose assertion to `SketchUxTest`.

### F-03 — P2 — In the CAD bootstrap, System Back discards the whole drawn sketch even when a panel is open

- **Verdict:** DEFECT-CONFIRMED. **Grade:** E2 (no runtime reproduction in this audit).
- **Location:** `EditorWorkspaceView.java:1177-1184`. The `bootstrapVisible()` branch calls `onCancelSketchRequested()` before:
  - the row-strip branch (`:1196-1201`);
  - the anchored-surface branch (`:1202-1229`; the inspector, i.e. the precision surface, is an anchored surface);
  - any palette check. `SketchModifyView.paletteOpen` (`:97`) and `CadExtrudeCanvasView.paletteOpen` (`:201`) are in neither `hasDismissibleSurface` (`:1112-1117`) nor `dismissTopmostSurface`.
- **Effect:** `onCancelSketchRequested` (`:4927-4940`) cancels the session and returns Home with no question.
- **Repro:**
  1. Home → New Project → CAD.
  2. Draw a rectangle.
  3. Tap the precision toggle (the sliders icon at the bottom of the rail, visible in capture `03`), or Finish Sketch and tap the action dock.
  4. Press System Back.
  5. Expected: the panel or palette closes. Actual by source: the sketch is cancelled and Home is shown.
- **Impact:** the user loses every drawn entity by pressing Back to close a panel. The sketch is volatile, so this is not project data loss, but there is no undo. The code's own comment (`:1150-1154`, "innermost outward") and `uilr106` ("Back dismisses the topmost surface before it leaves") state the contract this breaks.
- **Not covered by tests:**
  - `HomeFlowTest.e2eAppH1_11` tests bootstrap Back with nothing open.
  - `Ui3dStateAuditTest.ui3d13` tests Back with surfaces open in a Construction project only (`Ui3dStateAuditTest.java:715-748`).
- **Fix:** move the bootstrap branch after the row-strip, anchored-surface and palette branches. Add both palettes, and the Line dimension and revolve-angle editors, to both predicates.

### F-04 — P2 — On API 33+, Back during an in-project Revolve backgrounds the app; below API 33 it returns to Extrude

- **Verdict:** DEFECT-CONFIRMED. **Grade:** E2.
- **Location:**
  - `dismissTopmostSurface` handles the case at `EditorWorkspaceView.java:1166-1175`.
  - `hasDismissibleSurface` (`:1112-1117`) does not test `CadRevolvePresentation.extrudeInsteadShown`.
  - `ForgeShapeActivity.BackDismissal` (`:121-160`) registers the `OnBackInvokedCallback` only while that predicate is true. Inside a project (not the bootstrap), with no anchored surface open, nothing arms it.
- **Repro:**
  1. Open a project → New Sketch → XY → draw a closed profile → Finish → Revolve (`revolve_sketch`, `GlobalToolbarView.java:322`).
  2. Press System Back on an API 33+ device. The CI emulator is API 36 and `targetSdk` is 36.
- **Impact:**
  - The same press means two different things by API level.
  - On current Android the documented one-step Back ("A Revolve in progress is one step inside Ready: Back returns to the extrusion") is not delivered.
  - The task is moved to the background with the revolve still staged.
- **Fix:** derive `hasDismissibleSurface()` from the same ordered predicate list `dismissTopmostSurface()` walks, so they cannot diverge again. Add a device test that sends `KEYCODE_BACK` in this state (as `uilr106` does).

### F-05 — P2 — Three canvas editors silently drop the comma decimal separator, so "2,5" becomes 25

- **Verdict:** DEFECT-CONFIRMED. **Grade:** E2 (key-listener sets) + E1 (`LengthUnitTest.acceptsACommaDecimalSeparatorAndASignedValue` PASS, fresh JVM run: the parser accepts `"1,5"`).
- **Location:** `DigitsKeyListener.getInstance("0123456789.-")` in:
  - `BodyDimensionLabelsView.java:127`
  - `CadExtrudeCanvasView.java:644` (depth and second distance)
  - `SketchDimensionLabelView.java:112` (Line length)

  Four other fields accept `,`:

  - `NumericPropertyRow.java:109`
  - `CadRevolveAngleLabelView.java:87`
  - `SketchDimensionLabelsView.java:126`
  - `SketchModifyView.java:208`

  `LengthUnit.parse` (`:129`) and `ExactExpression.parse` both read `,` as `.`.
- **Repro:** on a keyboard that types `,` as the decimal key, tap the extrude value → type `2,5` → Apply. `DigitsKeyListener` filters out characters outside its set, so the field holds `25` and a 25 m depth passes validation.
- **Impact:** a silent order-of-magnitude error in authored truth. The same feature accepts `2,5` in the precision surface but not on the canvas, which is also a consistency defect.
- **Fix:** use `"0123456789.,-"` in the three listeners.
- **Unverified:** which IMEs emit `,` under `TYPE_NUMBER_FLAG_DECIMAL` (see H-F2).

### F-06 — P3 — In Sculpt the status line is overlapped by the brush column and the Tool Rail

- **Verdict:** DEFECT-CONFIRMED against "a surface may never partially cover another live control". **Grade:** E1, fresh captures.
- **Evidence:**
  - `screens/ui-3d-state-audit-r1/ui3d11_01_sculpt.png`: the text "Now sculpting —" runs under `brush_edge_controls`.
  - `ui3d11_05_resume.png`: the two-line "One finger on the model — …; elsewhere it orbits." is clipped on the left by the brush column and on the right by the rail.
  - Crops: `/tmp/claude-0/fc2-scratch/F/sc.png`, `sc2.png`.
- **Location:** the status pill is anchored under the toolbar (`GlobalToolbarView.java:446`). In Sculpt the brush column (`BrushEdgeControlsView`) and the rail start at the same y.
- **Impact:** Sculpt entry and resume verdicts are partly unreadable. They are transient, so P3.
- **Fix:** in Sculpt, bound the pill's width between the brush column's trailing edge and the rail's leading edge, or offset the brush column below the pill.

### F-07 — P3 — Selected state and verdicts do not reach a screen reader

- **Verdict:** PARTIAL (gap). **Grade:** E2.
- **Location:**
  - `EditorControlStyles.setChipActive` (`:433-439`) and the rail (`ToolRailView.java:242`) use `setActivated` only, which TalkBack does not announce. This covers transform mode and space, plane chips, unit chips and Display choices.
  - `status_message` has no accessibility live region.
  - The product calls `announceForAccessibility` exactly once (`EditorWorkspaceView.java:4308`, snap kind).
  - Settings rows (`SettingsPageView.java:216`), the CAD palette (`CadExtrudeCanvasView.java:714`), History rows (`FeatureHistoryView.java:349,366`) and Isolate (`SculptContextView.java:172`) do call `setSelected`. The treatment is inconsistent.
- **Impact:** a TalkBack user cannot tell which tool or mode is active, and hears no refusals.
- **Fix:** add `setSelected(active)` in `setChipActive` and on the rail, and `setAccessibilityLiveRegion(POLITE)` on `status_message`.
- **Note:** there is no OWNER accessibility specification beyond names and "shape + accessible label". The severity is advisory.

### F-08 — P3 — Light Charcoal focus ring 2.48:1 against the precision surface

- **Verdict:** OUT-OF-SCOPE (recommendation). **Grade:** E1 (recomputed: `#4C8FD6` on `#4B4E50` = 2.48) + E3 (`CONTRAST.md` reports it "for information").
- **Detail:** below WCAG 1.4.11's 3:1 for non-text. The palette anchors are owner-approved, so this needs an owner decision, not a fix.

### F-09 — P3 — No test sweeps every clickable view for the 48 dp floor

- **Verdict:** PARTIAL (test-coverage gap). **Grade:** E2.
- **Location:**
  - `assertTouchFloor` (`EditorWorkspaceLegibilityTest.java:1116-1139`) checks six ids.
  - `assertActsUsable` (`CadRetainedSketchOwnerTest`) checks the History act ids.
  - `uilr2c_11` checks "intrinsic targets".
  - Nothing walks the view tree.
- **Impact:** this is how F-01 shipped while the visual-evidence runs that measured it passed.
- **Fix:** one helper that walks `isShown() && isClickable()` views per captured state and asserts ≥ 48 dp. Exempt by id, with a reason.

### F-10 — INFO — Mouse-only use cannot zoom or pan

- **Verdict:** FUTURE-NOT-DUE. **Grade:** E2.
- **Detail:** `PointerSemantics.java:61` maps `TOOL_TYPE_MOUSE`, but there is no `onGenericMotionEvent` (wheel), no secondary-button handling, and no on-screen zoom control (no such id in `ids*.xml`). With one pointer, pinch and two-finger pan are impossible.
- **Owner decision:** whether Android desktop windowing and ChromeOS are targets.

### F-11 — INFO — The Settings page's own Back control is reachable only after scrolling

- **Verdict:** OUT-OF-SCOPE (recommendation). **Grade:** E1.
- **Detail:** `settings_back` is at y = 3409–3535 px in a 2400 px window (`screens/ui-pref-r1/facts.txt`). System Back works (`SettingsPreferencesTest.uiprefr1_01`).
- **Recommendation:** a top Back.

### F-12 — P3 — The status line carries instructions

- **Verdict:** DOC-DIVERGENCE against `CLAUDE.md` "Chrome reports, it does not instruct". **Grade:** E1 (captures 03 and `ui3d11_05`) + E2.
- **Strings:**
  - `status_bootstrap_sketch`, "Draw a closed profile, then Extrude to create the project" (`strings.xml:54`, shown at `EditorWorkspaceView.java:2374`);
  - `sculpt_gesture_rule`, "One finger on the model — %1$s; elsewhere it orbits." (`strings.xml:753`).
- **Detail:** both are shown on entry and are not always-true permanent surfaces. `uir4c09` guards resting Construction only.
- **Owner decision:** are first-entry hints permitted?

## Hypotheses (unproven)

- **H-F1.**
  - **Claim:** the navigator plane chips XY/XZ/YZ are about 45 dp wide.
  - **Basis:** computed from the measured 375 px inner width, three weighted chips and two 4 dp gaps: about 118 px each. Not measured (E5).
  - **Disproof:** read their rects in a visual-evidence run.
- **H-F2.**
  - **Claim:** common IMEs (e.g. Gboard with a pl/de locale) emit `,` from the decimal key for `TYPE_NUMBER_FLAG_DECIMAL`, so F-05 is hit in normal use.
  - **Status:** not tried on a device. F-05 stands whenever `,` is typed by any means, for example from a hardware keyboard.
- **H-F3.**
  - **Claim:** System Back in an in-project sketch with nothing open backgrounds the app rather than cancelling the sketch.
  - **Basis:** `hasDismissibleSurface` is false. This is source-derived; the intended behaviour is undocumented, so it is not a defect (UI-083).
- **H-F4.**
  - **Claim:** at font scale 1.5 or 2.0 the navigator's single-line Flip chip ("Along +Z" text in a 36 dp share) clips its label.
  - **Status:** no test exercises it.
- **H-F5.**
  - **Claim:** the sketch-dimension editor stands flush with the window's right edge (`dimension_editor` x2 = 1080 px, capture 08), so with the system gesture inset its Apply may sit inside the back-gesture zone.
  - **Status:** not measured.

## Adversarial counterexample attempts

1. **Is the Flip chip's small width compensated by a TouchDelegate or a parent click handler?**
   - Searched for `TouchDelegate`, `setTouchDelegate` and parent `OnClickListener` across the Java sources. There are none in `SketchOrientationNavigatorView`. The 95 px view is the hit area.
   - Outcome: F-01 stands.
2. **Can the precision surface really be open in the bootstrap, or does something close it before Back runs?**
   - Capture `03_immediate_flat_sketch.png` (fresh) shows the precision toggle on the bootstrap rail.
   - `dismissPrimarySurfacesExcept(null)` has many call sites, among them bootstrap entry (`:2371`) and inside `onCancelSketchRequested` (`:4931`). The second runs only after Back has already chosen to cancel the sketch, so nothing closes the surface first.
   - No `TouchDelegate` exists anywhere in the Java sources.
   - No test drives Back with a surface open in the bootstrap (`grep systemBack|dismissTopmostSurface` over `androidTest`).
   - Outcome: F-03 stands; runtime reproduction is still owed.
3. **Is the navigator ±90° capture identical only because the viewport is empty and symmetric?**
   - Partly yes; that is why F-02 rests on source (E2), with Seat C's host diagnostic (E1).
   - Commands, both in `/tmp/claude-0/fc2-scratch/F`:
     - `convert <cap> -fill black -draw "rectangle 442,336 859,691" -draw "rectangle 0,2250 1080,2400" a.png`
     - `compare -metric AE -fuzz 5% a.png b.png null:`
   - The remaining diff is confined to the toolbar box. The plane change (capture 05) does move the viewport, so the method can detect a re-frame.
4. **Does the revolve Back gap disappear because `refreshShellPhase` arms the callback while sketching?**
   - `refreshShellPhase` (`:2039-2041`) passes `hasDismissibleSurface()`, which has no sketch or revolve term outside the bootstrap.
   - Outcome: F-04 stands.
5. **Is F-05 already prevented by IME type?**
   - The fields use `TYPE_NUMBER_FLAG_DECIMAL`, but `setKeyListener(DigitsKeyListener)` replaces the input filter. Characters outside the set are dropped, not rejected with a message.
   - Outcome: F-05 stands.
6. **Contrast drift since `CONTRAST.md`?**
   - `git log -- app/src/main/res/values/colors.xml` shows one commit, `fd88db8`, which also added `CONTRAST.md`. The spot recompute matches (2.48 and 5.31).
   - Outcome: the PASS stands.

## Positive claims with evidence

| Claim | Evidence |
| --- | --- |
| Home and New Project are opaque full-window pages; New Project replaces Home; CAD enters an XY sketch at once with no project | E1: capture facts (`home_is_full_window=true`, `home_still_visible=false`, `support_chooser_active=false project_open=false`) |
| Back closes the topmost anchored surface and the app stays, through the real platform key route | E1: `uilr106` sends `KEYCODE_BACK` on API 36 (fresh PASS); E3 `HomeFlowTest.e2eAppH1_11` |
| Trailing-host controls keep ≥ 48 dp at font 1.3 and with the IME up; Apply is reachable without scrolling in portrait and short landscape | E1: `uilr103`, `uilr104`, `uilr110` (fresh) |
| One primary surface at a time; Display closes the inspector; the visibility ledger is clean | E1: `Ui3dStateAuditTest` `VISIBILITY.tsv` 119/119 PASS |
| World-anchored chrome tracks its projection to within 0.09–0.44 dp; an off-screen value is hidden, not guessed | E1: `SPATIAL_ATTACHMENTS.tsv` (68 PASS, 3 CLAMPED, 1 NOT_MEASURED with reason) |
| Handedness moves the host to the left edge (x 880 → 21 px) and the side panel opens inward | E1: ui-pref facts 14–16 |
| The five palettes meet 4.5:1 for primary, secondary and error text on six grounds | E3: `CONTRAST.md`; E1 spot recompute |
| History acts are usable at font 1.3 and 1.5, portrait and landscape | E3: `CadRetainedSketchOwnerTest.d17` (C2 runs) |
| HUD touch targets stay ≥ 48 dp independent of the scale band | E1: JVM `CadHudPresentationTest` |
| Debug key hooks and touch attribution are absent from release | E1: `RELEASE_SELFTEST_GUARD=PASS` (fresh) |

## Journeys proposed for END_TO_END_JOURNEYS

- **J-F1 (negative / fail-closed, currently failing — F-03).**
  1. Home → New Project → CAD → draw a rectangle → open the precision surface → System Back.
  2. Expected: the precision surface closes, the sketch keeps 1 entity, Home is not shown.
  3. Then Finish Sketch → open the action dock palette → Back. Expected: the palette closes, Ready stands.
  4. Assert `sketchState`, `homeVisible()==false`, and the entity count.
- **J-F2 (negative, currently failing on API ≥ 33 — F-04).**
  1. Open a Construction project → New Sketch → XY → rectangle → Finish → Revolve → send `KEYCODE_BACK` through the instrumentation.
  2. Expected: revolve ended, Ready on Extrude, `activity.isFinishing()==false`, task in front.
- **J-F3 (currently failing — F-02, F-01).**
  1. New CAD → capture the camera pose → tap `sketch_navigator_flip`.
  2. Expected: the view normal is reversed and the camera frame equals `viewFrame()`.
  3. Tap `sketch_navigator_rotate_cw`. Expected: a roll of 90°.
  4. Assert every navigator control is ≥ 126 px both ways at 420 dpi.
- **J-F4 (negative — F-05).**
  1. Staged One Side extrude → open the depth editor → type `2,5` → Apply.
  2. Expected: depth 2.5 m, or an explicit refusal. Never 25 m.
  3. Repeat for the Line length and the body dimension.
- **J-F5 (positive regression guard).** At font scale 1.5, in portrait and landscape, walk Home, New Project, the bootstrap sketch, Ready, the precision surface, the Display popover, Objects, Settings and Sculpt. Assert every shown clickable view is ≥ 48 dp, owns its centre, and lies inside the window.

## Owner decisions needed

1. What System Back means inside a project sketch with nothing open, and whether Back in the bootstrap with drawn geometry should ask before discarding (F-03, H-F3, UI-083).
2. Whether first-entry hints may use the status line (F-12).
3. The Light Charcoal focus ring at 2.48:1 (F-08).
4. Whether Android desktop windowing, ChromeOS, mouse and hardware keyboard are targets (F-10, UI-079/080).
5. Which form factors (tablet, foldable, split-screen) must carry runtime evidence (UI-047).
6. Whether TalkBack support is a product requirement, with a specification (F-07).
7. A physical-device session for stylus hover and pressure (UI-033, UI-081). These are OWNER-PHYSICAL-UNVERIFIED.

## Unresolved dependencies and open questions

- F-02 is shared with Seat C (C-01). The synthesis should carry one id.
- A-05 (Seat A, "saved" baseline lost on Activity recreation, FC-R1 D3) touches the palette-change journey. Seat F did not re-verify it.
- No visual evidence on this tree for the Freeform, Surface, History, Mirror chooser and question surfaces (`SCREEN_EVIDENCE_INDEX.md` §3).
- The coordinator's fresh CI DEVICE run is cited where its outputs existed in `evidence/device-run-40797031704`. The section below is left for its appendix.
- The canonical Master Plan and OWNER_DECISIONS are not available. Horizons are reconstructed from `CLAUDE.md` rule ids and `PRODUCT.md`.

## Fresh CI DEVICE evidence (appended by coordinator)

_Reserved for the coordinator._

**Completeness: PARTIAL.**

- Every screen in scope was inventoried from source, and the measured states were checked against fresh emulator facts.
- F-03, F-04 and F-05 are proven from source (E2) and were not reproduced at runtime. Gradle and device runs were not available to this seat.
- TalkBack, tablet, foldable, mouse and physical-stylus behaviour are UNKNOWN or OWNER-PHYSICAL-UNVERIFIED.
- The plan-level sources (Master Plan, OWNER_DECISIONS) are not available.
