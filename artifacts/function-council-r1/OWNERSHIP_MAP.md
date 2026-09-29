# End-to-end ownership map (FUNCTION-COUNCIL-R1)

For each feature family: the real path from the control to the pixels, and
what records or persists it. File references are to `app/src/main/`. JNI names
are `NativeViewport.<name>` in Java and `Java_com_forgeshape_app_NativeViewport_<name>`
in `cpp/forgeshape_jni.cpp`.

## 1. The shared spine

Every family below runs through the same five pieces, and each has ONE owner:

| Piece | Owner | Notes |
| --- | --- | --- |
| Input | `ForgeShapeSurfaceView` → `NativeViewport.touchEvent` → `forgeshape_input.h` samples → arbitration in `forgeshape_jni.cpp` (gizmo, sketch, support chooser, sculpt pending-then-promote, orbit, tap) | one arbiter, native, under `g_stateMutex`; hover goes to `supportChooserHover` only |
| The scene | `ConstructionScene` via `constructionScene()` (`forgeshape_scene.cpp:348`, function-static) | bodies, order, active body, id allocator; `hasProject()` answers "is a project open" |
| History | `ConstructionHistory` (`forgeshape_history.*`) and, per body, `SculptHistory` in `FrozenSculpt` | two histories, never merged; `historyUndo/Redo` pick by product mode |
| Persistence | `forgeshape_project_document.*` (codec) + `forgeshape_project_state.cpp` (capture, fingerprint, all-or-nothing load) | Android adapters: `ProjectSlot`, `ProjectCheckpoint`, `AutosaveController`, `ProjectTransfer` |
| Drawing | render thread in `forgeshape_jni.cpp` (`renderThreadMain`, `:1719-1983`) → `viewSceneSnapshot()` → `Renderer::drawFrame` (`forgeshape_renderer.cpp:3751`) | derived only; per-body GPU buffers keyed by the published mesh revision |

The chrome's rule is also single: `EditorWorkspaceView.syncFromNative()`
(`:2478`, 22 call sites) and `refreshShellPhase()` (`:1829`) re-read native on
every refresh; `refreshWorldAnchoredUi()` (`:3354`) is the cheaper per-gesture
path for anchored labels and the CAD HUD.

## 2. Family paths

### Project / shell

| Step | Path |
| --- | --- |
| Control | Home page (`StartPageView`), Project menu (`project_actions_button`, `ProjectActionsPopoverView`), Settings page |
| Java owner | `EditorWorkspaceView` (`onNewProjectChosen`, `saveProjectToSlot` `:4338`, `openSavedProjectSlot` `:4361`, `offerRecoveryIfPresent` `:4459`), `AutosaveController`, `ProjectTransfer` |
| JNI | `encodeProject`, `loadProject`, `validateProject`, `projectFingerprint`, `projectOpen`, `closeProject`, `beginSessionInitialization` / `endSessionInitialization`, `sketchBegin` + `sketchCommit` (CAD bootstrap) |
| C++ owner | `captureProjectDocument`, `loadProjectDocument` (`forgeshape_project_state.cpp:200-370`), `commitFirstCadProject` (`forgeshape_project_bootstrap.cpp`) |
| History | load, bootstrap and close leave an EMPTY history |
| Persistence | the one `.forge` codec; the checkpoint is the same document |
| Renderer | bodies re-derived from the loaded document |
| Tests | `HomeFlowTest`, `ProjectProcessDeathTest`, `ProjectAutosaveRecoveryTest`, `ProjectTransferTest`, `EditorWorkspaceProjectActionsTest`; `PROJECT` suite |

**Flags.** (a) Five different routes establish or end a project and each
resets a different hand-written list of process-scoped sessions (see §4).
(b) The saved-state memory `persistedFingerprint` / `everPersisted` is a field
of the view, not of `EditorUiState`, so it does not survive `recreate()`.
(c) The recovery candidate is validated on the UI thread.

### Objects / Construction

| Step | Path |
| --- | --- |
| Control | Objects capsule and row (`ObjectsSectionView`, row overflow → row command strip → mirror plane chooser), Add Primitive, the gizmo, the precision surface (`ConstructionShapeEditorView`, `ConstructionPlacementEditorView`), Dimensions |
| Java owner | `EditorWorkspaceView` callbacks; the editors hold typed text only |
| JNI | `sceneAddBody`, `sceneSelectBody`, `sceneDeleteBody`, `sceneRenameBody`, `sceneSetBodyVisible`, `sceneSetBodyLocked`, `sceneDuplicateBody`, `sceneMirrorBody`, `applyConstruction*`, `applyBoxTransform`, gizmo through `touchEvent` + `setGizmo*`, `applyBodyDimension`, `applyBodyRelativeScale` |
| C++ owner | `ConstructionScene`, `ConstructionObject`, `forgeshape_body_commands.*`, `forgeshape_body_delete.*`, `forgeshape_body_mirror.*`, `GizmoSession`, `forgeshape_body_dimensions.*` |
| History | one `ScopedConstructionEdit` per act; a no-op records nothing |
| Persistence | `SCNE` (v2 only for name / hidden / locked), `CONS` |
| Renderer | shape edits publish a new mesh revision; placement, name, hide and lock publish none; hidden is removed in `snapshot()` alone |
| Tests | `EditorWorkspaceGizmoTest`, `EditorWorkspaceHistoryTest`, `ObjectsDeleteTest`, `ObjectCommandsTest`, `MirrorSmokeTest`, `BodyDimensionsSmokeTest`; `GIZMO`, `SCENE`, `CONSTRUCTION_*`, `MIRROR`, `BODY_DIMENSIONS` suites |

**Flags.** Typed shape and placement Apply are not refused in Sculpt below JNI
(Sculpt seat; `applyPrimitive` even republishes the sculpt mesh in Sculpt).

### CAD: region → feature → kernel → persistence → renderer

| Step | Path |
| --- | --- |
| Control | New Sketch → support chooser (spatial, or `sketch_plane_by_name`); Tool Rail sketch tools; Finish Sketch; region tap; canvas extrude cluster (extent selector, value, operation badge, Flip); Extrude; feature rows in the Shape panel; Edit Sketch |
| Java owner | `SketchEditorView`, `CadExtrudeCanvasView` + `CadHudPresentation` (glyph and hit-area arithmetic), `CadFeatureEditorView`, `SketchChromePolicy`; none holds a draft value — each refresh reads `sketchState` / `cadExtrudeToolState` |
| JNI | `supportChooserBegin/Hover/Cancel`, `sketchBegin`, `sketchSetTool`, `touchEvent` (drawing, region tap, arrow drag), `sketchFinish`, `sketchSetExtrude`, `sketchSetExtrudeExtent`, `sketchSetExtrudeSide`, `sketchSetOperation`, `sketchFlipExtrudeDirection`, `sketchCommit`, `sketchBeginEdit`, `sketchBeginEditFeature`, `sketchCommitEdit`; the older size route `cadApplyRectangle/Circle/Extrude` |
| C++ owner | `SketchSession` (volatile, process-scoped, `forgeshape_sketch_session.cpp:2076`) owns the draft; `extractSketchRegions` → `ProfileRegionRef`; `CadBody::applyState` validates and regenerates the WHOLE chain; `regenerateCadBody` → `buildCadChainGeometry` → ONE `forgeshape_cad_kernel.cpp` boolean per later feature; `CadFeatureSupport` names an earlier feature's face; `TopoRef` names another body's face |
| Preview | `SketchSession::evaluateCandidate()` (`:1581`), latest-only by `candidateRevision_`; drawn by `applyCadOperationPreviewLocked` (`forgeshape_jni.cpp:1656-1717`) in place of the target's draw item |
| History | one `ScopedConstructionEdit` around `addCadBody` (New Body) or `applyState` (Add, Cut, feature edit, Edit Sketch); the step holds the whole `CadBodyState` both sides |
| Persistence | `CADB` v1..v5, the lowest version that carries the data; per-triangle face tags are derived, never stored |
| Renderer | the derived body mesh; the preview mesh is a derived copy in `CadPreviewCache` (`forgeshape_jni.cpp:1642-1652`) |
| Tests | `CadVerticalSliceTest`, `SketchUxTest`, `SpatialSketchTest`, `CadCanvasExtrudeTest`, `CadExtrudeExtentTest`, `SketchExtrudeTest`; `CAD`, `CAD_A3`, `CAD_FEATURE`, `SKETCH_UX`, `PROJECT` suites; `CadHudPresentationTest`, `SketchChromePolicyTest` (JVM) |

**Flags.**

1. **Two mechanisms for "a sketch on a face".** A face of ANOTHER body gives a
   `TopoRef` and a separate, derived-placement body (`CAD-A3`); a face of the
   SAME body's earlier feature gives a `CadFeatureSupport` inside one chain.
   Both are deliberate and both fail closed, but they are two dependency
   models with two lineage checks.
2. **"The preview IS the candidate" holds by determinism, not identity.** The
   commit consults the cached evaluation and then `applyState` regenerates
   again; New Body's `commit` does not consult it at all (CAD seat,
   `forgeshape_sketch_session.cpp:1632-1729`). Correct while regeneration is
   bit-deterministic, which `CADVS_OPS_21` asserts.
3. **The first evaluation after a drag sample runs on the UI thread.**
   `CadExtrudeCanvasView.refreshFromNative` → `cadExtrudeToolState` →
   `evaluateCandidate()` (`forgeshape_jni.cpp:4840`) under `g_stateMutex`,
   contrary to the "render thread evaluates at most once per frame" statement
   in `PERF_NOTES.md` §3 and the comment at `forgeshape_jni.cpp:1632-1635`.
4. **Two editing routes for a base rectangle or circle** (the Shape panel's
   `cadApply*` and Edit Sketch). One truth, two doors.
5. **Navigator view state never reaches the camera** (`beginSketchView` reads
   `frame()`, `forgeshape_jni.cpp:311`).

### Imported Mesh

| Step | Path |
| --- | --- |
| Control | Project menu → Import GLB… (`import_glb`) |
| Java owner | `EditorWorkspaceView.onImportGlbRequested` → SAF picker → `applyImportedGlbBytes` |
| JNI | `importGlbDurable` (parse outside the lock, commit inside) |
| C++ owner | `importGlb` (parser, geometry only) → `commitImportedGlbScene` (`forgeshape_import_commit.cpp`) → `ImportedMesh` (immutable) on a new `SceneObject` |
| History | one step for the whole import; bodies held by `holdDetachedBody` across Undo |
| Persistence | `IMPT` v1: the arrays ARE truth |
| Renderer | `buildDrawData` (reversed duplicates for double-sided) |
| Tests | `ImportedMeshDurableTest`, `ImportedMeshSculptTest`; `GLTF_IMPORT`, `PROJECT` suites |

**Flag.** No Sculpt-mode or sketch refusal on this path (`onImportGlbRequested`,
`importGlbDurable`, `commitImportedGlbScene`), and the commit makes the first
imported body active (`forgeshape_import_commit.cpp:174`).

### Sculpt

| Step | Path |
| --- | --- |
| Control | Start Sculpting / Resume Sculpt / Back to …; the seven brush entries on the Tool Rail; Radius / Strength edge sliders (`BrushEdgeControlsView`); Clear Mask and Isolate in the Sculpt inspector (`SculptContextView`); the History navigator in the history capsule |
| Java owner | `EditorWorkspaceView` mode transitions; `SculptHistoryNavigatorView` reads `sculptHistoryState` in one call |
| JNI | `freezeToSculpt`, `enterSculptMode`, `enterConstructionMode`, `setSculptTool`, `setSculptBrush`, strokes via `touchEvent`, `sculptClearMask`, `sculptCanClearMask`, `setSculptIsolate`, `sculptJumpToHistoryCursor`, `historyUndo/Redo` |
| C++ owner | `SculptSession` (process-scoped; rebinds to the active body's `FrozenSculpt` on every access, `forgeshape_scene.cpp:532-540`), `SculptStroke` (one closed enum, one switch), `SculptHistory` per body |
| History | `SculptHistory` only: one entry per completed stroke or Clear Mask; 32 entries / 4 MiB / 1 MiB per entry |
| Persistence | `SCUL`: positions and topology only — no mask, no history, no isolate |
| Renderer | the whole sculpt mesh republished per move; mask is a fourth vertex attribute; isolate is a `SceneViewRestriction` inside `viewSceneSnapshot()` |
| Tests | `SculptUndoTest`, `SculptBrushStage025Test`, `SculptHistoryNavigatorTest`, `Stage027SculptWorkflowTest`, `ImportedMeshSculptTest`, `EditorWorkspaceSculptRetentionTest`; `SCULPT_BRUSH_KERNEL`, `SCENE` suites |

**Flags.** (a) An over-cap stroke is not recorded but leaves earlier absolute
entries in place. (b) Clear Mask can be drawn and refused. (c) The load path
can reopen a hidden sculpted body into Sculpt (recorded debt).

### Renderer and selection

| Step | Path |
| --- | --- |
| Control | Display popover (shading, surface, projection, grid, Selection Outline); Settings (palette, gizmo size and weight) |
| Java owner | `DisplaySettingsPopoverView` → native store; `AppPreferences` for preferences |
| JNI | `setShadingModel`, `setSurfaceShading`, `setProjectionMode`, `setGridVisible`, `setSelectionOutlineVisible`, `setViewportBackground`, `setGizmoVisualScale`, `setGizmoStrokeWeight` |
| C++ owner | `DisplaySettingsStore` (session), `Renderer` (derived); `forgeshape_render_recovery.h` policy |
| History / persistence | none — presentation only |
| Tests | `SelectionOutlineTest`, `RendererCounterContinuityTest`, `DiagnosticsAndRendererLossTest`, `EditorWorkspaceDisplayTest`; `RENDER_SHADING`, `RENDER_RECOVERY` suites |

## 3. Truth, runtime and derived — classified

| Kind | What | Owner and evidence |
| --- | --- | --- |
| **Durable truth** (serialized, fingerprinted) | ids and the allocator high-water, scene order, active body, representation, the six parameter sets, placement, name / hidden / locked, Imported arrays and submeshes, the CAD chain (sketches, extents, regions, operations, supports, `TopoRef`, lineage), Frozen Sculpt positions and topology, `hasEdits`, `sourceStale` | each has ONE owner (`SceneObject`, `ConstructionObject`, `ImportedMesh`, `CadBody::state_`, `FrozenSculpt`); captured in `captureProjectDocument`, hashed in `projectSemanticFingerprint` (`forgeshape_project_state.cpp:541-635`) |
| **Runtime-local** (never serialized, never a history step) | `SculptHistory`, the mask, Isolate, the sketch session and its view flip / turns, the extrude tool, Relative Scale, the support chooser, the gizmo session, display settings, the selection-outline toggle, camera, selection, `g_lastCadApplyStatus` | process-scoped function-statics and JNI globals; no hit in the codec, the capture or the fingerprint (domain seat, §3) |
| **Application state** | `AppPreferences` | Java only, `SharedPreferences`; byte-identical project proven by `SettingsPreferencesTest` |
| **Derived** (rebuilt, never read back) | construction meshes, normals, adjacency, regions, polygons, triangles, boolean meshes and face tags, face frames, a dependent's world placement, render meshes, GPU buffers, the CAD preview mesh | `publishSceneObject`, `regenerateCadBody`, `resolveWorldModel` (per snapshot), `RenderMeshCache` |

One nuance: the fingerprint hashes a Frozen Sculpt Mesh by its revision and
freeze count, a deliberate proxy (`forgeshape_project_state.cpp:621-629`),
while `CLAUDE.md` says it hashes "semantic VALUES, never the domain's update
counters". Consequence: after a Save, a stroke and its Undo leave identical
geometry but a moved fingerprint, so the project reads unsaved; Construction
does not behave that way.

## 4. Routes that establish or end a project

| Route | Scene | History | Sculpt session | Sketch, chooser, gizmo, selection | Step |
| --- | --- | --- | --- | --- | --- |
| `loadProjectDocument` (Open, Recover, Open File) | replaced whole | `clear()` | stroke closed, rebound, mode by document | sketch cancelled by the JNI caller; chooser and gizmo NOT touched here | none |
| `commitFirstCadProject` (CAD bootstrap) | through the load path | via load | via load | sketch cancelled; the JNI caller adds chooser cancel + `endSketchView` | none |
| Sculpt bootstrap (seeded sphere) | ordinary mutations on the empty scene | `begin/endSessionInitialization` records nothing | `freezeToSculpt` | refusal path closes the project | none |
| `closeProject` | emptied | `clear()` — without checking an open edit, unlike load | stroke dropped, Construction | chooser, sketch, view, gizmo, gesture all reset | none |
| `debugResetConstructionHistory` (tests) | untouched | `clear()` | untouched | untouched | none |
| `WorkspaceTestSupport.resetToBaselineConstruction` (tests) | closes only a project with NO Construction body; otherwise re-applies a box to the first Construction body | via the debug seam | cannot un-freeze | cancels sketch and chooser | none |

Each route enumerates the process-scoped sessions it resets by hand, so a new
session object has to be added to each list separately. The two bootstraps
reach "a new project with an empty history" by two different mechanisms.

## 5. Test-only seams at the production boundary

| Seam | Compiled in release? | Used by |
| --- | --- | --- |
| Self-test runners, `debugMeshCommand`, `debugInjectDeviceLoss`, `debugSetCameraPose`, `debugCameraPose`, `debugResetConstructionHistory`, `debugLastPointerEvent` | exported, but the body is a no-op in release (`#ifndef NDEBUG`) | androidTest; `debugMeshCommand` also from `ForgeShapeActivity.onKeyDown` hardware keys (a no-op in release) |
| `debugRendererFramesPresented`, `debugRendererDeviceRebuilds`, `debugProjectWorld`, `debugViewSceneBodyIds`, `debugViewportSelection`, `debugActiveBodyMisuseCount`, `debugPreviewRendersBothSides` | **yes** — exported, unused by production | androidTest |
| Imported Mesh Preview (`importGlbPreview`, `glbPreview*`, `clearGlbPreview`, `setGlbPreviewVisible`), `nomadLikeGlbFixture`, `glbRoundtripReport`, `glbCompareReport` | **yes** | androidTest only; the render loop checks preview visibility every frame |
| The render-thread counter mirrors (`g_rendererFramesPresented`, `g_outline*`) | yes | the evidence waits; made process-monotonic in C2 |
| Six exports nothing calls: `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`, `supportChooserSelect`, `supportChooserConfirm`, `sketchToggleRegion` | yes | nobody (0 callers in `app/src/main` and `app/src/androidTest`) |

`NativeViewport.java` declares exactly 200 `static native` methods, matching
the 200 JNI exports.
