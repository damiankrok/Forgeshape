# CAD-V6-S2 correction — fill-bucket cells and a continuous extrude panel

`CAD-V6-S2-CORRECTION-FILL-HUD-R1` on `feature/cad-v6-sketch-face-r1` (intermediate
v6 branch; `main` untouched at `6c9f156`). Start: `8e87403`. Source findings:
`BEFORE.md` in this directory.

## What the OWNER saw, and what changed

| OWNER finding | Root cause (BEFORE.md) | Correction |
| --- | --- | --- |
| Only a few areas selectable in a mixed sketch; "Those regions overlap" | One Spline made the arrangement return `UnsupportedCurve`; Finish then silently used whole-loop regions | Splines are intersected span by span; Finish refuses, by name, an arrangement failure over crossing loops |
| The spline+line shape acts as one whole loop | Same | Its crossings split it into cells like any other curve |
| Further cells not reachable by tapping after the first | The arrow's 24-unit grab corridor took every tap on the shaft's projection | A still tap in the corridor but off the drawn arrow toggles the cell under it; a drag still takes the arrow |
| Action panel jumps during orbit | Three discrete candidate placements, first fit wins | One continuous function of the projected arrow, bounded rotation |

## Fill

- **Spline geometry has one statement.** `sketchSplineSpan` (`forgeshape_sketch.{h,cpp}`) gives span `i` as the exact cubic Bezier; `tessellateSketchCurve` now samples it (bit-identical: `SKETCH_UX` and every golden digest unchanged).
- **Identity.** Span `i` is edge-local index `i`; cuts are the existing semantic cuts (partner edge + ordinal). No sample, tessellation index or coordinate is identity; no `CADB` layout change, no v7.
- **Intersections** (`forgeshape_sketch_arrangement.cpp`): span↔line is a cubic in the span parameter, span↔circle/arc a sextic; real roots isolated between the derivative's roots and bisected. Span↔span: de Casteljau subdivision until both pieces are flat to 0.05 µm (depth ≤ 40, ≤ 32,768 visits per pair), chord intersection, Newton refinement on the exact curves, crossing vs touch decided by the signed side a 20 µm probe each way takes.
- **Tolerance and degeneracy** (all within the 1 µm coincidence rule): a turning point within tolerance with roots on both sides is ONE touch (no node, tangent counted); a span wholly within tolerance of a line/circle, identical or reversed twin spans, and an exhausted subdivision are `AmbiguousOverlap`; a span meeting itself is `SelfIntersectingCurve` → `SelfIntersectingProfile`. Spline winding = chord angle + 2π·(ray crossings of piece − of chord); area by exact 3-point Gauss–Legendre.
- **Mode decision** (`decideSketchSelectionMode`, `forgeshape_cad_body`): Ok arrangement → PlanarFaces iff `sketchRequiresPlanarFaces`, else LoopRegions (legacy-exact, legacy writer); failed arrangement → LoopRegions only if `sketchLoopsAreExact`, otherwise refused with the arrangement's own `CadStatus` (new user sentences for overlap, cap, degenerate).
- **Pinch** (two cells meeting only at a point) is the new appended `CadStatus::PlanarFacesTouchAtPoint` (code 61) — never `OverlappingRegions`.
- **Taps** (`SketchSession::onExtrudeTouch`, `CadExtrudeManipulator::onDrawnArrow`, `kCadExtrudeTapOnArrowUnits` = 10): corridor-but-off-arrow Down is captured and holds the depth until travel passes the tap slop; a still Up releases the capture (nothing written) and toggles the cell.

**OWNER stress sketch** (FILL-06, native): rectangle + overlapping circles A, B + circle C across a side + spline-and-line loop across the bottom → **8 bounded cells**, refs distinct, exterior absent (rectangle tiled to 1e-6 m²), every cell valid alone, all 28 pairs either merge (24) or `PlanarFacesTouchAtPoint` (4), none a legacy overlap status; all 8 union into one solid.

## HUD

`CadHudPresentation.layoutPanel`: centre on the arrow's screen line at `corridor + gap + half proxy extent` past the point; at an edge it slides back along the same line by the least that fits (`slideRange`), at most to the shaft's base; otherwise hidden whole. `panelRotationDegrees` = 0.35 × reading angle, capped ±25°, tapered to 0 over the last 20° before vertical (continuous through the wrap). One unrotated ≥ 48 dp proxy covers the turned plate's box; the plate turns and scales about its own centre (`ViewportAnchorSpace.placeCentred`). Constants OWNER-TUNABLE. The dimension leader and value are unchanged.

## Tests

- Native (CAD_FEATURE suite): FILL_01..13 + FILL_MODE_01..04 (`forgeshape_sketch_fill_selftest.cpp`), S2CORR_TAP_01..06 (session taps in the product's oblique view), updated PFS1_15a / CADV6_F10 / CADV6_P09..P12 / CADV6S2_REG_04..05.
- JVM: HUD-CONT-01..05 (`CadHudPresentationTest`).
- Device: `CadPlanarFaceOwnerCorrectionTest` J1..J5 — every cell of the OWNER-style sketch tapped through the WINDOW (the HUD gets first touch), J2 A/B/A with a tap inside the arrow corridor, J3 union = one shell, J4 spline cell New Body, J5 orbit sweep.

## Evidence

- Host aggregate: `HOST_SELFTESTS_OK (3971 checks, 0 failed)`; `fill_spline_us` median ≈ 6 ms (3 runs), eight maximal splines ≈ 29 ms.
- JVM 121/121; debug, release, androidTest build; release self-test guard PASS; corpus 57/57 byte-identical, `cad_spline_face_v6` now a VALID file with unchanged bytes.
- CI DEVICE `36848559518` on `7c4849a`: 16/22 — 5 failures were the new test's own grid precondition (CI grid 0.25 m), 1 a real regression (panel hidden at close zoom because the slide stopped at the tip). Both fixed in `130b6e2`.
- CI DEVICE `36851090923` on `130b6e2` (CadPlanarFaceOwnerCorrectionTest, CadPlanarFaceRuntimeTest, CadCanvasExtrudeTest): **PASS 22/22**, 321 s, 23/23 startup tokens. J5: 25/25 frames shown, largest offset jump 7.4 dp, largest turn 0.74°; the close-zoom panel now slides 202 dp back along the shaft instead of hiding.
- FullSharded NOT RUN. Emulator evidence closes no physical-device gate.
