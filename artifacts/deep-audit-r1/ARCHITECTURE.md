# Architecture as audited — DEEP-AUDIT-R1

What the code actually does at `d783d4b` (fixes applied), stated independently of the root documents and then compared with them.

## Layers

```
Android shell (Java, 51 files)         owns: lifecycle, window insets, chrome composition, drafts, presentation state
  ForgeShapeActivity ── ForgeShapeSurfaceView ── EditorWorkspaceView ── 40 view classes
        │ NativeViewport (155 static native methods; no geometry, no matrices)
JNI (forgeshape_jni.cpp, 5 870 lines)  owns: render thread, surface handshake, gesture arbitration, g_stateMutex, flat calls
Platform-neutral domain (C++17)        owns: every product truth
  scene ─ construction / imported_mesh / cad_body(+sketch, workplane, cad_face) ─ transform ─ sculpt(+history) ─ history
  project_document/bytes/state (.forge codec) ─ project_bootstrap ─ body_delete ─ import_commit
  camera ─ picking ─ selection ─ gizmo ─ sketch_session ─ support_chooser ─ input
  gltf_export ─ json ─ gltf_import ─ glb_roundtrip ─ import_preview
Renderer (Vulkan)                      owns: derived render meshes, GPU resources, pipelines; no truth
```

## The body model (verified in code)

* `SceneObject` owns for life exactly one of `ConstructionObject` (six primitives, all six parameter sets retained), `ImportedMesh` (positions, normals, indices, submesh batches with `doubleSided`, sanitized name) or `CadBody` (`CadSketch` + `ExtrudeFeature`; optional `TopoRef` face support), plus a `ConstructionTransform` (T·Rz·Ry·Rx·S, strictly positive scale), a `MeshStore`, and a `FrozenSculpt` (SculptMesh + SculptHistory + sourceStale).
* `ConstructionScene`: ordered `unique_ptr` list, monotonic `nextObjectId_` never rolled back, `hasProject()` = at least one body, `activeBody()` on an empty scene returns a counted null object. `resolveWorldModel` composes `producerWorldModel · faceFrame` for face-supported CAD bodies with a depth bound.
* Two histories that never merge: `ConstructionHistory` (snapshot per user act, capacity 64, holds detached bodies) and per-body `SculptHistory` (32 entries / 4 MiB / 1 MiB per entry, deltas only, volatile).
* `.forge`: `SCNE` + exactly one of `CONS`/`IMPT`/`CADB` per body + optional `SCUL`; `CADB` v1/v2/v3 superset chain; decode into temporary state, validate whole graph, commit in one step.

## Where the code disagrees with the documents (before this audit's docs commit)

| Statement | Where | Reality | Action |
| --- | --- | --- | --- |
| "ForgeShape comes up with a single body: a box" | PRODUCT.md | opens on Home; `sceneAddBody` creates the 2×1×0.5 box | corrected |
| Undo/Redo "drawn only in Construction" | PRODUCT.md | drawn in both modes since ARCH-OWNER-12; withdrawn only while sketching | corrected |
| sketch grid "quarter of a metre … heavier line every metre" | PRODUCT.md | adaptive 1/2/5·10^k since CAD-A3 | corrected |
| "no … arc, spline or face-based plane" | PRODUCT.md Not yet | all three implemented | corrected |
| "A sketch grid … does not exist" / "There is no sketching and no extruding" | PRODUCT.md | both exist | corrected |
| imported mesh "no sculpting yet" | PRODUCT.md | IMPORT-01B | corrected |
| "A body has two possible representations" | ARCHITECTURE.md §Durable import | three | corrected |
| "No snapping, and no Sketch grid" | ARCHITECTURE.md §Current boundaries | the sketch grid snaps | corrected |
| "one of two representations" | PROJECT_STATUS.md Current state | three | corrected |
| sketch grid fixed / top view at pitch clamp (debt) | PROJECT_STATUS.md Technical Debt | resolved by CAD-A3 | marked RESOLVED |
| "Process-scoped with no save, load or undo" (sculpt) | PROJECT_STATUS.md Technical Debt | saved (E2E-R1A), undo (ARCH-OWNER-12) | corrected |
| "no camera read-back across JNI" ×2 | PROJECT_STATUS.md | debug-only `debugCameraPose`/`debugSetCameraPose` | corrected |
| Still-out list names arcs, splines, face-based planes; "five sketch tools" | PROJECT_STATUS.md Next Stage | shipped / seven | corrected |
| Source comments: "Sculpt has no undo", "exactly ONE active object", "two representations", "Stage 003 … one indexed cube", "IMPORT-01A has no Start Sculpting", "spatial chooser then sketch" bootstrap | headers | superseded | corrected in commit 1 |

The domain architecture itself (ownership boundaries, platform neutrality, single dispatch points `publishSceneObject`/`buildSculptSourceMesh`/`generateCadMesh`, all-or-nothing load, one brush kernel, one anchored-surface motion) matches `ARCHITECTURE.md`'s description; no architectural violation was found. Dependency direction is one-way as drawn in `REPOSITORY_MAP.md`.

## Structural debt observed (not defects)

* `forgeshape_jni.cpp` is 5 870 lines and the only place gesture arbitration, publication and lifecycle meet; it is coherent but is the file every stage touches (F-14, P3).
* Five representation `if/else` chains (see `REPOSITORY_MAP.md`); the ARCH-HEALTH-01 unification moment has passed (F-13, P3).
* `sculptSession()` rebinding on every access is what makes F-02/F-08 possible: a borrowed pointer refreshed by a global accessor is a latent hazard whenever the active body can change with a stroke open (F-02 closed the reachable case).
* `forgeshape_mesh_fixtures.cpp` defines `buildSpherifiedBox` outside its anonymous namespace with no header declaration (external linkage by accident) — P3 hygiene (F-15).
