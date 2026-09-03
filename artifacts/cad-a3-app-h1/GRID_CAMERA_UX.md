# Adaptive grid and the exact sketch camera (`CAD-A3` B3/J1)

## Exact sketch camera

`CameraController::frameSketchView(origin, u, v, n)` installs the sketch frame
DIRECTLY: eye on the frame normal, up = frame V, orthographic. The view is
exactly normal to the plane or face -- including straight down, which the orbit
look-at (world-up) could not express without gimbal lock, and which the old
~87-degree pitch clamp only approximated. Pan slides the target along the frame
axes; orbit is inert; the user's pose is captured and restored on finish/cancel.
Presentation only: cleared by restorePose, frameWorkplane or resetCamera.
Verified native `CADA3-12`: the view looks exactly down the frame normal, is
orthographic, and frame U is screen-right / V screen-up.

## Adaptive grid

The fixed 0.25 m grid is replaced by `adaptiveSketchGridStep(worldPerPixel)`: the
smallest nice 1/2/5 x 10^k metres whose on-screen spacing is at least ~26 px, so
a grid square stays readable at every zoom. The grid is drawn as a bounded number
of lines each side of the frame origin (never a fixed world extent), so vertex
count stays bounded at any zoom. The step is sampled from the camera at
pointer-down and is fixed for the drag; it is NEVER persisted, and a typed
numeric value is NEVER re-snapped to it. Verified native `CADA3-44/45`: the
steps are nice, monotonic with zoom and bounded, and a fresh session authors on
the producer frame; and the CAD-R0 drag tests now assert corners on exact
multiples of the current step.
