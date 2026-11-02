# SKETCH-NAVIGATOR-OUTCOME-R1 — DESIGN DECISIONS

## Root cause (proved, not assumed)

The navigator's state, the session's `viewFrame()` and the camera's math were
all correct on their own:
- `viewFrame()` applied the flip and the roll, and was right-handed (pinned by
  the existing `CADUXR1_06..16` checks).
- `CameraController::frameSketchView` looks along whatever normal it is given,
  with whatever up it is given.

The fault was the one adapter between them. JNI `beginSketchView()` framed the
camera on `frame()`, the AUTHORING frame, so `viewFrame()` had no production
caller. The proof is the CR-06 diagnostic on `8a797ff`: identical view
matrices after Flip, +90 and −90, while framing on `viewFrame()` gives
different ones.

A second, smaller inconsistency was latent: `backToEditing()` kept the flip
and roll, while Back to Sketch is specified (`CAD-UX-S1-C1`) to reinstall the
ALIGNED view. It was invisible only because the camera ignored the state; any
fix of the first fault would expose it.

## Options considered

| Option | What | Cost / risk | Verdict |
| --- | --- | --- | --- |
| A | Change the one JNI line, `frame()` → `viewFrame()` | Smallest. The behaviour sits in a file the host cannot build, so only the device could test it. | Correct but untestable on the host |
| **B** | Move the "what does the sketch camera look at" statement into the domain as one function, `frameCameraOnSketchView(CameraController&, const SketchSession&)`, which JNI calls | Same behaviour as A. One owner for the decision. The host self-tests drive the exact production call with a real `CameraController`. | **Chosen** |
| C | Rotate or flip the picture in the Android layer (a View transform over the SurfaceView) | Breaks the full-bleed viewport rule. Picking and overlays would need a second, duplicated transform. Two truths. | Rejected |
| D | Give `CameraController` its own roll/flip state | Duplicates the navigator's state in a second owner; they can drift. | Rejected |
| E | Keep the flip and roll across Back to Sketch / Edit Sketch | A durable-within-session policy is OD-09 option (b), which needs an OWNER decision. | Not done: **reset on re-entry** instead (the gate's "none" form) |

## What changed (the minimal coherent set)

1. **`forgeshape_sketch_session.{h,cpp}`**
   - `frameCameraOnSketchView`, new: frames the camera on `viewFrame()`.
   - `backToEditing()` resets flip and roll, as `begin()` and `cancel()`
     already did, so the navigator says "aligned" exactly when the camera is.
   - `setViewFlipped` returns a `CadStatus`:
     - `NotSketching` outside a session (was a silent no-op);
     - `InvalidWorkplane` for a face-supported sketch, the guard behind the
       Flip chip the shell already disables there (`SketchOrientationNavigatorView:199-201`).
       A refusal changes nothing.
2. **`forgeshape_jni.cpp`**
   - `beginSketchView()` calls `frameCameraOnSketchView`.
   - `sketchSetViewFlipped` returns the session's status. Its codes for an
     active world sketch (Ok) and no sketch (NotSketching) are unchanged.

Not changed:
- picking (`screenToSketch`): already plane-based and two-sided;
- `endSketchView` / the saved pose;
- the feature (Ready) view;
- Java;
- renderer, kernel, formats, `.gitattributes`, CI and build.

## Why it is safe

**Picking.** It intersects the AUTHORING plane and reads (u, v) off the
authoring axes, so it is the same answer from either side and any roll.
Proved for every plane × flip × roll on the host (`NAVCAM_06`) and by a real
drag on the device (`nav06`).

**No mirroring.** `viewFrame()` is right-handed. The camera's view matrix has
`det = +1` for every combination (`NAVCAM_04`). Flip shows the true back of
the plane: the eye is on the −n side (`NAVCAM_03`).

**Nothing true moves.** Only the camera is written. Proved on the host
(`NAVCAM_07`) and on the device (`nav06` truth string: entities, project
bytes, fingerprint, Undo/Redo, bodies).

**Per-frame cost: none.** `frameCameraOnSketchView` runs only when a view is
(re)installed: begin, a navigator act, Back to Sketch, Edit Sketch, a plane
change. It does a 3-vector copy plus at most 3 axis swaps. No render-loop or
picking path changed, so no comparative timing was taken.

## Held

- **OD-09 option (b):** flip and roll surviving Back to Sketch or Edit
  Sketch. Not implemented; reset-on-re-entry is the current behaviour.
- **Flip chip hit area:** 36.19 dp (CR-06-S6 F-01, historical). Reported,
  not changed in this stage.
