# The sketch orientation navigator (`SKETCH-UX-R1` C)

## What it is

A compact translucent control in the sketch's **upper trailing corner**,
present only while a sketch is open. It answers the two questions a user has
constantly while sketching — *which plane am I on, and which way up is it?* —
and changes both without leaving the drawing.

```
        XY · Along +Z · 0°          <- the current view, in words
       [ XY ] [ XZ ] [ YZ ]         <- the support plane
       [-90°] [ +Z ] [+90°]         <- the view: roll, normal, roll
```

The label above the controls states what IS; the controls below change it. On a
navigator both are wanted at once, so the flip chip carries the signed axis
alone (`+Z`, `−Z`) and the sentence lives in the label.

## The two kinds of state, and why they are different

**The support plane is AUTHORING TRUTH.** It says what `(u, v)` mean. It is in
`CadSketch`, it is in the `.forge` file, it is in every history step.

**The flip and the roll are PRESENTATION**, in the strongest sense this project
has:

- never persisted — no `.forge` byte, no checkpoint, no fingerprint, no history
  step;
- incapable of moving one authored coordinate — `CADUXR1-13` draws a line,
  rotates four times and compares the entity bit for bit;
- incapable of mirroring anything — `viewFrame()` derives the camera's frame
  from the authoring frame and is **always right-handed**, which the native
  suite asserts as `u × v = n` on every state it produces.

`SketchSession::viewFrame()` is the whole derivation:

```
if flipped:   n = -n,  v = -v          (one in-plane axis reverses with the
                                        normal, or the basis would be left-
                                        handed and the drawing would mirror)
repeat turns: u, v  =  -v, u           (+1 turns the VIEW clockwise: what was
                                        at screen-up ends up at screen-left, so
                                        the DRAWING appears to turn CCW)
```

`beginSketchView` frames the camera on that derived frame through the existing
`CameraController::frameSketchView` — exactly along the normal, no pitch-clamp
approximation, no gimbal.

## Six orientations

Three planes × two normals. XY along ±Z, XZ along ±Y, YZ along ±X, each
reachable in at most two taps, each exact.

## Switching the support plane

| State | Answer |
| --- | --- |
| Empty, world-supported | allowed; the authoring frame is re-based to the new plane |
| Has geometry | **refused by name** (`SketchNotEmpty`) |
| Face-supported | **refused by name** (`InvalidWorkplane`) |

The rule that makes the first row safe is the one that forbids the second:
authored numbers only ever mean one plane, so the plane may change while there
are no numbers and never afterwards. **Nothing is remapped, projected or
reinterpreted** — the option this stage explicitly did not take.

A face-supported sketch refuses outright because its support is a `TopoRef` it
follows; changing it to a world plane would detach it from its producer, which
takes a Cancel and a new support choice.

The chips are also **disabled** when the plane cannot change — a control that
cannot succeed is not offered — but the domain guard stays regardless:
removing a control is not removing a guard, and `CADUXR1-15` calls
`sketchSetSupportPlane` directly to prove the refusal is below JNI.

## Placement

The navigator is inset from the trailing edge by the right contextual host's
own width plus a gap, so it stands on the **drawing** and never on the **Tool
Rail**. A surface may stand on the model; it may never partially cover another
live control, and z-order is not a fix because the control underneath is still
taking touches. `CADUXR1-08` asserts non-overlap by measured bounds.

## Holding no state

Every control reads `sketchViewState` on each refresh and sends an act back.
There is no plane, flip or roll remembered in Java. A refused act re-reads and
goes on showing what IS true rather than what was tapped.

## The accessibility fallback

The by-name plane list (`sketch_plane_by_name`) is unchanged and was not
removed. The navigator is the normal path, not the only one.
