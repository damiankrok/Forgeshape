# UI-AUDIT2A — Impeccable-only native UI audit (independent pass)

**Method: single-context, Impeccable only.** No Emil Kowalski lens, no Remotion
lens, no second opinion, by instruction. Declared here rather than left silent.

Read-only. **No product source, resource, shader or build file was changed.**
Product HEAD for this audit is `bd3f7d5` (Stage 020R3). `git diff --stat bd3f7d5
HEAD` at the start of the run showed **only** `artifacts/uiaudit1/**` — 23 files,
all evidence. There is no product delta from `bd3f7d5`, confirmed before any
evidence was taken.

The hard gate — which Impeccable files were read, their SHA-256 digests, the
sections consulted, the rules taken from them and the commands used — is
[`SKILL-PROOF.md`](SKILL-PROOF.md) in this directory. Impeccable **v4.1.1**,
routed `audit` → `reference/audit.native.md` + `reference/android.md`, with
`reference/layout.md`, `reference/adapt.native.md` and `reference/critique.md`
for composition, adaptation and cognitive load.

---

## Runtime provenance

| fact | value |
| --- | --- |
| device | `ForgeShape_Stage006`, confirmed by `adb -s emulator-5580 emu avd name` — **never** by port |
| serial | `emulator-5580`, started only by `scripts\start-forgeshape-emulator.ps1` |
| build | `gradlew.bat :app:assembleDebug`, installed with `adb -s emulator-5580 install -r` |
| launch health | **13/13** `*_SELFTEST_OK` then `FORGESHAPE_NATIVE_VIEWPORT_OK`, log ring at 16M — [`R00`](R00-selftest-transcript.txt) |
| foreground check | `topResumedActivity=com.forgeshape.app/.ForgeShapeActivity` confirmed before every capture |
| compact portrait | 1080×2400 @ 420 dpi = **2.625 px/dp** |
| short landscape | 2400×1080 @ 420 dpi = 2.625 px/dp |
| expanded | 1600×2560 @ 240 dpi = **1.5 px/dp**, taken after a `force-stop` relaunch so the density override reached the process |
| large text | `settings put system font_scale 1.3`, restored to `1.0` |
| device restored | `wm size reset`, `wm density reset`, `font_scale 1.0`, `accelerometer_rotation 1` |

Every control was located and driven from **its own runtime bounds resolved by
semantic id** (`uiautomator dump` → `resource-id="com.forgeshape.app:id/…"` →
centre of `bounds`). No screen coordinate appears in the driving harness except
in two deliberate control experiments, which say so.

Hardware caveat, per `android.md`: this is an emulator. It gives breadth for
layout, bounds, insets, text scaling and contrast. It does **not** give truth
about frame pacing, gesture feel or thermal behaviour, so the Performance
dimension below is scored on structure and launch evidence only and says so.

### One measurement was wrong and was corrected

My first bounds resolver matched the id substring on a line whose first
`bounds=` belonged to an ancestor, so a tap intended for the Display control
landed at the screen centre and dismissed the start chooser. That produced a
false "modal scrim leaks touches" reading. Re-tested with a strict
resource-id→bounds pairing and with raw-coordinate control taps: the scrim
**correctly swallows every touch**, including taps directly over the toolbar's
own controls. It is recorded below as a **positive** finding, and the misnamed
artifact was deleted rather than kept.

---

## Audit Health Score

| # | Dimension | Score | Key finding |
| --- | --- | --- | --- |
| 1 | Accessibility | **1** | The 48 dp floor is broken at runtime in four measured configurations, the worst being a **17.1 dp** precision toggle at `font_scale 1.3`; the secondary and error text roles fail WCAG AA in all three appearances |
| 2 | Performance | **3** | Clean launch, 13/13 self-tests, no list surfaces at all; not frame-profiled on hardware, so scored on structure only |
| 3 | Appearance & Theming | **3** | A complete three-appearance semantic token system with exactly one raw hex (a deliberate "missing role" sentinel) — but `p1_text_secondary` measures **3.93:1** and `p1_text_error` **2.96:1** |
| 4 | Platform Conformance | **2** | System **Back exits the app** with a context surface open; the gizmo reads as debug wireframe rather than a modeling instrument |
| 5 | Adaptivity | **2** | Three window classes genuinely restructure — and the IME crushes the transform selector to **2.3 dp** and deletes two live controls |
| **Total** | | **11/20** | **Acceptable — significant work needed** |

### Platform Conformance Verdict — start here

**Conditional pass, with one hard fail.**

This does not read as a ported website. It reads as a native, GPU-owned creative
tool: the Vulkan `SurfaceView` is genuinely full-bleed, window insets are applied
to chrome only, edge-to-edge is correct, text sizes come from `sp` dimens and
really do scale, and every control carries a stable semantic id and a content
description. There are no HTML-shaped controls, no hover affordances and no
scroll-jacking. `detect.mjs` is N/A here and it would have found nothing anyway.

The hard fail is **System Back**. With Add Primitive open, one Back press
returned the launcher — measured, `topResumedActivity=NexusLauncherActivity`,
artifact [`F03`](F03-after-system-back.png). `android.md` states the rule
plainly: *System Back always works … never trap or hijack the user*. A surface
that is open and dismissible by its own control must consume Back first. The
process survives and the workspace is restored on return, which is why this is
P1 and not P0 — but on a modeling tool, an accidental Back is the difference
between "closed a palette" and "left the app".

The deliberate divergences are **accepted, not scored against**: no Material
components, no navigation bar, no FAB, a hand-authored vector icon set, and a
custom three-appearance palette instead of Material color roles and Dynamic
Color. `android.md` allows brand to express through theming; a professional 3D
modeling workspace that wore Material chrome would be a worse product, and the
repository's own rules forbid the dependency. What `android.md` does *not*
excuse — the 48 dp floor, the 8 dp separation, Back, and contrast — is exactly
where the findings are.

---

## Executive summary

- **Audit Health Score: 11/20 (Acceptable).**
- **21 issues**: 1 P0, 6 P1, 8 P2, 6 P3. **8 positive findings.** **6 candidate
  findings tested and rejected**, one of them a previous audit's accepted finding.
- **One root cause produces the two most serious issues.** `railColumn`
  (`EditorWorkspaceView.java:455-600`) is a vertical `LinearLayout` whose four
  children are all `WRAP_CONTENT` with no weight and no minimum height. Whenever
  the column's available height shrinks — the precision surface opening, the IME
  opening, a short landscape window, a larger system font — `LinearLayout` takes
  the whole deficit out of the **last** children. The precision toggle measured
  37.0 dp, 29.0 dp and 17.1 dp in three different configurations, and with the
  IME up the transform-mode capsule was left as a **2.3 dp sliver** while the
  space capsule and the toggle disappeared entirely.
- **The gizmo is the product's centrepiece and its least finished surface.** Every
  handle — axis, plane, pivot marker, scale cube — is a hollow 1 px outline,
  because `wideLines` is not requested. One shape vocabulary is doing four jobs,
  the effective hit radius is 15 dp against a 24 dp floor, six to seven targets
  sit inside a ~120 dp circle, and depth test and write are off so a handle
  behind the body looks identical to one in front.
- **The three most valuable non-gizmo fixes are cheap.** A minimum height on the
  rail column's trailing children; `setSelectAllOnFocus(true)` plus an
  end-anchored ellipsis on the numeric field; one `onBackPressed` branch.
- **The expanded window class is already right.** At 1067×1707 dp the Objects
  capsule becomes a docked panel, the inspector docks as a 320 dp column, Apply is
  visible without scrolling, and the rail does not move vertically at all. The
  correct behaviour already exists in this repository; compact portrait is what
  has not caught up.

---

## Artifact inventory

### Screenshots

| id | artifact | shows |
| --- | --- | --- |
| A00 | `A00-start-chooser.png` | start chooser over scrimmed workspace, compact portrait |
| A01 | `A01-construction-resting.png` | Construction resting, baseline bounds |
| A02 | `A02-transform-move.png` | Transform / Move / World, gizmo + selector row |
| A03 | `A03-transform-rotate.png` | Rotate mode |
| A04 | `A04-transform-rotate-local.png` | Rotate, Local space |
| A05 | `A05-transform-scale.png` | Scale mode, space capsule correctly absent |
| A06 | `A06-double-tap-displacement.png` | end state of the two-identical-taps experiment |
| A07 | `A07-exact-transform-open.png` | precision surface open; 37 dp toggle; whole chrome |
| A08 | `A08-exact-transform-scrolled.png` | Scale row and unit chips after one swipe |
| A09 | `A09-exact-transform-bottom.png` | Apply Transform reached, three swipes in |
| A10 | `A10-after-precision-close.png` | exact bound restoration on close |
| A11 | `A11-add-primitive.png` | Add Primitive palette, zero displacement |
| A12 | `A12-objects-popover.png` | Objects popover, zero displacement |
| A13 | `A13-display-popover.png` | Display settings, five option groups |
| A14 | `A14-appearance-light-charcoal.png` | light-charcoal appearance, gizmo over light geometry |
| A14b | `A14b-crop-rail-column-2x.png` | 2× crop: rail concentricity, icon-only mode capsule |
| A15 | `A15-gizmo-move-world.png` | Move gizmo, default framing |
| A15b | `A15b-crop-gizmo-move-3x.png` | 3× crop: hollow shaft, open arrowhead |
| A16 | `A16-gizmo-rotate-world.png` | Rotate gizmo |
| A16b | `A16b-crop-gizmo-rotate-3x.png` | 3× crop: three full rings, six crossings, no pivot |
| A17 | `A17-gizmo-rotate-local.png` | Rotate, Local |
| A18 | `A18-gizmo-scale-local.png` | Scale gizmo |
| A18b | `A18b-crop-gizmo-scale-3x.png` | 3× crop: black hollow uniform cube on the pivot |
| A19 | `A19-sculpt-resting.png` | Sculpt resting: brush column, 4-tool rail, no history |
| A20 | `A20-sculpt-details.png` | Sculpt details surface |
| A21 | `A21-back-to-construction.png` | Back to Construction, Resume Sculpt appears |
| A22 | `A22-after-add-sphere.png` | after Add Primitive → Sphere, Undo enabled |
| A23 | `A23-after-undo.png` | after Undo, Redo enabled, Body #1 restored |
| B01 | `B01-landscape-construction.png` | short landscape Construction |
| B02 | `B02-landscape-sculpt.png` | short landscape Sculpt, 29 dp toggle |
| B03 | `B03-landscape-sculpt-details.png` | landscape details docked right |
| B04 | `B04-landscape-exact-transform.png` | landscape Exact Transform docked |
| C01 | `C01-expanded-start-chooser.png` | expanded start chooser |
| C02 | `C02-expanded-construction.png` | expanded Construction, Objects **docked** |
| C03 | `C03-expanded-transform.png` | expanded Transform, rail does not move vertically |
| C04 | `C04-expanded-exact-transform.png` | expanded Exact Transform, Apply visible |
| C05 | `C05-expanded-landscape.png` | expanded landscape 1707×1067 dp |
| D01 | `D01-fontscale-chooser.png` | chooser at font_scale 1.3 |
| D02 | `D02-fontscale-construction.png` | Construction at font_scale 1.3 |
| D03 | `D03-fontscale-exact-transform.png` | **17.1 dp** precision toggle |
| D04 | `D04-crop-precision-toggle-fontscale13.png` | 3× crop of the collapsed toggle |
| E01 | `E01-long-negative-value.png` | left-clipped negative value, IME open, 2.3 dp selector |
| F01 | `F01-shape-editor.png` | Shape editor: 3-column text chips |
| F02 | `F02-hide-ui.png` | Hide UI: one 51.4 dp restore chip, nothing else |
| F03 | `F03-after-system-back.png` | launcher, after one Back with a surface open |
| F04 | `F04-return-after-back.png` | workspace on return |
| G01 | `G01-scale-zero-refusal.png` | scale-zero rejection: message top, field bottom, no field state |
| H01 | `H01-precision-roundtrip.png` | 15 significant digits surviving Apply and redisplay |
| I01 | `I01-display-popover-warm.png` | display popover after an appearance change |

### Measurement records

| id | artifact | contents |
| --- | --- | --- |
| R00 | `R00-selftest-transcript.txt` | 13 `*_SELFTEST_OK` + `NATIVE_VIEWPORT_OK` |
| R01 | `R01-compact-portrait-bounds.txt` | every control's bounds, px and dp, for 13 compact-portrait states |
| R02 | `R02-gizmo-handles.txt` | runtime handle positions for move/world, rotate/world, rotate/local, scale/local |
| R03 | `R03-short-landscape-bounds.txt` | four short-landscape states |
| R04 | `R04-expanded-bounds.txt` | five expanded states, portrait and landscape |
| R05 | `R05-fontscale-bounds.txt` | three states at `font_scale 1.3` |
| R06 | `R06-numeric-field-evidence.txt` | append-on-focus and the IME chrome inventory |
| R07 | `R07-misc-surfaces.txt` | Hide UI, Back, display popover persistence |
| R08 | `R08-error-states.txt` | scale-zero refusal |
| R09 | `R09-precision-roundtrip.txt` | 15-digit round trip |

---

## Per-layout composition review

### Compact portrait (411 × 914 dp)

**Squint test.** Blur the detail and exactly one thing survives: the saturated
blue mode-transition pill in the top-left corner. The model — the product — is
mid-value tan on mid-value charcoal and reads *second*. The chrome is otherwise
a uniform family of translucent charcoal capsules, which is the right decision,
but it means the single high-chroma element in the composition is a mode switch,
not the work. Primary reads as secondary.

**Structure.** Four regions: a transparent toolbar row carrying two floating
groups, a right-edge vertical stack, a bottom row with two capsules, and the
viewport as the field between them. The idea is right and the execution of the
outer frame is genuinely good — nothing is a slab, nothing is anchored to a
window edge as a permanent panel, and the corners are concentric where they
should be (measured: rail capsule outer 26 dp, entry inner 20 dp, 6 dp padding —
`inner = outer − gap` holds exactly).

**Rhythm — the weakest part.** In Transform, the right edge carries four separate
capsules — rail (142 dp), mode (161 dp), space (109 dp), toggle (48 dp) —
separated by an identical **4 dp** (`row_gap_small`). Measured tops: 591 / 975 /
1408 / 1715 px. Four different semantic ranks at one interval means grouping
carries no information; the eye reads one 500 dp column broken by seams, which is
the "one spacing value repeated until everything has equal weight" failure
`layout.md` names. The column is also **62% of the usable height** on the side
where a right-handed thumb rests.

**Density.** Eight stacked controls in that column, plus up to seven gizmo
handles on the model, is 15 simultaneous targets at one decision point. The
Working Memory Rule puts 8+ in the overload band.

**Safe area.** Correct. The `SurfaceView` occupies the full 1080×2400 window; the
chrome starts at y=128 px, exactly under the status bar; the bottom row ends at
2316 px, clear of the gesture pill. No inset is applied to the render target.

### Short landscape (914 × 411 dp)

The workspace genuinely restructures rather than rotating: the selector row goes
horizontal, rail entries drop from 56×64 to 56×48 dp while keeping their icon and
caption, and `editing_context_label` appears. That is real adaptive work.

It is also where the column squeeze first became visible: the precision toggle
measures **48.0 × 29.0 dp** in Sculpt. In landscape the docked inspector takes
the right 300 dp and the rail slides 312 dp left to stay clear — justified
reflow, correctly distinguished from the accidental movement in **I-14**.

### Expanded (1067 × 1707 dp portrait, 1707 × 1067 dp landscape)

The best window class, and by some distance. The Objects capsule becomes a
**docked panel** at top-left, the rail sits at top-right with its top edge on the
same 233 px line — a real alignment, not an accident. The inspector docks as a
320 dp column and every row including Apply is visible with no scrolling. When it
opens, the rail moves horizontally (1486→988 px) and **does not move vertically
at all** (233 px before and after), because the expanded layout uses
`Gravity.TOP`. That is exactly the behaviour compact portrait lacks.

The one adaptive weakness: Undo/Redo stays pinned to the far bottom-right at
y=2416 px while every other control is in the top 900 px — about 1560 dp of
travel between the tool you are holding and the history you need. On a tablet the
history capsule should ride with the cluster it serves.

### Font scale 1.3

Text scales correctly everywhere — sizes come from `sp` dimens through
`getDimensionPixelSize`, so `Export` widened 60.6→74.3 dp and the Objects capsule
160.4→177.1 dp, both without clipping. Nothing truncated. But the same growth
took the precision toggle to **17.1 dp**, and its active fill is clipped to a
square inside its own pill ([`D04`](D04-crop-precision-toggle-fontscale13.png)) —
a non-text control losing 64% of its height because text elsewhere grew.

---

## Per-surface review

**Start chooser** — Two 315×108 dp options, a 440 dp-capped panel that is itself
the scroller, and a scrim that **provably swallows every touch**: a raw-coordinate
tap directly over the Display control neither opened the popover nor dismissed
the question. Correct modality. The panel is vertically centred on a 914 dp
window, leaving both options in the comfortable middle. **PASS.**

**Global Toolbar** — Two floating groups, not a bar. `Export` is drawn recessed
with the content description "Export is not implemented yet." — the one approved
unimplemented control, honestly labelled. The transition button carries its full
wording (`Back to Construction` at 150.1 dp, `Resume Sculpt` at 122.7 dp,
`Start Sculpting` at 121.1 dp) in every window measured, never an ellipsis. The
`editing_context_label` is present in landscape and expanded, absent in compact
portrait (**I-13**).

**Tool Rail** — Icon **and** caption per entry, 56×64 dp compact, 56×48 dp short.
Concentric corners verified. The scroll container carries the surface so the
shadow is not clipped. The single defect is what sits under it: the mode capsule
directly below is **icon-only**, and its Move glyph is a near-duplicate of the
rail's Transform glyph 4 dp away (**I-17**).

**Transform mode / space selectors** — 48×48 dp targets, both space states drawn
rather than a toggle, space correctly absent in Scale. Two problems, both
positional: the 4 dp uniform gaps (**I-07**), and the centre-anchored cluster that
moves the control you just pressed out from under your finger (**I-14**).

**Exact Transform (precision surface)** — See its own section below.

**Shape editor** — Six primitive options as 119.6 × 48 dp text chips, three per
row, then the dimension row for the chosen kind. Clean grid, consistent field
widths. It contradicts Add Primitive's vocabulary for the same six shapes
(**I-11**), and `apply_shape` is below the fold like `apply_transform`.

**Objects capsule / Objects popover / Add Primitive** — The best-behaved surfaces
in the product. Both popovers are anchored to their invoking control and cause
**zero** displacement of any other bound (rail 591 px and objects capsule 2168 px
identical before and after). Add Primitive's six 118 × 76 dp tiles with 26 dp
silhouettes are the most legible control group in the app. The only complaint is
that the body label `Body #1` is painted in the failing secondary role at 3.69:1.

**Undo / Redo** — Correct state machine, verified: at rest both `enabled=false`;
after one creation Undo `true`, Redo `false`; after Undo, Undo `false`, Redo
`true`, and the capsule label returns `Body #2`→`Body #1`. Both absent in Sculpt,
per the domain rule. Two gaps: no confirmation of *what* was undone
(**I-19**), and disabled controls still report `clickable="true"` (**I-18**).

**Start Sculpting / Back to Construction / Resume Sculpt** — All three transitions
work, all three carry full wording, and the workspace correctly swaps its
inventory each way: Sculpt drops Undo/Redo and Add, gains the brush column and a
four-tool rail. No user-facing string says "Freeze". **PASS.**

**Sculpt tools, Radius / Strength** — The strongest single control group in the
product. Two vertical sliders, 48 dp touch width against a 10 dp drawn track,
11 dp thumbs, filled track showing value, label above value above track. Legible
at a glance and unambiguous. One vocabulary problem: Radius reads **`120 px`** — a
device-pixel unit in a product whose every other number is metres, on a value the
sculpt kernel resolves in the world metric (**I-16**).

**Sculpt details** — 157 dp surface carrying the mesh summary and
`Reset Sculpt from Shape…`. Correct wording, correct scope.

**Display settings** — Five option groups in one 228 × 500 dp popover. Verified
**stays open across successive selections**, and survives even the Activity
recreate an appearance change triggers. Two issues: the Grid `On`/`Off` chips
measure 37.0 and 39.2 dp wide (**I-06**), and `Debug` is offered as a user-facing
shading mode (**I-20**).

**Hide UI** — Everything disappears; one 51.4 dp restore chip with the content
description "Show UI" remains. Exactly right. **PASS.**

---

## Move / Rotate / Scale — three separate reviews

Shared runtime geometry, from the `KEYCODE_G` diagnostic at default framing
([`R02`](R02-gizmo-handles.txt)), 2.625 px/dp:

- effective scale: Y shaft spans 157.8 px for 96 reference units = **1.644 px/unit**
- `kGizmoHitSlopUnits` = 24 → **39.5 px = 15.0 dp radius**, i.e. a **30 dp target**
  against a 48 dp floor
- nearest neighbouring handle centres (`yz`↔`y`): **123.7 px = 47.1 dp**
- pivot ↔ `xz` plane handle: **76.9 px = 29.3 dp** — closer than the two 30 dp
  discs are wide, so their hit areas overlap

### Move

**Arrow / axis readability — fail.** The shaft is a *hollow rectangle* built from
two 1 px lines, so the body shows through it, and the arrowhead is an **open V**
of two lines, 12 dp long and 9 dp wide
(`kGizmoArrowLengthFraction` 0.20, `kGizmoArrowHalfWidthFraction` 0.075). At 3×
([`A15b`](A15b-crop-gizmo-move-3x.png)) it is unmistakably a wireframe, not an
instrument. The cause is structural and known: `lineWidth > 1.0` needs the
`wideLines` device feature the product does not request, so every part of the
gizmo is a hairline.

**Pivot — present but effectively invisible.** `kGizmoPivotMarkerFraction` 0.10
gives a ~12 dp cross in a low-contrast grey, drawn on top of the point where all
three axes already cross. It guards a 15 dp dead radius. A hit target the user
cannot see is worse than none.

**Plane handles — wrong vocabulary.** Three hollow quads in the axis hues,
27→53 units from the pivot, drawn over the body's faces. They read as stray
selection wireframe. Nothing distinguishes "a plane you can drag" from "an axis
you can drag" except position and hue.

**Overlap and hit separation — fail.** Six to seven 30 dp targets inside a
~120 dp circle, with the pivot and the `xz` plane 29.3 dp apart.

**Contrast over light and dark geometry — marginal.** Over the tan body the red
X shaft and the green Y shaft both lose definition; there is no dark backing
stroke. Over the charcoal ground the blue Z is the dimmest element on screen.

**Edge of screen** — the gizmo is drawn in screen space at a camera-derived
scale and does not clip or invert near an edge; the one shared scale invariant
holds (drawing and hit-testing move together).

**Occlusion — none.** Depth test and depth write are off for the gizmo pipeline,
so a handle behind the body is pixel-identical to one in front.

**Verdict: debug wireframe, not a finished modeling control.**

### Rotate

Three **full** circles at 78 units = 49 dp radius, each two hairlines wide, all
three drawn 360° with no near/far distinction
([`A16b`](A16b-crop-gizmo-rotate-3x.png)). They cross each other at six points,
and at every crossing two 30 dp hit discs overlap, so which axis a tap grabs is
positional luck. There is no pivot mark, no screen-space outer ring and no
free-rotation affordance. Professional rotate gizmos draw the front hemisphere
solid and the back half dimmed or hidden; this draws both at equal weight, so the
ring nearer the camera cannot be told from the ring behind the model.

The one thing Rotate does better than Move: three curves are easier to tell apart
from each other than three straight lines plus three quads plus a cross.

**Verdict: readable as three rings, unreliable as three targets.**

### Scale

**Handle vocabulary — the clearest intent, the weakest execution.** Cubes are the
right idiom for scale. The uniform-scale cube is drawn in **near-black** with no
colour role, hollow, 11 dp across, sitting exactly on the pivot
([`A18b`](A18b-crop-gizmo-scale-3x.png)) — the runtime diagnostic confirms
`uniform=540.0,1200.0` and `pivot=540.0,1200.0`, the same point. Two different
meanings, two different hit radii (both 24 units), one location, and the drawn
mark reads as part of the model's wireframe rather than as a control.

Scale reuses Move's plane quads and shafts unchanged, so at a glance Move and
Scale are the same picture. The only difference is what sits at the ends and the
centre — and those are exactly the smallest, thinnest marks in the drawing.

**Verdict: correct vocabulary, drawn too faintly to carry it, and colliding with
the pivot.**

---

## Exact Transform review

**Long positive and negative values — P1 defect.** Typing `-98765.4321098` into
Pos X renders `765.4321098`. The clipping happens at the **left**, so the minus
sign and the leading digits vanish and the user is shown a plausible *positive*
number. No ellipsis, no fade, no scroll affordance says anything is missing.
Artifact [`E01`](E01-long-negative-value.png).

**Field selection and edit behaviour — P1 defect.** Tapping a field places a
caret; it does not select. Typing into a field showing `0` produced
`0-98765.4321098`, a malformed string the panel accepted into its own state. The
fix is `setSelectAllOnFocus(true)` on `NumericPropertyRow`.

**Storage precision vs display presentation — PASS, and worth saying loudly.**
`1.23456789012345` was typed, applied, the surface closed and reopened, and the
field redisplayed **`1.23456789012345`** — 15 significant digits, exact, no
rounding on the way through ([`R09`](R09-precision-roundtrip.txt)). The stored
value is double and the display does not truncate it. The only precision problem
in this panel is presentational clipping, which means the fix is safe: change how
the string is anchored, never what is stored.

**Units and unitless Scale — P2.** The group order is Position → Rotation →
Scale → *Display unit* → Apply. The mm/cm/m chips govern **Position only** and
the code knows it, but the layout seats them directly under the Scale row, which
is unitless by the product's own hard rule. In
[`G01`](G01-scale-zero-refusal.png) the chips read unmistakably as Scale's units.

**Fold and scroll affordance — P1.** In compact portrait the surface shows
Position and Rotation and ends flush at the Rotation row's bottom edge with no
peek row, fade or indicator. It takes **three swipes** to reach Apply. In short
landscape the docked column shows 133 dp of scroll — the Position row and nothing
else. In expanded, everything including Apply is visible at once.

**Apply reachability** — compact portrait: 3 swipes. Short landscape: worse.
Expanded: immediate. The surface's own toggle, which closes it, is simultaneously
squeezed to 37 dp (or 17.1 dp at large text, or gone entirely with the IME up).

**Field and label alignment — PASS in all three layouts.** Three equal columns
throughout: 117.0 / 117.0 / 117.3 dp compact, 85.3 dp landscape, 92.0 dp expanded,
with an 8 dp gutter and every field exactly 48 dp tall. Labels sit consistently
above their fields, section labels above their groups. The title bar is sticky
while the body scrolls. This is the most disciplined grid in the product.

**Error presentation — P2.** Scale X = 0 → *"Rejected: every scale must be greater
than 0. Transform unchanged."* The copy is excellent: it names the rule and the
outcome. But the message renders at y=286 px while the offending field is at
y=1780 px — 570 dp apart — and the field itself gets **no error state at all**;
its border is identical to its valid neighbours. And the message is drawn in
`p1_text_error`, measured at **2.96:1**.

---

## Stable anchors — bounds before / open / close

Compact portrait, measured, px. "Justified" means the movement is required by a
rule the product states; "accidental" means nothing requires it.

| transition | Tool Rail top | precision toggle top | objects capsule top | classification |
| --- | --- | --- | --- | --- |
| Construction resting | 955 | 1350 | 2168 | baseline |
| → Transform selected | **591** | **1715** | 2168 | **accidental** (−364 px / 139 dp) |
| Move → Rotate | 591 | 1715 | 2168 | none |
| Rotate → **Scale** | **739** | **1567** | 2168 | **accidental** (+148 px / 56 dp) |
| precision surface opens | **275** | **1388** | **1485** | mixed: capsule rise justified, rail rise accidental |
| precision surface closes | 955 | 1350 | 2168 | **exact restore** |
| Add Primitive opens | 591 | 1715 | 2168 | **zero movement** |
| Objects popover opens | 591 | 1715 | 2168 | **zero movement** |
| display popover opens | 591 | 1715 | 2168 | **zero movement** |
| IME opens over the panel | 275 | *absent* | 665 | **accidental destruction** |

**Justified adaptive reflow, verified and rejected as findings:**

- The Objects capsule and history capsule rising 683 px when the precision surface
  opens. Required by the rule that a surface may never cover another live control.
- The rail sliding 312 dp left (landscape) and 332 dp left (expanded) when the
  inspector docks. Same rule, horizontal.
- Scale removing the World/Local capsule entirely — a world-axis scale of a turned
  body is a shear.
- Sculpt removing Undo/Redo and Add Primitive.
- Expanded replacing the floating Objects capsule with a docked panel.

**Accidental movement, root cause named.** `EditorWorkspaceView.java:1329`:

```java
rail.gravity = docked ? Gravity.TOP : Gravity.CENTER_VERTICAL;
```

The floating cluster is centre-anchored, so any height change moves every child
by half the delta, in opposite directions above and below the centre. The
expanded layout, which uses `Gravity.TOP`, shows **zero** vertical rail movement.

**Runtime proof that this is not theoretical.** From Rotate, the Scale control's
centre is (985, 1323). Two identical taps at (985, 1323), one second apart:

1. first tap selects Scale; the capsule shifts +148 px because the space capsule
   is removed;
2. (985, 1323) is now inside **Rotate**, so the second tap reverts the mode.

Verified end state: `transform_space_group` present at [911,1408][1059,1693],
i.e. **not** in Scale. Artifact [`A06`](A06-double-tap-displacement.png). A user
who double-taps, or taps twice because the first press did not seem to register,
silently undoes their own selection.

---

## Findings matrix

Severity per `audit.native.md`: **P0** blocking · **P1** major / platform-guideline
violation · **P2** minor with a workaround · **P3** polish.

| ID | sev | artifact | measurement | source owner | Impeccable rule | verdict |
| --- | --- | --- | --- | --- | --- | --- |
| **I-02** | **P0** | `E01`, `R06` | IME open: `transform_mode_group` = 148×**6 px = 56.4 × 2.3 dp**; `transform_space_group` and `precision_toggle` absent from the tree | `EditorWorkspaceView.java:455-600` (`railColumn`, no weights, no min height) | audit.native.md 5 Adaptivity — IME must not hide or destroy controls | **accept** |
| **I-01** | **P1** | `A07`, `B02`, `D03`, `D04` | `precision_toggle` = 48.0 × **37.0** dp (portrait, surface open), **29.0** dp (short landscape sculpt), **17.1** dp (font_scale 1.3) | same as I-02 | android.md — 48×48 dp minimum | **accept** |
| **I-03** | **P1** | `F03` | Add Primitive open + one `KEYCODE_BACK` → `topResumedActivity=NexusLauncherActivity` | `ForgeShapeActivity` — no Back branch | android.md — System Back always works | **accept** |
| **I-04** | **P1** | `A15`,`A15b`,`A16b`,`A18b`,`R02` | hit radius 24 units × 1.644 px/unit = 39.5 px = **15.0 dp** (30 dp target); pivot↔`xz` centres **29.3 dp**; `uniform` and `pivot` at the identical point (540.0, 1200.0) | `forgeshape_gizmo.h:233,263,272,286,299-301`; gizmo pipeline `forgeshape_renderer.cpp` (depth off) | android.md touch targets; audit.native.md 4 conformance | **accept** |
| **I-05** | **P1** | `A01`, `G01` | `p1_text_secondary` #A29B91 on #403D38 = **3.93:1**; `p1_text_error` #D9695C = **2.96:1**; p2 **3.87:1**, p3 **3.69:1** — all below AA 4.5:1 at 11-14 sp | `res/values/colors.xml:45,49,87,91,122,126` | audit.native.md 1 — contrast in either appearance | **accept** |
| **I-08** | **P1** | `E01`, `R06` | `-98765.4321098` renders `765.4321098`; clipping at the **left**, sign lost, no affordance | `NumericPropertyRow` / `bg_field` | layout.md Extremes; critique.md error prevention | **accept** |
| **I-10** | **P1** | `A07`→`A09`, `B04` | Apply is **3 swipes** below the fold compact; landscape shows 133 dp of scroll = Position row only; surface ends flush with no peek or fade | `PropertyInspectorView` / `PrecisionScrollView` | layout.md reading order; critique.md hierarchy | **accept** |
| **I-14** | **P2** | `A02`,`A05`,`A06`,`A07` | rail top 955→591 (**−139 dp**), 591→739 (**+56 dp**), 591→275 (**−120 dp**); two identical taps at (985,1323) end in Rotate, not Scale | `EditorWorkspaceView.java:1329` | layout.md structure; critique.md consistency | **accept** |
| **I-06** | **P2** | `A13` | `view_grid_on` **37.0 × 48.0 dp**, `view_grid_off` **39.2 × 48.0 dp**; 4 dp apart | `DisplaySettingsPopoverView.java:269-290` + `chip_padding_horizontal` | android.md 48 dp, 8 dp separation | **accept** |
| **I-07** | **P2** | `A02`, `A14b` | four capsules separated by exactly **4 dp** (`row_gap_small`); tops 591 / 975 / 1408 / 1715 px | `EditorWorkspaceView.java:531,573,598` (`topMargin = row_gap_small`) | android.md 8 dp separation; layout.md rhythm | **accept** |
| **I-09** | **P2** | `A09`, `G01` | order Position → Rotation → Scale → "Display unit" → Apply; chips govern Position only | `ConstructionPlacementEditorView.java:81-100` | layout.md group by meaning | **accept** |
| **I-11** | **P2** | `A11` vs `F01` | Add Primitive: 2 columns, 118 × 76 dp, 26 dp icon. Shape editor: 3 columns, 119.6 × 48 dp, text only. Same six primitives. | `AddPrimitivePaletteView` vs `ConstructionShapeEditorView` | critique.md violation 6 — same action, same UI | **accept** |
| **I-12** | **P2** | `A02`, `A07` | 8 chrome controls in one 500 dp column + up to 7 gizmo handles = 15 simultaneous targets | `EditorWorkspaceView` `railColumn` composition | critique.md Working Memory Rule (8+ = overload) | **accept** |
| **I-15** | **P2** | `G01`, `R08` | refusal at y=286 px, offending field at y=1780 px (**570 dp** apart); field border identical to valid siblings | `EditorWorkspaceView` status line + `NumericPropertyRow` | critique.md error recovery; layout.md proximity | **accept** |
| **I-16** | **P2** | `A19` | brush Radius reads **`120 px`** — a device-pixel unit in a metre-based product whose kernel works in the world metric | `BrushEdgeControlsView` | android.md typography/vocabulary; critique.md jargon barrier | **accept** |
| **I-13** | **P3** | `A01` vs `B01`, `C02` | `editing_context_label` present in landscape (42.3 dp) and expanded (89.3 dp), absent in compact portrait | `EditorWorkspaceView` | critique.md violation 3 — show current location | **accept** |
| **I-17** | **P3** | `A14b` | `ic_tool_place` and `ic_gizmo_move` are the same 4-way arrow (paths differ only by a 0.7-unit inset and a centre dot), rendered 4 dp apart at 20-22 dp | `res/drawable/ic_tool_place.xml`, `ic_gizmo_move.xml` | critique.md recognition over recall | **accept** |
| **I-18** | **P3** | `R07` | `undo_action`, `redo_action`, `export_action` report `enabled="false"` **and** `clickable="true"` | `EditorControlStyles.iconButton` | audit.native.md 1 — traits and state | **accept** |
| **I-19** | **P3** | `A22`, `A23` | Undo changes only the capsule label (`Body #2`→`Body #1`); no verdict is reported | `EditorWorkspaceView` history wiring | critique.md visibility of system status | **accept** |
| **I-20** | **P3** | `A13` | `Debug` offered as one of three user-facing shading modes | `DisplaySettingsPopoverView`, `strings.xml:86` | audit.native.md 4 — system drift | **accept** |
| **I-21** | **P3** | `C02`, `C05` | expanded: history capsule at y=2416 px while all other chrome is above y=944 px | `EditorWorkspaceView` expanded placement | adapt.native.md — use the width, do not strand controls | **accept** |

---

## PASS / ISSUE / N/A inventory — every required surface

| required surface | verdict | evidence |
| --- | --- | --- |
| compact portrait | **ISSUE** — I-01, I-02, I-07, I-10, I-12, I-13, I-14 | `A00`-`A23`, `R01` |
| short landscape | **ISSUE** — I-01 (29 dp), I-10 | `B01`-`B04`, `R03` |
| expanded / tablet | **PASS with one issue** — I-21 only; no vertical rail movement, Apply visible | `C01`-`C05`, `R04` |
| Construction resting | **PASS** | `A01`, `R01` |
| Move | **ISSUE** — I-04 | `A15`, `A15b`, `R02` |
| Rotate | **ISSUE** — I-04 (ring crossings, no pivot, no depth cue) | `A16`, `A16b`, `A17`, `R02` |
| Scale | **ISSUE** — I-04 (uniform handle coincident with pivot, near-black hollow cube) | `A18`, `A18b`, `R02` |
| World / Local | **PASS** — both states drawn, correctly absent in Scale, 48 dp each | `A02`, `A04`, `A05` |
| precision / Exact Transform | **ISSUE** — I-08, I-09, I-10, I-15; grid and precision round-trip PASS | `A07`-`A10`, `B04`, `C04`, `E01`, `G01`, `H01`, `R06`, `R09` |
| Objects capsule / popover | **PASS** — zero displacement | `A12`, `R01` |
| Add Primitive | **PASS with I-11** (vocabulary conflict with Shape editor) | `A11`, `F01` |
| Undo / Redo | **PASS with I-18, I-19** — state machine verified correct | `A22`, `A23`, `R07` |
| Start Sculpting / Back / Resume | **PASS** — all three, full wording, correct inventory swap | `A19`, `A21`, `R01` |
| Sculpt tools | **PASS** — 4-entry rail, icon + caption, 48 dp | `A19`, `B02` |
| Radius / Strength | **PASS with I-16** (`px` unit) | `A19` |
| Sculpt details | **PASS** | `A20`, `B03` |
| appearance / display surfaces | **ISSUE** — I-06, I-20; stays-open behaviour PASS | `A13`, `A14`, `I01`, `R07` |
| Start chooser (not required, audited) | **PASS** — scrim provably blocks input | `A00`, `C01`, `D01` |
| Hide UI (not required, audited) | **PASS** | `F02` |
| System Back (not required, audited) | **ISSUE** — I-03 | `F03`, `F04` |
| large text (`font_scale 1.3`) | **ISSUE** — I-01; type scaling itself PASS | `D01`-`D04`, `R05` |

---

## Patterns and systemic issues

1. **One layout container produces the two worst findings.** `railColumn` has no
   weight, no `minHeight` and no overflow strategy, so every squeeze lands on its
   last children. Four measured configurations, one fix site.
2. **The trailing cluster is centre-anchored while the correct anchor already
   exists in the same file.** `Gravity.CENTER_VERTICAL` for floating,
   `Gravity.TOP` for docked — and only the docked path is stable.
3. **A single spacing value is doing all grouping work.** `row_gap_small` (4 dp)
   separates the four right-edge capsules, the display popover's chips and the
   Grid chips. It is simultaneously below the platform's 8 dp target separation
   and too uniform to express rank.
4. **One colour role fails contrast everywhere it is used.** `*_text_secondary`
   carries field captions, section labels and the active body's name in all three
   appearances at 3.69-3.97:1. It is one token, three files, one fix.
5. **The gizmo has one shape vocabulary for four handle kinds.** Axis, plane,
   pivot and scale cube are all hollow hairline outlines, because `wideLines` is
   unavailable. Everything else about the gizmo — the shared scale, the frozen
   drag basis, the transaction discipline — is right.
6. **The same domain concept is drawn twice, differently.** Six primitives in two
   vocabularies; a 4-way arrow meaning both "Transform tool" and "Move mode".

---

## Positive findings — keep and replicate

1. **`AnchoredSurfaceView` is the correct pattern and is already in the repo.**
   Three separate popovers, **zero** displacement of any other bound, measured.
2. **The expanded layout is right.** Docked Objects panel, docked inspector, no
   vertical rail movement, Apply visible, top edges aligned on one line.
3. **Precision survives the round trip.** 15 significant digits typed, applied,
   redisplayed exactly. The stored value is not the problem.
4. **The start chooser's modality is airtight.** The scrim swallows touches over
   live controls; the panel is its own scroller so a short window still reaches
   both answers.
5. **Refusal copy is excellent.** *"Rejected: every scale must be greater than 0.
   Transform unchanged."* — names the rule and the outcome, no jargon, no blame.
6. **The brush controls are the best control group in the product.** 48 dp touch
   width on a 10 dp track, value above label above track, unmistakable.
7. **Semantic ids and content descriptions are complete and stable.** The whole
   audit was driven by them; not one control had to be located by coordinate.
8. **Type genuinely scales.** `sp` dimens through `getDimensionPixelSize`; at
   `font_scale 1.3` every label grew and **nothing** truncated.

Also verified correct and worth stating: concentric corners (26 dp capsule / 6 dp
padding / 20 dp entry, exact); full-bleed `SurfaceView` with insets on chrome
only; `Export` recessed and honestly labelled; the domain guards that remove
controls rather than showing and refusing them.

---

## Rejected subjective recommendations

Candidates considered and **not** raised, each with the reason.

1. **"The modal scrim leaks touches."** My own first measurement, wrong. Retested
   with raw coordinates: the scrim swallows taps over the Display control and
   over empty ground. **Rejected — measurement error, corrected above.**
2. **"The display popover closes after every selection."** Carried by UI-AUDIT1
   as finding N. Retested: the popover bounds are identical after selecting
   Faceted, and it survives even the appearance change's Activity recreate.
   PRODUCT.md documents the stay-open behaviour. **Rejected — not reproducible.**
3. **"Adopt Material 3 components, a navigation bar and Material Symbols."**
   `android.md` allows brand to express through Material's theming, but a
   full-bleed GPU modeling workspace does not have Material's information
   architecture, and the repository forbids the dependency outright. Recommending
   it would be taste dressed as conformance. **Rejected.**
4. **"Ship a light theme."** All three appearances are dark; the app ignores the
   system uimode. For a tool where the model's own value must be judged, a light
   chrome would fight the work. A deliberate product decision, not a defect.
   **Rejected.**
5. **"Animate the chrome transitions so the jumps feel smoother."** Motion would
   make a 139 dp teleport legible without making it correct. Remove the
   displacement first (I-14); animate only what legitimately moves afterwards.
   **Rejected as a fix; the underlying displacement is I-14.**
6. **"The gizmo needs a redesign."** It needs stroke weight, a colour role for
   the pivot, and one constant raised. The geometry, the shared draw/hit scale,
   the frozen drag basis and the transaction discipline are all correct. A
   redesign would risk invariants the repository states as hard rules.
   **Rejected in favour of the narrow corrections in P0/P1 below.**

Not re-verified in this pass and therefore **not** carried as findings: the
double swapchain create on appearance change, and stale pixel sizes after a
*live* density change (this run force-stopped between density changes by design).
Both belong to UI-AUDIT1; neither was reproduced here, and neither is contradicted.

---

## Correction candidates — ranked, not implemented

Every P0 and P1 carries an artifact id, a source owner and a measured value.

### P0

1. **Give the rail column's trailing children a floor.** *(I-02, I-01)*
   Artifact `E01`, `D03`, `R06`. Owner `EditorWorkspaceView.java:455-600`.
   Measured: 2.3 dp selector, 17.1 / 29.0 / 37.0 dp toggle. A `minHeight` of
   `control_height` on `precisionGroup` and `transformSelectorRow`, and a scroll
   or a fold for the column when the remainder cannot hold them, rather than
   silent compression. Suggested command: `$impeccable adapt`.

### P1

2. **Let a surface consume System Back.** *(I-03)* Artifact `F03`. Owner
   `ForgeShapeActivity`. Measured: launcher resumed after one Back with Add
   Primitive open. One branch: if any context surface is open, close the topmost
   and consume; otherwise default. Suggested command: `$impeccable harden`.

3. **Select on focus, and anchor long values at the sign.** *(I-08)* Artifact
   `E01`. Owner `NumericPropertyRow`. Measured: `0-98765.4321098` accepted;
   `-98765.4321098` displayed as `765.4321098`. `setSelectAllOnFocus(true)` plus
   an end-anchored or shrink-to-fit presentation. The stored double is already
   correct (`H01`) and must not change. Suggested command: `$impeccable harden`.

4. **Raise the gizmo's minimum pixels-per-unit and give the pivot its own role.**
   *(I-04)* Artifacts `A15b`, `A16b`, `A18b`, `R02`. Owner
   `forgeshape_gizmo.h:299-301`, `:286`, and the gizmo pipeline. Measured:
   1.644 px/unit → 15.0 dp hit radius; pivot ↔ `xz` 29.3 dp; `uniform` and
   `pivot` coincident. Raising `kGizmoMinPixelsPerReferenceUnit` moves drawing
   **and** hit-testing together, preserving the one-shared-scale invariant; a
   dark backing stroke and a distinct pivot hue cost nothing and are drawn by the
   existing line pipeline. Suggested command: `$impeccable polish`.

5. **Fix the two failing colour roles.** *(I-05, I-15)* Artifacts `A01`, `G01`.
   Owner `res/values/colors.xml:45,49,87,91,122,126`. Measured: 3.93 / 3.87 /
   3.69:1 secondary, 2.96:1 error. Lighten both until they clear 4.5:1 on the
   surfaces they are actually drawn on — the audit measured those surfaces, so
   the target values are checkable. Suggested command: `$impeccable colorize`.

6. **Make the precision surface admit it continues.** *(I-10)* Artifacts `A07`,
   `A09`, `B04`. Owner `PropertyInspectorView` / `PrecisionScrollView`. Measured:
   3 swipes to Apply compact; 133 dp of scroll landscape. A peek row, a bottom
   fade, or a pinned Apply. Suggested command: `$impeccable layout`.

7. **Anchor the floating cluster to the top.** *(I-14)* Artifacts `A02`, `A05`,
   `A06`. Owner `EditorWorkspaceView.java:1329`. Measured: 139 dp, 56 dp and
   120 dp displacements; two identical taps end in the wrong mode. The docked
   path already does this. Suggested command: `$impeccable layout`.

### P2

8. Give the Grid chips a 48 dp minimum width and 8 dp separation *(I-06)*.
9. Make the four right-edge capsule gaps express rank instead of one 4 dp value
   *(I-07)* — and raise them to at least 8 dp.
10. Move the unit chips under Position, or label them "Position unit" *(I-09)*.
11. Unify the six-primitive vocabulary across Add Primitive and the Shape editor
    *(I-11)*.
12. Reduce the simultaneous target count on the right edge *(I-12)*.
13. Put the refusal next to the field it refuses, and give the field an error
    state *(I-15)*.
14. Express brush Radius in a unit the product owns *(I-16)*.

### P3

15. Show the editing-context label in compact portrait too *(I-13)*.
16. Differentiate the Transform-tool and Move-mode glyphs *(I-17)*.
17. Make disabled controls report `clickable="false"` *(I-18)*.
18. Report what an Undo reverted *(I-19)*.
19. Remove `Debug` from the user-facing shading modes *(I-20)*.
20. Bring the expanded history capsule up to the cluster it serves *(I-21)*.

### Recommended command order

1. **[P0]** `$impeccable adapt` — the rail column floor, IME and font-scale cases.
2. **[P1]** `$impeccable harden` — System Back, select-on-focus, long-value display.
3. **[P1]** `$impeccable polish` — gizmo stroke weight, pivot role, hit scale.
4. **[P1]** `$impeccable colorize` — the two failing text roles.
5. **[P1]** `$impeccable layout` — precision-surface continuation, cluster anchoring.
6. **[P2]** `$impeccable layout` — capsule rhythm, unit-chip grouping, target count.
7. **[P2]** `$impeccable clarify` — brush units, undo verdict, `Debug` removal.
8. **[P3]** `$impeccable polish` — the remaining polish set.

---

## Next step

**Run UI-AUDIT2B — Emil Kowalski independent audit.**
