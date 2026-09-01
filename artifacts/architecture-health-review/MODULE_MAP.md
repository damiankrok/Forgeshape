# Module map — ForgeShape architecture health review (2026-09-01)

Baseline `4c296f5415aa6955c2cc77e84d063c9ae5a59d47`. Read from the tree, not
from the docs; line counts are `wc -l` at the baseline.

## Layers and the direction of dependency

```
Android UI (Java views)            EditorWorkspaceView 3427, GlobalToolbarView 839,
                                   ToolRailView, ObjectsCapsuleView/ObjectsSectionView,
                                   PropertyInspectorView, ConstructionShapeEditorView,
                                   ConstructionPlacementEditorView, WorkspaceTrailingHostView,
                                   AnchoredSurfaceView, StartChooserView, RecoveryPromptView
        |
        v
Java workspace / state             EditorUiState 324, WorkspaceLayoutMode 210,
                                   ForgeShapeActivity 503, AutosaveController 288,
                                   ProjectCheckpoint 198, ProjectSlot, ProjectTransfer 252,
                                   Diagnostics 299 / DiagnosticLog 211
        |
        v
JNI boundary                       NativeViewport.java 1390 (91 `static native`)
                                   forgeshape_jni.cpp 4027 (92 JNIEXPORT; g_stateMutex L136)
        |
        v
Domain (platform-neutral C++17)    forgeshape_scene.{h,cpp}        SceneObject, ConstructionScene,
                                                                   BodyRepresentation, active-body accessors
                                   forgeshape_construction.{h,cpp} ConstructionObject, PrimitiveSpec,
                                                                   six generators, applyConstructionPrimitive
                                   forgeshape_transform.{h,cpp}    ConstructionTransform, Euler<->matrix bridge
                                   forgeshape_history.{h,cpp}      ConstructionHistory, ScopedConstructionEdit
                                   forgeshape_sculpt.{h,cpp}       FrozenSculpt, SculptSession, SculptStroke
                                   forgeshape_mesh.{h,cpp}         MeshStore, RuntimeMesh, MeshRevision
                                   forgeshape_imported_mesh.{h,cpp} ImportedMesh (+ buildDrawData)
                                   forgeshape_picking, forgeshape_selection, forgeshape_camera,
                                   forgeshape_gizmo, forgeshape_input.h, forgeshape_math.h
        |
        +---> Project / codec       forgeshape_project_document.{h,cpp} (.forge v1 layout + validation)
        |                           forgeshape_project_bytes.h (LE fixed-width reader/writer)
        |                           forgeshape_project_state.cpp (capture / load / fingerprint)
        +---> Import                forgeshape_gltf_import.{h,cpp} (one reader, fails closed by name)
        |                           forgeshape_import_commit.cpp (staged, atomic, one transaction)
        |                           forgeshape_import_preview.{h,cpp} (diagnostic, keys >= 2^60)
        |                           forgeshape_glb_roundtrip.cpp (diagnostic compare)
        +---> Export                forgeshape_gltf_export.{h,cpp} (bakes L, T on node)
        |                           forgeshape_json.cpp
        +---> Renderer              forgeshape_renderer.{h,cpp} 2714 (Vulkan; bodies_ map L351)
                                    forgeshape_render_mesh.{h,cpp}, forgeshape_display.{h,cpp},
                                    forgeshape_matcap.cpp, forgeshape_render_recovery.h
```

No arrow points upward. Verified by grep: no `jobject`, `JNIEnv`, `ANativeWindow`
or Android include outside `forgeshape_jni.cpp` and the renderer's one surface
seam; no `View`/`MotionEvent` reaches JNI (input crosses as `forgeshape_input.h`
samples). The domain never includes the renderer.

## Ownership (one owner per fact)

| Fact | Owner | Readers |
| --- | --- | --- |
| Body identity, order, active body, id allocator | `ConstructionScene` | history, project_state, jni, renderer (via snapshot) |
| Representation (Construction vs Imported) | `SceneObject` (`BodyRepresentation`) | 6 dispatch sites (see HOTSPOTS §D2) |
| Construction parameters, six remembered sets, kind | `ConstructionObject` | generators, history, project_state, export |
| Placement (`ConstructionTransform`) | `SceneObject` (body-level since IMPORT-01A) | gizmo, picking, renderer, export, project_state |
| Published geometry per body | `MeshStore` (own mutex) | renderer, picking |
| Frozen Sculpt Mesh + stale flag | `FrozenSculpt` on the body | `SculptSession` (borrows), project_state |
| Imported geometry | `ImportedMesh` on the body | `publishSceneObject`, export, project_state |
| Undo/redo, transaction boundary | `ConstructionHistory` | jni only |
| `.forge` layout and validation | `forgeshape_project_document` | project_state, Java slots via JNI bytes |
| Autosave timing / checkpoint file | `AutosaveController` + `ProjectCheckpoint` (Java) | — |
| GPU resources (derived) | `Renderer::bodies_` | — |

## Threads

- UI thread: every scene/history mutation, all JNI entry points except the
  two below.
- Render thread: `Renderer` only; takes a `SceneSnapshot` under `g_stateMutex`
  once per frame, then owns no domain reference.
- `forgeshape-autosave` HandlerThread: calls `projectFingerprint()` and
  `encodeProject()`, both of which take `g_stateMutex` and read the scene.
- Lock order, verified at every site: `g_stateMutex` → `MeshStore` mutex. The
  domain never takes `g_stateMutex` (no include of the JNI file, no global).

## Tests

- 17 native self-test suites (debug-only, once at `NativeViewport.start()`).
- JVM: 9 classes under `app/src/test` (70 tests).
- Instrumented: 32 classes under `app/src/androidTest`; `WorkspaceTestSupport`
  is the one shared helper; controls are located by `R.id` only.
