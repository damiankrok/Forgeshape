# MODELING-FOUNDATIONS-R1 — BEFORE (pre-flight audit)

Read-only audit of the tree the task starts from, taken before any product edit.
Every claim cites `file:line` on the baseline. Paths under `app/src/main/cpp/`
are written bare (`forgeshape_scene.h:58`); Java paths are under
`app/src/main/java/com/forgeshape/app/`.

## 0. Baseline

| Ref | SHA | Verified |
| --- | --- | --- |
| `origin/main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` | yes |
| `origin/feature/cad-v6-sketch-face-r1` | `bbae765645a318f83451e9a61b6a45cf2cb17e33` | yes |
| `origin/feature/cad-v8-sketch-drafting-toolkit-r1` | `a3643b5330f28f3590ba75f1a89c39e2424b3eed` | yes |
| tested prior product | `df7db66849863d0841c28ce1d668cb54ddef51ce` | ancestor of `a3643b5` (docs-only after it) |

Task branch `feature/modeling-v9-parametric-freeform-surface-r1` created at
exactly `a3643b5`. Host baseline on this tree:
`HOST_SELFTESTS_OK (4157 checks, 0 failed)` (23 suites), matching
`PROJECT_STATUS.md`.

Local environment: Linux container with the pinned SDK/NDK
(`/opt/android-sdk/ndk/29.0.14206865`, build-tools 36.1.0, CMake 3.22.1),
JDK 21, no `/dev/kvm` and no emulator. Device evidence therefore comes from
GitHub `CI DEVICE` (`docs/CI_CLOUD.md`), never from a local emulator.

## 1. Scene

| Topic | Evidence | Finding |
| --- | --- | --- |
| `BodyRepresentation` | `forgeshape_scene.h:58-71` | Closed enum `Construction=1, Imported=2, Cad=3`, one per body for its whole life; no conversion between them (`:52-57`). |
| Ownership | `forgeshape_scene.h:75-250` | `SceneObject` owns `construction_` / `cad_` by `unique_ptr`, `imported_` by value, plus `transform_`, `meshStore_`, `frozen_`, name, visible, locked (`:227-249`). One constructor per representation (`:79-101`). Not copyable (`:103-104`). |
| Creation | `forgeshape_scene.cpp:17-37, 189-213` | `addBody` / `addImportedBody` / `addCadBody` mint an id only after the state validates (`addCadBody` regenerates once first, `:190-208`). `makeCadBody` (`:158-164`) rebuilds a CAD body under an explicit id for history. |
| Publication | `forgeshape_scene.cpp:379-423` | `publishSceneObject` is the ONE dispatch: Construction → `publishConstructionObject`; Cad → `CadBody::generateMesh` → `MeshStore::publish`; Imported → `buildDrawData`. Anything else → `kNoMeshRevision`. |
| Render snapshot | `forgeshape_scene.cpp:266-321` | `snapshot()` is the one list the renderer draws and picking casts against; hidden bodies (`:270-283`) and isolate (`:284-291`) are skipped there and nowhere else. |
| Picking | `forgeshape_selection.h:40-46, 60-61`; `forgeshape_selection.cpp:14-100` | `pickScene` → `pickSceneSnapshot(viewSceneSnapshot())`; a hit is `{objectId, triangleIndex, distance, position}`; `frontFacesOnly = !renderBothSides`. No barycentrics. |
| Body transform | `forgeshape_scene.h:208-215` | One `ConstructionTransform` per body, representation-neutral. |
| Visibility / lock | `forgeshape_scene.h:184-206` | Durable, representation-neutral; hidden enforced only in `snapshot()`. |
| Frozen Sculpt | `forgeshape_scene.h:220-225`; `forgeshape_scene.cpp:426-483` | `FrozenSculpt` per body; `buildSculptSourceMesh` refuses a CAD body (`:436-439`) and seeds Construction / Imported only. |

## 2. CAD

| Topic | Evidence | Finding |
| --- | --- | --- |
| `CadBodyState` | `forgeshape_cad_body.h:540-567` | `sketches[]`, `nextSketchId`, `baseSketchId`, `baseKind`, `extrude`, `revolve`, `laterFeatures[]`, `nextFeatureId`. Plain, copyable, bounded, nothing derived. |
| Retained sketch table | `forgeshape_cad_body.h:442-514` | `CadSketchRecord {sketchId, hasFeatureSupport, featureSupport, sketch}`; ids body-local, non-zero, minted from `nextSketchId`; lifetime rule `:447-475`. |
| Base feature | `forgeshape_cad_body.h:547-558` | Implicit id 1 (`kCadFeatureId`, `forgeshape_sketch.h:504`), always New Body, kind Extrude or Revolve, payload of that kind only. |
| Later feature chain | `forgeshape_cad_body.h:516-526, 561`; cap `:418` | `CadFeature {featureId, operation (Add/Cut), sketchId, extrude}`; at most `kMaxCadFeatures` = 16 features in all. |
| `CadFeatureKind` | `forgeshape_cad_body.h:289-298` | `Extrude, Revolve`. |
| Extrude payload | `forgeshape_cad_body.h:174-214` | Selection (LoopRegions / PlanarFaces), `depth`, `direction`, `extent`, `secondDistance`. |
| Revolve payload | `forgeshape_cad_body.h:353-369` | Selection, `axis` (`CadSketchEdgeRef`, `forgeshape_sketch.h:730-733`), `angleDegrees`, `direction`. New Body only, no later features (`forgeshape_cad_body.cpp:1192-1194`). |
| Feature ids | `forgeshape_cad_body.h:518-526, 562-566` | Strictly ascending, never an index, high-water mark stored; `applyState` refuses to lower it (`forgeshape_cad_body.cpp:1600-1604`). |
| Chain view | `forgeshape_cad_body.h:647-672` | `CadFeatureView` + `cadFeatureAt` / `findCadFeature` walk the chain without caring which slot a feature lives in. |
| Feature editing | `forgeshape_sketch_session.h:289-328`; `forgeshape_sketch_session.cpp:261-402` | `beginEditFeature(body, state, featureId, frame, startReady)` stages a COPY of the whole state; `commitEdit` regenerates the whole candidate first, checks dependents, then applies inside ONE `ScopedConstructionEdit` (`:385-396`); `cancel` (`:213-250`) drops the staged copy and writes nothing. |
| Ordered regeneration | `forgeshape_cad_body.cpp:1375-1536` | Base, then each later feature through one kernel boolean, in order; atomic — published only when the whole chain passes. |
| First failure status | `forgeshape_cad_body.h:822-834`; `forgeshape_cad_body.cpp:1378-1385, 1393, 1477-1515` | `CadRegenerationReport {status, failedFeatureId}` already names the first failing feature. `validateCadChain(state, &failedFeatureId)` (`forgeshape_cad_feature.h:135`) does the same structurally. |
| Staged candidate evaluation | `forgeshape_sketch_session.h:74-91`; `forgeshape_sketch_session.cpp:2646-2669` | `CadCandidateEvaluation.failedFeatureId` carries the downstream failure of a staged edit; JNI exposes it as `cadExtrudeToolState` slot 25 (`forgeshape_jni.cpp:4980`). |
| Current feature/history UI | `CadFeatureEditorView.java:141-144, 254-299` | The precision surface lists features as rows (`cad_feature_row`) only when the chain has more than one feature (or is a Revolve); a row calls `onEditCadFeatureRequested` → `sketchBeginEditFeature(body, id, true)` (`EditorWorkspaceView.java:4242-4267`). Sketches are not rows; no failing-row state exists; nothing lists the chain as a timeline. |
| Relationship to project history | `forgeshape_history.h:76-110`; `forgeshape_history.cpp:65-86` | A Construction step snapshots every body's `CadBodyState` (before/after); CAD uses Construction history, never `SculptHistory`. `kConstructionHistoryCapacity` = 64 steps (`forgeshape_history.h:66`), no byte cap. |

Gap for Phase A: the domain already regenerates in order, attributes the first
failure and stages an edit as one transaction. What does not exist is a DERIVED
timeline (sketch + feature rows in construction order with stable ids and a
per-row state), a user-visible History surface, and a failing-row presentation
with Fix / Cancel.

## 3. Sculpt

| Topic | Evidence | Finding |
| --- | --- | --- |
| Source / frozen truth | `forgeshape_sculpt.h:404-575, 797-823` | `SculptMesh` owns `MeshVertex` positions + fixed `indices_` + topology; `FrozenSculpt {mesh, sourceStale, history}` per body. |
| Mesh deformation only | `forgeshape_sculpt.h:398-403, 472-477`; `forgeshape_sculpt.cpp:544-556, 773-845, 880-1300` | `setVertexPosition` is the ONLY geometry mutator; every brush (Grab, Clay, Smooth, Flatten, Inflate, Crease) writes positions through it; Mask writes weights. Topology never changes after `freezeFrom` (`forgeshape_sculpt.cpp:465-518`). There is no control cage, no subdivision and no limit surface anywhere in Sculpt. |
| `SculptHistory` | `forgeshape_sculpt_history.h:77-123, 232-339` | Per-body stroke deltas (indices + before/after positions + mask side), 32 entries / 4 MiB / 1 MiB per entry, volatile, never serialized. |
| Brush/session ownership | `forgeshape_scene.cpp:532-540` | One process `SculptSession` re-bound to the active body's `FrozenSculpt` on every access. |

Conclusion: Sculpt is per-vertex displacement of a fixed triangle mesh. A
Freeform control cage cannot be expressed in it and must not be stored in it.

## 4. Renderer

| Topic | Evidence | Finding |
| --- | --- | --- |
| Runtime mesh publication | `forgeshape_mesh.h:51-55, 65-67, 164-166`; `forgeshape_mesh.cpp:100-125` | `MeshVertex {position[3], color[3], mask}`; `MeshStore::publish(..., renderBothSides)` validates, mints a monotonic revision; caps 4 M vertices / 24 M indices. |
| Two-sided | `forgeshape_render_mesh.cpp:339-355`; `forgeshape_renderer.cpp:2463-2464` | Back-face culling always on; `renderBothSides` duplicates triangles with reversed winding in the derived render mesh; picking sees only the source triangles. |
| Revision-gated upload | `forgeshape_renderer.cpp:1051-1145` | `syncBody` uploads only when the revision changed. |
| Triangle picking → semantics | `forgeshape_cad_body.h:802-820`; `forgeshape_cad_face.cpp:51-74`; `forgeshape_support_chooser.cpp:182-205` | A CAD regeneration carries `triangleFace` (per triangle, into a face table); the chooser maps `triangleIndex` → semantic face. A triangle index is never stored. |
| Overlay hook | `forgeshape_sketch_overlay.h:28-113`; `forgeshape_renderer.cpp:1586-1733`; `forgeshape_jni.cpp:1865-1922` | One `SketchOverlay` slot (world-space LINE_LIST, 7 styles, highlight flag per vertex, 65536 vertices), mutually exclusive producers chosen in the render loop (support chooser, body dimensions, sketch). `buildBodyDimensionOverlay` (`forgeshape_body_dimension_overlay.cpp:31-172`) proves a non-sketch producer needs no renderer change. Overlay pipeline: no depth test, 1 px lines. |
| Gizmo hook | `forgeshape_gizmo.h:530-551, 829-1031`; `forgeshape_renderer.h:125` | `GizmoSnapshot` is a pure value (pivot, orientation, scale, mode); the renderer draws any snapshot. `GizmoSession` itself is bound to the active body's transform (`forgeshape_gizmo.cpp:1200-1210, 1438-1456`); its solvers (`solveAxisParameter`, `intersectRayPlane`, `signedAngleAround`, `gizmoWorldScale`, `projectWorldToScreen`) are pure (`forgeshape_gizmo.h:368-448`). |
| Touch routing | `forgeshape_jni.cpp:8183-8813` | Order: support chooser → sketch session → gizmo → sculpt → camera + selection fallback (`:8754`). |

## 5. Format

| Topic | Evidence | Finding |
| --- | --- | --- |
| Header | `forgeshape_project_document.h:75-101`; `forgeshape_project_document.cpp:1557-1570, 2848-2893` | `FORGESH1`, major 1 / minor 0 (minor ignored), 28-byte header, flags bit0..bit3 (Construction, Sculpt, Imported, Cad); an unknown flag bit is `BadHeader`. |
| Sections | `forgeshape_project_document.h:106-110`; `forgeshape_project_document.cpp:2669-2832` | `SCNE, CONS, SCUL, IMPT, CADB`; 24-byte section header with CRC-32; unknown required tag → `UnknownRequiredSection`; header flags must equal the tags present. |
| Representation encoding | `forgeshape_project_document.cpp:757-895`; `forgeshape_project_state.cpp:92-105, 140-171, 244-275` | No representation tag in `SCNE`: a body's representation is whichever of `CONS` / `IMPT` / `CADB` names it, at most one (`covered[]`), at least one (`runtimeCanEvaluateProject`). |
| `CADB` v8 | `forgeshape_project_document.h:140-199`; `forgeshape_project_document.cpp:1450-1470, 1600-1608, 2454-2488` | One `CADB` section whose version (1..8) is the highest any body needs; v8 when any sketch carries drafting truth. |
| Conditional writer | `forgeshape_project_document.cpp:480-568, 1450-1458` | Every version predicate looks only at what a body actually carries; a project needing none of a newer layout keeps its older bytes. |
| Fingerprint | `forgeshape_project_state.cpp:684-755` | FNV-1a over semantic values per representation; newer blocks are mixed only when the body uses them. |
| Corpus | `testdata/forge/v1/` (66 files); `.github/workflows/ci-fast.yml:133-163` | 66 fixtures, regenerated by `scripts/build-forge-corpus.ps1` (table `:2601-2668`) and compared byte-for-byte in CI FAST (`test ... -eq 66`, `:160-163`). 61 digests are pinned in C++ self-tests; 5 envelope fixtures are behaviour-tested. |
| Bounds pattern | `forgeshape_project_bytes.h:96-190`; e.g. `forgeshape_project_document.cpp:1889-1896` | Count read, `0`/over-cap → `ImpossibleCount`, `count × minimum record` against `remaining()` → `Truncated`, only then allocate; every payload fully consumed. |

## 6. Startup self-tests

`forgeshape_jni.cpp:2011-2046` runs 23 suites from `NativeViewport.start()`,
debug-only (`CMakeLists.txt` `FORGESHAPE_SELFTEST_SOURCES`), checked by
`scripts/ci-device-smoke.sh` (23 tokens, in order) and the release guard.
Revolve and drafting checks run INSIDE the CAD-feature suite
(`forgeshape_cad_feature_selftest.cpp:5781-5828`), adding no token.

## 7. What each phase must add (summary)

* **Parametric History** — a derived timeline over `CadBodyState`, its JNI
  read, a History surface, a failing-row presentation with Fix / Cancel, and
  Revolve axis editing reachable from the timeline. No new truth.
* **Freeform/SubD** — a fourth `BodyRepresentation` whose truth is a control
  cage; Catmull-Clark derivation; cage editing tools; history snapshot fields;
  a new required section; a cage overlay producer and cage picking.
* **Surface** — a fifth `BodyRepresentation` whose truth is an ordered
  surface feature list over retained sketches; derived patches; staged edit
  with first-failure attribution; a new required section; two-sided render.
* Every place a representation is decided must learn the two new ones:
  `forgeshape_history.cpp:51-81, 251-334` (capture / compare / fabricate /
  restore — an unknown representation is silently rebuilt as a Construction
  body today), `forgeshape_body_commands.cpp:248-269` (Duplicate),
  `forgeshape_project_state.cpp:92-171, 244-275, 702-723` (codec bridge and
  fingerprint), `forgeshape_gltf_export.cpp:202-268` and
  `forgeshape_glb_roundtrip.cpp:45-75` (export), `forgeshape_scene.cpp:379-483`
  (publish, sculpt seed), and `forgeshape_jni.cpp:6343` (representation code).
