# FUNCTION-COUNCIL-R2 — Android UI/UX audit (Seat F)

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` = `f3db953`). Research only.

Companion files:

- `SEAT_F.md`: findings, with evidence detail.
- `UI_SCREEN_STATE_MATRIX.tsv`: 36 states.
- `SCREEN_EVIDENCE_INDEX.md`: every screenshot set.
- `work/req_F.tsv`: 87 atomic requirements, `UI-001`..`UI-087`.

Evidence grades and verdicts follow the council vocabulary. Every runtime number here comes from
the emulator (Android 16 / API 36, 1080x2400 at 420 dpi, SwiftShader) on `70a6c14`, across three
fresh CI DEVICE runs, each with 25/25 startup tokens in order (results recorded in `TESTS.md`):

- `40797031704`: 32/32, the visual-evidence classes. Screens: `evidence/device-run-40797031704/screens/`
  (`cad-a3-c2-sketch-ux-r1/` 12 PNG, `sel-out-r1/` 12, `ui-3d-state-audit-r1/` 58, `ui-pref-r1/` 20).
- `40800569636`: 77/77, Surface, Freeform, Sculpt, Import-sculpt, CAD vertical slice, extent and
  SketchUx. Screens: `evidence/device-run-40800569636/screens/` (`cad-vertical-slice-r1-after/` 23 PNG,
  `modeling-freeform/` 6, `modeling-surface/` 9).
- `40805804385`: 90/90, persistence, transfer, process death, objects, mirror, dimensions, lifecycle,
  JNI hardening, diagnostics/renderer loss and settings. No screen set; only
  `evidence/device-run-40805804385/startup-screenshot.png` and `post-test-screenshot.png`.

A PASS in these runs is functional emulator evidence (E1). A captured PNG is visual evidence only
where a row says it was inspected; the `facts-*.txt` files of the two modeling sets carry capture
and byte facts, not view rects, so nothing in them measures a touch target.
**No statement here is a physical-device result.**

Styling judgements carry no OWNER-approved specification beyond what `CLAUDE.md`,
`PRODUCT.md` and the accepted UI-SPEC-R0 record. Where that is so, they are labelled
**recommendation**.

## 1. Navigation map

```
Cold launch ─► HOME (start page)
                 ├─ New Project ─► NEW PROJECT page ─┬─ CAD ──► BOOTSTRAP SKETCH (XY, +Z; no mark)
                 │                                   │            ├─ Finish Sketch ─► READY (extrude HUD, dock ─► action palette)
                 │                                   │            │      ├─ Revolve ─► REVOLVE (ring, angle label)
                 │                                   │            │      ├─ Back to Sketch ─► BOOTSTRAP SKETCH
                 │                                   │            │      └─ Extrude / Revolve commit ─► EDITOR (project)
                 │                                   │            └─ Back to Home / System Back ─► HOME (sketch discarded)
                 │                                   ├─ Sculpt ─► EDITOR in SCULPT (seeded sphere)
                 │                                   ├─ Freeform ─► EDITOR (Freeform Box)
                 │                                   ├─ Surface ─► bootstrap sketch ─► Finish ─► Surface surface ─► EDITOR
                 │                                   └─ Back ─► HOME
                 ├─ Open File… ─► system picker ─► EDITOR | stays HOME on cancel or refusal
                 └─ Settings ─► SETTINGS page ─► Back ─► HOME
EDITOR (project open)
  Global Toolbar: [ForgeShape mark ─► PROJECT DRAWER: New Sketch, New Project…, Save Project, Open Saved
                   Project, Save Copy…, Open File…, Share Diagnostics…, Import GLB…, Settings…]
                  [mode transition: Start Sculpting / Back to Construction / Resume Sculpt] [Export]
                  [Display ─► DISPLAY popover] [Hide UI]
  Tool Rail (trailing host): Construction: Shape, Place(Transform)… | Sculpt: 7 brushes | Sketch: 7 tools
  Precision toggle ─► PRECISION SURFACE (Property Inspector: Shape / Transform / Sketch / Feature /
                       Freeform / Surface editors; bottom sheet, side overlay or side dock by window)
  Objects capsule ─► OBJECTS popover ─► row ─► overflow ─► ROW COMMAND STRIP ─► Mirror ─► MIRROR PLANE CHOOSER
                   └─ + ─► ADD PRIMITIVE palette (6 primitives + New Sketch)
  History capsule: Undo, Redo, [History ─► CAD/Surface HISTORY surface | Sculpt HISTORY NAVIGATOR]
  New Sketch ─► SPATIAL SUPPORT CHOOSER (by-name fallback) ─► SKETCH (navigator, Modify palette) ─► READY …
  Dimensions / Relative Scale (Construction body) ; Edit Sketch chip (retained CAD sketch)
  Questions over a live project: UNSAVED CHANGES (Save / Discard / Cancel), RECOVERY (Recover / Discard)
```

What System Back does in each state (`EditorWorkspaceView.dismissTopmostSurface`,
`EditorWorkspaceView.java:1149-1230`). The checks run in this order:

1. unsaved question → Cancel
2. Settings → back to where it was opened
3. New Project page → Home
4. Revolve inside Ready → Extrude
5. bootstrap → Cancel sketch, then Home
6. support chooser → cancel
7. row command strip → close
8. the topmost anchored surface → close
9. otherwise the platform's Back

**Back gaps:**

- **F-03.** Step 5 runs before step 8, and the two sketch palettes are in no step at all.
- **F-04.** Step 4 is not mirrored in `hasDismissibleSurface`, so on API 33 and later the
  callback is not armed inside a project.

## 2. Objective pass/fail criteria used

| ID | Criterion | Source | How measured |
| --- | --- | --- | --- |
| C-TOUCH | Every user-operated control's hit rectangle is ≥ 48 dp in both directions | `CLAUDE.md` "48 dp is the interactive floor … it is the HIT AREA" | Recorded rects in `facts.txt` (px ÷ 2.625); test `assertTouchFloor` / `assertActsUsable` |
| C-OCCL | No surface partially covers another live control; tap ownership at the control centre | `CLAUDE.md` "No surface owns the resting workspace" | `ownsItsCentre` (d17), visual inspection of fresh captures |
| C-BACK | Back closes the innermost thing first; it means the same on every API level | `CLAUDE.md` APP-H1 "Back is one step in every phase"; `uilr106` | Source walk of both routes; HomeFlowTest e2eAppH1_11; uilr106 |
| C-ABSENT | A control that cannot succeed is not drawn; a drawn control does what it says | `CLAUDE.md` | Source plus test |
| C-INPUT | Numeric fields accept the same syntax the parser accepts | `LengthUnit.parse`, `LengthUnitTest.acceptsACommaDecimalSeparatorAndASignedValue` | Source: `DigitsKeyListener` sets |
| C-CONTRAST | Text roles ≥ 4.5:1 on six grounds, all five palettes | `artifacts/ui-pref-r1/CONTRAST.md` (UILR1-11) | CONTRAST.md, spot recompute in this audit |
| C-A11Y | Icon-only controls named; selected state and verdicts reach a screen reader | Android accessibility guidance (no OWNER spec beyond names and "shape + accessible label") | Source |
| C-FONT | Controls usable at font scale 1.3 and 1.5 | CAD-R2-C1 d17; UILR1-03 | Tests (1.3 for the trailing host, 1.3 and 1.5 for History acts) |
| C-ORIENT | Portrait, short landscape and expanded keep anchors and targets | UI-LAYOUT-R2 | `uilr101/102`, `uilr2c_*`, `WorkspaceLayoutModeTest` |

## 3. Per-screen findings (summary; details in `SEAT_F.md`)

| Screen | Result | Notes |
| --- | --- | --- |
Screenshot paths are relative to `evidence/device-run-<run>/screens/`; `A` = run `40797031704`,
`B` = run `40800569636`.

| Screen | Result | Notes | Screens |
| --- | --- | --- | --- |
| Home / New Project | Supported (E2), **not PASS-REPRODUCED**: the positive claim "opaque full-window pages; New Project replaces Home; New CAD enters an XY sketch at once" was judged overstated in `work/VERIFICATION.md`. Fresh emulator captures and `SketchUxTest.cadUxR1_01_02_03` / `_04` / `_05_06_07` passed in B (E1 for what those tests assert) | Targets 90–108 dp tall. The Freeform and Surface rows appear in no facts file | A `cad-a3-c2-sketch-ux-r1/01_home_full_screen.png`, `02_new_project_page.png`; A `ui-3d-state-audit-r1/ui3d03_01_home.png`, `ui3d03_02_new_project.png`; A `ui-pref-r1/01_home_settings_entry.png` |
| Bootstrap sketch | Layout captured (E1); **F-03** Back ordering (E2; contested against `PRODUCT.md:196-198`, OWNER decision D-5 in `COUNCIL_DELIBERATION.md`) | The status line carries an instruction (F-12, DOC-DIVERGENCE) | A `cad-a3-c2-sketch-ux-r1/03_immediate_flat_sketch.png`; A `ui-3d-state-audit-r1/ui3d03_03_first_sketch.png` |
| Orientation navigator | **F-01** (Flip 36 dp wide, E1); **F-02** (Flip and ±90° move no camera, E1+E2). `SketchUxTest.cadUxR1_08_09_10` and `_11_12_13` passed again in B but assert state bits only (the false-green named in F-02) | Plane chips about 45 dp wide (computed only, H-F1) | A `cad-a3-c2-sketch-ux-r1/04_navigator_default_plus_z.png`, `05_navigator_other_plane.png`, `06_navigator_rotated_plus_90.png` |
| Line dimension | **F-01** (container 43.4 dp wide, E1); **F-05** (comma filtered, E2) | The editor sits flush with the window's right edge (x=1080 px) | A `cad-a3-c2-sketch-ux-r1/07_line_dimension.png`, `08_length_editor_open.png`; A `ui-3d-state-audit-r1/ui3d07_01_line_selected.png` .. `ui3d07_04_tool_changed.png` |
| Arc / Spline / Edit Sketch | PASS function (E1: `SketchUxTest` `cadUxR1_25_27`, `_28_30`, `_32_33_34_35` in B) | — | A `cad-a3-c2-sketch-ux-r1/09_arc_selected.png` .. `12_edit_finished_regenerated.png`; A `ui-3d-state-audit-r1/ui3d03_08_edit_sketch.png`, `ui3d10_08_edit_sketch.png` |
| Extrude HUD / dock / palette | PASS (E1 ledger + JVM; `CadVerticalSliceTest` 9/9 and `CadExtrudeExtentTest` 5/5 in B) | Palette not in the Back chain (F-03) | A `ui-3d-state-audit-r1/ui3d08_01_one_side.png` .. `ui3d08_05_after_zoom_in.png`, `ui3d09_01_symmetric.png` .. `ui3d09_06_back_to_one_side.png`; B `cad-vertical-slice-r1-after/04_compact_hud_one_side.png` .. `07_compact_hud_labels_on.png`, `15_ready_chrome.png`, `16_ready_precision_on_request.png` |
| Regions / Add / Cut | PASS function (E1, `CadVerticalSliceTest` in B) | — | B `cad-vertical-slice-r1-after/01_rectangle_circle_two_regions.png` .. `03_ring_extruded.png`, `08_add_preview.png` .. `14_feature_chain_reopened.png`, `16_union_o_plus_a.png` .. `20_merged_cut.png` |
| Revolve ring | PASS function (E3); **F-04** (Back) | No fresh capture; `CadRevolveOwnerTest` is in none of the three runs | — |
| History surface | PASS (E3, d17 at 1.3 and 1.5) | No screenshot; `CadRetainedSketchOwnerTest` is in none of the three runs | — |
| Property Inspector | PASS (E1, uilr107–110, 116 in A) | — | A `ui-pref-r1/16_left_handed_exact_open.png` |
| Display popover | **F-01** (Selection Outline On 37 dp, Off 39 dp wide, E1) | Grid On/Off are built the same way; not measured | A `sel-out-r1/02_construction_outline_off.png`, `11_left_handed_view_overlay.png` |
| Objects / row strip / Mirror | PASS function (E1, emulator: `ObjectCommandsTest.e2eObj018a01_theRowCommandsRunThroughTheRealControls`, `MirrorSmokeTest.e2eMirror0101_theRowMirrorsThroughTheRealControls` and `ObjectsDeleteTest` 8/8 in `40805804385`) | No capture of the row command strip or the mirror plane chooser (that run has no screen set); visual UNKNOWN | A `ui-3d-state-audit-r1/ui3d06_01_body_b.png` .. `ui3d06_06_after_undo.png`; A `sel-out-r1/12_after_delete_fallback.png` |
| Add Primitive | Layout E3. The Back positive claim ("System Back closes the topmost anchored surface … through the real platform key route on API 36") was judged overstated in `work/VERIFICATION.md`; `uilr106` (A, PASS) is the one platform-key test and covers only this surface in Construction | — | No capture identified |
| Dimensions / Relative Scale | PASS (E1 ledger; `BodyDimensionsSmokeTest` 1/1 in `40805804385`); comma filtered in the dimension fields (F-05) | Body labels CLAMP while sketch and CAD labels HIDE (INFO) | A `ui-3d-state-audit-r1/ui3d04_01_dimensions_open.png` .. `ui3d04_06_after_undo.png`, `ui3d13_02_relative_scale.png` |
| Sculpt | **F-06** (P3): status pill overlapped by the brush column and the rail (E1) | History navigator PASS function (E1, emulator: `SculptHistoryNavigatorTest` 9/9 in `40800569636`; `e2eSchnav09` closes the navigator through `dismissTopmostSurface()`, not the platform key) | A `ui-3d-state-audit-r1/ui3d11_01_sculpt.png`, `ui3d11_02_navigator.png`, `ui3d11_03_mask.png`, `ui3d11_04_back_to_construction.png`, `ui3d11_05_resume.png`; A `sel-out-r1/04_sculpt_selected.png` |
| Settings | PASS (E1; `SettingsPreferencesTest` 15/15 in `40805804385`) | Back control only after scrolling (F-11, recommendation) | A `ui-pref-r1/01_home_settings_entry.png`, `02_settings_page.png`, `03_editor_settings_entry.png` |
| Palettes / handedness / contrast | PASS (E1+E3) | Light Charcoal focus ring 2.48:1 (F-08, recommendation) | A `ui-pref-r1/04_editor_warm_graphite.png` .. `13_home_cool_light.png`, `14_right_handed_editor.png`, `15_left_handed_editor.png`; A `sel-out-r1/09_warm_light.png`, `10_cool_light.png` |
| Gizmo size / weight | PASS (E1) | — | A `ui-pref-r1/17_gizmo_default.png` .. `20_gizmo_largest_bold_rotate.png` |
| Selection outline | PASS (E1, `SelectionOutlineVisualEvidenceTest` in A) | — | A `sel-out-r1/01_construction_selected_dark.png` .. `08_two_bodies_selection_b.png` |
| Freeform surface | PASS function (E1, emulator: `FreeformOwnerTest` 3/3 in `40800569636`). Visual: captured; this audit inspected `03_loop_and_crease.png` — the Cage panel is a bottom sheet under the Objects and history capsules, its content scrolled (a field clipped at the sheet's top edge). No touch rects measured | The facts files carry no view rects | B `modeling-freeform/01_freeform_box.png` .. `06_level_3.png` |
| Surface surface | PASS function (E1, emulator: `SurfaceOwnerTest` 3/3 in `40800569636`). Visual: captured; this audit inspected `04_staged_failure.png` — a staged-edit chip ("Edit Thicken 2") stands under the ForgeShape mark, and the body is drawn partly off the window's right edge. No touch rects measured | The facts files carry no view rects | B `modeling-surface/01_patch.png`, `01_open_wall.png`, `01_revolved.png`, `02_lofted.png`, `02_thickened.png`, `02_trimmed.png`, `03_stitched.png`, `03_first_feature_edited.png`, `04_staged_failure.png` |
| Questions: recovery | PASS function (E1, emulator: `ProjectAutosaveRecoveryTest` 14/14 and `ProjectProcessDeathTest` 8/8 in `40805804385`) | No capture of the recovery question; visual UNKNOWN | — |
| Questions: unsaved changes | PASS (E3) | `HomeFlowTest` is in none of the three runs; no capture | — |

## 4. UX journey scorecard

Scale: 2 = good, 1 = friction, 0 = fails. Every score cites its evidence. Seven criteria:

- **Disc**: discoverability
- **Pred**: predictability
- **Feedb**: state feedback
- **Recov**: error recovery
- **Ergo**: touch and stylus ergonomics
- **Dens**: information density
- **Cons**: consistency

| Journey | Disc | Pred | Feedb | Recov | Ergo | Dens | Cons | Grounding |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| J1 Cold launch → New CAD → sketch → Extrude → project | 2 | 1 | 2 | 1 | 2 | 2 | 2 | `SketchUxTest` `cadUxR1_01_02_03`, `_04`, `_05_06_07` (B, E1; the Home/New Project positive claim stays E2, see §3); HomeFlowTest 01–03 (E3, not in the fresh runs); captures A `cad-a3-c2-sketch-ux-r1/01`–`03` and `ui-3d-state-audit-r1/ui3d03_*`; Back from Ready goes Home, not to the sketch (by design, PRODUCT.md:196-199) |
| J2 Sketch orientation (navigator Flip, ±90°) | 2 | 0 | 1 | 2 | 1 | 2 | 1 | F-02 (no camera change; label changes); F-01 (Flip 36 dp) |
| J3 Type an exact length / depth on a comma-decimal keyboard | 1 | 0 | 1 | 1 | 2 | 2 | 0 | F-05: three fields drop ',' while four accept it |
| J4 Open a panel in the bootstrap sketch, then press Back | 1 | 0 | 1 | 0 | 2 | 2 | 0 | F-03: the whole drawn sketch is cancelled; nothing asks |
| J5 In-project Revolve, then System Back | 1 | 0 | 1 | 1 | 2 | 2 | 0 | F-04: app backgrounds on API 33 and later; returns to Extrude below 33 |
| J6 Enter Sculpt / Resume and read the status | 2 | 2 | 1 | 2 | 2 | 1 | 1 | F-06: text under the brush column and the rail |
| J7 Toggle Selection Outline / Grid | 2 | 2 | 2 | 2 | 1 | 2 | 2 | F-01: 37–39 dp chips |
| J8 Change palette / handedness in Settings | 2 | 2 | 2 | 2 | 2 | 2 | 1 | A `ui-pref-r1/01`–`16`; `SettingsPreferencesTest` 15/15 (`40805804385`); Back control below the fold (F-11) |
| J9 Transform + exact values + IME | 2 | 2 | 2 | 2 | 2 | 2 | 2 | uilr101–110, 116 PASS in `40797031704` |
| J10 Object row commands (rename, hide, lock, duplicate, mirror) | 1 | 2 | 2 | 2 | 2 | 2 | 2 | Overflow hides 5 acts (deliberate, Stage 018A); `ObjectCommandsTest` and `MirrorSmokeTest` PASS in `40805804385` (E1, emulator) |
| J11 TalkBack user picks a tool / mode | 1 | 1 | 0 | 1 | — | — | 1 | F-07: active state not announced; verdicts not announced |
| J12 Mouse-only user on a desktop window | 0 | 1 | 1 | 1 | 0 | — | — | F-10: no wheel zoom, no pan; FUTURE-NOT-DUE |

### High-friction journeys

The evidence supports **six**, not eight: J2, J3, J4, J5, J6 and J11.

- J2–J6 have E1 or E2 evidence of a concrete wrong outcome.
- J11 is source-only (E2) and has not been tried with TalkBack.
- J12 is outside current scope.

No further journey is padded in to reach eight.

## 5. Quick wins vs structural redesign

### Quick wins

Each is local, a few lines, and needs no change to any format or native contract.

1. **`EditorControlStyles.chip`.** Add `setMinimumWidth(control_height)`. This fixes F-01 for
   Flip, Selection Outline On/Off, Grid On/Off and the dimension value. Check the weighted rows
   (navigator) still fit the 184 dp navigator cap.
2. **Comma separator.** Use `"0123456789.,-"` in `BodyDimensionLabelsView.java:127`,
   `CadExtrudeCanvasView.java:644` and `SketchDimensionLabelView.java:112` (F-05).
3. **Back chain.** Move the bootstrap branch below the row-strip and anchored-surface branches,
   and add `SketchModifyView.paletteOpen()` and `CadExtrudeCanvasView` palette state to both
   `hasDismissibleSurface` and `dismissTopmostSurface` (F-03). Derive `hasDismissibleSurface`
   from the same ordered list, so the revolve state is included (F-04).
4. **`beginSketchView`.** Frame `viewFrame()` (F-02, shared with Seat C C-01).
5. **Status line.** Make `status_message` an `ACCESSIBILITY_LIVE_REGION_POLITE` view, and call
   `setSelected(active)` in `setChipActive` and on the rail (F-07).
6. **Sculpt.** Bound the status pill's width between the brush column and the rail, or start the
   brush column below the pill (F-06).

### Structural

These need owner direction or a larger design.

- One declarative **Back/dismiss registry**, ordered, from which both the API 33+ arming and the
  pre-33 decision are derived. Today they are two hand-kept predicates.
- A **touch-floor sweep test** that walks every clickable, shown view in each visual-evidence
  state, instead of per-test id lists (F-09).
- **Desktop/mouse input**: wheel zoom, button-drag pan, hover cursor. This needs an OWNER decision
  on Android desktop windowing (F-10).
- A **tablet/foldable evidence run** for the expanded layout on a real large-window AVD.

## 6. Form-factor and input support

| Target | Status | Evidence |
| --- | --- | --- |
| Phone portrait (1080x2400, 420 dpi) | Supported, verified on emulator | E1 (`40797031704`, `40800569636`, `40805804385`) |
| Phone short landscape | Supported, verified on emulator | E1 (`uilr102`, `uilr110`, ui-pref capture 16) |
| Tablet / expanded | **UNKNOWN** at runtime on this tree: policy JVM-tested only | `WorkspaceLayoutModeTest` (E1 JVM); historical density-override captures (E3) |
| Foldable / posture changes | **UNKNOWN**: no test, no evidence | — |
| Split-screen / multi-window | **UNKNOWN**. `configChanges` includes `screenSize\|smallestScreenSize\|screenLayout`; nothing tested | source only |
| Mouse | **UNKNOWN / not supported**: tool type mapped; no wheel, no secondary button | E2 |
| Hardware keyboard | Debug-only key hooks (NDEBUG-guarded); no shortcuts, by decision | E2 |
| Stylus | Hover highlight in the chooser is tested with injected events on the emulator. Pressure is captured (`ForgeShapeSurfaceView.java:84`) and nothing claims it is used | **OWNER-PHYSICAL-UNVERIFIED** |
| TalkBack | Names present; selected state and live verdicts missing | E2, never run with TalkBack |
| Font scale 1.3 | Trailing host and History acts verified | E1 / E3 |
| Font scale 1.5 | History acts only | E3 (d17) |
| Font scale 2.0 (Android 14+ nonlinear) | **UNKNOWN** | — |

## Fresh CI DEVICE evidence (appended by coordinator)

All emulator only (`ForgeShape_CI_API36`, API 36 x86_64, `medium_phone`,
SwiftShader Vulkan, font scale 1.0, portrait unless a test rotates); none of it
is a physical or OWNER visual PASS.

| Run | Result | Screens kept (`evidence/device-run-<id>/screens/`) | UX-relevant classes |
| --- | --- | --- | --- |
| `40797031704` | 32/32 | `ui-3d-state-audit-r1` (58), `ui-pref-r1` (20), `cad-a3-c2-sketch-ux-r1` (12), `sel-out-r1` (12) + startup / post-test | `Ui3dStateAuditTest`, `UiPrefVisualEvidenceTest` (five palettes, handedness, gizmo size/weight), `SketchUxVisualEvidenceTest`, `SelectionOutlineVisualEvidenceTest`, `EditorWorkspaceLegibilityTest` (rects, 48 dp checks, `uilr*`) |
| `40800569636` | 77/77 | `cad-vertical-slice-r1-after` (23), `modeling-freeform` (6), `modeling-surface` (9) | Surface, Freeform, Sculpt (brushes, history navigator, Isolate), import-sculpt, CAD vertical slice, extent, `SketchUxTest` |
| `40805804385` | 90/90 | startup / post-test only | persistence, transfer, process death, objects, Mirror, Dimensions, lifecycle, settings |

Not covered by these runs (so `UI_SCREEN_STATE_MATRIX.tsv` keeps them E3 or
UNKNOWN): font scale 1.3 / 1.5 and landscape (last run in C1/C2 by
`CadRetainedSketchOwnerTest`, E3), tablet / foldable sizes, TalkBack, hardware
keyboard, mouse and stylus hover. See `TESTS.md` §3.
