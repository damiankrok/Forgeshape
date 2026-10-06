# CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1 — BEFORE

Baseline audited before any product edit.

| ref | SHA |
| --- | --- |
| `origin/main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` |
| `origin/feature/cad-v6-sketch-face-r1` | `bbae765645a318f83451e9a61b6a45cf2cb17e33` |
| `origin/feature/cad-v6-revolve-newbody-r1` | `5dcb73ba43fff36a38631c5d1d870549a3172efe` (descendant of the v6 branch: verified with `git merge-base --is-ancestor`) |
| task branch | `feature/cad-v8-sketch-drafting-toolkit-r1` created at `5dcb73b` |

Baseline host run (`bash scripts/host-native-selftests.sh`, this container):
23 suites, `HOST_SELFTESTS_OK (4064 checks, 0 failed)`.

Paths are relative to `app/src/main/cpp/` unless stated. Line numbers are
the baseline's.

## 1. Durable entity kinds and ids

- `forgeshape_sketch.h:472-479` — `enum class SketchEntityKind { Line,
  Polyline, Rectangle, Circle, Arc, Spline }`; the variant order IS the kind
  (`:553-554`, `kind()` = variant index, kinds APPENDED).
- `forgeshape_sketch.h:483-541` — payloads: `SketchLine{start,end}`,
  `SketchPolyline{vertices, closed}`, `SketchRectangle{center,width,height}`
  (axis-aligned), `SketchCircle{center,radius}`, `SketchArc{start,mid,end}`
  (three authored points), `SketchSpline{points}`.
- `forgeshape_sketch.h:547-575` — `class SketchEntity { id_, payload_ }`. **No
  role field exists**: every entity is material. `sameSketchEntity`
  (`forgeshape_sketch.cpp:231`) compares id, kind and payload bits only.
- `forgeshape_sketch.h:361-362` — `using SketchEntityId = uint32_t`,
  `kNoSketchEntity = 0`; per-sketch, never an `ObjectId`.

## 2. Entity id allocation

- `forgeshape_sketch.h:650-669` — `CadSketch { plane, entities, nextEntityId
  = 1, hasFaceSupport, faceSupport }`. `nextEntityId` is STORED (a reopened
  sketch never re-mints a worn id).
- `forgeshape_sketch.cpp:773-797` — `addSketchEntity` validates with a
  provisional id, then appends and increments; a refusal mints nothing.
- `forgeshape_sketch.cpp:818-829` — `removeSketchEntity` never touches the
  allocator (deleted ids are not reused).
- `forgeshape_sketch.cpp:732-771` — `validateCadSketch`: non-zero, unique,
  below the high-water mark; per-entity rules; caps `kMaxSketchEntities` 256.
- There is **no dimension table and no `nextDimensionId`** anywhere.

## 3. Selected-entity state

- `forgeshape_sketch_session.h:827` — `SketchEntityId selectedEntityId_`: ONE
  entity, volatile. `select` (`.cpp:1270`), `clearSelection` (`:1279`),
  `deleteSelected` (`:1284`, removes that one entity).
- Selection by tap: `onTouch` Up with `SketchTool::Select` (`.cpp:1072-1078`)
  — `selectedEntityId_ = hitTest(cursor_, 24 dp)`; a miss clears it.
- Placement also selects the placed entity (`placeFromDrag` `:1166`,
  `placeArcThrough` `:1186`, polyline/spline `:1221`, `:1259`).
- JNI: `sketchSelectEntity` (`forgeshape_jni.cpp:4448`), `sketchSelectedEntity`
  (`:4473`), `sketchDeleteSelected` (`:4436`). No multi-selection exists; it is
  separate from the Ready-state region/planar-face selection
  (`extrude_.profile*` / `planarFaces`), which is untouched by this task.

## 4. Current line dimension

- `forgeshape_sketch_session.cpp:496-512` `selectedLineLength`,
  `:552-569` `selectedLineDimensionAnchor` (midpoint offset
  `kSketchDimensionOffsetUnits` 30 dp along the CCW perpendicular),
  `:571-608` `applyLineLength` (P0 fixed, direction kept, refused not
  clamped, Editing only).
- Overlay: `:3094-3143` — the `SketchOverlayStyle::Dimension` range: two
  extension lines, the dimension line and two 45° ticks for the ONE selected
  Line, plus the extrude leader.
- Android: `SketchDimensionLabelView.java` — one chip at the projected anchor,
  tap → field + Apply → `sketchApplyLineLength` (`jni.cpp:5732`); data from
  `sketchLineDimension` (`jni.cpp:5693`). `SketchChromePolicy.lineDimensionShown`
  = Editing only.
- **Not persistent**: an interaction overlay of the current selection. No
  record, no id, no Driving/Reference, no other kinds.

## 5. Numeric edit path

- `SketchSession::replaceEntity` (`.cpp:1299-1308`) → `replaceSketchEntity`
  (`forgeshape_sketch.cpp:799`), Editing only, never snapped.
- JNI `sketchApplyRectangle` (`jni.cpp:4528`), `sketchApplyCircle`,
  `sketchApplyLine` (`:4574`); Android `SketchEditorView` (Editing section:
  rectangle width/height, circle radius, line endpoints).
- Input parsing: `LengthUnit.parse` (`LengthUnit.java:128-134`) =
  `new BigDecimal(text)` — plain numbers only, **no expression parser**.

## 6. Snap types and priority

- `forgeshape_sketch_session.h:122-126` — `SketchSnapKind { None, Grid,
  Endpoint }`.
- `forgeshape_sketch_session.cpp:98-126` `collectSnapPoints`: line ends,
  polyline vertices, rectangle corners AND centre, circle centre, arc's three
  authored points, spline points — all reported as `Endpoint`.
- `:710-741` `snap`: nearest of those within `24 dp × worldPerUnit` (ties: the
  later candidate with `<=`), else grid (`round(raw/step)*step` of the
  adaptive step). Priority today: **Endpoint > Grid**. No midpoint,
  intersection, origin, or H/V inference; the centre is not its own kind.
- Marker: one cross (`:2986-2994`) for every kind.

## 7. Profile extraction

- `forgeshape_sketch.cpp:1180-1253` `extractClosedProfiles`: rectangle /
  circle / closed polyline each one loop; `chainCurves` (`:987-1176`) chains
  Line/Arc/Spline by coincident authored endpoints. **Every entity in
  `sketch.entities` takes part** — there is no role filter.
- Regions: `forgeshape_sketch_region.cpp` `extractSketchRegions` builds on it.

## 8. PlanarFace arrangement input

- `forgeshape_sketch_arrangement.cpp:312-397` `collectSourceEdges`: every
  entity of the sketch becomes source edges (line 1, polyline n or n-1,
  rectangle 4, circle 1 closed, arc 1, spline one per span). **No role
  filter**: a dangling line splits faces (T-junctions).
- `deriveSketchArrangement` (`:1674`) validates the sketch, collects contacts
  analytically (segment/round/Bezier), clusters NODES, builds FRAGMENTS
  between consecutive cuts with semantic `ArrangementCut`s, and faces. Public
  output: `nodes`, `fragments{ref,startNode,endNode,curved,boundsFace,points}`,
  `faces`.

## 9. Revolve axis source-edge resolution

- `forgeshape_cad_body.h:325-360`, `forgeshape_cad_body.cpp:106-160` —
  `CadSketchEdgeRef{entityId, edgeLocalIndex}`; `sketchEntityStraightEdges`
  (Line 0; Polyline segment i, closing n-1; Rectangle CCW edges 0..3 from
  (-w/2,-h/2)); `resolveRevolveAxis` refuses `RevolveNeedsAxis`,
  `RevolveAxisUnresolved`, `RevolveAxisNotStraight`, `RevolveAxisDegenerate`.
  It resolves against ALL entities (role does not exist yet).
- Session pick: `pickRevolveAxisAt` (`forgeshape_sketch_session.cpp:2305`).

## 10. Sketch overlay / rendering

- `forgeshape_sketch_overlay.h` — line list in WORLD space, `GizmoVertex`
  (`axis` = hue tag, `handle` = emphasis), ranges styled `GridMinor`,
  `GridMajor`, `Axes`, `Entities`, `Dimension`
  (`kSketchOverlayStyleCount = 5`), weights via the pure
  `sketchOverlayStyleWeights`.
- `forgeshape_sketch_session.cpp:2772-3155` `buildOverlay`: grid, axes,
  entities (emphasis = selected or chosen loop members), polyline/arc/drag
  previews, ONE cross snap marker, hatch, revolve ring, extrude preview and
  arrow, then the Dimension range. Solid lines only; no dash.

## 11. CADB v7 sketch-entity encoding

- `forgeshape_project_document.cpp:1039-1086` `writeCadEntities`: u32 count,
  per entity u32 id, u8 kind code, kind payload. Shared by v1..v7.
- `:1178-1239` `writeCadBodyV6(out, body, featureKinds)`: per body u64 id,
  u32 nextSketchId, u32 nextFeatureId, u32 sketch count; per sketch u32 id,
  u8 placement (+ placement payload), u32 nextEntityId, entities; then the
  features, each u32 id, (v7) u8 kind, payload.
- Decode: `readCadEntities` (`:1783`, entity count must be 1..256),
  `decodeCadBodyV6(in, body, featureKinds)` (`:2115-2292`),
  `decodeCadPayload` (`:2294`, v6/v7 table layout), version gate `:2589-2596`
  (v1..v7 accepted). Relations are judged after decode by
  `validateCadBodyState`. No role byte, no dimension block.

## 12. Conditional v6/v7 writer

- `forgeshape_cad_body.cpp:627` `cadBodyStateLegacyRepresentable` (false for
  a Revolve, shared/odd sketch tables, PlanarFaces, non-derived marks).
- `forgeshape_project_document.cpp:428-447` `cadDocumentNeedsV6` /
  `cadDocumentNeedsV7`; `:1348-1356` lowest-version selection
  (`cadV7 → cadV6 → …`), `:1498-1504` section version.
- Fingerprint: `forgeshape_project_state.cpp:588-647` mixes a v6 block only
  outside the legacy predicate and a `REV7` block only for a Revolve.

## 13. Fixture corpus baseline

- `testdata/forge/v1/`: **61** `.forge` fixtures (`ls | wc -l` = 61).
- Independent encoder `scripts/build-forge-corpus.ps1` (2561 lines; v7 block
  `:1761-1860`, table `:2475-2478`, digests `:2553`). CI FAST regenerates all
  and requires byte identity (`docs/CI_CLOUD.md`).
- Verdicts: `artifacts/cad-v6-revolve-newbody/corpus-verdicts-after.txt`.

## 14. Edit Sketch + history

- `SketchSession::beginEditFeature` (`forgeshape_sketch_session.cpp:272-335`)
  stages `*view.sketch` (the whole `CadSketch`) plus the feature; Cancel costs
  nothing.
- `candidateState` (`:1935-2004`) writes `sketch_` back into the table record
  (base or later feature) — the WHOLE `CadSketch`, so anything added to
  `CadSketch` travels with every commit path automatically.
- `commitEdit` (`:365-413`): evaluate → dependent-face check → ONE
  `ScopedConstructionEdit` around `body->applyState` → one Undo; Undo restores
  the snapshot (`CadBody::restoreState`), Redo the edited one.
- In-session edits (place, delete, typed values, line length) are NOT history
  steps; they are staged until the one commit.

## Consequences for the design

1. A role must live in `SketchEntity` (domain truth), and BOTH
   `extractClosedProfiles` and `collectSourceEdges` must skip Construction.
2. Dimensions must live in `CadSketch` (they then ride every staging/commit
   path and the history snapshot for free).
3. v7 has no byte for either → a new section version is required (v8).
4. Snapping, Trim and Extend need intersections over ALL entities (Construction
   included); the arrangement's analytic contact machinery is reused by
   deriving it over an all-curves view, never a second intersector.
