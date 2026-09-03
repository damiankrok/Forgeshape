# Device E2E - `E2E-CADA3`

`SpatialSketchTest` on `emulator-5580` (confirmed `ForgeShape_Stage006`), real
MotionEvents through the production `SurfaceView`, pixels asked for from
`sketchScreenPoint` / `debugProjectWorld` and never written down. **OK (2
tests).** `FOCUSED_SPATIAL_SKETCH.txt` is the raw log.

| Case | What it drives | Result |
| --- | --- | --- |
| spatial world plane | New Sketch -> Pick plane or face in 3D -> the chooser is active -> tap-tap a pixel projecting to a point on the XY plane -> a sketch begins on a world plane | PASS |
| face-supported dependent | Body A (2x2 rectangle extruded 2 on XY, via the by-name path) -> New Sketch -> Pick in 3D -> tap-tap A's far-cap centre -> a sketch begins face-supported -> rectangle drawn on the face -> Extrude New Body = B; B is a CAD body and `sceneActiveBodyIsFaceSupportedCad`; deleting the producer A is refused with `DELETE_REFUSED_HAS_DEPENDENTS`; save and reopen restores every body | PASS |

## Mapping to the brief's E2E-CADA3-01..18

Covered here: 03/04 (tap a plane in the viewport, then rectangle/extrude via the
by-name path in the helper), 05/06 (New Sketch -> tap A face -> rectangle ->
extrude B), 12 (save/reopen dependency restored). Covered by the native
`CADA3-*` suite instead of a device run: 07 (curved side refusal), 08 (undo/redo
dependent), 09/10 (parent edits keep the child attached), 11 (parent transform),
13 (autosave/recovery by fingerprint+checkpoint design). Deferred: 01/02/16/17
(Home/Open/dirty-guard, APP-H1), 14 (adaptive-grid-with-zoom device visual), 15
(stylus hover), 18 (New Project -> Sculpt regression -- the existing Sculpt path
is unchanged and covered by `SculptUndoTest` in the aggregate).

## Runs before the passing one

- Run 1 failed to COMPILE: `loadProject` returns an int `PROJECT_*` code, not a
  boolean; the assertion was corrected to `PROJECT_OK`.
- Run 2: OK (2 tests).
