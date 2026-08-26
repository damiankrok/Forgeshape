# UI-AUDIT1 — gizmo / chrome / Exact Transform visual + motion audit

Read-only audit. **No product source, resource or build file was changed.**
Baseline and final product HEAD: `bd3f7d5`. The only repository change is this
evidence directory.

Runtime evidence taken on `ForgeShape_Stage006` / `emulator-5580` (confirmed by
`adb -s emulator-5580 emu avd name`, never by port), ForgeShape confirmed as the
resumed activity, 13/13 `*_SELFTEST_OK` and `FORGESHAPE_NATIVE_VIEWPORT_OK` on a
clean launch, log ring buffer at 16M. Every control was located and tapped from
its **own runtime bounds resolved by semantic id**; no screen coordinate appears
in the driving harness.

Compact density is 420 dpi = **2.625 px/dp**. Expanded evidence was taken at
1600×2560 after a `force-stop` relaunch so the 240 dpi override actually reached
the process (density 1.5) — see finding **J** for why that matters.

## Audit lenses

| lens | location | status |
| --- | --- | --- |
| Impeccable | `~/.claude/skills/impeccable/SKILL.md` v4.1.1, scored with `reference/audit.native.md` | present, read |
| Emil Kowalski design engineering | `~/.claude/skills/emil-design-eng/SKILL.md` | present, read |
| Remotion | `~/.claude/skills/remotion-best-practices/SKILL.md` v4.0.517 (router) | present, read — assessed for external prototype value only |

No React, Remotion or audit package was added to the repository or the APK.

## Findings matrix

Severity: P0 blocking · P1 major · P2 minor · P3 polish.

| # | finding | sev | evidence | source owner | Impeccable | Emil | Remotion | verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A | System Back exits the app while a surface is open | P1 | runtime: Add Primitive open → one BACK → launcher | `ForgeShapeActivity` — no BACK case at all | Conformance | — | — | **accept** |
| B | Precision toggle is 48×**29 dp** in short landscape | P1 | `L01`, bounds `[2242,772][2368,848]` = 126×76 px | `EditorWorkspaceView` rail-column height distribution | A11y / Conformance | — | — | **accept** |
| C | Trailing cluster jumps on three unrelated state changes | P1 | bounds below | `EditorWorkspaceView.java:1329` | Adaptivity | strong | candidate | **accept** |
| D | Long values clip at the LEFT — the minus sign disappears | P1 | `C01`: text `-98765.4321098` renders `765.4321098` | `NumericPropertyRow` / `bg_field` | Conformance | — | — | **accept** |
| E | Tapping a numeric field appends instead of replacing | P1 | `C01`: typing into `0` yielded `0-98765.4321098` | `NumericPropertyRow` (no select-on-focus) | Conformance | — | — | **accept** |
| F | `Apply Transform` is two full swipes below the fold | P1 | `P05` → `P06` → `P07` | `PropertyInspectorView` / `PrecisionScrollView` | Hierarchy | — | — | **accept** |
| G | Unit chips sit under **Scale**, which is unitless by hard rule | P1 | `P07` | `ConstructionPlacementEditorView.java:81-100` | Hierarchy / copy | — | — | **accept** |
| H | Gizmo hit radius ≈ **15 dp** (30 dp target), handles ~48 dp apart | P1 | `KEYCODE_G` dump + source constants | `kGizmoMinPixelsPerReferenceUnit` = 0.25 | A11y | — | — | **accept** |
| I | No surface hides background content from TalkBack | P2 | zero `setImportantForAccessibility` in the shell | whole Java shell | A11y | — | — | **accept** |
| J | Pixel sizes go stale after a live density change | P2 | 126 px vs 72 px for one 48 dp control | `EditorWorkspaceView.onConfigurationChanged` | Adaptivity | — | — | **accept** |
| K | Appearance change recreates the Activity → 2× swapchain create | P2 | logcat, reproduced twice at unchanged extent | `ForgeShapeActivity.requestTheme` → `recreate()` | Performance | weak | — | **defer** |
| L | Gizmo has no occlusion cue at all | P2 | `forgeshape_renderer.cpp:2098` depth test/write OFF | gizmo pipeline | Theming | — | — | **defer** |
| M | `editing_context_label` ("Sculpt") exists only in landscape/expanded | P3 | `L01`, `E01` vs `P11` | `EditorWorkspaceView` | Consistency | — | — | **defer** |
| N | Display popover closes after every single selection | P3 | runtime, 5 setting groups | `DisplaySettingsPopoverView` | Flow | — | — | **defer** |
| O | `display_mode_debug` is a user-facing shading option | P3 | `P12` | `DisplaySettingsPopoverView` | Conformance | — | — | **defer** |
| P | Primary fill is worn by both a mode switch and a commit | P2 | `P01`, `P07` | `attrs.xml` `fsPrimaryFill` | Hierarchy | — | — | **owner** |

## C — the chrome jump, measured

Compact portrait. One root cause, three reproductions.

| transition | Tool Rail top | precision toggle top | rail delta |
| --- | --- | --- | --- |
| Construction resting | 955 | 1350 | — |
| → Transform selected | 591 | 1715 | **−364 px (139 dp)** |
| Move → Rotate | 591 | 1715 | 0 |
| Rotate → **Scale** | 739 | 1567 | **+148 px (56 dp)** |
| precision surface opens | 397 | 1225 | **−342 px (130 dp)** |
| precision surface closes | 955 | 1350 | exact restore |

`EditorWorkspaceView.java:1329`:

```java
rail.gravity = docked ? Gravity.TOP : Gravity.CENTER_VERTICAL;
```

The floating (compact) cluster is centre-anchored, so **any** height change moves
every child by half the delta, and the halves above and below the centre move in
opposite directions. The centring is deliberate and documented ("a capsule
standing on the picture belongs where the thumb is"); the consequence was not
weighed. In each case the control the user just tapped travels 56–139 dp away
from the finger, and the finger is left resting on a *different* live control
(tap Scale at y=1323 → Scale moves to 1471 → the finger now sits on Rotate).

**The expanded layout, which uses `Gravity.TOP`, shows zero rail movement**
(`E03`→`E04`: rail unchanged at 233, only the precision toggle moves). The
correct behaviour already exists in the repository.

### Intentional reflow, correctly distinguished

Not defects — verified and rejected as findings:

- `objects_capsule` and `history_group` rise 683 px when the precision surface
  opens. Required by the rule that a surface may never cover another live
  control; the bottom row deliberately rides above the sheet.
- Add Primitive, the Objects popover and the display popover cause **zero**
  movement of any chrome bound. `AnchoredSurfaceView` is the correct pattern and
  is already in the repository.
- Scale mode removes the World/Local group entirely (a world-axis scale of a
  turned body is a shear). Correct per the domain rule.
- Sculpt removes Undo/Redo and Add Primitive. Correct per the domain rules.

## Gizmo verdict

Runtime geometry, Move/World, default framing, via the existing `KEYCODE_G`
diagnostic:

```
pivot=540.0,1200.0  x=678.5,1255.9  y=540.0,1042.2  z=422.2,1267.0
xy=624.8,1137.0     xz=553.5,1275.7 yz=468.2,1143.0
```

Derived, at 2.625 px/dp:

| measure | value |
| --- | --- |
| effective scale (Y shaft, 96 reference units) | **1.64 px/unit** |
| hit radius (24 units: slop, plane, pivot dead zone) | **≈39 px ≈ 15 dp radius → a 30 dp target** |
| nearest neighbouring handle centres (`yz`↔`y`) | 123.9 px = **47.2 dp** |
| pivot ↔ `xz` plane handle | 76.9 px = **29.3 dp** |

Six to seven touch targets sit inside a ~120 dp circle, every one of them below
the 48 dp hit-area floor the repository sets for user-operated chrome.

It reads as debug wireframe rather than a finished modeling tool, and the reason
is structural: the gizmo is drawn **entirely** as line primitives, because
`lineWidth > 1.0` needs the `wideLines` device feature the product does not
request. So the axis-end handle, the plane handle and the pivot are all thin
hollow outlines — **one shape vocabulary doing three different jobs**,
distinguished only by position and hue. Depth test and depth write are OFF
(`forgeshape_renderer.cpp:2098`), so a handle behind the body is indistinguishable
from one in front.

Smallest visual-system corrections, no geometry redesign:

1. Raise `kGizmoMinPixelsPerReferenceUnit` from `0.25` to `≈2.625` so a 24-unit
   hit radius is never below 24 dp. This moves drawing **and** hit-testing
   together, preserving the one-shared-scale invariant. One constant.
2. Give the pivot its own colour role so shape-identity is carried by hue when
   three outline forms cannot carry it.
3. A dark second stroke pass behind each line, so the instrument survives on
   light tan geometry. Uses the existing line pipeline.
4. (Defer) A two-pass depth-tested / depth-free draw at different alpha restores
   an occlusion cue without making any handle ungrabbable.

## Exact Transform verdict

- **Long values clip at the left with no affordance.** `-98765.4321098` renders
  as `765.4321098`. The sign is invisible. This is a presentation bug and must be
  fixed **without** reducing stored precision: the authoritative value stays
  double; only the display needs an end-anchored or shrink-to-fit treatment.
- **Tapping a field appends.** Typing into a field showing `0` produced
  `0-98765.4321098`, a malformed string. A precision field should select its
  contents on focus.
- **Apply is two swipes below the fold** in compact portrait, and the Rotation
  row ends flush with the sheet's visible bottom, so the sheet reads as complete.
  No fade, peek row or scroll indicator says otherwise.
- **The unit chips are adjacent to Scale.** Order is Position → Rotation → Scale
  → "Display unit" → Apply. The chips govern **Position only**; the code knows
  this (`"No unit after the scale, deliberately: a multiplier has none."`) but the
  layout places them against the one group that is unitless by hard rule.
  Smallest fix: move the chips under the Position row, or label them
  "Position unit".
- Field alignment, label pairing and the three-column grid are otherwise clean
  and consistent across all three layouts.
- Opening the surface does **not** disturb the toolbar, and closing it restores
  every bound exactly. The unrelated movement it does cause is finding **C**.

## Motion verdict (Emil lens)

The motion that exists is genuinely good, and correct by Emil's rules:

| decision | value | verdict |
| --- | --- | --- |
| anchored enter / exit | 190 ms / 150 ms | exit faster than enter ✓ |
| fade enter / exit | 120 ms / 90 ms | ✓ |
| easing | `cubic-bezier(0.23, 1, 0.32, 1)` (easeOutQuint) | expressive ease-out ✓ |
| start scale | `0.96`, uniform in X and Y | nothing appears from nothing ✓ |
| reduced motion | duration → 0 at one seam, honoured exactly | ✓ |

The defect is **where** it is spent. `ChromeMotion` is consumed in exactly two
places: `AnchoredSurfaceView`, and the Hide-UI fade. Everything else is instant —
including all three chrome jumps in finding **C**, the precision surface, the
selector row, and every mode transition. So motion is applied to the transitions
that already displace nothing, and withheld from the ones that move a control
139 dp. That is precisely inverted.

Preferred order: **remove** the jump (finding C), then animate what legitimately
moves. Do not animate a 364 px teleport and call it fixed. The reduced-motion
contract is correct and must be preserved as-is.

## Remotion — external prototype candidates (max 3)

Review aids only. Nothing ships; no runtime or product integration.

1. **Trailing-cluster anchoring, A/B.** Split screen, same tap sequence
   (Transform → Rotate → Scale → open precision), centre-anchored on the left and
   top-anchored on the right, with a fixed horizontal guide across the Tool Rail's
   top edge and a finger marker that stays where it tapped. Communicates: how far
   the just-touched control travels, and that the expanded layout already solves it.
2. **Gizmo touch-target overlay.** The Move gizmo at its real 1.64 px/unit scale
   with each handle's 15 dp hit radius drawn as a translucent disc, plus a 24 dp
   reference disc, then a scrub to the proposed 2.625 px/unit. Communicates: the
   handles overlap today and separate cleanly after one constant changes.
3. **Precision surface continuation.** The compact sheet scrolling from Position
   to Apply at real duration, comparing the current hard cut against a peek row
   plus bottom fade. Communicates: why users do not believe there is more content.

## Stage 020R3 starburst — classification: **SCULPT_QUALITY**

Not `UI`, and not `TECH_DEFECT`. The transform-space brush kernel is provably
correct on both halves:

- **Footprint.** Set selection and falloff weight both measure with
  `brushWorldDistance` (`forgeshape_sculpt.cpp:611` and `:615`) — the same metric,
  so there is no discontinuity at the brush boundary. `sculptFalloff` is
  `(1 − t²)²`, C¹ and reaching exactly zero at the rim. The recorded evidence has
  `maxWorld < radiusWorld` in all twelve rows.
- **Displacement.** `brushLocalStepAlongNormal` builds the step in world space at
  the exact requested magnitude, then carries it back through the inverse model,
  so `M · localStep == worldStep` identically. Every affected vertex moves exactly
  `amount × weight` world metres along its world normal. There is no per-vertex
  magnitude error under a non-uniform Scale.

The mechanism is sampling density. From `S020R3-R00-runtime-transcript.txt`, the
affected-vertex count on the same 482-vertex sphere at the same screen radius:

| brush | Scale (1,1,1) | Scale (3,1,1) |
| --- | --- | --- |
| Grab | 129 | 65 |
| Clay | 111 | 54 |
| Smooth | 66 | 40 |
| Inflate | 82 | 47 |

A world-round brush is **correctly** narrower in the body's stretched axis, so it
collects roughly half the vertices, distributed as a thin lens in local X. At 54
vertices the deformation resolves into individual triangles, and adjacent world
normals — which diverge much faster under `R·S⁻¹` on a non-uniformly scaled body —
push neighbouring vertices in visibly different directions. That is expected
quality for a sparse fixed-topology mesh, not a bug.

**Smallest decisive follow-up experiment** (one run, no code change): sculpt the
same four strokes on the **unscaled** body with the brush radius reduced until the
affected count matches the scaled run (≈54 for Clay). If the unscaled body facets
equivalently at equal vertex count, sampling density is the whole cause and Scale
is irrelevant — proven rather than argued. If it stays smooth, reopen as
`TECH_DEFECT`.

No Remesh or subdivision scope is proposed here.

## Files

| file | what it shows |
| --- | --- |
| `P00-start-chooser.png` | New Project modal over scrimmed workspace |
| `P01-construction-resting.png` | Construction resting, baseline bounds |
| `P02-transform-move-gizmo.png` | Move gizmo + selector row |
| `P03-transform-rotate.png`, `P03-transform-scale.png` | Rotate and Scale gizmos |
| `P04-transform-scale-local.png` | Scale, World/Local group absent |
| `P05..P07-exact-transform-*.png` | precision surface open, scrolled, at Apply |
| `P08-after-precision-close.png` | exact bound restoration |
| `P09-add-primitive.png`, `P10-objects-popover.png` | anchored surfaces, zero displacement |
| `P11-sculpt-resting.png` | Sculpt, no Undo/Redo, no Add |
| `P12-display-popover.png` | display settings |
| `P13-light-charcoal.png`, `P14-warm-graphite.png` | appearance switches |
| `C01-long-position-values.png` | long values, clipped sign, IME-clipped rail |
| `L01-sculpt-landscape.png` | short landscape, 29 dp precision toggle |
| `E01..E04-expanded-*.png` | expanded layout, docked objects, no rail jump |
