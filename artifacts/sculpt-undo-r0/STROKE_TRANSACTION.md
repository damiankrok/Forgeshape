# STROKE_TRANSACTION — where a history entry begins and ends

## The boundary is the stroke, not the pointer event

A sculpt stroke is already a single coherent transaction in the domain, and had
been since the brush existed. `SCULPT-UNDO-R0` did not invent a transaction; it
recorded the one that was there.

```
  pointer DOWN on the sculpt mesh
      probe only  (hitsSculptMesh)      -> no vertex written
  pointer MOVE past the arm threshold
      SculptStroke::begin               -> affected set + weights + BASE POSITIONS
                                        -> beganWithEdits_ = mesh.hasEdits()
  pointer MOVE  x N                     -> update(): writes positions, one
                                           advanceRevision() per batch
  pointer UP  (or CANCEL, or a 2nd finger, or leaving Sculpt)
      SculptSession::recordActiveStroke -> buildDelta() -> ONE entry
      SculptStroke::end()/cancel()      -> affected set cleared
```

## Why the before-positions are free

`SculptStroke::begin` already captured every affected vertex's `basePosition`,
because all four tools need it: Grab recomputes `base + delta * weight` every
move (which is what makes it idempotent in its own displacement rather than
drifting with the event rate), and Clay reads the base normal captured beside it.
So the BEFORE side of a delta costs nothing new. The AFTER side is read off the
mesh at close.

This is also why an entry must be built BEFORE `end()`/`cancel()`: those clear
the affected set, which is where the before-positions live.

## The affected set is the whole story

The set is captured once at begin and fixed for the stroke's life — a vertex
cannot wander into or out of the brush mid-stroke. All four `apply*` rules were
read to confirm they write only vertices in `affected_`:

| Tool | Writes | Reads outside the set |
| --- | --- | --- |
| Grab | `affected_` only | no |
| Clay | `affected_` only | no |
| Inflate | `affected_` only | current normals (read) |
| Smooth | `affected_` only | neighbour POSITIONS (read, Jacobi snapshot) |

Smooth reads neighbours that may be outside the set but never writes one, so a
delta over `affected_` is complete for every tool.

## One entry, however many events

`buildDelta` runs once per stroke, over the whole affected set. Fifty pointer
moves produce one entry, and the domain proves it by count rather than by
inspection: `SCUNDO_03_and_four_moves_did_not_make_four_entries` drives four
moves and asserts `undoDepth() == 1`, alongside
`SCUNDO_13_the_revision_advanced_more_than_once_inside_the_stroke`, which proves
those moves really were separate batches that each minted a revision. One
transaction, several publications — exactly as a gizmo drag already behaved.

## Only vertices that moved

`buildDelta` keeps a captured vertex only when its current position differs from
its base position, bit-for-bit. That drops:

- low-weight rim vertices a short stroke never actually displaced
- an isolated vertex Smooth could not average
- every vertex of a stroke that began and got no Move

and it is what makes the no-op case fall out rather than needing a special case.

## The no-op stroke records nothing

If nothing moved, `buildDelta` returns false and nothing is recorded — no entry,
and the redo stack is left exactly as it was, because nothing changed. Two
domain checks cover it:

- `SCUNDO_04_a_stroke_that_moved_nothing_records_no_entry` — begin, then end
- `SCUNDO_04_and_a_cancel_with_no_movement_records_nothing_either`

A stroke whose ray misses never begins at all, so it cannot reach here.

## A cancelled stroke that DID move is recorded — deliberately

This is the one place the implementation is worth stating plainly, because the
brief's §2 and its `SCUNDO-04` row can be read two ways.

The product rule, unchanged since the brush existed and stated in
`ARCHITECTURE.md`, is that **a cancelled stroke keeps the positions it already
wrote**: "a stroke that stopped, not one that is rolled back". Given that the
deformation STANDS and the user can see it, leaving it out of the history would
make it the one deformation in the product that cannot be taken back — which is
worse than either reading of the brief.

So `cancelStroke` records the same single entry `endStroke` does. What the brief
forbids — "a cancelled/invalid stroke must not leave a partial history entry" —
is guaranteed structurally instead: an entry is built in ONE pass from the whole
affected set, or not at all. There is no code path that can write half an entry.

`SCUNDO-04` is therefore tested as the case both readings agree on: a cancel that
moved nothing records nothing, and so does an end that moved nothing. See
`DEVIATIONS` in `INDEX.md`.

## The four ways a stroke can close

All four go through `SculptSession`, and all four record:

| Event | Call | Records |
| --- | --- | --- |
| pointer UP | `endStroke()` | yes, if anything moved |
| CANCEL from the window | `cancelStroke()` | yes, if anything moved |
| a second finger arrives | `endStroke()` | yes, if anything moved |
| leaving Sculpt mid-gesture | `cancelStroke()` | yes, if anything moved |

`freezeToSculpt`, `enterConstruction` and `enterSculpt` were changed to route
their `stroke_.cancel()` through `cancelStroke()` for exactly this reason. In
`freezeToSculpt` the recording happens BEFORE the freeze and against the OLD
mesh, which is correct: if the freeze then fails, that mesh keeps the stroke's
deformation and the user must still be able to take it back. If it succeeds, the
history is cleared immediately afterwards anyway.

## A stroke in progress blocks a step

`undoStroke`/`redoStroke` return `StrokeActive` while `stroke_.active()`, and
`canUndoSculpt`/`canRedoSculpt` report false, so the chrome control is disabled
rather than refusing. Applying an entry under a finger that is still writing
positions would apply a state the stroke is about to overwrite. The condition is
self-clearing: the finger lifts, the stroke commits, and the step is available.
Proved by `SCUNDO_24_undo_is_refused_while_a_stroke_is_active` and
`SCUNDO_24_and_the_refusal_moved_no_vertex`.

## A new stroke ends the redo branch

`SculptHistory::record` clears the redo stack — including for a stroke too large
to retain, because the geometry moved either way and those entries describe a
future that no longer follows from the present. `SCUNDO-08` and `E2E-SCUNDO-04`.
