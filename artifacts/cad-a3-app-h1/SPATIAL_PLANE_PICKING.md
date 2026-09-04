# Spatial support picking (`CAD-A3` B/D)

`forgeshape_support_chooser.{h,cpp}` resolves a viewport tap to a support.

- World planes are drawn as large touchable target squares at the origin
  (bordered, lightly gridded, axis-tinted), pickable by a ray-plane hit within
  the square bounds; the nearest facing one wins.
- Faces (existing CAD project) are picked through the ORDINARY scene pick, so
  what is hittable is exactly what is drawn; the rendered triangle is mapped
  through `cadFaceRanges` to a semantic token, and a curved side resolves but is
  never eligible. The chosen face becomes a TopoRef plus a resolved world frame.
- Disambiguation is deterministic: nearest hit, with a face ahead of a plane at
  the same depth (the user physically touched the body).

The chooser draws its targets through the existing sketch-overlay line path (no
new renderer pipeline). It owns no project state; nothing it does is a body, an
ObjectId or a `.forge` byte until the sketch it starts commits.

## Interaction (device)

New Sketch lands DIRECTLY in the spatial chooser (`CAD-A3-C1`, `UI-OWNER-46`);
the by-name plane list is reached from "Choose a plane by name"
(`sketch_plane_by_name`) under the tile and stays as the accessibility fallback,
so the CAD-R0 by-name flow is untouched. New Project -> CAD enters the same
chooser with faces off, over a scene that holds no project. In the chooser a
single-finger TAP selects and highlights a target; a second tap on the SAME
target confirms and begins the sketch on it -- tap to aim, tap to commit. Two
fingers navigate throughout; System Back cancels with no mutation. The native
touch arbitration owns this so no gesture logic leaks above JNI.

## Verified

Native `CADA3-08/24/25/26`: tapping a world plane selects it, tapping a planar
cap selects that face, a curved side never selects, a tap far off target is a
plane or nothing. Device `E2E-CADA3` (SpatialSketchTest): a tap-tap on a world
plane begins a sketch; a tap-tap on body A's far cap begins a face-supported
sketch that extrudes body B.

## Deferred (honestly)

Stylus HOVER highlight is wired at the JNI (`supportChooserHover`) but not
device-verified. The face-first contextual shortcut (tap a face OUTSIDE New
Sketch mode to get a New Sketch action) is not implemented; the guaranteed
action-first flow (New Sketch, then tap support) is the one shipped.
