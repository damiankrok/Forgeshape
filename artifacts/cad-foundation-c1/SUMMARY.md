# CAD-FOUNDATION-C1 — union of regions and a technical-leader extrude HUD

Baseline `origin/main = 509de02`. Branch `feature/cad-foundation-c1-union-hud-r1`.
Tested product candidate: `__PRODUCT_SHA__` (product `262cfc1` + the test-only
correction `1fa1bec`). BEFORE evidence: `BEFORE.md` in this directory.

## What changed

| Area | Change | Owner |
| --- | --- | --- |
| S3 one scale fact | `cadExtrudeManipulatorScale` reads `metersPerPixel` ONCE at the arrow's base; `SketchSession::extrudeViewFacts` hands the same fact to the overlay, the tool-state slot and the chip; the hit test calls the same function. Without the frame's facts no arrow is drawn. | `forgeshape_cad_extrude_tool`, `forgeshape_sketch_session`, `forgeshape_jni` (render path, tool state, chip) |
| S4 overlay revision | A rebuild caused by another `worldPerUnit` or other view facts advances `overlayRevision_`; an authored change still bumps once. | `SketchSession::overlay` |
| Region union | `mergeSelectedRegions`: a loop bounds the union exactly when selection membership differs across it. `validateRegionSelection` allows a region beside its OWN direct hole and keeps every touch/cross and material-overlap refusal. `toggleRegion` is a pure toggle; an unmergeable addition is refused by name. Faces, lineage token, solid (one prism per component), components count, preview hatch/edges and arrow anchor read the union. | `forgeshape_sketch_region`, `forgeshape_cad_feature`, `forgeshape_cad_body`, `forgeshape_sketch_session`, `forgeshape_cad_extrude_tool` |
| Leader HUD (native) | Dimension leader (extension lines, dimension line, 45° ticks) in the `Dimension` overlay range, on the reading-up side of the shaft; band 0.40..1.60; tool-state slots 32..41. | `forgeshape_cad_extrude_tool`, `forgeshape_sketch_session`, `forgeshape_jni` |
| Leader HUD (Android) | No row, no capsule. Invisible unscaled ≥ 48 dp proxies placed on the projected leader; glyph `28 dp × clamp(scale, 0.40, 1.60)` with a disc its own size; value text above the leader, rotated and upright, `clamp(14 sp × scale, 11, 18)`; off-screen glyphs hidden; palettes stay screen chrome and carry Tool Labels captions. | `CadHudPresentation` (pure policy), `CadExtrudeCanvasView` (placement only), `bg_hud_glyph*.xml` |

`EditorWorkspaceView` is unchanged (0 lines). `forgeshape_jni.cpp` grew by 55
lines (the frame's view facts, slots 32..41, the chip's scale call).

## Rectangle O + circles A, B — truth table (native `CADFC1_REG_01..07`, device J1/J2)

| Selection | Union components | Area |
| --- | --- | --- |
| O | O with holes A, B | rect − A − B |
| A | disk A | A |
| B | disk B | B |
| O+A | O with hole B | rect − B |
| O+B | O with hole A | rect − A |
| A+B | disk A, disk B (two components) | A + B |
| O+A+B | O, no hole | rect |

## Persistence

No `CADB` byte, section or version changed. The stored selection is still the
atomic `ProfileRegionRef` list (`CADFC1_PER_02`), a union selection round-trips
as the unchanged v5 record (`CADFC1_PER_03`), a nesting edit under it is still
`ProfileRegionMismatch` (`CADFC1_PER_01`). `forgeshape_project_document.*`,
`scripts/build-forge-corpus.ps1` and `testdata/` are untouched; the independent
corpus regenerates 44/44 byte-identical (local and `CI FAST`); the project
suite's golden digests are unchanged. `DATA_PACKAGE_SPEC.md` states the union
reading and that an older build refuses a region-beside-its-hole list by name.

## Evidence

__EVIDENCE__

## OWNER review (physical device) — OWNER REVIEW REQUIRED

Provisional, OWNER-TUNABLE values: band 0.40..1.60; glyph 28 dp at scale 1;
value 14 sp in 11..18 sp; value gap 3 dp; glyph gap 4 dp; leader offset 0.55,
extension gap 0.12, overshoot 0.12, tick 0.10 (fractions of the control world
size). Checklist:

1. rectangle + two circles selection;
2. O+A leaves only B as a hole;
3. all selected becomes a solid rectangle;
4. the leader/value feels attached to the geometry;
5. zoom out visibly shrinks leader and glyphs;
6. zoom in grows them within the upper band;
7. controls stay tappable despite smaller visible glyphs;
8. Two Sides stays understandable;
9. orbit/zoom does not make the HUD drift.

Known presentation consequence for review: a union's arrow stands on the
union's centroid, which can lie on a disk the user wants to tap again; a press
on the arrow is a drag, so that disk is tapped off the arrow's corridor.

## Deferred

`CAD-SKETCH-IDENTITY-R1` (sketch ids, body sketch table, feature → sketch
references, reuse, `CADB` v6) is NOT started.
