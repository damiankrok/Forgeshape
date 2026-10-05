# CAD-V6-REVOLVE-NEWBODY-E2E-R1 — BEFORE audit

Written before any product edit, on branch `feature/cad-v6-revolve-newbody-r1`
created at `3e2443f281981e082e113ab97470a8dbe05dd120` (the MULTIFACE correction
head). Baseline refs verified: `origin/main` = `6c9f156c1df91fe1a55445aed6186105a075ab31`,
`origin/feature/cad-v6-sketch-face-r1` = `bbae765645a318f83451e9a61b6a45cf2cb17e33`,
`origin/feature/cad-v6-s2-owner-feedback-multiface-r1` = `3e2443f`. Baseline host
aggregate on this tree: `HOST_SELFTESTS_OK (4024 checks, 0 failed)`.

Paths are under `app/src/main/cpp/` unless stated.

## 1. `CadBodyState` assumes the base feature is an Extrude

- `forgeshape_cad_body.h:420-438` — `struct CadBodyState { sketches; nextSketchId;
  baseSketchId; ExtrudeFeature extrude; laterFeatures; nextFeatureId; }`. The base
  feature IS the `extrude` field (line 429); nothing records a feature KIND.
- `forgeshape_cad_body.h:415-419` — the comment states the base is "always New
  Body", and what is stored for it is "the sketch it extrudes ... and its extrusion".
- `forgeshape_cad_body.cpp:1007-1031` `validateCadBodyState` calls
  `validateCadFeatureGeometry(cadBaseSketch(state), state.extrude, ...)`.
- `forgeshape_cad_body.cpp:539-560` `cadFeatureAt` hands every view an
  `ExtrudeFeature*` (`CadFeatureView::extrude`, `forgeshape_cad_body.h:502-512`).

## 2. Later `CadFeature` records contain `ExtrudeFeature`

- `forgeshape_cad_body.h:398-406` — `struct CadFeature { featureId; operation;
  sketchId; ExtrudeFeature extrude; }`.

## 3. The v6 codec writes an Extrude payload with no feature-kind byte

- `forgeshape_project_document.cpp:1111-1170` `writeCadBodyV6`: per feature
  `featureId u32 | operation u8 | sketchId u32 | extent u8 | direction u8 |
  depth f64 | second f64 | selectionKind u8 | selection`. No kind tag.
- `forgeshape_project_document.cpp:1980-2161` `decodeCadBodyV6` reads exactly that
  and builds an `ExtrudeFeature` for every record (base into `state.extrude`).
- Version selection `forgeshape_project_document.cpp:1277-1281` and `:1423-1428`;
  readable versions `:2459-2464` (v1..v6). `kCadSectionVersionV6 = 6`
  (`forgeshape_project_document.h:180`).
- **Conclusion:** v6 cannot carry a Revolve without either reusing Extrude fields
  (depth as an angle, direction as a rotation sense — semantic abuse, forbidden) or
  a reader silently misreading a Revolve as an Extrude. A feature-kind tag requires
  a new layout: **CADB v7**.

## 4. The feature editor and the edit/history path assume Extrude

- `forgeshape_sketch_session.cpp:262-315` `beginEditFeature` stages
  `extrude_ = *view.extrude` (line 292); `candidateState()` (`:1900-1960`) rebuilds
  `state.extrude = extrude_` for the base and `existing.extrude = extrude_` for a
  later feature.
- `forgeshape_jni.cpp:5749-5800` `cadState` reports depth/direction/extent of
  `state.extrude`; `:5870-5935` `cadApplyExtrude/Rectangle/Circle` write
  `candidate.extrude` (the inspector's Apply); `:5125-5165` `cadFeatureInfo`
  reports extent and distances only.
- Java: `CadFeatureEditorView.java:186-235` (depth + direction fields),
  `:240-270` (feature rows labelled by operation only), `EditorWorkspaceView
  .onEditCadFeatureRequested` (feature 1 → Edit Sketch).

## 5. Ready selection already produces LoopRegions and PlanarFaces

- `forgeshape_sketch_session.cpp:1344-1409` `finish()` derives regions and the
  arrangement and takes ONE decision (`decideSketchSelectionMode`,
  `forgeshape_cad_body.cpp:849-865`).
- `:1484-1525` `toggleRegion` (LoopRegions), `:1704-1744` `togglePlanarFace`
  (PlanarFaces), `:1527-1573` `toggleRegionAt` (the canvas tap), `:1579-1596`
  `selectionChosen`/`unchosenStatus`. The selection lives in the session's
  `extrude_` (`forgeshape_sketch_session.h:756`).

## 6. Profile-component derivation used by Extrude

- LoopRegions: `forgeshape_cad_feature.cpp:261-279` — `validateCadFeatureGeometry`
  then `mergeSelectedRegions(g.regions, extrudeRegions(g.extrude))` (line 276);
  loops via `sketchComponentLoops`.
- PlanarFaces: `forgeshape_cad_feature.cpp:212-242` `derivePlanarFeature` —
  `resolvePlanarFaceSelection` + `mergePlanarFaceSelection` → `planarComponents`.
- Both give components whose outer loop is CCW and whose holes are stored CCW
  (`forgeshape_sketch_region.h` `sketchComponentLoops`; `PlanarProfileLoop`,
  `forgeshape_sketch_arrangement.h:253-267`).
- Prism emission `forgeshape_cad_feature.cpp:490-613` (`appendPrism`,
  `appendCadFeatureSolid`).

## 7. Renderer / publication path used by CAD regeneration

- `forgeshape_cad_body.cpp:1172-1320` `regenerateCadBody` — the ONE path
  (`generateCadMesh` wraps it). A one-feature body skips the kernel
  (`sortSolidByTag`, line 1309); a chain validates through `cadKernelValidateSolid`
  (line 1245) and the boolean kernel.
- `CadBody::applyState` (`:1371-1411`) regenerates once and caches the mesh;
  `publishSceneObject` (`forgeshape_scene.cpp:379`) publishes it; the preview is
  `SketchSession::evaluateCandidate` (`forgeshape_sketch_session.cpp:2029-2061`)
  over `candidateState()`.

## 8. History / edit path for CAD features

- New Body: `SketchSession::commit` (`:2079-2125`) — ONE `ScopedConstructionEdit`
  around ONE `addCadBody`. First project: `commitFirstCadProject`
  (`forgeshape_project_bootstrap.cpp:19-93`) through `loadProjectDocument`.
- Edit: `SketchSession::commitEdit` (`:345-393`) — ONE `ScopedConstructionEdit`
  around `CadBody::applyState`. IDs: `CadSketchId` lifetime comment
  `forgeshape_cad_body.h:322-356`; `applyState` refuses a lowered high-water mark.

## 9. Stable straight sketch source-edge identities

- `ClosedProfile::edgeEntityId/edgeLocalIndex` (`forgeshape_sketch.h`, filled at
  `forgeshape_sketch.cpp:1127-1131` for chains and `:1199-1208` for
  rectangle/circle/polyline): a Line is `(id, 0)`, a polyline segment is
  `(id, i)` for `vertices[i] -> vertices[i+1]` (closed: `n-1 -> 0`), a rectangle
  edge is `(id, k)` over `rectangleProfilePolygon` corners CCW from `(-w/2,-h/2)`:
  0 bottom, 1 right, 2 top, 3 left. The same `(entity, edgeLocalIndex)` pair is
  what `CadFaceToken` and `FragmentRef::sourceEdgeLocalIndex` already persist.
  Circles, arcs and splines are curved (`sketchEntityIsCurved`).

## 10. The 57-fixture codec baseline

- `testdata/forge/v1/` holds 57 fixtures; `scripts/build-forge-corpus.ps1:2303-2361`
  lists them; `.github/workflows/ci-fast.yml:147-163` regenerates them under `pwsh`
  and requires 57/57 byte-identical. `scripts/host-forge-corpus-verdicts.sh` prints
  each fixture's decode verdict; the baseline capture is
  `artifacts/cad-v6-revolve-newbody/corpus-verdicts-baseline.txt`.

## Architecture decision (made before coding)

1. **Durable kind, typed payload.** `CadFeatureKind { Extrude, Revolve }` on the
   BASE feature (`CadBodyState::baseKind`) with a separate `RevolveFeature
   revolve` payload. When `baseKind == Revolve` the `extrude` field must be the
   canonical default and is never encoded, read, hashed for geometry or displayed;
   validation refuses a non-default one by name (`RevolvePayloadMismatch`). No
   Revolve value is ever stored in an Extrude field.
2. **R1 scope = base feature only.** A Revolve is a body's FIRST feature with
   operation New Body. A Revolve body carries no later features in R1
   (`RevolveLaterFeatureUnsupported`) — Revolve Add/Cut and features on a revolve
   are out of scope, and every revolve face is non-hostable.
3. **`RevolveFeature`** = its own selection (the same LoopRegions / PlanarFaces
   shapes, held in fields of its own) + `CadSketchEdgeRef axis {entityId,
   edgeLocalIndex}` + `angleDegrees` (binary64) + `RevolveDirection`. The
   feature names its sketch through the existing `baseSketchId`, so the edge ref
   does not repeat it. The selection validators that take an `ExtrudeFeature`
   are reused through a transient carrier built inside `forgeshape_cad_body.cpp`,
   never stored.
4. **Angle unit: degrees, binary64.** The prompt prefers radians "if it fits
   existing native conventions"; the product's existing authored-angle convention
   is DEGREES (Euler rotations are stored as degrees, `CLAUDE.md` rotation rule),
   and degrees makes 360 / 180 / 90 / 37.5 exact in the file and on screen.
   Range `kMinRevolveAngleDegrees (0.001) <= a <= 360`, refused by name otherwise.
5. **Axis resolution** — Line edge 0, Polyline segment i, Rectangle edge 0..3;
   Circle / Arc / Spline refused (`RevolveAxisNotStraight`); a missing entity or
   an index out of range refused (`RevolveAxisUnresolved`); no nearest edge.
6. **Format: CADB v7**, its own layout = v6 with a `kind u8` after each feature
   id and a kind-specific payload. Written ONLY when a body carries a Revolve; a
   v6-representable document writes exactly the v6 bytes it writes today.
7. **Geometry** — `forgeshape_cad_revolve.{h,cpp}`: deterministic angular
   segments `sketchArcSegmentCount(angle)` (32 for 360°, the circle density),
   a full turn closes its seam topologically (ring index wraps, no caps), a
   partial turn has two caps, on-axis profile vertices collapse to ONE apex
   vertex per fan (a vertex touching the axis between two swept edges gets one
   apex per fan on a full turn, so the link of every vertex is a single disc),
   and every candidate passes `cadKernelValidateSolid`.
8. **Session** — a volatile `featureKind_` in Ready (Extrude | Revolve) with an
   axis-pick sub-state; the selection stays the session's one selection; the
   revolve candidate is evaluated by the same latest-only evaluation; the commit
   is the existing New Body commit (and the first-project bootstrap).
9. **Faces** — a revolve exposes semantic face tokens (CapPlane = start cap,
   CapFar = end cap, Side per boundary edge/fragment) for picking only; all are
   `eligible = false` in R1, so no TopoRef or feature support can name one.
