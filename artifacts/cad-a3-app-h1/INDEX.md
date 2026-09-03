# CAD-A3 + APP-H1 - evidence index

**Result: `FAIL-CAD-A3-APP-H1`** - not every acceptance criterion is met. The
CAD-A3 domain and the viewport-first face-sketch feature are complete and
verified; APP-H1 Home and several breadth items are honestly deferred (below).

## What IS done and verified

`Existing CAD project -> New Sketch -> Pick plane or face in 3D -> tap a world
plane OR a planar CAD face -> exact-normal orthographic sketch -> draw ->
Extrude New Body -> a durable, editable, face-supported CAD body -> its producer
cannot be deleted while it stands -> saved and reopened with the dependency
intact.`

- **Semantic CAD face topology + TopoRef** (`FACE_TOPOLOGY.md`,
  `TOPOREF_CONTRACT.md`): caps and per-edge sides, stable feature tokens, never
  a triangle index, lineage-checked, fail-closed.
- **Dependency graph + transform contract** (`DEPENDENCY_GRAPH.md`,
  `TRANSFORM_CONTRACT.md`): a face-supported body's placement is derived from
  its producer and follows a parent Move / Rotate / edit; the graph is acyclic
  and delete-with-dependents is refused.
- **CADB v2 persistence** (`CADB_V2_DATA_CONTRACT.md`): v1 byte-identical,
  v2 adds the support, load validates the whole graph before applying.
- **Exact sketch camera + adaptive grid** (`GRID_CAMERA_UX.md`): no pitch clamp,
  view-adaptive nice grid, typed values never re-snapped.
- **Spatial support chooser** (`SPATIAL_PLANE_PICKING.md`): world planes and CAD
  faces picked in the viewport, tap-to-aim/tap-to-commit.

| Gate | Result |
| --- | --- |
| Baseline HEAD | `3a00e0be6c528c9f96ae1f004bdaf70bc67afb82` (matched, clean) |
| Native suites | **19/19**, the new CAD-A3 suite 44 checks, 0 failed |
| Golden digests | all four CAD digests unchanged |
| JVM | 70/70 |
| Builds | debug + release, `arm64-v8a` + `x86_64` |
| Device guards | `DEV2-01..07`, `DEV3-01..06` PASS |
| Corpus | 16 fixtures unchanged (CADB v1 byte-identical) |
| Focused device | `SpatialSketchTest` OK (2), `SketchExtrudeTest` OK (9) |
| Authoritative aggregate | see `FULL_SHARDED.txt` |
| Device | `emulator-5580` = `ForgeShape_Stage006`; `emulator-5554` never contacted |

## What is DEFERRED, and why (honest)

1. **APP-H1 Home / New Project routing and the empty New-CAD bootstrap**
   (`HOME_FLOW.md`): New CAD needs an empty scene, which conflicts with the
   load-bearing "no empty project" invariant (`activeBody()` dereferences
   `bodies_.front()`). Deferred rather than risk destabilising the app lifecycle
   and ~20 EditorWorkspace test classes.
2. **Independent PowerShell v2 corpus + the five named v2 fixtures** (Part I):
   not created; the v2 codec is proved by native round-trip instead.
3. **Stylus hover device verification and the face-first contextual shortcut**
   (Parts K1/D3): wired or absent, not device-verified.
4. **Deterministic screenshot contact sheet** (Part K3): not captured; geometry
   and interaction are proved by the native suite and the device E2E.

## Files

`FACE_TOPOLOGY.md`, `TOPOREF_CONTRACT.md`, `DEPENDENCY_GRAPH.md`,
`TRANSFORM_CONTRACT.md`, `CADB_V2_DATA_CONTRACT.md`, `GRID_CAMERA_UX.md`,
`SPATIAL_PLANE_PICKING.md`, `HOME_FLOW.md`, `TEST_RESULTS.md`, `DEVICE_E2E.md`,
`PERFORMANCE.md`, `NATIVE_SELFTEST_CADA3.txt` (19 suites at launch),
`FOCUSED_SPATIAL_SKETCH.txt`, `FULL_SHARDED.txt`, `FULL_SHARDED_RUN1_FAIL.txt`
(the first aggregate, which flagged the device grid assertions).
