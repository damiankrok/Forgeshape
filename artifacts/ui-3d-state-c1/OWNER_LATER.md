# UI-3D-STATE-C1 — OWNER LATER

Only what automation cannot judge. Everything mechanical — does a surface show,
does it hide, does it stand on its anchor, does it stay inside the viewport, does
it follow the camera and the body — is asserted by `Ui3dStateCorrectionTest` and
measured by the strict re-run of `Ui3dStateAuditTest`, and is **not** repeated
here as a manual pack.

**No visual acceptance is claimed.** The correction proves attachment, not taste.

---

### OL-C1-01 — do the dimension numbers now feel well placed?

They are mechanically on the midpoint of each dimension line, which is where the
annotation says they belong. Whether sitting exactly on the line reads well —
against sitting just clear of it, the way a draughtsman would break the line for
the number — is a drawing-convention judgement, not a measurement.

*How to look:* select a Construction Body → Transform → **Dimensions**. Orbit
slowly. Watch whether the three numbers read as part of the drawing or as three
chips lying on top of it.

### OL-C1-02 — is the viewport clamp margin right?

A label or the extrude cluster whose anchor falls outside the viewport is held at
the nearest edge, flush with it. It now reaches that edge where the window
requires rather than an inset early, which is the fix — but flush against the
edge may read as too tight, and a small margin may read better.

*How to look:* open Dimensions and orbit until one number's anchor leaves the
screen; then stage an extrusion and zoom out until the cluster reaches an edge.
There is no user setting for this, by design; changing it means changing the one
authored bound.

### OL-C1-03 — is CAD chrome overlap pleasant on a real device?

The extrude cluster, the second-side value and the `Edit Sketch` chip are now
where their anchors are rather than one inset below, which changes what overlaps
what. Emulator geometry is not a phone in a hand.

*How to look:* on a real phone and, if available, a tablet, stage a **Two Sides**
extrusion and drag each arrow in turn. Watch whether the two values crowd each
other at close camera distances.

### OL-C1-04 — the five palettes and both handednesses

The correction is geometry only and touches no colour, no size and no edge
anchoring. It is worth one look per palette and one in Left-handed layout to
confirm nothing anchored moved with the rail zone, because that is the one
combination automation did not exercise.

---

**Not owner questions, and deliberately not in this pack:** whether a surface
appears or disappears in a given state, how far it stands from its anchor, and
whether it survives a body switch, a mode change or a camera move. All of that is
mechanical and is asserted.
