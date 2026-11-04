# Chair plan

ForgeShape's domain is already the kind SolidCraft and Shapr3D users would respect: refusals are named, edits are staged, the preview is the candidate, faces are semantic, and holes, patterns, fillets, Through All and To Face are all there. On screen it sits a tier below both. The viewport has no anti-aliasing and no edge lines, so parts look like debug renders. State cues measure 1.2 to 2.8:1, the type ramp is inverted (readouts outrank titles), and the desktop draws two or three copies of Undo, Home and the display toggles. Functionally the tablet is ahead of the PC, which cannot edit a part after Extrude, has no Fillet or Hole, and puts 116 of its 123 refusals into words only as enum names; the tablet in turn lacks command-first entries, a context menu, palm rejection and safe keyboard letters. Of the P0s, 130bebc fixed the navigator overlap for right-handed windows only, with no test, and left-handed seating is still unmeasured. The two P0s on the PC are open: the History chips do nothing, and a part cannot be edited after Extrude. The five iterations below should make it look comparable to the benchmarks after iteration 2 and reach functional parity across both shells after iteration 4, with no `.forge` format, section or fixture change anywhere; pen, palm, hover and real-GPU MSAA cost stay physical-device gates, UNVERIFIED until the owner tries them.

## Conflicts resolved
- Selected-state colour: VC-05, VC-07, the tablet seat, accessibility and the product lead proposed accentInk #A3C8F0 labels, pills and 22% tints. The visual seat vetoed this, citing attrs.xml ('fsAccent is spent on exactly two things: a primary commit, and the ring on a focused numeric field'). VETO HONOURED: selected = the approved neutral fill + textPrimary ink + a shape bar (~7:1). The accent stays for the single primary and the focus ring, and no accentInk token is added.
- VC-07's move of p1_selected_rest #52504A to #52606F was vetoed by accessibility and the product lead, citing colors.xml's twelve OWNER-APPROVED values. VETO HONOURED: no approved value moves. The bar carries the state. On the same principle, the approved success green keeps its value: status text becomes textPrimary with a tinted verdict glyph.
- Check glyph versus indicator bar for single-select segments: the check is dropped. It widens the 3-up mm/cm/m chips past their width at font scale 1.5. The bar plus the accessibility checked state already carries shape.
- Wide strokes: the tablet seat and the product lead wanted the optional wideLines feature with dynamic line width. The renderer's own contract (forgeshape_renderer.cpp:2595/2730) forbids it, and SwiftShader lacks it, so CI could not see it. VISUAL VETO HONOURED: screen-space quad expansion, applied ONLY to the sketch Entities, Construction and extrude-shaft ranges, which also meets the product lead's 'not every overlay line'. The grid, Dimension range and gizmo stay 1 px line lists.
- Dimming bodies while sketching: VC-16 asked for 40% opacity, and four seats vetoed it (no blended path; it washes out the face a face sketch stands on). The visual seat's opaque ghost through the tint slot is also not adopted: it dims the same support face and would rewrite SEL-OUT-R1's 'only the pulse tints' sentence. Hierarchy instead comes from a dimmer sketch grid, wide strokes and suppressing the outline in Editing only.
- Grid file: VC-16 and the product lead named forgeshape_grid.cpp, which is the FLOOR grid. The sketch grid is sketchOverlayStyleWeights (GridMinor 0.22, GridMajor 0.45), so the change is made there and the floor grid is untouched.
- Drawing the extrude arrowhead as Android or desktop chrome (VC-29) was vetoed by the mechanical engineer and the visual seat, citing 'what is seen and what can be grabbed cannot differ'. VETO HONOURED: a native filled cone in the wide-stroke pipeline, on the same control-scale band and arrow point the hit test uses.
- Feature edges: deriving them from face tags (VC-03, mechanical engineer) versus the one 40° crease policy (visual seat, product lead). Chose the crease policy. It is the threshold shading already splits on and is representation-neutral with no branch. It also draws no facet lines on 32-segment cylinders and no seam on coplanar Add caps.
- Field text precision: %.10g (VC-26) or %.15g (visual seat) versus 12 significant digits (mechanical engineer). Chose 12, Android's shipped DRAWN_SIGNIFICANT_DIGITS / LengthUnit.withoutBinaryNoise rule, so both shells format the same way. VC-26's and the product lead's uniform 3-decimal Measure was vetoed by the mechanical engineer (a 20 mm cube prints 0.000 m³). HONOURED: Measure uses 6 significant digits in unit²/unit³. The HUD value keeps CLAUDE.md's 3-decimal display precision.
- Demoting typed field values from 16 to 14 sp (VC-09, product lead) was vetoed by accessibility (M3 Body Large; the typed value is what the user verifies). HONOURED: typed values stay 16 sp, and readouts and body lines become 14 sp.
- Caption floor: IX-21 asked for 10 px captions under the extent icons and IX-32 for 10 sp rail captions. Vetoed by accessibility and the visual seat. HONOURED: 11 sp on Android and 12 px on Windows. Cells widen or wrap to two rows; text never shrinks.
- Desktop Undo/Redo: VC-10 and the product lead (owner's voice; WORKBENCH-RIBBON-R1's strip arrows) remove the title-bar pair. The mechanical engineer's veto cites Windows habit, not a repo rule, and is OVERRULED: the strip pair is always visible at a fixed place, Ctrl+Z/Ctrl+Y are unchanged, and removing the pair also ends the VC-33 shift. Every seat agrees on removing the Sketch ribbon's Edit group.
- Home: IX-14's 'Home lives on the cube only' was vetoed by accessibility (the house cannot take keyboard or TalkBack focus). HONOURED: cube_home is dropped and the view bar's Home stays. The View ribbon's Views and Projection groups are also dropped, because the view bar owns them in both shells.
- Where the desktop commit lives: VC-11 and IX-14 put it in Properties; IX-16 puts it in the ribbon as primary. The ribbon's Finish / Extrude is the ONE primary, matching the tablet's toolbar transition, and it survives a collapsed Properties panel. Properties keeps only a secondary Extrude that submits a typed depth first, the CLAUDE.md precision-surface clause. Its 'Finish sketch' goes.
- The confirmation-corner ✓/✕ (IX-13, mechanical engineer) was vetoed by the tablet seat and accessibility: that corner held the P0 overlap, and it would be a third copy of the commit and a second ambiguous ✕. HONOURED: a captioned Cancel sits in the transition capsule, with Enter/Esc.
- Cancel placement: the product lead put it 'right after Finish'; accessibility put it 'leading, separated by the sketch label'. Chose accessibility's: cancelling a non-empty sketch cannot be undone, so Cancel never shares an edge with the commit.
- Context-menu triggers: IX-04's two-finger tap was vetoed by the mechanical engineer, the tablet seat and the product lead (it collides with pinch and fires on palm contact). HONOURED: a still 500 ms long-press in Construction only (accessibility's veto on sketches), the S Pen button, or a still right-click. A right-DRAG still orbits (mechanical engineer's veto).
- Context-menu contents: the product lead included Hole, Pattern and Round. The tablet seat cites CAD-UI-DOCK-R1 (face acts are docked). Chose the split: the menu carries Edit feature, Edit sketch, Sketch here, Fit to body and the body acts; finishes, holes and patterns stay in the dock on Android. Flagged for owner confirmation.
- Hover pre-highlight colour: IX-05 and the tablet seat used 'accent 40%'. That breaks the accent rule and the orange-means-selected rule (visual seat's veto). Chose cool neutral #DCE6F2: no blink, a fade of at most 100 ms, and it never commits.
- Debug-key gate: the mechanical engineer proposed Ctrl+Alt+Shift, the product lead the selftests property, and accessibility a dedicated property. Chose the dedicated debug.forgeshape.debugkeys: CI DEVICE sets the selftest property and must be able to exercise iteration 3's plain-letter shortcuts.
- Spline key: the mechanical engineer proposed I; the tablet seat, accessibility and the product lead proposed S. Chose S in the Sketch scope. Shift+S New Sketch lives in the Model scope, so the two do not collide.
- Status-chip slot: top-centre (VC-12, tablet seat, accessibility, product lead) versus bottom-leading with a message log (IX-18). Chose top-centre. The log is deferred.
- Desktop Model ribbon Modify group: the mechanical engineer had it withdrawn while nothing is offered, and also offered a tool-first prompt; those contradict each other. Resolved on the tablet seat's command-first rule: drawn when native says the body has an eligible face; with nothing offered it enters a pick mode with a step tip.
- Auto-offsetting new primitives (IX-25) was vetoed by the mechanical engineer and the product lead. HONOURED: placement stays at the origin. The 'Overlaps Body #2 — Add would merge it' line is dropped too: a world-plane sketch offers New Body alone (CLAUDE.md, 'nothing guesses a target').
- Live preview while typing in tablet editors (IX-20) was vetoed by the mechanical engineer and the product lead, and CLAUDE.md lists it as not this stage. HONOURED: a dirty-only Apply pinned beside the fields. The staged preview on field commit is deferred.
- Tablet ribbon (product lead #1; PROJECT_STATUS lists WORKBENCH-RIBBON-R2 as deferred) versus CLAUDE.md's vertical-group rule for the trailing host. Adopted for the EXPANDED workbench only, as the same host with the same ids laid horizontally. The CLAUDE.md sentence change needs OWNER sign-off before merge. Phone and medium layouts are untouched.
- Debug shading: VC-35 proposed hiding it behind BuildConfig.DEBUG, but the owner runs the debug APK, so that would hide nothing. Gated by the selftests property instead.
- View cube: IX-15's 8 corner targets of 32 dp or more were vetoed by accessibility (each cell is 25–30 dp under a 48 dp fingertip). HONOURED: deferred for now. Opposite-end tap and drag-to-orbit go in instead. VC-31's '−X on hover only' was also vetoed by accessibility: the labels show at rest.
- Icon-only chrome (IX-32's 'rely on tooltips', VC-25's icon-only Export option) was vetoed by the tablet seat and accessibility. HONOURED: everything keeps captions plus tooltips.
- Excluded outright, following the vetoes: a rollback marker or slider (the owner declined it), tap-empty-space-to-commit, automatic Add/Cut inference beyond push-through, FXAA/TAA/8×/sRGB, PBR/SSAO, Mica panels, a bundled typeface or third-party icons, dynamic colour, pixel-identical shells, a radial marking menu on touch, and any third-party UI library.

## Iteration 1: Fix what is broken, and lay the tokens
Nothing stands over or jumps under a live control. No control is drawn that cannot succeed, and no field writes a value the user did not type. Both shells share one measured set of colour roles, selection grammar, type ramp and spacing/radius tokens for the later iterations to build on.

### The workbench stands still under the hand [tablet (EXPANDED workbench only)]
Files: app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/java/com/forgeshape/app/GlobalToolbarView.java, app/src/main/java/com/forgeshape/app/DisplaySettingsPopoverView.java, app/src/androidTest/java/com/forgeshape/app/WorkbenchLayoutStabilityTest.java (new)

(a) Keep 130bebc's placeSketchCornerColumn, which today falls back to the fixed trailing_host_width margin whenever leftHanded(), and measure the docked inspector on whichever side it sits. Margin = distance to its inner edge + row_gap (8 dp). If the free model width is under 184 + 16 dp, seat the navigator and Modify under the view cube.
(b) On a workbench, statusMessage leaves the toolbar column for overlayRoot:
- top-centre of the model area, 8 dp under the Global Toolbar row, max width 480 dp, never intersecting a docked panel or the view cube;
- text in fsTextPrimary, with a 16 dp leading verdict glyph (ic_check / ic_error / ic_info) tinted in the unchanged approved verdict colour;
- setAccessibilityLiveRegion(POLITE);
- hold max(5 s, 1.5 s + 70 ms × characters), capped at 15 s; a fault stays until the next user act.
The phone keeps its current placement.
(c) On a workbench, opening the Display popover no longer hides the trailing host or moves the docked panel. The popover stands over them under the transient-menu exception.

Acceptance: WorkbenchLayoutStabilityTest at 1706×1066 dp, font scales 1.0/1.3/1.5/2.0, Right and Left handedness, in Construction, Editing and Ready:
- Rect.intersects is false for navigator vs property_inspector, sketch_modify vs property_inspector, navigator vs the trailing host, and navigator vs the Browser.
- The Apply and unit-chip centres hit-test to themselves.
- The Objects and Properties tops are identical before and after a status message.
- The precision surface's left x and the view bar's centre are identical before and after Display opens.
- The status chip has a polite live region.
Recaptured 03/04/06 show nothing over the panel and no shift.
Sources: VC-01, IX-01, VC-12, IX-18, IX-24; tablet seat top 3 #3; accessibility #1 and status proposal; product lead veto ('add the Rect.intersects check and stop')

### A field never writes a value you did not type, and Apply exists only when there is something to apply [all]
Files: desktop/src/ui/ui.cpp, desktop/src/ui/ui.h, desktop/src/desktop_editor.cpp, app/src/main/java/com/forgeshape/app/PropertyInspectorView.java, app/src/main/java/com/forgeshape/app/SketchEditorView.java, app/src/main/java/com/forgeshape/app/CadFeatureEditorView.java, app/src/main/java/com/forgeshape/app/ConstructionShapeEditorView.java, app/src/main/java/com/forgeshape/app/NumericPropertyRow.java

Desktop Ui::numberField (ui.cpp:514, editCommit 435-449):
- Replace "%.*f" with the shortest round-trip value rounded half-even to 12 significant digits, with no exponent and trailing zeros stripped. This is Android's DRAWN_SIGNIFICANT_DIGITS = 12 / LengthUnit.withoutBinaryNoise rule.
- Print −0 as 0.
- Select all on focus.
- Return true ONLY when the committed text differs from the text shown at focus, so Tab, blur or Enter on an untouched field submits nothing.
- Call sites: desktop_editor.cpp:1489, 1514, 2047.
Android:
- SketchEditorView's commit control, and PropertyInspectorView's PinnedCommit footer, are GONE while no sketch entity is selected. Once one is selected, the commit reads 'Apply size'.
- CadFeatureEditorView and ConstructionShapeEditorView draw Apply only while some field's text differs from native's value formatted by the same 12-digit rule. The IME Done key still commits.
Measure on both shells: 6 significant digits, area in unit² and volume in unit³, tabular figures, −0 as 0.

Acceptance: Desktop headless check:
- Focus a Position field holding 0.0004 m and blur it: no transform write and an unchanged fingerprint; the field reads 0.0004, not 0.000.
- A height of 1.0000000000000002 reads 1.
Tab 03 recaptured shows no Apply with nothing selected. Tab 02 shows no Apply at rest; typing in Width shows it, and restoring the text hides it again. A 20 mm cube's volume reads 0.000008 m³, never 0.000.
Sources: Mechanical engineer #2 (new finding, ui.cpp:517 / 435-449); VC-13, IX-17, VC-26; mechanical engineer's veto on uniform decimals

### The owner's debug build stops answering CAD letters with stress meshes [android]
Files: app/src/main/java/com/forgeshape/app/ForgeShapeActivity.java, README.md

In ForgeShapeActivity.onKeyDown (401-437), the 1–9 / A–G fixture switch runs only when the system property debug.forgeshape.debugkeys == 1, read the same way debug.forgeshape.selftests is. Otherwise the key falls through to super.onKeyDown. Plain E (the ~500k-vertex stress mesh) and F (freeze to Sculpt) are therefore inert on the Galaxy keyboard cover. Grep scripts/ and androidTest for keyevent senders, and setprop wherever one is found. README documents the property beside the selftest one.

Acceptance: Instrumented, property unset: KEYCODE_E and KEYCODE_F leave the body count, the published revisions and the fingerprint unchanged, and log no stress-tier token. With the property set to 1 they still publish.
Sources: IX-11; accessibility proposal 4; product lead keyboard (1); tablet seat keyboard step 1

### One commit and one home per act on the desktop [desktop]
Files: desktop/src/desktop_editor.cpp, desktop/src/ui/ui.cpp, desktop/src/ui/ui.h, desktop headless check (ids list)

Remove, keeping one home for each act:
- tb_undo/tb_redo (desktop_editor.cpp:1010-1011). Undo/Redo stay at the History strip's leading end, and Ctrl+Z/Ctrl+Y are unchanged.
- The Sketch ribbon 'Edit' group (sk_undo/sk_redo, ~1859).
- The Properties DISPLAY section (1587-1606).
- cube_home (1314). The view bar keeps Home.
- The ribbon 'Panels' group (~1171). The title-bar toggles stay at a fixed x.
- The View ribbon's Views and Projection groups. The view bar is the one home for Fit, Home, the standard views and projection, and the View tab keeps Show (Grid, Selection outline).
No act is passed active=true. sk_finish (1862) and sk_extrude (1871) become a new RibbonRow::primary(): accent fill #4C8FD6, textOnPrimary #12110F (5.58:1), radius 4, the single primary per screen. sk_extrude is WITHDRAWN while !canExtrude. Both Finish controls use one predicate (entities > 0, the one native refuses on). Properties' 'Finish sketch' button is removed. Properties keeps a secondary Extrude only because it submits a typed depth first.

Acceptance: Recaptured desktop-02/03/07:
- exactly one Undo/Redo pair, the strip's;
- one Home, the view bar's;
- no DISPLAY section and no Panels group;
- in 03, Rectangle is the only selected-style control and Finish the only accent-filled one.
Headless check: sk_extrude is absent from widgetRects_ with no region chosen, Finish agrees between ribbon and Properties at 0 curves, and the updated id list passes.
Sources: VC-10, IX-14, VC-11, IX-16, VC-33; product lead 'cut list'; accessibility veto on cube-only Home; mechanical engineer's title-bar veto overruled (see conflicts)

### Colour roles that pass measurement on all five palettes [all]
Files: app/src/main/res/values/attrs.xml, app/src/main/res/values/colors.xml, app/src/main/res/values/themes.xml, app/src/main/res/drawable/bg_field.xml, desktop/src/ui/theme.h, desktop/src/ui/theme.cpp, desktop/src/ui/ui.cpp, desktop/src/desktop_editor.cpp, app/src/main/java/com/forgeshape/app/HistoryStripView.java, app/src/main/java/com/forgeshape/app/FeatureHistoryView.java, app/src/androidTest/java/com/forgeshape/app/EditorWorkspaceLegibilityTest.java, artifacts/ui-pref-r1/CONTRAST.md

New role fsFieldBorder, desktop token fieldBorder, one value per palette:
- Warm Graphite #9A9389
- Neutral Charcoal #8F969C
- Light Charcoal #B0B4B8
- Warm Light #776F63
- Cool Light #6B747E
Each is at least 3:1 against its palette's floating, precision, chrome and field-well grounds. It is used for resting field borders (bg_field.xml, ui.cpp:494) and switch off-tracks (ui.cpp:422); the 2 dp accent focus ring is unchanged.
Ink rules, stated in a theme.h / attrs.xml comment:
- textDisabled is for disabled controls only.
- Live hints, empty states and ENABLED ribbon captions use textSecondary (desktop_editor.cpp:1213-1216, 1628; ui.cpp:370).
- Desktop MEASURE values use textPrimary, with textSecondary keys (1571).
- Desktop instructions use textSecondary (1998, 2017, 2080).
- textMeasure is kept only for the on-canvas measure-distance line.
HistoryStripView and FeatureHistoryView drop setAlpha(0.6f) on not-rebuilt, pending and unused rows: alpha 1, a textSecondary label, and the existing row.mark() glyph carries the state.
No approved value changes.

Acceptance: EditorWorkspaceLegibilityTest (UILR1-11) extended:
- fsFieldBorder ≥ 3:1 against every ground it borders, in all five palettes;
- a walker asserts every visible enabled TextView has effective alpha 1 and ≥ 4.5:1 against its ground.
Desktop headless: a pixel sample of 'The active body has no feature history' is ≥ 4.5:1. Recaptured desktop-06 shows the MEASURE values off-white, not amber, and the fields have a visible border.
Sources: VC-30, VC-06, VC-17, VC-24; accessibility 'live content never in disabled ink' and 'control boundaries at 3:1'; product lead state cues; accessibility veto on Warm-Graphite-only tokens

### One selection grammar: neutral fill, textPrimary ink, a shape bar [all]
Files: app/src/main/res/drawable/bg_control_active.xml, app/src/main/res/drawable/bg_rail_entry.xml, app/src/main/java/com/forgeshape/app/EditorControlStyles.java, app/src/main/java/com/forgeshape/app/ToolRailView.java, desktop/src/ui/ui.cpp, desktop/src/ui/theme.h, desktop/src/ui/theme.cpp, app/src/test/java/com/forgeshape/app/ThemeParityTest.java (new)

Selected at rest = the approved selected-at-rest fill (p1 #52504A, unchanged) + a textPrimary label or glyph + an indicator bar in textPrimary.
Bar placement:
- horizontal items (segments, chips, tabs, ribbon buttons): centred inside the bottom edge, 4 dp inset, 3 dp × 20 dp (desktop 2 px × 16 px);
- vertical items (rail entries, list rows): on the leading edge, 3 dp × 20 dp.
The bar stays inside the 48 dp box and draws no frame. No check glyph.
Desktop:
- drop the accent.alpha(0.22) fills, accent.alpha(0.55) borders and accent ink (ui.cpp:341-348, 360-368, 380-402);
- switch (421): ON is a textPrimary thumb at the trailing end on the selectedRest track with a 1 px fieldBorder; OFF is a textSecondary thumb on the field well.
Accessibility: an AccessibilityDelegate in setChipActive, setListRowActive and ToolRailView.showActive reports selected, or checkable + checked for single-select segments. It does not call View.setSelected, which would repaint through the state lists.
ThemeParityTest fails when any role shared by the desktop's theme.h defaults and colors.xml p1_* differs.

Acceptance: Recaptured tab 03: 'Rectangle', 'm' and 'XY' each carry a bar. UILR1-11 asserts bar vs selected fill ≥ 3:1 (≈ 7.15:1 on Warm Graphite) in all five palettes. AccessibilityNodeInfo of the chosen unit chip reports checked. Recaptured desktop-07: no blue-tinted button other than one primary. ThemeParityTest is green.
Sources: VC-05, VC-07, VC-11, VC-23, IX-16; visual seat #3 and veto (attrs.xml accent rule) honoured; accessibility and product-lead vetoes on p1_selected_rest honoured

### One type ramp per shell, each size with a fixed job [all]
Files: app/src/main/res/values/dimens.xml, app/src/main/java/com/forgeshape/app/EditorControlStyles.java, app/src/main/java/com/forgeshape/app/CadFeatureEditorView.java, desktop/src/ui/theme.h, desktop/src/ui/ui.cpp, desktop/src/desktop_editor.cpp, desktop/src/main_win32.cpp

Android:
- text_title 15 → 16 sp/24 medium, for the panel title only.
- New text_heading 14 sp/20 medium in textPrimary for sectionLabel, with a 16 dp top gap.
- text_label 11 → 12 sp/16 for fieldLabel.
- Typed field values STAY text_value 16 sp.
- Readouts and body lines 14 sp/20.
- text_display 21 → 22 sp.
- rail_label_size 10 → 11 sp/16 medium.
- Measure (CadFeatureEditorView.java:158) becomes a two-column key/value grid: key 12 sp textSecondary, value 14 sp textPrimary, fontFeatureSettings "tnum".
- 'Sketched on the XZ plane.' and 'Rectangle profile' become 14 sp textSecondary.
Desktop:
- Tokens: caption 12/16, body 14/20, bodyStrong 14/20 Semibold, subtitle 20/28 Semibold, title 28/36 Semibold.
- fontSize 13.5 → 14.
- Ribbon captions, group names and cube letters → 12 px (ui.cpp:370, desktop_editor.cpp:949).
- Panel titles 14 Semibold.
- section() headings in sentence case at 14 Semibold, no uppercase.
- main_win32.cpp:303 loads seguisb.ttf before segoeuib.
- A selected listRow keeps its weight.
- The ribbon may grow from 96 to 100 px.

Acceptance: Recaptured tab 02: the panel title is the largest text in the panel, the 'Extrude' heading is distinct from the 'Depth' caption, and Measure is an aligned grid. The C1-P3 font-scale cases (1.0/1.3/1.5, portrait and landscape) are green with no ellipsized caption. Recaptured desktop-07 has no uppercase headings. The headless check finds no desktop text below 12 px.
Sources: VC-09, VC-20, VC-25, VC-23; visual seat ramp; product lead ramp; accessibility veto (typed values stay 16 sp) honoured

### Desktop panels docked square, on one radius and spacing scale [desktop]
Files: desktop/src/ui/theme.h, desktop/src/ui/theme.cpp, desktop/src/ui/ui.cpp, desktop/src/desktop_editor.cpp

Docked panels:
- New Ui::dockedPanel() beside Ui::panel() (ui.cpp:268): radius 0, no shadow, opaque chromeSurface #3A3835, and a 1 px chromeHairline on the viewport side.
- The Browser docks at x = 0 and Properties at x = w − 300, each running from the ribbon bottom to the strip top.
- The History strip runs full width above the status bar, with a top hairline.
- surfaceFloating behind anything docked becomes opaque.
Floating surfaces: Ui::panel keeps radius 8 with ONE shadow (blur 16, #66000000), used only by the view bar, menus, tooltips and the view-cube disc.
Tokens in theme.h:
- radiusControl 4, radiusOverlay 8 and radiusDocked 0; the panelRadius alias is kept for theme.json.
- space2/4/8/12/16/24/32.
- rowHeight 32.
- panelInset 16, as panel padding rather than a window gutter.
Strip chips take radius = host radius − gap. px() literals are replaced only in the panels this iteration touches.

Acceptance: Recaptured desktop-02/06/07: the panels meet the window edges with no gutter and no shadow. Browser pixels on and off a grid column are identical, so nothing shows through. The model area is ≥ 20 px wider at 1600×900. The headless id clicks still pass.
Sources: VC-21, VC-22, VC-32; visual seat proposal 7; product lead docked desktop; visual veto on translucent/Mica panels

Critique: Capture 03/04/06 at Right AND Left handedness and at font scale 1.3. Check:
- nothing stands on the docked panel;
- the Browser and Properties do not move when a status message appears or Display opens.
On the desktop, check:
- one accent-filled control per screen;
- one Undo/Redo pair;
- flush panels with no grid showing through.
At arm's length, judge whether a selected chip, rail entry and tab read by their bar alone; then check:
- the panel title outranks the Measure readouts;
- no heading is uppercase;
- no field shows binary noise.
Watch for:
- wrapping in the 220–260 dp Browser at 1.3/1.5;
- 12 px ribbon captions clipping;
- the bar crowding a 48 dp Objects row;
- a light palette where fsFieldBorder looks heavy.

## Iteration 2: The viewport reads as CAD
On both shells a part should look like a Shapr3D or SolidCraft part:
- smooth silhouettes;
- dark feature lines;
- a shadow side that separates from the ground;
- a selection outline that stays clean on coplanar faces;
- a sketch that out-reads its grid;
- a solid extrude arrow;
- a preview that shows source and result;
- a value at the geometry that can be read over any preview.

### 4× MSAA in the main pass, resolving into the swapchain [all]
Files: app/src/main/cpp/forgeshape_renderer.cpp, app/src/main/cpp/forgeshape_renderer.h, app/src/main/cpp/forgeshape_render_recovery_selftest.cpp

sampleCount_ is the highest of 4, 2 or 1 present in framebufferColorSampleCounts & framebufferDepthSampleCounts; log FORGESHAPE_RENDER_MSAA:<n>.
createRenderPass (2322-2371) gets three attachments:
- MSAA colour (loadOp CLEAR, storeOp DONT_CARE);
- MSAA depth;
- the swapchain image as pResolveAttachments.
Memory: on Android the MSAA images use TRANSIENT_ATTACHMENT + LAZILY_ALLOCATED where offered, otherwise plain device-local.
Pipelines: every main-pass pipeline takes sampleCount_ (2489, 2606, 2741, 3200, and the desktop createUiPipeline). The outline MASK pass (3112) stays at 1×, and the composite rule is unchanged.
Lifetime: the MSAA images are swapchain-scoped, rebuilt only on an extent change or a device rebuild, and the allocation-count check covers them.
There is no user setting, no MeshRevision and no format change.

Acceptance: - FORGESHAPE_RENDER_MSAA:4 is logged on lavapipe and the emulator.
- A desktop-02 crop of the box silhouette shows intermediate coverage pixels and no 1 px stairs.
- The render-recovery self-test, RendererCounterContinuityTest and SelectionOutlineTest are green.
- The swapchain allocation count moves only on a resize.
- CI DEVICE frame time is recorded; a real Adreno cost is UNVERIFIED.
Sources: VC-02; visual seat #1; product lead #2; visual veto on FXAA/TAA/8×/sRGB honoured

### Feature edges from the one crease policy, and a shadow side that separates from the ground [all]
Files: app/src/main/cpp/forgeshape_feature_edges.h (new), app/src/main/cpp/forgeshape_feature_edges.cpp (new), app/src/main/cpp/forgeshape_render_mesh.h, app/src/main/cpp/forgeshape_render_mesh.cpp, app/src/main/cpp/forgeshape_renderer.cpp, app/src/main/cpp/shaders/edge.vert (new), app/src/main/cpp/shaders/edge.frag (new), app/src/main/cpp/shaders/surface.frag, app/src/main/cpp/forgeshape_display.h, app/src/main/cpp/forgeshape_display.cpp, app/src/main/java/com/forgeshape/app/DisplaySettingsPopoverView.java, desktop/src/desktop_editor.cpp

Derivation: a pure function over a published RuntimeMesh's SOURCE indices, welded by position. It emits:
- every edge whose two triangles' normals differ by more than kCreaseAngleDegrees (40°, the threshold shading already splits on);
- every open boundary edge.
The result is index pairs into the body's OWN render vertex buffer, held as a second index range. It is rebuilt only when MeshRevision changes and never serialized.
Drawing: one line-list pipeline in the main pass, recorded after surfaces and before the grid.
- position-only, with an mvp push constant;
- depth test LESS_OR_EQUAL, no depth write;
- an eye-ward kEdgeDepthNudge, stated beside kGridDepthNudge with the opposite sign.
Colour per ground family, through viewportBackgroundIsLight (OWNER-TUNABLE):
- dark grounds: #161513 at α 0.85;
- light grounds: #3A3835 at α 0.80.
No edges on a Frozen Sculpt Mesh. Bodies above 400k source triangles are skipped with a logged token.
Toggle: 'Edges' On/Off beside Grid and Selection outline, in the Display popover and in the desktop View Show group. It is session-only in the native display store, default On, and not an AppPreferences field.
Shading (surface.frag): kStudioGroundAmbient 0.26/0.25/0.24 → 0.34/0.33/0.32, and kStudioFillIntensity 0.18 → 0.30. The darkest vertical face moves from #4E4F52 (1.65:1) to about #66676A (≥ 2.3:1 on the dark grounds). Rewrite the measured comment.

Acceptance: Host self-test (in an existing suite, so the count stays at 25):
- a box gives exactly 12 edges;
- a 32-segment cylinder gives 2 rim loops and no facet lines;
- a coplanar Add cap gives no seam;
- a Plane gives its 4 boundary edges.
Recaptured desktop-06: the Cut's rim and both bore ends are drawn lines.
Recaptured tab 02 with the outline off: the unselected box's silhouette is visible.
The shadow face against the ground samples ≥ 2:1, and with Edges off no edge pixels are drawn.
All 25 *_SELFTEST_OK tokens still appear.
Sources: VC-03; visual seat #2; product lead #2; mechanical engineer 'shaded with edges'; tag-vs-crease conflict resolved

### A deterministic selection outline on coplanar faces [all]
Files: app/src/main/cpp/forgeshape_renderer.cpp, app/src/main/cpp/forgeshape_selection_outline.h, app/src/main/cpp/forgeshape_selection_outline.cpp

recordOutlineMaskPass (~3680) records the selected body LAST, and createOutlineMaskPipeline (3123) changes depthCompareOp from VK_COMPARE_OP_LESS to LESS_OR_EQUAL. An equal-depth tie therefore always resolves to the selected body, while a body strictly in front still wins. Add a coplanar case, two bodies sharing a top plane, to the selectionOutlineCoverage CPU reference and the render self-test.

Acceptance: - New host check: zero band pixels inside a shared coplanar face.
- A recaptured desktop-02 crop (690–910, 360–540) shows no speckle.
- SelectionOutlineTest and SelectionOutlineVisualEvidenceTest are green.
Sources: VC-04; visual seat proposal 5; product lead (5)

### The sketch out-reads its grid, with wide strokes and no outline while drawing [all]
Files: app/src/main/cpp/forgeshape_sketch_overlay.cpp, app/src/main/cpp/forgeshape_renderer.cpp, app/src/main/cpp/shaders/overlay_wide.vert (new), app/src/main/cpp/shaders/overlay_wide.frag (new)

Sketch grid (sketchOverlayStyleWeights; the floor grid in forgeshape_grid.cpp is untouched): GridMinor 0.22 → 0.12, GridMajor 0.45 → 0.24, Axes 0.9 → 0.6, each × kGizmoAxisAlpha.
Wide strokes without wideLines:
- On upload, expand the Entities, Construction and extrude-preview/arrow-shaft overlay ranges into screen-space quads: 6 vertices per segment, carrying the other endpoint and a side sign.
- Offset in clip space by the viewport, with SDF edge coverage as in ui.frag's segment mode.
- Widths: regular 1.5 dp (min 2 px), emphasised 2.5 dp (min 3 px), Construction 1.5 dp with its dash kept.
What stays as it is:
- The domain SketchOverlay stays a line list, so no vertex-count pin moves.
- The grid, the Dimension range and the gizmo stay 1 px line lists, and wideLines is never requested.
- Hit tests read reference units, never the drawn width.
While a sketch is in Editing, the renderer records neither outline pass. The toggle and the selection are untouched. No body is dimmed.

Acceptance: - Recaptured desktop-03 and tab 04: the rectangle stroke is ≥ 2 px and is the brightest mark on screen.
- Pixel samples: a major sketch-grid line is ≤ 2:1 against the ground, and the entity is ≥ 4.5:1 over a major line.
- No orange outline while in Editing.
- The gizmo at Regular weight is still 1116 vertices.
- The sketch hit-test host checks are unchanged.
- The sketch-overlay self-test is updated to the new alphas.
Sources: VC-16, IX-26; accessibility line weights; visual seat proposal 4 and wideLines veto; product lead (1)(2)

### A solid extrude arrow, drawn and grabbed by native [all]
Files: app/src/main/cpp/forgeshape_cad_extrude_tool.cpp, app/src/main/cpp/forgeshape_renderer.cpp

The shaft goes through the wide-stroke path at 2 px. The head becomes a filled cone, emitted as triangles in the same triangle-list pipeline:
- shaded lit #F4F1EB or shade #B6B1A9 according to facing;
- a 1 px #1E1D1B rim;
- the gizmo's held-handle highlight while grabbed.
The head keeps the control-scale band and cadExtrudeArrowPoint, so the drawn head is the grabbed head, and the 24-unit corridor is unchanged.

Acceptance: - Tab 05 and desktop-04 crops show a filled, shaded head.
- A host check finds cadExtrudeArrowPoint, onDrawnArrowHead and the corridor results bit-identical to before.
- CadCanvasExtrudeTest and CadHud3dOwnerTest are green.
Sources: VC-29; IX-09; mechanical engineer and visual seat vetoes on a chrome arrowhead honoured

### The preview shows source and result, and orange means selected [all]
Files: app/src/main/cpp/forgeshape_cad_extrude_tool.cpp, app/src/main/cpp/forgeshape_sketch_overlay.cpp

cadOperationPreviewTint (127-134):
- Cut body alpha 0.34 → 0.10;
- Intersect → 0.12;
- Add stays at 0.30.
The tool's hatch and wire carry the Cut hue, #E8A093 at 2 px, through the wide path. Saturated red is reserved for refusals: the badge's error ring plus the ✕ shape. Orange and amber in the viewport are reserved for selection, an emphasised entity and a held handle.

Acceptance: - Recaptured desktop-05: the target body keeps its shading, with the top face within 10% of its unselected tone, and the removed volume is hatched salmon.
- The CAD suites are green with the updated tint values.
Sources: VC-18, VC-17, IX-26; visual seat proposal 10

### A value you can read at the geometry, on both shells [all]
Files: app/src/main/java/com/forgeshape/app/CadExtrudeCanvasView.java, app/src/main/java/com/forgeshape/app/SketchDimensionLabelView.java, app/src/main/java/com/forgeshape/app/BodyDimensionLabelsView.java, app/src/main/java/com/forgeshape/app/CadRevolveAngleLabelView.java, app/src/test/java/com/forgeshape/app/CadHudPresentationTest.java, desktop/src/desktop_editor.cpp, CLAUDE.md (value floor, OWNER-TUNABLE)

Android: replace setShadowLayer (CadExtrudeCanvasView.java:1058-1068) with an opaque plate:
- fsSurfaceFloating at 92% (#EB46433F on Warm Graphite)
- radius 6 dp, padding 6 × 2 dp
- 1 dp fsChromeHairline
The value floor goes from 9 to 12 sp in clamp(14 sp × scale, 12, 18). That is OWNER-TUNABLE, and the CLAUDE.md sentence changes with sign-off. The same plate goes on the sketch, body-dimension and revolve-angle labels, and the 48 dp targets are unchanged.
Desktop (line 763, 'no shell overlays'):
- draw the same plate and value at tool-state slots 32..41;
- draw the operation badge at slots 45..58 with Properties' glyphs;
- hide both exactly where native hides them;
- clicking the plate opens an in-place numberField: Enter goes through setExtrude, Esc restores;
- show a resize cursor over the arrow corridor.

Acceptance: - Tab 05 crop: the '1 m' plate is ≥ 7:1 over the New Body preview.
- CadHudPresentationTest pins the 12 sp floor.
- Recaptured desktop-04 shows the plate and badge at the arrow.
- Desktop headless: click the plate, type 0.25, press Enter. The depth becomes 0.25 and one Undo restores it.
Sources: VC-14, IX-09; visual seat proposal 9; accessibility plate; product lead (1)(2); mechanical engineer type-to-override (a)

Critique: Compare before and after at desktop 1× and tablet 240 dpi:
- the box and cylinder silhouettes and the outline band should be smooth;
- the Cut rim and bore ends should read as lines;
- the shadow face should separate from the ground with the outline off;
- desktop-02's coplanar tops should be clean.
In a sketch, the drawn rectangle should be the strongest mark and the grid a whisper. The arrow should look like an instrument, and the value should be readable over the pale New Body preview.
Watch for:
- edges z-fighting at grazing angles;
- crease noise on imported meshes;
- dashes breaking up at 2 px;
- the Cut hatch reading too faint;
- the plate hiding the leader;
- SwiftShader/lavapipe frame time after MSAA.

## Iteration 3: Edit what you made, through one core on both shells
The PC becomes a parametric CAD tool. On both shells the same acts are reached the same ways: any feature or sketch can be edited from History, Properties, a face or a context menu, and the desktop gains Fillet, Chamfer and Hole. Refusals read as sentences, and one shortcut table drives both shells.

### Edit acts move into the editor core; desktop History chips and a Features section open the staged edit [desktop (+ core; Android re-pointed, behaviour byte-identical)]
Files: app/src/main/cpp/forgeshape_editor_core.h, app/src/main/cpp/forgeshape_editor_core.cpp, app/src/main/cpp/forgeshape_jni.cpp, app/src/main/cpp/forgeshape_sketch_session.cpp, desktop/src/desktop_editor.cpp

Lift Java_..._sketchBeginEdit (8444), sketchBeginEditFeature (8512, with resolveCadFeatureWorldFrame from 6329) and the commit-edit path (8587) into the core as:
- editorSketchBeginEdit(ObjectId)
- editorSketchBeginEditFeature(ObjectId, featureId, startReady)
- editorSketchBeginEditSketch(ObjectId, CadSketchId)
- editorSketchCommitEdit()
editorSketchCommit (404) routes an edit session to SketchSession::commitEdit. Today commit refuses NotSketching when editingBodyId_ is set. JNI becomes thin wrappers with the same lock, refusals and FORGESHAPE_SKETCH_EDIT_* tokens.
Desktop History strip (~1223-1244):
- Sketch and Extrude/Revolve chips become ui.button("hist_<kind>_<id>"); a click or a double-click opens them.
- A sketch row opens beginEditSketch. A feature row opens beginEditFeature(startReady=true) on the existing Ready panel.
- The chip being edited takes the selected grammar.
- Hole, Pattern and Finish rows stay flat secondary text, with no surface or border, until the desktop has their editors.
Desktop buildProperties (1397) gains a 'Features' section for a CAD body: an 'Edit Sketch' button first, then one row per buildCadTimelineFor row with the same acts. The first failing row shows in textError with a ✕ glyph, and later rows show as not rebuilt.

Acceptance: - Host self-test through the core: begin a feature edit, change the depth and commit. The result is one Undo, the same ObjectId and still one body.
- Desktop headless: click hist_feature_2, set a distance and Extrude. The mesh changes, and Undo restores it.
- Recaptured desktop-06 shows the Features section.
- Android CadVerticalSliceTest, SketchExtrudeTest and CadParametricHistoryOwnerTest are green.
Sources: IX-02 (P0), IX-03 (P0); mechanical engineer #1; product lead #3; DESKTOP-R1 (glue moves into the editor core)

### The face and edge offer moves into the core: the desktop gets Sketch here, Fillet, Chamfer and Hole, plus a hover pre-highlight [all (desktop gains the acts; Android re-pointed)]
Files: app/src/main/cpp/forgeshape_cad_offer.h (new), app/src/main/cpp/forgeshape_cad_offer.cpp (new), app/src/main/cpp/forgeshape_editor_core.cpp, app/src/main/cpp/forgeshape_jni.cpp, app/src/main/java/com/forgeshape/app/ForgeShapeSurfaceView.java, app/src/main/java/com/forgeshape/app/NativeViewport.java, desktop/src/desktop_editor.cpp

Move g_faceSketchOffer, g_edgeFinishOffer and their rules (forgeshape_jni.cpp:328-330 and 520-630; holeOfferBegin at 5130) into the core, with an explicit camera and viewport. The rules are resolveFaceSupportFromHit, the outside-edge filter, the 36 px / 35% room rule, withdrawal on load or close, and the hole draft. JNI keeps thin wrappers that emit the same tokens.
Desktop Properties, top of panel:
- A still left click on an eligible face fills a 'Face' section: Sketch here (through the chooser's own confirm), Round edges, Bevel edges, and Hole (Simple / Counterbore / Countersink, diameter, Through All or depth, Apply). Each is drawn only where the core accepts it.
- A click near a finishable edge opens 'Fillet / Chamfer': a distance seeded with the 1/2/5 value, a Fillet | Chamfer segment, an Apply primary, Enter and Esc.
Desktop Model ribbon:
- Create: Sketch, and Revolve in Ready when an axis exists.
- Modify: Fillet, Chamfer, Hole. Drawn while the active unlocked CAD body has an eligible face; clicked with nothing offered, they show the step tip 'Click a face' / 'Click an edge'.
- Inspect: Measure distance.
Pattern stays Android-only this round.
Hover (editorHoverPick, using the same rules):
- one pick per frame, drawn in the sketch-overlay slot;
- face boundary in #DCE6F2 at 35% with an 8% fill; a finishable edge in #DCE6F2 at 2 px; an offered face's boundary held at 60%;
- no blink, a fade of 100 ms or less, honouring the animation scale;
- it never commits, selects or offers;
- on Android only TOOL_TYPE_STYLUS and the mouse produce it.

Acceptance: - Android FaceSketchOfferTest, HoleTest, EdgeFinishTest and RevolvedPartHoleTest are green, and the full sharded suite runs once.
- Desktop headless: clicking a top face lists Sketch here / Round edges / Bevel edges / Hole.
- Desktop headless: Round edges at 0.05 adds one finish and one Undo, and Hole Apply cuts a bore.
- A hover capture shows the face boundary lit, with the fingerprint and history depth unchanged.
- Real stylus hover is UNVERIFIED.
Sources: IX-07, IX-05; mechanical engineer #3 and proposal 5; product lead face-acts slice (DESKTOP-R3); tablet seat hover proposal; accent-vs-neutral hover conflict resolved

### Edit the feature that made a face, and move Edit Sketch off the face [all]
Files: app/src/main/java/com/forgeshape/app/NativeViewport.java, app/src/main/cpp/forgeshape_cad_offer.cpp, app/src/main/java/com/forgeshape/app/CadExtrudeCanvasView.java, app/src/main/java/com/forgeshape/app/PropertyInspectorView.java, app/src/main/res/values/ids.xml, app/src/main/res/values/strings.xml, desktop/src/desktop_editor.cpp

The offer reports the producing body, featureId and kind (faceSketchOfferFeature), re-validated on every read.
The docked face column gains two rows after Sketch here:
- 'Edit <Extrude 2>' (id face_edit_feature, string 'Edit %1$s' from FeatureHistoryText.name), which calls sketchBeginEditFeature(body, id, true);
- 'Edit sketch' (id face_edit_sketch), the same call with false.
They are drawn only where native accepts them (not while sculpting, sketching or choosing), and a revolved part's disc opens the Revolve.
On a workbench, the canvas pencil chip cad_canvas_edit_sketch (564-577) leaves the face. It becomes a captioned 'Edit Sketch' in the docked column below the face acts and in the Properties header, hidden while a feature, hole or pattern editor or History is open. Only the hollow, non-clickable marker stays on the model.
The phone keeps placeAt(bodyAnchor). The desktop Face section gets the same two rows.

Acceptance: - Instrumented: tap a Cut's floor and choose 'Edit Cut'. Ready opens on that feature.
- Instrumented, on a workbench: a tap at the projected body sketch anchor produces a face offer, not a sketch session.
- Recaptured tab 02 and 07 show no pencil on the face.
Sources: Mechanical engineer proposal 3; IX-12; tablet seat 'Edit Sketch leaves the face'; product lead cut list

### Refusals in words, wherever a control is withheld or a step fails [all]
Files: app/src/main/cpp/forgeshape_cad_status_text.h (new), app/src/main/cpp/forgeshape_cad_status_text.cpp (new), desktop/src/desktop_editor.cpp, desktop/src/ui/ui.cpp, app/src/main/java/com/forgeshape/app/CadStatusMessages.java, app/src/main/res/values/strings.xml, app/src/main/java/com/forgeshape/app/FeatureHistoryView.java, app/src/main/java/com/forgeshape/app/PropertyInspectorView.java

cadStatusUserText(CadStatus) gives a short reason and an optional hint for ALL 123 codes, for example ToFaceBehind: 'The target face is behind the sketch plane — Flip, or choose a face on the other side'. A host check walks every enum value and fails on a fallback to cadStatusName.
Desktop: sketchStatusText (145-156) reads the table. Ui hover is allowed over disabled controls (312/339/358), and their tooltip shows the refusal reason.
Android: CadStatusMessages stays keyed by string resource, with the English strings generated once from the table. A JVM test asserts every NativeViewport CAD_* code has a string; 88 of 123 do today.
Both shells:
- a Failed History row shows its reason on hover or long-press;
- while the candidate is invalid, the precision surface's title line turns textError with a ✕ glyph and the reason beneath.
The owner reviews the sentences.

Acceptance: - The host check finds no fallback, and the JVM test passes 123/123.
- Desktop headless: a To Face target behind the plane shows a sentence, not 'ToFaceBehind'.
- A hover capture over a disabled ribbon Extrude shows its reason.
- A device capture of an invalid Cut shows the ✕ title line.
Sources: IX-08; mechanical engineer refusal proposal; accessibility disabled-control tooltips

### A context menu at the object, built only from acts that already exist [all]
Files: app/src/main/java/com/forgeshape/app/ViewportContextMenuView.java (new), app/src/main/java/com/forgeshape/app/ForgeShapeSurfaceView.java, app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/res/values/ids.xml, desktop/src/desktop_editor.cpp, app/src/androidTest/java/com/forgeshape/app/ContextMenuTest.java (new)

Android triggers, Construction only (never during a sketch, the chooser, Sculpt or a Freeform cage edit):
- a still 500 ms press within the 24 px tap slop, by finger or pen;
- a stylus Down with BUTTON_STYLUS_PRIMARY held;
- a mouse BUTTON_SECONDARY released within 4 dp. A right-drag still orbits.
When a trigger fires:
1. The Up is forwarded at the press point, so native resolves the ordinary tap (selection and face offer).
2. The rest of the gesture is swallowed.
3. The menu opens at that point, an AnchoredSurfaceView flipped above or to the left near the window edges.
Rows, each drawn only where its existing predicate accepts it:
- Edit <feature> (ctx_edit_feature)
- Edit sketch (ctx_edit_sketch)
- Sketch here (ctx_sketch_here)
- Fit to body (ctx_fit_body; frameBounds over the body's bounds)
- Rename (ctx_rename)
- Show/Hide (ctx_toggle_visibility)
- Lock/Unlock (ctx_toggle_lock)
- Duplicate (ctx_duplicate)
- Delete (ctx_delete)
Rows are 48 dp on touch and 32 px on the desktop; the menu closes on the next act or on Esc. Hole, Pattern, Mirror and Round/Bevel stay in the dock on Android (CAD-UI-DOCK-R1).
Desktop:
- a right press released within 4 px opens the same rows; a right-drag orbits as today;
- each Browser row gains a trailing eye (a crossed eye and 60% text when hidden) and a lock badge;
- F2 or a double-click renames inline;
- right-clicking a row opens the same body rows;
- every act is one Undo through forgeshape_body_commands.

Acceptance: ContextMenuTest drives finger, injected stylus and mouse:
- a long-press on a body opens ctx_rename and the other rows, and Rename is one Undo;
- a long-press while a sketch is Editing opens no menu;
- a right-drag moves the camera and opens no menu.
Desktop headless:
- a still right-click on a body opens the menu;
- Hide makes the row show a crossed eye.
Sources: IX-04, IX-30; tablet seat top 3 #2; product lead context menu; mechanical engineer proposal 13; two-finger-tap and in-sketch vetoes honoured

### One shortcut table for both shells, published and switchable [all]
Files: app/src/main/cpp/forgeshape_editor_shortcuts.h (new), app/src/main/cpp/forgeshape_editor_shortcuts.cpp (new), app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/java/com/forgeshape/app/ToolRailView.java, app/src/main/java/com/forgeshape/app/AppPreferences.java, app/src/main/java/com/forgeshape/app/SettingsPageView.java, desktop/src/desktop_editor.cpp, desktop/src/ui/ui.cpp

One constexpr table of {key, modifiers, act, scope, caption}:
- Sketch Editing: V Select, L Line, P Polyline, R Rectangle, C Circle, A Arc, S Spline, T Trim, D Dimension, X Make Construction/Regular.
- Enter: Finish Sketch in Editing and Extrude in Ready, only where that control is drawn. Esc: cancel or step back once.
- Model: Shift+S New Sketch, Shift+F Round edges on the offered face or edge, Shift+H Hole on the offered face.
- View: F Fit; Ctrl+1..7 = Home, Front, Back, Left, Right, Top, Bottom.
- Ctrl+Z/Y/S and Del as today. Shift+/ opens the shortcut sheet: a desktop overlay, and the system helper on Android.
Rules:
- Keys are inert while a field has focus or outside their scope.
- Each key calls the handler its control calls, so a key cannot do what a withdrawn control cannot.
- Construction Undo stays refused under a sketch, through historyUndo.
Publishing:
- Android publishes Sketch, Model and View groups through onProvideKeyboardShortcuts.
- Every tooltip appends its key ('Rectangle  R').
- The desktop's permanent mouse hint moves into the Shift+/ sheet.
Settings › Interface 'Single-key shortcuts' (an AppPreferences field, default on, missing key = on) turns the single letters off; the desktop reads the same setting from its own preferences file (WCAG 2.1.4).

Acceptance: - Host check: no duplicate (key, modifiers, scope).
- Instrumented: hardware R in Editing holds Rectangle, R in Construction does nothing, and R in a focused field types 'r'. With the preference off, R does nothing.
- SettingsPreferencesTest keeps the project bytes identical.
- Desktop headless: L then a drag draws a Line, Ctrl+5 gives the Right view, and Shift+/ opens the sheet.
Sources: IX-11; mechanical engineer proposal 4; tablet seat keyboard step 2; accessibility shortcut table (2.1.4 toggle); product lead keyboard (2); S-vs-I conflict resolved

Critique: On the PC, edit a part four ways: History chip, Features row, face and context menu. Then fillet an edge, drill a hole, and read every refusal as a sentence. On the tablet, tap a Cut floor and edit it from the dock; the pencil should no longer sit on the face. Long-press opens a menu at the finger that flips at the edges. Hover is calm and never takes a selection. Tooltips show their keys.
Watch for:
- regressions in the moved JNI paths (run the full sharded suite);
- the long-press firing during a slow orbit;
- hover cost on patterned parts;
- the desktop Properties panel growing too long once Face, Features and Fillet sections stack.

## Iteration 4: The tablet laid out like the PC, and drawing that works under pen, finger and mouse
On an EXPANDED workbench the tablet takes the PC's arrangement: a captioned command row on top, panels flush to the edges, and a full-width History strip with Undo/Redo inside it. Commit and abort sit together, and the pen owns its stroke. Shapes can be placed by tap-tap, with live sizes that land on round numbers. Phone and medium windows are unchanged.

### Command row under the Global Toolbar (WORKBENCH-RIBBON-R2), with command-first Sketch, Hole and Round [tablet (EXPANDED workbench only; phone gets palette entries)]
Files: app/src/main/java/com/forgeshape/app/WorkspaceTrailingHostView.java, app/src/main/java/com/forgeshape/app/ToolRailView.java, app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/java/com/forgeshape/app/GlobalToolbarView.java, app/src/main/java/com/forgeshape/app/ToolbarRowBudget.java, app/src/main/java/com/forgeshape/app/AddPrimitivePaletteView.java, app/src/androidTest/java/com/forgeshape/app/TabletWorkbenchTest.java, app/src/androidTest/java/com/forgeshape/app/EditorWorkspaceLayoutTest.java, CLAUDE.md (scoped amendment, OWNER sign-off)

This applies only while WorkspaceLayoutMode.workbench holds. The ONE trailing host lays contentColumn and transformSelectorRow horizontally, seated as a full-width 64 dp row directly under the Global Toolbar.
Layout: entries are max(64 dp, caption + 16 dp) wide and 56 dp tall, with a 24 dp icon and an 11 sp medium caption. Groups are split by 12 dp plus a 1 dp fsChromeHairline.
Construction:
- Shape and Transform. While Transform is held, Move/Rotate/Scale and World/Local appear inline.
- A Model group: Sketch, Hole and Round edges, each drawn only where native says it can succeed.
- Hole or Round with no face offered set a session-only pending intent: the entry is lit and the status chip shows 'Tap a face to drill' or 'Tap a face to round its edges'.
- The next face offer then runs holeOfferBegin or the face-edge finish.
- A second tap, Esc, a load, a close or any offer withdrawal ends the intent.
Sketch: Select … Trim, as the PC's Draw group.
Ids, handlers and handedness are unchanged; on Left handedness the row reads mirrored.
The Properties toggle moves into the trailing toolbar capsule.
Phone: Sketch, Hole and Round edges join AddPrimitivePaletteView, and 'Add body' becomes 'Add'.
CLAUDE.md: the trailing-host vertical-group sentence gains 'on an EXPANDED workbench the same host lies horizontally under the Global Toolbar'. This needs owner sign-off before merge.

Acceptance: - Recaptured tab 02, 03 and 05 show the row under the toolbar and no right-edge rail.
- TabletWorkbenchTest and EditorWorkspaceLayoutTest run at 1706×1066 dp and on an 840 dp-wide EXPANDED window, at font scales 1.0 and 1.3, in Construction, Editing and Ready. No command-row, navigator, Modify, toolbar or strip view intersects property_inspector or the Browser.
- The phone and medium assertions are unchanged: the 76-case layout lane is green.
- A host check confirms the pending intent never reaches the fingerprint.
Sources: Product lead #1 (owner ask; PROJECT_STATUS 'Not this stage: WORKBENCH-RIBBON-R2'); IX-06; tablet seat command-first proposal; VC-25; IX-32

### Commit and abort sit together in the transition capsule [tablet (workbench)]
Files: app/src/main/java/com/forgeshape/app/GlobalToolbarView.java, app/src/main/java/com/forgeshape/app/ToolbarRowBudget.java, app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/res/values/strings.xml

Capsule layout:
- Editing: [✕ Cancel] [Sketch · XY] [Finish Sketch]
- Ready: [✕ Cancel] [← Back to Sketch] [Sketch · XY] [Extrude]
Style: Cancel and Back are tonal, captioned pills on the LEADING side. The sketch label separates them from the filled primary, so the destructive abort never shares an edge with the commit. Cancelling a non-empty sketch cannot be undone.
Ids: sketch_cancel and the Back id are kept, because the act is the same act.
One home: the rail's uncaptioned ✕ and ← are not drawn on a workbench. The phone and medium windows keep them.
ToolbarRowBudget measures the pills, and critical navigation is never abbreviated. Enter and Esc come from the table in iteration 3.

Acceptance: - Recaptured tab 03 and 05: Cancel is captioned and sits inside the capsule, and its 48 dp box shares no edge with Finish or Extrude.
- Instrumented: tapping sketch_cancel cancels and creates no body.
- Instrumented, on a phone: the rail Cancel still exists.
Sources: IX-13, IX-32; tablet seat proposal 4; accessibility commit/abort Fitts analysis; product lead (2); confirmation-corner veto honoured

### A full-width History strip with Undo/Redo inside it, and chips that read as steps [tablet (workbench)]
Files: app/src/main/java/com/forgeshape/app/HistoryStripView.java, app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/java/com/forgeshape/app/FeatureHistoryView.java, app/src/main/java/com/forgeshape/app/FeatureHistoryPresentation.java, app/src/main/res/values/strings_modeling.xml

The strip runs full width along the bottom: the 'History' label, then Undo/Redo inside its leading end, mirrored with handedness. The separate trailing history capsule is not drawn on a workbench. As on the desktop, the strip arrows are Undo and Redo, and there is no marker.
Chips:
- the '•' from row.mark() becomes an 18 dp per-feature icon, from one Java table mirroring the desktop's timelineRowIcon;
- 1 dp fsFieldBorder; radius = host radius − gap; drawn 40 dp tall inside a 48 dp target;
- textPrimary label; a Failed row keeps its cross.
The Browser's History drops its 'Steps' segment while the strip is visible, keeping Sketches and Features; Steps returns when the strip is hidden, as in a sketch. The Browser footnote (strings_modeling.xml:13) shows only while History is empty.

Acceptance: - Recaptured tab 02 and 08: icons in the chips, Undo/Redo inside the strip's leading end, no Steps segment.
- UILR1-11: chip border ≥ 3:1 against the strip in all five palettes.
- The Undo/Redo ids lie inside the strip's bounds.
- The TabletWorkbenchTest strip case is green.
Sources: VC-24, IX-19; tablet seat 'bottom row on the off-hand side'; product lead (4); owner ('back and forward arrows, no slider')

### Tablet panels docked flush, with Apply beside the fields [tablet (workbench)]
Files: app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java, app/src/main/java/com/forgeshape/app/PropertyInspectorView.java

Docking: the Browser sits at x = 0 and Properties at the trailing edge, each running from the command row's bottom to the strip's top. Both have radius 0, no shadow, opaque fsSurfaceContext and a 1 dp fsChromeHairline on the model side. Collapse is unchanged. This is the same spec as the desktop's docked panels from iteration 1.
Apply/Revert: on a workbench, the dirty-only pair from iteration 1 is pinned in the panel's header row beside the title, instead of a footer about 600 dp below the fields. IME Done still commits.

Acceptance: - Recaptured tab 02: the panels meet the window edges.
- Editing Width shows Apply inside the header row's bounds.
- The EditorWorkspaceLayoutTest workbench cases are green.
Sources: VC-21 applied to the tablet for one spec; IX-20 (placement only); accessibility commit-placement proposal; product lead (3)

### The pen owns its stroke: palm and finger rejection at the surface boundary [tablet]
Files: app/src/main/java/com/forgeshape/app/PalmFilter.java (new), app/src/main/java/com/forgeshape/app/ForgeShapeSurfaceView.java, app/src/test/java/com/forgeshape/app/PalmFilterTest.java (new), app/src/androidTest/java/com/forgeshape/app/PenPalmTest.java (new)

PalmFilter runs before NativeViewport.touchEvent:
(a) While any pointer reports TOOL_TYPE_STYLUS or TOOL_TYPE_ERASER, only stylus pointers are forwarded. A finger POINTER_DOWN or POINTER_UP during a pen gesture becomes a MOVE of the stylus set, so a palm never becomes the 'second pointer' that restores the pre-drag state.
(b) On API 33 and later, a POINTER_UP carrying FLAG_CANCELED drops that pointer, and an UP carrying it is forwarded as ACTION_CANCEL. That restores the pre-drag state and records nothing.
(c) A gesture with no stylus is forwarded byte-for-byte, so finger pan and pinch are unchanged.
Native code and forgeshape_input.h are untouched.

Acceptance: - PalmFilterTest passes on synthetic pointer arrays.
- PenPalmTest: a pen Line with a finger landing mid-stroke commits exactly one entity.
- PenPalmTest: a pen extrude-arrow drag with a finger landing keeps its depth.
- EditorWorkspaceGestureTest is green.
- Real S Pen palm behaviour is recorded as UNVERIFIED (a physical-device gate).
Sources: Tablet seat top 3 #1 (new finding, not visible in any screenshot)

### Tap-tap placement with a hover rubber band, and snaps shown before the press [all]
Files: app/src/main/cpp/forgeshape_sketch_session.h, app/src/main/cpp/forgeshape_sketch_session.cpp, app/src/main/cpp/forgeshape_editor_core.cpp, app/src/main/cpp/forgeshape_jni.cpp, app/src/main/java/com/forgeshape/app/ForgeShapeSurfaceView.java, desktop/src/desktop_editor.cpp, app/src/main/cpp/forgeshape_sketch_ux_selftest.cpp

Line, Rectangle and Circle:
- A still tap with nothing pending arms pendingFirst_ at the snapped point, drawn as a point marker.
- The next still tap places the shape through placeFromDrag's own addSketchEntity path.
- Tapping the pending point again, Esc, Back, a tool change or sketch Undo drops it first. It is never an entity and never an Undo entry.
Arc: tap the start, tap the end, then the existing through-point tap.
Press-drag is unchanged.
Hover: SketchSession::hover(x, y) updates cursor_ and the snap with no mutation and no Undo entry. The desktop forwards mouse moves with no button held (1752-1769); Android forwards stylus and mouse HOVER_MOVE on the existing route (226-236).
While a point is pending, the step tip reads 'Tap the opposite corner'.

Acceptance: Host sketch-UX checks:
- a tap-tap rectangle equals the drag rectangle bit for bit;
- a tap followed by Esc gives no entity and leaves the undo depth unchanged;
- hover mints nothing.
A desktop headless hover capture shows the snap marker before the press. SketchKnifeUndoTest and SketchExtrudeTest are green.
Sources: Accessibility top 3 #2 (WCAG 2.5.7); IX-10(a); mechanical engineer precision (b)

### Live sizes while placing, landing on round numbers [all]
Files: app/src/main/cpp/forgeshape_sketch_session.cpp, app/src/main/cpp/forgeshape_sketch_overlay.cpp, app/src/main/cpp/forgeshape_cad_extrude_tool.cpp, app/src/main/java/com/forgeshape/app/SketchDimensionLabelView.java, desktop/src/desktop_editor.cpp

Live labels: while a Line, Rectangle, Circle or Arc is being placed (by drag, or tap-tap with a point pending), native emits READ-ONLY labels in the Dimension overlay range, with label anchors:
- Line: L and ∠
- Rectangle: W and H
- Circle: Ø
- Arc: R
They are drawn as the iteration 2 plates. They mint no revision, reach no snapshot and are never exported. They are hidden by priority, never placed over the stroke, and a Driving label wins over them.
Quantising, applied only to drags (a typed value is never snapped):
- a dragged sketch size with no geometric snap winning rounds to the view-adaptive sketch grid step;
- an extrude drag rounds to the 1/2/5·10^k step of about 8 px at the FROZEN pointer-down metersPerPixel, before setExtrude, so validation, the millimetre floor and push-through are unchanged;
- the revolve ring uses whole degrees, 5° steps when coarse;
- holding Alt on the desktop suppresses quantising for that gesture.

Acceptance: - Recaptured tab 04 mid-drag shows 'W' and 'H' plates; after release, Height reads 0.5, not 1.0000000000000002.
- Host check: an extrude drag lands on a 1/2/5 step, and zooming mid-drag does not move the value.
- The live labels are absent from the snapshot and the GLB.
- The drag tests are updated to the new expected values.
Sources: IX-10, VC-26 (root cause); mechanical engineer 'drags land on round numbers'; tablet seat values at the geometry; product lead live labels (4)

Critique: Lay tablet 02, 03 and 05 beside desktop-02, 03 and 04. Check that:
- the arrangement matches: row on top, flush panels, a strip with Undo/Redo at its leading end;
- Cancel and Finish sit in one capsule;
- the navigator still stands clear at 1.3 font scale and on Left handedness;
- the row fits on an 840 dp EXPANDED window.
Draw a rectangle by tap-tap and confirm its live W×H labels are readable and land on round numbers.
Watch for:
- label crowding on small shapes;
- a stray tap arming a point;
- the command row's Model group at narrow widths;
- muscle-memory loss from the old right rail.
Pen, palm and hover remain physical-device gates; list them as UNVERIFIED.

## Iteration 5: Polish and consistency
The two shells look and speak like one product:
- one drawing per act, on the pixel grid;
- a view cube that a finger, a mouse and TalkBack can all drive;
- one Home page;
- one vocabulary;
- chrome that reports and does not instruct;
- every icon-only control explained.

### One drawing per act, on the pixel grid [all]
Files: app/src/main/res/drawable/ic_display.xml, app/src/main/res/drawable/ic_cad_part.xml (new), app/src/main/res/drawable (view-bar Front/Top/Right glyphs), desktop/src/desktop_editor.cpp, desktop/src/ui/ui.cpp, app/src/test/java/com/forgeshape/app/DrawableUniquenessTest.java (new)

Drawings:
- ic_display becomes a half-lit sphere (a circle and a filled crescent).
- A new ic_cad_part (a cube with a sketched rectangle on its front face) is used for Make CAD part and CAD Body rows on both shells (desktop_editor.cpp:1096, 1560).
- ic_op_new_body means New Body only.
- The view-bar Front, Top and Right glyphs fill their target face at 100% with a 2-unit stroke.
Desktop title bar:
- Close project moves into the ForgeShape mark's menu (1019), so ✕ means Cancel and nothing else.
- Reload theme moves into Settings (1023).
Icon sizes on the desktop: only 16, 20 and 24 px. Button icons go from 17 to 16 px (ui.cpp:324), History-chip icons from 18 to 20 px, and the icon origin is snapped to whole pixels.
Tests: a JVM test fails when two res/drawable/ic_*.xml files share pathData, and a desktop check confirms one drawable per icon name.

Acceptance: - DrawableUniquenessTest is green.
- Recaptured tab 02: Display reads as a sphere, not the mark or a Box.
- The desktop title bar has no ✕ in its trailing corner.
- 20 px crops of the three view glyphs are visibly distinct.
Sources: VC-19, IX-27, VC-34; visual seat proposal 11; product lead icons

### A view cube that a finger, a mouse and TalkBack can drive, and a view bar that shows the current view [all]
Files: app/src/main/java/com/forgeshape/app/WorkbenchViewCubeView.java, app/src/main/java/com/forgeshape/app/WorkbenchViewBarView.java, app/src/main/cpp/forgeshape_camera.h, app/src/main/cpp/forgeshape_editor_core.cpp, app/src/main/cpp/forgeshape_gizmo.cpp, app/src/main/res/values/attrs.xml, desktop/src/desktop_editor.cpp

Cube behaviour (WorkbenchViewCubeView 139-163, desktop 1248-1318):
- A press that travels more than 8 dp (4 px on the desktop) orbits through cameraPointerOrbit and snaps nothing.
- Tapping the end that already faces the viewer (within 10°) goes to the opposite view, so Bottom, Back and Left become reachable.
- A still 500 ms press opens Home, Perspective/Orthographic and Fit.
Cube drawing:
- Negative ends become 1.5 dp rings at full axis colour, with '−X/−Y/−Z' visible at rest.
- Axis colours come from the gizmo palette, through fsAxisX/Y/Z roles parity-tested against kDarkPalette/kLightPalette.
- Letters are sized in sp.
TalkBack: an AccessibilityNodeProvider exposes six virtual nodes ('Right view' … 'Back view') with ACTION_CLICK, invalidated on every camera refresh.
Views:
- StandardView::Isometric is APPENDED after Bottom, so indices 0..6 do not move; check that no JNI caller sizes an array by kStandardViewCount. It is azimuth 45°, elevation 35.264°, span kept.
- The view bar gains Iso, lights the button whose view the camera equals (cleared on orbit, pan or zoom), and shows projection as two segments.
- The iteration 3 context menu gains 'Normal To' (editorNormalToOffered): exactly along the offered face's normal, up from the face frame's v, span kept.
No corner targets on touch.

Acceptance: Instrumented:
- in Top view, tapping +Y gives Bottom;
- a drag on the cube orbits the camera without snapping;
- the TalkBack virtual node count is 6.
Recaptured tab 07: the negative ends are labelled rings, and Top is lit on the view bar. The host camera self-test confirms the Iso pose is exact and indices 0..6 are unchanged.
Sources: IX-15, IX-31, VC-31; mechanical engineer views proposal; tablet seat cube; accessibility cube node provider; accessibility vetoes on touch corners and hover-only labels

### One Home on both shells [all]
Files: app/src/main/java/com/forgeshape/app/StartPageView.java, app/src/main/res/values/dimens.xml, desktop/src/desktop_editor.cpp, desktop/src/ui/theme.h

On EXPANDED windows:
- the content column is centred, with a max width of 720 dp (start_page_max_width 520 → 720);
- the mark grows from 40 to 64 dp, with the wordmark at 28/36;
- New Project and Open File… become two side-by-side tiles, 160 dp tall, each with a 28 dp icon, a 16/24 medium title and one 14/20 secondary line;
- Settings is a 48 dp secondary button aligned to the column edge;
- 'Nothing is created until you choose.' is removed.
Desktop (905-921):
- draws the same full-window page instead of a card, and drops 'Ready';
- 0xFF262421 becomes a startPageSurface token;
- the starts are named 'New Project' and 'Open File…', and New Project lists only the kinds the desktop editor can open today.
The phone keeps its single column, with the same ids.

Acceptance: - Recaptured tab 01 and desktop-01 follow this spec.
- HomeFlowTest is green.
- A compact-phone capture shows the unchanged single column.
Sources: VC-28, IX-28 (layout part); visual seat Home proposal

### One vocabulary and one toggle grammar [all]
Files: desktop/src/desktop_editor.cpp, app/src/main/res/values/strings.xml, app/src/main/res/values/strings_modeling.xml, app/src/main/res/values/strings_cad_r2.xml, app/src/main/java/com/forgeshape/app/CadFeatureEditorView.java, app/src/main/java/com/forgeshape/app/DisplaySettingsPopoverView.java

Vocabulary:
- The desktop representation label 'CAD part' becomes 'CAD Body'. The act 'Make CAD part' keeps its CLAUDE.md name.
- 'Open File…' and 'Selection outline' are used in sentence case on both shells.
- Both shells use <plurals> '%d curve' / '%d curves'.
Extrude labels:
- Each label follows its extent: One Side 'Distance', Symmetric 'Each side', Two Sides 'Side A' / 'Side B'.
- 'Along normal / Against normal' becomes a single Flip toggle chip, in One Side only.
- The desktop's 'Flip direction' becomes 'Flip'.
- A CAD Body's panel title reads 'CAD Body — Body #2', not 'Exact Shape'.
Display popover:
- Grid, Selection outline and Edges become switch rows: a 14 sp label and a 48 dp Switch, matching the desktop.
- 'Debug' shading shows only while debug.forgeshape.selftests is set. BuildConfig.DEBUG would not hide it from the owner's debug APK.

Acceptance: - String tests are updated.
- Recaptured tab 02: the Browser reads 'XZ · 1 curve'.
- Recaptured tab 05: a Flip chip instead of Along/Against.
- desktop-02: the row label reads 'CAD Body'.
- A capture with the property unset shows no Debug chip.
Sources: VC-23, IX-22, VC-35, VC-24

### Chrome reports, it does not instruct [all]
Files: desktop/src/desktop_editor.cpp, app/src/main/java/com/forgeshape/app/GlobalToolbarView.java, app/src/main/java/com/forgeshape/app/StartPageView.java, app/src/main/res/values/strings.xml

Desktop:
- The status bar carries verdicts only. The mouse hint (1621-1629) lives in the Shift+/ sheet, and a dismissible first-run chip points there once.
- The left status echo and the buildSketchProperties hint lines are removed.
- The History strip's empty state follows the state: in Editing 'Finish the sketch to add it', in Ready 'Choose a region, then Extrude'.
Tablet:
- A tool's hint shows on its first use per tool per session, then only verdicts follow.
- The hint stays reachable as the entry's tooltip.
- Step tips while a tool waits for input stay.
Both shells: during the first sketch the Browser's empty state reads 'No parts yet — Extrude creates the first one'.

Acceptance: - Recaptured desktop-03/04: at most one instruction channel.
- Picking Rectangle a second time in a session shows no hint.
- A mouse hover on the Rectangle entry shows its tooltip.
Sources: VC-27, IX-23, IX-28 (empty states); accessibility veto on removing reachable text honoured

### Every icon-only control explained, and captions everywhere [all]
Files: app/src/main/java/com/forgeshape/app/EditorControlStyles.java, app/src/main/java/com/forgeshape/app/GlobalToolbarView.java, app/src/main/java/com/forgeshape/app/WorkbenchViewBarView.java, app/src/main/java/com/forgeshape/app/HistoryStripView.java, app/src/main/java/com/forgeshape/app/CadExtrudeDockView.java, desktop/src/ui/ui.cpp, desktop/src/desktop_editor.cpp

Android:
- One helper, EditorControlStyles.iconAction(view, description), sets setTooltipText to the content description.
- It is applied to Display, Hide, the collapse and panel toggles, the mark, the view-bar buttons, Undo/Redo and the dock badge.
- On a workbench the toolbar capsule captions Export, Display and Hide alike; the phone keeps its budgeted forms.
Desktop tooltips:
- appear after a 500 ms delay that resets on leave;
- show the name, the key and one sentence ('Through All — cuts right through the part, however thick it becomes').
Desktop extent and operation rows: segmented cells with a 12 px caption under every icon, wrapping to two rows in the 276 px panel rather than shrinking text.

Acceptance: - Instrumented: a mouse hover on Display shows a tooltip equal to its content description.
- Recaptured desktop-05: every extent and operation icon carries a caption of 12 px or more.
- A tooltip capture shows nothing before 500 ms.
Sources: VC-25, IX-21, IX-32; accessibility tooltips proposal and caption-floor veto

Critique: Lay matching desktop and tablet captures side by side for Home, the part, a sketch, Ready and the Top view. Check that the words, icons, selected treatment, Home layout and toggle grammar match across the two.
Also check:
- the cube's negative ends can be read at a glance and reached by tap;
- no ✕ is ambiguous;
- no permanent instruction remains;
- every icon-only control explains itself on hover or long-press.
Watch for:
- caption wrap in the desktop extent rows;
- light-palette axis colours;
- the tiles at the narrowest EXPANDED width;
- any string a test still asserts in its old form.

## Deferred
- Desktop keyboard focus traversal and focus ring, Windows text scaling, and Windows contrast themes: These are real WCAG 2.1.1, 2.4.7 and 1.4.11 gaps (VC-15, IX-29, accessibility seat), but they are large and best built once iterations 1–4 have shrunk and settled the widget set. They form their own desktop-accessibility stage, and the product lead recommended scheduling them that way. Until then, tooltips on disabled controls (iteration 3) and the shortcut sheet cover part of the need.
- Exact-value input stage: arithmetic in desktop fields, a numeric value pad on the tablet, and an mm display unit on the desktop that Android also remembers: The default length unit is an OWNER decision. Iteration 1's round-trip numberField already removes the 1 mm quantisation that made mm urgent. The value pad is the largest and least urgent item, and its own seat ranked it last. The three should land together so both shells share one expression reader.
- Hole size presets (ISO 273 / 4762 / 10642): These only make sense when the display unit is mm (deferred above). The owner must also check the values against the standards first, because a wrong preset is worse than none.
- Adding and removing edges of a Fillet or Chamfer while its editor is open: This needs iteration 3's core offer to settle first. It also needs a stated tap-routing rule inside an open editor: arm on Down, cancel on travel, and a drag still orbits. The format already stores 1..32 references, so no CADB change is involved.
- Editable size labels on a selected Rectangle, Circle or Arc, and SolidCraft-style typed value boxes while placing: Both need a new pending-shape input model in SketchSession, and the product lead vetoed adding it this round. Iteration 4 ships the read-only live labels first.
- A finger pans the sketch once a pen is in use, with a 'Finger in a sketch' preference: This changes a durable CLAUDE.md sketch rule, so it needs owner approval. It is also a physical-device gate that the emulator cannot close.
- Staged live preview on field commit for the Hole, Pattern and Fillet editors: Each editor needs its own staged candidate path, which is weeks of work, and CLAUDE.md lists 'a live preview while typing' as not this stage. Meanwhile iterations 1 and 4 make Apply dirty-only and pin it beside the fields.
- Recent-projects grid with thumbnails on Home: It needs offscreen thumbnail rendering and MRU persistence, a new subsystem for a screen that is seen once per session (product lead veto).
- Isometric corner and edge targets and roll arrows on the view cube: On a 112 dp touch cube, 14 targets leave cells of 25–30 dp under a 48 dp fingertip (accessibility veto). Iteration 5's Iso button, opposite-end tap and drag-to-orbit cover the need. Revisit for mouse and stylus, or for a larger cube.
- Status message log (the last 50 messages, opened from the chip): It is optional, and should wait until iteration 1's floating chip and its hold rules have been judged on a device.
- Pattern on the desktop: The desktop has no Pattern editor yet. Its History rows stay flat text rather than buttons that do nothing.
- Sculpt and Surface starts on the desktop's Home: The desktop editor cannot open those kinds yet, and a control that cannot succeed is not drawn.
- Fuzzy command search and a desktop right-drag marking menu: Both are new subsystems, and the plain context menu (iteration 3) has to exist and prove itself first.