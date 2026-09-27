# UX contract — the compact CAD extrude HUD and Ready-state chrome

This page states what the owner sees after Finish Sketch, and the rule behind
each part of it.

- **Owning code:** `CadExtrudeCanvasView`, `CadHudPresentation` (pure Java, JVM
  tested), `SketchChromePolicy` (pure Java, JVM tested),
  `WorkspaceTrailingHostView`, `EditorWorkspaceView`, `SettingsPageView` and
  `AppPreferences`.
- **Verified on device by:** `CadVerticalSliceTest.compact_extrude_hud` and
  `ui_context_withdrawal`.

## 1. One question at a time

| State | What is on screen | What is absent |
| --- | --- | --- |
| **Drawing** (Editing) | the drawing tools on the Tool Rail, the orientation navigator, a selected Line's dimension, Finish Sketch, Cancel | the HUD (nothing to extrude yet) |
| **Extruding** (Ready) | the HUD at the arrow, the region hatch and preview, Extrude (disabled while the preview is invalid), Back to Sketch, Cancel, the precision toggle | the Tool Rail, the orientation navigator, the Line dimension; the precision surface stays collapsed |

- **One rule.** `SketchChromePolicy` is the single statement of the table above.
  `SketchChromePolicyTest` pins it in every state.
- **Absent, not disabled.** Withdrawn chrome is ABSENT rather than disabled. The
  drawing tools have nothing to act on in Ready, and Back to Sketch returns to
  them in one tap.
- **Precision surface.** It no longer opens by itself at Finish. It is the
  exact-value route and it lists the regions, the operations and the depth, but
  it is one tap away on its toggle rather than covering 28.8 % of the viewport
  unasked, as it did before.

## 2. The HUD

**Layout.** One row, anchored at the extrusion arrow:

```text
[ extent ] [ 1.000 m ] [ operation ] [ flip ]        ← labels OFF: icons only
     ▼ opens                ▼ opens
[ one side | symmetric | two sides ]  [ new body | add | cut ]
```

**Extent control.** It shows the CURRENT mode's icon:

- One Side: a base line with ONE outward arrow.
- Symmetric: a base with two EQUAL arrows.
- Two Sides: a base with two arrows of visibly DIFFERENT lengths.

A tap opens the three-icon palette directly under it, and a choice closes it.
The chosen mode is readable with labels off, from shape alone.

**Exact value.** It is placed so its centre lands on the arrow shaft's midpoint
anchor, measured by the device test at ≤ 4 dp; before the change it was
99.5 dp away. It follows an orbit, a pan or a zoom through the one anchored
refresh path. A tap opens the typed editor, which submits through the same
native door as the precision field. In Two Sides the second value stands at its
own arrow.

**Operation badge.** It shows the current operation by SHAPE:

- New Body: a separate box.
- Add: a solid with a plus.
- Cut: a solid with a notch.

Colour is a second carrier only, and never the only indication that a Cut is
armed: Add uses the success colour, Cut the error colour, New Body the primary
text colour.

- **Palette.** A tap opens the palette of the operations native OFFERS. On a
  world-plane sketch only New Body is offered, so the badge states the
  operation and is not a control.
- **Invalid preview.** When the preview is not valid (a disjoint Add, a Cut
  that misses) the badge wears an error outline and its description appends the
  named reason.

**Flip.** One Side only; absent in Symmetric and Two Sides.

**Edit Sketch control.** It stands over a committed CAD Body at
`cadBodySketchAnchor`.

## 3. Sizes: visual size and hit size are different facts

| | Before | After |
| --- | --- | --- |
| Drawn glyph | 60 dp text pills, scaled 0.8–1.6 with their hit areas | `clamp(28 dp × scale, 24, 32)`: 24 dp at scale 0.8, 28 dp at 1.0, 32 dp at 1.6 |
| Hit area | 60 dp × scale (≥ 48 by arithmetic) | **≥ 48 × 48 dp at every scale, never `setScale`d** |
| Value | a 60 dp pill | a 32 dp pill inside a 48 dp hit row, 14 sp |
| Cluster | 1027 × 171 px, **6.78 %** of a 1080 × 2400 viewport, nearly full width | ≤ 360 dp wide and < 3 % (measured value in `TEST_EVIDENCE.md`) |
| Captions | always text | only with Tool Labels ON: 11 sp under the glyph |

- **Capsule geometry.** Capsules pad 2 dp around 48 dp members. Member corners
  are 24 dp, concentric with the capsule (inner = outer − gap).
- **Coverage rule.** Nothing covers a large fraction of the viewport merely to
  offer three mutually exclusive options: the options live in a palette that is
  closed at rest.

## 4. Accessibility

Every control always has a description stating what it is and its current
value, whatever the Tool Labels setting. Examples:

- "Extent: One side. Change extent"
- "Operation: Cut. Change operation"
- "Flip direction. …"

The chosen palette item carries `selected` and `activated` and a visible fill,
and its description says "selected". Colour is never the only carrier.

## 5. Tool Labels

**Settings → Interface → Tool labels: Off / On.**

- **Default and storage.** Default OFF. It is persisted in `AppPreferences`
  under the key `tool_labels`; a missing or wrong-typed value reads as OFF.
- **Scope.** It is application state. It never writes `.forge`, the project
  fingerprint or history. The device test re-encodes the project across an
  Activity recreation and compares bytes.
- **Effect when ON.** Short one-word captions appear under the HUD's glyphs,
  and the row stays ≤ 360 dp.
- **What it does not touch.** It changes only the contextual CAD HUD. Nothing
  else in the product reads it.

## 6. Regions in Ready

Region selection is described in `PROFILE_REGIONS.md` §4–5.

- **Choosing.** With more than one region nothing is chosen and Extrude is
  disabled. The status line says how many regions were found. A tap toggles
  the region under the finger.
- **Showing the choice.** The chosen region is hatched, and a hole stays empty.
- **Previewing.** The preview updates on every change.

## 7. Not in this slice

- A per-control long-press tooltip.
- A HUD for the precision surface's own rows.
- Haptics.
- A landscape-specific HUD layout. The same row is clamped into the viewport.
