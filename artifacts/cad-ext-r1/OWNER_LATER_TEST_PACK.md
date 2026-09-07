# OWNER LATER — `CAD-EXT-R1` (One Side / Symmetric / Two Sides)

**Task:** `CAD-EXT-R1` · **Result:** technically complete, owner-later ready.
Nothing below is accepted. This is a bounded pack of what an owner should look
at with the build in hand, and none of it blocks the stage.

The DATA CONTRACT is settled and proved (36 fixtures, two independent
implementations, byte-identical). Everything in this pack is **presentation and
wording**, which is exactly the half a test cannot answer.

---

## 1. Is the selector understandable without instruction?

Open a sketch, Finish Sketch, and read the three chips **without** being told
what they do.

* Does **One Side / Symmetric / Two Sides** say enough on its own, or does one
  of the three need different wording?
* Is the selector in the right place in the cluster — first, before the value it
  qualifies — or does the number want to be read first?
* Three chips plus a value plus a badge is a wide cluster. On a phone in
  portrait, does it stay legible, or does the selector want to become a single
  cycling chip?

## 2. `Each side` for Symmetric

The stored value is the distance **per side**, and the label says
`Each side`.

* Is that unambiguous at a glance, or does the number read as the total?
* Would an owner ever prefer a **total-length** presentation (with a "per side"
  secondary reading)? Note that this is a PRESENTATION question only: the
  durable value stays the distance per side whatever is shown, and changing the
  display would move no `.forge` byte. Deciding this later costs a UI change and
  nothing else — which is precisely why the file stores the side distance.
* Is `0.75 m` each side reading as a 1.5 m solid confusing at the moment of the
  first Symmetric extrusion?

## 3. Dragging two sides, and occlusion

* With two arrows out of one profile, is it obvious which one a finger will
  take? The primary side wins a tie deterministically — does that feel right, or
  does the nearer one want to win?
* At a shallow viewing angle the two arrows nearly overlap in screen space. Try
  it: is the grab still predictable, or does the view want nudging first?
* Does the second value cluster ever sit on top of the first one? It does when
  both distances are small and the camera is close.
* A Two Sides side may be **zero**; its arrow then has no length and its value
  stands at the base. Is that readable, or should a zero side be refused
  outright?

## 4. `A` / `B` versus `+` / `−`

The two sides are read as **Side A** (along the sketch normal) and **Side B**
(against it).

* Is `A`/`B` better than `+N`/`−N` on a phone, where the normal is not labelled
  anywhere in the viewport?
* Would `Up`/`Down`, `Front`/`Back` or the plane's own axis letters read better
  for a world-plane sketch? Note they would be wrong for a face-supported one.

## 5. Layout, orientation and hardware

* **Portrait**, **landscape** and a **tablet** width: does the wider cluster
  still clear the Tool Rail and the trailing host in all three?
* **Handedness** Left and Right: the cluster is canvas-anchored and is not
  mirrored, which is correct by rule — confirm it does not collide with the
  rail zone on the left setting.
* All **five palettes**: the selector's active chip is drawn by
  `setChipActive`, the same treatment every chip in the product uses. Confirm
  the active/inactive contrast reads in Warm Light and Cool Light as well as in
  the three dark ones.

## 6. Support classes

* A **world-plane** sketch on XY, XZ and YZ.
* A **face-supported** sketch, on both a cap and a side. A Symmetric extrusion
  on a face reaches back THROUGH the producer — is that surprising in the
  viewport, and is the preview readable while it does?

## 7. Camera

* **Zoom near and far**: the cluster scale is bounded to `[0.80, 1.60]` exactly
  as before. Does the second cluster attenuate with the first convincingly?
* Does anything **jump** when a side crosses zero, or when a mode change moves
  both caps at once?

## 8. Edit Sketch re-entry

* Apply a Two Sides body, Edit Sketch, Finish: the extent comes back. Is the
  return to the preview view as legible as it is for a One Side body, now that
  the solid straddles the sketch plane?

## 9. Where an `Add` / `Cut` badge would eventually go

Not implemented, and deliberately not drawn. The **New Body** badge currently
states the operation. When an operation choice exists, does it belong:

* beside the extent selector as a second chip row, or
* replacing the badge in place, or
* on the toolbar with Extrude?

Worth an opinion now, because it changes whether the cluster can afford a fifth
control.

---

**Do not write OWNER ACCEPTED here.** This pack records what to look at, not
what was decided.
