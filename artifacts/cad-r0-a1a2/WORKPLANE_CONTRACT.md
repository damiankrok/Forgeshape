# Workplane contract

`forgeshape_workplane.{h,cpp}`. Platform-neutral; reads no camera and no pixel.

| Plane | U | V | N | `(u, v, n)` -> local | Camera (yaw, pitch) |
| --- | --- | --- | --- | --- | --- |
| XY | +X | +Y | +Z | `(u, v, n)` | (0, 0) - from +Z |
| XZ | +X | -Z | +Y | `(u, n, -v)` | (0, +pitch limit) - from +Y |
| YZ | -Z | +Y | +X | `(n, v, -u)` | (pi/2, 0) - from +X |

- Every frame is right-handed: `U x V = N` (asserted by `CADR0-01..03`).
- Read from the plane's positive normal with world +Y up, U runs right and V
  runs up on every plane (asserted by
  `CADR0_01_the_sketch_view_reads_u_right_and_v_up_on_every_plane`).
- The mapping is written per plane in double and rounded to float once; an
  exact sketch coordinate stays as exact as a float can hold.
- `localToWorkplane` drops the component along the normal, so a point off the
  plane maps to its projection.
- The sketch origin is the body's local origin. Nothing is recentred. A CAD
  Body created by extruding a sketch starts at the identity placement, so while
  sketching, body-local space IS world space and the overlay's world-space
  lines are the sketch's own coordinates.
- The top view sits at the pitch clamp (`kPitchLimitRadians`, about 87.1 deg),
  not exactly vertical; ray-plane intersection is exact regardless, so what the
  finger places is exact.
- No camera-dependent truth, no Android pixel coordinate stored as geometry.
