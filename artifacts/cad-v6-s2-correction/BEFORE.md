# CAD-V6-S2 correction (fill + HUD) — BEFORE

Source state read at `feature/cad-v6-sketch-face-r1` = `8e8740316cbbe40f928b57c37c0519cb3f26f640`
(merge-base with `origin/main` = `6c9f156c1df91fe1a55445aed6186105a075ab31`), before any
product edit. Every claim below names the line it was read from.

## OWNER finding 1 — "only a few areas are selectable", "Those regions overlap"

### 1a. One Spline anywhere disables the whole arrangement

`app/src/main/cpp/forgeshape_sketch_arrangement.cpp:148` — `collectSourceEdges()` returns
`ArrangementStatus::UnsupportedCurve` the moment ANY entity is a `SketchEntityKind::Spline`,
before a single source edge is built. The header states the rule
(`forgeshape_sketch_arrangement.h:34-35`, `:53`): "A Spline is refused (`UnsupportedCurve`)
rather than intersected on its tessellation".

So the OWNER's mixed sketch (rectangle + crossing circles + a spline-and-line loop) derives
NO arrangement at all — not the spline's cells only, every cell.

### 1b. Finish then degrades silently to legacy loop regions

`app/src/main/cpp/forgeshape_sketch_session.cpp:1266` `SketchSession::finish()`:

- `:1276` derives the legacy `extractSketchRegions`;
- `:1280` derives the arrangement;
- `:1281` `sketchRequiresPlanarFaces(arrangement, extraction)` —
  `forgeshape_cad_body.cpp:801-818` returns `false` as soon as
  `arrangement.status != Ok` (its header, `forgeshape_cad_body.h:563-571`, says so
  outright: "a Spline, an overlap or a cap keeps the sketch on loop regions");
- `:1318` `reconcileRegionSelection()` — the sketch is now in `LoopRegions` mode.

Nothing reports the failure. The user is handed the loop model for a sketch whose loops cross.

### 1c. Loop semantics explain the exact messages the OWNER saw

In loop mode a crossing never splits anything:

- `forgeshape_sketch_region.cpp:226-227` marks every touching/crossing loop pair as a
  `conflict`; such a loop is never a hole, so each crossing circle and the rectangle stay
  WHOLE regions (only "a few areas" are offered — one per loop, not one per cell);
- `forgeshape_sketch_region.cpp:276` a region whose holes touch/cross is
  `OverlappingHoles` (listed, not selectable);
- `forgeshape_sketch_region.cpp:356-374` `validateRegionSelection` refuses two chosen
  regions whose loops conflict as `OverlappingRegions`, whose Android string is
  `strings_cad_vs.xml:12` "Those regions overlap — one extrusion cannot use both."

Both refusals are correct for the loop model and wrong for the product rule: intersecting
authored loops must be split into atomic cells instead.

### 1d. The spline-derived closed shape behaves as one whole loop

Same cause as 1a/1b: with the arrangement refused, the spline+line chain is one legacy loop
(`extractClosedProfiles` chains it), so its intersections with other curves cut nothing.

### 1e. Spline geometry has exactly one implementation today

`app/src/main/cpp/forgeshape_sketch.cpp:561-591` `tessellateSketchCurve` builds each span's
cubic Bezier inline (Catmull-Rom tangents, end spans reflecting their neighbour,
`c1 = p1 + (p2 - p0)/6`, `c2 = p2 - (p3 - p1)/6`) and samples it at
`kSplineSegmentsPerSpan` (8). The span geometry is not reachable from any other module, so the
arrangement could not have used the same curve truth without duplicating the arithmetic.

## OWNER finding 2 — repeated real taps after the first selection

`forgeshape_sketch_session.cpp:789-875` `onExtrudeTouch`: on `Down` the arrow hit test
(`:813`, corridor `kCadExtrudeGrabRadiusUnits` = 24 reference units around the WHOLE projected
shaft, `forgeshape_cad_extrude_tool.cpp:440-476`) wins outright — it disarms the region tap
(`:817`) and captures a drag. The shaft stands on the selected union's area centroid and, in the
oblique feature view Finish installs, its screen projection crosses neighbouring cells. Every
tap inside that corridor therefore starts an arrow drag and never toggles the cell under it.

`app/src/androidTest/java/com/forgeshape/app/CadPlanarFaceRuntimeTest.java:524-527` documents
the workaround: every face after the first is added "through the precision surface's row path,
which a later pick uses so a tap can never land on the extrude arrow a first pick drew"
(`:142-143`, `:198`). The device suite never proved a second REAL viewport tap, and it never put
a Spline in a planar arrangement.

## OWNER finding 3 — the action panel jumps during orbit

`app/src/main/java/com/forgeshape/app/CadHudPresentation.java:594-665` `layoutPanel`:

- three DISCRETE candidates (`:633-634`): `PANEL_BEYOND` (past the point along the arrow),
  `PANEL_AWAY` and `PANEL_TOWARD` (perpendicular, either side of the shaft);
- the beside placements try `PANEL_SLIDE_STEPS` = 4 discrete slides (`:648`, `:671`);
- the FIRST candidate that fits wins (`:641-662`).

The chosen box is a discontinuous function of the projected arrow: when the BEYOND box crosses a
viewport edge by one pixel the answer jumps a whole corridor-plus-box to the perpendicular side,
and the AWAY/TOWARD choice itself flips with the leader's side (`:626-631`), which native signs
from the camera. The plate is never rotated (`CadExtrudeCanvasView.java:1062-1100` scales it
about pivot (0, 0) and places it axis-aligned), so it reads as screen-horizontal floating UI
while the value beside it (`placeValue`, `:1104-1111`) follows its leader.

The dimension leader and value (`layoutLeader`, `CadHudPresentation.java:449-489`) are a single
continuous function of the projected leader and are not part of this defect.
