# The owner's failure, reproduced and corrected

## What the owner saw

The owner opened ForgeShape and rejected the first screen: **Home was a large
rounded card floating in the middle of the viewport**, over a partial scrim.
They then rejected the first CAD step: **New Project → CAD put three floating
squares into an otherwise empty 3D world** and asked which one to sketch on.

## Why it looked like that

Both were deliberate decisions of the previous pass, and both were wrong for
the same reason.

`ChooserSurfaceView` — the shape Home, New Project and the unsaved-changes
question all shared — is a partial scrim with one raised panel centred on it.
Its own comment said why: *"the Vulkan viewport keeps rendering behind it, so
the first thing ForgeShape shows is still the viewport."* That reasoning holds
for a question asked **over a project the user can see**. It does not hold for
Home, because **at Home there is no project** — the scene is deliberately empty
(`APP-H1`). So the thing showing through the scrim was nothing at all, and a
card floating over an empty viewport reads exactly like a dialog stranded over
a broken editor. The composition was honest about the architecture and
dishonest about the product.

The plane chooser had the mirror-image problem. Picking a plane **in the
viewport** is the right way to choose a support when there is something in the
viewport to relate it to — which is true of a later New Sketch, where real
bodies with real faces are on screen. For the FIRST sketch of a brand-new
project the world is empty, so the three squares floated in nothing, at an
arbitrary size, and the question they asked had no spatial context to answer it
with.

## What was changed

| The owner's point | The correction |
| --- | --- |
| Home is a popup/card | `StartPageView`: full-window, opaque, product mark and name at the top, actions as full-width rows. `ChooserSurfaceView` stays for the two questions asked over a live project. |
| New Project should be part of that page | It is the next **page**, with `Back` at its foot, and it REPLACES Home rather than floating over it. |
| The first sketch should not begin by tapping floating planes | `New Project → CAD` opens a sketch directly, XY along +Z, exact orthographic, grid filling the viewport. |
| Plane/view selection belongs in a top-right navigator | `SketchOrientationNavigatorView`: three planes, a normal flip, ±90° view rotation, and a line that always names the current view. |
| Line, Curve/Arc and Spline are wanted | Arc and Spline are durable sketch entities on the Tool Rail beside the five that were there. |
| A selected line should show its length like a technical drawing | Extension lines, a dimension line, end ticks and the length beside them. |
| That length must be typeable | Tap the number, type an exact value, Apply. P0 fixed, direction preserved. |

## What was NOT changed, and why

The **spatial support chooser is unchanged and still the normal path for a
later New Sketch** (`UI-OWNER-46`). The owner's objection was to the FIRST
sketch beginning that way, and that is the case where the viewport is empty.
Removing face picking would have removed the whole of `CAD-A3`.

The unsaved-changes and recovery questions are **still scrim modals**. Each is
asked over a project the user can see and is about to affect, which is the one
moment a modal is the honest shape.

No aesthetic approval is claimed by any of this. The frames in
`VISUAL_EVIDENCE_C2.md` are for the owner to judge.
