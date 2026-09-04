# Edit Sketch (`SKETCH-UX-R1` F)

## Why it had to exist

The dimension edit is the point of this stage, and without Edit Sketch it would
have worked **only before the first Extrude** — a feature the user gets one
chance at, on a body that does not exist yet. A CAD Body's truth is its sketch;
reopening that sketch is the only honest way to change it.

## The session knows which body it is editing

```cpp
CadStatus beginEdit(ObjectId bodyId, const CadBodyState& state,
                    const SketchFrame& worldFrame);
ObjectId  editingBodyId() const;      // kNoObject when authoring a new body
CadStatus commitEdit(ConstructionScene&, ConstructionHistory&);
```

`editingBodyId_` is the ONE answer to "is this an edit?" — no Java flag mirrors
it. That is also what lets **one Extrude control** serve both: `onExtrudeRequested`
asks native which kind of session is open and routes accordingly, so there is
no second code path and no shell state deciding what a button means.

`beginEdit` validates the state before staging it: a session opened over a state
this build could not regenerate could never be finished, and would strand the
user in a sketch with no way forward.

## Staged means staged

The session takes a **copy**. The body keeps its own state, its own published
mesh and its own place in the scene for the whole edit.

- `CADUXR1-32` changes the staged rectangle to 5 m wide and asserts the BODY
  still reads 2 m.
- `CADUXR1-33` then cancels and asserts the body is untouched and the history
  depth has not moved. `cancel` is the ordinary sketch cancel: there is nothing
  special to undo, because nothing was written.

## Finish is one transaction

`commitEdit`:

1. refuses unless Ready with a chosen profile;
2. refuses if a Construction edit is already open;
3. re-finds the body — it may have been deleted, or the project replaced;
4. validates the whole candidate state;
5. **checks every dependent** (below);
6. opens ONE `ScopedConstructionEdit`, applies the state through
   `CadBody::applyState` (itself all-or-nothing), and republishes.

One Finish is **exactly one Undo**. Undo restores the entire previous sketch;
Redo restores the edited one. No second body is ever created — `CADUXR1-34`
asserts the body count is still one.

## Dependents

A dependent's world placement is **derived** (`resolveWorldModel` composes it
from the producer every snapshot), so a supported edit needs no push at all:
the dependent simply resolves against the new producer state on the next frame.
`CADUXR1-36` moves a producer's rectangle from 2 m to 3 m wide with a dependent
on its far cap and asserts the dependent still resolves.

An edit that would **remove** a face a dependent stands on is a different
matter, and it is **refused by name** (`DependentFaceLost`) — checked against
the CANDIDATE before anything is written, so the refusal costs nothing:

```cpp
for (dependentId : scene.cadDependentsOf(editingBodyId_)) {
    if (cadTopologySignature(candidate) != ref.lineageToken
        || resolveCadFace(candidate, ref.face, &face) != Ok || !face.eligible) {
        return fail(CadStatus::DependentFaceLost);
    }
}
```

This is exactly the rule Delete already follows (`RefusedHasDependents`): the
dependency is never cascaded, never retargeted to the nearest surviving face,
and never silently broken. A size-only edit keeps the face SET and so keeps
every reference valid; replacing a rectangle profile with a circle does not, and
`CADUXR1-36` proves both halves.

## What it is not

There is **no feature tree**. A CAD Body has exactly one sketch and one
extrusion, so "edit the sketch" needs no browser to say which — one control on
the body's own precision surface, beside the fields that edit its sizes.
