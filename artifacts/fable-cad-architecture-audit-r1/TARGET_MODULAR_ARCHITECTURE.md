# Target modular architecture (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

A target, not code. Nothing here is implemented. It keeps every invariant in
`CLAUDE.md` (no engine, one approved kernel behind one file, the domain
platform-neutral, one Construction history, Sculpt history separate, the
renderer derived-only, a `.forge` document all-or-nothing) and moves
responsibilities to where a senior developer would look for them. Prefer
extracting coherent coordinators and adapters over renaming.

## 1. The navigation a new senior developer should have

```
app/src/main/cpp/
  domain/                 platform-neutral truth and its rules (today: forgeshape_scene, construction,
    scene/                object_id, transform, history)
    construction/
    history/
  feature/                one directory per user-facing CAD workflow, each a pure domain + a session
    sketch/               forgeshape_sketch, sketch_region, workplane, (later) sketch identity
    extrude/              forgeshape_cad_body (ExtrudeFeature, extent policy), cad_extrude_tool
    features/             forgeshape_cad_feature, cad_face, support chooser; the chain and its lineage
    kernel/               forgeshape_cad_kernel (the ONLY Manifold include), vendored third_party/
    session/              forgeshape_sketch_session (the volatile editing session, candidate, overlay)
  sculpt/                 forgeshape_sculpt, sculpt_history
  commands/               body_commands, body_delete, body_mirror, body_dimensions, dimension_overlay,
                          import_commit, project_bootstrap, session_reset (NEW)
  persistence/            project_document, project_state, project_bytes; gltf_export, gltf_import
  render/                 renderer, render_mesh, grid, matcap, selection_outline, render_recovery
  view/                   camera, picking, selection, gizmo, display, sketch_overlay (moved here),
                          input
  platform/android/jni/   forgeshape_jni_lifecycle, _render_thread, _input, _sketch_cad, _scene,
                          _sculpt, _project, _display, _debug (NEW split of forgeshape_jni.cpp)
  selftest/               the 23 *_selftest.cpp (debug-only; host-runnable)
app/src/main/java/com/forgeshape/app/
  shell/                  EditorWorkspaceView (composition + layout ONLY), WorkspaceLayoutMode,
                          insets, dismiss stack, EditorUiState
  coordinators/           ProjectLifecycleCoordinator, SketchCadChromeCoordinator,
                          ChromeRefreshScheduler (NEW extractions)
  chrome/                 every child view; AnchoredValueEditor (NEW shared), ViewportAnchorSpace,
                          CadHudPresentation, SketchChromePolicy
  platform/               NativeViewport (declarations), NativeViewportDebug (NEW, debug source set),
                          ProjectTransfer, AutosaveController, AppPreferencesStore
```

Whether the directories are physical or a naming convention inside the flat
`cpp/` tree is an engineering choice (CMake and the host self-test script glob
the flat tree today); the OWNERSHIP boundaries below are what matter.

## 2. Module definitions

For each: responsibility · owns · must not own · public interface · allowed
dependencies · forbidden dependencies · testing layer.

### `domain/scene`
- Responsibility: the project's bodies as identities with ONE representation each, their order, placement, name/hidden/locked, the active body, the id allocator; the ONE snapshot list.
- Owns: `ConstructionScene`, `SceneObject`, `ObjectId` allocation, `resolveWorldModel`, `snapshot` (hidden + isolate resolved here and nowhere else).
- Must not own: any representation's rules (it holds them, it does not interpret them), any history, any session, any camera.
- Interface: body CRUD by id, `snapshot(restriction)`, representation accessors returning pointers so every caller says what it does about "none".
- Allowed deps: `construction`, `sculpt`, `feature/extrude` (the `CadBody` type), `imported_mesh`, `mesh`, `math`.
- Forbidden: `render/`, `view/`, `persistence/`, `platform/`, any session.
- Tests: `forgeshape_scene_selftest`, `HomeFlowTest` (null-object count).

### `domain/history`
- Responsibility: the ONE Construction transaction boundary and undo/redo.
- Owns: `ConstructionHistory`, `ScopedConstructionEdit`, detached-body holds, the session-initialization bracket, (target) a per-step byte budget.
- Must not own: sculpt state, mesh bytes, any presentation.
- Interface: `begin/commit/cancel`, `undo/redo`, `clear()` (product reset, documented as such).
- Allowed: `scene`. Forbidden: everything above it.
- Tests: `forgeshape_history_selftest`, `EditorWorkspaceHistoryTest`.

### `feature/sketch`
- Responsibility: authored 2D entities on a workplane, closed loops, regions (nesting), and (target) the merged boundary of a region selection.
- Owns: `CadSketch`, `SketchEntity`, `extractClosedProfiles`, `extractSketchRegions`, `ProfileRegionRef`, `validateRegionSelection`, (target) `mergeSelectedRegions` (parity rule), `sketchRegionHatch`, `tessellateSketchCurve`.
- Must not own: extrusion, features, camera, pixels, a session.
- Interface: pure functions over `CadSketch` values.
- Allowed: `math`, `workplane`. Forbidden: `session/`, `view/`, `render/`, `kernel/` (a 2D merge must not need the 3D kernel).
- Tests: `forgeshape_cad_selftest` (`CADR0_*`), `forgeshape_cad_feature_selftest` (`CADVS_REG_*`), `forgeshape_sketch_ux_selftest`.

### `feature/extrude`
- Responsibility: what an extrusion IS (regions, two distances, mode, side, operation) and the pure policies over it (extent transitions, anchors, control scale, the manipulator's drag arithmetic).
- Owns: `ExtrudeFeature`, `ExtrudeExtentMode`, `extrudeFeatureWith*`, `CadExtrudeAnchors`, `CadExtrudeControlScale`, `CadExtrudeManipulator`, (target) the dimension-leader emitter for the extrude annotation.
- Must not own: the session's volatile intent, a camera, a pixel constant that is Android's (the 48 dp floor stays in `CadHudPresentation`).
- Interface: value types and pure functions.
- Allowed: `feature/sketch`, `math`, `view/camera` (for projection in the manipulator only). Forbidden: `session/` from the header (today `cad_extrude_tool.cpp` includes `sketch_session.h`; that edge should point the other way only).
- Tests: `forgeshape_sketch_ux_selftest` (`CADUXS1_*`), `CadHudPresentationTest` (Java half).

### `feature/features` (the chain)
- Responsibility: `CadBodyState`, the ordered feature chain, semantic faces and lineage, supports, regeneration through the kernel, the derived face table.
- Owns: `CadBody`, `CadFeature`, `CadFeatureSupport`, `CadFaceToken`, `cadFeatureTopologySignature`, `buildCadChainGeometry`, `regenerateCadBody`, `SupportChooser` (the spatial pick of a face), (target) `sketches[]` with sketch ids and a stored feature-id high-water mark.
- Must not own: a sketch session, a camera pose policy, any render key.
- Interface: `applyState`, `regenerated()`, `validateCadBodyState`, face resolution.
- Allowed: `feature/sketch`, `feature/extrude`, `feature/kernel`, `math`, `mesh`. Forbidden: `session/`, `render/`, `platform/`.
- Tests: `forgeshape_cad_feature_selftest` (`CADVS_OPS_*`, `CADVS_SES_*` via the session), `CadVerticalSliceTest`.

### `feature/kernel`
- Responsibility: the ONE boolean/triangulation adapter over a vendored kernel.
- Owns: `forgeshape_cad_kernel.{h,cpp}`; the only Manifold include in the tree.
- Must not own: any ForgeShape type semantics beyond `CadSolid` in/out; no stored output.
- Interface: `cadKernelBoolean`, `cadKernelTriangulateRegion`, `cadKernelValidateSolid`.
- Allowed: `third_party/manifold`, `math`. Forbidden: everything else.
- Tests: the `CADVS_*` kernel checks; the capability corpus (`KERNEL_GATE.md`).

### `feature/session`
- Responsibility: the ONE volatile sketch/extrude editing session: draft state, the staged copy of a committed body, the candidate evaluation (latest-only), the overlay line list, the view frame policy, the drag capture.
- Owns: `SketchSession` and its `candidateState`/`evaluateCandidate`/`commit*`.
- Must not own: a second model of the extrusion; a camera (it reads one); a pixel policy other than the tap slop and the snap tolerance.
- Interface: begin/beginOnFace/beginEditFeature/commit/commitEdit/cancel; setters that all land in `applyExtrudeFeature`; reads for chrome.
- Allowed: `feature/*`, `domain/*`, `view/camera`, `view/input`, `view/sketch_overlay`. Forbidden: `render/renderer`, `platform/`.
- Tests: `CADVS_SES_*`, `CADUXS1_*`, `SketchUxTest`, `SketchExtrudeTest`.

### `commands/`
- Responsibility: every user act on a body that is not sketching or sculpting, each as ONE transaction; the project bootstraps; (target) the ONE session reset.
- Owns: `deleteSceneBody`, `body_commands`, `mirrorSceneBody`, `body_dimensions` (pure), `BodyDimensionSession`, `commitImportedGlbScene`, `commitFirstCadProject`, (target) `seedSculptProject`, `resetProcessScopedSessions`.
- Must not own: rendering, a camera, JNI types.
- Allowed: `domain/*`, `feature/*` (for the CAD bootstrap), `sculpt`, `persistence` (bootstrap through the load path). Forbidden: `render/`, `platform/`.
- Tests: `forgeshape_mirror_selftest`, `forgeshape_body_dimensions_selftest`, `ObjectsDeleteTest`, `HomeFlowTest`.

### `persistence/`
- Responsibility: the `.forge` codec (layout, validation, determinism), the fingerprint, GLB in/out; (target) a version table.
- Owns: `project_document`, `project_state`, `project_bytes`, `gltf_export`, `gltf_import`.
- Must not own: a filesystem, a `Uri`, a session, GPU state.
- Allowed: `domain/*`, `feature/features` (the `CadBodyState` type), `sculpt`, `imported_mesh`. Forbidden: `session/`, `render/`, `platform/`.
- Tests: `forgeshape_project_selftest`, corpus parity, `ProjectProcessDeathTest`, `ProjectAutosaveRecoveryTest`.

### `view/`
- Responsibility: the camera, picking, selection, the gizmo, display settings, the overlay line-list TYPE, pointer samples.
- Owns: `CameraController`, `pickScene`, `SelectionController`, `GizmoSession`, `DisplaySettingsStore`, `SketchOverlay` (moved beside `render_mesh`, so `sketch → gizmo` disappears), `forgeshape_input`.
- Must not own: project truth; a history (target: the gizmo's `ScopedConstructionEdit` becomes a `commands/` concern the gizmo calls).
- Allowed: `domain/scene`, `mesh`, `math`. Forbidden: `feature/session`, `persistence/`, `platform/`.
- Tests: camera/picking/gizmo self-tests, `EditorWorkspaceGizmoTest`.

### `render/`
- Responsibility: Vulkan, derived render meshes, uploads keyed by revision, the outline, the grid, recovery policy.
- Owns: `Renderer`, `render_mesh`, `selection_outline`, `render_recovery`.
- Must not own: anything a `.forge` byte, a history step or the fingerprint could see.
- Interface: `setScene(snapshot)`, `setSketchOverlay(line list)`, push-constant policies.
- Allowed: `view/` types, `mesh`, `math`. Forbidden: `feature/*`, `domain/history`, `persistence/`, any session.
- Tests: `forgeshape_render_mesh_selftest`, `forgeshape_render_recovery_selftest`, `SelectionOutlineTest`.

### `platform/android/jni/` (split of `forgeshape_jni.cpp`)
- `_lifecycle`: `start/stop`, `surface*`, the session-initialization bracket, `closeProject`/`loadProject` adapters (calling `commands/resetProcessScopedSessions`).
- `_render_thread`: `ViewportThread`, `renderThreadMain`, the atomics, the CAD preview cache; the ONLY TU that includes `forgeshape_renderer.h`.
- `_input`: `touchEvent` as an arbiter with one pointer-ownership state; the arbitration globals become one struct.
- `_sketch_cad`: the 56 sketch/CAD exports, the sketch-view camera policy, the frame resolvers, `cadExtrudeToolState`.
- `_scene`, `_sculpt`, `_project`, `_display`: their families.
- `_debug`: every `debug*`, the preview family, the report probes, the self-test runners — under `#ifndef NDEBUG`, exported through `NativeViewportDebug`.
- Shared: one `jni_common.h` with `g_stateMutex`, the accessors, `utf16ToUtf8`, the status-code bridges.
- Must not own: domain rules (each export is a lock + a domain call + a code).
- Tests: the device classes, unchanged; `verify-device-guards` unchanged.

### Java `shell/`
- `EditorWorkspaceView`: composition, layout mode, insets, the dismiss stack, `refreshShellPhase`. Target size: the constructor plus layout, under 2000 lines.
- `EditorUiState`: every piece of session memory that must survive `recreate()`, including the saved-state baseline (D3).

### Java `coordinators/`
- `ProjectLifecycleCoordinator`: New/Open/Save/Save-copy/Recover/Close, the dirty guard by fingerprint, SAF transfer, import/export entry, autosave wiring. Owns `pendingLeave`; reads the baseline from `EditorUiState`.
- `SketchCadChromeCoordinator`: everything between `SketchSession` reads and the sketch/CAD chrome (tool rail state in a sketch, navigator, dimension label, extrude canvas, feature list, precision surface bodies, status line for CAD), the `nativeSketch`/`nativeExtrude` scratch arrays, `lastReported*` memories.
- `ChromeRefreshScheduler`: `syncFromNative` with a reason enum, the anchored re-post bounded retry, the per-gesture cheap refresh. The 22 sites call `requestResync(reason)`.
- Allowed: `NativeViewport`, child views' `refreshFromNative`, `EditorUiState`. Forbidden: layout, insets, `View` measurement.
- Tests: the existing device classes (they locate by id), plus JVM tests where the coordinator is pure (dirty-guard decision table, refresh reasons).

### Java `chrome/`
- `AnchoredValueEditor`: the shared reading `TextView` + swap-in `EditText` + Apply + IME + parse-and-report + the accessibility label, used by the extrude HUD, the body dimension labels and the sketch line dimension.
- `CadExtrudeCanvasView` shrinks to: per-control anchors, the rotated value, transparent hit proxies, palettes.
- `CadHudPresentation` keeps the two numbers apart by name: `glyphDp(scale)` and `hitDp()`.

## 3. Reducing the four burdens named in the brief

| Burden | Today | Target |
| --- | --- | --- |
| `EditorWorkspaceView` coordination | 226 methods, 20 listeners, 97 natives, 22 resync sites | three coordinators + a scheduler; the view keeps composition and layout; listeners are implemented by the coordinator that owns the family |
| JNI monolith | 8100 lines, 13 families, 715-line `touchEvent`, 23 self-test includes | nine TUs by family, one arbiter, one debug TU; each export is lock + call + code |
| Android-to-native fan-out | one class calls 12 families | `EditorWorkspaceView` calls lifecycle and layout only; each coordinator calls its family; child views stay single-family (they already are) |
| Renderer / control coupling | already loose (snapshot + line list); the coupling is HUD ↔ overlay through two `mpp` reads | one `mpp` at the anchor, produced by the session and consumed by both; the extrude annotation is one more `Dimension`-range producer |

## 4. Dependency rules to enforce mechanically (no new tooling required)

A grep-based check in `scripts/` (the repository already has
`verify-device-guards.ps1` as the pattern) could assert:

1. Only `feature/kernel` includes a Manifold header (exists as a rule; make it a check).
2. Only `platform/android/jni/_render_thread` and `render/renderer.cpp` include `forgeshape_renderer.h`.
3. No `feature/*` or `domain/*` header includes `view/camera.h` except `feature/extrude`'s manipulator and `sculpt` (both by documented exception).
4. No `persistence/` file includes a session header.
5. No `debug*` export outside `_debug`.
