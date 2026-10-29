# FUNCTION-COUNCIL-R2 — Android UI/UX audit (Seat F)

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` = `f3db953`). Research only.

Companion files:

- `SEAT_F.md`: findings, with evidence detail.
- `UI_SCREEN_STATE_MATRIX.tsv`: 36 states.
- `SCREEN_EVIDENCE_INDEX.md`: every screenshot set.
- `work/req_F.tsv`: 87 atomic requirements, `UI-001`..`UI-087`.

Evidence grades and verdicts follow the council vocabulary. Every runtime number here comes from
the emulator: CI DEVICE `40797031704` on `70a6c14`, Android 16 / API 36, 1080x2400 at 420 dpi,
SwiftShader. **No statement here is a physical-device result.**

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
| Home / New Project | PASS (E1) | Full-window pages; targets 90–108 dp tall. The Freeform and Surface rows appear in no facts file |
| Bootstrap sketch | PASS layout (E1); **F-03** Back ordering (E2) | The status line carries an instruction (F-12, DOC-DIVERGENCE) |
| Orientation navigator | **F-01** (Flip 36 dp wide, E1); **F-02** (Flip and ±90° move no camera, E1+E2) | Plane chips about 45 dp wide (computed only, H-F1) |
| Line dimension | **F-01** (value chip 43 dp wide, E1); **F-05** (comma filtered, E2) | The editor sits flush with the window's right edge (x=1080 px) |
| Extrude HUD / dock / palette | PASS (E1 ledger + JVM) | Palette not in the Back chain (F-03) |
| Revolve ring | PASS function (E3); **F-04** (Back) | No fresh capture |
| History surface | PASS (E3, d17 at 1.3 and 1.5) | No screenshot |
| Property Inspector | PASS (E1, uilr107–110, 116) | — |
| Display popover | **F-01** (Selection Outline On 37 dp, Off 39 dp wide, E1) | Grid On/Off are built the same way; not measured |
| Objects / row strip / Mirror | PASS (E3) | — |
| Add Primitive | PASS (E1 Back via uilr106; E3 layout) | — |
| Dimensions / Relative Scale | PASS (E1 ledger); comma filtered in the dimension fields (F-05) | Body labels CLAMP while sketch and CAD labels HIDE (INFO) |
| Sculpt | **F-06**: status pill overlapped by the brush column and the rail (E1) | History navigator PASS (E3) |
| Settings | PASS (E1) | Back control only after scrolling (F-11, recommendation) |
| Palettes / contrast | PASS (E1+E3) | Light Charcoal focus ring 2.48:1 (F-08, recommendation) |
| Freeform / Surface surfaces | PASS function (E3); visual UNKNOWN | No screenshot set |
| Questions (unsaved, recovery) | PASS (E3) | — |

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
| J1 Cold launch → New CAD → sketch → Extrude → project | 2 | 1 | 2 | 1 | 2 | 2 | 2 | HomeFlowTest 01–03; captures 01–03 and ui3d03_*; Back from Ready goes Home, not to the sketch (by design, PRODUCT.md:196-199) |
| J2 Sketch orientation (navigator Flip, ±90°) | 2 | 0 | 1 | 2 | 1 | 2 | 1 | F-02 (no camera change; label changes); F-01 (Flip 36 dp) |
| J3 Type an exact length / depth on a comma-decimal keyboard | 1 | 0 | 1 | 1 | 2 | 2 | 0 | F-05: three fields drop ',' while four accept it |
| J4 Open a panel in the bootstrap sketch, then press Back | 1 | 0 | 1 | 0 | 2 | 2 | 0 | F-03: the whole drawn sketch is cancelled; nothing asks |
| J5 In-project Revolve, then System Back | 1 | 0 | 1 | 1 | 2 | 2 | 0 | F-04: app backgrounds on API 33 and later; returns to Extrude below 33 |
| J6 Enter Sculpt / Resume and read the status | 2 | 2 | 1 | 2 | 2 | 1 | 1 | F-06: text under the brush column and the rail |
| J7 Toggle Selection Outline / Grid | 2 | 2 | 2 | 2 | 1 | 2 | 2 | F-01: 37–39 dp chips |
| J8 Change palette / handedness in Settings | 2 | 2 | 2 | 2 | 2 | 2 | 1 | Fresh UiPref captures; Back control below the fold (F-11) |
| J9 Transform + exact values + IME | 2 | 2 | 2 | 2 | 2 | 2 | 2 | uilr101–110, 116 fresh PASS |
| J10 Object row commands (rename, hide, lock, duplicate, mirror) | 1 | 2 | 2 | 2 | 2 | 2 | 2 | Overflow hides 5 acts (deliberate, Stage 018A); E3 tests |
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
| Phone portrait (1080x2400, 420 dpi) | Supported, verified on emulator | E1 (fresh run) |
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

_Reserved for the coordinator._
