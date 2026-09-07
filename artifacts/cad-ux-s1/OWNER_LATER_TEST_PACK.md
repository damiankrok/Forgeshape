# OWNER LATER — `CAD-UX-S1` (canvas-first Extrude, retained Sketch access)

**Nothing in this document is OWNER ACCEPTED.** The technical side of the stage
is proved by the native `CADUXS1-*` cases, the device `CadCanvasExtrudeTest` and
the corpus verification; everything below is a look-and-feel judgement no
emulator can make, and every number named as *provisional* is a one-line change
if a different feel is wanted.

---

## 1. What to open, and where the controls are

1. Home → **New Project → CAD**, or an open project → Objects capsule → **Add
   Primitive → New Sketch**.
2. Draw a rectangle or a circle, then **Finish Sketch**.
3. The **canvas extrude cluster** appears at the middle of the arrow, standing
   on the profile: the exact **depth**, a direct **Flip**, and a **New Body**
   badge. The **arrow** itself is drawn by the renderer along the extrusion
   normal, its shaft the depth and its head the control.
4. After **Extrude**, one **Edit Sketch** chip stands on the committed body's
   own sketch.

---

## 2. The five provisional constants, and the arithmetic behind each

These are chosen with stated reasoning, not by eye, and none is approved.

| Constant | Value | Why this value | Where |
| --- | --- | --- | --- |
| `kCadExtrudeControlWorldMeters` (`W`) | **0.45 m** | The cluster is `S_ref` pixels wide at the sketch's own default framing (≈9 m over a 2400 px phone, ≈0.00375 m/px), so a freshly opened sketch shows it at exactly its authored size. | `forgeshape_cad_extrude_tool.h` |
| `kCadExtrudeControlReferencePixels` (`S_ref`) | **120 px** | A little under the gizmo's own screen budget (its rotate rings are 156 reference units across at the default visual scale), so the extrude cluster never dominates the instrument bodies are placed with. | same |
| `kCadExtrudeControlMinScale` | **0.80** | **Arithmetic, not taste.** The cluster's controls are authored at `R.dimen.cad_canvas_control` = 60 dp, and 60 × 0.80 is exactly the 48 dp interactive floor. | same |
| `kCadExtrudeControlMaxScale` | **1.60** | Makes the whole visible range a factor of two — enough that the attenuation reads, bounded enough that a close camera cannot bury the profile. | same |
| `kCadExtrudeGrabRadiusUnits` | **24 reference units** | A 48-unit diameter, the interactive floor, and deliberately **not** scaled by the visual multiplier — exactly the reason the gizmo's own hit corridors are not scaled by its visual-size preference. | same |

Arrow proportions (head 0.42 of the control size, half-width 0.16, six barbs,
base tick 0.18) are provisional in the same sense.

**If the owner wants a different feel**, the two most likely knobs are
`kCadExtrudeControlMaxScale` (how large it gets when you come in close) and `W`
(what distance reads as "authored size"). Lowering `kCadExtrudeControlMinScale`
requires raising `R.dimen.cad_canvas_control` in step, or a live control falls
under 48 dp — the native case
`CADUXS1_09_d_the_minimum_scale_keeps_a_60dp_control_at_the_48dp_floor` asserts
that relation so it cannot drift silently.

---

## 3. What only a human can judge

### The arrow

- Does the arrow read as **"the solid grows this way, this far"**, or as
  geometry belonging to the drawing?
- Is the head large enough to grab confidently one-handed at mid zoom, and small
  enough not to cover the profile at close zoom?
- Do the six barbs read as a cone from an oblique view, or as clutter?
- Does the base tick help ("the measurement starts here") or read as an extra
  sketch entity?
- At a shallow viewing angle — nearly along the extrusion axis — the drag holds
  its last good value rather than jumping. **Does holding feel broken, or does
  it feel correct?**

### The camera-attached scaling (§4.4 of the prompt, the deliberate departure)

- Pull the camera back and come in again. Does the cluster **shrinking and
  growing** read as belonging to the work, or as an instrument that lost its
  grip?
- At the two clamps, is the saturation smooth, or is there a visible "pumping"
  as the control stops following the camera?
- Compare directly against the transform gizmo, which is deliberately
  **constant screen size**. Do two instruments with two different scaling rules
  in the same product read as inconsistent, or as correctly different?

### Flip

- Is **Flip** discoverable without instruction? Does the word say what happens?
- Is one tap next to the geometry a real improvement over the two direction
  chips in the precision panel — and should those chips now go, or stay as the
  accessible route?
- The status line says which side it flipped to. Does that help or is it noise?

### The exact value

- Is the depth legible at all five palettes, at both grounds, over the grid and
  over the profile?
- Tapping the value opens a compact field. With the **IME up**, does the field
  still sit clear of the profile it measures, in portrait *and* landscape?
- Should the field open on a tap, or should the reading always be a field?

### The `New Body` badge

- Does it read as **information** ("this makes a new body") rather than as a
  control the user should be able to change?
- Is it reassuring, or does it raise the question "what else could it be?" —
  which would be an argument for removing it until Add and Cut exist.

### Retained Sketch access

- After Extrude, is the **Edit Sketch** chip on the body findable without being
  told about it?
- Does it make clear that the sketch **survived** the extrusion, which is the
  whole point of it being there?
- Does it get in the way when the user is done with the body and just wants to
  move it? (It stands on the body's sketch plane, so it can overlap the body.)
- Is one chip right, or should the retained sketch be listed somewhere instead?

### Occlusion and overlap (the whole list, none of it settled)

Check every one of these with the cluster live:

- the **orientation navigator** in the sketch's upper trailing corner;
- the **Tool Rail** (the cluster may stand on the drawing; it may never stand on
  the rail);
- the **trailing contextual host** and the **precision surface** when open;
- the **history capsule**;
- the **selection outline** of a nearby body;
- the **IME**, in both orientations;
- **left handedness**, where the rail zone and the trailing host swap edges.

### Both supports

- A **world-plane** sketch (XY, XZ, YZ) — does the arrow point the way the plane
  name suggests?
- A **face-supported** sketch on a committed CAD body's planar face — does the
  arrow read correctly standing on another body, and does the cluster survive
  the producer being nearby?

### Palettes and grounds

All five: Warm Graphite, Neutral Charcoal, Light Charcoal, Warm Light, Cool
Light. The arrow is drawn in the sketch overlay's `Entities` weight, so it takes
the same per-ground treatment the extrude preview already does — check that it
is distinguishable from the preview edges it sits among.

---

## 4. Screenshots to take

Nine, on the isolated AVD, with the log ring buffer at 64M:

1. the cluster at **near**, **mid** and **far** zoom (three shots — the scaling
   question, and the two clamps);
2. mid-drag, with the arrow held (the emphasis weight);
3. the numeric field open with the IME up;
4. Flipped, so the arrow points the other way;
5. the retained **Edit Sketch** chip on a committed body;
6. the whole workspace with the Tool Rail, the navigator and the trailing host
   visible, proving nothing covers a live control;
7. one light ground and one dark ground.

---

## 5. Explicitly not in this stage, and not to be judged as missing

`Add`, `Cut`, `Symmetric`, `Two Sides / Asymmetric A+B`, any boolean, any
feature list, `Revolve`, `Intersect`, a Hole feature, a constraint solver,
`CADB` v4 or v5, and multi-feature reuse of one sketch. The **retained-sketch
access** delivered here is *re-edit*, and it is not the same thing as one sketch
driving several downstream features — that stays a later stage.
