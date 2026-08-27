# UI-AUDIT2D — Mobbin comparative pattern audit (independent pass)

**Result: COMPLETE.** Mobbin invocation proven, 15 targeted invocations across 13
pattern families, 65 references inspected and tied to named ForgeShape findings.

Read-only. **No product source, resource, shader, layout or build file was
changed.** Product HEAD for this audit is `bd3f7d5` (Stage 020R3). The only
repository changes are this evidence directory. No device was used, no emulator
was started, no APK was built: this is a comparative pattern pass over an
external library, and every ForgeShape fact it reasons about comes from the
runtime evidence already recorded in `artifacts/uiaudit1/`, `uiaudit2a/`,
`uiaudit2b/` and `uiaudit2c/`.

## Method, and the limit of this audit's authority

Mobbin is a **comparative pattern library, not the authority for ForgeShape.**
Where a Mobbin pattern and a ForgeShape hard rule disagree, the hard rule wins and
the pattern is recorded as rejected — that happens six times below. Popularity is
not treated as correctness anywhere: several of the most polished references in
the set are argued against at length in §6.

The earlier audit reports were read **as evidence, not as conclusions**. Two of
their positions are contested here on the strength of what the library shows:
UI-AUDIT2B's proposed sheet-pivot change (E-03) and UI-AUDIT2A's framing of the
Objects capsule as needing no work. Both are recorded in §4 with the
counter-evidence.

### One structural limitation, stated up front

The Mobbin tool exposes exactly two platform values: **`ios` and `web`**. There is
no `android` and no tablet or iPad value. Every mobile reference in this audit is
therefore an **iOS phone** capture. Three consequences, none of them hidden:

1. **No Android convention is confirmed by this audit.** Where iOS and Android
   diverge — System Back (I-03), the 48 dp floor versus 44 pt, edge-to-edge inset
   handling — Mobbin has nothing to say and this report says nothing.
2. **No reference here is a tablet capture.** The brief asked for "dense
   professional iPad/tablet productivity UI"; the library, through this tool,
   cannot serve it. Every phone-versus-tablet claim below is *reasoned from the
   phone evidence* and is labelled as reasoned. This matters because ForgeShape's
   expanded window class is the one both prior audits called already-correct, and
   this audit cannot corroborate that from references.
3. **Stylus behaviour is not observable.** Two references imply stylus use (R01
   and R05 are Apple markup surfaces), but no pressure, hover or barrel-button
   behaviour is visible in a static capture. Stylus implications below are
   reasoned from target sizes and from what the tool would have to do, not
   observed.

---

## 1. Mobbin proof

The full log — exposed tool schemas, every query string, every returned
reference, and which were inspected versus seen-and-discarded with reasons — is
[`MOBBIN-INVOCATION-LOG.md`](MOBBIN-INVOCATION-LOG.md). Summary:

| fact | value |
| --- | --- |
| server namespace | `mcp__mobbin__` |
| exposed tools | `mcp__mobbin__search_screens`, `mcp__mobbin__search_flows`, `mcp__mobbin__search_sections` |
| how they were reached | surfaced as **deferred** tools (name only, no schema); loaded with `ToolSearch(query="select:mcp__mobbin__search_screens,mcp__mobbin__search_flows,mcp__mobbin__search_sections")` before any call |
| platform values available | `ios`, `web` only — **no `android`, no tablet** |
| invocations | **15** (14 `search_screens`, 1 `search_flows`) |
| `search_sections` | deliberately not invoked — it searches marketing website sections, irrelevant to a native 3D editor |
| result rows returned | 81 |
| references inspected and catalogued | **65** (R01-R64 plus R21b) + 2 flows (F01-F02) |
| returned but not used | 12, each with a recorded reason |
| web searches used | **0** |

| # | query (verbatim) | returned | inspected |
| --- | --- | --- | --- |
| Q01 | floating contextual toolbar over a canvas with tool icons, in a drawing or design app | Apple Mail, Craft, Freeform, Obsidian, Apple Notes, Spotify | 6 → R01-R06 |
| Q02 | tablet side inspector panel with layer properties next to a canvas | Play, Freeform, Grok Bot, Spotify, Photoroom, Pinterest | 5 → R07-R11 |
| Q03 | bottom sheet with editable numeric input fields and unit values for object dimensions | Play, Yazio, Crouton, Alta, Alma, Tinder | 5 → R12-R16 |
| Q04 | layers list panel showing stacked objects with visibility toggles in an editor | Play, Photoroom, Notion, Timepage, Grab, Bevel | 4 → R17-R20 |
| Q05 | 3D model viewer or AR object placement with rotation and scale controls | Best Buy, Target, Apple Store, Crate & Barrel, Tonal, Replika | 5 → R21-R25 |
| Q06 | photo editor with a brush size slider and strength slider along the edge of the image | Edits, Google Arts & Culture, Telegram, Apple Photos, eBay, Shopee | 6 → R26-R31 |
| Q07 | version history panel with undo redo and a list of recent edits | Wabi, Canva, Mimo, Obsidian, Replika, Squarespace | 5 → R32-R36 |
| Q08 | project file browser home screen with a grid of design document thumbnails and a new project button | Edits, Google Drive, Riveo, ElevenLabs, CapCut, Canva | 6 → R37-R42 |
| Q09 | settings and preferences screen with grouped rows and toggles in a creative app | GitHub, Obsidian, Google TV, Replika, MasterClass, Wispr Flow | 6 → R43-R48 |
| Q10 | segmented control switching between edit modes at the top of a full screen editor | Posh, Linktree, Vocabulary, Google Photos, Bears Gratitude, Craft | 5 → R49-R53 |
| Q11 | long press context menu with grouped destructive and duplicate actions over selected content | Obsidian, Craft, Play, Notion, Freeform, yope | 5 → R54-R58 |
| Q12 | *(flows)* creating a new project and choosing a starting template in a design tool | Play, Unfold, Play (dup) | 2 → F01-F02 |
| Q13 | video editing timeline with dense tool controls and precise trim handles | GoPro Quik, Netflix, Polarsteps, Shopee, X | 3 → R59-R61 |
| Q14 | measuring tool showing dimension labels overlaid on a real world object | Best Buy, Redfin, Target, Bump, Apple Store | 3 → R62-R64 |
| Q15 | Best Buy AR product placement with dimension labels and a selection bounding box | Best Buy | 1 → R21b (id re-verified in full) |

Q15 exists because the Best Buy screen id appeared twice in scrolled output and
this audit would rather spend one extra call than publish a UUID transcribed from
memory. The log says so explicitly.

---

## 2. Reference catalogue

Full catalogue with per-reference observations:
[`REFERENCE-CATALOGUE.md`](REFERENCE-CATALOGUE.md). Thirteen families, 65
references. The eight that carry most of this audit's weight:

| REF | app | pattern | ForgeShape surface | why it matters here |
| --- | --- | --- | --- | --- |
| **R12** | Play | numeric sheet: preset chips, huge right-aligned value, dedicated clear, **unit segmented beside the value** | Exact Transform | Answers I-08, I-09 and uiaudit1-E in one composition |
| **R32** | Wabi | history rows **named by the act** ("Title changed to Meal planner recipe") with per-row revert | Undo/Redo | The answer to I-19, which is a naming problem, not a list problem |
| **R04** | Obsidian | right-edge vertical rail, **top-anchored**, two groups with a gap | Tool Rail | Independent structural confirmation of the finding-C / I-14 fix |
| **R01** | Apple Mail | **two-tier chrome**: contextual bar rides the selection, persistent tray owns the edge | Tool Rail + selector row | The governing principle of this whole audit |
| **R07** | Play | inspector **segmented by concern via tabs**, rows are label + value pill | Exact Transform | Removes the fold problem (I-10) without shortening anything |
| **R62** | Redfin | dimensions drawn **on the edges they measure**; active vertex marked | future CAD / dimension overlay | The most CAD-like screen in the library |
| **R27** | Google Arts & Culture | brush size shown as a **true-size tapered wedge**, no unit at all | Sculpt Radius (I-16) | Dissolves the `120 px` vocabulary problem instead of renaming it |
| **R43** | GitHub | **live preview of the thing being configured, above the settings that change it** | future Settings Hub | The one idea that makes a settings hub belong in a visual tool |

---

## 3. Pattern comparison, by ForgeShape surface

Each row: the current issue (from the prior audits' runtime evidence), what the
library actually shows, the principle common to the references, the verdict, and
a **principle-level** recommendation. No pixel values are prescribed from any
reference.

### 3.1 Top transition + utility group (Global Toolbar)

- **Current state.** Two floating groups rather than a bar. The transition button
  carries full wording in every measured window (`Back to Construction` 150.1 dp,
  `Resume Sculpt` 122.7 dp, `Start Sculpting` 121.1 dp), never an ellipsis.
  `Export` is drawn recessed and honestly labelled. `editing_context_label` is
  present in landscape and expanded, absent in compact portrait (I-13). Prior
  verdict: PASS.
- **Mobbin patterns seen.** R23, R24, R25 all pin a small mode switch top-centre
  over a 3D view with close and share flanking it, and nothing else. R03 and R59
  keep a fixed utility capsule (undo/share/overflow) top-right that never moves.
  R29 pins undo/redo top-left in a capsule of its own.
- **Common principle.** Over a canvas, the top edge carries **identity and
  escape** — where am I, how do I leave, how do I undo — and carries almost
  nothing else. None of these references puts a tool on the top edge.
- **Verdict: ADOPT PRINCIPLE (already largely held).** ForgeShape's top group is
  already this shape and should be protected from accretion.
- **Recommendation.** Treat the top group as closed to new controls. On I-13, the
  library is split — R23/R24 show a mode indicator top-centre even on a bare 3D
  view, which argues *for* showing the editing context in compact portrait; but
  ForgeShape's own rule says the rail entry and the transition button already
  answer "where am I". This audit does not raise I-13's severity: it stays P3, and
  the references neither confirm nor refute it. Recorded as **contested, low
  confidence**.

### 3.2 Shape / Transform rail (Tool Rail)

- **Current issue.** The rail itself is well built — icon and caption per entry,
  concentric corners, scroll container carrying the surface. The defect is
  positional: `rail.gravity = Gravity.CENTER_VERTICAL` in the floating (compact)
  branch (`EditorWorkspaceView.java:1329`) makes the whole right-edge column
  centre-anchored, so **any** height change moves every child by half the delta —
  measured at −139 dp, +56 dp and −120 dp on three unrelated state changes
  (finding C / I-14 / E-02). The docked branch already uses `Gravity.TOP` and shows
  zero rail movement.
- **Mobbin patterns seen.** R04 (Obsidian) is the direct structural twin: a
  vertical right-edge rail over a canvas, **top-anchored**, split into two groups
  with a deliberate gap. R01, R05, R06, R50, R53, R59 all pin their persistent tool
  tier to a screen edge and hold it there. R51 handles a rail too long for its
  space by **scrolling horizontally**, not by shrinking entries. In 65 inspected
  references there is **not one** instance of a persistent tool cluster
  centre-anchored on a variable-height container.
- **Common principle.** *The persistent tool tier is spatially constant.* A user
  builds muscle memory against a fixed edge, and contextual changes are absorbed
  by a different tier (§3.3).
- **Verdict: ADOPT PRINCIPLE.** This is the strongest cross-reference finding in
  the audit and it converges exactly with UI-AUDIT1's finding C, UI-AUDIT2A's
  I-14 and UI-AUDIT2B's E-02, all reached by different methods.
- **Touch/stylus.** A control that moves out from under the finger is worse with a
  stylus than with a finger: the stylus user is targeting a smaller point and has
  committed to it earlier in the gesture.
- **Phone vs tablet (reasoned).** On a phone the centre anchor was presumably
  chosen for thumb reach, and the reach argument is real. R04 answers it: the rail
  is top-anchored but the *groups within it* are placed so the most-used group is
  reachable. The fix is group placement, not a floating anchor.
- **Recommendation.** Make the persistent tier's anchor invariant to contextual
  height changes — the repository already contains the correct behaviour in its
  docked branch. Then, and only then, animate what legitimately still moves
  (UI-AUDIT2B's ordering, which this audit endorses). Do not animate the jump.

### 3.3 Move / Rotate / Scale selector

- **Current issue.** 48×48 dp targets, both states drawn, correctly absent in
  Scale. Two problems: 4 dp separation between four adjacent capsules (I-07), and
  the capsule is inside the centre-anchored column so selecting a mode moves the
  selector (I-14) — two identical taps at the same coordinate end on *Rotate*, not
  *Scale*. The Move glyph is also a near-duplicate of the rail's Transform glyph
  4 dp away (I-17).
- **Mobbin patterns seen.** R50 is the cleanest demonstration: a contextual action
  row changes above a **fixed** two-tab bar, and the tab bar does not move. R01,
  R03, R05 attach the contextual tier to the *selected object* so it moves with
  the object and never with the chrome. R04 separates its two rail groups with a
  visible gap, not a hairline. R49, R51, R52, R53 all draw active state as a
  filled peer among equals, matching ForgeShape.
- **Common principle.** *Contextual controls change; persistent controls hold
  still; and the two tiers are visibly separated.*
- **Verdict: ADOPT PRINCIPLE for the anchoring and the separation; REJECT the
  selection-attached placement.** ForgeShape's transform mode is not a property of
  a *selection region* the way a text style is — it is a persistent modal state
  that outlives selection, and attaching it to the body would put it under the
  user's own hand during a gizmo drag and would move it every time the camera
  orbits. Recorded as a deliberate divergence.
- **Recommendation.** Separate the four stacked capsules by more than a hairline
  so they read as distinct groups (R04's visible gap), and give the mode capsule
  a glyph that cannot be confused with the rail entry above it (I-17). Neither
  needs a new pattern; both are already ForgeShape's own vocabulary.

### 3.4 World / Local (coordinate-space selector)

- **Current state.** Both states drawn rather than a toggle, 48 dp each,
  correctly absent in Scale by hard rule. Prior verdict: PASS. The open question
  from UI-AUDIT2B (E-10) is that World↔Local reorients the gizmo in one frame, so
  the user cannot read which way Local points.
- **Mobbin patterns seen.** R49 (Craft `Select Cells`) is the closest analogue in
  the library: a **scope** choice — Cell / Column / Row — drawn as three equal
  labelled tiles, with **the affected region highlighted in the document behind
  the sheet** while the choice is being made. R54 carries scope-like state as
  check-marked rows inside the object menu (`✓ Constrain Proportions`). R21b and
  R63 show that a 3D reference communicates axis meaning by drawing on the object,
  not by labelling a control.
- **Common principle.** *A scope selector should show what it scopes.* Craft
  highlights the cells; the AR references draw the axes.
- **Verdict: ADOPT PRINCIPLE — and it independently supports E-10.** The
  reference's mechanism (highlight the affected region while choosing) maps onto
  ForgeShape as: when the space changes, the gizmo's reorientation is the
  explanation, so it must be *legible*, which a one-frame cut is not.
- **Touch/stylus.** Both states being drawn (rather than one toggle) is correct
  for touch and should be preserved: a toggle requires the user to infer the
  inactive state, which is a memory task mid-gesture.
- **Recommendation.** Keep the two-drawn-states control exactly as it is. Treat
  E-10 as corroborated: the space change needs to be readable on the instrument.

### 3.5 Precision toggle

- **Current issue.** P0/P1 territory, and not a pattern problem: the toggle is the
  last child of a weightless `LinearLayout` rail column, so it absorbs the whole
  height deficit — measured at 37.0 dp, 29.0 dp and **17.1 dp** in three
  configurations, and with the IME open it disappears entirely along with the
  space capsule while the mode capsule survives as a **2.3 dp sliver** (I-01,
  I-02).
- **Mobbin patterns seen.** R52 (Bears Gratitude) and R16 (Alta) both show the
  IME answer directly: the controls that must survive the keyboard are **pinned in
  a bar immediately above it** and move with it. R02 anchors its opening control
  in a bottom toolbar that the sheet grows out of, so the control's size never
  depends on a column's leftover space.
- **Common principle.** *A control's size is never the remainder of a
  subtraction.* Every reference sizes its controls first and lets the content area
  take what is left — the opposite of ForgeShape's current distribution.
- **Verdict: ADOPT PRINCIPLE.** This is a layout-contract fix, not a redesign, and
  the references only confirm what the measurements already prove.
- **Recommendation.** The interactive tier claims its floor first; the flexible
  content absorbs the deficit. Under the IME, the controls that must remain
  operable travel with the keyboard rather than being squeezed by it (R52, R16).

### 3.6 Undo / Redo (history capsule)

- **Current issue.** The state machine is verified correct. Two gaps: no
  confirmation of *what* was undone — Undo changes only the Objects capsule label
  from `Body #2` to `Body #1` (I-19) — and disabled controls still report
  `clickable="true"` (I-18).
- **Mobbin patterns seen.** **R32 (Wabi) is decisive**: every history entry is
  named by the act it performed — "Title changed to Meal planner recipe", "Cover
  image updated" — with a per-row revert. R17 (Play) shows the lighter form: a
  transient toast naming the result, "Text Field 1 is Hidden". R33, R34 name and
  group versions. R20 (Bevel) renders an inactive item **dimmed in place**, legibly
  inert rather than merely unresponsive. R59 and R29 keep undo/redo in a fixed
  capsule that never moves — which ForgeShape does too, except that the capsule
  itself rides the jumping column.
- **Common principle.** *An undo that does not name what it undid is not
  reversible in the user's head.* The act has a name at the moment it is recorded;
  the references simply keep it.
- **Verdict: ADAPT (R17's form), REJECT (R32/R33/R34's form).** ForgeShape must
  **not** grow a version list: its hard rule says there is one native Construction
  history, a step holds bounded Construction-domain state, and no second answer may
  exist. A browsable list of steps in the Java shell would be exactly that second
  answer. But the *naming* half transfers with no architectural cost — the
  transaction already knows what it was.
- **Recommendation.** When a Construction undo or redo completes, the status line
  reports the act by name. This is a **verdict**, not an instruction, so it
  satisfies ForgeShape's "chrome reports, it does not instruct" rule; and it needs
  no new state, only the name of the transaction that already exists. On I-18,
  R20 supports making the disabled state legible as well as inert.

### 3.7 Exact Transform (precision surface)

The densest comparison in the audit; four separate accepted findings, and the
library answers all four.

- **Current issues.** I-08: `-98765.4321098` renders as `765.4321098` — clipping
  at the **left**, sign invisible, no affordance. uiaudit1-E: tapping a field
  **appends** rather than replacing, so typing into a field showing `0` produced
  `0-98765.4321098`. I-09: the unit chips sit under **Scale**, which is unitless
  by hard rule, though they govern Position only. I-10: `Apply Transform` is three
  swipes below the fold in compact portrait, and the surface ends flush with no
  peek, fade or scroll indicator, so it reads as complete. I-15: a refusal is
  reported 570 dp away from the offending field.
- **Mobbin patterns seen.**
  - *Value legibility (I-08).* R12 sets the value very large and **right-aligned**
    with a dedicated clear control beside it. R28 (Telegram) right-aligns **signed**
    values in an accent colour (`+0.72`, `−0.59`) so the sign is the most protected
    glyph, not the first sacrificed. R14 quotes the previous value underneath while
    editing.
  - *Replace-on-focus (uiaudit1-E).* R12 gives the value a dedicated clear button
    rather than relying on caret placement; R12 and R13 both compose the value in a
    display region separate from the keypad, so there is no ambiguous insertion
    point at all.
  - *Unit placement (I-09).* R12 puts the unit segmented control **directly beside
    the value it governs**. R13 puts unit chips inline above the keypad with the
    composed value echoed above them. R15 puts the unit toggle on its own row
    immediately under the value. Three independent references, one placement rule,
    and it is the opposite of ForgeShape's current layout.
  - *Commit reachability (I-10).* R09 and R18 put `Done` in the **sheet header**.
    R31 puts `Apply` in the header of a full-screen editor. R16 pins `Cancel`/`Save`
    immediately above the keyboard. R12 keeps its commit affordances permanently
    visible. **Not one of the 65 inspected references places its commit action below
    a scroll fold.**
  - *Length management (I-10, structurally).* R07 (Play) is the strongest: the
    inspector is **segmented by concern with a tab bar** — Text / Layout / Position
    / Appearance / Interactions — so no category is ever long enough to need a fold.
    R11 does the same with three tabs. R08 fits a dense property group in one
    popover with no scroll at all.
  - *Error proximity (I-15).* R60 (Netflix) prints `Start 1:00` and `End 1:15`
    **beside the handles they belong to**. R62 marks the active vertex in place.
- **Common principles.** (a) The value is the largest and best-protected element,
  and its sign is part of the value. (b) The unit belongs with the value it
  governs, never with a neighbouring group. (c) The commit is always reachable
  without scrolling. (d) A long inspector is shortened by **categorisation**, not
  by scrolling. (e) Feedback appears where the thing it concerns is.
- **Verdicts.** (a) **ADOPT PRINCIPLE** — I-08 and uiaudit1-E. (b) **ADOPT
  PRINCIPLE** — I-09; note that ForgeShape's own code comment already knows the
  chips govern Position only, so this is a layout correction, not a decision.
  (c) **ADOPT PRINCIPLE** — I-10. (d) **ADAPT** — the tab bar itself is too much
  chrome for three property groups, but the underlying move (make each visible
  region short enough to need no fold) transfers directly; ForgeShape's three
  groups are Position, Rotation and Scale, which is already a natural
  categorisation. (e) **ADAPT** — see §3.11 on why the instructional half is
  rejected.
- **Touch/stylus.** R12's preset chips are a genuinely touch-native idea worth
  noting for the future: common values reachable without the keypad. For a CAD
  tool the analogue is a snap or a recall of the value the field had on open (R14),
  not an arbitrary preset list.
- **Phone vs tablet (reasoned).** ForgeShape's expanded layout already docks the
  inspector as a 320 dp column with `Apply` visible without scrolling — which is
  the state all these references achieve on a phone. The correction is to compact
  portrait, and the expanded layout is the target, exactly as UI-AUDIT2A concluded.
- **Recommendation.** Protect the value and its sign; move the unit to the group
  it governs; keep the commit reachable without a scroll; shorten each visible
  region rather than lengthening the scroll. Every one of these is already the
  behaviour of ForgeShape's own expanded layout.

### 3.8 Objects capsule / Add Primitive

- **Current state.** Prior audits' best-behaved surfaces: both popovers anchor to
  their invoking control and cause **zero** displacement of any other bound; Add
  Primitive's six tiles with silhouettes are the most legible control group in the
  app. Two open items: Add Primitive and the Shape editor use two different UIs for
  the same six primitives (I-11), and the body label is painted in a failing
  contrast role.
- **Mobbin patterns seen.** R02 (Craft) opens a list **anchored upward out of the
  control that was tapped**, with the current value check-marked — independent
  confirmation of `AnchoredSurfaceView`, including growth direction. R05 stacks a
  popover above a toolbar without either covering the other. R03 places a detail
  card and a contextual toolbar so neither occludes the other. R54, R55, R56, R57
  all group acts, mark current state with a check, and use `…` on acts that open
  further UI.
- **Common principle.** *A surface grows from the control that opened it, and
  never covers a live control.* This is precisely ForgeShape's existing rule, and
  the library confirms it without qualification.
- **Verdict: ADOPT PRINCIPLE (already held) — and this is where this audit
  contests UI-AUDIT2B's E-03.** E-03 proposes changing the precision sheet's pivot
  to the corner nearest the invoking toggle. The references support growth-from-the-
  invoker as a *principle* (R02, R05), but they also show the constraint E-03 does
  not weigh: in R02 and R05 the growing surface never crosses another live control
  on its way. ForgeShape's precision surface is a full-width bottom sheet whose
  opening also lifts the bottom row to avoid covering it. Changing its pivot
  changes the direction of that growth relative to controls it must not cover.
  Recorded as: **E-03's principle is confirmed; its specific pivot prescription is
  not, and should be re-measured against the non-occlusion rule before it is
  implemented.** Medium confidence.
- **Recommendation on I-11.** R54 and R57 both show one object menu carrying
  several registers (icon row, act list, state rows) rather than two different
  surfaces for one domain. The principle — *one domain concept, one control
  vocabulary* — supports I-11 as accepted. Which of the two vocabularies wins is a
  ForgeShape decision the library cannot make.

### 3.9 Sculpt Radius / Strength

- **Current state.** Prior audits called this the strongest single control group
  in the product: two vertical sliders, 48 dp touch width against a 10 dp drawn
  track, filled track showing value, label above value above track. One
  vocabulary problem: Radius reads **`120 px`** — a device-pixel unit in a
  metre-based product whose kernel resolves the brush in the world metric (I-16).
- **Mobbin patterns seen.** **R27** shows brush size as a **tapered wedge whose
  thickness at the thumb is the actual brush width** — no unit at all, because the
  control *is* the preview. R30 floats the value above the thumb, at the point of
  manipulation, with a `Reset` beside it. R26 paints the track with the value it
  controls. R28 marks the neutral point with a detent tick. R11 keeps a slider
  group short by tabbing it.
- **Common principle.** *A parameter with no natural unit should be shown, not
  numbered.* Every reference that could avoid printing a unit did.
- **Verdict: ADAPT.** ForgeShape cannot simply copy R27 — the brush radius *does*
  have a real unit here, because Stage 020R3 established that the kernel resolves
  the brush in the world metric, and a modeling tool has a legitimate reason to
  expose metres. But R27 reframes I-16 usefully: the fix is not "rename `px` to
  `m`", it is "decide what the number means and show the brush at that size on the
  model".
- **Touch/stylus.** R30's value-above-the-thumb is the right instinct for a
  finger, which occludes its own control; a stylus occludes less but still
  benefits. ForgeShape's label-above-value-above-track already achieves this
  without the transient.
- **Recommendation.** Resolve the vocabulary by resolving the meaning: state the
  radius in the metric the kernel actually uses, and let the on-model brush cursor
  be the primary size feedback (R27's principle), with the number as
  confirmation. Do not introduce a transient popup value; the existing static
  label is better.

### 3.10 Sculpt details

- **Current state.** A 157 dp surface carrying the mesh summary and
  `Reset Sculpt from Shape…`. Prior verdict: PASS, correct wording, correct scope.
- **Mobbin patterns seen.** R55 places destructive acts **last and in red**, with
  `…` on acts that open further UI. R36 guards a restore behind an explicit confirm
  dialog naming the object. R18 carries state in the row itself (`White Smoke -
  74%`) rather than in a separate readout.
- **Common principle.** *A destructive act is positionally and chromatically
  distinct, and is confirmed.*
- **Verdict: ADOPT PRINCIPLE (already held).** ForgeShape's `Reset Sculpt from
  Shape…` already carries the ellipsis convention and already has a confirmation
  step (`stage015b_refreeze_confirmation.png`, `stage016_16`). The library confirms
  the existing design; no change recommended.
- **Note.** R55's red-destructive convention is *not* recommended for adoption
  wholesale — see §6, item 7, on ForgeShape's error colour role currently failing
  contrast at 2.96:1.

### 3.11 Status line, and the one principle this audit refuses

- **Current issues.** I-15: a refusal appears 570 dp from the offending field, and
  the field's border is identical to its valid siblings. I-19: undo reports
  nothing.
- **Mobbin patterns seen.** Two distinct kinds, and they must not be conflated.
  **Verdicts:** R17 "Text Field 1 is Hidden" (a result, after the fact); R60
  `Start 1:00` / `End 1:15` at the handles (a current value); R62's corner card
  `550 ft²` (a computed result); R59's `100%` zoom readout. **Instructions:** R61
  "Drag to resize the clip"; R62 "Drag to edit the current point"; R59 "Tap or hold
  to trim".
- **Common principle among the verdicts.** *Report where the thing is.*
- **Verdict: ADOPT PRINCIPLE for verdicts; REJECT the instructional half
  outright.** ForgeShape's hard rule is explicit: chrome reports, it does not
  instruct, and a message that is always true is a permanent surface whatever its
  timeout says. R61 and R62's coach chips are always true, and would be permanent
  surfaces over the viewport. That several polished products ship them is exactly
  the case where popularity is not correctness.
- **Recommendation.** For I-15, the principle transfers as: mark the offending
  field itself, so the refusal is locatable without reading a distant line. For
  I-19, §3.6.

### 3.12 Gizmo — what Mobbin cannot adjudicate

- **Current issue.** Every handle is a hollow 1 px outline because `wideLines` is
  not requested; one shape vocabulary does four jobs; the effective hit radius is
  15 dp against ForgeShape's own 48 dp floor; six to seven targets sit inside a
  ~120 dp circle; depth test and write are off, so a handle behind the body looks
  identical to one in front (I-04).
- **What the library actually shows.** **No reference in the set contains a 3D
  transform gizmo.** The AR references solve the problem by avoiding it: R21b and
  R23-R25 use direct manipulation with a bounding box and no per-axis handles at
  all; R22 substitutes an on-screen D-pad plus rotate arrows. R21 and R63 label the
  object's extents rather than its manipulators.
- **Honest conclusion.** **Mobbin cannot adjudicate I-04.** The consumer AR corpus
  has no per-axis constrained manipulation because consumer AR does not need it;
  ForgeShape does. The touch-target and shape-vocabulary evidence from UI-AUDIT1
  and UI-AUDIT2A stands entirely on its own, and this audit neither strengthens nor
  weakens it. Saying so is more useful than manufacturing a reference.
- **The one thing that does transfer.** R21b separates **selection** (a wireframe
  bounding box, non-interactive, showing what is selected) from **manipulation**
  (the contextual toolbar). ForgeShape currently makes the gizmo carry both jobs.
  **ADAPT, low priority:** a selection affordance distinct from the manipulation
  instrument would let the gizmo stop doing two things at once — but this is a
  renderer change well outside any current correction scope, and it is recorded
  here as an observation, not a recommendation.
- **R22 is rejected** — see §6, item 1.

---

## 4. Cross-reference matrix

Confidence is this audit's confidence in the *pattern-level claim*, not in the
underlying ForgeShape measurement (which the prior audits own).

| ForgeShape finding | supporting Mobbin refs | contradicting refs | confidence |
| --- | --- | --- | --- |
| **C / I-14 / E-02** — centre-anchored column displaces the control just tapped | R04 (top-anchored twin), R01, R05, R06, R50, R53, R59 (persistent tier edge-pinned); R51 (overflow by scrolling, not shrinking); 0 of 65 references centre-anchor a persistent cluster | none | **High** — the strongest convergence in the audit; independently reached by three prior audits and unopposed in 65 references |
| **I-10** — `Apply Transform` below the fold | R09, R18 (Done in header), R31 (Apply in header), R16 (pinned above IME), R12 (commit always visible), R07/R11 (categorise instead of scroll) | none | **High** — not one inspected reference puts its commit below a fold |
| **I-09** — unit chips under Scale | R12, R13, R15 (three independent instances of unit-adjacent-to-value) | none | **High** — and ForgeShape's own code comment already agrees |
| **I-08 / uiaudit1-E** — sign clipped at left; field appends | R12 (large right-aligned value + dedicated clear), R28 (signed values right-aligned in accent), R14 (prior value quoted) | none | **High** |
| **I-19** — undo reports nothing | R32 (acts named), R17 (result toast), R33/R34 (named versions) | none for the naming; **R32/R33/R34 contradict any browsable-list form**, which ForgeShape's one-history rule also forbids | **High** for naming; the *list* form is rejected |
| **I-01 / I-02** — precision toggle shrinks to 17.1 dp; IME destroys controls | R52, R16 (controls travel with the IME), R02 (control sized independently of leftover space) | none | **High** — though this is a layout-contract bug the references only illustrate |
| **I-16** — brush Radius reads `120 px` | R27 (size shown, not numbered), R30, R26 | R11, R28 (do print bare numbers) — but for unitless parameters, which brush radius is not | **Medium** — the references reframe the problem rather than settling it |
| **I-15** — refusal 570 dp from the field | R60 (values at their handles), R62 (active vertex marked), R17 (result named) | R59, R61 put status text in a fixed strip far from the object | **Medium** — the library is genuinely split |
| **I-07** — four capsules 4 dp apart | R04 (visible gap between rail groups), R50 (tiers visibly separated) | none | **Medium** — supported, but the specific spacing is an Android-guideline matter Mobbin cannot speak to |
| **I-11** — two vocabularies for six primitives | R54, R57 (one menu, several registers, one vocabulary) | none | **Medium** |
| **I-18** — disabled controls report clickable | R20 (inactive item dimmed in place) | none | **Low** — an accessibility-tree defect; the reference speaks only to its visual half |
| **I-13** — `editing_context_label` absent in compact portrait | R23, R24 (mode indicator shown even on bare 3D views) | ForgeShape's own rule: the rail entry and transition button already answer it | **Low — contested.** Severity not raised |
| **E-03** — precision sheet pivot | R02, R05 (growth from the invoker) | R02, R05 also never cross a live control while growing; ForgeShape's sheet must not cover the bottom row | **Medium — principle confirmed, prescription not.** Re-measure before implementing |
| **E-10** — World↔Local reorients in one frame | R49 (affected region highlighted while choosing), R21b/R63 (axis meaning drawn on the object) | none | **Medium** |
| **I-04** — gizmo hit radius, shape vocabulary, no depth cue | **none — the library contains no 3D gizmo** | R22 (offers a D-pad instead — rejected, §6) | **N/A — Mobbin cannot adjudicate.** Prior audits' evidence stands alone |
| **I-03** — System Back exits the app | none — iOS-only corpus has no Back | none | **N/A — out of Mobbin's reach** |
| **I-05** — contrast failures (3.93:1, 2.96:1) | none — contrast is not assessable from curated captures | none | **N/A** |
| Objects capsule / Add Primitive anchoring | R02, R05, R03 (grow from invoker, never occlude) | none | **High — confirms existing behaviour**, no change |
| Start chooser | F02 (three equal tiles in an anchored sheet), R49 | none | **High — confirms existing behaviour** |
| Start Sculpting / Back / Resume transitions | R50 (persistent tier survives a contextual swap) | none | **Medium — confirms**, and supports E-01's case for motion |

---

## 5. Future-only inspiration

Roadmap material. **Not current correction scope**, and nothing here is proposed
for the next stage.

### 5.1 Settings Hub

- **R43 (GitHub) is the idea worth keeping.** A live preview of the thing being
  configured sits at the top of the settings screen, above the groups whose toggles
  change it. For a 3D tool this is unusually apt: a Settings Hub could show a live
  viewport with the current shading, grid and appearance applied, rather than
  describing them in words. ForgeShape's Display popover already gestures at this
  by staying open across selections — the natural extension is showing the effect.
- **R44, R46** — every setting carries a one-line plain-language explanation.
  ForgeShape has settings that genuinely need one (`Debug` shading, orthographic
  versus perspective, matcap).
- **R45, R48** — a row's current value is its subtitle, so the state is readable
  without opening the row.
- **R47, R46** — group by concern, not by control type.
- **Caution.** ForgeShape's rule that no surface owns the resting workspace means a
  Settings Hub is a destination reached from a control, not a permanent panel. All
  six references are full-screen destinations, which is compatible.

### 5.2 Project / App Hub

- **R38 (Riveo)** — `New Project` as a **dashed-outline tile in the first grid
  cell**, a peer of the projects rather than a separate control. The empty state
  and the populated state become the same layout, which removes an entire screen.
- **R41 (Canva)** — the item subtitle names the document's *type and dimensions*
  (`40 × 40 px`). ForgeShape's analogue is the primitive kind and its dimensions,
  which the Construction Source already knows exactly.
- **R37, R40** — per-item metadata a creator actually uses: age, size, duration.
- **R39** — creation promoted above browsing as a full-width primary action.
- **F01 (Play flow)** — the empty editor is the *same layout* as the full one, so
  a new project is not a different screen. This matches ForgeShape's existing
  behaviour, where a new session seeds a default body into the same workspace.
- **F02 (Unfold flow)** — `Choose a Starting Point` as three large equal tiles in
  an anchored sheet: independent confirmation that ForgeShape's two-option start
  chooser is the conventional shape and should not be redesigned.

### 5.3 CAD sketch / dimension UI

The most valuable future material in this audit.

- **R62 (Redfin)** — dimension labels drawn **on the edges they measure**, the
  active vertex marked, and the measured region named and quantified in a corner
  card (`550 ft²`). This is what a touch CAD tool looks like when it exposes exact
  numbers without opening a panel.
- **R63 (Target)** — **one dimension pill per axis**, which maps directly onto a
  box primitive's three parameters, a cylinder's radius and height, and so on.
- **R21 / R21b (Best Buy)** — crucially, the dimension overlay is **toggleable**:
  the same screen exists with and without it. That is the correct architecture for
  ForgeShape, where the viewport is the workspace and permanent overlay chrome
  would violate it.
- **R60 (Netflix)** — a value printed beside the handle that changes it. Applied
  to a gizmo drag, this is the strongest single future idea for Exact Transform:
  the user could read the value they are producing without the precision surface
  being open at all.
- **R54 (Freeform)** — `✓ Constrain Proportions` is literally a CAD constraint
  expressed as a check-marked row in an object menu. When ForgeShape acquires
  constraints, this is the touch vocabulary for them.
- **Hard-rule caution.** All of this is renderer work — in-scene labels are drawn
  by the Vulkan renderer, not by the Java shell — and none of it may read a
  Construction parameter back out of a render product. A dimension overlay must
  read the Construction Source directly, which it can, since the Source is exact
  and authoritative.

### 5.4 Tablet / desktop adaptation

Stated with its limitation: **the Mobbin tool exposes no tablet platform, so this
subsection is reasoned, not evidenced.**

- The pattern the phone references keep reaching for under space pressure —
  categorising an inspector (R07, R11), pinning commits to a header (R09, R18,
  R31) — is what ForgeShape's expanded layout *already does* by docking a 320 dp
  inspector column with `Apply` visible. The tablet target is not a new design; it
  is the design the phone layout has not caught up to.
- **R10 (Pinterest)** — a layer strip along the edge is a plausible tablet-only
  affordance for a future multi-body scene, using width ForgeShape's compact
  layout does not have.
- **I-21** (expanded strands the history capsule at y=2416 px while all other
  chrome is above y=944 px) is not contradicted by anything in the library, but
  neither is it supported: no reference had the width to show the problem.

---

## 6. Anti-copy / rejected patterns

Attractive, well-executed, and wrong for ForgeShape. Popularity is not
correctness.

1. **R22 — Target's on-screen D-pad and rotate arrows.** A permanent nudge cluster
   over the viewport that duplicates direct manipulation. It exists because
   consumer AR has no precise manipulator; ForgeShape has a gizmo and exact-value
   editors. It would add chrome over the workspace, take a large permanent bite of
   the viewport, and give the user a third answer for placement beside the gizmo
   and the precision surface. **Rejected: adds chrome, conflicts with the existing
   interaction model.** (A *nudge* affordance on an exact-value row is a different
   and defensible idea; the on-viewport D-pad is not.)
2. **R12's arithmetic operators (`÷ × − + =`).** The rest of R12 is the audit's
   best reference, but a calculator embedded in a modeling tool is chrome for a
   problem ForgeShape does not have. **Rejected: too much chrome; take the value
   presentation and the unit placement, leave the calculator.**
3. **R17 / R10 — the layer tree.** ForgeShape has one active Construction Body.
   A tree is chrome for a scene graph that does not exist yet, and building the UI
   before the domain would create a surface that must lie about what it contains.
   **Rejected for now**; revisit when multi-body genuinely lands.
4. **R32 / R33 / R34 — the browsable version list.** The naming idea is adopted
   (§3.6); the list is not. ForgeShape's hard rule allows exactly one Construction
   history, native, holding bounded Construction-domain state. A Java-side
   browsable step list would become a second answer to undo depth — the precise
   thing the rule forbids. **Rejected: conflicts with a hard architectural rule.**
5. **R61 / R62 / R59 — instructional coach chips** ("Drag to resize the clip",
   "Drag to edit the current point", "Tap or hold to trim"). Always true, therefore
   permanent surfaces over the viewport whatever their timeout says. **Rejected:
   ForgeShape's chrome reports, it does not instruct.** The verdict half of these
   same references is adopted.
6. **R06 / R51 / R53 — the permanent labelled bottom tool bar.** R06 gives roughly
   a third of the screen to a permanently visible tool row. ForgeShape's rule that
   no surface owns the resting workspace forbids this directly, and on a 3D tool
   the viewport is the product. **Rejected: obscures the viewport.**
7. **R55 / R58's red destructive convention, adopted naively.** The *placement*
   convention (destructive last, visually distinct) is sound and ForgeShape already
   follows it. Adopting the red **colour** as-is would compound I-05, where
   `p1_text_error` measures 2.96:1 against AA's 4.5:1. **Rejected as a colour
   decision** until the error role passes contrast; the placement stands.
8. **R29 — the tick-ruler scrubber, applied to Scale.** Genuinely excellent touch
   numeric input, and defensible for Rotation, which is bounded and cyclic with a
   meaningful zero. It is wrong for **Scale**: a ruler with a centre detent implies
   a bounded range and a neutral origin, but ForgeShape's Scale is a strictly
   positive unitless multiplier where zero is singular and negative is a Mirror the
   product does not have — both refused, never clamped. A control that draws a
   continuum through a refused value teaches the wrong model. **Rejected for Scale;
   possible for Rotation only.**
9. **Freeform's and Apple Notes' translucent materials, and the whole visual
   register of R01/R03/R05.** Beautiful, and pure platform style. ForgeShape has a
   deliberate three-appearance semantic token system and correctly refuses Material
   and Cupertino chrome alike. **Rejected: branding/style, not interaction
   improvement.**
10. **R09 / R18's `Done`-in-header, copied literally as a sheet header bar.** The
    *principle* (commit always reachable) is adopted at high confidence. The
    literal form — adding a persistent header bar to every ForgeShape surface —
    would add a permanent strip to surfaces that currently grow cleanly out of
    their invoking control. **Adopt the principle, reject the chrome.**
11. **R07's five-tab inspector bar, copied literally.** Five tabs for ForgeShape's
    three property groups would be more chrome than the content. **Adopt the
    categorisation principle, reject the tab bar.**

---

## 7. Implications for the current correction scope

This audit **raises no new P0 or P1**. It is a comparative pass; it did not touch
a device and cannot measure anything. What it does is change the *confidence* of
findings the other three audits already own.

**Strengthened to the point that they should be treated as settled:**

| finding | why this audit strengthens it |
| --- | --- |
| **C / I-14 / E-02** (chrome jump) | Zero of 65 references centre-anchor a persistent tool cluster on a variable-height container; the closest structural twin (R04) is top-anchored. Three prior audits, three methods, one conclusion, and now an unopposed external corpus |
| **I-10** (Apply below fold) | Not one inspected reference places its commit below a scroll fold, across six independent apps |
| **I-09** (unit chips under Scale) | Three independent references place the unit with the value it governs; ForgeShape's own code comment already agrees |
| **I-08 / uiaudit1-E** (sign clipping, append-on-focus) | R12 and R28 show both halves solved as a matter of course |

**Reframed rather than confirmed:** I-16 — the fix is to decide what the brush
radius *means* and show the brush at that size, not to rename the unit.

**Contested, and flagged for the coordinator rather than acted on:** UI-AUDIT2B's
E-03 pivot prescription (principle confirmed, prescription unverified against the
non-occlusion rule) and UI-AUDIT2A's I-13 (references split; severity unchanged).

**Explicitly outside Mobbin's reach, where the prior audits' evidence stands
alone and must not be diluted:** I-04 (gizmo — the library has no 3D gizmo at
all), I-03 (System Back — iOS-only corpus), I-05 (contrast — not assessable from
curated captures), I-02's Android IME behaviour.

---

## 8. Git scope

- Product HEAD: `bd3f7d5` (Stage 020R3), unchanged.
- Files added: `artifacts/uiaudit2d/README.md`,
  `artifacts/uiaudit2d/MOBBIN-INVOCATION-LOG.md`,
  `artifacts/uiaudit2d/REFERENCE-CATALOGUE.md`.
- Files modified outside `artifacts/uiaudit2d/`: **none.**
- One evidence-only commit. Final tree clean.

No screenshots of Mobbin references are stored in this repository. The references
are cited by their canonical `mobbin.com` URLs and screen ids so any reader can
open the original; redistributing a commercial pattern library's captures into a
project repository would be inappropriate regardless of the audit's needs.

---

## Next step

**Return all four independent audit reports — UI-AUDIT2A (Impeccable),
UI-AUDIT2B (Emil Kowalski motion), UI-AUDIT2C (Remotion prototyping) and
UI-AUDIT2D (Mobbin comparative) — to the owner/coordinator for synthesis.**
