# CAD-V6-S1 — BEFORE (source truth at `6c9f156`, written before any product edit)

Baseline: `origin/main = 6c9f156c1df91fe1a55445aed6186105a075ab31` (PF-S1
candidate `7d09e4a` is an ancestor). Branch `feature/cad-v6-sketch-face-r1`
created from exactly that commit; `git status --short` and `git diff --check`
empty. Host baseline: `HOST_SELFTESTS_OK (3852 checks, 0 failed)`, CAD_FEATURE 223. Corpus baseline: `scripts/build-forge-corpus.ps1` regenerates all 44
`testdata/forge/v1` fixtures byte-identically on this host (pwsh).

## 1. Sketch ownership today — inline, by value, no identity

`forgeshape_cad_body.h`:

```
CadBodyState
    CadSketch        sketch           feature 1's sketch (the base), BY VALUE
    ExtrudeFeature   extrude          feature 1's selection + extrusion
    vector<CadFeature> laterFeatures  features 2..n
CadFeature
    uint32_t              featureId
    CadFeatureOperation   operation   Add | Cut
    CadFeatureSupport     support     (featureId, CadFaceToken, lineageToken)
    CadSketch             sketch      BY VALUE, forced XY, no TopoRef
    ExtrudeFeature        extrude
```

* A `CadSketch` has NO id. The only ids inside it are per-entity
  (`SketchEntityId`, minted by `nextEntityId`, never reused within a sketch).
* Two features cannot reference one sketch: there is nothing to reference.
* The base feature has no `CadFeature` record. `cadFeatureAt(state, 0)`
  synthesises `CadFeatureView{kCadFeatureId, NewBody, &state.sketch,
  &state.extrude, support = nullptr}` (`cad_body.cpp:318-331`).
* Two support mechanisms exist by design: the BASE sketch may carry a
  cross-body `TopoRef` inside `CadSketch` (`hasFaceSupport`/`faceSupport`,
  plane then canonically XY); a LATER feature carries a `CadFeatureSupport`
  naming a face of an EARLIER feature of the same body, and its sketch must be
  XY with no TopoRef (`buildCadChainGeometry`, `cad_feature.cpp:224-232`;
  `SketchSession::candidateState` strips it, `sketch_session.cpp:1486-1492`).
* Support/placement therefore lives in two places: `CadSketch` (plane,
  TopoRef) for the base, and `CadFeature::support` for later features.
  Placement is DERIVED every time: base = `workplaneFrame64(plane)` at the body
  origin; later = the named face's frame of the earlier feature's OWN prism.

## 2. Feature ids and "next id"

* Base id is the constant `kCadFeatureId` (1). Later ids strictly ascending,
  each > 1 (`buildCadChainGeometry` refuses otherwise as `TooManyFeatures`).
* `nextCadFeatureId(state)` = `laterFeatures.back().featureId + 1`, or 2 —
  DERIVED, no stored high-water mark (`cad_body.cpp:343-346`). The audit
  (`artifacts/fable-cad-architecture-audit-r1/SKETCH_FEATURE_HISTORY_AUDIT.md`
  §1, `COMPLEXITY_RED_FLAGS.md`) flagged it: ids would be reused the day a
  feature can be deleted. No delete-feature API exists today.

## 3. Caps

| Cap | Value | Owner |
| --- | ---: | --- |
| `kMaxCadFeatures` (base included) | 16 | `cad_body.h` |
| `kMaxSketchEntities` / `kMaxPolylineVertices` | 256 / 256 | `sketch.h` |
| `kMaxSplinePoints` | 32 | `sketch.h` |
| `kMaxProfileVertices` | 1024 | `sketch.h` |
| `kMaxProfileRegions` / `kMaxRegionHoles` | 16 / 64 | `sketch_region.h` |
| `kMaxArrangementSourceEdges` / `kMaxArrangementContacts` | 1024 / 4096 | `sketch_arrangement.h` |
| `kMaxProjectBodies` | 4096 | codec |

## 4. `CADB` v5 record layout (DATA_PACKAGE_SPEC.md §7f; codec `forgeshape_project_document.cpp:1097-1166` write, `1684-1847` read)

```
u64 objectId | u8 workplaneCode | u8 supportKind (+TopoRef 29 B if 1)      <- v2
u32 nextEntityId | u32 profileEntityId | u8 extentCode (v4) | u8 directionCode
f64 depth | f64 secondDistance (v4) | u32 entityCount + entities (§7b/§7d)
REGIONS (first feature)                                                    <- v5
u32 laterFeatureCount 0..15
  per later: u32 featureId | u8 op (2/3) | u32 supportFeatureId | u8 faceKind
             u32 faceEdgeEntityId | u32 faceEdgeLocalIndex | u64 lineage
             u32 nextEntityId | u32 profileEntityId | u8 extent | u8 direction
             f64 depth | f64 secondDistance | entities | REGIONS
```

Writer picks the LOWEST version per section (`cadDocumentNeedsV2..V5`); v5 is
required only for a non-simple region selection or a later feature.

## 5. How v1..v5 loading builds a body today

`decodeCadPayload` fills `state.sketch` (plane, v2 support, nextEntityId,
entities), `state.extrude` (profile, v4 extent/second distance, v5 REGIONS) and
`state.laterFeatures` (v5; each sketch forced `plane = XY`). Absent fields keep
their defaults (One Side, secondDistance 0, no holes, no later features).
`validateProjectDocument` then calls `validateCadBodyState` per body, runs
`generateCadMesh` (kernel) when later features exist, and validates the
cross-body `TopoRef` graph. Load (`forgeshape_project_state.cpp`) is
all-or-nothing via `runtimeCanEvaluateProject` + `loadProjectDocument`.

## 6. PF-S1 types that v6 will serialize (`forgeshape_sketch_arrangement.h`)

```
ArrangementCutKind : SourceStart = 0 | Intersection = 1 | SourceEnd = 2
ArrangementCut     { kind, partnerEntityId, partnerEdgeLocalIndex, ordinal }
FragmentRef        { sourceEntityId, sourceEdgeLocalIndex, startCut, endCut, reversed }
FragmentCycle      = vector<FragmentRef>
PlanarFaceRef      { FragmentCycle outer; vector<FragmentCycle> holes }
```

Canonical form (from `deriveSketchArrangement`): outer CCW, holes CW, each
cycle rotated to its smallest `FragmentRef` (unique: a fragment appears at most
once per cycle), holes sorted by `compareCycle`; faces sorted by
`comparePlanarFaceRef`. A cut at a source end is `SourceStart`/`SourceEnd` with
zero partner and ordinal; a start cut is never `SourceEnd` and an end cut never
`SourceStart`. `resolvePlanarFaceRef` = exact tuple equality, no fallback.
Status enum is module-local: `Ok, InvalidSketch, UnsupportedCurve,
AmbiguousOverlap, DegenerateFace, CapExceeded`. Nothing in the product calls it.

## 7. Consumers of the inline sketch (what the refactor must carry)

`state.sketch` / `feature.sketch` read or written in: `cad_body.cpp` (18),
`cad_feature.cpp` (5, via `CadFeatureView`), `project_document.cpp` (29),
`project_state.cpp` (fingerprint, 11), `scene.{h,cpp}` (face support, 3),
`sketch_session.cpp` (12: `beginEditFeature`, `candidateState`), `jni.cpp`
(19, mostly through `CadBody::sketch()` and `CadFeatureView`), and ~450
self-test sites. `CadBody::sketch()` returns the base sketch.

## 8. Decisions this reading forces (recorded before editing)

* The base feature keeps its two implicit invariants (id 1, NewBody) and its
  `extrude` slot on `CadBodyState`; its SKETCH moves into the table and is
  referenced by id like every other feature's. No inline sketch survives.
* Support/placement moves onto the sketch RECORD, because a sketch shared by
  two features must stand in one place.
* A monotonic `nextFeatureId` is adopted (the audit's finding), persisted only
  in v6; v1..v5 derive `last + 1`.
* A PlanarFaces selection is validated (arrangement + exact resolution) but NOT
  regenerated in S1: the runtime refuses to load it by name, the
  `runtimeCanEvaluateProject` pattern ("one thing the FORMAT allows and this
  RUNTIME does not").
