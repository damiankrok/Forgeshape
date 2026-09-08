# UI-3D-STATE-C2 — OWNER LATER

`UI3D-F-005` is closed on the technical question it was raised about: the
`Dimension` style has an explicit, deterministic, visible mapping, and both
producers' geometry is measured drawn on the device. **None of the questions
below blocks that, and none of them is marked accepted.**

What was chosen, so the questions have something concrete to be about: the
annotation draws at `kGizmoAxisAlpha × 0.85` (0.8075) in the gizmo's existing
held-handle amber — (1.00, 0.84, 0.28) on the three dark grounds, (0.86, 0.56,
0.02) on the two light ones — one pixel wide, like every other line the gizmo
pipeline draws.

1. **Is the colour and alpha readable on all five palettes?** Measured only on
   Warm Graphite. The light grounds take the saturated palette, which is darker
   and more saturated by design, but no contrast measurement was made for this
   mark specifically — `artifacts/ui-pref-r1/CONTRAST.md` covers the gizmo
   handles, not a 1 px annotation over a shaded body.
2. **Line thickness on a phone, a tablet and a high-density display.** The
   overlay is a one-pixel line list by renderer contract. At 2.625 density that
   is a hairline; on a lower-density tablet it is a hairline of a different
   physical size. Whether a technical annotation should be one pixel at all is
   an owner question, and answering it "yes, but wider" is a renderer change
   this correction deliberately did not make.
3. **Does the annotation compete with the selection outline or the gizmo?** It
   now shares the gizmo's held-handle amber, which was chosen so no NEW hue
   entered the viewport — but the selection outline is also amber, and in
   Dimensions the gizmo is withdrawn while in a sketch there is no gizmo, so the
   overlap was never exercised in one frame. The measurements here switch the
   outline off precisely because it is the same colour family.
4. **Readability on the light and dark sides of a model, under both shading
   models.** The Stage 020M leaders cross the body they measure and are not
   depth-tested against it, so part of a leader stands on a lit face and part on
   a shadowed one. Not evaluated under Studio Solid.
5. **Sketch against Body Dimensions consistency.** Both now take the same style
   and therefore the same weight, which is the intent. Whether a sketch
   annotation and a body annotation SHOULD look identical, given one is
   authored-truth adjacent and the other is a placement readout, was not asked.
6. **Portrait, landscape and handedness.** Measured in compact portrait only.
   The annotation is world-space and carries no chrome inset, so no placement
   question arises; the readability questions above do repeat per window.

Two related observations, recorded rather than acted on:

- **The active axis is now distinguished by colour as well as by the field that
  opened.** With three leaders standing and one warmer, "which axis is this
  number for" is answered by the drawing. That is the behaviour the style was
  always intended to give and is now visible for the first time; whether the
  contrast between active and neutral is strong enough is question 1 again.
- **The annotation is drawn over the body rather than occluded by it**, exactly
  as it was before this correction — the overlay range has never been depth
  tested against scene geometry. Nothing here changed that, and whether a
  dimension leader should be hidden behind the solid is a separate product
  decision.
