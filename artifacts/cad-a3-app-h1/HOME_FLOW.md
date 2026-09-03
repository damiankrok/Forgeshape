# APP-H1 Home / project entry -- NOT IMPLEMENTED THIS PASS

Honest status: the APP-H1 Home surface (cold-launch Home with Open File / New
Project, and New Project into CAD / Sculpt) was NOT implemented in this pass,
and the "New CAD enters an EMPTY CAD project" bootstrap was not attempted.

## Why

New CAD requires a scene with NO bodies during support selection. ForgeShape's
`ConstructionScene` has a load-bearing "no empty project" invariant:
`activeBody()` returns a reference and falls back to `bodies_.front()`, and many
accessors assume at least one body exists. Making the scene safely empty during a
New-CAD bootstrap touches that invariant across many call sites and is high-risk
for an otherwise-green build. Rather than a rushed change that could destabilise
the app lifecycle and the ~20 EditorWorkspace instrumented test classes, this was
deferred and is flagged for the coordinator.

## What DOES work for project entry today

- The existing start chooser (Construction/Sculpt) and the existing SAF Open /
  Save / Save Copy / Import GLB / Export GLB paths are unchanged and green.
- A CAD Body is created from an existing project via New Sketch, now with the
  viewport-first spatial support chooser (world planes AND CAD faces).

## What a future APP-H1 pass needs to decide

1. Whether New CAD holds a genuinely empty scene (with the invariant relaxed by
   guarded accessors) or seeds a transient bootstrap body.
2. The Home surface layout and the New Project chooser, reusing the start
   chooser's structure.
3. The dirty-state Save/Discard/Cancel guard on New/Open from a dirty editor.
