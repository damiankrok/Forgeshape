# The line dimension, and what a typed length means (`SKETCH-UX-R1` E)

## Two halves, drawn by two layers, for one reason each

**The annotation** — extension lines, a dimension line, a slash tick at each
end — is geometry-shaped and belongs in the same space as the stroke it
measures, so it is built by `SketchSession::buildOverlay` into its own
`SketchOverlayStyle::Dimension` range of the sketch overlay.

**The number** is not geometry-shaped. It has to stay legible at any zoom,
upright whatever the view rotation, and — the whole point — **editable**. So it
is a small chrome view (`SketchDimensionLabelView`) positioned over the viewport
at the anchor native reports.

## It is an interaction overlay, and structurally so

Neither half is project geometry:

- it is never exported to GLB — the exporter re-evaluates `generateCadMesh`,
  reads a Frozen Sculpt Mesh or reads an imported body's arrays, and the
  overlay is none of those;
- it never reaches a `.forge` byte — the codec stores `CadBodyState`, which has
  no annotation in it;
- it mints no revision and appears in no snapshot;
- nothing downstream can read a dimension back out of it, because nothing
  downstream is given it.

## The proportions

In reference units (dp), so the annotation reads the same at any zoom:

| | units |
| --- | --- |
| dimension line offset from the stroke | 30 |
| extension line gap from the endpoint | 6 |
| extension line overshoot past the dimension line | 8 |
| end tick half-length (a 45° slash) | 7 |

The perpendicular is the line's direction turned a quarter turn
counter-clockwise — deterministic, so the annotation does not jump to the other
side when the line is redrawn the other way round. The label anchor is the
dimension line's midpoint, which `CADUXR1-17` asserts is exactly the offset
distance from the stroke's midpoint.

Ticks rather than filled arrowheads: the draughting convention, and it reads
better at phone sizes.

## The edit

```
P0' = P0                                  the first endpoint does not move
d   = normalize(P1 - P0)                  the direction does not change
P1' = P0 + d * newLength
```

**That is the whole rule.** Deliberately not a constraint solver:

- no neighbouring entity moves;
- no angle is preserved for anything else;
- nothing is re-snapped — the grid is a drawing aid and this field is the way
  around it.

## When the edit opens a profile

It often will: shortening one line of a closed chain opens the chain. The
product's answer is to **say so and refuse to extrude**, not to drag the rest of
the sketch along:

- the edit itself is allowed — it is a legitimate intermediate edit state;
- `finish()` then refuses `OpenProfile` by name and stays in Editing;
- the sketch is still fully editable, so the user fixes it or takes it back.

`CADUXR1-24` drives exactly this journey through the real chrome.

## Refusals

| Input | Answer |
| --- | --- |
| `0` | refused (`ZeroLengthLine`) — zero is not a shorter line |
| negative | refused — "make it −40 mm" is a different question, not a length |
| NaN / ∞ | refused (`NonFinite`) |
| beyond the sketch range | refused |
| not a number at all | reported by name from the shell; nothing submitted |

**A refused value leaves the editor open** with the text still in it, so the
number can be corrected without hunting for the label again. An accepted one
closes it. `CADUXR1-23` asserts both, and that no authored coordinate moved
under any of the four bad inputs.

## Only a straight Line

`selectedLineLength` returns false for every other selection, which is the
whole condition for the annotation being drawn at all. A curve has no single
length to type, and an arc's length is derived from its three points rather
than authored.
