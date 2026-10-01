# CAD-PLANAR-FACE-PF-S1 — BEFORE (source map, written before geometry code)

Base: `f751773` (C2 integrated into `main`). All paths under
`app/src/main/cpp/` unless stated.

## 1. Entity and edge identity available today

| Identity | Where | Stable? | Use in PF-S1 |
| --- | --- | --- | --- |
| `SketchEntityId` (u32, 0 = none) | `forgeshape_sketch.h:253-259`; minted by `addSketchEntity` from `CadSketch::nextEntityId` (`forgeshape_sketch.cpp:669-693`), never reused (`removeSketchEntity`), persisted with `nextEntityId` | Yes — monotonic, survives edits, save and reopen | The `sourceEntityId` of every fragment and every intersection partner |
| Per-edge local index | `ClosedProfile::edgeLocalIndex`, `CadFaceToken::edgeLocalIndex` (`forgeshape_sketch.h:284-298`) | Yes for rectangle (0..3 in `rectangleProfilePolygon` order: bottom, right, top, left), polyline (segment k = vertex k → k+1, the closing segment n-1 when closed) and line / arc / spline (0) | Reused unchanged as `sourceEdgeLocalIndex` |
| Circle edge index | `extractClosedProfiles` gives a circle 32 TESSELLATION edges, 0..31 (`forgeshape_sketch.cpp:1106-1111`) | Tessellation index, not semantic — only ever used for the ineligible circle side face | **Not used.** The arrangement treats a circle as ONE analytic edge, local index 0, parameter = angle from the entity's own `+u` direction counter-clockwise (the origin `circleProfilePolygon` already starts at) |
| `CadFaceToken`, `TopoRef`, lineage token | `forgeshape_cad_face.*`, `forgeshape_sketch.h:270-320` | Yes | Untouched; fragment side faces are a PF-S2 concern |
| `ProfileRegionRef {outerAnchorId, holeAnchorIds}` | `forgeshape_sketch_region.h:88-91` | Yes (whole loops only) | Untouched; the nested case is compared against it (PF-S1-04) |

There is no fragment, intersection-node or face identity anywhere today.

## 2. Tolerance and range constants reused (not widened)

| Constant | Value | Where | PF-S1 use |
| --- | --- | --- | --- |
| `kSketchCoincidenceMeters` | 1e-6 m | `forgeshape_sketch.h:199` | Two points within it are one arrangement node; an endpoint within it of another curve is a T-junction; two roots within it are one tangent contact; an overlap shorter than it is a touch |
| `kMinProfileAreaSquareMeters` | 1e-12 m² | `forgeshape_sketch.h:203` | A bounded face below it is refused (`DegenerateFace`) rather than emitted |
| `kMaxSketchCoordinateMeters` | 1e5 m | `forgeshape_sketch.h:193` | Guarantees every intersection computation is finite (validated by `validateCadSketch`) |
| `arcGeometry` collinearity test | `|d| ≤ kSketchCoincidenceMeters · max(1, scale)` | `forgeshape_sketch.cpp:406-470` | Reused as THE arc centre/radius/start/sweep derivation — no second arc model |

## 3. Caps today

| Cap | Value | Where |
| --- | --- | --- |
| entities per sketch | 256 (`kMaxSketchEntities`) | `forgeshape_sketch.h:208` |
| polyline vertices | 256 (`kMaxPolylineVertices`) | `forgeshape_sketch.h:209` |
| spline points | 32 (`kMaxSplinePoints`) | `forgeshape_sketch.h:231` |
| profile polygon vertices | 1024 (`kMaxProfileVertices`) | `forgeshape_sketch.h:246` |
| regions selected / holes per region | 16 / 64 | `forgeshape_sketch_region.h:83-84` |

The legal worst case is therefore 256 polylines × 256 vertices ≈ 65 536
straight source edges. That is quadratic work no touch UI needs, so the
arrangement gets its OWN caps (source edges, contacts), checked before the
quadratic work completes and refused by name (`CapExceeded`).

## 4. Existing helpers

| Helper | Where | Kind | PF-S1 verdict |
| --- | --- | --- | --- |
| `coincident` | `forgeshape_sketch.cpp:109` (file-private) | exact distance test at the coincidence tolerance | Same rule restated in the arrangement module (one-line predicate, same constant) |
| `sketchSegmentsIntersect` | `forgeshape_sketch.cpp:171` | boolean cross-or-touch, NO intersection point | Not enough: gives no point, no parameter, no overlap classification |
| `arcGeometry` | `forgeshape_sketch.cpp:406` | analytic centre / radius / start angle / signed sweep | **Reused** |
| `rectangleProfilePolygon` | `forgeshape_sketch.cpp:740` | exact corners, edge order | **Reused** for rectangle source edges (exact, not a tessellation) |
| `circleProfilePolygon` | `forgeshape_sketch.cpp:749` | 32-gon TESSELLATION | **Forbidden** for semantic identity |
| `tessellateSketchCurve` | `forgeshape_sketch.cpp` | arc / spline polyline TESSELLATION | **Forbidden** for semantic identity (and a spline is refused outright) |
| `extractClosedProfiles`, `validateLoop`, `chainCurves` | `forgeshape_sketch.cpp:788-1149` | loop extraction on tessellated polygons | Not used by the arrangement; unchanged |
| `loopsTouch`, `sketchPointStrictlyInside`, `extractSketchRegions`, `mergeSelectedRegions` | `forgeshape_sketch_region.cpp` | nesting regions on polygons | Unchanged; used only as the PF-S1-04 comparison |
| renderer / overlay tessellation | `forgeshape_sketch_session.cpp:736, 1913-1981` | presentation | **Forbidden** |

## 5. Consequences for the design

- One new portable module, `forgeshape_sketch_arrangement.{h,cpp}`, reading
  `CadSketch` and returning derived values only; no session, JNI, render or
  codec dependency.
- Supported sources: Line, Polyline segment, Rectangle side, Circle, Arc.
  Any Spline in the sketch → `UnsupportedCurve` (no tessellation identity).
- Identity is built only from entity ids, edge-local indices and per-pair
  intersection ordinals along the source edge's own parameter.
- A new derived status enum lives in the module; `CadStatus` (a JNI-crossing
  code list) is not extended.
