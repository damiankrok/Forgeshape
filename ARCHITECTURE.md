# ForgeShape — Architecture

Current production architecture only. No roadmap, no history.

ForgeShape is a standalone Android application that owns its own viewport and
renderer. No game engine (Godot/GDExtension, Unity, Unreal) and no third-party
runtime, rendering, math or input library participates in the running product.

## Layer map

What each box owns is in the Ownership table below; what the diagram adds is the
**direction of every arrow**, which the table cannot show.

```
ForgeShapeActivity
        |
        +-- EditorWorkspaceView   (+ EditorUiState, WorkspaceLayoutMode)
        |        +-- HomeView / NewProjectChooserView
        |        |        (two StartPageViews: full-window, opaque, added to the
        |        |         workspace root rather than the inset overlay, because
        |        |         a page is the window's own ground)
        |        +-- UnsavedChangesPromptView / RecoveryPromptView
        |        |        (two ChooserSurfaceViews: questions asked OVER a live
        |        |         project, where a scrim modal is the right shape)
        |        +-- GlobalToolbarView
        |        +-- SketchOrientationNavigatorView  plane / normal / +-90 view
        |        +-- SketchDimensionLabelView        a selected Line's length,
        |        |                                   editable in place
        |        +-- WorkspaceTrailingHostView
        |        |        +-- BoundedScrollView -> vertical context column
        |        |                 +-- ToolRailView / transform + space / precision
        |        +-- BrushEdgeControlsView / ObjectsSectionView / ObjectsCapsuleView
        |        +-- AnchoredSurfaceView   one growth, five surfaces
        |                 +-- ObjectsPopoverView / AddPrimitivePaletteView
        |                 +-- DisplaySettingsPopoverView / ProjectActionsPopoverView
        |                 +-- PropertyInspectorView  (+ PrecisionScrollView)
        |                          +-- ConstructionShapeEditorView    what the object IS
        |                          +-- CadFeatureEditorView   what a CAD Body IS: sketch sizes + depth
        |                          +-- SketchEditorView       the sketch in progress: entity values,
        |                          |                          profile choice, depth, Extrude
        |                          +-- ConstructionPlacementEditorView  where it SITS
        |                          +-- SculptContextView  mesh state + guarded re-Freeze
        |
        +-- ProjectSlot           one app-private .forge file; bytes only,
        |                         no meaning. Save/Open cross JNI as byte[]
        |
ForgeShapeSurfaceView         Surface lifecycle + raw pointer forwarding
NativeViewport (JNI decls)
        |  JNI  -- Android/JNI types stop HERE --
forgeshape_jni.cpp            render thread, ANativeWindow, MotionEvent ->
        |                     TouchAction, camera/selection locking,
        |                     stroke-vs-navigation arbitration
        |
        +--> CameraController -> CameraSnapshot
        |
        +--> SelectionController -- on a valid tap --> pickScene() -> Picking
        |         |                                        ^
        |         v                                        | current snapshot
        |    ObjectId or none -> bool "draw highlight"      |
        |                                                   |
        +--> ProjectDocument  <-- forgeshape_project_bytes (LE + CRC-32)
        |         ^   |            forgeshape_project_document (v1 codec)
        |  capture|   |commit      forgeshape_project_state (the bridge)
        |         |   v            all-or-nothing; meshes are REGENERATED
        |    ConstructionScene
        |
        |    ConstructionObject   CadBody          SculptSession
        |        |                   |                 |
        |        |          SketchSession --commit--> CadBodyState (sketch + extrude)
        |        |          (volatile; overlay ->        |
        |        |           Renderer, never truth)  generateCadMesh() [LOCAL]
        |        |                   |                 |
        |        generateMesh() [LOCAL] --Freeze--> SculptMesh [LOCAL copy,
        |        |                                  own SculptRevision]
        |        +---- the ACTIVE one is published -+
        |        v                                  |
        |    MeshStore (immutable revisions, monotonic, latest-wins)
        |        |                                  |
        |    modelMatrix() / inverseModelMatrix()   |
        v        v                                  v
      Renderer  (Vulkan, frame loop, upload; owns no geometry truth)
        |            ^-- forgeshape_grid: world reference floor, NOT in the scene
        |            ^-- forgeshape_selection_outline: band width and colour
        |                policy. The mask pass rasterises the bodies' OWN
        |                buffers, so the outline needs no geometry of its own
        |
   ANativeWindow -> VkSurfaceKHR -> swapchain
```

## Ownership

| Concern | Owner | Explicitly NOT an owner |
| --- | --- | --- |
| Activity lifecycle, edge-to-edge window | `ForgeShapeActivity` | — |
| Which surfaces are on screen, adaptive layout, window insets, chrome visibility | `EditorWorkspaceView` | it decides no mode — `syncFromNative()` *reads* `NativeViewport.productMode()` and builds from that |
| Right-context composition, vertical child order, internal scrolling, Display suppression and fixed top/right/width with downward-only height | `WorkspaceTrailingHostView` | it owns no product, transform, tool or precision-open truth; callbacks report intent and `PresentationState` is a derived snapshot |
| Display unit, the *draft* primitive kind, the Construction rail selection, whether the precision surface was asked for, chrome-hidden | `EditorUiState` | every field is safe to lose; none of them can change the model |
| The window-dp breakpoints and chrome sizing rules | `WorkspaceLayoutMode` | it holds no Android type and reads no state; it is arithmetic |
| Shape/transform field text and input validation messages | `ConstructionShapeEditorView`, `ConstructionPlacementEditorView` | neither owns a parameter, a kind, a transform, a mesh or a publish decision |
| Brush slider positions, which rail entry looks active, the sculpt mesh summary | `BrushEdgeControlsView`, `ToolRailView`, `SculptContextView` | none owns a vertex, a brush value, a tool or a mode; all four are read back from native state |
| The active product mode, the one Frozen Sculpt Mesh, the active tool, the brush and the live stroke | `SculptSession` (`forgeshape_sculpt.{h,cpp}`) | Java owns none of these; the renderer owns no sculpt truth |
| Frozen local vertex/index data and its `SculptRevision` | `SculptMesh` | it is NOT Construction truth and no parameter is ever read back out of it |
| 1-ring adjacency and incident triangles of the frozen mesh | `SculptTopology` | built once per Freeze, never per move; it is not a half-edge mesh and cannot change topology |
| Area-weighted vertex normals from current positions | `computeVertexNormals` + `SculptMesh`'s dirty-flagged cache | derived data, not truth; no brush computes its own |
| The stroke kernel: hit, affected set, falloff, radius resolution, lifecycle | `SculptStroke` | it restates no FOV or aspect — it reads the `CameraSnapshot`'s own matrices |
| What each tool does to the vertices it captured | one `SculptStroke::apply*` per `SculptTool` (seven of them since `SCULPT-FCM-R1`) | there is no brush base class, registry or plugin surface |
| The Sculpt Mask weight of every vertex, and the `1 - w` factor over the six geometry brushes | `SculptMesh`'s mask accessors + `sculptMaskFactor` | runtime-local, per body by ownership, never project truth, never a `.forge` byte |
| Whether a one-finger Down becomes a stroke or a navigation | `g_strokePending` in `forgeshape_jni.cpp`, using `SculptSession::hitsSculptMesh` | Java decides none of it; the probe cannot mutate the mesh |
| mm/cm/m ↔ meter conversion and number formatting | `LengthUnit` | the domain never sees a display unit |
| The ordered collection of Construction Bodies, ObjectId minting, and which body is active | `ConstructionScene` (`forgeshape_scene.{h,cpp}`) | a flat list, not a scene graph or a hierarchy; ids are never a collection index |
| Identity, which primitive is active, every primitive's parameters, and placement — for ONE body | `ConstructionObject` (`forgeshape_construction.{h,cpp}`) | it knows nothing of the collection holding it |
| Which of the five approved appearances the app wears, which edge the rail zone stands on, and how large and how heavily the gizmo is drawn | `AppPreferences` (one immutable value) + `AppPreferencesStore` (the SharedPreferences adapter) | app-level and persisted, never project truth; the domain sees only a viewport-background index, a bounded visual-scale multiplier and a stroke-weight index |
| The Settings page: the one surface every persistent preference is chosen on | `SettingsPageView` (a start page) | owns no state; reports a request, and is repainted from the store |
| Seating the rail zone on the chosen edge | `EditorWorkspaceView.applyHandedness` | mirrors row ORDER and margins only; no axis, workplane, camera, gizmo or gesture is mirrored |
| What the viewport is cleared to | `ViewportBackground` in `forgeshape_display.{h,cpp}` | native owns the colours; no Android theme or RGB crosses JNI |
| The world reference grid's plane, spacing, extent, tiers and palette | `forgeshape_grid.{h,cpp}` | it is not a `SceneObject`, has no `ObjectId` or revision, is not pickable, and is not a snap target |
| Whether the grid is drawn | `DisplaySettingsStore` | the renderer owns no presentation preference; the grid module owns no visibility |
| The selected body OUTLINE: its band width, its per-ground colour and the edge-extraction rule | `forgeshape_selection_outline.{h,cpp}` | policy only, and no Vulkan: it holds no `ObjectId`, no geometry and no visibility. Width and colour are policy, not preference, so no control for either exists |
| Whether the selection outline is drawn | `DisplaySettingsStore` | the grid's lifecycle exactly: session-only, process-scoped and native-owned, never an `AppPreferences` field |
| Whether Objects has a dedicated column for a given window | `WorkspaceLayoutMode.objectsDocked(widthDp)` | arithmetic on window dp; it reads neither theme, mode nor domain state. The trailing host has one fixed top/right placement in every window and therefore has no docked-state flag |
| Which surface currently hosts the Objects list | `EditorWorkspaceView` | there is exactly ONE `ObjectsSectionView`, re-parented; no second list and no Java-side selection truth |
| Each primitive's exact parameters and its deterministic **local**-space mesh | `ConstructionBox` (W/H/D), `ConstructionCylinder` and `ConstructionSphere` (diameter…), `ConstructionCone` (bottom diameter, height), `ConstructionCapsule` (diameter, **total** height) | no JNI/Android/Vulkan/renderer/UI types. Tessellation counts are fixed, not parameters. A radius, and the capsule's cylindrical middle, are derived and never stored. The cone's apex radius is zero by definition: no top diameter, no frustum. The mesh, the GPU and the picker own no parameter |
| The one radial-segment and latitude-stack count every round primitive is drawn with | `kPrimitiveRadialSegments` / `kPrimitiveLatitudeStacks` (`forgeshape_construction.h`) | no generator writes down a segment count of its own |
| The capsule's `totalHeight >= diameter` relation | `validateCapsuleMeters` | the UI restates none of it; it is a domain rule, not an input check |
| Which parameters belong to which primitive | `PrimitiveSpec`'s payload variant | no caller reads a primitive's numbers as another's; the kind is derived from the payload, not stored beside it |
| Authoritative primitive update **and** its mesh publication | `applyPrimitive` (`forgeshape_construction.{h,cpp}`) | JNI and Java restate none of this rule |
| The Construction transaction boundary and the whole Undo/Redo history | `ConstructionHistory` (`forgeshape_history.{h,cpp}`) | Java holds no history, no depth counter and no mirror scene; it holds no sculpt vertex, no `SculptRevision` and no mesh data of any kind |
| Which handle a touch landed on, the Move/Rotate/Scale solvers, the frozen World or Local drag basis, and the transaction around one drag | `GizmoSession` (`forgeshape_gizmo.{h,cpp}`) | it owns NO transform of its own — the authoritative `ConstructionTransform` moves throughout the drag; Java owns no pivot, no solver, no basis and no captured pointer |
| Whether there IS a gizmo at all (product mode, rail context, a body to act on) | `EditorWorkspaceView` | it decides nothing about where the handles are, how large they are or what a drag means; it pushes one boolean, a mode index, a space index and the display pixel scale, and reads back which of them the session actually took — including whether a space is offered at all |
| The gizmo canonical geometry, its axis/highlight/neutral colours and its reference-unit sizes | `forgeshape_gizmo.{h,cpp}` | it is not a `SceneObject`, has no `ObjectId` or revision, is never published, is not pickable by `pickScene`, and is not exported |
| The solved-target → applied-target placement and quantization seam | `applyGizmoPlacementModifier` plus `quantizeGizmoTranslation` / `...Rotation` / `...Scale` | all four are the identity today; neither Surface Snap nor Grid Snap has an approved contract, so there is no setting, no indicator and no hidden snapping |
| Authoritative placement (double-meter position, double-degree rotation, unitless positive scale) and the derived model / inverse / rotation / normal matrices | `ConstructionTransform` (`forgeshape_transform.{h,cpp}`) | no JNI/Android/Vulkan/renderer/UI types; it cannot publish a mesh because it cannot reach `MeshStore` |
| The axis and Euler convention | `forgeshape_transform.h` | the renderer and the picker define none of their own |
| World ray → local object ray | `transformRayToLocal` (`forgeshape_picking.{h,cpp}`) | no Vulkan state is consulted |
| Surface create/change/destroy | `ForgeShapeSurfaceView` | native code does not touch Android views |
| Raw pointer ids + coordinates | `ForgeShapeSurfaceView` | it interprets nothing |
| MotionEvent action decoding | `forgeshape_jni.cpp` | camera module never sees Android constants |
| Render thread, `ANativeWindow` | `forgeshape_jni.cpp` | renderer never creates/releases the window |
| Camera pose, gestures, projection | `CameraController` | renderer and Java own none of it |
| Platform-neutral pointer event data | `forgeshape_input.h` | one shared type, not an input framework. Carries tool type, pressure and tilt as well as id and position |
| Tap-vs-navigation decision, selected `ObjectId` | `SelectionController` | renderer and camera own no selection |
| Screen ray, ray/triangle, nearest hit | `forgeshape_picking.{h,cpp}` | no JNI/Android/Vulkan/renderer types |
| Current CPU mesh, revisions, validation | `MeshStore` / `RuntimeMesh` (`forgeshape_mesh.{h,cpp}`) | no JNI/Android/Vulkan types; owns no GPU resource |
| Baseline cube numbers, and the DEBUG mesh fixtures | `forgeshape_demo_mesh.{h,cpp}`, `forgeshape_mesh_fixtures.{h,cpp}` | test infrastructure, not product geometry, and not on any startup path |
| Vulkan, presentation, and every mesh buffer, staging and upload | `Renderer` | it owns no CPU mesh and mutates none, interprets no input and owns no identity |
| Vector/matrix math | `forgeshape_math.h` | no GLM or other third-party math |
| The `.forge` binary layout, its little-endian primitives, its CRC-32, its bounded section parsing, its compatibility rules and its deterministic writer | `forgeshape_project_bytes.{h,cpp}` + `forgeshape_project_document.{h,cpp}` | no Android, JNI, Vulkan, renderer, filesystem or `ConstructionScene` type; it knows what a project MEANS and nothing about where the bytes live. `DATA_PACKAGE_SPEC.md` owns the layout as documentation |
| Turning the running project into a document, and a validated document back into a running project in one all-or-nothing commit | `forgeshape_project_state.{h,cpp}` | it publishes no Construction mesh the codec could have carried — every one is regenerated; it restores no revision; a refused load has touched nothing |
| Where the MANUAL project file lives, and writing it durably | `ProjectSlot` (Java) | it owns no byte of meaning: one app-private slot, a temp-file + fsync + rename write, and a bounded read. It is written by an explicit Save and by nothing else — autosave has its own file |
| Where the RECOVERY checkpoint lives | `ProjectCheckpoint` (Java) | a separate app-private file, written atomically, quarantined rather than retried when it fails to decode. It is not the manual slot, and that separation is the point: autosave writing the user's named file would make a safety net into a destroyer of it |
| WHEN the project is checkpointed, and on which thread | `AutosaveController` (Java) | it owns no bytes and no format: it decides, coalesces, and calls the codec. The debounce is an implementation detail and is not product semantics |
| Whether the project actually changed | `projectSemanticFingerprint` (`forgeshape_project_state.{h,cpp}`) | it hashes semantic VALUES, never the domain's update counters — an undo does not advance those — and it is a change DETECTOR, never an identity |
| Turning a document `Uri` into bytes and back | `ProjectTransfer` (Java) | THE boundary: no `Uri`, `ContentResolver`, authority or path passes it, and none is ever project truth. It converts; it decides nothing |
| Reading a `.glb` | `forgeshape_gltf_import.{h,cpp}` + `forgeshape_json.{h,cpp}` | platform-neutral, and deliberately shares NO code with the writer: it re-derives every offset, length, stride and bound from the file. Supports a bounded STATIC subset — a node matrix or TRS, several TRIANGLES primitives, a generated NORMAL, ignored colour/UV, `doubleSided` — and fails closed by name on everything else. It bakes the node transform and produces GEOMETRY: it decides nothing about the project and creates no body, which is what lets one parse serve both the durable import and the diagnostic preview |
| What an Imported Mesh may be, and what a `.forge` file may carry for one | `forgeshape_imported_mesh.{h,cpp}` | ONE validator (`validateImportedMeshData`) and ONE name rule (`sanitizeImportedMeshName`), called by the importer and by the codec alike, so a file can never carry geometry or a name the importer would have refused. It resolves per-submesh `doubleSided` into draw geometry; it holds no material, no `SculptRevision` and no source path |
| Turning a parsed file into durable project objects | `forgeshape_import_commit.{h,cpp}` | it decides how many objects a file becomes, what they are called and where the node transform ends up. Atomic: everything is built and validated off the scene, and the whole commit is ONE `ScopedConstructionEdit`, so an import is one Undo and a refusal costs no `ObjectId` |
| Which LOCAL mesh a body is sculpted FROM | `buildSculptSourceMesh` in `forgeshape_scene.{h,cpp}` | the second ONE dispatch point beside `publishSceneObject`: a Construction Body regenerates from its parameters, an Imported Mesh hands over the arrays it owns. Either source is only READ. It copies an imported object's RAW indices, never `buildDrawData`'s reversed duplicates, and collapses per-submesh `doubleSided` into the frozen mesh's one sidedness answer — see *Sculpting an Imported Mesh* |
| The three principal workplanes and the ONE mapping between sketch `(u, v)` and body-local 3D | `forgeshape_workplane.{h,cpp}` | no camera, no pixel: a workplane is a fact about the body, right-handed by construction, and it reads nothing from the view |
| What a sketch entity may be, the sketch's per-sketch identities, closed-profile extraction and ear-clipping triangulation | `forgeshape_sketch.{h,cpp}` | truth is the entity list; profiles, polygons and triangles are DERIVED and never stored. It fails closed by name — open, forked, crossing, zero-area, duplicate-edge and nested loops are refused, never repaired — and every refusal is one `CadStatus` |
| A CAD Body's authored truth (`CadBodyState`: one sketch, one linear extrusion with its EXTENT) and the ONE regeneration path from it to a mesh | `forgeshape_cad_body.{h,cpp}` | the extent is TWO non-negative distances read through `extrudePositiveDistance` / `extrudeNegativeDistance`, and the mode names which the controls author rather than adding a second geometry rule; `extrudeFeatureWithExtent` is the transition policy as a pure function; each mode has ONE canonical form and a non-canonical one is refused by name. `applyState` validates and regenerates the whole requested state and writes nothing unless all of it passes; no vertex is truth; no parameter is ever read back out of a mesh |
| The sketch edit session: the entity being placed, the selection, snapping, the pointer it owns, the profile choice and the depth BEFORE the one commit | `SketchSession` (`forgeshape_sketch_session.{h,cpp}`) | volatile: nothing in it is project truth, the scene, the history, the fingerprint and the codec never see it, and `commit` is ONE `ScopedConstructionEdit` around ONE `addCadBody`. Java holds no sketch: not an entity, not a profile, not a depth |
| The sketch overlay the renderer draws: the plane grid, the axes, the entities, the drag and the extrude preview, as world-space lines | `SketchOverlay` (`forgeshape_sketch_overlay.h`), built by `SketchSession::overlay` | presentation on the gizmo's terms: no `ObjectId`, no revision, never published, never picked, never exported, never a `.forge` byte. The renderer re-uploads it only when its revision changes and draws it through the gizmo's line pipeline |
| How ONE overlay style is weighted — the grey a hue-less vertex takes and the alpha the whole range draws at | `sketchOverlayStyleWeights` (`forgeshape_sketch_overlay.{h,cpp}`) | a pure function over values (`UI-3D-STATE-C2`): no renderer, no device, no frame, so "is this style drawn at all" is a self-test rather than a screenshot. Extracted from the renderer's switch because a style with no case draws at alpha 0 — invisible, with nothing failing, which is what `UI3D-F-005` was. Its own switch has no `default:`, and it refuses a code outside the enum rather than inventing a weight for it. The renderer still owns everything a range does NOT vary: the matrix, the three axis hues, the highlight colour and the emphasis tag |
| Where the extrude manipulator stands in world space, how big it is drawn and grabbed, what a drag along the extrusion axis means, and which view that axis can be dragged in | `forgeshape_cad_extrude_tool.{h,cpp}` (`CadExtrudeAnchors`, `CadExtrudeControlScale`, `CadExtrudeManipulator`, `cadFeatureViewPose`) | **not** a second model of the extrusion: `SketchSession` still owns the profile, the depth and the direction and is still the only writer, and a dragged distance lands through `setExtrudeSide`, the one door a typed value already used. Since `CAD-EXT-R1` the anchors carry one `CadExtrudeSideAnchor` per side and the manipulator freezes WHICH side at pointer-down with the basis. The anchors carry no camera; the size rule is a new function BESIDE `gizmoWorldScale` (which holds a constant pixel size) and produces the ONE number drawing and hit testing share; the drag is the gizmo's contract verbatim; the view policy is a pure function over a pose, a frame and the anchors that answers another pose. Nothing here is serialized or reaches a history step |
| The views a sketch borrows — the exact support-normal one it is AUTHORED through, the feature-preview one the staged extrusion is adjusted through, and the user's own pose kept for the way back | `beginSketchView` / `beginExtrudeFeatureView` / `endSketchView` in `forgeshape_jni.cpp` over `CameraController::frameSketchView` / `capturePose` / `restorePose`, with `cadFeatureViewPose` deciding the second | the sketch stores no camera. The authoring view comes from the frame's own axes; the preview is installed on the `finish()` that reaches `Ready` and withdrawn by every path back to Editing; `g_sketchSavedPose` is the pre-sketch view and is restored on commit and on cancel alike, unconsumed by the preview |
| Whether a one-finger gesture while sketching draws, selects, is swallowed, or navigates — and that two fingers always pan and pinch | the sketch arbitration block in `forgeshape_jni.cpp`, using `SketchSession::onTouch` and `state()` | a single finger never orbits while the sketch is being DRAWN; in `Ready` one that misses the arrow navigates, because the drawing is done and there is no aligned view left to protect. The gizmo and the sculpt arbitration are already out of the picture, because the gizmo is withdrawn at begin and a sketch cannot start in Sculpt |
| Removing one body from the project | `forgeshape_body_delete.{h,cpp}` | one representation-neutral operation over the scene and the history. One Delete is one transaction; the removed body is HELD by the history rather than destroyed, so an Undo restores that object with its Imported Mesh and its Frozen Sculpt Mesh intact; the replacement selection and the last-body refusal are stated here and nowhere else |
| The object commands — Rename, Show/Hide, Lock/Unlock, Duplicate, Mirror | `forgeshape_body_commands.{h,cpp}` | one module over the scene and the history, on `forgeshape_body_delete`'s terms and for the same reason: each is a decision ABOUT the project that needs both collaborators and neither owns the other. Each act is one transaction; the name rule is the domain's existing one rather than a second policy, and one `derivedBodyName` serves Duplicate and Mirror alike; Duplicate is the only entry point that dispatches on representation, because it has to COPY one, and Mirror is the only one that REFUSES on representation |
| Reflecting a body across a principal world plane | `forgeshape_body_mirror.{h,cpp}` | the ONE mirror arithmetic (`MIRROR-01`), a pure function over values — no scene, no history, no body, no camera, no renderer — plus the eligibility predicate. The reflection is carried by a PROPER rotation `R' = F·R·Qx` with the positive scale untouched, so nothing about the transform contract or the `.forge` format moved; `mirrorSceneBody` beside the other object commands owns identity and the transaction |
| A body's overall DIMENSIONS, and every resize about an anchor | `forgeshape_body_dimensions.{h,cpp}` | the ONE resize/anchor solver (`UI-OWNER-33B`, Stage 020M), deliberately a pure function over values — no scene, no history, no body, no camera, no renderer — so Stage 020D's Directional Scale drives the same arithmetic rather than a second copy of it. It also owns the exact local bounds of the six Construction primitives, read from their PARAMETERS, and the two product acts over the scene and the history |
| The dimension leaders, and the Dimensions interaction state | `forgeshape_body_dimension_overlay.{h,cpp}` | presentation on the sketch overlay's terms: a world-space line list in the SketchOverlay's own structure and ranges, fed through the renderer's existing line path so no renderer change was needed. Also the session-only mode, active axis and anchor, which no Java flag mirrors |
| The deterministic external-GLB compatibility fixture | `forgeshape_glb_import_fixture.{h,cpp}` | a synthetic file with the structural feature set of an external low-poly export; every coordinate an integer over a power of two, so its bytes are the same everywhere. A debug test seam, reachable from no product path |
| What an imported preview IS, and every boundary it may not cross | `forgeshape_import_preview.{h,cpp}` | session-only: no scene `ObjectId`, no Construction Source, no sculpt representation, no `MeshStore`, no history, no `.forge`, no checkpoint, not selectable, not re-exportable, gone with the process. Its renderer keys are resource keys and never identities. Since `IMPORT-01A` it is reachable only from the verification suites |
| What a body is CALLED wherever the user reads it | `BodyLabels` (Java) | one answer for the Objects list, the Objects capsule, the precision surface's title and the status line: an Imported Mesh uses the name its file gave it, a Construction Body is `Body #id`, and the empty string native code returns for the latter is the SIGNAL for that fallback, not a name |
| Whether the file agrees with the scene | `forgeshape_glb_roundtrip.{h,cpp}` | compares the independently parsed file against DOMAIN truth re-derived from the generator and `modelMatrix()`, never against the exporter's captured arrays. Reads only, and reports a number rather than a boolean |
| The glTF 2.0 / GLB export: what the file contains and every byte of it | `forgeshape_gltf_export.{h,cpp}` | platform-neutral C++ with no Android, JNI, `Uri`, `ContentResolver`, path or Vulkan type. It READS the scene and writes bytes: it mints no revision, opens no transaction, allocates no `ObjectId` and moves no sculpt vertex. It re-evaluates a Construction Source, reads a Frozen Sculpt Mesh or reads an Imported Mesh's own arrays — never a GPU buffer and never a decoded `.forge` — and applies NO coordinate conversion — see *Export coordinates* |
| What a lost GPU device means and how many rebuilds are attempted | `RenderRecoveryPolicy` (`forgeshape_render_recovery.{h,cpp}`) | deliberately free of Vulkan so it can be self-tested without a GPU; it owns no handle, performs no teardown and touches no project state |
| Tearing the device down and building it again | `Renderer::rebuildDeviceAfterLoss` | it rebuilds the GPU COPY of derived data; the CPU project is not consulted and not touched, and `syncScene` re-uploads from the published revisions |
| The bounded local diagnostic ring, and its redaction | `DiagnosticLog` (Java, free of Android types) | it carries tokens, never geometry, `.forge` bytes, a path or a `Uri`; `Diagnostics` is the Android half that renders a report |
| Whether the project surface is open, and what a save or a refused open SAYS | `EditorWorkspaceView` + `ProjectActionsPopoverView` | neither owns the format, the storage or the fail-closed rule; both report an outcome native code decided |

## Platform boundary

Android is the **first production platform and the only one that exists**. There
is no Apple target, no Xcode project, no Metal backend, no MoltenVK and no
cross-platform UI framework, and none is authorized. What follows is a constraint
on how this codebase is arranged, not a claim about where it runs. There is no
Apple, Windows, web or cloud client, no account, no login and no sync — none of
those is implemented, and nothing here should be read as saying otherwise.

**The domain is platform-neutral C++ and the Android layer is an adapter over
it.**

| | rule |
| --- | --- |
| Domain code | Construction, geometry, sculpt, picking, camera and selection stay platform-neutral C++17. `forgeshape_camera`, `forgeshape_construction`, `forgeshape_transform`, `forgeshape_picking`, `forgeshape_selection`, `forgeshape_mesh` and `forgeshape_sculpt` contain no JNI, Android, Vulkan, renderer or UI type — that is asserted throughout the ownership table above and must stay asserted |
| Android types | `View`, `Activity`, `MotionEvent`, `Surface`, `jobject` and every other Android or JNI type may never become domain truth. They reach exactly as far as `forgeshape_jni.cpp` and stop |
| The Android UI | is a platform shell/adapter. It owns draft, presentation and layout state and nothing else, and reads authoritative state back from native code rather than assuming it |
| Input | crosses the boundary as **semantic, platform-neutral** data. `forgeshape_input.h`'s `TouchAction`/`TouchPointer` is that boundary, and it carries tool type, pressure and tilt alongside id and position -- in ForgeShape's own enum and its own units, never Android's. A pointer sample is translated out of Android's vocabulary in the Android layer rather than carried inward. Hover and generic (non-touch) motion are still outside the vocabulary and stay a consumer-driven question |
| Platform services | file, storage and system services have narrow boundaries of their own, for the same reason input has one: `ProjectSlot`, `ProjectCheckpoint` and `ProjectTransfer` (Java) turn a path or a `Uri` into bytes and bytes into a document, and none of them reaches JNI or the codec |
| Serialization | the `.forge` document (`forgeshape_project_document.{h,cpp}` + `forgeshape_project_bytes.{h,cpp}`) is **platform-independent and explicitly versioned**: a byte layout, an endianness and a version field decided by the domain, never a platform serializer, an Android `Parcelable`, a Java object stream or anything whose meaning depends on which OS wrote it. A file written on one platform is readable on another, and an older file is readable by a newer build or refused by version, never misread. `DATA_PACKAGE_SPEC.md` owns the layout |
| Renderer coupling | the renderer's dependency on a platform surface stays **explicit and local**: `forgeshape_jni.cpp` owns the `ANativeWindow` and hands it over, and `Renderer` never creates or releases one. That single visible seam is what a second backend would be added beside |

The practical test is one question: *if this file had to compile on a platform
that has no Android, what would break?* For everything below `forgeshape_jni.cpp`
the answer must stay "nothing". This is deliberately **not** an abstraction layer
— no renderer interface, no platform façade, no `#ifdef` for an operating system
that has no target. The seams stay where they are; nothing new crosses them.

## Android layer

**The Android UI is structured framework Views, and that is a decision rather
than an accident.** The product APK has no `dependencies { }` block,
`android.useAndroidX=false`, and there is no Kotlin source. Compose would require
AndroidX, the Kotlin Gradle plugin and stdlib, the Compose compiler plugin pinned
to the Kotlin version and the `activity-compose`/`ui`/`foundation`/`material3`/
`runtime` graph — dozens of artifacts in a project that ships none — and would
place Compose's pointer-input pipeline above the raw-`MotionEvent`-to-JNI path
and the sculpt gesture arbitration, which are the most carefully proven
behaviours in the product. The two things Compose is genuinely better at here,
`WindowInsets` and window size classes, are a few dozen lines of Views in one
class. Migrating is not authorized.

**System Back dismisses before it leaves.** With a context surface open, one Back
press used to return the launcher. The Activity owns the platform registration
and the workspace owns the decision: `EditorWorkspaceView.dismissTopmostSurface()`
closes the most recently opened surface through the workspace's own close path —
the same path the surface's own control takes, so the keyboard is released and
the invoking control un-lights — and returns whether it consumed the press. The
callback is registered **only while there is something to dismiss**, which leaves
the platform's own exit behaviour, predictive back included, untouched for the
press that really does leave. Every `AnchoredSurfaceView` reports its open state
to one listener, so that condition cannot go stale when a new call site forgets
to report. Two routes because the platform has two: `OnBackInvokedDispatcher` on
API 33+, where an app targeting SDK 36 no longer receives `onBackPressed` at all,
and the override below that. Dismissal goes through `setOpen(false)`, so the
surface plays the exit it already had — the motion was written; the most common
dismissal gesture simply never reached it — and reduced motion still lands
instantly, because that decision is inside `setOpen`.

`ForgeShapeSurfaceView` is a plain `android.view.SurfaceView` (no Compose, no
AndroidX). Its whole contribution to navigation is `onTouchEvent`, which copies
the masked action, the id of any lifting pointer, and each pointer's semantic
sample into preallocated arrays, then makes one JNI call. `GestureDetector` /
`ScaleGestureDetector` are deliberately unused: they would move camera semantics
into the Android layer.

### Editor Workspace composition

The Activity's content view is `EditorWorkspaceView`, a `FrameLayout` with three
children in z-order: the `SurfaceView` at the **whole window size**; `chromeRoot`,
a transparent, non-clickable vertical `LinearLayout` holding every interactive
surface; and `overlayRoot`, holding what must survive chrome being hidden — the
restore chip, the Display popover, the Objects panel, the Add Primitive palette,
Home, the New Project chooser, the unsaved-changes question and the recovery
question.

Inside `chromeRoot`: `GlobalToolbarView` at the top; then a weighted horizontal
row carrying — leading edge first — the Objects column (expanded windows only),
`BrushEdgeControlsView` (Sculpt only), a weighted gap where the model lives, and
one `WorkspaceTrailingHostView`. That host is the one trailing context surface:
one bounded vertical scroll contains the `ToolRailView`, contextual transform
mode and coordinate-space controls, then the precision/details trigger. The row is followed by
`bottomRow`, which wraps its content and holds the Objects capsule on the leading
side and nothing else.
`PropertyInspectorView` is added after `bottomRow` (bottom sheet) or inside the
middle row **immediately before the trailing host** (side placement) — and **only
while it is open**. That ORDER is the invariant: a laid-out sibling in a
horizontal row costs width, so a panel appended after the host translates it by
the panel's own width, and the host would lose the trailing edge for as long as
Exact or Details was open. All are plain
framework views built in code; no Compose, no AndroidX in the product, no design
system, no drawer.

**The viewport is the workspace; everything else is an edge.** Nothing spans the
bottom of the window at rest, and the model reaches the bottom edge. The
exact-value panel is either open with its whole body or absent from the window
entirely — a collapsed panel is still a full-width strip anchored to the edge of
a viewport-first tool, a permanent structural claim made by a surface nobody
asked for. Exact values are ForgeShape's advantage and are not less reachable for
it: they are one tap from the tool context that owns them.

**The right context has one composition owner and one external frame.**
`WorkspaceTrailingHostView` is itself the only floating surface and owns a single
`BoundedScrollView` with a vertical context column. Shape/Transform or Sculpt
brush entries come first, transform mode and space appear below them when
offered, and the precision/details trigger remains the final member of the same
surface. The host owns padding, spacing, corner/elevation and external width.

Its top, right and width do not depend on tool, mode, space, precision-open state,
IME or window class. Context may change only the internal content and total
height/bottom edge, so expansion is downward. If available height is insufficient,
the whole internal column scrolls vertically; no child changes orientation,
detaches or becomes a second surface. The root may compensate for an unrelated
second toolbar status line during measurement, but the final host margin is part
of that same traversal rather than a visual translation.

`EditorWorkspaceView` still owns native reads and commands and passes one derived
`PresentationState` snapshot into the host. Host callbacks report semantic
intent back to the root and never call JNI; the host is presentation authority,
not a second product-state authority.

**Every context surface grows out of the control that opened it.** The scene list
and the Add Primitive palette out of the Objects capsule; the precision surface
out of the rail's toggle. `anchorOverlayTo(overlay, invoker)` is the one
implementation: it takes the invoker's bounds in workspace coordinates, aligns
the surface's leading edge with it, places it above or below according to which
half of the window the invoker sits in, and tells the surface which way to
unfold so the growth animation starts from that corner. Positions are arithmetic
on the invoker rather than a gravity chosen per panel, because the same two
surfaces are opened from a capsule low on a phone and from a column high on a
tablet, and both have to read as attached. The overlay's own fixed width is read
from its layout params, since the anchor runs before the surface has ever been
measured, and the overlay container's inset padding is subtracted because the
chrome container the invoker lives in carries the same padding.

**Primary task surfaces are exclusive by policy, not by z-order.**
`dismissPrimarySurfacesExcept(keeper)` names the Objects popover, Add Primitive,
the precision/details inspector and Display as the four primary surfaces.
Opening one closes the other three through their ordinary workspace paths, so a
close also clears field focus/IME ownership and the invoking control's active
state. A Construction↔Sculpt mode change closes all four, so a Display surface
cannot carry its rail-suspension policy into the next mode. The method
intentionally does not iterate `anchoredSurfaces()`: anchoring
is a motion/placement primitive, not a declaration that every future lightweight
popover must compete for the primary-task slot.

`applyPrimarySurfaceChromePolicy()` owns the two collision decisions. Display
temporarily makes the trailing host `GONE`, then returns it to the same external
frame on dismissal. A bottom-sheet Exact/Details inspector owns the lower region
for its full entry, open and exit lifetime: the conflicting Objects/history row
is `GONE`, never translated above the sheet, and returns at its exact resting
bounds only after the surface is absent. Both decisions are immediate and
unanimated. The Vulkan surface is still full-window and receives none of these
insets or visibility changes.

**A context surface may stand on the model; it may not stand on another
control.** The palette is wider than the distance from the Objects capsule's `+`
to the trailing window edge, so an unbounded clamp would slide it under the
trailing tool cluster and leave a crescent of the precision toggle showing from
behind it. A half-covered control still takes a touch, and the panel over it
reads as a rendering fault rather than as a layer. `trailingLimitFor` bounds the
surface at the leading edge of whichever of the trailing host and an open
side-placed precision surface comes first, less the same `overlay_anchor_gap`
every anchored surface stands off its
invoker, so the *surface* moves and the live control keeps its place. The panel
is part of that trailing region because it is seated inboard of the host; when it
sat outboard, clamping to the host cleared it for free. Skipped for
a surface the cluster itself opened — the precision surface's invoker IS the
cluster's toggle — and never tighter than the surface's own width, so a window
too narrow to seat it beside the cluster still lays it out at the leading edge
rather than at a negative margin. Resolving this by z-order was rejected: the
control would still be under the panel and still be taking touches.

**Each surface answers one question, and only one.** The Global Toolbar says what
mode this is and carries the acts true in every mode that the user does not work
*from*; the Tool Rail says which tool is held and its toggle opens the numbers
behind it; the Objects capsule says which body is current and offers the one
creation affordance; the Property Inspector says what that body's exact values
ARE, naming it in its own title — "Exact Shape — Body #1". Scene-level content
inside the active-body value panel is a role confusion: it buries the fields the
panel is named after below the fold, nests one scroll in another, and vanishes in
every mode that editor is not the inspector's body.

**Objects is a capsule, not a toolbar button.** `ObjectsCapsuleView` is a Tier 1
floating capsule holding the active body's name and a `+`. It is the main entry
to the scene on a phone and is in the same place in **both** modes, which is most
of what makes Construction and Sculpt read as one workspace rather than two
applications sharing a viewport. It holds no scene state: the label is written
from `sceneActiveBodyId()` on every refresh. The Global Toolbar's Objects icon is
gone — a toolbar icon can only offer to open a list, it cannot say which body is
current, and two controls opening the same panel would be two answers to "where
does the scene live". The capsule is withdrawn exactly when the window gives the
scene a permanent column, for the same reason: a capsule naming the active body
beside a list that already names it is one fact drawn twice.

**Creation is a choice, not an append.** Both `+` controls — the capsule's and the
docked column's — are *anchors*: they open `AddPrimitivePaletteView` and create
nothing. A body exists only once a shape has been chosen. The palette offers
exactly six tiles, one per primitive the product builds, and choosing one runs
the two calls a user would make by hand: `sceneAddBody()`, then that primitive's
own `applyConstruction*()` with the parameters **read back from the new body's own
native state**. There is no Java-side generator, no second table of default sizes
and no second validation, so a shape chosen from a palette can be refused exactly
as a typed one can — and on a refusal the body stays a box and the status line
says so, because there is no scene delete and inventing one to tidy up after a
refusal would put a destructive path into the shell. There is deliberately no
*Add from file*, no template and no disabled placeholder: the seam for a later
creation **category** is structural — the palette owns its own layout and routing
— rather than drawn.

**Every entry on the Tool Rail does something.** An entry that looks like a tool
and is not is a promise the shell cannot keep, and half of the one control the
user reaches for most is the worst place in the workspace to spend on a feature
that does not exist. `ToolRailView` has no notion of a reserved entry.
Construction's two entries are *Shape* and *Transform*. **"Transform" carries
both halves**: the direct handles in the viewport and the exact numbers behind
the precision toggle, which are two front ends to one placement. The surface
names itself "Exact Transform — Body #1" because that is what it is — the typed
half — not because it is the only half.

**The transform selectors are contextual members of that one surface.**
*Transform mode* (Move / Rotate / Scale) and *coordinate space* (World / Local)
are transparent vertical groups under the high-level Tool Rail entries, absent
where the gizmo is absent. They use the host's entry material and spacing rather
than drawing detached capsules. Short windows and IME keep this same vertical
grammar and rely on the host's internal scroll. The one approved-but-unimplemented
control left in the product is the global `Export`, which is drawn recessed and
says why.

**The precision toggle belongs to the right context, not to the toolbar.** What
it opens is entirely a function of the high-level entry above it — *Exact Shape*,
*Exact Transform*, or Sculpt *Details*. It is therefore the final internal entry
of the same host surface, names what it will open, and is drawn active exactly
while that surface is up.

**The status line has a lifecycle, and two kinds of message**, both owned by
`GlobalToolbarView`. A **transient** — every verdict: applied, rejected, selected,
created, switched — is written with `showStatus`, holds for `STATUS_HOLD_MS` (5 s)
or `STATUS_FAULT_HOLD_MS` (10 s) in the error role, and then the line returns to
whatever **stands**. A rejection gets the longer hold because ForgeShape's
rejection copy explains a constraint and has to be readable to the end: the
wording is not shortened to fit a timeout, the timeout fits the wording. A newer
message cancels the older one's pending clear before writing — cancel-first, the
same rule and reason as `ChromeMotion` — or the first message's timer would blank
the second early. The **standing** message is set by `showStandingStatus` from the
one refresh every surface re-reads, so it re-asserts itself after any transient
that covered it and clears the moment it stops being true. It is empty in the
ordinary case, and an empty line is **no capsule at all**: a surface with nothing
in it is the same permanent claim on the workspace nothing else is allowed to
make.

**And neither kind is an instruction.** Entering Construction writes nothing, and
so does switching between *Shape* and *Transform*. A caption that is always true
is a permanent surface however short its timeout is, and what such a caption
would say is already answered by what the user is looking at: the held rail
entry, the toggle beneath it that names what it opens, the surface's own title —
*Exact Shape — Body #1* — and the body named on the Objects capsule. There is no
first-run help mechanism, and none is to be built as somewhere to put an
instruction.

**A standing fault is visible without opening anything.** The stale-source warning
lives in the Sculpt context surface next to the action that resolves it, and
because that surface never opens by itself it is *also* the standing message in
Sculpt. It is the only one the product has, and it is a *state* rather than a
verdict — which is what the two kinds exist to separate.

**Creation is offered only where it can succeed.** `sceneAddBody()` refuses in
Sculpt Mode, so both `+` controls — the capsule's and the docked column's — are
`GONE` there, set from the one refresh so the hosts cannot disagree. Nothing is
drawn disabled in their place: a greyed `+` says "not now", which is exactly as
much as its absence says, at the cost of a dead control in the resting workspace.
The scene itself stays reachable. The native refusal path is kept and still writes
its message — it is the guard, and removing a control is not removing a guard.

**The Tool Rail remembers what it was told.** `rebuild()` discards every entry view
and builds fresh ones at the resting background, and it runs from
`setCompactEntries`, which the adaptive pass calls on any window crossing the
short-height threshold — so a rotation left the rail with no entry drawn active
while the tool was unchanged below JNI. The rail keeps the last key it was given
and re-applies it after a rebuild. It still **decides** nothing: the caller reads
the held tool back from native state and pushes it in, and `setEntries` clears the
key outright, because `TOOL_GRAB` and `CONSTRUCTION_TOOL_SHAPE` are both 0 and
carrying one across a mode change would light an entry by coincidence.

### Appearance: roles, not colours

**No colour is written in Java, and no component knows which theme it is in.**
A role is declared once as a theme attribute in `attrs.xml`, given a value once
per theme in `themes.xml`, and referenced as `?attr/fs*`. Backgrounds are
`res/drawable` state lists and content colours `res/color` state lists, both
carrying attributes; the one imperative path is
`EditorControlStyles.themeColor(context, attr)`, for text roles and the two brush
sliders, which are drawn onto a Canvas rather than composed.

That buys three things at once: pressed and active states come from the platform
instead of a repaint call; instances share one parsed `ConstantState` rather than
allocating a `GradientDrawable` per control; and **five appearances cost one
component tree** — one `bg_control.xml`, one `chip()`, one
`control_content_tint.xml`, no `if (warmGraphite)` anywhere — so the fourth and
fifth palettes touched `attrs.xml`, `themes.xml` and `colors.xml` (and the native
viewport ground they clear to) and no view class at all.

**The approved appearance set is five palettes** (UI-OWNER-42): three DARK —
Warm Graphite (the default), Neutral Charcoal and Light Charcoal — whose twelve
owner-approved values each live in `colors.xml` untouched, with everything else
in a palette a derived neighbour of one of those twelve; and, since
`UI-PREF-R1`, two LIGHT — Warm Light (a cream paper) and Cool Light (a steel
paper) — derived through the same roles and held to the same measured contrast
targets, with every text role clearing 4.5:1 on every ground
(`artifacts/ui-pref-r1/CONTRAST.md`). The light two share
`Theme.ForgeShape.LightBase`, which also states that the system bars draw dark
icons; `ForgeShapeActivity.applySystemBarAppearance` restates it through the
insets controller once the window is attached. Below JNI a ground is one of
five `ViewportBackground` members with the three dark indices unchanged, and
every per-ground tool colour — the grid, the gizmo — asks
`viewportBackgroundIsLight` rather than naming a member: grid lines sink into a
light paper instead of lifting off a dark floor, and the gizmo takes the
saturated palette Light Charcoal already used. **Two of the twelve were lightened on
instruction in UI-LAYOUT-R1**, because they were measured below the repository's own
accessibility target on the grounds they are actually drawn on: `*_text_secondary`
(field captions, section headings, slider labels, the active body's name) at
3.93 / 3.87 / 3.59:1 against the precision surface, and `*_text_error` (every
refusal the product reports) at 3.15 / 3.68 / 2.49:1. A refusal a user cannot
read is a refusal that did not happen. Both roles now clear **4.5:1 on all six
grounds** either is ever drawn on — the viewport ground, the chrome surface, the
floating material, the precision surface, a control fill and a field well — with
each hue kept, so the appearances still read as themselves. `UILR1-11` measures
every combination and `EditorWorkspaceThemeTest` holds the caption role to the
body-text target it used to be excused from. There is no `-night` qualifier
and no System member: the appearance is an explicit choice on the Settings
page, and the two light palettes are that choice, never the system's.

**Three material tiers, and every surface is exactly one of them.**
`fsSurfaceFloating` (Tier 1) is a control group standing ON the model — the
toolbar's two capsules, the Tool Rail, the brush controls — and is the only
translucent tier, because the model behind a narrow capsule is informative.
`fsSurfaceContext` (Tier 2) is an expanded surface carrying a body of content to
be read: the Display popover, the Objects panel, the Add Primitive palette,
Home and the other chooser surfaces.
`fsSurfacePrecision` (Tier 3) is the Property Inspector, where exact values are
typed. **Nothing blurs and nothing pretends to**: the viewport is a `SurfaceView`
and the platform cannot blur what is behind one, so "glass" is tone plus opacity
plus a soft shadow — a look the platform actually keeps, rather than a blur that
silently degrades to a grey slab.

**States are fill-led, and nothing rests on an accent outline.** `fsAccentFill`
(what a SELECTED control rests in) and `fsPrimaryFill` (a PRIMARY COMMIT, carrying
`fsTextOnPrimary`) are two roles and deliberately not the same family: a selection
is a quiet lifted surface carried by its fill and its brightened label; a commit
is the accent itself. The selected state carries **no** accent hairline: an
outline reads as the first signal rather than the third, a selected chip becomes
a blue-outlined box, six of them make a panel look like a form of framed cells,
and the outline does nothing the fill and the brightened label are not already
doing. `fsAccentBorder` is consequently referenced by no
drawable and is deliberately still declared and still answered by all three
palettes: those values are owner-approved, and dropping one to record a styling
decision would change an approved palette. The accent is spent on exactly two
things — a primary commit and the ring on a focused numeric field.
`fsTextOnPrimary` is an **ink**, not white: `#4C8FD6` carries white at 3.4:1 and
near-black at 5.5:1. Controls otherwise draw no box until pressed — one step of
tone plus the space around them is the separation — and exactly two resting
outlines are left, both earned: `bg_field`, because a value you can type into is
an editing affordance, and `bg_warning`, because a standing fault is under-stated
by tone alone. A reserved control — `Export`, the only one drawn — is **recessed**
rather than outlined.

Icons are local vector drawables on one 24 dp grid, drawn white and tinted from
the same state list, so an entry's glyph and its caption cannot disagree about
whether it is active, pressed or reserved. Metrics come from `dimens.xml`: corner
radius is four semantic levels — control, floating surface, sheet, capsule — plus
the two **nesting** radii a member takes from its host (see *Chrome depth*), and
type is five roles rather than five sizes.

**The interactive floor is 48 dp, and it is a HIT AREA.** `control_height`,
`icon_button_size` and `brush_touch_width` are all at it; the glyphs are still
20 dp, the rail's compact caption is still 10 sp, and nothing grew a drawn box to
reach the number. Raising it from 44 dp cost 8 dp of the Global Toolbar's row,
and the mode-transition button gives that up rather than an icon control — an
icon control squeezed past the window edge is unreachable, and a button has
width it can give. What it gives it up *to* is arithmetic on the row rather than
a constant: see *the transition is fitted to the row it is in*.

**No type role upper-cases a string the product did not choose character by
character.** `sectionLabel` sets no `setAllCaps`, and that is a correctness rule
rather than a taste one: a section heading can carry a unit — "Position (m)" —
and an upper-casing transform renders that as "POSITION (M)". In SI, `m` is
the metre and `M` is not a unit at all. A transformation that can change what a
symbol *means* has no business being applied automatically, and the next unit to
appear in a heading would have inherited the same defect silently. What separates
a heading from a field caption is weight, tracking and the section gap above it,
which is what was carrying it anyway.

**What the approved palettes cost is stated rather than hidden.** The twelve
anchor values are not the UI layer's to move, so `EditorWorkspaceThemeTest`
asserts what they deliver: primary text and every typed value at WCAG AA (4.5:1),
secondary captions at 3.0:1 (measured 3.6–4.7 across the three), and verdicts at
2.4:1 against the precision surface — the tightest number in the product, Light
Charcoal's error red on its own inspector surface, and a consequence of both the
red and the ground being fixed. That debt is **carried, not silently repaired**:
the twelve anchors are owner-approved values and are not the UI layer's to move.

Every surface in the workspace — the Objects capsule, the Add Primitive palette,
the precision toggle — maps through those same roles. The capsule and the toggle are
Tier 1, the palette is Tier 2, and no colour, tint or state anywhere in them is
written in Java, so the five appearances still cost one component tree.

**The appearance is one field of the persisted `AppPreferences`** (see
*Application preferences* below), read through `AppPreferencesStore` by the
Activity **before** anything is inflated and applied with `setTheme()`, so the
first frame is already the stored palette. It is changed by recreating the
Activity — the only clean way to re-resolve themed resources for a UI built
entirely in code. Safe, because nothing that matters lives in the Activity:
scene, bodies, active ObjectId, mode, Frozen Sculpt Mesh, camera and display
settings are process-scoped native state. Free, because **`onDestroy` skips
`NativeViewport.stop()` while `isChangingConfigurations()`**, so the render
thread, the Vulkan device and every GPU buffer survive and `start()` returns
early rather than re-running the self-tests. `EditorUiState` is handed to the
incoming workspace — the open Settings page included, so the user comes back to
the row they chose. A real process kill returns to the stored palette.

### Application preferences and the Settings page (`UI-PREF-R1`)

**`AppPreferences` is one immutable, versioned value** — the palette, the
handedness, the gizmo's visual scale, its stroke weight and, since
`CAD-VERTICAL-SLICE-R1`, Tool Labels (default off; key `tool_labels`, a missing
or wrong-typed value reads as off, and `SCHEMA_VERSION` is unchanged because a
missing key IS the default) — and it is
**application truth, never project truth**: it enters no `.forge` byte, moves
no fingerprint, dirties no project, records no history step and never reaches a
checkpoint. `SettingsPreferencesTest.uiprefr1_09_10_11_37` serialises a
project, changes every field, and asserts the bytes, the fingerprint, the dirty
flag, both history depths and the native snapshot are identical. It holds no
Android type; the rules it applies when a stored value is read are one per
field and are proven on the JVM (`AppPreferencesTest`): an unknown enum name is
the default, a non-finite number is the default, a finite out-of-range number
is clamped to the nearer bound, a missing key is the exact product default, and
a key this build does not know is ignored — so a newer build's file never
crashes an older reader, without a migration framework.

**`AppPreferencesStore` is the Android adapter**: the app's own
`SharedPreferences` file (`forgeshape_preferences`), written synchronously and
atomically by the platform, read once per process into one cached copy. Enum
fields are stored by NAME, so a reordering can never change a user's palette.
The store knows nothing about projects and no project file, `Uri` or path is
involved. The Activity reads it before `setTheme`; the workspace pushes the two
gizmo values to native presentation state and seats the rail zone on the chosen
edge once per construction, and again on every change.

**`SettingsPageView` is a start page** on `StartPageView`'s terms — opaque,
full-window, content insets, Back at the foot — reached from Home's quiet
Settings row and from the Project surface's `Settings…`, and it is the ONE home
of every persistent preference (UI-OWNER-37). The transient viewport controls
(Shading, Surface, Projection, Grid) stay in the Display popover; the palette,
which used to live there, moved because it is now persistent. Every option is a
full-width list row carrying the selected fill, a check mark, `isSelected()` and
a "selected" content description, so the choice is never colour alone. The page
owns no state: each row reports a request, the workspace writes the store,
applies the change and repaints the page from what is in force. Tool Labels
has its own Interface group (`tool_labels_off`, `tool_labels_on`); the workspace
hands the flag to `CadExtrudeCanvasView.setToolLabelsVisible` at construction
and on every change, and the next anchored refresh draws it. Whether the page
stands is `EditorUiState.settingsOpen`, carried across the recreation a palette
change performs. There is no Handle Style row: the gizmo is a one-pixel line
list by renderer contract and no second style shares its hit semantics, so the
row is absent rather than inert.

**Handedness mirrors edge anchoring and nothing else** (UI-SPEC-R0 Rev1).
`EditorWorkspaceView.applyHandedness` re-seats the middle row's members —
Objects column, brush controls, the weighted gap, a side-placed precision
surface, the trailing host — in mirrored ORDER and swaps their edge margins;
`WorkspaceTrailingHostView.setMirrored` moves the host's one 8 dp inset to the
other margin. Right is the default and reproduces the accepted UI-LAYOUT-R2
frame exactly (`SettingsPreferencesTest.assertRightHandedFrame`); Left seats the
host first in the row with the same width, top and inset, seats a side-placed
precision surface AFTER it with its standoff toward it, and
`leadingLimitFor` keeps an anchored surface clear of a left rail exactly as
`trailingLimitFor` keeps one clear of a right rail. The bottom row and the
Global Toolbar are untouched, and no axis, workplane, camera, gizmo solver,
transform, exported byte or gesture meaning is mirrored — a left-handed user
sees the same model from the same place.

**The Vulkan viewport is full-bleed and stays that way.** No layout decision
insets, pads or resizes the `SurfaceView`; window insets are applied to
`chromeRoot` and `overlayRoot` only. Nothing in the Android layer can therefore
cause a swapchain rebuild, and renderer ownership of the surface is untouched.

**That full-bleed rule is exactly why anchored chrome needs a conversion.** A
projected anchor is in VIEWPORT-CONTENT pixels — origin at the rendered surface,
which is the window — while every anchored surface is a child of the
inset-padded `overlayRoot`, and a view's translation is measured from its own
layout position inside its parent. `ViewportAnchorSpace` is the ONE bridge
(`UI-3D-STATE-C1`), used by `BodyDimensionLabelsView`, `SketchDimensionLabelView`
and `CadExtrudeCanvasView` alike rather than written three times:

```
translation = anchor + (viewportOriginInWindow
                        - parentOriginInWindow
                        - placedLayoutPositionInParent)
```

There is no constant in it — no status-bar height, no navigation-bar height, no
density correction, and no assumption that the horizontal inset is zero — so a
landscape navigation bar or a display cutout is absorbed by the same arithmetic.
The **clamp bound is the real viewport** carried through that same offset, never
the padded content box of whichever container holds the view: a control is held
inside the window, not inside the chrome's margin. A surface is measured under
the constraint its parent will impose, so the box centred on the anchor is the
box drawn. And a surface placed in the very pass that first lays it out is
re-placed once afterwards — a `GONE` view is skipped by its parent's layout and
still reports position 0, so there is nothing better to read at that instant;
the repeat is capped and reset by any settled pass, which is what keeps it layout
readiness rather than a poll.

**The canvas CAD HUD** (`CadExtrudeCanvasView`, `CAD-VERTICAL-SLICE-R1`) is
one compact row — the extent control, the exact value, the operation badge and
Flip — with the extent and operation palettes opening directly beneath it, the
Two Sides value at its own anchor, and the retained-sketch Edit Sketch control
at `cadBodySketchAnchor`. Every icon control is a `LinearLayout` holding a glyph
`ImageView` and an optional caption; the container carries the id, the click,
the selected/activated state and the content description. `CadHudPresentation`
is its pure-Java arithmetic, proven on the JVM: the glyph is
`clamp(28 dp × CAD_EXTRUDE_SCALE, 24, 32)`, the hit area is never below 48 dp
and is never `setScale`d, which extent and operation icon and caption belong to
which mode, and which operations a bitmask offers. The cluster is measured
first and then placed so the VALUE's centre lands on the shaft-midpoint anchor
(`ViewportAnchorSpace.measureUnderParent` + `measureAndPlace`), still clamped
into the viewport. An open palette is centred under the control that opened it;
a closed one is `INVISIBLE`, so it is already laid out when it opens, and takes
no touch.

**What a sketch shows is one statement** (`SketchChromePolicy`,
`CAD-VERTICAL-SLICE-R1`): in Editing the Tool Rail carries the drawing tools and
the orientation navigator and the Line dimension stand; in Ready all three are
absent (`WorkspaceTrailingHostView.render`,
`EditorWorkspaceView.refreshSketchViewportSurfaces`), Back to Sketch and Cancel
stay, and Finish Sketch does not open the precision surface. The toolbar's
Extrude is withdrawn while native's candidate status is not `CAD_OK`
(`GlobalToolbarView.showExtrudeReadiness`), and the named reason is on the
canvas operation badge; the precision surface's pinned Extrude stays, because
it submits a typed depth before it commits. A precision-surface act that
changes the candidate (a region row, a side chip) ends in
`InspectorHost.onSketchCandidateChanged`, which brings the toolbar and the HUD
to the same candidate without a full re-read.

**One refresh path owns every world-anchored surface.**
`EditorWorkspaceView.refreshWorldAnchoredUi()` recomputes ownership AND placement
for the sketch orientation navigator, the selected Line's dimension label, the
CAD extrude cluster with its retained-sketch chip, and the three body-dimension
labels. Each re-reads native truth and decides for itself whether it is shown, so
no shell predicate can disagree with the session. It is deliberately cheaper than
`syncFromNative()` and rewrites no exact-value editor, which is what makes it safe
on every pointer sample of a viewport gesture — and a camera move must reach it,
because an orbit, a pan and a zoom change where every anchor projects while
changing nothing the heavy sync watches for.

Which surfaces exist is decided by **native state**: `syncFromNative()` reads
`NativeViewport.productMode()`, the sculpt state and the active tool, and builds
from that — never from what was last tapped. In Sculpt Mode the shape and
transform editors are not merely disabled but **absent**, and in Construction the
brush controls are, so nothing on screen can edit the representation that is not
being worked on.

**The Global Toolbar is not a bar.** `GlobalToolbarView` is a transparent
container holding two floating capsules — `toolbar_editing_group` (the context
label and the one mode transition) and `toolbar_utility_group` (Export and the
three icon controls) — with a weighted gap and the status capsule between them. A
full-width opaque strip with a hairline under it reads as an Android app bar
whatever colour it is painted. Grouping also carries the hierarchy without
colour: the primary commit is the loudest thing in the leading group and the
trailing group is uniformly tertiary.

**A capsule is a relation between controls, so a group of one is not drawn as a
group.** The editing group holds the context label and the one transition, and a
`COMPACT` window withdraws the label — which left a 26 dp dark pill painted
around a single 22 dp button, a crescent of host showing all the way round it,
and the transition as the heaviest object in a workspace whose subject is the
model. `applyEditingComposition` resolves it in the only direction that is
honest: the lone member *becomes* the capsule, taking `radius_capsule` and the
floating elevation while the group stops painting and stops padding. It is 8 dp
of host removed, never 8 dp of control — the hit area is untouched. Where the
group genuinely holds two members the segmented relation and the concentric
member corner are exactly as they were.

Three chrome layout contracts are load-bearing. **The flexible child of the row
is the gap between the two groups**, so a row out of width closes that gap before
anything gives up a touch target; a `COMPACT` window also withdraws the context
label, because the inspector's title already names the mode and the body.
**The transition is fitted to the row it is in**, in `GlobalToolbarView`'s own
measure pass: the budget is what is left after the utility group has the width it
asked for, an inline status has `toolbar_status_min_width`, and the context label
(where it is drawn) has its bounded share. Unbounded, the button pushed the last
icon control past the window edge in Sculpt Mode; bounded by a single dp constant
sized against the narrowest supported window, it truncated *Back to Construction*
in every window including two with 50 dp of unused row beside it. Only that one
label has a second form, `back_to_construction_short`, taken when the row
genuinely cannot carry the sentence — it is the one way out of Sculpt Mode, and a
destination the user has to guess at is not navigation. The content description
is the full wording in both forms. And a **Tool Rail entry holds its
gesture against the enclosing `ScrollView`**: it disallows interception on Down
and allows it again once travel passes twice the platform slop, at which point
the container takes the next event and `ACTION_CANCEL` prevents the click —
otherwise a scroll container takes the tap the moment a stylus tip drifts. None
of it touches viewport gesture arbitration: all of it is chrome.

Every chrome **surface** swallows every touch inside its bounds that none of its
own controls takes. Because the viewport is a sibling *below* them, and Android
never offers a consumed event to a sibling underneath, a touch on chrome provably
cannot orbit the camera or deform the model. The toolbar container is the one
chrome view that deliberately does **not** consume: it draws nothing, and
swallowing its full-width bounds would put a 56 dp band of dead space across a
viewport the user can see straight through. The rule is enforced one level down —
each capsule is clickable and swallows what its own controls did not take — and
`chromeRects()` reports those capsules rather than the container, so the
viewport-floor measurement describes what is actually painted. In the other
direction, `ACTION_DOWN` on the viewport pulls focus and the soft keyboard away
from any field being edited, so navigation never happens "through" a focused
editor.

### Adaptive layout and window insets

`WorkspaceLayoutMode` is the whole adaptive decision, as arithmetic on **window**
dp — never display size and never orientation, so a rotation, a split-window
resize and a free-form drag all take one path. It holds no Android type and is
unit-tested on the JVM.

| window | class | precision surface, **when open** |
| --- | --- | --- |
| width < 600 dp | `COMPACT` | inset bottom sheet, capped at 30 % of window height |
| 600–839 dp, or any width with height < 480 dp | `MEDIUM` | bottom sheet, or **side overlay** when height < 480 dp |
| ≥ 840 dp wide **and** ≥ 480 dp tall | `EXPANDED` | side panel beside the model, ≤ 30 % of width |

None of the three is a resting state: the table says where the surface *appears*,
not what the window permanently gives up. `WorkspaceLayoutMode` has no opinion at
all about whether it is open — the only thing that opens it is the precision
toggle, and what the user last decided is remembered per mode in `EditorUiState`.
The rule the previous shell had, where a roomy window opened the panel by itself,
is gone with it: a rotation could otherwise put a surface on screen that had
never been asked for.

The height gate does two things at once: a short window never gets a bottom sheet,
and a 914 × 411 dp phone in landscape is not classified as a tablet merely because
it is wide. The surface's body always scrolls and its bottom-sheet height is
capped in `onMeasure`.

**No chrome column spans the window, and none of them is a slab.** Both side
placements and the Objects column wrap their own content and hang from the top of
the row: a panel stretched to a tablet's full height is mostly empty — a box has
three dimensions and a scene often has two bodies — and that emptiness is what
made the expanded layout read as a desktop CAD frame. They still cannot outgrow
the window, because a `WRAP_CONTENT` child of a bounded `LinearLayout` is measured
`AT_MOST` the parent height and a long body simply scrolls. A docked Tool Rail is
top-aligned with them for the same reason: a rail centred on the window while the
panel beside it hangs from the top is one surface stranded halfway down the
model, not a layout.

**Every chrome surface wears the same material in every window.** The Objects
column, the docked rail and the docked inspector are inset from the edge, rounded
on every corner and raised, exactly as on a phone. Drawing them opaque, flat and
squared against the window edge would show the same three controls in two visual
languages on one device depending on which way the tablet was held.
**The trailing host has no docking state.** The rail was top-aligned when docked
and centred on the thumb when floating; the centre anchor is what UI-LAYOUT-R1
removed. UI-ARCH-R1 removed the now-semantically-dead `railDocked` flag and left
one top/right placement contract in `WorkspaceTrailingHostView`. Its explicit
`refreshParentPlacement()` still re-resolves density-backed margins after a
configuration change, preserving the old runtime side effect without preserving
a false state distinction. A phone and a tablet are one workspace with more
room, not two arrangements of the same controls.
`sideDockWidthDp` is 30 % capped at 340 dp rather than 28 % capped at 320,
because a panel must fit its own content before it may be narrow — at the old
numbers a docked inspector gave the primitive chooser 85 dp a chip and clipped
"Cylinder" to "Cyl". The 260 dp floor is unchanged and is what keeps the 60 %
central-viewport rule true at the bottom of the expanded range.

The decision runs at the top of `EditorWorkspaceView.onMeasure`, not in
`onSizeChanged`: a surface added or re-parented during the layout pass is
measured against the previous pass and laid out at zero height. It is idempotent,
so it converges within one traversal. `configChanges` is kept and widened with
`smallestScreenSize` so no window change destroys the Vulkan surface;
`onConfigurationChanged` discards the cached window and re-runs the decision.

The app is edge-to-edge (`Theme.ForgeShape`, `setDecorFitsSystemWindows(false)`).
`setOnApplyWindowInsetsListener` applies `systemBars | displayCutout` — plus the
`ime()` inset, which replaces rather than adds to the navigation bar — as padding
to the chrome containers only. A `WindowInsetsAnimation.Callback` follows every
IME frame and resolves against the root target inset, so a bottom inspector never
remains at a partial early inset. `windowSoftInputMode` is `adjustResize`, but
with decor-fits off the window is **not** resized: the keyboard arrives as a
root-owned inset, the host keeps its vertical spatial grammar, and the Vulkan
surface is untouched.

**Chrome depth.** Every surface that stands over or beside the model carries the
same small elevation, in every layout mode — a docked variant does not drop it,
for the reason above. Containers set `clipChildren(false)`, since a shadow is drawn
outside its child's bounds — drawing only, never hit-testing. The unified right
host owns the one surface and elevation; its internal scroll and groups are
transparent so they cannot read as nested capsules.

**A control's corner is concentric with its host's.** The rule is
`inner = outer − gap`: two rounded rectangles that do not share a corner centre
leave a crescent of the outer one showing at each end, which reads as a rendering
fault rather than as a control. `radius_control_inset` (22 dp) is the corner of a
member of a `radius_capsule` (26 dp) control group with 4 dp of padding;
`radius_rail_entry` (20 dp) is a Tool Rail entry inside the same 26 dp capsule
with 6 dp of padding. It applies to the resting, pressed and active forms alike,
which is why capsule members have their own drawables rather than reusing
`bg_control*`.

**The Objects surface is one view with two hosts, owned by the workspace.** An
expanded window with room gives the scene list a leading-edge column
(`objectsDock`, a `ScrollView`); every other window gives it `ObjectsPopoverView`,
a floating overlay panel opened from the Objects capsule, anchored to it, and
capped at 55 % of the window. The **same `ObjectsSectionView` instance** moves
between them, and
`EditorWorkspaceView` owns it and refreshes it from `syncFromNative()` in **every**
mode — never a second list, because a second Java Objects view would be a second
place for "which body is active" to be remembered, and that answer lives below
JNI. Because one view moves, a viewport pick, a row tap and Add Body all end at
the same native fact and the same `refreshFromNative()`, and no Objects list is
ever nested inside the inspector's scroll.

**A row is a pair, not a control with two meanings** (`UI-OWNER-45`). The label
carries `object_row`, the ObjectId tag and the activated state it always had, and
tapping it selects; the Delete beside it carries `object_row_delete`, the same
tag, the product's existing error colour and the ordinary 48 dp icon-button hit
area, and tapping it removes the body. Two targets rather than one gesture with
two meanings, because a list where the tap that chooses a thing sits anywhere
near the tap that destroys it is a list people stop trusting. The pair itself is a
plain container with no id, so every caller that reaches a row still gets the
label.

**Delete is not confirmed, and that is the same rule the reset dialog follows.**
The product confirms exactly one act — Reset Sculpt from Shape — and confirms it
because it genuinely cannot be undone. A Delete is one Undo away, the history
capsule is on the same screen, and the status line says so in the same breath. A
dialog in front of a reversible act is what trains a user to dismiss the dialog in
front of the irreversible one.

It is withdrawn where it cannot succeed, on both halves of the domain rule: the
last remaining body's row never builds the control, and every row hides it while
sculpting. `deleteControlFor` reports those two the same way, because they mean
the same thing to a user. Both guards remain below JNI regardless — removing a
control is not removing a guard.

The two hosts are mutually exclusive, and enforced rather than assumed: the
Objects **capsule** is `GONE` exactly when the column is up, opening any context
surface closes the others, hiding the chrome closes all of them, and a window
that grows into a column closes both the panel and the palette on the way — the
palette because it would otherwise be anchored to a control that is no longer
there. A column is presentation the window pays for permanently; the panel is
presentation the user asks for and dismisses.

**The column is Construction's, not the window's alone.** Whether Objects gets a
permanent panel is `objectsColumnAffordable && !isSculpting()`, for two reasons.
Body switching and creation are both refused below JNI while sculpting, so a
permanent list of bodies there is a surface with nothing to do — and it is not
free, because the column sits *before* `BrushEdgeControlsView` in the middle row,
so its 180 dp pushed Radius and Strength inboard onto the model and out from under
the reaching hand. Expanded Sculpt gets the phone's Objects capsule in the phone's
place. The window's half of the answer is cached in `objectsColumnAffordable` by
the adaptive pass, so a mode change re-asks through `applyObjectsPlacement` —
called from both `applyLayoutForWindow` and `syncFromNative` — without re-running
the whole decision.

Whether the window can afford it at all is arithmetic, **not a fourth
breakpoint**: `EXPANDED` is necessary and not sufficient, and a window qualifies
only when a 180 dp Objects column, the rail and the inspector still leave a
central viewport at least 480 dp wide. Deriving it means a later change to any column width moves the answer
instead of silently violating that floor, and it is why the bottom of the expanded
range gets no third column: three permanent chrome columns on a large phone in
landscape is the desktop-CAD clutter UI-OWNER-02 rules out. The re-parent is
**instant**, because it runs inside `onMeasure` and starting an animation there is
the same defect as running the decision in `onSizeChanged`. **None of it touches
the render target:** the `SurfaceView` is the whole window in every layout mode.

### Property Inspector ownership boundary

`EditorUiState` is the closed list of what the UI may remember: display unit,
draft primitive kind, which Construction context the rail points at, whether the
precision surface was asked for (per mode, and **false** to begin with),
chrome-hidden, and whether the start question has been answered.
Every field is safe to lose — kill the process and the object is exactly what it
was. Anything that would change the model if it were wrong belongs in native code
instead. The start flag and the theme are the **static** members, and deliberately
so: both questions are per *process*, not per Activity, and an instance field
would be destroyed by the very recreation that applies a theme. The start flag
records only *that* an answer was given, never which one — the mode is native
truth, read back on every refresh, and a copy here could disagree with it.

The primitive chooser is a **draft**: it swaps which parameter fields are on
screen and nothing else. The kind changes only when Apply Shape reads the drafted
primitive's own fields and calls **that primitive's own native method**, so there
is no window in which the object is a cylinder carrying box dimensions.
`refreshFromNative` resets the draft to the object's real kind. Exactly one
parameter row is on screen and it is always the drafted kind's; every primitive's
fields stay populated and converted, including hidden ones, so an inactive draft
cannot silently change meaning off screen.

**The surface ends on a row, not through one.** The bottom-sheet cap is a number
of pixels and the body is a stack of rows of unrelated heights, so the two lined up
only by accident and the boundary regularly crossed a chip or a caption at rest.
`PrecisionScrollView` rounds the visible body **down** to the bottom of the last
row that fits whole, computed from measured heights and margins (a row's
`getBottom()` is 0 during the measure pass it runs in). It caps nothing — the cap
is still `PropertyInspectorView`'s and still comes from the window — changes no
content, order or scroll range, and does nothing when everything fits, so the
content-sized philosophy is intact. A control sliced across its middle reads as a
rendering fault rather than as "there is more below", and in a panel whose whole
claim is exact numbers that is the most expensive thing it can show. The scroll
container clips to its padding, so nothing draws into the sheet's own inset.

**The commit is pinned; the body scrolls under it.** Apply used to be the last
row of the body, which in compact portrait put it three swipes below a fold the
surface did not admit to having. A body that has a commit hands it to the panel
(`PropertyInspectorView.PinnedCommit`), which draws it in a footer below the
scroll: the editor still owns the control, its id and what pressing it means, and
the panel owns only where it is drawn, because only the panel knows how much of
the body is on screen. The panel reserves the title bar's and the footer's full
height in `onMeasure` and caps the scroll with the remainder, for exactly the
reason the trailing cluster does — otherwise a long body would take everything
and squeeze the commit under it to nothing. A body with nothing to commit (the
Sculpt context) does not implement the interface, and the footer is absent. The
scroll draws a fading bottom edge while there is more below it, which is the one
cue the round-down-to-a-whole-row boundary costs: a clean edge reads as the end
of the content.

**The unit chips sit inside the group they convert.** In the placement editor
they are Position's, headed *Position unit*, immediately under the Position
fields — not after Scale, where a millimetre/centimetre/metre choice sat directly
beneath the one group that is unitless by a hard product rule and read as Scale's
units. Rotation is untouched by them because an angle is not a length, and Scale
because a multiplier is not one either. The shape editor keeps *Display unit*,
where the chips do govern the fields above them.

Shape and placement live in **separate inspector bodies with separate Apply
buttons** — *Apply Shape* and *Apply Transform* — because they are separate
truths with different consequences: one republishes the mesh, the other cannot.
One button doing both would hide that. The Construction Tool Rail chooses which
body is on screen; that choice is UI layout state, makes no native call, and
deliberately does not refresh the editors, so a half-typed value in the other
section survives.

Consequently: typing changes no geometry or placement, publishes no revision and
uploads nothing; switching display units makes **no native call at all**, being a
decimal point shift on text applied to size and position and never to rotation,
which is degrees in every unit; and the UI never clamps, rounds or repairs an
invalid value — it refuses only input it can already name a problem with (blank,
non-numeric, and non-positive *for a dimension only*) so it can report a useful
message, and everything else goes to native validation, the final authority. The
positivity rule is deliberately not applied to placement, where zero and negative
are ordinary.

`LengthUnit` converts by exact `BigDecimal` point shift, never a floating-point
multiply, so switching is lossless and a value re-entered in another unit produces
the identical `double` — which is why re-applying the same box in a different unit
reports `Unchanged`. Parsing accepts `.` or `,`; fields use a numeric IME whose
key listener accepts `0123456789.,-`, and the minus matters twice over: a negative
coordinate or angle is ordinary, and a negative *dimension* must be enterable so
it can be visibly refused rather than unreachable.

### Home, the New Project chooser and the CAD bootstrap (`APP-H1`)

**Home is not a project.** The process starts with the scene EMPTY —
`constructionScene()` is built with `NoProjectTag`, and `ConstructionScene::
hasProject()` (at least one body) is the ONE answer to "is a project open"; no
Java flag mirrors it. `EditorWorkspaceView::refreshShellPhase` derives, at the
end of every `syncFromNative`, which of Home, the New Project chooser, the
unsaved-changes question or the editor stands, so a rotation, a recreation and a
resume all land where native truth says. Behind Home the viewport is honestly
empty: nothing is drawn (an empty snapshot), nothing is picked, no history
exists, `encodeProject` returns null, `projectFingerprint` returns 0 and the
autosave worker skips rather than writing an empty fake scene. Nothing
fabricates a default primitive or an invisible placeholder body.

**An empty scene is safe because the funnels are.** The four process-scoped
accessors answer for "no project" without touching `activeBody()`:
`activeConstructionOrNull()` is null, `meshStore()` and `constructionTransform()`
return an unbound store and an unbound identity that belong to no body, and
`sculptSession()` binds a null target (its own `unbound_` answer). The handful of
JNI entry points that read `activeBody()` directly ask `hasProject()` first and
refuse by name (`NoProject`). `activeBody()` on an empty scene still cannot
dereference an empty list: it answers with a process-static null object that is
in no scene and wears `kNoObject`, and COUNTS the read
(`activeBodyMisuseCount`, `debugActiveBodyMisuseCount` across JNI) so the
device suite can assert the product never reads a body across Home — which it
does, in `HomeFlowTest`, on every journey. Nothing here is a repository-wide
optional-body rewrite: `SceneObject`, the history, Delete and the codec are
untouched, and every self-test still builds its scene with the project
constructor that creates the default Box.

**The CAD bootstrap** is the existing spatial support chooser and the existing
volatile `SketchSession` run over that empty scene (`supportChooserBegin(false)`
— no faces, there is nothing to sketch on). What changes is where the first
commit lands: with no project open, `sketchCommit` dispatches to
`commitFirstCadProject` (`forgeshape_project_bootstrap.{h,cpp}`), which builds
a complete one-body `ProjectDocument` from the session's candidate state — one
world-plane CAD body at the identity, wearing the id the allocator hands out
next — and replaces the scene through `loadProjectDocument`, the same validated,
all-or-nothing path Open and Recover take. So the sketch owns no `ObjectId` and
no `SceneObject` until that moment; a refused profile, depth or regeneration
creates no project and leaves the sketch in Ready with its reason in
`lastStatus`; the new project starts with an EMPTY history, as every loaded
document does (undoing the only body would give an empty project, which does
not exist); and Back to Home before that moment costs nothing (`closeProject`
cancels the chooser, the sketch and the borrowed view, and has no body to
remove). Cancel in the bootstrap sketch goes one step back, to the plane
chooser; Back from the chooser goes Home. The toolbar draws **Back to Home**
and withdraws the project and export controls while the bootstrap is open; the
Objects capsule, the history capsule and creation are withdrawn too, because a
body created from the palette then would be a project the user never chose.

**The Sculpt bootstrap** is the seeded path the product already had, inside
the session-initialization bracket: `sceneAddBody`, `applyConstructionSphere`
with the diameter **read back from native state**, then `freezeToSculpt`.
Nothing about Freeze is duplicated, so *Back to Construction* finds the exact
sphere and *Resume Sculpt* returns the same frozen mesh for the ordinary reasons.
A refusal closes the project again and says so: a project that could not become
the sculpt the user asked for is not the project they asked for.

**Leaving a project is guarded by fingerprint.** The workspace remembers the
`projectFingerprint` at the last Save, Open, Open File or Recover and whether
there ever was one; a project whose fingerprint differs, or that was never
stored anywhere (a new project), is dirty. New Project…, Open Saved Project and
Open File… from the Project surface ask `UnsavedChangesPromptView` first when it
is: *Save and continue* writes the app's own slot and continues only if that
succeeded (a failed save keeps the project alive and the question open);
*Discard* continues without writing and retires the recovery checkpoint that
was protecting the discarded changes; *Cancel* — and System Back — return to
the project unchanged. For New Project the project closes at once so the
chooser stands over Home; for Open File the project stays live until a file
actually opens. `closeProject` (JNI) drops the history, cancels every session
and destroys the bodies; it writes nothing.

**Back is deterministic in every phase**, innermost outward: the unsaved
question (Cancel), the New Project chooser (Cancel), a bootstrap sketch (to the
plane chooser), the bootstrap chooser (Home), a support chooser inside a project
(cancelled), then the anchored surfaces. Home with nothing open is the
platform's Back. `hasDismissibleSurface` includes all of these, so the Activity's
predictive-back registration follows them.

### The destructive-act guard

**Two vocabularies, on purpose.** This document, the code below JNI and the view
ids say *Freeze*, *re-Freeze* and *Frozen Sculpt Mesh*, because those name what
the operation does: `SculptMesh::freezeFrom` copies the Construction local mesh
and the copy's topology can never change again. The **user** reads *Start
Sculpting*, *Reset Sculpt from Shape…* and *Sculpt mesh*, because a user starts
sculpting and the copy is the product's business, not theirs. Neither is a
translation of the other and neither is wrong; what would be wrong is one
vocabulary serving both readers. `UIR4B-15` scans every `R.string` the product
declares and fails on any user-facing "Freeze" or "frozen". Nothing enforces the
reverse and nothing needs to.

Three mode transitions live in the Global Toolbar and exactly one is on screen:
**Start Sculpting** while no Frozen Sculpt Mesh exists, **Resume Sculpt** once one
does, **Back to Construction** while sculpting. None is guarded, and none needs to
be: none of the three discards anything.

The one irreversible act in the product is **re-Freeze**, which rebuilds the
sculpt mesh from the current Construction shape and throws away what was
sculpted into the old one. It lives in the Sculpt context surface as **Reset
Sculpt from Shape…** and it confirms **only when `SCULPT_HAS_EDITS` says the
CURRENT frozen mesh has edits** — deliberately not the session-lifetime stroke
count, which describes meshes that no longer exist and would raise a dialog with
nothing behind it on every later re-Freeze of an untouched mesh. Cancel makes no
native call at all.

### Android UI verification boundary

The native self-tests own geometry and math; the runtime smoke owns the
Vulkan/input bridge; the Android suites own shell behaviour, control visibility
and the UI→native call contract. **No Java test asserts a rendered pixel**, every
control is reached by its stable semantic id from `res/values/ids.xml`, and no
assertion depends on a screen coordinate. `src/test` is JVM/JUnit only, over the
pieces deliberately free of Android types; `src/androidTest` is instrumentation
with Espresso deliberately absent — the assertions are view state, measured
geometry and touch consumption read directly from the view tree. "A UI action
changed nothing" is asserted by comparing a **bit-identical** snapshot of the
native primitive, transform and sculpt arrays across the action; "chrome does not
leak a gesture" by dispatching a drag to the surface and requiring it to return
`true` with the native sculpt revision and stroke count unchanged. The camera has
no read-back across JNI, so camera immobility is proven at runtime instead, by a
pixel-identical viewport region across a chrome drag. An **adaptive** case reads
the window it is actually in and asserts the contract belonging to that window,
so running the suite under an overridden window size is a genuine expanded-layout
run rather than a simulation.

**Suites assert no literal colour, radius or shadow — and `UIR4B-14` asserts a
radius RELATION, which is the same rule.** What it pins is `inner = outer −
padding`, which survives any deliberate change to either number; a literal
"10 dp" would break on the next restyle. The same principle covers the other
correction cases: `UIR4B-15` scans every declared `R.string` rather than a list
maintained by hand, and `UIR4B-08` asserts the anchored contract over the set of
anchored surfaces rather than over four named ones — a list is exactly as
complete as whoever last remembered to update it, which is how three of the four
surfaces came to have a wrong first-open pivot while the fourth was correct.

## JNI boundary

`NativeViewport` is the entire boundary, in six groups. **Lifecycle**: `start`,
`surfaceCreated`, `surfaceChanged`, `surfaceDestroyed` (blocks until the
`ANativeWindow` is released), `stop`. **Input**: one `touchEvent` carrying the
masked action, the lifting pointer's id, and per pointer a stable id, view-local
pixels, a neutral tool type, a pressure and a tilt -- see *Pointer semantics*
below. Plus one DEBUG-only reader, `debugLastPointerEvent`, which exists so a
test can observe what actually crossed; it is compiled out of a release build.
**Construction**: `constructionPrimitive` / `boxTransform` to read, one
`applyConstruction*` per primitive plus `applyBoxTransform` to submit.
**Scene**: `sceneBodyCount`, `sceneBodyIds`, `sceneActiveBodyId`,
`sceneSelectBody`, `sceneAddBody`. **Sculpt**: `productMode`, `freezeToSculpt`,
`enterConstructionMode`, `enterSculptMode` (resume WITHOUT re-freezing),
`sculptState`, `setSculptBrush`, `setSculptTool`, `sculptTool`, `sculptClearMask`
and `sculptCanClearMask`.
**Presentation**: `setShadingModel`, `setSurfaceShading`, `setProjectionMode`,
`setViewportBackground`, `setGridVisible`, `setReducedMotion` and their readers —
every one of which requests a value, refuses an index it does not recognise, and
returns what is actually in effect. `setGridVisible` takes a plain `bool` rather
than an index, because there are two answers and no third to refuse.

The methods are listed by group rather than one by one because the *shape* is the
contract and an exhaustive list only drifts. Reading and writing mirror each
other: read the whole section, or submit the whole section. The shape read reports
**every** primitive's parameters plus the active kind, so the panel can populate an
inactive draft without inventing defaults, and each slot of that array has one
fixed meaning whatever the active kind is.

**The shape write is one method per primitive.** There is deliberately no generic
`kind + a + b + c` entry point: a signature whose third double means "depth, or
nothing, depending on an int" is a boundary that only documentation can keep
correct. Each method's parameter list *is* that primitive's parameter list, so
Java cannot ask for a cylinder while sending box dimensions and has no spare
number to get wrong. Below the boundary all six build the matching typed
`PrimitiveSpec` and go straight into `applyConstructionPrimitive`, so the per-kind
split is a boundary shape only and the update-then-publish rule still has exactly
one implementation. The JNI layer adds logging and the status code and nothing
else — it decides no validity, decides no publication, and never touches
`MeshStore` or the transform. The DEBUG primitive driver calls the same function.

Every length crosses in meters and every angle in degrees, so no display unit and
no radian ever reaches native code. Writing carries a whole section in one call
and returns a status code (`APPLY_APPLIED`, `APPLY_UNCHANGED`, or one of the
`APPLY_REJECTED_*` reasons; the transform path reuses the same vocabulary, where
`APPLY_REJECTED_NOT_POSITIVE` simply cannot occur because zero and negative are
ordinary coordinates and angles). There is no `setWidth`, no `setRotationX`, no
`setKind`, no separate `publish`, and no way for Java to observe or produce a
half-applied state. Nothing is ever measured back from the mesh, from GPU data or
from a model matrix.

The sculpt methods follow the same shape: Java may **request** a mode or a tool
and is then told what is actually active, and it reads the brush back rather than
assuming its slider mapping was honoured. There is no method that sets a vertex,
no method that starts a stroke and no method that carries geometry across the
boundary in either direction — a stroke is driven entirely by the existing
`touchEvent` path, and the geometry it produces reaches the GPU through
`MeshStore` exactly as every other revision does. In particular there is no
method for the arbitration: whether a Down becomes a stroke is decided below the
boundary, where the mesh actually is.

### Pointer semantics

One JNI call carries one complete `MotionEvent`, never one call per pointer, and
never more than `kMaxTrackedPointers` (6) of them. `forgeshape_jni.cpp`
translates Android's masked action constants into `forgeshape::TouchAction`;
unrecognised actions (hover, scroll, button) are dropped rather than forwarded,
which is why hover has no representation below the boundary yet.

**A `TouchPointer` carries what a stylus reports, in ForgeShape's own
vocabulary.** Beyond the stable id and view-local pixels it carries:

| field | meaning | units / range | fallback |
| --- | --- | --- | --- |
| `toolType` | `PointerToolType`: Unknown, Finger, Stylus, Eraser, Mouse | closed enum, ForgeShape's own wire codes | `Unknown` -- an ordinary contact pointer, never a dropped event |
| `pressure` | normalised contact force | `[0, 1]`, 1 = the device's full force | `1.0` (full contact) when non-finite or unreported; clamped otherwise |
| `tiltRadians` | lean away from perpendicular | radians, `[0, pi/2]`; 0 = straight up | `0` when non-finite; clamped otherwise |
| `tiltOrientationRadians` | which way it leans, in the screen plane | radians, `(-pi, pi]`; 0 = screen -y | `0` when non-finite or when there is no tilt; **wrapped**, not clamped, because it is periodic |

The two-angle tilt model is the smallest one that keeps direction. Android
reports exactly these two axes, and an Apple Pencil's altitude/azimuth converts
into them with arithmetic alone (`tilt = pi/2 - altitude`), which is what keeps
the contract portable without a portability layer. It is deliberately **not** a
full stylus pose: no barrel rotation, no hover distance, no button state.

**The mapping has one home each way.** Android's `MotionEvent.TOOL_TYPE_*`
constants reach exactly as far as `PointerSemantics` in the Android layer, which
turns them into wire codes; an unrecognised code -- including one a future
Android release invents -- becomes `Unknown` rather than being guessed at.
Ranges and non-finite fallbacks are owned once, natively, in `forgeshape_input.h`
and applied in `forgeshape_jni.cpp`, so the two sides cannot disagree and no NaN
can reach a domain consumer. The stylus arrays are individually optional: a
caller with nothing to say about tilt passes null and every pointer keeps its
documented default, which is also why the pre-stylus behaviour is exactly the
default behaviour.

**Carried is not consumed.** No brush, camera or selection rule reads
`toolType`, `pressure` or tilt. A stylus and a finger tracing the same pixels
produce bit-identical geometry, an eraser switches no tool, and a mouse gets no
wheel, hover or context behaviour. That is asserted, not assumed: the sculpt
suite drives the same stroke path at both ends of the pressure range for all four
tools and compares vertices bit-exactly. Making pressure *mean* something is a
Sculpt stage's work, and it will have to change the brush kernel to do it.

## Camera ownership

`CameraController` (`forgeshape_camera.{h,cpp}`) is the single source of camera
truth and contains no JNI, Android or Vulkan types. It owns `target` (orbit
centre), `yaw`, `pitch`, `distance`; the **projection mode and the orthographic
world span**; FOV, near and far planes; viewport width/height and therefore
projection aspect; and the gesture state machine. It produces a `CameraSnapshot`
— view matrix, projection matrix, eye, target, pose scalars, the active
`ProjectionMode` and the visible half-height — which is the only thing the
renderer, picking and sculpt ever see.

Gesture rule: every touch event recomputes the set of pointers that are still
down, sorted by pointer id. If that set differs from the tracked one, the
controller re-anchors and applies **no** delta; deltas are applied only on Move
events whose pointer set is unchanged. This is what makes 1↔2 pointer transitions
and MotionEvent index reordering jump-free.

### Projection mode

`ProjectionMode` is a closed enum with two members — `Perspective` (the product
default) and `Orthographic` — and a switch, the same rule the sculpt tools and
the shading models follow. There is no camera framework and no projection
registry.

It is **camera/presentation state, never geometry truth**. Changing it mints no
`MeshRevision` and no `SculptRevision`, moves no vertex, and touches no
Construction parameter, `PrimitiveKind`, transform or `ObjectId`. It changes
which pixels a fixed piece of geometry lands on, and nothing else. Like the
camera pose and the display settings it is process-scoped — see *Lifecycle
contract*.

| | Perspective | Orthographic |
| --- | --- | --- |
| Matrix | `mat4Perspective(kFovYRadians, aspect, near, far)` | `mat4Orthographic(halfHeight, aspect, near, far)` |
| Field of view | 60° vertical (`kFovYRadians`), unchanged by this stage | not applicable |
| Visible scale set by | `distance` | `orthoHalfHeightMeters` |
| `proj.m[11]` | `-1` — divides by depth | `0` — **w is 1 everywhere**, a true parallel projection |
| Depth | `[0, 1]`, non-linear | `[0, 1]`, linear |
| `snapshot.eye` | the pinhole, `target + dir × distance` | the **view-plane centre**, `target + dir × kOrthoViewPlaneDistance` |
| Pinch changes | `distance`, clamped to `[0.35, 400] m` | `orthoHalfHeightMeters`, clamped to `[0.02, 250] m` |

**The orthographic scale is a world length, not a zoom factor.**
`orthoHalfHeightMeters` is half the world-space height the viewport shows, in
meters, measured at the target plane, so it can be reasoned about against an exact
Construction dimension. The snapshot carries it in **both** modes: in Perspective
it is the equivalent framing `distance × tan(fovY / 2)`, never a stale leftover.

**Switching preserves the framing at the target plane**, converting between the
two descriptions rather than resetting — `orthoHalfHeight = distance ×
tan(fovY/2)` one way and `distance = orthoHalfHeight / tan(fovY/2)` the other.
These are one identity read in opposite directions, so a round trip returns to
where it started (up to the distance clamps). The target, yaw and pitch are never
touched, so the frame keeps its centre and its viewing direction and the object
can neither jump nor vanish.

**Why the orthographic eye is pulled back.** A parallel projection produces the
same image from anywhere on the view axis, so the view plane's distance is free,
and `kOrthoViewPlaneDistance` (= `kFarPlane / 2`, 250 m) spends that freedom on
centring the `[near, far]` slab on the target: nothing framable is sliced by the
near plane, and every drawn surface lies in front of the pick-ray origin, so
*what is pickable stays what is drawn*. Ortho depth is linear, so a 500 m slab
costs no precision worth naming. The consequence to know: `snapshot.eye` is
**not** `target + dir × distance` in Orthographic, and a reported pick distance
is measured from that view plane rather than from the orbit eye.

**Pinch must not fake orthographic zoom with distance**, which would change
nothing on screen and read as a dead gesture. Pinch changes the visible span; the
orbit distance is left alone, because it is still the pose radius and still what
a switch back to Perspective is computed from. Pan is scaled by whichever
quantity is active, so "one pixel of finger is one pixel of world at the target
plane" holds identically in both modes.

### Screen ray, per projection

`buildPickRay` handles both, and the difference is structural rather than a
tweak to a constant:

| | origin | direction |
| --- | --- | --- |
| Perspective | one point — the eye | depends on the pixel; the rays fan out |
| Orthographic | depends on the pixel; slides across the view plane | one shared direction — the view axis |

Both are inverted out of `camera.proj` and `camera.view`; neither restates a
field of view, an orthographic span or an aspect. Keeping a perspective origin
under an orthographic image would agree with the picture only at the screen
centre and drift further toward every edge — which is why the picking suite
probes off-centre pixels and round-trips each hit back through the same matrices
to the pixel it came from.

The same split governs the sculpt brush. `worldPerPixelAtDepth` reads
`proj.m[5]` in both modes, but multiplies by the hit depth only in Perspective:
a parallel view does not open with distance, so the orthographic brush covers the
same amount of surface at every depth. `CAMPROJ-11` measures both halves — that
the orthographic radius does *not* move when the object is pushed along the view
axis, and that the perspective one does.

## Canonical winding and culling

A triangle's vertices are ordered **counter-clockwise when the triangle is viewed
from outside the surface**, in right-handed world space. Equivalently the
geometric normal `N = (v1 - v0) x (v2 - v0)` points away from the solid, and a
triangle is front-facing to a ray when `dot(N, rayDirection) < 0`.

The projection in `forgeshape_math.h` flips Y for Vulkan clip space. That flip
lives **in the matrix** and is already applied by the time Vulkan classifies a
triangle, so it must not be compensated for a second time in the pipeline enum.
The pipeline uses

```
cullMode  = VK_CULL_MODE_BACK_BIT
frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
```

CPU picking accepts front faces only, under the same rule, so what can be picked
is exactly what the rasterizer draws. The self-tests assert the convention
triangle by triangle, assert that render normals and the model→view normal
transform stay outward (`NOR-01`..`NOR-10`), and assert that a ray fired from
inside a closed solid misses under front-face-only picking.

**The Plane is a bounded, explicitly named exception to both halves of this
rule**, never a change to the rule itself. It is the one primitive with no
"inside": a flat, open, single-sided sheet has no far wall a two-sided render
or pick could wrongly reach, unlike every other, closed primitive above.

**Sidedness is a property of the active published representation, and exactly
one value carries it.** The chain is:

```
ConstructionMesh::renderBothSides      (set only when generating a Plane)
  -> SculptMesh::renderBothSides()     (copied wholesale by freezeFrom)
    -> RuntimeMesh::renderBothSides()  (carried by BOTH publication paths:
                                        publishConstructionObject and
                                        publishSculptMesh)
```

and the three consumers all read the last link and nothing else:

| Consumer | Reads |
| --- | --- |
| Render (`buildRenderMesh`) | `RuntimeMesh::renderBothSides()`, duplicating its one-sided result — see *Derived render geometry and shading* |
| Selection (`pickScene`) | `meshStore().current()->renderBothSides()` |
| Sculpt hit-test (`SculptStroke::begin`, `SculptSession::hitsSculptMesh`) | the `SculptMesh`'s own `renderBothSides()` |

**No consumer may re-derive this from `PrimitiveKind`.** A Frozen Sculpt Mesh
outlives the Source it was frozen from, so the Source can be a Plane while the
frozen geometry is a closed solid, and the reverse — asking the Source is wrong
in both directions. `forgeshape_selection.cpp` deliberately does not include
`forgeshape_construction.h`, so the dependency that permits the mistake is absent
rather than merely unused; `SIDE-01`..`09` assert the chain, both stale-source
directions included. The pipeline is untouched: one global
`VK_CULL_MODE_BACK_BIT` / `VK_FRONT_FACE_COUNTER_CLOCKWISE` for every primitive
including the Plane.

**Naming this `VK_FRONT_FACE_CLOCKWISE` double-counts the Y flip** and inverts
culling. It does not blank the viewport: a closed solid keeps its silhouette but
draws its far walls, so every convex primitive reads as a concave interior — and
the renderer silently splits from the picker, which stays correct.

## Construction domain

`forgeshape_construction.{h,cpp}` owns the product geometry. It contains no JNI,
Android, Vulkan, renderer or UI type, and it holds no GPU resource.

### The scene, and the active body

`forgeshape_scene.{h,cpp}` owns the collection. `ConstructionScene` holds an
ordered vector of `SceneObject`, one per Construction Body, plus the id of the
active one. Platform-neutral C++17 like the rest of the domain: no JNI, no
Android, no Vulkan, no renderer type.

A `SceneObject` owns exactly three things, and each is **per body** rather than
process-global — which is what makes several bodies possible at all:

| Owned per body | Why |
| --- | --- |
| `ConstructionObject` | the exact primitive, its parameters, and (through it) its `ConstructionTransform` |
| `MeshStore` | its own publication chain, so revisions are per body and A's edit cannot replace B's mesh |
| `FrozenSculpt` | its Frozen Sculpt Mesh and its stale-source flag |

Bodies are held by `unique_ptr`, so their addresses are stable as the collection
grows — a `SceneObject` owns a mutex through `MeshStore` and must not move — and
`SceneObject` is non-copyable, because duplicating one would duplicate identity.

**`constructionObject()`, `meshStore()` and `sculptSession()` mean "the ACTIVE
body's".** They are defined in `forgeshape_scene.cpp` rather than beside their own
types, because their answer is a scene question and defining them in their own
translation units would make those units depend on the scene, which depends on
them. Every caller that means "the object the user is editing" reads correctly;
only code meaning "every body in the scene" — the renderer and scene picking —
enumerates the collection.

**`ObjectId` allocation.** Monotonic, minted by the scene, never reused, and
never derived from a collection index, a `MeshRevision` or a GPU resource. It
survives primitive edits, transform edits, Freeze/Resume/re-Freeze and
selection. The first body keeps `kConstructionBoxObjectId`, so startup is
identical to the single-object product's. **Delete does not roll it back**: a
deleted body's id returns by NAME through its Undo and the next creation mints a
fresh one, so there is deliberately still no reuse policy. Reuse is what would
let a stale `ObjectId` held in a selection or a render snapshot resolve to a
different body.

**The collection is flat.** Root-level bodies in insertion order, with stable
enumeration. There is no parent, no child, no group, no reorder and no
speculative field for any of them.

### The scene snapshot

`ConstructionScene::snapshot()` is the one thing the renderer and CPU picking
consume. Per body it carries an `ObjectId`, a `RuntimeMeshPtr`, a model and an
inverse-model matrix, and a selection flag — a `shared_ptr` copy and two
matrices, and **no geometry**.

That cheapness is the point. A snapshot is taken under the existing state mutex
and then used with that mutex released, so no lock is ever held across normal
generation, a GPU upload or a triangle scan. Because each item holds a
`shared_ptr` to the exact revision it names, an older snapshot stays valid and
self-consistent even while newer revisions are published, and a body with
nothing published yet is simply absent rather than a null to guard against.

**What the viewport draws and picks is ONE list, under a restriction the scene
is GIVEN** (Stage027). `snapshot(const SceneViewRestriction&)` skips a hidden
body and, in the same loop, any body outside `restriction.isolateTo` — so durable
Hide and the Sculpt Isolate compose, the second can only remove bodies and never
bring a hidden one back, and an id no body carries yields an empty list rather
than a substitute. The scene reads no session: `viewSceneSnapshot()`
(`forgeshape_scene.cpp`) reads the isolate from the Sculpt session and hands it
in, and it is the ONLY call the renderer (`forgeshape_jni.cpp`, the frame's
`setScene`) and `pickScene` make — neither carries an isolate predicate of its
own, and the selection outline follows because its mask pass rasterises the
same list. Callers that are not the viewport (the CAD support chooser) take the
default, unrestricted snapshot.

The isolate value itself (`SculptSession::isolateTarget_`, one `ObjectId`) is
session-only view state: never serialized, never in a history step, a
checkpoint or the fingerprint, never a visibility write and never an
`AppPreferences` field. Every Sculpt entry (`freezeToSculpt` from Construction,
`enterSculpt`, including a loaded Sculpt project) and exit (`enterConstruction`,
including Close) clears it, so Construction is never drawn isolated and Resume
opens un-isolated; a Reset from source keeps it, because it is not geometry.
The control is the Sculpt Property Inspector's (`SculptContextView`,
`R.id.sculpt_isolate`), reading `sculptIsolated()` on every refresh.

**Two Sculpt entry guards sit beside it (Stage027).** A viewport tap that starts
no stroke is refused in Sculpt before the pick
(`FORGESHAPE_SCENE_SELECT_REFUSED:in_sculpt_mode:viewport_tap`), so neither the
viewport selection nor the active body — which the Sculpt session re-binds to on
every access — can move; `sceneSelectBody` already refused the Objects-row
path. And Start Sculpting / Resume Sculpt refuse a hidden active body by name
(`SCULPT_REFUSED_HIDDEN_BODY`), with the toolbar withdrawing both controls over
one.

**Lock order:** the one state mutex, then `MeshStore`'s own mutex inside
`publish`/`current`. `ConstructionScene` is deliberately *not* internally
synchronised — a second scene-level lock would add a new ordering to get wrong,
for a collection whose callers already hold the state mutex.

### The one active body's object

`ConstructionObject` is the single owner of everything that makes one body what
it is:

- a stable `ObjectId`;
- a `PrimitiveKind` — `Box`, `Cylinder`, `Sphere`, `Cone`, `Capsule` or
  `Plane` — saying which primitive is **active**;
- a `ConstructionBox`, a `ConstructionCylinder`, a `ConstructionSphere`, a
  `ConstructionCone`, a `ConstructionCapsule` and a `ConstructionPlane`, each
  holding its own exact parameters;
- a `ConstructionTransform` holding its placement.

One per Construction Body, owned by that body's `SceneObject`. It is not a
registry — no container, no list, no parent and no child; the collection is the
scene's job, above it. `constructionTransform()` returns
`constructionObject().transform()` rather than owning a singleton of its own, so
identity, shape and placement are one body's state and cannot drift apart.

**Every** primitive's parameters are retained across a kind change. Switching
Box → Cylinder → Sphere → Box does not silently forget the box's dimensions; only
`kind` decides which set is authoritative for the geometry right now. That is
also what lets the panel show an inactive primitive's remembered values without
inventing defaults of its own.

### The typed primitive payload

A primitive *request* is a `PrimitiveSpec`, and its payload is a
`std::variant<BoxDimensionsMeters, CylinderDimensionsMeters,
SphereDimensionsMeters, ConeDimensionsMeters, CapsuleDimensionsMeters,
PlaneDimensionsMeters>` — a real tagged union, not a struct carrying all six
groups at once. A spec built for a cylinder physically does not contain box or
sphere values, so there is no "inactive parameter group" riding along beside
the active one and nothing for a caller to read by mistake.

A cone's `(diameter, height)` and a capsule's `(diameter, totalHeight)` are the
same two numbers in the same order meaning different things; the payload makes
reading one as the other impossible rather than merely discouraged.
`kind()` is **derived** from the payload's alternative index rather than stored
beside it, so tag and data cannot disagree; a `static_assert` pins the variant's
alternative order to `PrimitiveKind`'s. Access is typed — `box()` … `plane()`
each return a pointer that is null unless the spec really is that primitive —
and specs are built only through the matching `forBox` … `forPlane`.
`ConstructionObject::spec()` therefore returns only the **active** primitive's
parameters; the remembered parameters of inactive ones are reachable only by
asking for that primitive by name, which is what the panel's draft display does.

**Update dispatch is typed, not stringly or if-else.** Both the "changed?" check
and the write `std::visit` the payload against a private per-kind overload set
(`parametersDiffer` / `writeParameters`) rather than an if-else chain. A
`PrimitiveSpec` alternative with no matching overload fails to **compile** — the
same guarantee `validateParameters` has — so a seventh kind added to the variant
without its overloads cannot ship.

### Source-of-truth hierarchy

```
ConstructionObject           kind + that kind's parameters
    |                        (double meters — the ONLY authoritative dimensions)
    -> generateMesh()        (derived float RuntimeMesh data, LOCAL space)
        -> MeshStore         (immutable revision)
            -> CPU picking + Renderer
                -> Vulkan buffers

ConstructionTransform        (double-meter position, double-degree rotation)
    -> modelMatrix() / inverseModelMatrix()   -> Renderer / picking
```

These directions are the only ones that exist. Nothing infers a parameter from
mesh vertices, from a bounding box, from a model matrix or from GPU data; the
parameters are read, the mesh is regenerated, and the mesh is republished. The two
branches are independent, and that independence is the point: a shape change
republishes the mesh and leaves the placement alone, while a placement change
changes one 4×4 matrix and publishes nothing.

### Unit contract

The internal Construction length unit is the **meter**, carried as `double` and
named `...Meters` at every boundary (`Meters`, `widthMeters()`,
`diameterMeters()`, `BoxDimensionsMeters`, `CylinderDimensionsMeters`).
`RuntimeMesh` positions are **derived `float`** data.

Curved primitives are authored by **diameter**, because that is what a drawing
and a caliper give you. `radiusMeters()` exists but is derived on demand and
never stored — as is the capsule's cylindrical middle.

mm/cm/m exist **only** above the JNI boundary, in `LengthUnit` and the two
Construction inspector bodies. Every value crossing into native code is already in
meters, and no native type, function or log line names a display unit. The unit a
dimension was typed in is therefore unrecoverable from domain state — which is
correct: it is not part of what the object is.

### Primitive representation

`N = kPrimitiveRadialSegments` (32) and `S = kPrimitiveLatitudeStacks` (16) are
the one owner of how finely every round primitive is divided; each generator
names those constants rather than writing down a count of its own, so a capsule's
hemispherical end is provably the same tessellation as a sphere's —
`kCapsuleHemisphereBands` is literally `S / 2`. `R` below is the capsule's ring
count, `S`.

| Primitive | Authoritative parameters | Axis and origin | Tessellation | Vertices : indices |
| --- | --- | --- | --- | --- |
| Box | width, height, depth | axis-aligned, centred on local origin | none — a fixed index table | 8 : 36 |
| Cylinder | diameter, height | axis local +Y, centred | `N` radial segments | `2N+2` = 66 : `12N` = 384 |
| Sphere | diameter | centred | `N` meridians × `S` stacks | `(S-1)N+2` = 482 : `2N(S-1)`·3 = 2880 |
| Cone | bottom diameter, height | axis local +Y, base at `-h/2`, apex at `+h/2` | `N` radial segments | `N+2` = 34 : `6N` = 192 |
| Capsule | diameter, **total** height | axis local +Y, centred | `N` meridians × `R` rings | `RN+2` = 514 : `6NR` = 3072 |
| Plane | width (local X), depth (local Z) | lies in the local XZ plane, `y = 0`, front `+Y`, centred | none — a fixed 4-vertex, 2-triangle rectangle | 4 : 6 |

Every row above but the last is a **closed solid**; the Plane is the one
open, single-sided sheet, and it is called out on its own after the shared
invariants below rather than folded into them.

Invariants every generator shares:

- **Bounds are exact, not approximate.** The cardinal directions (and the sphere's
  and capsule's equator/seam rings) are **written down rather than computed**:
  `cos(pi/2)` in floating point is about 6.1e-17, not 0, and that error would make
  the bounds not quite the radius. `N` is divisible by four so those directions
  land on real vertices, `S` is even so exactly one ring falls **on** the equator
  plane, and the two exact factors are multiplied in `double` before the single
  conversion to `float`. Self-tests assert exact equality on all six bounds, not a
  tolerance, across shapes as extreme as 40 m x 0.01 m.
- **Winding is the canonical convention above**, so sides and caps alike survive
  back-face culling, and closure is asserted directly: every directed edge exactly
  once with its reverse present, and `V - E + F = 2`, which fails on a hole, a
  duplicated triangle or a triangle wound the wrong way. **No degenerate
  triangles** - a pole or apex is one vertex fanned to its adjacent ring, never a
  collapsed ring, whose `N` zero-area triangles would be noise in every winding,
  area and validation check.
- **Generation is a pure deterministic function of the parameters.**
- **Tessellation is fixed and not exposed.** It is a rendering detail of an exact
  shape, not a property of it; exposing it would put the approximation into
  authored state, to be persisted, versioned and validated like a real parameter.
  Between two ring vertices the surface is a flat facet, so a picked point on a
  curved side lies between `radius * cos(pi/N)` and `radius` from the axis
  (`radius * cos(pi/N)^2` on a sphere). The *parameters* stay exact, and no
  dimension is ever reconstructed from mesh floats.
- **Vertex colour is derived presentation data**, not Construction truth: no
  dimension is ever recovered from it. `RuntimeMesh` carries **no normals**, so
  per-primitive shading hardness is not expressible today.

Four cases need more than the table says.

**The cone's base is closed** and its apex is a true point - apex radius is zero
by definition and is not a parameter, because a truncated cone is a different
shape, not a cone with an extra number. The side triangle is the cylinder's
`(b0, t1, b1)` with the top ring collapsed to the apex; the degenerate
`(b0, t0, t1)` partner does not exist. The base fan is the cylinder's bottom cap
unchanged, and `V - E + F = 2` is what would fail if the base were left open.

**The capsule's middle is not a special case in the index loop** - just the band
between the two seam rings, from the same code as every other band, centred on
the origin. The poles are written down from the authoritative **total** height
rather than accumulated as `middle/2 + radius`.

**The capsule equality case is a sphere, and is generated as one.**
`totalHeight == diameter` describes a capsule with no middle: the two seam rings
are **the same ring**, `R` is one smaller, that band does not exist, and the
result is exactly the sphere's 482 : 2880 - pinned by a `static_assert`, which
keeps the degenerate case free of zero-length rings and zero-area triangles.
Capsule topology is therefore a deterministic *function* of the parameters (the
two sides of one exact `double` comparison), making the capsule the one primitive
whose edit can change a vertex count and so trigger a buffer growth.

**The Plane has no tessellation constant and no relation to validate.** Source
topology is always 4 vertices and 2 triangles, `kPlaneIndices = {0, 3, 2, 2, 1,
0}` over the four corners, CCW seen from `+Y`, so both geometric normals are
exactly `(0, 1, 0)`. Width and depth are validated independently by the same
`validateDimensionMeters` as every other per-length field; there is no
cross-field rule because the two extents do not constrain each other. Being the
one open, single-sided primitive, it is the one whose `ConstructionMesh` sets
`renderBothSides = true`. It is also the one primitive that can sit exactly
coplanar with the world reference grid, which is why the grid carries a depth
nudge.

### The capsule relation, and float resolvability

The capsule is the only primitive whose two parameters are **related** rather
than independent, so it is the only one with validation beyond the per-length
rule. `validateCapsuleMeters` owns both halves:

- `totalHeight >= diameter`, because the two hemispherical ends alone are already
  `diameter` tall. Violating it reports `DimensionValidation::RelationInvalid`, a
  reason distinct from `NotPositive` because "0.5 m is not a length" and "0.5 m is
  too short to be this capsule's total height" deserve different messages. It
  surfaces at the JNI boundary as `APPLY_REJECTED_RELATION`.
- **float resolvability**: the generated positions must not merely be finite,
  they must be far enough apart to describe the shape. A 1e30 m capsule 1e-6 m
  across is finite in every coordinate and yet its whole hemisphere rounds to a
  single float, which would be a mesh of zero-area triangles. The check compares
  the pole against the first ring below it — the smallest latitude step anywhere
  on a hemisphere, so resolving it resolves all of them — and refuses the request
  as `NotRepresentable`. This is the one validation deliberately coupled to the
  tessellation constant: it asks "can this shape be built at this fidelity".

The UI restates neither half. It refuses only what it can name from the text
alone (blank, non-numeric, non-positive), and the relation is reported back to it
as an ordinary rejection.

### Update and invalid-update behaviour

`ConstructionObject::setPrimitive` reports `Applied`, `Unchanged` or `Rejected`:

- **Rejected** — a parameter that is not finite, not positive, whose derived
  `float` half-extent would be zero or non-finite, or that violates the capsule
  relation. This **fails closed**: every value the request carries is validated
  before anything at all is written, so a bad depth cannot leave a half-applied
  width behind *and a bad diameter cannot leave the kind switched*. The previous
  kind, parameters, transform and mesh revision all stand. The only limits are
  physical validity and float representability — no arbitrary size policy.
- **Unchanged** — the request is valid, the kind matches and every relevant
  parameter matches. Nothing is written and no revision is published, so a
  redundant edit cannot cost a mesh revision or a GPU upload.
- **Applied** — the kind and/or the parameters changed, and a new mesh revision
  is published.

A **kind change is always a change**, even when the target primitive already held
exactly those parameters: the object is a different shape afterwards.

### The one apply entry point

`applyPrimitive(object, store, spec)` is the single place where a shape change
becomes a mesh revision. It validates, updates and publishes, and returns a
`PrimitiveApplyResult` carrying the status, the rejection reason, the
authoritative spec after the call, the published vertex/index counts, the store's
resulting revision and whether anything was actually published. Because
`PrimitiveSpec` carries the typed payload described above, it never branches on a
kind field that could disagree with the numbers beside it.

Callers therefore decide nothing. The product UI path (for all six primitives)
and the DEBUG driver all go through it, so there is exactly one implementation of
the rule in the process. `publishConstructionObject` remains separately callable
for the startup publish, which changes no parameter. Atomicity is preserved by
construction: nothing is written unless every relevant value is valid, and no
caller can observe a changed shape without the matching published revision.

**The transform is never read, written or reset by this call**, in any outcome.
That is what makes "change the shape, keep the placement" true by construction
rather than by remembering to preserve it.

The `ObjectId` is fixed at construction and is independent of the kind, the
parameters and the mesh, so no shape change, primitive change, mesh revision or
buffer reallocation can alter what is selected. Turning the box into a cylinder
does not create a new object; it changes what this object is.

## Construction history and the transaction boundary

`forgeshape_history.{h,cpp}` owns one thing: what a Construction *edit* is, and
how to go back to before one. It is platform-neutral C++ over a
`ConstructionScene` handed in by reference, so a self-test drives a whole history
against a scene of its own and the process-scoped pair (`constructionHistory()`
over `constructionScene()`) is just the one the product happens to use.

### What a transaction is

`beginEdit()` captures the scene's Construction-domain state; ordinary domain
mutations then run; `commitEdit()` compares the state afterwards and records
**exactly one** step if — and only if — the two differ. `cancelEdit()` puts the
captured state back and records nothing.

There is deliberately **no separate update call**. An update is an ordinary
mutation made while an edit is open, so the live state stays authoritative for
the renderer and the picker throughout, rather than being buffered somewhere the
rest of the product cannot see. That is the property the direct transform gizmo
needs: a drag opens one edit, writes the transform on every frame exactly as a
typed Apply does, and commits once.

Nesting is refused rather than counted. A composite user act opens ONE edit
around the mutations it is made of, and the mutation entry points each declare a
`ScopedConstructionEdit`, which opens an edit only if none is open and commits
only the one it opened. That is what lets a single Apply be its own step *and* be
absorbed into the creation transaction around it without either caller knowing
which case it is in.

### What is a transaction, and what is not

A transaction is one **user act on the Construction Source**: an Exact Shape
Apply, an Exact Position+Rotation Apply, and the creation act that is
`sceneAddBody()` plus that primitive's own apply. Choosing Sphere is therefore
one step, and undoing it removes the body outright rather than leaving behind the
default Box the append produces before the primitive is written.

Not a transaction, and never a step: selection, the Shape/Transform context, any
surface opening or closing, the display unit, the appearance, the grid, shading,
projection, chrome-hidden, a rotation or a resume — and **crossing the
Construction/Sculpt seam**. A commit that finds no difference records nothing and,
crucially, leaves the redo stack alone, so a refused or identical Apply after an
undo does not throw the forward branch away. Only a commit that actually records
clears redo.

### What a step holds

A bounded before/after copy of the **Construction-domain** state: per body, its
`ObjectId`, which primitive is active, all six primitives' remembered parameters
and its placement, plus scene order and which body was active. Roughly twenty
doubles per body.

It holds no vertices and no indices — geometry is derived from the parameters by
the same generator the product already uses, so copying a published mesh into
every step would store a product of the truth beside the truth at hundreds of
kilobytes a step. It holds no sculpt vertex, no `SculptRevision` and no stroke.

`activeBodyId` is carried but **not compared**: a step restores the selection so
that undoing a creation leaves a valid active body and redoing one re-selects
what came back, while picking a different body remains no edit at all.

A snapshot rather than a typed inverse per operation, because a per-operation
inverse would have to be written and kept correct for every mutation *and every
composition of them*, and because creation's inverse spans the body list, the
order and the selection. The cost is that a step is proportional to the scene
rather than to the edit; at this scene size that is kilobytes.

**Bounded**: `kConstructionHistoryCapacity` = 64 undo steps, in memory, for the
life of the process. Beyond it the oldest step is dropped, one per commit,
deterministically; the current state and the redo stack are unaffected, because
dropping a step only shortens how far back the user can go. There is no disk
history: the recovery checkpoint and the manual slot carry the PROJECT and never
a step, so a load — Open, Recover or a restart — always begins a fresh, empty
history.

### Identity, and the bodies a step holds

The `ObjectId` allocator is **only ever pushed forward**. Undoing a creation does
not roll it back; a redo restores id 5 by name and the next creation mints 6.
Reuse is what would let a stale `ObjectId` held anywhere — a selection, a render
snapshot — silently resolve to a different body.

`ConstructionScene` grew three internal operations for this — `detachBody`,
`insertBody`, `makeBody` — reachable from the history and, since `UI-OWNER-45`,
from `deleteSceneBody`. A detached body is **held whole** rather than destroyed,
Frozen Sculpt Mesh and Imported Mesh included, so putting one back restores the
same object rather than a fresh one wearing its id. `holdDetachedBody` is how an
act outside the history hands one over, and `pruneDetachedBodies` releases
anything no step in EITHER stack still names, on either side — a redo restores an
undone creation from a step's AFTER state, an undo restores a deleted body from a
step's BEFORE state. There is deliberately still no Duplicate.

### Publication discipline

A restore does the least geometry work it can. A body whose **shape** differs is
restored and republished once; a body whose **placement** differs is restored and
publishes nothing, exactly as an ordinary transform Apply publishes nothing; a
body coming back from the history publishes nothing either, because it still
holds its own published revision — only a body that has never published anything
is forced to. Commit itself publishes nothing at all: the mutations already
produced the final state. `ConstructionRestoreReport` reports those counts so
this is a checkable claim rather than an assertion.

The state mutex is held across a restore, which is the one place in the product a
lock is held across mesh generation. It has to be: a restore rebuilds the scene's
body list, and the render thread takes its whole-scene snapshot under the same
mutex, so a frame that observed the list mid-rebuild would draw a scene that
never existed. An undo is one discrete user act, not a per-frame path.

### Sculpt is separate, and stays separate

Sculpting has its own history (`ARCH-OWNER-12`, below), and the separation is the
point of both: a stroke writes no Construction history, and Construction Undo
never moves a sculpt vertex, a `SculptRevision` or a stroke count. Neither
history is ever consulted for the other, and there is no combined timeline.

What a Construction restore *does* touch is the same stale-source bookkeeping an
ordinary Construction edit performs: a body whose shape changed has its Frozen
Sculpt Mesh marked as frozen from a source that has since moved. That rule now
has one implementation, `FrozenSculpt::markSourceStale()`, because a restore acts
on bodies the session is not bound to — an undo that changes body #2 while body
#1 is active must mark #2, and routing it through the session would mark the
wrong body. Adopting a changed source stays an explicit user act.

The CONSTRUCTION entry points — `constructionUndo` and `constructionRedo` — are
refused below JNI while sculpting, and that did not change when Sculpt Undo
arrived: one entry point that quietly meant either history is the ambiguity two
histories exist to avoid. What the chrome calls is a third pair, `historyUndo`
and `historyRedo`, which dispatch on the product mode in native code. The
workspace no longer withdraws the controls in Sculpt — there they mean the Sculpt
history — but the guard below JNI stays regardless: removing a control is not
removing a guard.

### The drag coalescing boundary

A gizmo drag uses the boundary exactly as designed and adds no history code of
its own. `GizmoSession::beginDrag` opens ONE edit on the pointer down; every
`updateDrag` is an ordinary `applyTransformValues` inside it, precisely as a
typed field is; `commitDrag` closes it once and records at most one step;
`cancelDrag` puts the captured state back and records nothing. A drag of one
sample and a drag of a thousand cost the history the same, and a tap costs it
nothing — because the commit compares Construction state rather than trusting
that a gesture happened.

## The Construction transform gizmo

`forgeshape_gizmo.{h,cpp}` owns direct manipulation of a body's placement:
which handle a pointer landed on, which pointer is captured, the three drag
solvers, and the transaction around one drag. It is platform-neutral C++17 — a
`CameraSnapshot`, a viewport size in pixels and a platform-neutral pointer
position cross in; a snapshot the renderer can draw crosses out.

**It owns no transform.** The authoritative `ConstructionTransform` of the
captured body moves throughout the drag, so the renderer, the picker and the
exact-value editors read the same numbers mid-drag that they read at rest.
There is no second solver, no parallel pivot and no Java-side placement.

### Three modes, two spaces, one pivot

| mode | handles | World | Local |
| --- | --- | --- | --- |
| Move | axis X/Y/Z, plane XY/XZ/YZ | yes | yes |
| Rotate | ring X/Y/Z | yes | yes |
| Scale | axis X/Y/Z, plane XY/XZ/YZ, uniform | no | yes |

`GizmoMode` and `GizmoSpace` are the closed enums; `GizmoHandle` is what a
pointer actually grabbed, separate from `GizmoAxis` because a plane handle is
constrained to two basis directions and a uniform handle to none — neither is an
axis and neither may be nameable as one.

**WORLD** is the world basis. **LOCAL** is the body's own basis: the three
columns of its rotation matrix, deliberately *scale-free*, so a stretched body
still has a local X that points one way and a handle direction never depends on
how large the body happens to be. One drag freezes its basis at the pointer
down — in Local a rotate drag is changing the very rotation the basis comes
from, and a basis re-read every sample would chase its own output.

**Scale has no World**, and that is a domain fact rather than a scope decision:
a world-axis scale of a turned body is a shear, which cannot be written as
`T · R · S` for any diagonal `S`, so it could neither be stored in the
authoritative transform nor read back by the exact-value editors. The selector is
absent in Scale rather than shown and refused, and `setSpace` refuses World
there regardless — removing a control is not removing a guard.

The pivot is always the body's authoritative Construction placement origin —
never a mesh AABB centre, a screen-space centroid or a camera-facing proxy, all
of which are derived products of the truth this gizmo writes.

### Rotation is composed as matrices and stored as Euler degrees

A ring drag does **not** add its angle to one Euler component. That is only
correct when the other two are zero, and on a mixed orientation it turns the
body about neither the world axis nor the local one. Each sample composes from
the **immutable start orientation** and
the accumulated angle:

```
World:  R_target = Relem(A, delta) * R_start     (turn, then place)
Local:  R_target = R_start * Relem(A, delta)     (place, then turn)
```

Both are "rotate about the ring the user can see": in World the ring normal is
the world axis, in Local it is `R_start · e_A`, and the conjugation identity
makes the two formulations agree with the drawn geometry.

`R_target` is then decomposed back to the authoritative Euler degrees by
`eulerFromRotationMatrix` — the ONE bridge between the two forms, in
`forgeshape_transform.{h,cpp}`. A correct rotation on a mixed orientation
legitimately moves more than one Euler field, so correctness is a statement about
the **orientation** and every test asserts against matrices rather than fields.

### Branch-continuous Euler decomposition

A ZYX triple is not unique, and two facts make the naive answer wrong for a drag:
every component is periodic, and `(x, y, z)` and `(x+180, 180−y, z+180)` name the
same orientation. So both branches are generated, every component is shifted by
whole turns into a half-turn window around the **last accepted** answer, and the
closer branch wins. That is what makes a drag continuous through ±180°, keeps it
counting past 360° and past 720° instead of wrapping, and stops all three fields
flipping at once for no motion the user made.

Near ±90° of pitch the outer two angles are genuinely inseparable. There the yaw
is held at its previous value and the whole remaining turn goes into roll:
deterministic, finite, and orientation-correct — the matrix it rebuilds is the
matrix it was given, which is the only property assertable at a singularity.

A component the drag never touched comes back **exactly** as it was.
`kEulerStickyDegrees` (1e-9°) is what makes that true: a float matrix and an
`atan2` return a few times 1e-14 rather than the zero the component started at,
and the exact-value editors would otherwise show a body rotated about world Y as
carrying −0.00000000000006 about X. A billionth of a degree is far below anything
a gesture can express and far above the residue of a matrix round trip.

### One scale for drawing and for grabbing

The gizmo is anchored in 3D and **sized in screen units**, so it stays reachable
at any zoom. `gizmoWorldScale` derives one uniform world length per reference
unit from the camera's own projection matrix and the pivot's depth — reading
`proj.m[5]` rather than a remembered field of view, so it cannot drift from what
is drawn. Both the renderer's model matrix and the hit test use that same scale,
which is what keeps what the user sees and what the finger can grab from ever
disagreeing. The **body's** scale is not part of it: stretching a body must not
stretch the instrument used to stretch it, and the snapshot carries no scale for
the renderer to reach.

Hit-testing is done in **pixels**, against the projected geometry, rather than
as ray/cylinder, ray/torus and ray/box tests: measuring in pixels is what lets
the 48-unit floor be a number the domain states rather than an aspiration. Every
target meets it — `2 * kGizmoHitSlopUnits` for a shaft or an arc,
`2 * kGizmoPlaneHitRadiusUnits` around a plane square's centre,
`2 * kGizmoUniformHitRadiusUnits` around the pivot.

**The visual size preference** (`UI-PREF-R1` E, UI-OWNER-32) rides beside the
camera-derived scale in the snapshot as `visualScale`, bounded to
`[kGizmoMinVisualScale, kGizmoMaxVisualScale]` = `[0.9, 1.5]` and refused, never
clamped, outside it (`GizmoSession::setVisualScale`). `gizmoPlacementScale`
multiplies the two, and it is the ONE scale every handle is PLACED at — the
renderer's model matrix, `gizmoHandleGrabPoint` and the hit test all read it, so
the drawing and the grabbing still cannot disagree. What it does not touch: the
hit corridors (density pixels, so the 48-unit floor holds at the smallest
size), the pivot dead disc, and every drag amount — the axis and plane solvers
are ray arithmetic, the ring solver is an angle about the pivot, and the scale
mapping's reference length is measured on a CANONICAL snapshot with the
preference set aside, so the same pixel drag stretches a body by the same
factor at every size (`uipref_the_same_pixel_drag_scales_by_the_same_factor…`).
The floor is 0.9 because a plane handle is grabbed within 24 units of its
centre and the dead disc is 24 units: from an oblique three-quarter view the
centre's projected distance is about 0.68 of its 40-unit standoff — 27 units at
1.0, 24.4 at 0.9, and 20 at 0.75, where the handle would be lost.

**The stroke weight preference** (`UI-PREF-R1` F) is a closed bundle recipe,
`GizmoStrokeWeight` in the display store: Regular is the pre-preference gizmo
vertex for vertex (`generateGizmoVertices`'s two-argument form, pinned at 1116
vertices by static_assert and by the self-test's byte comparison); Thin is the
same bundle at half the spread; Bold widens the shaft bundle to thirteen
one-pixel lines, bundles the arrowhead strokes and adds a concentric pass to
the rings, squares and cubes (2268 vertices). Hit testing never reads it. The
renderer keeps ONE buffer sized to the widest recipe and
`syncGizmoGeometry` re-uploads the canonical list only when the store's weight
differs from the one the device holds — a size change is a matrix and uploads
nothing, and `FORGESHAPE_GIZMO_UPLOAD_OK` now names the weight so any other
occurrence is still evidence of a re-upload that must not happen.

Handles resolve in **priority tiers**, smallest target first: uniform, then
planes, then axes, nearest-within-tier, with the enumeration order breaking an
exact tie deterministically. Without the tiers the three shafts, which are long,
would win every contest against the small handles between them. The drawn plane
square starts outside the axis corridor (`kGizmoPlaneInnerUnits >
kGizmoHitSlopUnits`), so priority is a tie-break rather than a way of hiding an
overlap.

A touch inside `kGizmoPivotDeadRadiusUnits` of the projected pivot names **no
handle at all** in Move and Rotate: every shaft converges there and every ring
passes around it. Excluding the inner span alone was not enough — the corridor
*around* that span reaches back to the pivot, and foreshortening pulls it further
in. In Scale the same disc IS the uniform handle, so the exclusion is per-mode
rather than a property of the pivot.

The three rings genuinely intersect — the X and Z rings both pass through +Y,
and so on — so `gizmoRingGrabOffset` is the one definition of a point that names
a single ring. `gizmoHandleGrabPoint` is the one definition of where ANY handle
is grabbed, and drawing, hit-testing, the scale reference direction, the JNI
diagnostic and verification all read it.

### Four kinds of handle, four kinds of mark

The instrument is one line list, uploaded once, and the whole of its legibility
is which lines it holds. Every mark used to be a hollow outline in an axis hue,
so axis, plane, pivot and uniform-scale were told apart by position alone and the
whole thing read as stray selection wireframe over the body's own faces. UI-LAYOUT-R1
changed the marks and **nothing else**: no hit radius, no handle set, no grab
point, no solver and no space rule moved, and `UILR1-14` asserts that.

- **Axis** — a bundle of five parallel lines a stroke offset apart. A single
  hairline is what the `wideLines` device feature would be needed to thicken, and
  the product does not request it to draw a handle.
- **Arrowhead** (Move) — four spokes back from the tip plus **two lines closing
  across them**. The cross is what separates a head from two more hairlines
  leaving the tip.
- **Cube** (Scale) — the professional vocabulary for "this stretches", which is
  what makes Move and Scale readable apart at a glance rather than by remembering
  which is selected.
- **Plane** — a doubled square **with its two diagonals**. A crossed square reads
  as a surface; a bare one read as one more outline in an axis hue.
- **Pivot** — three NEUTRAL arms through the origin, drawn once rather than as
  part of each shaft, and inside `kGizmoPivotDeadRadiusUnits`, so the one mark
  that is not a control is drawn as one and sits inside the disc that grabs
  nothing. Rotate draws it too now: three rings around a point with nothing at
  the point does not say where the rotation is centred.
- **Uniform** (Scale) — the largest mark on the instrument, a doubled cube at the
  pivot in the neutral level. It acts on all three axes and stands where every
  shaft converges. Scale draws **no** pivot mark, because this is what stands
  there: a reference mark and a control sharing one point is what made the
  control unreadable.

A **held** handle is stated twice over — the highlight hue no axis owns, and
every other handle dropping to `kGizmoIdleAxisAlphaScale` — so a reader who can
separate neither hue still has the weight and the shape. The gizmo self-test
checks all of it as arithmetic over the generated buffer.

### The Move solvers, and their degeneracy

**Axis.** Closest approach between the pick ray and the infinite basis line. The
denominator is `1 - cos²` of the angle between them and vanishes exactly when
they are parallel; below `kGizmoAxisParallelDenominator` (about 8°) the solver
falls back to the intersection with the camera-facing plane **through** the
axis, projected back onto the axis — the constraint is still the axis, only the
surface read against has changed. When that plane is edge-on too, the result is
`Unresolvable`: the drag writes nothing and holds its last good value. Guessing
there is what produces the jump the status exists to prevent.

**Plane.** The pointer ray meets the plane through the pivot whose normal is the
third basis direction, and the displacement from the down-point is **projected
onto the two allowed directions**. The third component is therefore untouched by
construction rather than by a subsequent correction, which is what makes "a plane
drag never leaves its plane" a property of the arithmetic. An edge-on plane is
refused and the drag holds, exactly as an edge-on ring does.

Both apply from the START values rather than incrementally, so a long drag cannot
accumulate the solver's own rounding, and the world displacement is accumulated
in `double` so a world-axis drag writes the same exact number a typed Apply
would.

### The Rotate solver, and its degeneracy

The pointer ray meets the ring plane; the signed angle from the drag's first
spoke is `atan2(axis · (a × b), a · b)`, which keeps its sign and its precision
near 0 and π where an `acos` has neither. Consecutive samples are far less than
half a turn apart, so `unwrapAngleDelta` turns each raw difference into the real
motion: crossing ±180° is not a jump, and because the accumulator is never
itself reduced, a drag past a full turn keeps going — 350 + 30 reads back as
380, matching the exact-transform convention that keeps what the user did. An
edge-on ring is refused by `intersectRayPlane`, and refusing IS the documented
fallback: the body stops rather than spinning on noise, and resumes the moment
the plane has something to intersect again.

### The Scale solver

One formula for all three scale handles, so there is one thing to describe and
one thing to test:

```
factor = 1 + (pointer - down) · dir / referencePixels
```

`dir` is the handle's own **screen** direction and `referencePixels` is how long
that handle is on screen: the projected shaft for an axis, the projected in-plane
diagonal for a plane, and the screen diagonal (right and up) at one shaft length
for the uniform handle, which has no direction of its own. Screen space is the
right space for this because a scale factor is a screen-space question — there is
no world quantity a pointer offset could be intersected against.

Measured from the **pointer down point** rather than from the pivot, so the
factor is exactly 1 at zero drag wherever on the handle the user grabbed. It is
monotone in the pointer displacement, finite, and clamped at
`kGizmoMinScaleFactor`, so dragging past the pivot pins the body at a sliver
rather than passing through zero into a mirror. A handle that projects shorter
than `kGizmoScaleMinReferenceUnits` has no usable screen direction and the
capture is **refused** rather than anchored on rounding error.

An axis handle writes one component, a plane handle writes its two by the same
factor, and the uniform handle writes all three — which preserves an existing
non-uniform body's proportions exactly.

### Input arbitration

Decided once, on the pointer down, in `forgeshape_jni.cpp` beside the sculpt
arbitration and mutually exclusive with it by product mode. A pointer that goes
down **on a handle** belongs to the gizmo for the whole of its life and is
swallowed — neither the camera nor the selection sees it. A pointer that goes
down anywhere else navigates and picks exactly as it always has: orbit, pan,
pinch and tap are untouched away from the handles.

The gizmo follows ONE pointer by stable id, never by index. A second finger does
not steer it and does not get to orbit around it either: it **cancels** the
drag, restoring the placement to where the first finger found it and recording
nothing. `ACTION_CANCEL` does the same. A drag can only move the body it started
on, whatever the selection does underneath it.

### Cost

A transform-only drag publishes no mesh revision, uploads nothing and
regenerates no primitive, because `applyTransformValues` cannot reach a
`MeshStore` — the same structural guarantee a typed Apply already had. The
renderer's gizmo geometry is a compile-time constant uploaded once with the
device (`FORGESHAPE_GIZMO_UPLOAD_OK`, logged once per device); where the gizmo
is, which way it points and how large it looks are a push-constant matrix per
frame. Switching Move to Rotate to Scale changes two integers on one
`vkCmdDraw`, and switching World to Local changes the rotation in that matrix.

The gizmo push block is also exactly 128 bytes — the guaranteed minimum — which
leaves four `vec4`s after the matrix for three axis colours plus a highlight, a
neutral grey, the held-handle code and the idle alpha. The three extra scalars
ride in the axis colours' otherwise-wasted `w` components, the same technique the
surface block uses for its shading model; the packing is documented once in
`shaders/gizmo.vert` and written once in `GizmoPush`. Each vertex carries a
colour tag AND a handle code, separately, because a plane handle borrows the hue
of the axis perpendicular to it and holding that axis must not light the plane
too.

The gizmo pass is last in the frame, on its own pipeline with no descriptor set,
with depth test AND depth write off. Off so a handle stays reachable where it
passes through the body it moves; no write so the depth buffer is left exactly
as the bodies and the grid left it.

### The quantization and placement seam

One pipeline, stated once:

```
raw pointer -> coordinate constraint (the solvers) -> placement / snap modifier -> authoritative apply
```

`quantizeGizmoTranslation`, `quantizeGizmoRotation` and `quantizeGizmoScale` are
the per-component half; `applyGizmoPlacementModifier` is the whole-placement
half, and it takes the solved target, the pre-drag placement, the mode, the
handle and the space, so an approved snap can consult all five without any solver
learning about it. `GizmoSession::applyTarget` is the only path from a solved
target to the transform, so nothing can bypass the seam.

All four are the identity today and the self-tests assert that they are. Neither
Surface Snap nor Grid Snap has an approved contract — no translational increment,
no relation to the display unit, no rotational increment, and no definition of
what a surface even means for a body that has not been picked — so there is no
setting, no indicator and no hidden snapping. The seam exists so an approved
contract lands in these functions instead of in the gesture architecture.

## Construction transform

`forgeshape_transform.{h,cpp}` owns **where** the object is; the primitive
parameters own **what** it is. They are separate truths on purpose, and neither
reads the other. Both are held by the same `ConstructionObject`, so they cannot
become inconsistent about which object they describe; the two branches of the
source-of-truth hierarchy above are exactly these two.

The published `RuntimeMesh` is the object's geometry in **object space**. The
transform never touches it. That is the whole point: moving, rotating **or
scaling** the object changes derived 4×4 matrices and nothing else — no vertex is
rewritten, no `MeshRevision` is published and no GPU upload happens.
`ConstructionTransform` could not publish one if it wanted to; it has no access
to `MeshStore`. The independence runs both ways: a shape change republishes the
mesh and leaves the placement exactly as it was, including across any switch
among the six kinds.

**Unit contract.** Position is `double` **meters**, rotation is `double`
**degrees**, scale is a `double` **unitless multiplier**, named `...Meters`,
`...Degrees` and `...Factor` at every boundary. Degrees are authoritative because
degrees are what the product exposes; radians exist only inside the derived
trigonometry. mm/cm/m applies to position exactly as it applies to a dimension,
and never to rotation or to scale.

**Scale is not a dimension.** A 2 m box at scale 2 is drawn 4 m across and its
Construction parameter is still 2 m. That separation is the whole reason a scale
may live on the transform at all: it moves a derived matrix and never a primitive
parameter, so the Construction Source is untouched by it and the exact-value
surfaces stay two clearly different questions — what the body IS, and where and
how large it is drawn.

### Axis and Euler convention

Right-handed world space, **+Y up**, column-vector math (`p' = M * p`), storage
column-major. A positive angle rotates by the **right-hand rule**. Rotations
compose in **local X → Y → Z** order, and the scale sits **inside** all of them,
which for column vectors is written right to left:

```
Model      = T * Rz * Ry * Rx * S
Model^-1   = S^-1 * Rx(-x) * Ry(-y) * Rz(-z) * T(-p)
NormalM    = R * S^-1
```

so `S` acts on the object first, then `Rx`, and the translation last — a rotated
object at (2, 0, 0) is still centred at (2, 0, 0), because the rotation does not
turn its own translation, and a scaled one grows about its own origin rather than
sliding away from it. Scale is therefore **local / object space**: it stretches
the body along the body's own axes, which is the only meaning that survives a
rotation without inventing a shear, and is why the product offers no world-space
scale.

`NormalM` is the third derived matrix, and it is a real inverse transpose:
`(R·S)^-T = R^-T·S^-T = R·S^-1` for an orthonormal `R` and a diagonal positive
`S`. It equals `R` exactly when the scale is (1,1,1), which is why nothing about
an unscaled body changes. The renderer composes `view · NormalM` for the shader
and never `view · Model` — the two agree for every unscaled body and part company
the moment a body is stretched, where the model would tilt a normal off the
surface it belongs to.

There is exactly one convention in ForgeShape. The renderer consumes
`modelMatrix()` and `normalMatrix()`, the picker consumes
`inverseModelMatrix()`, the gizmo's Local basis consumes `rotationMatrix()`, and
the GLB exporter consumes `localMatrix()` — `R · S`, the model matrix with `T`
factored off, which exists so a static export can bake the linear half and leave
the pivot on the node. All five are composed here, from the same authoritative
values in the same order, so `modelMatrix() == T(position) · localMatrix()`
holds by construction rather than by coincidence and none of them builds a
rotation itself, so they cannot drift apart. The inverse is
composed from the authoritative values — each factor inverted, the order
reversed, the scale by reciprocal — rather than inverted numerically, so it
cannot disagree with the model it undoes. The self-tests assert the convention
directly: `Rx(+90)` takes +Y to +Z, `Ry(+90)` takes +Z to +X, `Rz(+90)` takes +X
to +Y, `Model * Model⁻¹` is the identity both ways under scale, and a carried
normal stays perpendicular to a carried tangent.

### Rotation values are not canonicalized

370° stays 370° in domain state and −90° stays −90°. Reduction modulo 360 happens
only inside the derived trigonometry, so what the user typed is what comes back
out and re-applying the same numbers reliably reports `Unchanged`. The self-tests
assert both halves: the stored value is untouched, and 370° and 10° produce the
same derived matrix.

### Validation

A position or rotation value is refused only when it is not finite, or when it
could not survive into the derived `float` matrix. Zero and negative are
**ordinary** for all six: a coordinate is a place and an angle is a direction,
neither is a size, and there is no arbitrary product limit.

A **scale** carries that rule and one more: it must stay strictly above
`kMinScaleFactor`. Zero would make the model matrix singular — no inverse, no
local ray for picking, a divide by zero in the normal matrix — and negative would
be a **Mirror**, which this product does not have and which would silently invert
winding order and make every normal and every front-face test wrong. The floor is
a REFUSAL threshold and not a rounding rule: a request at or below it leaves
every previous value standing rather than being clamped into a size the user did
not ask for. It surfaces as `NotPositive`, the same code a non-positive dimension
uses, because it means the same thing to a reader.

`applyBoxTransform` (`applyTransformValues` / `applyConstructionTransform`) is
the one entry point. It fails closed: **all nine** are validated before any is
written, so a bad scale cannot leave a half-applied position behind. It reports
`Applied`, `Unchanged` or `Rejected` exactly as the dimension path does.

### Picking a transformed object

The ray moves, not the mesh. `transformRayToLocal` carries the world ray into
object space with the inverse model, and the unchanged local `RuntimeMesh` is
intersected there — the same vertices the GPU already holds. The hit point is
carried back to world space for reporting.

The distance needs no conversion, and that stays true under a **non-uniform
scale** precisely because the local direction is never renormalized:

```
localOrigin + t * localDirection = M^-1 * (worldOrigin + t * worldDirection)
```

holds for any invertible `M`, so the `t` a local intersection reports is exactly
the parameter along the world ray it came from — and since the world ray is unit
length, that parameter is world meters. Renormalizing is what would break it: on
a stretched body the local direction is genuinely not unit length, and rescaling
it would rescale every hit distance by a factor that varies with the direction
the ray happens to point.

Scale is strictly positive, so the transform still has no reflection: winding is
unchanged and front-face-only picking means the same thing in either space. That
is what keeps "what is drawn" and "what is pickable" identical under a transform,
with no second collision representation to keep in sync.

## CAD domain (`CAD-R0-A1A2`)

A **CAD Body** is a body's third representation: a bounded, ordered FEATURE
CHAIN whose first feature is one sketch on a principal workplane and one linear
**New Body** extrusion of the regions it selects, and whose later features are
Add and Cut extrusions of retained sketches standing on the body's own planar
faces (`CAD-VERTICAL-SLICE-R1`). Like the other two domains it is
platform-neutral C++ with no JNI, Android, Vulkan, renderer or UI type, and it
holds no GPU resource.

### Why a third representation

A Construction Body is a primitive plus its parameters; an Imported Mesh IS its
geometry. A CAD Body is neither. It has no `PrimitiveKind` and no six
remembered parameter sets, so it cannot be a Construction Source without
inventing a primitive it was never made from — the rule an Imported Mesh already
refuses to break — and its geometry is DERIVED, so storing it as an Imported Mesh
would throw away the very thing that makes it editable. So `BodyRepresentation`
has three values, a `SceneObject` owns exactly one of them for its whole life,
`cadOrNull()` is a pointer for the same reason `constructionOrNull()` is one, and
the `.forge` document carries it in its own required section (`CADB`) on
`IMPT`'s terms.

### Dependency direction

```
forgeshape_workplane      (u, v) <-> body-local 3D; three fixed right-handed frames
        ^
forgeshape_sketch         entities, ids, validation, closed loops, triangulation
        ^
forgeshape_sketch_region  loops -> regions (outer minus direct children), the
        ^                 semantic selection, hatch, tap resolution
forgeshape_cad_kernel     THE boolean seam: CadSolid (binary64, one face tag per
        ^                 triangle) -> Union / Difference; the only TU that
        |                 includes vendored Manifold (third_party/manifold)
forgeshape_cad_feature    one feature's geometry: support frame, faces, prism,
        ^                 lineage signature; builds the chain's geometry
forgeshape_cad_body       CadBodyState = base (sketch + ExtrudeFeature) +
        ^                 laterFeatures[]; regenerateCadBody / generateCadMesh
forgeshape_cad_face       semantic faces of any feature, from the regenerated
        ^                 mesh's face table; resolve, signature, carries-face
        ^
forgeshape_scene          SceneObject owns a CadBody; addCadBody; publishSceneObject
        ^                                                    ^
forgeshape_history        a step copies CadBodyState;         |
                          rebuilds a CAD body it never held    |
forgeshape_project_*      CADB record <-> CadBodyState; regenerate on load
forgeshape_gltf_export    re-evaluates generateCadMesh, never a buffer
        ^
forgeshape_sketch_session the volatile edit session; touch -> entities; overlay
        ^                 owns ONE CadExtrudeManipulator and is the ONE writer
        |                 of the profile, the depth and the direction
forgeshape_cad_extrude_tool  anchors (no camera), the screen-scale rule,
                             the axial drag, the arrow's line list, and the
                             feature-preview view policy
        ^
forgeshape_jni            the acts, the touch arbitration, the borrowed views
```

`forgeshape_cad_extrude_tool` sits BELOW the session in this chain and is
included by it, not the other way round: the manipulator is a part the session
owns. It names `SketchFrame` by forward declaration and dereferences it only in
its implementation, which is what keeps the include acyclic.

**Two views, one authored truth (`CAD-UX-S1-C1`).** A sketch is AUTHORED
through the exact support-normal view, and the support normal IS the extrusion
axis, so the arrow drawn in it has no screen extent and no axial drag can be
resolved from it. `cadFeatureViewPose` is the whole policy and is a pure
function over values — a current pose, an optional prior pose, the sketch frame
and the anchors in, one `CameraController::Pose` and a `CadFeatureViewSource`
out — with no scene, no session, no renderer and no Android type in it. Two
paths: `PriorView` gives back the user's own pre-sketch view, re-centred on the
work anchor, when `cadFeatureViewAxisSine` already clears
`kCadFeatureViewMinAxisSine`; otherwise `ObliqueFallback` leans
`kCadFeatureViewObliqueRadians` off the normal in the frame's own `(u, v)`, so
no world up enters its construction. Because the orbit pose clamps pitch, the
candidate is re-derived from the CLAMPED angles and re-measured, and a failure
steps the azimuth a quarter turn for a bounded
`kCadFeatureViewAzimuthAttempts`; a degenerate frame or anchors are
`Unavailable` and install nothing. `forgeshape_jni`'s `beginExtrudeFeatureView`
is the adapter — it reads the camera, hands the values over and installs the
answer through `restorePose`, inside the same lock as the `finish()` that
reached `Ready` — and it deliberately does NOT consume `g_sketchSavedPose`, so
`endSketchView` still returns the pre-sketch view on cancel and on commit.
`sketchBackToEditing` and `sketchBeginEdit` both reinstall the exact aligned
view through `beginSketchView`. In `Ready` the JNI arbitration hands an
unclaimed single pointer to the camera instead of swallowing it, which is the
one gesture rule the transition changes and the one state in which there is no
aligned view to protect.

**The extrusion's EXTENT is two distances (`CAD-EXT-R1`).** `ExtrudeFeature`
gained an `ExtrudeExtentMode` and a `secondDistance`, and everything downstream
reads the pair `extrudePositiveDistance` / `extrudeNegativeDistance` rather than
the mode — so `generateCadMesh` spans `-B .. +A` in one line for all three
modes, the overlay preview reads the same two numbers, and a fourth mode could
not silently mean a fourth geometry rule. A mode is not a second model of the
extrusion: it names which combinations the controls author, and each mode has
ONE canonical form (a direction only in One Side, a second distance only in Two
Sides) which `validateCadBodyState` refuses by name rather than repairing, so
one solid has exactly one encoding in a file, a history step and a live edit
alike. `extrudeFeatureWithExtent` is the whole transition policy as a pure
function over values — no camera, no averaging, no heuristic — taking the
user's last One Side choice as an argument rather than reading a hidden field;
`SketchSession` holds that choice as volatile intent (`oneSideDirection_`),
never persisted and never a second answer to which side the solid is on.
`SketchSession::applyExtrudeFeature` is the ONE writer every typed value, drag
sample, Flip and mode change lands through. `CadExtrudeAnchors` grew a
`CadExtrudeSideAnchor` per side and `CadExtrudeManipulator` freezes WHICH side
at pointer-down with the basis, so a Symmetric extrusion growing under the
finger cannot move the gesture to the other arrow. `CapPlane` is now the cap
the extrusion grows FROM and `CapFar` the one it grows TO — for One Side still
literally the cap on the plane — so the face TOKENS and the §7c lineage
signature are unchanged and a face-supported dependent stays attached across an
extent edit while its derived placement follows the cap that moved. `CADB`
gains section version 4 for the extent; a project whose every extrusion is One
Side stays byte-identical at v1, v2 or v3.

Nothing above a line reads truth from below it through a mesh. The renderer
sees a CAD Body exactly as it sees a primitive — a published `RuntimeMesh` —
and sees a sketch only as a world-space line list it cannot read a coordinate
back out of.

### Workplane mapping

`forgeshape_workplane.h` states the three frames once. Each is right-handed
(`U × V = N`), so a profile that is counter-clockwise in `(u, v)` is
counter-clockwise seen from `+N`, which is what lets the extrusion produce
canonical outward winding on any plane without asking which:

| Plane | U | V | N | The view it is read from |
| --- | --- | --- | --- | --- |
| XY | +X | +Y | +Z | the front view, from +Z |
| XZ | +X | −Z | +Y | the top view, from +Y |
| YZ | −Z | +Y | +X | the side view, from +X |

XZ's V and YZ's U run along −Z so that, seen from the plane's positive normal
with world +Y up, U runs to the RIGHT and V runs UP — the same reading the
front view has. The mapping is written per plane in double and rounded once,
so an exact sketch coordinate stays as exact as a float can hold it. The sketch
origin is the body's local origin; nothing is recentred, and a CAD Body created
from a sketch starts at the identity placement.

### Sketch truth, and what is derived

TRUTH is the entity list: a line's two endpoints, a polyline's vertices and
whether it is closed, a rectangle's centre and its two sizes (axis-aligned in
sketch space, PARAMETRIC on purpose — "make it 40 mm wide" is an edit to a
rectangle, and four unrelated lines have no width to edit), a circle's centre
and radius. Each entity carries a per-sketch `SketchEntityId`: minted by the
sketch, monotonic, never reused, never an `ObjectId`. The sketch stores its
allocator's high-water mark so a reopened sketch cannot mint a collision.

DERIVED is everything `forgeshape_sketch` computes from that and never stores:
which entities close a profile, the polygon a profile becomes, the triangles.
Bounds are explicit — 256 entities, 256 polyline vertices, a 1e5 m coordinate
range, a 1 µm coincidence tolerance — and every size computation downstream is
provably finite because of them; they are also what makes a history step of a
CAD Body BOUNDED.

### Semantic faces and the dependency graph (`CAD-A3`, `ARCH-OWNER-13`)

`forgeshape_cad_face.{h,cpp}` turns a `CadBodyState` into a bounded set of planar
faces — two caps and one side per profile edge — each with a stable
`CadFaceToken` (its kind plus the profile-edge entity it came from) and a
deterministic right-handed frame in body-local space. `cadFaceRanges` maps the
generated mesh's triangle ranges to those tokens for picking, and is NEVER
persisted; `cadTopologySignature` hashes the token set for lineage. A sketch
supported by a face stores a `TopoRef` (producer `ObjectId`, feature id, token,
lineage) on its `CadSketch`; the child authors on its own canonical local XY and
`cadFaceFrameMatrix` places that space onto the producer's face.

A face-supported body has NO independent placement. `ConstructionScene::
resolveWorldModel` composes `producerWorldModel · faceFrame` every snapshot,
bounded and cycle-guarded, so the renderer and picker (which already consume
arbitrary snapshot matrices) carry the dependent with the producer and nothing
is stored to go stale. `SceneObject::isFaceSupportedCad` marks such a body so the
gizmo and the exact-value editors leave it alone; `hasCadDependents` /
`cadDependentsOf` back Delete's `RefusedHasDependents`. A stale lineage, a
missing producer, a bad token, a curved circle side or a cycle all fail closed.
`forgeshape_support_chooser.{h,cpp}` resolves a viewport tap to a world plane or
a face (through the ordinary scene pick), and the sketch camera frames exactly
along the sketch frame's normal via `CameraController::frameSketchView` (no
pitch clamp). `CADB` gains section version 2 for the support; v1 stays
byte-identical, and load validates the whole dependency graph before applying.

### Curves, the view, the dimension and Edit Sketch (`SKETCH-UX-R1`)

**Curves are authored points.** `SketchArc` is three points ON the curve and
`SketchSpline` is the point list its curve interpolates; `arcGeometry` and
`tessellateSketchCurve` derive the centre, radius, sweep, handles and every
tessellated point from those alone, bounded and deterministically, with no
camera or zoom anywhere in the derivation. `chainCurves` — the generalization of
what was `chainLines` — chains Lines, Arcs and Splines through ONE walker on
coincident AUTHORED endpoints, so connectivity and triangulation stay separate
and a denser tessellation can never open or close a profile. `ClosedProfile`
gains a per-edge `edgeCurved`, and `forgeshape_cad_face.cpp` reads it so a
curve's side face is ineligible while a line's in the same profile is not.

**The view is not the plane.** `SketchSession` keeps `viewFlipped_` and
`viewQuarterTurns_` and derives `viewFrame()` from the authoring `frame_`;
`beginSketchView` frames the camera on the derived one. Nothing about either
reaches the sketch, the history, the codec or the fingerprint, and `viewFrame`
is always right-handed so no view state can mirror a sketch. `setSupportPlane`
re-bases the authoring frame only while the sketch is empty.

**The dimension is an interaction overlay.** `SketchOverlayStyle::Dimension` is
its own range in the same line list the grid and the entities use;
`selectedLineDimensionAnchor` gives the shell a sketch point to place the
editable label at. `applyLineLength` is the exact edit: P0 fixed, direction
preserved, everything else untouched.

**Edit Sketch is staged.** `beginEdit` copies a committed body's
`CadBodyState` into the session and records `editingBodyId_` — the ONE answer
to whether a session is an edit, which is also what makes one Extrude control
serve both. `commitEdit` validates the candidate, checks every dependent's
reference against it (`DependentFaceLost` when one would be stripped), and then
applies inside ONE `ScopedConstructionEdit`. `CADB` gains section version 3 for
the curve kinds, written only when one is present and always carrying the v2
support block; v1 and v2 stay byte-identical.

### Profile extraction

`extractClosedProfiles` reads every closed profile out of a valid sketch. A
rectangle and a circle each close one by construction (a circle tessellates to
`kSketchCircleSegments` = 32 vertices — the same count every round primitive
uses — with exact cardinal points). A polyline closes one when it is flagged
closed or its last vertex coincides with its first. Lines are CHAINED by
coincident endpoints: a connected component in which every endpoint meets
exactly one other line is one loop, a free end is `OpenProfile`, a fork is
`BranchingChain`. Every loop is then held to the same rules, in this order:
at least three vertices, no duplicate consecutive edge, no crossing between
non-adjacent edges (checked BEFORE area, because a bow tie's lobes cancel to
zero area and "it crosses itself" is the reason the user can act on), non-zero
area, and orientation normalised to counter-clockwise.

A loop is identified by its **anchor entity id** — the rectangle, the circle
or the polyline itself, or the smallest id among a chain of lines — never by an
index, which would move when an unrelated entity was deleted. Loops are
reported ascending by anchor, which is the deterministic order every caller
sees. Loops no longer refuse each other for nesting (`NestedProfileUnsupported`
stays in the enum for ABI stability and is no longer produced by extraction):
what a loop inside another MEANS is the region layer's answer, below. Two loops
that merely overlap are both kept. Every refusal is a named `CadStatus`, and
nothing is repaired.

### Regions (`CAD-VERTICAL-SLICE-R1`)

`extractSketchRegions` (`forgeshape_sketch_region`) turns every loop into
exactly one region: its interior minus its DIRECT children, where B is L's
child when L is the smallest loop that cleanly contains B (strictly inside and
no edge touching or crossing). A rectangle around a circle is the disk and the
rectangle-with-a-hole; deeper nesting is even/odd from the same sentence. It is
nesting, not a planar arrangement, so a touching or crossing loop is never a
hole and stays its own region exactly as v1..v4 read it — which is what keeps
every earlier `CADB` record meaning what it meant. An `ExtrudeFeature` selects
regions by `ProfileRegionRef` (outer anchor plus the hole anchors it was chosen
with): the first region's outer anchor stays in `profileEntityId`, its holes in
`profileHoleIds`, and further disjoint regions in `additionalRegions`, so a
one-region, no-hole selection is the R0 field set, bit for bit.
`validateRegionSelection` refuses a mismatch against the derived holes, overlap
or a shared loop, and the caps, by name; `sketchRegionAt` resolves a tap to the
innermost region under it; `sketchRegionHatch` is the bounded even/odd hatch
that leaves a hole empty.

### The feature chain and the boolean kernel (`CAD-VERTICAL-SLICE-R1`)

`CadBodyState` is the base (`sketch` + `extrude`, feature id 1, always New
Body) plus `laterFeatures`, each `{featureId, operation (Add|Cut), support,
sketch, extrude}` with `featureId` strictly ascending and at most
`kMaxCadFeatures` (16) in all. A later feature's `support` names a planar face
of an EARLIER feature of the same body by `(featureId, CadFaceToken,
lineageToken)` — deliberately not a `TopoRef`, which places ANOTHER body in the
world; this places one feature's sketch inside its own body. Its sketch is
canonical XY with no support block, and its placement is the named face's frame
from `buildCadChainGeometry`, computed from the earlier feature's OWN prism in
binary64 and never from a boolean result.

`regenerateCadBody` is THE regeneration path, with three branches chosen by the
state alone: a base-only, single-simple-region body takes the unchanged R0
float path (`generateSimpleProfileMesh`), so every pre-existing body's mesh is
bit-identical; a base-only body with holes or several regions uses the
binary64 prism generator (`appendCadFeatureSolid`, holes through the kernel's
region triangulator) with no boolean; and a body with later features builds the
base solid and applies each later feature in order through ONE
`cadKernelBoolean` (`Union` for Add, `Difference` for Cut), checking each result
against the operation rules — a new shell is `AddDisjoint`, a gain below 1e-9 of
the tool is `AddNoEffect`, an empty result is `CutRemovesBody`, a loss below
1e-9 of the tool is `CutNoIntersection`, a support face the solid built so far
no longer carries is `SupportFaceLost` — and naming the failing `featureId` in
`CadRegenerationReport`. Every input triangle carries a face tag indexing a
`CadMeshFace {featureId, token, eligible}` table; the kernel carries tags
through the boolean and canonicalises its output (triangles by ascending tag,
vertices renumbered in first use), so the published `CadBodyMesh` pairs the
mesh with a per-triangle face table and the result is a pure function of the
chain. `forgeshape_cad_face`, the support chooser, the dependents check and
`cadMeshCarriesFace` all read faces through that table; a triangle index is
never identity. `CadBody` caches its last regeneration (`regenerated()`), reset
by every state change.

`forgeshape_cad_kernel.{h,cpp}` is the seam. Its types (`CadSolid`,
`CadKernelStatus`, `CadBooleanOp`, `CadSolidMeasure`) are ForgeShape's own; the
`.cpp` is the ONLY file that includes a Manifold header, so replacing the kernel
is a one-file change. Inputs must be finite, closed, 2-manifold and positively
oriented — an inward-wound solid is `InvertedInput`, because the kernel would
silently subtract it — and the result is bounded by `kMaxCadKernelTriangles`.
Manifold 3.5.4 is vendored unmodified (`third_party/manifold`, Apache-2.0, see
its `FORGESHAPE_VENDOR.md`) and built as the static `forgeshape_manifold`
target: single-threaded (`MANIFOLD_PAR=-1`), no iostream or filesystem API, no
`MANIFOLD_DEBUG`, `-O2` in every variant. Nothing is fetched at build or run
time. Why this kernel and the evidence it had to pass are in
`artifacts/cad-vertical-slice-r1/KERNEL_GATE.md`.

### Operations, staging and the preview (`CAD-VERTICAL-SLICE-R1`)

The support chooser stages a face sketch's producer chain into the session
(`beginOnFace(frame, TopoRef, producerState)`), which is what makes Add and Cut
available there; a world-plane sketch has no target and offers New Body alone.
`SketchSession::candidateState` builds the ONE candidate: a new body, the
target's chain with a feature appended, or a staged edit of one feature
(`beginEditFeature`). `evaluateCandidate` regenerates it latest-only, keyed by
`candidateRevision_`, which every authoring act bumps (`touchCandidate`); the
JNI render loop (`applyCadOperationPreviewLocked`) substitutes that evaluation
for the target's draw item — same key, a revision from a space no published
revision reaches — or adds one item for a New Body, and sets
`SceneDrawItem::previewTint`, which `recordBodyDraw` feeds through the existing
selection-tint push slot. `commitIntoTarget` applies that same evaluation as
ONE `ScopedConstructionEdit` around ONE `applyState` on the same `SceneObject`;
`commitEdit` does the same for a staged feature edit after the dependents check
(`checkDependentsKeepTheirFaces`: signature, resolution, eligibility and
`cadMeshCarriesFace` on the new mesh). Nothing in the preview is truth: no
published revision, history step, fingerprint or autosave moves for it.

### Triangulation

`triangulateSimplePolygon` is ear clipping over a counter-clockwise simple
polygon: deterministic (the same polygon yields the same triangles in the same
order), bounded (at most n − 2 clips, each a search over at most n candidates,
n ≤ 256), and fail-closed (a polygon it cannot finish is `TriangulationFailed`
with nothing written). A vertex on an ear's boundary counts as inside, so a
collinear vertex is never cut across. Clockwise input is refused rather than
reversed, because orientation was decided upstream once.

### The extrusion

For a base-only, single-simple-region body (every body before
`CAD-VERTICAL-SLICE-R1`), `generateCadMesh` validates the whole state, extracts,
finds the chosen profile, triangulates and extrudes exactly as follows; the
other two branches are in "The feature chain and the boolean kernel" above. The solid always spans from its
−N face to its +N face; which of the two sits ON the sketch plane is the
direction, so the winding rule never asks which way the user chose. The mesh
shares the 2n profile vertices between the two caps and the n side quads, so
every edge lies on exactly two triangles — watertight by construction — and
hard edges are the render layer's business, exactly as they are for a box. The
+N cap keeps the profile's counter-clockwise order (counter-clockwise seen from
outside), the −N cap is the same triangles reversed, and each side quad
`(lower_i, lower_i+1, upper_i+1)` has normal `edge × N`, which for a
counter-clockwise profile is outward. The self-test proves it by signed volume,
which unlike a centroid heuristic is exact for a concave solid.

### Regeneration is atomic

`CadBody::applyState` regenerates the WHOLE requested state once into scratch
and writes nothing unless it passes. An edit that would leave the body with no
closed profile — or a profile the extrusion no longer names, or a depth that is
not a length — is refused by name and the last valid state stands. An identical
request reports Ok, changes nothing and counts nothing. The typed editors
(`applyCadRectangle`, `applyCadCircle`, `applyCadExtrude`) build one candidate
from the current state and apply it once, so a rectangle's centre and every
entity the user did not type survive untouched. A JNI Apply is ONE
`ScopedConstructionEdit` around that one call, so one user Apply is one history
step and no step when refused or identical.

### The history

A `BodyConstructionState` carries a `CadBodyState` for a CAD Body, compared
with `sameCadBodyState` (bit-exact), and `applyState` restores it through
`CadBody::restoreState` and republishes. Because a CAD Body IS derivable from
its state, a step can REBUILD one it never held (`makeCadBody`) — unlike an
Imported Mesh, which can only come back as the object that was taken out. The
placement is the body's and is restored beside it, so a CAD edit never moves a
placement and a gizmo drag never moves a sketch.

### The sketch session

`SketchSession` is the bounded, VOLATILE state between New Sketch and the one
commit. `Inactive → Editing → Ready → Inactive`, with `cancel` from anywhere
and `backToEditing` from Ready. Nothing before `commit` is project truth: the
scene, the history, the fingerprint and the codec know nothing of a half-drawn
line, so `cancel` costs the project nothing and process death loses the sketch
and nothing else. `commit` is one `ScopedConstructionEdit` around one
`addCadBody` (New Body) or one `applyState` on the target (Add, Cut, or a staged
feature edit); a refusal mints no `ObjectId` and stays in Ready. In Ready a
single pointer that does not travel past the tap slop toggles the region under
it on Up; one that does is the arrow drag or the camera, as before.

Pointer samples arrive as the same `TouchPointer` the camera and the gizmo
consume. The session owns ONE pointer by id; a second pointer cancels the
entity in progress and hands the gesture to the camera, so pan and pinch stay
available and a stroke can never become a pan half way through. Pixels become
sketch coordinates ONCE, at the moment they are used, by intersecting the
camera's pick ray with the workplane; the pixel itself is never stored. A
rectangle is dragged corner to corner, a circle centre to radius, a line end to
end; a polyline is placed tap by tap, closed by tapping its first vertex and
ended open by tapping its last, and a tool change ends it open. Select is a
tool, so a tap has exactly one meaning at a time.

Snapping: an endpoint snap (existing endpoints, polyline vertices, rectangle
corners and centres, circle centres, the polyline being placed) wins over a
grid snap, both are always on, and both produce EXACT coordinates — the other
entity's own stored value or an exact multiple of the 0.25 m grid — never a
rounded pixel. A typed value is never snapped. Tolerances are in reference
units (24 dp either side, the 48 dp floor as a diameter), resolved through the
same camera-derived world-per-unit the gizmo uses, and sampled once at pointer
down so a drag's tolerances are fixed for its life.

### The sketch view

The session stores no camera. The JNI layer keeps the user's pose
(`CameraController::capturePose`) for as long as the sketch is open, frames the
plane with `frameWorkplane` — the plane's own orbit angles, the world origin,
the current distance, and the ORTHOGRAPHIC projection through the ordinary
framing-preserving switch — and restores the pose on commit and on cancel
alike. The top view sits at the pitch clamp rather than exactly vertical; the
ray–plane intersection is exact regardless, so what the finger places is
exact, and the ~3° tilt is honest about being near-orthographic.

### The overlay

`SketchSession::overlay` builds a world-space line list in four ranges — grid
minor, grid major, the two axes in their world hues, and the entities with the
selected one, the one being drawn, the snap marker and the extrude preview
emphasised — under a revision that changes only when something did. The
renderer uploads it through the same fenced staging path every mesh uses,
re-uploads only on a revision change, and draws it last, through the gizmo's
own line pipeline (depth test off) with one push constant per range. It is
presentation on the gizmo's exact terms: no `ObjectId`, no revision of the
scene's, never published, never picked, never exported, never a `.forge` byte,
and the renderer cannot read a coordinate back out of world positions.

### Threading

Every session mutation and every read of it happens under `g_stateMutex`,
exactly as the camera, the gizmo and the scene. A pointer move updates a few
doubles and bumps a revision; it regenerates nothing. Extraction runs at
Finish, and regeneration runs at commit and at an Apply — discrete user acts,
each one publication. The render thread takes the overlay pointer under the
same lock as the scene snapshot and does its upload with the lock released.

### What this stage deliberately does not do

CAD → Sculpt: `buildSculptSourceMesh` returns false for a CAD Body, the freeze
refuses by name (`CadBodyNotSculptable`), *Start Sculpting* is absent for one,
and a `SCUL` entry over a `CADB` body is refused by the codec. Enabling it needs
three decisions this stage does not make — the wording of the way back out of
Sculpt over a CAD Body, the stale-source rule over a CAD edit, and the
`CADB`+`SCUL` file combination — and each deserves its own approval. Since
`CAD-VERTICAL-SLICE-R1` holes, face-supported sketches, arcs, splines and the
Add / Cut booleans exist; fillets, chamfers, shells, revolves, sweeps, lofts,
patterns, sketch mirrors, offsets, trims, constraints, Intersect, feature
delete/reorder and suppression are absent and are not drawn anywhere.

## Sculpt domain

`forgeshape_sculpt.{h,cpp}` owns each body's second representation and the seven
tools that edit it. Like the Construction domain it contains no JNI, Android,
Vulkan, renderer or UI type, and it holds no GPU resource.

### What is per body, and what is the session

| Per body — `FrozenSculpt`, owned by `SceneObject` | Session-wide — one `SculptSession` |
| --- | --- |
| the Frozen Sculpt Mesh (`SculptMesh`) | the product mode (Construction / Sculpt) |
| its stale-source flag | the held tool |
| its `SculptHistory` (`ARCH-OWNER-12`) | brush radius and strength |
| its Sculpt Mask, inside the mesh's own vertices (`SCULPT-FCM-R1`) | the stroke in progress, and the session-lifetime stroke count |

`sculptSession()` re-points the one session at the **active body's**
`FrozenSculpt` on every access — a single pointer write, and rebinding every time
rather than only on a selection change removes the whole class of bug where the
session is left pointing at the body the user just navigated away from. Making
`SculptSession` itself per body is wrong twice over: `productMode()` would answer
for whichever body is active, so selecting a body left in Sculpt mode would refuse
every later selection; and **"Radius and Strength are shared" is a product
contract** — switching bodies must no more change the brush than switching tools
does — which a per-body copy breaks silently.

Body switching is refused while in Sculpt mode. The Sculpt target is fixed for
the duration of the mode and the user returns to Construction to change bodies,
which avoids having to decide what a body switch does to a half-finished stroke.

### Two representations, one body

```
SOURCE representation                   Frozen Sculpt Mesh
  ObjectId                                the SAME ObjectId
  Construction Source: kind + parameters   a COPY of the local vertices/indices
  or Imported Mesh:    the geometry        its own SculptRevision
  -> buildSculptSourceMesh() [LOCAL] --Freeze--> SculptMesh
```

Since `IMPORT-01B` the source may be either representation; the diagram above is
the only line that differs between them, and it is one function. These are
separate truths and neither writes to the other:

- **Freeze copies.** `SculptMesh::freezeFrom` takes the body's current local
  source mesh wholesale. Nothing is shared, so no sculpt edit can reach back into
  the source -- neither a Construction parameter nor an imported vertex.
- **A sculpt edit changes no parameter, no kind, no imported array and no
  transform.** It cannot: the sculpt module has no mutable access to
  `ConstructionObject` or `ImportedMesh`, and the only mutation `SculptMesh`
  offers is a single vertex POSITION — nothing here can add,
  remove or reorder a vertex or touch an index, so topology is preserved by
  construction rather than by discipline. **Nothing reconstructs a parameter from
  sculpt vertices** either; that direction does not exist.
- The transform is deliberately **not** duplicated. Both representations are local
  geometry under the one `ConstructionTransform`, so there is no second placement
  to keep in sync.
- `SculptRevision` is emphatically **not** a `MeshRevision`. `MeshStore`'s
  revisions count every published snapshot of whichever representation is active;
  a `SculptRevision` counts changes to *this* mesh and restarts at 1 on every
  Freeze. Neither is derived from the other and they cannot be compared.

### Mode ownership

`SculptSession` is process-scoped like the camera, the selection and the mesh
store. The mode is **native state**: the Android UI may request a change and is
then told what the mode actually is; `syncFromNative()` reads `productMode()` back
rather than assuming its request succeeded, so a refused request (entering Sculpt
with nothing frozen) cannot leave surfaces on screen that lie about what is being
edited. The active tool is read back the same way.

- **Start Sculpting** (`freezeToSculpt`) snapshots the body's current local
  source mesh through `buildSculptSourceMesh`, creates the Frozen Sculpt Mesh
  with the same `ObjectId`, and enters Sculpt mode. It asks nothing about which
  representation the body has.
- **Back to Construction**, or **Back to Imported Mesh** over a body whose
  geometry came from a file, keeps the sculpt mesh untouched and republishes the
  body's SOURCE representation, so the original object comes back exactly as it
  was. One native act, two labels: see *Sculpting an Imported Mesh*.
- **Resume Sculpt** re-enters Sculpt **without** re-freezing, so prior
  deformation returns. Freezing again is a separate button precisely because it
  discards the sculpted vertices; collapsing the two into one control would make
  which of those happens depend on hidden state.

### Stale-source policy

When the Construction Source changes while a Frozen Sculpt Mesh exists, the
sculpt mesh is **never** silently replaced or re-derived. It is marked
`sourceStale`, the Sculpt panel says so in as many words, and adopting the new
source stays an explicit user act — another Freeze, the only thing that clears
the flag. There is no automatic sculpt-edit transfer.

An **Imported Mesh can never go stale.** It is immutable for the life of its
body: there is no edit path to one, so nothing can make the flag true and the
warning simply never appears over one. That is a consequence of the
representation, not a branch in the UI.

### Active representation

Exactly one place decides which representation the renderer and the picker see:
`publishActiveRepresentation` in `forgeshape_jni.cpp` publishes the Construction
mesh in Construction mode and the Frozen Sculpt Mesh in Sculpt mode, both through
the existing `MeshStore` path.

That is the whole of the sculpt render/pick integration: making the sculpt mesh
active is a *publication*, not a renderer change. The renderer still owns no
geometry truth, there is no second upload mechanism, and the two consumers
cannot end up looking at different representations.

### Fixed topology: adjacency and normals

Sculpt topology never changes — `SculptMesh` can only move a vertex — so the
index buffer a Freeze copied is the index buffer for the life of that frozen
mesh. `SculptTopology` is therefore built **once, in `freezeFrom`**, and reused by
every stroke and every move; nothing rebuilds it per Move, and the self-tests
assert that its build count stays at 1 across a whole stroke.

It stores exactly two things, in flat CSR arrays: each vertex's **1-ring vertex
neighbours** and its **incident triangles**. Neighbour lists are sorted and
deduplicated, because a shared edge is seen once from each of its two triangles
and without the dedup every interior neighbour would be weighted double in a mean.
Out-of-range indices are skipped rather than trusted and no vertex is ever its own
neighbour. It is deliberately **not** a half-edge structure: half-edges are the
right shape for *changing* topology, and nothing here can change topology.

`computeVertexNormals` produces **area-weighted** normals from the CURRENT
positions: each triangle contributes its unnormalized `(v1-v0) x (v2-v0)` — the
same canonical outward normal picking uses, whose length is twice the area — to
all three corners, then each vertex normal is normalized. A degenerate triangle
contributes a zero vector rather than a NaN, and a vertex with no usable
accumulated direction gets the **zero** vector rather than an invented axis, so a
normal-based brush simply does not move it. The cache is marked dirty by every
accepted `setVertexPosition` and recomputed on the next read — at most once per
batch of position writes, never per frame and never when nothing has moved.

### The brush kernel

`SculptStroke` is the one kernel. It captures everything on DOWN and then holds
it fixed for the whole stroke: the **active tool**, the affected vertex set,
their falloff weights, their starting positions and **starting normals**, the
world-space camera plane, the world-per-pixel scale at the hit depth and the
inverse model transform.

```
DOWN   buildPickRay -> transformRayToLocal -> pickTriangleMesh (front faces only)
       miss                  -> no stroke at all, for any tool
       hit                   -> anchor, depth along the camera FORWARD axis,
                                worldPerPixel at that depth,
                                worldRadius = radiusPixels * worldPerPixel,
                                capture every vertex inside it IN THE WORLD
                                METRIC, with its weight, its base position and
                                its base normal
MOVE   accumulate pointer path length, then dispatch on the captured tool
       -> advance SculptRevision, publish through MeshStore
UP     finalize        CANCEL  drop the stroke
```

Everything above the dispatch is shared. **A tool is a deformation rule and
nothing else**, selected by a closed enum and a switch — no brush base class,
registry, plugin surface or reflection, because a framework would have to be
persisted, versioned and validated like real authored state.

Deliberate properties of the kernel, shared by all seven tools:

- The **radius is authored in screen pixels** and resolved to **world meters** at
  the depth of the hit point, so the brush feels the same size at any zoom. It is
  a property of the gesture, not a length belonging to the object, which is why
  the authored value is not in meters. It is never carried into object space —
  see *The brush metric under a non-uniform Scale* below.
- Depth is measured along the camera **forward axis**, not along the ray, so the
  scale is the same everywhere on screen. The field of view comes from the
  snapshot's own projection term (`proj.m[5]`, which is `-1/tan(fovY/2)` in
  Perspective and `-1/orthoHalfHeight` in Orthographic); nothing here restates
  `kFovYRadians`, the orthographic span or the aspect. The depth factor applies
  in Perspective only — see *Screen ray, per projection*.
- The **affected set is fixed at stroke start**, so a vertex cannot wander into
  or out of the brush mid-stroke and a stroke stays one coherent deformation. It
  is found by a linear scan, like picking; there is no spatial acceleration.
- The falloff is `w = (1 - (d/r)^2)^2`: 1 at the centre, 0 at and beyond the rim,
  with zero derivative at both ends, so a stroke leaves no crease at the edge.
  One function, `sculptFalloff`, used by every tool, and `d` is always the world
  distance.
- A brush that would capture **no** vertex starts no stroke, exactly as a miss
  does.
- **Radius and strength are shared by every tool.** There is no per-tool copy of
  either, so switching tools never changes how big or how strong the brush is.
  Both are **clamped**, not refused: a slider cannot produce a brush that does
  nothing or a brush without bound, and native code stays the authority.
- A stroke **holds the tool it began with**. Changing the session's tool
  mid-stroke changes what the next stroke will be, never what this one is.
- A cancelled stroke is a stroke that **stopped**, not one that is rolled back.
  Positions already written stay written — and because they stand, they are
  RECORDED: `SculptSession::cancelStroke` commits the same single history entry
  an ordinary end would, so the one deformation a user can see is never the one
  they cannot take back. A cancel that moved nothing records nothing. What is
  forbidden is a PARTIAL entry, and `SculptStroke::buildDelta` makes that
  structural: an entry is built in one pass from the whole affected set or not
  at all.

### The brush metric under a non-uniform Scale

**Every brush measures in the world/display metric, never in local
coordinates.** This is one invariant with one implementation, and it is the only
place a body's Scale reaches the brush.

The Frozen Sculpt Mesh is local geometry; the body carries it into the world
through `Model = T · R · S`. The radius is resolved in world meters, so the
distance compared against it has to be the world one. For a local offset `d`:

```
brushWorldDistance(model, d) = |R · S · d| = |S · d|
```

because a rotation preserves length — which is why the helper takes the whole
model matrix rather than a scale triple, and why a mixed rotation changes
nothing about the answer. Only at `S = (1,1,1)` does that reduce to `|d|`, which
is exactly why the pre-020R3 kernel — which compared local offsets against a
world radius — selected a footprint three times narrower along the stretched
axis of a body scaled `(3,1,1)` and left an oval mark. That was the wrong metric,
not a look, and no single averaged, largest or smallest scale factor can repair
it: an anisotropic stretch is not a scalar, and collapsing it to one only
chooses which axis stays wrong.

Two shared helpers in `forgeshape_sculpt.{h,cpp}` carry the whole correction, so
there is one conversion rather than one per tool:

| helper | used by | what it converts |
| --- | --- | --- |
| `brushWorldDistance(model, localDelta)` | the shared capture, so all seven tools | a local offset to its displayed length |
| `brushLocalStepAlongNormal(inverseModel, localNormal, worldMeters)` | Clay, Inflate, Crease | a world deposition to the local step that produces it |

`brushLocalStepAlongNormal` needs the **normal matrix**, and takes no third
argument for it: the inverse transpose of the model's upper-left 3×3 is exactly
the transpose of the *inverse* model's upper-left 3×3, which is `R · S⁻¹` — the
same matrix the renderer shades a scaled body with. It builds the step in world
space, where the amount is measured, and carries it back through the inverse
model, which is precisely what Grab already did with its camera-plane delta.
That is why Grab needed no displacement correction: `M · (base + M⁻¹ · Δ) =
M · base + Δ` for any invertible `M`, so a grabbed vertex already moved by
exactly the world delta the finger described. Smooth needed none either —
interpolating toward a neighbour mean is an affine combination, and an affine
combination commutes with any linear transform.

What this does **not** do is equally load-bearing:

- No scale is baked into a sculpt vertex, at Start Sculpting or ever. The Frozen
  Sculpt Mesh is a byte-exact copy of the Construction local mesh, and the nine
  authoritative transform values are untouched by entering, leaving or resuming
  Sculpt.
- The correction publishes no `MeshRevision`, opens no Construction edit and
  records no history step. A sculpt stroke remains a sculpt mutation.
- At `S = (1,1,1)` every formula reduces to what it was, so unscaled sculpting is
  unchanged.

### The seven tools

| | driven by | direction | accumulates |
| --- | --- | --- | --- |
| Grab | where the finger **is** | camera plane | no — `base + delta × weight` |
| Clay | pointer **path length** | each vertex's normal **at stroke start** | yes |
| Smooth | pointer **path length** | toward the 1-ring neighbour mean | yes |
| Flatten | pointer **path length** | toward one plane fitted at stroke start | yes |
| Inflate | pointer **path length** | each vertex's normal **right now** | yes |
| Crease | pointer **path length** | inward along the start normal **plus** a tangential pinch | yes |
| Mask | pointer **path length** | — it writes a WEIGHT, never a position | yes |

Clay's, Inflate's and Crease's direction is that normal **as displayed** —
carried through `R · S⁻¹` — so on a stretched body they deposit along the
surface the user can see rather than along a local direction that points
somewhere else. On an unscaled body the two are the same vector.

`SCULPT-FCM-R1` APPENDED Flatten, Crease and Mask to the enum, so the four that
had already crossed JNI as an index keep theirs; the RAIL presents the seven in
the product's reading order above, which is a separate decision made in
`WorkspaceTrailingHostView`. **Six write a POSITION and Mask writes a WEIGHT** —
`sculptToolMovesGeometry` is the one place that division is stated, and the mask
factor, the revision rule and the history's two sides all ask it rather than
each testing for `SculptTool::Mask`.

**Grab** is position-driven, so its result depends only on where the finger *is*,
never on how many events it took to get there. The other six are
**path-driven**. Per move,
`travelFraction = pointer travel this move / brush radius` (both in pixels), and

```
amount = strength * worldRadius * kNormalBrushGain * travelFraction   (Clay, Inflate, Crease)
lambda = strength * weight * kSmoothGain * travelFraction             (Smooth)
lambda = strength * weight * kFlattenGain * travelFraction            (Flatten)
delta  = strength * weight * kMaskGain * travelFraction               (Mask)
```

`amount` is **world meters** — the same metric that chose the affected set — so a
stretched body gets an even slab rather than a deeper one along its long axis.

Measuring path length rather than counting events makes them independent of the
event rate: the same finger path deposits the same amount whether Android
delivered it in five events or fifty, a stationary finger does nothing, and there
is no timer and no per-event dab. `amount` is clamped to one brush radius per
move, so a teleporting pointer cannot produce an unbounded displacement; `delta`
is clamped to `kMaxMaskStep` for the same reason. `kNormalBrushGain` (0.35),
`kSmoothGain` (1.0), `kMaxSmoothLambda` (0.9), `kFlattenGain` (1.0),
`kMaxFlattenLambda` (0.9), `kCreaseInwardFraction` (0.75),
`kCreasePinchFraction` (0.45), `kMaskGain` (1.0) and `kMaxMaskStep` (0.5) are
chosen defaults, not derived constants; the low deposition gain is why a short
stroke with a large brush reads as doing very little, and the same arithmetic is
why painting a mask is a matter of working over an area rather than one tap.

**Clay is deposition and Inflate is expansion**, and the difference is the
formula, not a constant. Clay reads `baseNormal`, captured once on Down and held
fixed exactly as the affected set and weights are, so it lays a coherent slab in
one consistent set of directions. Inflate reads `mesh.vertexNormals()`,
recomputed from CURRENT positions every move, so on a forming bulge the flank
normals have tilted outward and it widens and rounds instead of extruding —
directly observable, and asserted by self-tests with thresholds three orders of
magnitude apart.

**Smooth is bounded interpolation, never extrapolation** —
`p := p + (neighbourAverage - p) * lambda`, `0 < lambda <= kMaxSmoothLambda` — so
a vertex can only move part of the way toward a point it is already surrounded
by. That is what makes repeated smoothing converge (deviation shrinking by
`(1 - lambda)` per application, never changing sign) and why no clamp on the
result is needed. Targets are computed from a coherent snapshot **before**
anything is written (Jacobi, not Gauss-Seidel), so the outcome cannot depend on
the affected set's order; only affected vertices are written. Inflate snapshots
its normals the same way and for the same reason.

**Flatten is Smooth's rule with a plane as the target.** `fitFlattenPlane` runs
ONCE, at pointer-down, from the captured set: the weighted centroid of the base
positions and the weighted average of the base normals, both in WORLD space,
both weighted by the same falloff the brush deforms with. No camera, no zoom and
no viewport enters it — which is what makes a flattened result independent of
how the sculpt was looked at — and a fit whose normals cancel (a saddle, a fold)
is refused, leaving Flatten to move nothing rather than invent a plane. Each
move then measures the signed WORLD distance `d = dot(P - C, N)` and writes
`d' = d · (1 - lambda)`, so `|d|` shrinks by a bounded factor on every pass and
can never change sign. That is the whole convergence claim, and it holds per
vertex rather than on average.

**Crease is one displacement with two components**, and the split between them
is the whole character of the tool: inward alone digs a round dent, pinch alone
gathers the surface without deepening it. Both are fractions of the SAME shared
`amount`, so Crease answers to Radius, Strength and travel exactly as Clay does:

```
inward = -n * amount * weight * kCreaseInwardFraction
pinch  =  t * amount * weight * kCreasePinchFraction
```

where `n` is that vertex's base normal in world space and `t` is the unit part of
"toward the brush centre" perpendicular to `n`. Gathering the surface ALONG the
groove rather than pushing it through is what makes the channel narrower than
the brush. The two fractions live together because their RATIO is the tool; a
vertex with no usable normal is skipped and one whose tangent degenerates (it
sits under the centre) gets the inward component alone, so no small or
degenerate local configuration can produce a NaN.

### The Sculpt Mask (`SCULPT-FCM-R1`)

**Runtime-local editing state, and never project truth.** A per-vertex weight in
`[0, 1]`, default 0, saying how much a vertex is HELD against the six geometry
brushes.

**Where it lives, and why there.** In the Frozen Sculpt Mesh's own vertex
records — `MeshVertex::mask`, a fourth float beside the position and the colour.
That is a DERIVED PRESENTATION CHANNEL on exactly the terms the colour already
is, and putting it there rather than in a parallel array means no publication
signature between `SculptMesh` and the vertex buffer had to learn that masking
exists: `publishSculptMesh`, `MeshStore::publish`, `createRuntimeMesh` and
`RuntimeMesh` are unchanged. It is sized and zeroed by `freezeFrom`, nothing can
resize it (topology is fixed for a frozen mesh's life), and it is per body by
OWNERSHIP — one body's mask is as incapable of reaching another's as one body's
mesh is.

**What it does to a brush.** `sculptMaskFactor(w) = 1 - w`, with **exact ends**:
`w = 0` is the full effect and `w = 1` is exactly zero, by an equality rather
than by arithmetic that happens to land there — a residual `1e-8` would still
write a position, mint a revision and put an entry in the history. The factor is
captured at pointer-down into `SculptStrokeVertex::maskFactor` with everything
else the stroke holds fixed, and every geometry brush multiplies by
`effectiveWeight() = weight * maskFactor`. The falloff weight is left untouched
beside it, so the brush's own footprint stays introspectable independently of
what the mask allowed. **The Mask brush is deliberately not held off by the mask
it paints**: a brush that masked itself could never reach 1.0.

**It is not geometry, and the plumbing says so.** A mask write mints no
`SculptRevision` and sets no `hasEdits`, because the project fingerprint mixes
the revision and `.forge` stores the flag — advancing either would make a
runtime annotation dirty the project and earn a recovery checkpoint. What a mask
change does need is a re-publication, and `MeshStore::publish` mints its own
`MeshRevision` on every call regardless of the sculpt counter, so the viewport
updates without the geometry's own number moving at all.

**The persistence boundary**, stated once:

| | survives |
| --- | --- |
| Back to Construction, then Resume Sculpt | **yes** — it lives on the body, and leaving Sculpt is navigation |
| body switching | **yes**, per body — the session rebinds and finds that body's own |
| Save, autosave checkpoint, Export | **no byte of it exists to save** |
| reopening a project | **no** — the mesh comes back, the mask starts empty |
| Freeze / destructive Reset from source | **no** — cleared, on the boundary the history cannot cross either |

**Viewport feedback.** One fragment-stage mix toward a cool, low value
(`kMaskTint`, `kMaskMaxMix` in `surface.frag`), driven by a fourth vertex
attribute (`RenderVertex::mask`, `VK_FORMAT_R32_SFLOAT` at location 3). It is a
mix rather than a multiply because a multiply vanishes wherever the surface is
already dark — the shadow side of a form is exactly where a mask most needs to
be visible — and it is applied AFTER shading and BEFORE the selection tint, so
one rule works in Studio Solid, in MatCap and in the debug mode alike and a
selection the user just made still reads over a masked body. The value is
clamped into `[0, 1]` on the CPU, once, in `buildRenderMesh`, so no shader has
to defend against a value the domain says cannot exist. **No second pass, no
second pipeline and no push-constant slot** — the surface block is already at
the guaranteed 128-byte minimum.

**Clear Mask** is `SculptSession::clearMask()`: one act, one history entry, one
Undo. It refuses `NothingToDo` for an empty mask and `EntryTooLarge` for one
whose single entry would exceed `kMaxSculptHistoryEntryBytes` — deliberately NOT
the brush's own `NotRetained` policy, and the difference is which act is being
asked about. A brush stroke is bounded by the brush and its deformation is what
the user is doing, so refusing to sculpt because the history is full would be
the tail wagging the dog; Clear Mask is bounded by the MESH, is a discrete
command rather than a gesture, and its whole value is that it can be taken back.
`canClearMask()` answers the same conditions minus the size test, so the control
is ABSENT rather than drawn and refused.

### Sculpt Undo and Redo — `SculptHistory` (`ARCH-OWNER-12`)

`forgeshape_sculpt_history.{h,cpp}`. The product's SECOND history, and
deliberately not an extension of the first.

**Why a second one at all.** A `ConstructionHistory` step holds bounded
Construction-domain state and never one mesh byte. A sculpt stroke's entire
effect *is* mesh bytes, so it could not become a step there without inverting
that rule. Two histories that never merge is the answer; a combined timeline does
not exist, and neither is consulted for the other.

**Ownership is the mechanism.** A `SculptHistory` lives in `FrozenSculpt`, beside
the mesh it describes, so it is per body by construction — no key, no registry,
no "current sculpt stack". `sculptSession()` rebinds to the active body's
`FrozenSculpt` on every call, so switching bodies switches history with no
lookup, and body A's Undo is *structurally incapable* of reaching body B. It
survives Back and Resume for the same reason it survives a body switch: leaving
Sculpt is navigation and takes nothing off the body.

**One completed stroke is exactly one entry.** The transaction boundary is the
stroke's, not the pointer event's. `SculptStroke::begin` already captures the
affected set with each vertex's base position — the tools need it — so the
BEFORE side costs nothing extra; `SculptStroke::buildDelta` reads the AFTER side
off the mesh at close, keeping only vertices that actually moved.
`SculptSession::recordActiveStroke` is the ONE caller, reached from both
`endStroke` and `cancelStroke`, which is what makes the boundary structural
rather than a convention two call sites share.

**The entry is a delta.** Sorted unique indices, before positions, after
positions, and the edited flag on both sides. Normals are absent because the mesh
regenerates them deterministically from positions and a stored copy could only
disagree. A document, a Construction parameter, an `ImportedMesh`, a transform,
the `ObjectId` allocator, a GPU buffer and a `.forge` byte are all absent because
none of them is what a stroke changed.

**Both caps are enforced, on every record.** `kMaxSculptHistoryEntries` (32) and
`kMaxSculptHistoryBytes` (4 MiB) per body, with `kMaxSculptHistoryEntryBytes`
(1 MiB) for a single stroke. Either alone is escapable — a step cap lets 32
whole-mesh strokes hold hundreds of megabytes; a byte cap lets an unbounded
number of one-vertex allocations accumulate. Eviction is oldest-first and never
takes the newest entry. A stroke over the per-entry cap still APPLIES, is counted
by name (`RecordOutcome::NotRetained`) and is reported once to the user, because
refusing to sculpt because the history is full would be the tail wagging the dog.
The byte measure is recomputed from the contents rather than tracked
incrementally: a running total that drifts from what is held is exactly how a cap
stops being one.

**`hasEdits` became a stored fact.** It was `revision > kFrozenSculptRevision`,
and Sculpt Undo is what forced the two apart. The case a depth counter cannot
answer: a project loaded with an already-edited sculpt mesh starts with an EMPTY
history and must still report edits, so undoing the one new stroke taken since
must land on `true` — while undoing the first stroke after a fresh Freeze must
land on `false`. Only the value each entry captured at its own stroke's start
answers both. `SculptMesh::restoreEditedFlag` has exactly one caller.

**Revisions stay monotonic.** Geometry goes backwards; the counter goes forwards.
The renderer, CPU picking and `projectSemanticFingerprint` all notice a sculpt
change by that number, so an Undo that rewound it would be invisible to precisely
the caches that must see it. `applyHistorySide` advances the revision and then
restores the flag — the one place the two legitimately disagree.

**Nothing here is serialized.** No `.forge` byte, no checkpoint, no schema
change, no version bump. A document stores what cannot be recomputed; the PATH a
user took to the current positions is a property of the editing session, like the
camera or the held brush. Reopening a project restores the geometry and starts a
fresh, empty history, and the first stroke after that is entry one.

**Two boundaries Undo cannot cross.** A Freeze — including the destructive Reset
from source — clears both stacks, because every entry names positions in a mesh
that no longer exists and the confirmation the user just gave said the previous
sculpt is gone. And a Delete: the body leaves whole, and its history leaves with
it. Undoing the Delete restores the SAME object, so the stacks come back with it
— not because anything was serialized or added to a history step, but because
`holdDetachedBody` holds the object rather than destroying it. When no step names
the body any more it is released, and the history dies with it.

**The History navigator is a VIEW of this branch** (`SCULPT-H1`). It adds no
storage, no capacity, no budget and no second stack, because the retained branch
was always a line and the cursor was always a position on it —
`SculptHistory::cursor()` derives `{cursor, undoCount, redoCount, stateCount}`
from the two deques on every read and stores none of it. A ROW is a STATE, not
an entry: `u` undo entries and `r` redo entries are `u + r + 1` states, the
cursor stands at `u`, and state 0 is the OLDEST RETAINED state rather than
necessarily the Freeze, because eviction drops from that end and what it dropped
cannot be jumped to.

`SculptSession::jumpToHistoryCursor` is the one act. It is **implemented as**
repeated `undoStroke()` and repeated `redoStroke()` rather than described that
way, so "jumping back three is the same as tapping Undo three times" is a
structural fact rather than a property two implementations have to keep
agreeing on — there is no second delta path, no batched apply and no snapshot
restore. It asks the same three refusals a single step does, adds
`OutOfRange` for an ordinal that is not on the branch (**refused, never
clamped** — landing the user somewhere they did not tap is worse than a refusal
they can see), and answers `NothingToDo` for the row already stood on, applying
nothing and minting no revision. It records nothing: no entry, no Construction
step, no `.forge` byte. The **abandoned future is not dropped here** — jumping
backward leaves it walkable exactly as an Undo does, and the existing rule
(`record` clears the redo stack) drops it when the next stroke makes it describe
a future that no longer follows.

Above JNI, `sculptHistoryState` returns the whole model in ONE locked read —
five values that must describe one body's branch at one instant — and
`sculptJumpToHistoryCursor` publishes through `publishSculptRepresentation`
once for the whole jump rather than once per intermediate step. Body switching
needs no rebinding code: the model is per body below JNI, so a refresh after a
switch reads the new body's branch and `SculptHistoryNavigatorView` rebuilds.

**The apply is not in this class.** `SculptHistory` includes `forgeshape_math.h`
and nothing else from the domain: it is a bounded stack that decides what is
retained and never writes a vertex. `SculptSession::undoStroke`/`redoStroke`
read the top entry, apply it, then commit the move — so there is exactly one
place a sculpt vertex can be written by history, and it is the same class that
owns the stroke that wrote it in the first place. Every step runs under
`g_stateMutex`, the same lock a stroke's own position writes and the render
thread's scene snapshot take, and publishes through the ordinary
`publishSculptRepresentation` path — a history step reaches the renderer exactly
the way a stroke does.

### Sculpt-mode gesture rule and arbitration

One finger that goes **down on the Frozen Sculpt Mesh** is a brush stroke and
owns the whole gesture; one finger that goes down anywhere else navigates exactly
as it always has. Two-finger pan and pinch are untouched. Whether the finger
landed on the mesh is decided **once, on Down, and never revisited**, so a stroke
cannot turn into an orbit half way through a drag as the finger crosses the
silhouette.

What *is* deferred is whether the gesture is a stroke at all. A one-finger Down
on the mesh is ambiguous when it arrives — it is either the start of a stroke or
the first of two fingers — so the rule is **pending-then-promote**:

| event | result |
| --- | --- |
| Down, one finger, ray hits the mesh | **PENDING**. Swallowed: no stroke exists, nothing is deformed, and neither the camera nor the selection sees it. |
| Move, still one finger, travelled >= `kStrokeArmPixels` (8 px) | **PROMOTE**. The stroke begins at the **original down point**, and this same event is applied as its first move. |
| a second finger, an Up, a Cancel, any multi-pointer event | **ABANDON**. No stroke ever existed, so there is nothing to end and nothing to undo. |

`SculptSession::hitsSculptMesh` is the probe that answers "would a stroke start
here?" *without* starting one. It provably cannot move a vertex, mint a revision
or advance the stroke counter — the structural half of the guarantee that a
gesture which turns out to be navigation cannot have mutated the sculpt mesh.

Anchoring the promoted stroke at the original down point makes the deferral free:
the hit, the affected set and the weights are exactly what they would have been
had the stroke begun on Down. The camera re-anchors on any pointer-set change so
an abandoned gesture produces no jump, the selection never saw the Down, and while
a stroke owns the gesture neither controller sees the event at all — which is what
makes a brush gesture structurally unable to orbit, select or clear.

The 8 px threshold sits between touch jitter and the 24 px tap slop, and the
residual it accepts is stated rather than hidden: a first finger that
*deliberately drags more than 8 px* before the second lands does commit a stroke.
`g_grabbing` and `g_strokePending` in `forgeshape_jni.cpp` are gesture routing
only, guarded by `g_stateMutex`, and are dropped whenever the Surface goes away;
the mode, the active tool and the sculpted vertices are not, being process-scoped.

## Runtime mesh ownership

`forgeshape_mesh.{h,cpp}` owns the CPU side of geometry. It contains no JNI,
Android or Vulkan types and holds no GPU resource.

`RuntimeMesh` is one **immutable** published revision: an object id, a revision
number, interleaved `MeshVertex` (position + colour) data and `uint32_t` indices.
The only way to build one is `createRuntimeMesh`, which validates first, so a
`RuntimeMesh` that exists has already been proven usable. Consumers hold it
through a `shared_ptr<const RuntimeMesh>`, so a pick in progress keeps reading
its own revision even while a newer one is published. `MeshStore` is the single
publication point, one per body. It mints monotonically increasing revisions, and:

- invalid data **fails closed** — the previous revision stays current and the
  reason is reported (null data, zero counts, index count not a multiple of
  three, an index >= vertex count, a non-finite position, or past the hard caps);
- a snapshot whose revision is not strictly newer than the current one can never
  replace it;
- the stable `ObjectId` belongs to the store, not to the geometry and not to any
  GPU resource, so replacing the mesh cannot change what is selected;
- consumers observe only the newest revision. Revisions published between two
  observations are **coalesced away**, deliberately: that is what bounds the GPU
  upload path.

The publication path is synchronous and republishes the whole mesh, O(vertices)
per sculpt move regardless of how few vertices the brush touched — the known cost
a future partial-update or asynchronous path has to beat.

**Index width.** The runtime and render paths use **32-bit indices** (`uint32_t` /
`VK_INDEX_TYPE_UINT32`). 16-bit indices would structurally cap every mesh at
65,535 vertices, and there is no measured benefit at these sizes; the CPU
`TriangleMeshView` and the GPU index buffer agree on one width, so they cannot
drift.

## Derived render geometry and shading

`forgeshape_render_mesh.{h,cpp}` sits between the authoritative mesh and the GPU.
It is platform-neutral and holds no GPU resource. The chain is one-way:

```
Construction / Sculpt truth
  -> authoritative RuntimeMesh   (positions + indices + colour)
      -> RenderMeshData          (positions + NORMALS + colour)
          -> Vulkan buffers
```

Nothing is read back. A normal is never a dimension, a Construction parameter, a
sculpt deformation or picking topology, and **CPU picking still runs on the
source `RuntimeMesh`** — which is precisely what frees this layer to duplicate
vertices, because a render vertex has no identity anything outside the renderer
can observe.

### Render-only vertex duplication, and the two counts

A hard edge needs two different normals at one position, and a vertex carries one
normal, so a corner on a crease becomes several **render** vertices. Therefore:

- render vertex count >= source vertex count, and usually differs;
- render index count == source index count, always — a corner is remapped, never
  added, so the triangle list is the same triangles;
- every diagnostic names which it means. `FORGESHAPE_MESH_UPLOAD_OK` reports
  render counts in its historical positions and adds `src=v:i`;
  `FORGESHAPE_RENDER_MESH_BUILD` reports both explicitly.

The measured per-primitive counts live in `PROJECT_STATUS.md`'s shading cost
record. Two shapes of result matter architecturally: a fully smooth closed surface
has no crease to split on, so its render mesh *is* its source topology — the
cheapest available proof that the grouping does not fragment a smooth surface —
and the Plane's doubled count is the **other** reason a render count can exceed a
source one, the bounded two-sided exception below rather than a crease split.

### The two-sided render exception

`buildRenderMesh` takes an optional `renderBothSides` argument
(`RenderMeshCache::refresh` reads it straight off `RuntimeMesh::
renderBothSides()`, so the renderer itself never branches on `PrimitiveKind`).
When true — today, only for a Plane — the ordinary one-sided Smooth or Faceted
result is duplicated once more by `appendMirroredBackFace`: every render
vertex is repeated with its normal negated, every triangle is repeated with
reversed winding, on the duplicated vertex set. Nothing about the source
`RuntimeMesh` changes and nothing is re-validated against a different rule;
this runs entirely on already-built render data.

This needs no pipeline, culling or material change because of the winding
reversal itself: from the front, the duplicate is the **back**-facing triangle and
the existing `VK_CULL_MODE_BACK_BIT` culls it; from the far side the original is
back-facing and culled, leaving the duplicate, whose negated normal is correct for
a viewer there. One global pipeline keeps drawing exactly one of the two triangles
at any position, for every primitive — the exception is entirely in what data
reaches the pipeline, not in the pipeline itself.

### The crease policy

One threshold, `kCreaseAngleDegrees = 40`, stated once and used nowhere else.
Two triangles sharing a vertex contribute to the same smoothed normal when the
angle between their face normals is at most that; otherwise the vertex splits and
each group gets its own normal. Grouping is a per-vertex union-find over that
vertex's incident triangles, so transitivity lets a sphere pole's 32-triangle fan
become one group even though its extreme members are far apart in azimuth.

The value must clear the coarsest curved adjacency (360/32 = 11.25°) and stay
well under the sharpest edge a primitive presents (90°); 40° is near the middle
of that band. That single rule produces every per-primitive contract — a box's
hard 90° edges, a cylinder's smooth side with flat caps and hard rims, a sphere's
continuous shading and stable poles, a cone's smooth side with a hard base rim and
an on-axis apex normal, and a capsule's seamless hemisphere-to-middle transition —
with no per-primitive special case.

Normals are area-weighted (`(v1-v0) x (v2-v0)`), the same weighting
`computeVertexNormals` uses for the sculpt cache, so a surface does not change
character between the two paths. A degenerate triangle contributes the zero
vector rather than a NaN, and a group with no usable length keeps the zero
normal — handled by a documented shader fallback. Faceted shading is the other
branch: three private vertices per triangle carrying that triangle's flat normal,
intentionally exposing triangle structure.

### Rebuild policy

`RenderMeshCache` rebuilds when — and only when — the source `MeshRevision` or
the `SurfaceShading` changed. It does **not** rebuild for a camera move, a
rotation, a window resize, a unit switch, an inspector toggle, a mode or tool
change, or a Studio<->MatCap change, because that last one is a fragment-stage
uniform touching no geometry at all. The renderer gates on the same pair before
calling in, so a steady frame costs two integer comparisons.

The cache derives its own adjacency from the index buffer and deliberately does
**not** reuse `SculptTopology`: the renderer depends on the published
`RuntimeMesh` and on nothing in the Sculpt domain, or "presentation only" would
stop being true the moment sculpt state changed shape.

### Studio Solid and MatCap

Two shading models plus a debug-only source-colour path, as a closed enum and a
switch — the same rule the sculpt tools follow. Both are evaluated in **view
space**, so the lights follow the camera. That is a product decision, not a
convenience: while modelling the user orbits constantly, and world-fixed lights
would swing a face from lit to unlit purely because the viewpoint moved, which
reads as the shape changing. Camera-relative light means a change in shading
always means a change in the *model*.

Studio Solid is computed per fragment in `shaders/surface.frag` and is tuned
matte and even, for judging planar faces and exact silhouettes. MatCap is a
single texture lookup at `uv = n.xy * 0.5 + 0.5`, tuned glossier and
higher-contrast, for reading curvature and sculpt deformation. They share one
geometry of light — same key direction, same fill, same hemispherical ambient —
so switching does not relight the object. Both fills are placed lower-**front**,
not opposite the key: a fill opposite the key lifts exactly the planes the key
leaves dark, so a box's left and right faces end up nearly the same value and the
form stops reading. There is no PBR here and none is implied: no metalness, no
roughness, no environment probe, no shadow map, no ambient occlusion and no
tone-mapping stack.

### The MatCap asset

`forgeshape_matcap.{h,cpp}` **computes** the one 128x128 RGBA8 preset at device
initialization from a closed-form model in that file. No image in the repository,
nothing downloaded, nothing derived from another application's asset — and it is
the only option that respects the no-third-party-library rule, since decoding a
PNG would need a decoder ForgeShape may not depend on. Texels outside the unit
disc are clamped to the rim value in the same direction, and the sampler uses
`CLAMP_TO_EDGE`, so filtering at a silhouette does not bleed. Exactly one preset:
no library, no browser, no import, no per-object material.

### Display settings ownership

`forgeshape_display.{h,cpp}` holds the shading model, the surface shading, the
**viewport background**, grid visibility and the reduced-motion bool as
process-scoped atomics. Native owns them exactly as it owns the product mode and
the active tool; the Android UI may request a change and read the value back, but
does not hold it. A snapshot is pushed into the renderer per frame, outside the
state mutex, because no domain invariant depends on it and a frame must never
wait on the geometry lock to learn which shading model to draw with.

**The viewport background is the whole of what a theme means below JNI.** It is a
closed `ViewportBackground` enum — `NeutralDark`, `WarmLight` — and what crosses
JNI is its index, refused if unrecognised, exactly like a shading model. No
Android theme, no style, no Android type and no RGB authored above JNI reaches
this layer: native code owns what each appearance looks like, so the geometry
domain never learns that themes exist. `viewportBackgroundColor` is read when the
render pass records its clear value, which happens every frame anyway, so a switch
rebuilds no geometry, mints no revision, re-uploads nothing and does not touch the
swapchain, the pipeline, the descriptor set or any buffer. The two float triples
are duplicated in `colors.xml` as the Android *window* background — what covers
the moment before the surface has content — and two self-tests pin them so the
pair cannot drift into a launch flash.

## GPU mesh upload

`Renderer` owns every mesh-related `VkBuffer`, `VkDeviceMemory`, copy and
destroy, and performs all of them on the render thread. Publishers never touch
Vulkan. Steady-state vertex and index buffers are **DEVICE_LOCAL** with
`TRANSFER_DST` usage, written through one reused **HOST_VISIBLE** staging buffer
that carries the vertex block followed by the index block, copied in a single
command buffer with one memory barrier
(`TRANSFER_WRITE` → `VERTEX_ATTRIBUTE_READ | INDEX_READ`).

Capacity policy (`growCapacityBytes`, pure arithmetic, self-tested):

- sufficient existing capacity is **reused as-is**, including for a smaller
  mesh — capacity never shrinks and a same-topology update never recreates a
  buffer;
- otherwise capacity grows by 1.5x, but never to less than what is needed;
- everything is bounded by a hard cap, so the size arithmetic cannot overflow.

The "reuse a smaller mesh" rule earns its keep now that render vertex counts are
derived: a sculpt stroke that creates genuine creases splits vertices, so the
**render** count drifts move to move even though the source count is fixed, and
because capacity never shrinks that whole stroke uploads with zero buffer growth.

### In-flight resource safety

Before a mesh buffer is overwritten or retired, the renderer waits on **its own
frame fences** — all `kMaxFramesInFlight` of them — so no submitted frame can
still be reading it. The transfer itself is submitted with a dedicated upload
fence that is waited on before the staging buffer or the upload command buffer is
reused. A retired buffer is therefore destroyed only after every frame that could
reference it has finished, and per body exactly one vertex buffer and one index
buffer are live at any time. Retired buffers are freed inline after that fence
wait rather than through a deferred-destruction queue, which is what makes the
wait necessary. The mesh update path deliberately calls **neither
`vkDeviceWaitIdle` nor `vkQueueWaitIdle`**; those remain only where they already
were — process teardown, surface detach and swapchain rebuild.

Mesh buffers are device-scoped, not surface-scoped: a Surface swap does not touch
them, so the uploaded revision survives home/resume with no re-upload.

## Picking and selection

`forgeshape_picking.{h,cpp}` converts a view-local pixel plus a `CameraSnapshot`
into a world-space ray, and intersects that ray with indexed triangles
(Möller–Trumbore, nearest positive hit). It reads the snapshot's own `proj` and
`view` matrices rather than restating FOV or aspect, so there is one camera
truth. It contains no JNI, Android, Vulkan or renderer types, decides no object
identity, and uses no GPU id buffer.

`pickScene` takes a `SceneSnapshot` and intersects EVERY body in it — never
Vulkan buffer memory — keeping the nearest positive hit and taking the object id
from the published mesh rather than from the geometry. Each item is intersected
with ITS OWN model/inverse-model pair and ITS OWN sidedness, so what is pickable
is each body where it actually appears. Distances are directly comparable
between bodies because every Construction transform is rigid, which is what
makes "nearest wins" meaningful across the scene; ties keep the earlier body in
scene order, so the result is deterministic rather than an iteration accident.
Because each item is whichever representation that body has published, picking
automatically follows an edit with no separate collision representation to keep
in sync. Picking is a linear scan over bodies and over triangles; there is no
spatial acceleration. Taking the snapshot as a parameter is deliberate: it lets a
self-test pick a scene it built itself, and it lets the caller take the snapshot
under the state mutex and then scan triangles with that mutex released.

**The two-sided picking exception is one boolean, computed once per item.**
`pickScene` reads the sidedness of the **published mesh** it is about to intersect
— never `constructionObject().kind()`, which after a Freeze can describe geometry
that is not on screen — and passes its negation as `frontFacesOnly` to the
explicit-transform overload, which forwards it to `pickTriangleMesh`. That
argument defaults to `true`, so every other caller, including the self-tests that
use the explicit overload precisely to avoid process-scoped state, is unaffected.
See *Canonical winding and culling* for the single ownership chain.

`SelectionController` (`forgeshape_selection.{h,cpp}`) owns the selected
`ObjectId` and the tap-versus-navigation decision. `ObjectId` is an opaque
`uint64_t` minted by the scene: never a pointer, list index, Vulkan/renderer
handle or display name, so it survives buffer recreation and Surface swaps.
`kNoObject == 0` means nothing is selected. Exactly one body is selected at a
time — no multi-select, no lasso or box selection, and no hierarchy.

## Tap versus orbit

Both controllers receive the same `forgeshape::TouchAction` event, and camera and
selection never consult each other. Tap candidacy is one-way — it can only be
revoked during a gesture, never restored, and only a fresh `Down` starts a new
candidate. It is revoked when:

- total displacement from the **original** down position exceeds
  `kTapSlopPixels` (24 px) — measured from the down point, not per-move, so a
  slow creeping drag still cancels;
- the gesture ever reaches two or more pointers (`PointerDown`/`PointerUp`, or
  any event carrying 2+ pointers);
- the event stream carries a pointer id other than the tracked one;
- `ACTION_CANCEL` arrives, or the Surface goes away.

Only `Up` on a still-valid candidate resolves a pick. A hit selects that object;
a miss clears the selection. A gesture that orbited, panned or pinched therefore
cannot select or clear on release.

## Renderer

`Renderer` (`forgeshape_renderer.{h,cpp}`) owns the Vulkan instance, device,
queues, surface, swapchain, depth resources, render pass, pipeline, command
buffers, synchronization and the geometry buffers. It exposes
`setCamera(const CameraSnapshot&)` and `setScene(SceneSnapshot)` and consumes
both verbatim; it derives no camera pose, builds no rotation, owns no Euler
convention, interprets no pointer data and holds no geometry truth. The mesh
vertices are each body's **local** geometry and never move: where a body appears
comes from its own model transform, where the viewer stands comes from the camera
snapshot, and the renderer only composes them as `mvp = proj * view * model` — so
a screen-space change is always attributable to the camera or to that body.

### Per-body GPU resources

`BodyRenderResources` holds everything the GPU keeps for **one** body: its
device-local vertex and index buffers and their capacities, its
`RenderMeshCache`, and which revision and surface shading those buffers
currently hold. They live in a map keyed by **stable `ObjectId`** — never by
scene index, which would silently rebind a body's buffers to a different body if
the collection were ever reordered.

Staging, the upload command buffer and the upload fence stay **shared**: they
are transient scratch used inside one upload and waited on before the next, so
one copy is both correct and the smaller footprint.

`syncScene()` runs the existing per-frame gate once per body: if that body's
published revision and the surface shading both match what it already holds, it
returns without generating a normal, allocating anything or touching a buffer.
**This is what makes body independence structural rather than a promise** — an
edit to A mints a revision in A's own `MeshStore`, so B's cached revision still
equals B's published revision and B's branch returns immediately. Camera motion,
rotation, a selection change and a Studio↔MatCap switch all land in that gate
and stop, for every body.

`recordBodyDraw` then issues one draw per body, with that body's own model
matrix, its own buffers and its own selection flag. Selection reaches the
fragment shader as a tint push constant **per draw**; there is deliberately no
renderer-wide selection bool, because a single one would tint every body at once
as soon as anything was picked.

`FORGESHAPE_RENDER_MESH_BUILD` and `FORGESHAPE_MESH_UPLOAD_OK` carry a `body=`
field, appended so the historical prefix keeps parsing. With several bodies an
upload line is otherwise ambiguous about which one it describes, and that
ambiguity would destroy the only direct evidence for "editing A did not rebuild
or re-upload B".

What the GPU holds is `RenderVertex` (position + normal + colour), not
`MeshVertex`. The authoritative format is no longer handed to Vulkan directly.

### Push constant budget

One 128-byte range covering both stages, which is the **smallest**
`maxPushConstantsSize` Vulkan guarantees, so the layout stays valid on
implementations exposing only the minimum:

```
  0  mat4 mvp
 64  vec4 normalRow0    xyz = row 0 of the view-space normal matrix, w = shading model
 80  vec4 normalRow1    xyz = row 1
 96  vec4 normalRow2    xyz = row 2
112  vec4 selectionTint rgb = tint, a = mix amount (see Motion and selection feedback)
```

A `static_assert` pins the size. The shading model rides in an otherwise-dead
`w` component rather than taking a fifth 16-byte slot the budget does not have;
the next thing needing per-draw uniform data belongs in a descriptor, not here.

Normals are transformed by the upper-left 3x3 of `view * model` applied
**directly**, not as an inverse-transpose. That is valid only because both
factors are rigid — a look-at view matrix, and a `ConstructionTransform`
documented as rotation + translation with no scale — so the product is
orthonormal and its inverse-transpose is itself. **Adding scale to the transform
would make normals silently wrong on scaled objects**; it is one of exactly two
places that shortcut is taken, the other being picking's "local distance is world
distance".

### The two descriptor sets

Everything else in the renderer travels as push constants — the grid, the gizmo
and the outline's mask pass all declare `setLayoutCount = 0` — so a descriptor
set exists only where something is genuinely *sampled*, and there are exactly
two places.

**The MatCap sampler.** One `COMBINED_IMAGE_SAMPLER` at set 0 binding 0,
allocated once and never updated again because the image is immutable for the
life of the device. It is bound unconditionally even in Studio Solid, since
leaving a declared binding unbound is invalid usage whichever branch runs.
Image, view, sampler and set are device-scoped.

**The selection outline's mask** (`SEL-OUT-R1`). Also one
`COMBINED_IMAGE_SAMPLER` at set 0 binding 0, and deliberately its **own** layout,
pool and set rather than a second binding on the MatCap's. The MatCap set is
written once and shared by every body draw; this one names an image whose only
reason to change is the render extent, so it is **rewritten** on a resize.
Folding them together would mean re-writing the MatCap binding on every rotation
for no reason. Its sampler clamps to an opaque-black border rather than to the
edge — see the outline section for why that is load-bearing. Layout, pool, set
and sampler are device-scoped; only the image the set points at is
swapchain-scoped.

Binding the outline's set at set 0 disturbs the MatCap's binding, which is
harmless and intended: the composite is recorded after every body draw in the
pass, and the next frame rebinds from the top.

### Surface orientation convention

There is exactly one orientation convention: **ForgeShape always renders in
Android window orientation.** The swapchain image is the size of the window the
user sees, and any display rotation is performed by the presentation engine, never
by this renderer. `Renderer::createSwapchain` requests
`preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR` whenever identity appears
in `caps.supportedTransforms`, and takes `imageExtent` from `caps.currentExtent`,
which on Android is the window's own width and height. So one coordinate space —
the Android window — runs unbroken through `SurfaceView` size, the camera
viewport and projection aspect (`CameraController::setViewport`), the swapchain
image, the Vulkan viewport and scissor, and the screen pixel that picking turns
into a ray. Nothing anywhere transposes width and height, and no matrix carries a
display rotation.

ForgeShape deliberately does **not** pre-rotate. Pre-rotation would require
`imageExtent` in the display's *pre-transform* (panel) space — the transpose of
the window at 90° and 270° — plus a matching clip-space rotation and camera
aspect. Declaring `preTransform = caps.currentTransform` while passing the
window-space extent is the inconsistent combination: SurfaceFlinger rotates the
buffer into the transposed layout and stretches it back, an anisotropic scale.

The cost is one compositor rotation on a rotated display, which every
non-pre-rotated Android application already pays. Its consequence is that
`vkAcquireNextImageKHR` and `vkQueuePresentKHR` report `VK_SUBOPTIMAL_KHR` for as
long as the device is rotated — the surface's transform is genuinely not the one
the swapchain declared. **That is the convention working, not a stale swapchain,
so the frame loop must not rebuild on it:** `Renderer::expectSuboptimal_` records
when the declared pre-transform differs from the surface's own, and suboptimal is
ignored in exactly that case. Rebuilds still happen on
`VK_ERROR_OUT_OF_DATE_KHR` and on the explicit `requestResize()` that
`surfaceChanged` raises. Losing this distinction rebuilds the swapchain on every
single frame while rotated.

One non-per-frame `FORGESHAPE_SURFACE_CONFIG` line per swapchain creation records
the window size, `currentExtent`, `currentTransform`, `supportedTransforms`, the
chosen extent and the chosen `preTransform`, so the whole chain is auditable from
a log; `FORGESHAPE_CAMERA_VIEWPORT` is its companion for the camera half.

### The world reference grid

The grid is a viewport **reference**, and the boundary is absolute: it has no
`ObjectId`, never enters `ConstructionScene` or a `SceneSnapshot`, is not a
`RuntimeMesh`, is invisible to picking, takes no part in Freeze, Resume or a
sculpt stroke, is not exported and is not a snap target. A future Sketch grid —
the one that snaps, drawn on a sketch plane — is a **different contract** with
its own approval and must not be grown out of this one.

Ownership splits in the usual place. `forgeshape_grid.{h,cpp}` is
platform-neutral and owns the *contract*: the plane (`y = 0`), the spacing (1 m
minor, 5 m major), the extent (20 m half), `GridLineTier`, one pure vertex
generator and the palette per `ViewportBackground`. `DisplaySettingsStore` owns
*visibility*, beside the shading model. The renderer owns only Vulkan.

**Generated once, never regenerated.** The vertex count is a compile-time
constant, so `createGridResources()` uploads 2624 bytes at device creation and is
never called again. Device-scoped like the MatCap, so a Surface swap or rotation
does not re-upload it, and `FORGESHAPE_GRID_UPLOAD_OK` appears once per device.
Showing or hiding the grid decides only whether **one** already-built `vkCmdDraw`
is recorded. The tier travels in the vertex buffer and the colour does not: the
four tier colours ride in a second 128-byte push block, so switching appearance
re-uploads nothing.

It has **its own pipeline and layout with `setLayoutCount = 0`**, because it
consults no sampler — no normal, no light, no shading model. It was the one
blended pipeline in the renderer until `SEL-OUT-R1` added the selection
outline's composite; that there are now two is not an invitation to grow a
general overlay path, and each still states its own blend rather than sharing
one. Depth is **tested, not written**, and it draws after every
body, so the model occludes it and it contributes nothing a later draw could be
occluded by. Three non-obvious constraints are documented at their sites in
`forgeshape_renderer.cpp` and `shaders/grid.{vert,frag}`: `polygonMode` must be
`FILL` under `LINE_LIST`; the destination **alpha channel must be preserved**, or
the compositor punches the viewport translucent along every line; and the radial
fade must be **per fragment**, because a line's only two vertices are its
fully-faded endpoints.

**The coplanar case is settled in the vertex shader.** A Construction Plane at
world `y = 0` shares the grid's plane exactly, so the grid is pushed `1e-4` of the
depth range further from the eye and loses every tie deterministically under
`VK_COMPARE_OP_LESS`. **Nothing in the domain moves for this.** It is in the
shader rather than in `VkPipelineRasterizationStateCreateInfo` because Vulkan's
depth bias is defined for **polygon** fragments and would have been inert on a
line list — reading as the fix while doing nothing.

## Motion and selection feedback

Both are **presentation**, and the ownership rule is the same one shading
follows: nothing here is truth, nothing mints a revision, and nothing may be read
back out.

**Selection feedback is renderer-owned and per body.**
`forgeshape_selection_pulse.{h,cpp}` owns the peak alpha (**0.55**), the decay
(**220 ms**, smoothstep, monotone and bounded), the resting alpha (**0**, since
`SEL-OUT-R1` — see below) and
one pure function over an explicit frame delta; a delta over 100 ms is a resume or
a stall and is clamped. A pulse acknowledges a **change in selection truth**, not
a tap, and the tint hue is unchanged by it. It holds no `ObjectId`,
reads no clock and touches no `MeshStore`, so a whole selection cycle beside a
published mesh leaves that mesh bit-identical *structurally* rather than by
promise. `Renderer::advanceSelectionFeedback` runs once per frame from
`drawFrame`, deliberately **outside** `syncScene`'s revision gate — a pulse has
to keep decaying on the frames where nothing was published, which is nearly all
of them — and writes one float into the `BodyRenderResources` entry that already
keys that body's GPU buffers by stable `ObjectId`. That keying is what makes A's
pulse structurally unable to reach B's. The renderer is still never told *which*
object is selected: `SceneDrawItem` carries a plain bool per item and identity
stays with `SelectionController`.

### The selection outline (`SEL-OUT-R1`, UI-OWNER-10 / UI-OWNER-11)

**The pulse is now the whole tint.** UI-R1C1 cut the legacy 0.55 flood to a 0.20
resting tint; `SEL-OUT-R1` takes it to **zero**, because a whole-object wash is a
whole-object wash at any strength. Persistent selection is the Objects capsule
plus an outline, and the tint's only remaining job is the 220 ms
acknowledgement.

**The outline is a true silhouette, and it is derived from the geometry the GPU
already holds.** Two steps per frame, and only while a drawable body is
selected:

1. **the mask pass** — a render pass of its own, recorded *before* the frame's
   pass begins, over an `R8_UNORM` colour attachment and its own depth image at
   the render extent. Every scene body is drawn through a position-only pipeline
   with the *same* cull mode, winding and `proj·view·model` the surface pipeline
   uses; the fragment stage writes one push-constant float — 1 for the selected
   body, 0 for every other. Occlusion is therefore resolved by the depth test:
   what survives is exactly the part of the selected body that is **visible**;
2. **the composite** — a full-screen triangle from `gl_VertexIndex` (no vertex
   buffer), recorded *inside* the main pass after the grid and before the gizmo,
   depth test and write **off**, one blended draw. It discards where the mask is
   inside, and otherwise paints where any of 12 ring taps within the band radius
   is inside.

The pass's `finalLayout` is `SHADER_READ_ONLY_OPTIMAL` and two subpass
dependencies order it against the frame either side, so **no manual barrier
exists anywhere in this path**. The `EXTERNAL -> subpass 0` dependency is what
also settles the frames-in-flight hazard: it orders the previous frame's
composite reads before this frame's mask writes, so one shared mask image is
correct with two frames in flight.

**Why a mask and not a shell.** A normal-extruded shell was the other candidate
and is wrong for this product: `RenderMeshCache` splits normals at every hard
edge, so a Faceted box has no shared corner normal to extrude along and the
shell opens a gap at every corner of the primitive the product is most used on.
A screen-space mask has no such failure mode.

**Representation neutrality is structural, not a branch.** The mask pass binds
each body's own device-local vertex and index buffers — the ones `recordBodyDraw`
binds a few instructions later — so Construction, Imported Mesh, Sculpt and CAD
are correct without the renderer asking what a body is. A selection change is one
push-constant float per body: no rebuild, no upload, no regeneration.

`forgeshape_selection_outline.{h,cpp}` is the platform-neutral policy, and holds
no Vulkan: the width (a fraction of the viewport's short side clamped to
`[2, 5]` px — the **camera is not an input**, which is what makes the band stable
under zoom), the per-ground colours chosen by `viewportBackgroundIsLight` alone,
the WCAG arithmetic the self-test measures them with, and
`selectionOutlineCoverage` — the CPU **reference** implementation of the edge
rule that `shaders/outline.frag` mirrors, exactly as `grid.vert` mirrors
`kGridDepthNudge`. Width and colour are **policy, not preference**: no control
for either exists.

**The band is drawn outside the silhouette**, never inside. An inner band sits on
the body's own pixels and, on a small or thin body, covers all of them — the
full-object fill this stage exists to remove. The cost is that a selected body
reads a few pixels larger, which is what every tool that draws one accepts.

**Lifetime.** Shaders, sampler, descriptor set layout, pool, the one descriptor
set and both pipeline layouts are **device-scoped**; the two images, the render
pass, the framebuffer and both pipelines are **swapchain-scoped**. The descriptor
set is rewritten, not reallocated, on an extent change, so the descriptor
allocation count is fixed for the life of the device. The mask pass carries its
own depth image rather than sharing the main pass's: sharing would put a
write-after-write hazard between the mask's depth writes and the main pass's
depth clear, which the main render pass's existing external dependency does not
cover, and widening a dependency in the pass every frame depends on to save an
image freed with the swapchain is the wrong trade.

**The toggle is the grid's.** `DisplaySettingsStore::selectionOutlineVisible` is
process-scoped, session-only, native-owned and rides in the same
`ViewportDisplaySettings` snapshot, so the render thread reads it once at a known
point and cannot record half an outline. It is **not** an `AppPreferences`
field — the Settings page owns persistent preferences, and a transient viewport
overlay belongs with the other transient viewport overlays. With it off the
renderer records neither pass; `selectionOutlineActive()` is one boolean, asked
before any pass is begun.

`Renderer::selectionOutlineStats` is a bounded diagnostic seam carrying a count,
an extent and a width — no `ObjectId`, no geometry, no dimension — mirrored out
of the render thread beside the device-rebuild count. It exists so "repeated
selection switches allocate nothing" is asserted rather than inferred.

**Motion is a shared helper, not a system.** `ChromeMotion` (Java) owns the
decisions and nothing may be added to it that describes motion as data: the fade
durations (**120 ms** arriving, **90 ms** leaving), the anchored-growth durations
(**190 ms** arriving, **150 ms** leaving), the one easing curve
(`PathInterpolator(0.23, 1, 0.32, 1)` — an ease-out, built lazily so the JVM test
source set can load the class), the one start scale (**0.96**, both axes), the
reduced-motion question, cancel-first, and one alpha helper.

**Every surface that grows out of a control is an `AnchoredSurfaceView`**, the
single owner of that growth: `ObjectsPopoverView`, `AddPrimitivePaletteView`,
`PropertyInspectorView`, `DisplaySettingsPopoverView`. A subclass answers only
`anchoredToTrailingEdge()`; durations, curve, uniform scale, cancel-first,
reduced-motion and the pivot it inherits and may not re-decide. **The pivot waits
for a size:** it is expressed in the surface's own bounds, so an open before the
surface has ever been laid out is *staged* — start transform applied, made visible
so the traversal measures it — and the growth starts from `onSizeChanged`. Without
that the first open of a surface in a process grows from the top-left of a
zero-sized box rather than from the control that opened it, and every later one is
correct, which is why a per-surface fix is not enough.

Three rules constrain every caller. **Nothing on the path of a pointer sample:** a
chrome transition never runs while a viewport gesture is in flight, because input
responsiveness outranks motion, and `setMotionAllowed` is pushed to all four
anchored surfaces. **Nothing that changes a size:** chrome hide/restore is alpha
only, because the `SurfaceView` is full-bleed and a transition that changed a size
would rebuild the swapchain, and every anchored surface animates **alpha and
uniform scale about the corner it grew from**, never a height that would
`requestLayout` per frame and re-run the whole adaptive decision. **Only a user
act animates** — the instant, idempotent paths are what the measure pass and every
state refresh call, which is also why the Objects re-parent does not animate.

**Reduced motion crosses the boundary as one bool.** The Android layer reads
`ANIMATOR_DURATION_SCALE`, decides what it means, and hands the answer to
`DisplaySettingsStore::setReducedMotion`, which carries it to the renderer in the
same per-frame snapshot as the shading model — the same seam shape as
`ViewportBackground`: native code is given the *meaning*, never the platform
value. It deliberately does not advance the store's `changeCount_`, which exists
to prove a display transition the user chose. `ChromeMotion.duration` returns
**0** rather than a small number, and every caller branches on that zero to land
directly on the final state; in the viewport, reduced motion runs no pulse at
all and lands on the resting state at once — which since `SEL-OUT-R1` is no tint,
because the outline already says what is selected without taking any time.

## Threading

- Android UI thread: surface callbacks, `onTouchEvent` → JNI, both panels and
  both Applies, Construction publication, every brush stroke and its per-move
  sculpt publication, and DEBUG mesh fixture publication. `ConstructionObject`,
  the scene, the history and `SculptSession` are mutated from this thread only,
  which is why none of them carries a mutex of its own; `MeshStore` has one and
  is what the render thread reads.
- Render thread (owned by `forgeshape_jni.cpp`): Vulkan work, presentation and
  every mesh buffer create/copy/destroy.
- Autosave worker (`AutosaveController`'s `forgeshape-autosave` `HandlerThread`):
  the one thread other than the UI thread that READS Construction, scene and
  sculpt state — `projectFingerprint()` and `encodeProject()` both run there,
  under `g_stateMutex`. That reader is why every JNI entry point that writes
  Construction state (a shape Apply, a placement Apply, a history step, a load,
  an import, a mode change) takes `g_stateMutex` across the write, mesh
  generation included: a parameter written unlocked is a parameter a checkpoint
  could read half-written. The same reader is why every READER of the sculpt
  session takes it too: `sculptSession()` re-points the session at the active
  body on every call (a write to its borrowed target), so every call of it in
  `forgeshape_jni.cpp` is inside a `g_stateMutex` scope — a multi-field read
  such as `sculptState` copies under one hold and writes the JNI array after
  it, the publish helper takes the session its caller already holds, and a
  helper that reads it on its caller's lock is named `...Locked`
  (`artifacts/post-audit-harden-r1/tools/lock_context.js` checks the rule).
- One short-lived thread per DEBUG stress run, which publishes CPU revisions and
  exits. There is no general task or job system.

`MeshStore` has its own mutex, held only long enough to swap or copy a
`shared_ptr` — never across a GPU copy, and never together with the camera and
selection mutex in a way that could invert. A publisher never blocks a frame, and
the render thread's mesh upload happens outside `g_stateMutex` entirely.

`g_stateMutex` guards the single `CameraController`, `SelectionController` and
the scene — every body's Construction Source, placement and sculpt safety state,
and the history — together, plus the gesture-routing flags. A tap resolves
its pick against the camera snapshot *and* the transform and updates the selection
under **one** lock hold, so they can never be seen out of step; the render thread
takes the camera snapshot, the derived model matrix and the selection flag under
the same lock immediately before each `drawFrame()`. The display snapshot (shading
model, surface shading, background, grid visibility, reduced motion) is read from
plain atomics rather than under that lock, because no combination of those values
is invalid to draw.

## Lifecycle contract

The `CameraController`, the `SelectionController`, the `MeshStore`, the
`ConstructionObject` and the `SculptSession` are all process-scoped and outlive
every Surface: camera pose and projection, selected `ObjectId`, the active
primitive, its authoritative parameters and placement, the current mesh revision,
the active product mode, the active tool, the display settings and every sculpted
vertex survive home/resume and swapchain recreation. There is no rehydration step,
because nothing was discarded — the GPU mesh buffers, the MatCap and the grid are
device-scoped and are not destroyed when the Surface goes away, so a resume
re-presents the same revision without re-uploading it. Across a process restart
only what a Save put in the manual slot or autosave put in the recovery
checkpoint survives — the `.forge` document, never presentation or session state
— and the Construction history dies with the process along with everything else.

Gesture tracking is separate: camera anchors and tap candidacy are both reset on
`ACTION_CANCEL`, on `surfaceDestroyed` and on `surfaceCreated`, and a live brush
stroke and any pending one are cancelled with them — so a Surface swap can never
leave a stale touch anchor, a half-finished tap or a half-finished stroke behind,
and never disturbs what is selected or what has been sculpted. Neither camera
state nor selected identity is stored inside swapchain or surface resources.
`surfaceDestroyed` blocks until native code has released the `ANativeWindow`, so
the render thread can never touch a destroyed window.

## Export coordinates

The exporter performs **no coordinate conversion**, because there is nothing to
convert. ForgeShape's world convention and glTF 2.0's are the same convention:

| | ForgeShape | glTF 2.0 |
| --- | --- | --- |
| Handedness | right-handed | right-handed |
| Up axis | +Y | +Y |
| Linear unit | metres (`Meters`) | metres |
| Matrix storage | column-major, `m[column * 4 + row]` | column-major |
| Front face | counter-clockwise from outside | counter-clockwise from outside |

So `1.0` ForgeShape metre is `1.0` glTF metre, and a conversion node, a
transposed matrix, a global scale factor or a reversed index triple in an export
would each be a **defect**. Because the identity is easy to break silently, each
is asserted against by name — see the table at the end of
`artifacts/e2er1c/COORDINATE_AUTHORITY.md`.

What the file *does* carry, and cannot avoid carrying, is the crease-policy
normals and the render-only vertex duplication that produces them, because those
are the surfaces — see *Derived render geometry and shading*. Two-sidedness
travels as `material.doubleSided` rather than as duplicated geometry: a Vulkan
pipeline has one cull mode for the frame and a file has one flag per material,
and the flag is the honest form.

### The baked static transform (ARCH-OWNER-07)

The placement is **split**, not copied whole:

```
Model = T · L        where   L = Rz · Ry · Rx · S
```

`L` is baked into the geometry — every exported position is `L · p`, every
exported normal is `normalize(transpose(inverse(L)) · n)`. `T` stays on the
node, written as a glTF `translation`; the node carries **no rotation, no scale
and no matrix**, so every consumer reads identity for both.

The normal matrix is the product's existing `ConstructionTransform::normalMatrix()`
(`R · S⁻¹`), which for `L = R · S` with orthonormal `R` and diagonal positive `S`
**is** `transpose(inverse(L))` — proved numerically in the self-test against
`inverseModelMatrix()` rather than assumed, so there is still one normal
authority and not two. Carrying a normal by `L` instead is the classic error and
is invisible to a length check or an outward-facing check: what it destroys is
perpendicularity to the surface, which is what the tests assert.

Why this shape rather than the node matrix it replaces (both are valid glTF, and
both place the object identically): a static asset arriving with a live rotation
and a non-uniform node scale is an object whose every downstream operation — a
modifier, a boolean, a physics shape, a normal recalculation, a second export —
has to keep re-deriving the shape the user actually made. Baked, the shape
simply *is* the mesh. The owner reviewed the pre-bake files in Blender and chose
this.

Four things the split deliberately does not do:

- **It does not recentre.** `L` is applied about the body's LOCAL ORIGIN, and
  `T` then positions that same origin, so the pivot a body is drawn and rotated
  about is the pivot it exports with. A cone is what the tests use to prove it:
  a box, plane or sphere is centrally symmetric, and rotating a centrally
  symmetric point set leaves its bounding box symmetric too — so a
  recentre-on-bounds would be invisible in any of them.
- **It does not merge.** Each body bakes its own `L` into its own mesh and keeps
  its own node, in scene order.
- **It does not mirror.** `det(L) = sx·sy·sz > 0` for the strictly positive
  scale domain, so winding survives the bake unchanged. A zero or negative
  determinant is refused — `SingularTransform`, `MirroredTransform` — never
  compensated by reversing every triangle, which would be implementing a Mirror
  the domain says cannot exist.
- **It does not reach `.forge`.** A project still stores the nine authored
  values. Baking is an interchange decision and lives only in the exporter.

### Representation, and why the exporter never guesses

A Construction project exports re-evaluated Construction geometry. A Sculpt
project exports, **per body**, the Frozen Sculpt Mesh if that body has one and
its Construction geometry otherwise. There is no fallback in the other
direction: a body whose representation is Sculpt is never exported from its
Construction Source, because that would silently ship the shape the user
abandoned. `captureGlbExportScene` decides this once, from
`ProjectKind` and `frozen.mesh.frozen()`, and `encodeGlb` never revisits it.

`encodeGlb` also validates what it was handed — every matrix element finite,
every mesh non-empty, index counts whole triangles, every index in range —
because a JSON document containing `nan` parses, passes a length check and is
still not a glTF file. Refusing is the only correct answer and it is the
writer's job, not the caller's.

## Reading a `.glb`

One parser, two destinations. `GLB-IMPORT-R0` (`ARCH-OWNER-08`) and
`GLB-IMPORT-R1` (`ARCH-OWNER-09`) built the reader and a session-only preview to
answer a diagnostic question. `IMPORT-01A` (`ARCH-OWNER-10`) sends the SAME parse
somewhere else as well: into durable project objects the user keeps.

The parser is unchanged by that and knows nothing about it. What decides what a
file becomes is which of the two commit paths the bytes are handed to, and only
one of them — durable import — is reachable from the product.

## Durable import

`IMPORT-01A` under `ARCH-OWNER-10`. The user's *Import GLB…* reads a file and
creates real bodies: an `ObjectId` from the scene's own allocator, a row in the
Objects list, the ordinary Move/Rotate/Scale gizmo, one Undo step for the whole
import, and geometry the `.forge` document carries so the project reopens without
the source file ever being consulted again.

### A body has three possible source representations

`SceneObject` owns exactly one of them for its whole life, named by
`BodyRepresentation`:

- a **Construction Source** is a primitive plus its parameters, and its geometry
  is DERIVED — `generateMesh()` regenerates it on every load, so a `.forge` file
  stores parameters and no vertices;
- an **Imported Mesh** IS the geometry. No rule could recreate it and the `.glb`
  is not part of the project, so it is project truth and it is serialized;
- a **CAD Body** is a sketch and one extrusion (`CADB`), regenerated through
  `generateCadMesh` — see *CAD domain* above.

There is no conversion between them and no fabrication in any direction:
nothing invents a primitive an imported object was never made from, and no
Construction Body becomes one. `IMPORT-01B` does not weaken that -- it lets a
Construction Body or an Imported Mesh be sculpted, and a Frozen Sculpt Mesh is a
body's SECOND representation rather than a change of its first (a CAD Body has
none yet). `constructionOrNull()`, `importedOrNull()` and `cadOrNull()` are
POINTERS precisely so the compiler asks every call site what it does about a
body of another kind, and the process-scoped accessor is
`activeConstructionOrNull()` for the same reason.

The **placement is the BODY's**, hoisted out of `ConstructionObject` by
`IMPORT-01A`: a body has one because it is a body, not because it is a primitive.
That is what lets the gizmo, the picker, the exact-value editors, the history and
`SCNE` all treat an imported object exactly as they treat every other one, with
no second transform path.

`publishSceneObject` is the ONE dispatch point for what a body DRAWS: a
Construction Body regenerates from its parameters, an Imported Mesh republishes
the geometry it owns. Neither path reads the other's truth.

`buildSculptSourceMesh` is the second one, added by `IMPORT-01B`, for what a body
is SCULPTED FROM -- see *Sculpting an Imported Mesh* below.

### The commit is atomic, and it is one transaction

`forgeshape_import_commit.{h,cpp}` turns a `ParsedGlbScene` into bodies. Every
object is built and validated OFF the scene first; only when all of them exist
does anything reach `ConstructionScene`. A file describing four good meshes and
one broken one is not four fifths of a project, so a refusal costs nothing at
all: no `ObjectId` minted, no body appended, no history step, no fingerprint
movement. The whole commit runs inside ONE `ScopedConstructionEdit`, so an import
of forty objects is exactly one Undo — the rule Add Primitive already follows for
the two mutations it is made of.

Undo and redo of an import work through the machinery creation already had: the
history detaches the bodies and HOLDS them, so a redo returns the same objects
with their geometry intact rather than fresh ones wearing their ids. A history
step carries a body's identity, representation and placement and never its
vertices — an imported mesh could not be copied into a step cheaply and must not
try.

**Import is a Construction act, and Sculpt refuses it** (`FUNCTION-COUNCIL-C1`
D1). The commit makes its first body ACTIVE and records a Construction step,
and in Sculpt the active body is the sculpt target and Construction history is
refused. `commitImportedGlbScene` is platform-neutral and cannot see the mode,
which lives in the Sculpt session. So `importGlbDurable` asks
`sculptSession().inSculptMode()` under the same lock as the commit and answers
`ImportCommitStatus::RefusedInSculpt` (appended, ordinal 5) without touching the
scene or either history. The same pattern guards add, delete and the object
commands. Above JNI the import row is ABSENT in Sculpt
(`ProjectActionsPopoverView.showImportAvailable`, from native `productMode` on
every refresh). `onImportGlbRequested` and a picker result that arrives in
Sculpt refuse before the file is read, and no Java mode flag exists. There is
no automatic exit from Sculpt and no repair after the fact: the import does
not happen.

### The transform split

`Model = T · L`. glTF states a node transform this product cannot store: its
linear part may carry rotation, non-uniform scale and shear, and ForgeShape's
placement is nine authored values with a strictly positive diagonal scale. Rather
than decompose it — which for a sheared node has no correct answer — the linear
part is BAKED into the object's local geometry and the translation becomes the
body's placement. An imported body therefore arrives at `rotation = 0,0,0` and
`scale = 1,1,1`, and every later Move/Rotate/Scale is an ordinary ForgeShape
transform over that.

Nothing is recentred: the object's origin is the node's own origin, which is the
pivot every downstream tool inherits. The parser already bakes the whole node
transform into world positions, so the split is `local = world − translation` —
the translation is applied last in the same composition, so taking it back off is
exact rather than a re-derivation.

### Naming

Node `name`, then mesh `name`, then the deterministic `Imported <n>` fallback,
where `n` is 1-based within that import and never an `ObjectId` — an id is minted
and would make the same file produce different names in different sessions.

A name from another tool is arbitrary bytes, so it goes through
`sanitizeImportedMeshName` before it can become project truth: control characters
replaced, malformed UTF-8 dropped, trimmed, cut on a character boundary at 96
bytes. The function is idempotent, which is what lets the `.forge` decoder state
its rule as "the stored name is what this would produce" rather than as a second
list. `BodyLabels` is the one place the UI asks what a body is called: an
Imported Mesh uses its stored name, a Construction Body is still `Body #id`, and
the empty string native code returns for the latter is the signal for that
fallback rather than a name.

### What an imported body is NOT

- **Not parametric.** No `PrimitiveKind`, no dimensions, no remembered parameter
  sets, and nothing may reconstruct one. `Shape` has no answer for it, so the
  rail entry is absent for one and `applyConstructionPrimitive` refuses it below
  JNI as well — withdrawing a control is not removing a guard.
- **Sculptable since `IMPORT-01B`**, and it was not before. Start Sculpting is
  offered for one and `freezeToSculpt` accepts it, seeded from its own geometry.
  What that does NOT do is give it parameters: see below.
- **Not appearance.** The parser validated COLOR/TEXCOORD and the material's
  colour, roughness, metallic and textures, and decoded none of them. They were
  never read, so nothing preserves them. `doubleSided` is the single exception,
  and it is carried per SUBMESH because it changes which triangles are VISIBLE.
  `buildDrawData` resolves it into geometry by emitting a reversed copy of a
  two-sided submesh's triangles, which is what keeps the answer per submesh
  through a published mesh whose own flag is one answer for the whole thing.

An imported body **is** exported: `captureGlbExportScene` reads its CPU arrays
through the same `buildDrawData` and bakes `L` exactly as it does for every other
body. A `.glb` quietly missing an object the user can see, select and move is the
one thing that exporter refuses to do anywhere else.

## Sculpting an Imported Mesh

`IMPORT-01B` (`ARCH-OWNER-11`) lets either representation be sculpted. It builds
no second sculpt subsystem: the mode, the four brushes, the stroke kernel, the
adjacency, the stale-source rule, the destructive-reset guard and the `SCUL`
branch of the document are all exactly the ones Construction sculpting already
used. What was added is **source acquisition** and the wording around it.

### One dispatch point for the seed

`buildSculptSourceMesh(body, out)` in `forgeshape_scene.{h,cpp}` is the whole
addition, and it sits beside `publishSceneObject` for the same reason: a caller
should not have to ask what a body IS before it can ask for something to sculpt.
A Construction Body regenerates from its parameters through the same generator
the product publishes from; an Imported Mesh hands over the arrays it already
owns. Either way the source is only READ — no primitive parameter, no placement
and no imported vertex is written by a freeze, then or ever.

It returns a `ConstructionMesh` because that type is already this codebase's
plain carrier for "vertices, indices and a sidedness answer": `.forge`'s sculpt
restore has built one out of stored bytes since E2E-R1A. It carries no
Construction meaning there, and nothing about the returned mesh claims the body
has a Construction Source.

Two decisions inside it are load-bearing for an imported source:

- **The RAW index array is copied, never `buildDrawData`'s.** That one emits a
  double-sided submesh's triangles a SECOND time with reversed winding, which is
  right for drawing and ruinous for sculpting: the reversed copy contributes the
  exact negation of its twin to every area-weighted vertex normal, so a two-sided
  submesh would freeze with zero normals and no normal-based brush could move it.
- **Per-submesh `doubleSided` therefore collapses into the mesh's one
  `renderBothSides`,** true when ANY submesh is two-sided. A frozen mesh has a
  single sidedness answer by construction, and the safe collapse is the
  permissive one: it keeps an imported sheet both visible and reachable by a
  brush from behind, where the strict one would leave half of what the user
  imported untouchable.

The file's own stated normals are deliberately not carried across. A Frozen
Sculpt Mesh derives its normals from its CURRENT positions because a stroke moves
them — already true of every Construction freeze — and the Imported Mesh keeps
its own normals untouched, so *Back to Imported Mesh* shows them exactly as the
file stated them.

### The placement is not baked twice

Both representations are LOCAL geometry under the one authored
`ConstructionTransform`. The import already split the node transform — linear
part into the geometry, translation onto the body — and the freeze copies local
positions unchanged, so the same model matrix carries the imported point and the
seeded point to the same place. Nothing is recentred and nothing is re-baked.

### The source stays immutable

An `ImportedMesh` has no mutation path at all: it is built once, validated once,
and read thereafter. So an imported body's `sourceStale` flag is never set —
there is no source change for it to record — and the stale-source warning simply
never appears over one. That is a consequence of the representation, not a branch
in the UI.

### What the user reads

The act is the same act and the code is the same code; the WORDS differ because
the destination does. Over a body whose geometry came from a file the toolbar's
context reads *Imported Mesh*, the way out of Sculpt reads *Back to Imported
Mesh* (short form `← Imported Mesh`, with the full wording always kept as the
content description), and the destructive reset reads *Reset Sculpt from Imported
Mesh…*. *Start Sculpting* and *Resume Sculpt* are unchanged, because those name
what the user does rather than where the geometry came from. The view id of the
way out is still `back_to_construction`: an id names the ACT the code performs —
leaving Sculpt and republishing the body's source — and copy never renames one.

### `.forge`

A `SCUL` entry's body may now have either source. Which one it was frozen from is
not stored, because nothing reads it back. The exactly-one-of rule is untouched:
it is about `CONS` versus `IMPT`, the two things a body's geometry can come FROM,
and `SCUL` is not one of them. `DATA_PACKAGE_SPEC.md` owns the layout and the
compatibility direction, which is fail-closed: an older build refuses an
`IMPT`+`SCUL` file rather than opening half a body.

## Deleting a body

`UI-OWNER-45`. `forgeshape_body_delete.{h,cpp}` owns the operation, as its own
small module for the reason `forgeshape_import_commit` is one: deleting a body is
a decision ABOUT the project that needs both the scene and the history, and
neither of those owns the other.

**Representation-neutral by construction.** Nothing in it asks what a body is. A
Construction Body, an Imported Mesh, and either of them carrying a retained
Frozen Sculpt Mesh are removed by the same three lines, because a body is removed
as a whole object: its identity, its representation, its placement, its published
mesh and its sculpt state leave together and come back together. A
per-representation delete path is exactly how one of them would eventually come
back missing something.

**The body is not destroyed.** It is detached and handed to
`ConstructionHistory::holdDetachedBody`, which keeps it for as long as some step
still names it. That is not an optimization: an Imported Mesh's geometry and any
Frozen Sculpt Mesh are not derived from anything a step holds, so a step could
not rebuild them. Undo restores the SAME object; a redo removes that same object
again.

`pruneDetachedBodies` was widened to match. It asks both stacks and BOTH sides of
every step, because a redo restores an undone creation from a step's AFTER state
and an undo restores a deleted body from a step's BEFORE state. Asking only the
forward side was correct while an undone creation was the only way a body could
leave the scene.

**One Delete is one transaction**, through the ordinary `ScopedConstructionEdit`
and always as the scope that OWNS the edit: an edit already in progress is
refused outright (`RefusedEditInProgress`), the same rule `loadProjectDocument`
and `commitImportedGlbScene` apply. Nothing in the product wraps a delete in a
larger act, and refusing rather than joining keeps the boundary unambiguous.

**Selection.** If the deleted body was not active, the active one does not
change. If it was, the selection falls to the NEXT body in scene order, or — when
the deleted body was last — to the one before it. Scene order is the Objects
list's order, so what the user sees selected afterwards is the row that took the
deleted row's place.

**The last body is refused by name.** A project is never empty: a project
scene creates a body eagerly, `validateProjectDocument` refuses a file with zero
bodies, and since `APP-H1` an EMPTY scene means "no project is open" — Home —
which Delete must never reach. `RefusedLastBody` says so and changes nothing;
no replacement primitive is ever invented. The one way to an empty scene is
`closeProject`, and that is leaving the project, not editing it. **Refused while sculpting** too, on
the same terms body switching and Undo/Redo already follow: the Sculpt target is
fixed for the duration of the mode, and Undo is refused there, so a delete made
there could not be taken back until the user left.

The `ObjectId` allocator is never rolled back. A deleted body's id comes back by
NAME through its undo, and the next creation mints a fresh one.

### Renderer resources

`Renderer::releaseBodiesAbsentFromScene`, called at the top of `syncScene`, frees
the GPU copy held for any body the current scene no longer names. Until Delete
existed this was not needed: a body could only leave the scene by having its
creation undone, the history holds at most `kConstructionHistoryCapacity` of
those, and each is a handful of kilobytes. Delete removes that bound — add and
delete in a loop and every cycle mints a fresh `ObjectId`, because the allocator
is deliberately monotonic, so the map would grow one entry per cycle forever.
Entries are small, but each holds two `VkDeviceMemory` allocations and
`maxMemoryAllocationCount` is a hard device limit commonly around 4096.

Undoing costs one re-upload and nothing else: the body comes back with the same
`ObjectId` and the same published revision, a fresh resource record starts at
`kNoMeshRevision`, and `syncBody`'s gate misses and uploads it again — the same
path a body takes the first time it is drawn. A steady frame pays one walk of a
map with a handful of entries and nothing else.

The in-flight wait before anything is destroyed is **bounded**
(`kMeshReleaseWaitNanoseconds`, 100 ms), and that is not a detail. This release
is opportunistic and retried every frame, so it must never stake the render
thread on a fence that may never signal: a failed `vkQueueSubmit` leaves that
frame's fence reset with nothing left to signal it, an unbounded wait there hangs
the render thread, `surfaceDestroyed` blocks on that thread, and the Activity
never tears down. On anything but success the release leaves every entry resident
and asks again next frame. A capacity GROW keeps the unbounded wait, because it
must complete before it reallocates.

## The object commands: Rename, Show/Hide, Lock/Unlock, Duplicate, Mirror

`UI-OWNER-40`, Stage 018A, and `MIRROR-01`. `forgeshape_body_commands.{h,cpp}`
owns all five, as its own module on exactly `forgeshape_body_delete`'s terms and
for the same reason: each is a decision ABOUT the project that needs both the
scene and the history, and neither of those owns the other. Delete stays where
it is and was not touched. Mirror's ARITHMETIC is not here either -- it is a
pure function over values in `forgeshape_body_mirror.{h,cpp}`, the same split
`forgeshape_body_dimensions` uses.

**Three of the five are representation-neutral by construction.** Nothing in
Rename, Show/Hide or Lock/Unlock asks what a body is, because a body has a name,
a visibility and a lock *because it is a body* — exactly as it has a placement
because it is a body. `SceneObject` carries all three beside `transform_`, and
that is the whole of it. Duplicate is the one that dispatches on
`BodyRepresentation`, because it has to COPY a representation; it dispatches
through the existing enum rather than through a new boolean. Mirror is the one
that REFUSES on representation, and deliberately -- see below.

**Each act is one transaction**, through the ordinary `ScopedConstructionEdit`
and always as the scope that OWNS the edit: an edit already in progress is
refused (`RefusedEditInProgress`), the rule `deleteSceneBody` and
`loadProjectDocument` already apply. One Rename is one Undo; so is one toggle,
one Duplicate and one Mirror. An act that changes nothing records nothing and
leaves the redo stack alone — that is `commitEdit`'s existing comparison, and
none of these restates it.

**Undo and Redo needed no per-command inverse.** The Construction history is a
scene SNAPSHOT, so adding `name`, `visible` and `locked` to
`BodyConstructionState` — captured, compared and restored beside `transform` —
is the entire implementation of undoing all three. `applyState` restores them
unconditionally rather than behind a "differs" test, because all three are plain
assignments with no derived work behind them: no mesh to republish, no adjacency
to rebuild, no revision to mint.

### Hidden is enforced in exactly one place

`ConstructionScene::snapshot` skips a body that is not visible, and that is the
only place visibility is read. It is one place on purpose: this snapshot is what
the renderer draws AND what CPU picking casts against, so leaving a hidden body
out of it makes "not rendered" and "not pickable" the same fact rather than two
predicates that could drift apart. The selection outline follows for free — the
mask pass rasterises the snapshot, so a hidden body cannot leave a stale
silhouette behind, and no renderer branch was added.

Hiding is not deleting. The body keeps its row, its selectability from that row,
its published revision, its `.forge` record and its export; showing it again
costs no republication. Hiding the ACTIVE body is allowed and the selection does
NOT move — the smallest coherent behaviour, because the row stays selected so
Show is one tap away, and nothing downstream needs an active body to be
drawable.

### Locked stays visible and stays pickable

What lock removes is the ability to be MOVED, and it is enforced by two named
guards rather than by a missing control:

* `setGizmoActive` refuses over a locked active body and turns the gizmo OFF —
  which also cancels a captured handle, so a finger already dragging stops
  (`FORGESHAPE_GIZMO_REFUSED:body_locked`);
* the transform entry point rejects the write
  (`FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:ui:BodyLocked`).

The refusal carries its own JNI code, `APPLY_REJECTED_LOCKED`, reported through
an out-parameter rather than through `TransformValidation`, because a lock is
not a statement about a VALUE: every number the caller passed may be perfectly
good, and a status line that blamed a coordinate would describe the wrong
problem. The workspace withdraws the transform controls as well, on the standing
rule that a control which cannot succeed is not drawn — and the guards stay
regardless, because removing a control is not removing a guard.

A locked body is deliberately still selectable and still pickable: a body you
can see and select but not move is what a lock means to a user, and one that
silently stopped responding to taps would read as a rendering fault. Reaching
Unlock therefore needs no special path. **Delete is unchanged by lock**, which
is `UI-OWNER-45`'s semantics left alone rather than redefined here.

### Rename reuses the domain's one name rule

There is one name policy in this product and Rename is a new way to reach it,
not a second one: `sanitizeImportedMeshName` trims, drops malformed UTF-8,
replaces control characters and cuts on a UTF-8 boundary at
`kMaxImportedMeshNameBytes`, and `importedMeshNameIsStorable` states the check
as "this is what the sanitizer would produce for it". A name that sanitizes to
empty is REFUSED and the body keeps what it had — empty is the UI's signal for
the ObjectId-derived fallback label, and a user must not be able to type their
way into it.

The JNI boundary needed its own half. `GetStringUTFChars` returns MODIFIED
UTF-8, in which a supplementary character is two 3-byte surrogate encodings
rather than one 4-byte sequence; the sanitizer would correctly drop that as
malformed, and an emoji typed into Rename would silently vanish. So
`readJavaString` reads UTF-16 units and encodes real UTF-8 through
`utf16ToUtf8`, the exact inverse of the `utf8ToUtf16` that has carried names the
other way since `IMPORT-01A`. The two live beside each other in
`forgeshape_imported_mesh.{h,cpp}` because they are one boundary rule read in
two directions.

### What a Duplicate copies, and what it must not

Copied: the source representation's own truth (a Construction Source's active
kind and all six remembered parameter sets, an Imported Mesh's positions,
normals, indices and submesh batches, or a CAD Body's sketch and extrusion), the
placement, a deterministic `name copy` / `name copy 2` suffix from the shared
`derivedBodyName` rule Mirror also uses, the visibility,
the lock, and the Frozen Sculpt Mesh's CURRENT positions and topology.

Not copied, and each for its own reason:

* the `ObjectId` — a fresh one is minted, which is the point, and the allocator
  is never rolled back;
* the `SculptHistory` — it is a bounded, volatile, per-body Undo over strokes
  made on THIS mesh (`ARCH-OWNER-12`), and the copy has made none. The clone
  goes through `freezeFrom`, the same entry point a Freeze and a `.forge` load
  use, so an empty history is a property of the construction rather than
  something cleared afterwards. The `hasEdits` FACT is carried, because that is
  a stored fact about the geometry rather than a stack depth;
* renderer resources, published revisions and GPU handles — the copy publishes
  once through the one `publishSceneObject` dispatch and owns its own
  `MeshStore` by construction;
* the Construction history, the selection pulse and the outline.

The copy is appended (scene order is insertion order and this product has no
reorder) and becomes the active body, the same answer Add Primitive and Import
already give.

**A face-supported CAD Body is refused by name** (`RefusedFaceSupportedCad`).
Its world placement is DERIVED from its producer's face frame and is not stored,
so a copy would stand exactly where the original stands, permanently, and
`SceneObject::isFaceSupportedCad` is the very predicate that refuses to let the
user move it apart. A duplicate that can be neither seen as separate nor
separated is not a duplicate. Nothing is retargeted, nothing is detached from
its `TopoRef`, and no dependency is rewritten — the refusal is the whole
behaviour, and it is asked before an id is minted or an edit opened. A
world-plane CAD Body duplicates normally, and so does a PRODUCER that has
dependents: one Duplicate copies one body and never a graph, so the copy is
simply a producer of its own with none.

### Mirror is a proper rotation, and that is the whole design

`MIRROR-01`. Reflecting a Construction Body across one principal WORLD plane —
`XY` reflects Z, `XZ` reflects Y, `YZ` reflects X — creates ONE new body. The
source is not touched: this is a discrete CREATION act and not a live symmetry
modifier, so nothing links the two afterwards, moving one does not move the
other, and no second Mirror is implied by the first.

**A negative scale was never an option.** Scale in this product is strictly
positive by contract (`forgeshape_transform.h`); a negative factor inverts
winding, flips every normal and every front-face test, and would make the glTF
exporter's own `MirroredTransform` refusal a lie. So the reflection is carried
by the ORIENTATION instead. With `Qx = diag(-1, +1, +1)` — the primitive's own
local-X symmetry — and `F` the world reflection:

```
p' = F · p          R' = F · R · Qx          S' = S
```

`det(R') = det(F)·det(R)·det(Qx) = (-1)(+1)(-1) = +1`, so `R'` is a proper
rotation and comes back through `eulerFromRotationMatrix`, the one bridge
between the matrix form and the authoritative Euler degrees, with the source's
own angles as the branch hint. No second Euler convention was introduced.

**The determinant is not the proof.** It only says the result is a legal
orientation. The geometric claim is the identity

```
Model_mirror(q)  ==  F · Model_source(Qx · q)      for every local q
```

which follows because `Qx` and `S` are both diagonal and therefore commute:
`T' + R'·S·q = F·T + F·R·Qx·S·q = F·(T + R·S·(Qx·q))`. It holds for ANY `q`
with no symmetry assumed at all, and `MIRROR01-06` asserts it over probe points
no primitive generates. What the symmetry then adds is that `Qx` maps each
generated local vertex SET onto itself — all six primitives are origin-centred
and their rings carry `kPrimitiveRadialSegments` (32, divisible by four) — so
`WorldGeometry(mirror) == F · WorldGeometry(source)` as a set.
`MIRROR01-07/08` assert both halves over six primitives × three planes × ten
poses, including a body on the plane, a body crossing it, a non-uniform scale
and an angle past a whole turn.

`Qx` is ALGEBRA. It is never persisted, never reaches a `.forge` byte and never
appears in a history step.

**It is the one object command that is deliberately not
representation-neutral.** `mirrorEligibilityOf` refuses an Imported Mesh
(`NotConstruction`), a CAD Body (`NotCad`) and a body carrying a Frozen Sculpt
Mesh (`HasSculptTruth`) BY NAME. The reason is the same in all three: the
reflection is exact only because the body's own geometry is symmetric under
`Qx`, and none of those three is. An imported mesh's triangles would come out
inside-out; a CAD Body's truth is a sketch, and mirroring one would mean
mirroring authored sketch geometry, which this stage does not do; sculpt
vertices are arbitrary. The eligibility is asked BEFORE anything is minted or an
edit is opened, so a refusal costs no `ObjectId` and leaves no empty
transaction, and the reason is reported in `MirrorBodyReport::eligibility`
beside a single `RefusedNotMirrorable` status — one status per OUTCOME, the
reason still named in the log. The row simply has no Mirror control for such a
body (`sceneBodyCanMirror`), and the domain guard stays regardless.

**What the reflection carries**: a fresh `ObjectId`, the source's Construction
Source truth through the same `captureState`/`restoreState` pair Duplicate uses,
the mirrored placement, Stage 018A's Duplicate policy for visibility and lock
(a hidden source produces a hidden reflection, a locked source a locked one, and
the source keeps both), and a `<name> Mirror` name from `derivedBodyName` — the
ONE collision rule, which `duplicateBodyName` now calls with `"copy"` so the two
commands cannot drift into two policies. It publishes exactly once, for itself;
the source is not republished and not re-tessellated, because the reflection
read nothing but nine numbers off its transform.

**One Mirror is one Undo.** Undo removes only the reflection and restores the
previous active body; Redo restores the SAME `ObjectId` with the same placement,
source, name and flags, because the allocator is never rolled back.

**No format change.** A mirrored body is an ordinary Construction body wearing
an ordinary transform, so nothing stores which plane made it and no `.forge`
field, section or version moved. `MIRROR01-12` proves it the only way worth
proving: a project reached by mirroring encodes byte-identically to one whose
second body was placed at the same numbers by hand.

### The row keeps two targets plus one overflow

The Objects panel is 220 dp wide. More 48 dp targets beside the label would
leave the label nothing, and a persistent command column is the desktop shape
this product does not have. So the row stays a pair — the label, which selects,
and Delete, which removes, exactly where `UI-OWNER-45` put them — plus one
overflow that grows a **row command strip** out INLINE beneath its own row. The
strip pushes the rows below it rather than standing over them, so it never
partially covers another live control; at most one row is open at a time; and
Rename replaces the strip with an inline field whose IME Done commits and whose
System Back cancels, costing the project nothing because nothing is a
transaction until it is committed. System Back closes the strip before the panel
that hosts it, innermost outward, so Back stays one step in every phase.

The strip itself wraps to TWO lines — Rename, Show/Hide and Lock on the first,
Duplicate and Mirror on the second. Five 48 dp targets in a row are 256 dp and
the panel offers 192 dp of content width, so wrapping is what keeps every target
at the floor; the alternative is shrinking the drawn boxes, which the floor
exists to prevent. Mirror replaces the strip with a **mirror plane chooser** of
three equal-width `XY` / `XZ` / `YZ` chips, each naming the world axis it
reflects in its content description because two letters read aloud say nothing
on their own. Opening it is a CHOICE and not yet an act: nothing is created, no
step is recorded and no `ObjectId` is minted until a plane is picked, and System
Back closes it exactly as it closes the rename editor.

Both toggles change their GLYPH with the state as well as their words, so what
is hidden and what is locked reads without relying on colour. The overflow and
the strip are withdrawn in Sculpt and while sketching, where every one of the
commands is refused below JNI — and, as everywhere else, the guard stays.

## Body dimensions, the shared resize/anchor solver, and Relative Scale

`UI-OWNER-33B`, Stage 020M. Construction-only.

**A dimension is derived, and nothing stores one.** For a Construction Body's
own axis `a`,

```
dimension[a] = unscaledLocalExtent[a] * absoluteScale[a]
```

The extent comes from `constructionLocalBounds`, which reads the ACTIVE
primitive's parameters — a box's width, a cylinder's diameter, a capsule's TOTAL
height, a plane's exactly-zero thickness — and never measures a generated
vertex. That is the same one-way rule the rest of the domain follows: a mesh is
derived, and reading a dimension back out of one is the loop this project
forbids. It is also why **rotation is not an input**: a world axis-aligned
bounding box of a turned body reports the size of the box AROUND it, which is a
fact about that hull and not about the body. Neither is the camera an input;
nothing in this module projects.

**One solver owns every resize.** `solveAxisResize` and `solveAxisDimension` are
pure functions over values — they take a `TransformValues`, a `LocalBounds`, an
axis, a target and an anchor, and they take no scene, no history, no body, no
`ObjectId` and no camera. That shape is deliberate and is exactly what
`UI-OWNER-33B` asks for: Stage 020D's Directional Scale handles are meant to
drive this same arithmetic from a drag, so there must be nothing about it that
only an exact-value editor could supply. No renderer math owns this contract and
there is no second implementation.

**The anchor arithmetic, stated once.** With `Model = T · Rz · Ry · Rx · S`, a
local point `p` sits at `P = T + R·S·p`. Only `S[a]` changes, so holding the
point at local coordinate `b` on that axis stationary requires

```
T_new = T_old + (R · e_a) · (S_old[a] − S_new[a]) · b
```

`R · e_a` is column `a` of the rotation matrix, obtained through
`rotationMatrixFromEuler` — the ONE bridge between the Euler truth and a matrix
(`forgeshape_transform.h`) — rather than through a second copy of the
convention. The correction does not depend on which point of the face was
chosen, which is what makes "the negative side stays where it is" a statement
about a whole face. `b` is `bounds.min(a)` for the negative side and
`bounds.max(a)` for the positive.

**Centre is a rule, not an inference.** Centre writes NO position: the scale
changes and the placement does not. For today's origin-centred primitives that
coincides with "the bounds centre stays put", but the stated rule is the one
that holds, and `LocalBounds` carries min AND max rather than a half-extent so
the solver never assumes the two descriptions are the same — which is also what
lets Stage 020D reuse it for bounds that are not centred.

**Refusals are by name and never clamped.** Zero, negative, non-finite, an
out-of-range axis, and a DEGENERATE axis — a plane's zero-thickness local Y,
where no scale gives a size — each have their own `ResizeStatus`. No thickness
is fabricated and no division by zero happens; the plane's own width and depth
still resize normally, because it is an axis that is refused and not a body.
Every solution is put through the transform's own `validateTransformValue` /
`validateScaleValue` before it is returned, so a caller is never handed a value
the transform is then going to reject.

**Relative Scale is a temporary multiplier, and it is not a second scale
vector.** It opens at `(1, 1, 1)` every activation for the strongest possible
reason: nothing anywhere stores one. `solveRelativeScale` commits
`newAbsolute = oldAbsolute ⊙ multiplier` with the position untouched — it is
pivot-based in Stage 020M and exposes no one-sided anchor — and fails closed on
all three multipliers at once, on exactly the terms `applyTransformValues`
refuses a bad ninth value. The multiplier never reaches a `.forge` byte, a
history step, a checkpoint or the fingerprint, and there is no accessor to read
one back. `RelativeScaleEditorView` is the only thing in the product that ever
holds one, and it resets to 1 when it opens and again the moment an Apply lands.

**The two product acts are one transaction each.** `applyBodyDimension` and
`applyBodyRelativeScale` sit over the scene and the history in the same module,
on `forgeshape_body_commands`' terms: a shared preamble refuses an open edit,
an unknown body, a non-Construction representation, a LOCKED body (Stage 018A's
guard, because a resize MOVES one) and a HIDDEN body (the leaders are read off
geometry that is not drawn — and the refusal does not touch the visibility the
user set), then one `ScopedConstructionEdit` writes the solved placement.
A commit that finds the placement it already had reports `Unchanged` and records
nothing. Neither publishes a mesh, mints a `MeshRevision`, rebuilds a CAD mesh
or moves a sculpt vertex — a dimension edit is a transform edit, however it was
expressed.

**No persistence change.** Because the whole act writes Position and Scale, the
`.forge` document is untouched: no field, no section, no version. `DIM020M-16`
proves it directly — a project reached by a dimension edit encodes
byte-for-byte identically to one reached by typing the same numbers into the
placement editor — and the thirty-fixture corpus digests are unchanged beside
it.

**The leaders are the sketch overlay, reused.**
`forgeshape_body_dimension_overlay.{h,cpp}` builds a `SketchOverlay`: a
world-space `GizmoVertex` line list in two of the ranges the renderer already
draws — the active axis in `Dimension` (the annotation highlight) and the other
two in `Entities` (the neutral weight). It becomes the third producer for the
one overlay slot the JNI frame loop fills, beside the support chooser and the
sketch session, and **the renderer needed no change at all**. The geometry is
the body's own local bounds carried through its placement; the stand-off that
holds the annotation clear of the body is applied in WORLD units along the
body's own UNIT axes, so a large scale does not push the leaders away and a
small one does not bury them. The overlay is rebuilt only when the bounds, the
placement, the active axis or the camera scale actually changed, so a resting
frame costs no transfer.

**Dimensions mode withdraws the gizmo, below JNI as well as above it.** The
leaders and the transform handles are two instruments for one placement, and a
mode whose whole point is an exact typed value must not also invite a drag. The
session turns the gizmo off when it opens; the frame loop answers again every
frame, so a mode entered mid-drag cannot leave one standing. The mode also
CLOSES ITSELF the moment what it measures stops being measurable — another body
selected, this one locked, hidden or deleted, Sculpt entered, the project closed.
`activeBodyDimensionsEditable` is that rule, named ONCE, and it is asked by the
frame loop and by every chrome read alike through `settledBodyDimensionSession()`
(`UI-3D-STATE-C1`). Stating it in one place is what makes "the shell's next
refresh finds the mode shut" true: while the render thread owned the rule alone,
a refresh taken at the instant of the transition — which is exactly when the
shell refreshes — still saw the mode open, and drew three numbers over a sculpt
session.

**The label anchors are DERIVED when asked, never cached.**
`bodyDimensionLabelAnchors` is a pure read over bounds, placement and camera
scale, implemented through the same builder that draws the leaders so there is no
second copy of the midpoint arithmetic, and the session holds no anchor field at
all. A cache filled by the render thread is a frame behind by construction, which
is a defect twice over: on the frame the mode opens there is nothing in it yet, and
on the frame after a body switch what is in it belongs to the previous body. An
anchor does not depend on the active axis — that decides which range a leader is
emitted in, not where its midpoint is — and `DIM020M-17` pins that, because the
anchors-only read is sound only while it holds.

**The numbers are chrome.** `BodyDimensionLabelsView` draws three labels over
the viewport, each centred on the projected midpoint of its own real dimension
line (`bodyDimensionLabelPoint`), because a number has to stay upright at any
zoom and be typeable. It is the same split, and the same grammar,
`SketchDimensionLabelView` uses for a selected sketch Line. One label opens a
compact field at a time; native owns which axis that is, because it is what the
renderer draws in the highlight weight.

**Not this stage:** Directional Scale handles or mode (Stage 020D, blocked by
OQ-01); Sculpt dimensions (`SCULPT-DIM-01`, blocked by OQ-02); Imported Mesh and
CAD Body dimensions; CAD FEATURE dimensions — a sketch line length, a circle
radius, an extrusion depth — which stay CAD authored truth and are edited by the
CAD feature editor; multi-select and group scale; hierarchy; snapping; Mirror; a
new unit system.

## The diagnostic imported mesh preview

`GLB-IMPORT-R0` under `ARCH-OWNER-08`, widened by `GLB-IMPORT-R1` under
`ARCH-OWNER-09`. **This is a diagnostic, and it is no longer reachable from the
product.** Since `IMPORT-01A` there is exactly one user-facing GLB route and it
is the durable import above; the preview's seams remain below JNI, driven only by
the verification suites, because the question they answer — does the geometry in
the FILE match the geometry in the SCENE — is still worth asking.

The owner saw a discrepancy between the ForgeShape scene and an external tool
that the corrected node scale of 1/1/1 did not explain. Three things could
produce that — a wrong exporter, a wrong reader, or a tool presenting the same
geometry differently — and only the first is ForgeShape's defect. So the product
gained the one thing that can separate them: a reader that shares nothing with
the writer, and a way to put what it read on the screen beside the thing it was
read from.

R1 widened the readable subset to the class of **static** file another sculpting
tool writes, so an external low-poly mesh can be looked at in the viewport. What
it did not widen is anything about what the preview IS — the section below is
unchanged, and most of the R1 test suite exists to hold that line while the
parser gets more permissive. `IMPORT-01A` did not widen it either: it gave the
same parse a SECOND destination, and left this one exactly as it was.

### Parser ownership

`forgeshape_gltf_import.{h,cpp}` calls nothing in `forgeshape_gltf_export.*`. It
shares `forgeshape_json.{h,cpp}` — which knows only what JSON is — and
`forgeshape_math.h`, and it re-derives every offset, length, stride and bound
from the file rather than from what a writer intended. `forgeshape_json` does
its own number grammar rather than handing text to `strtod`, because `strtod`
accepts `nan` and `inf` and a non-finite value that reaches geometry is the
exact failure the reader exists to prevent.

Everything outside the supported subset fails closed with its **own named
status** — `SparseAccessor`, `InterleavedAccessor`, `ExternalBuffer`,
`HasAnimation`, `NodeHierarchy`, `NodeTransformConflict`, `UnknownAttribute` and
the rest. Nothing is silently ignored, because a diagnostic that skipped a
transform would answer the question wrongly, which is worse than refusing to
answer it. `artifacts/glb-import-r1/SUPPORTED_SUBSET.md` is the full table
(`artifacts/glb-import-r0/SUBSET.md` records the narrower R0 boundary).

### What R1 reads, and what it does with it

- **The node transform** is a `matrix` (column-major, affine) or a TRS composed
  `T·R·S`; stating both is `NodeTransformConflict`, because which one wins is
  not a guess a diagnostic may make. A node with children is `NodeHierarchy`,
  refused rather than flattened: a flattened hierarchy is a different scene from
  the one the file describes.
- **The transform is BAKED** into the preview positions, so every draw item
  carries an identity model matrix and the placement exists in exactly one
  place. Normals ride `transpose(inverse(L))` and are normalized — not `L·n`,
  which is only the same answer while the scale is uniform. A negative
  determinant corrects triangle winding **for the preview only**; the product
  still has no Mirror and the exporter still refuses to write one. A zero
  determinant or a non-affine bottom row is `SingularNodeTransform`.
- **Several TRIANGLES primitives per mesh** become separate `ParsedGlbBatch`
  ranges over one shared vertex array, because `doubleSided` is a per-primitive
  fact the preview must honour per primitive. Primitives naming the same
  POSITION/NORMAL accessors share one decoded block, so a seven-material
  character stays the vertex count the file states rather than seven copies.
- **A missing NORMAL is generated**: unnormalized face normals from the baked
  positions, accumulated per vertex and normalized, so the weighting is area.
  A referenced vertex with no finite non-zero accumulation is
  `CannotGenerateNormals` — never a NaN, never an invented default. A durable
  import STORES whatever normals it ends up with, generated or stated, rather
  than re-deriving them on every load: re-deriving would silently replace an
  artist's hard edges with this rule's guess.
- **COLOR_0/COLOR_1/TEXCOORD_0/TEXCOORD_1** are structurally validated — the
  accessor resolves, its range is inside the buffer, its count agrees with
  POSITION — and then **not decoded**. Any other attribute is
  `UnknownAttribute`. The preview draws one flat neutral and never claims to
  show a colour it did not read.
- **`materials[].doubleSided`** is the only material member read at all, and it
  reaches preview culling and nothing else. `extras`, `asset.generator` and
  `extensionsUsed` are read by nothing.
- **No coordinate conversion of any kind.** glTF is right-handed, +Y-up and
  metric, and so is ForgeShape — the same fact that makes the exporter's lack of
  a conversion node correct.

`glbImportStatusCategory` maps every status to one of three bounded categories —
unreadable, unsupported, inconsistent — which is what the user is shown. The
status token itself goes to the log and the diagnostics ring, where somebody
chasing a particular file can act on it. The mapping lives beside the enum so
the Android layer never has to know which refusal means what.

### The preview is not a body

`ImportedMeshPreview` is session-only diagnostic renderer state. It has no
`ObjectId` from the scene's allocator, no entry in `ConstructionScene`, no
Construction Source, no Frozen Sculpt Mesh, no `MeshStore` publish; it never
enters `ConstructionHistory`, `.forge`, the checkpoint or
`projectSemanticFingerprint`; it cannot be selected, picked, edited or
re-exported; and it is gone with the process.

It **replaces** what the renderer is handed for a frame rather than merging with
the project snapshot, because a mixed list is a list somebody would eventually
pick, save or export from. While it is shown, the gizmo is withdrawn, a tap
selects nothing, and every editing control is absent — over an imported mesh
each of them would point at a body the user cannot see.

The renderer caches GPU buffers per `SceneDrawItem::objectId`, so every preview
BATCH needs a distinct key. They come from `kFirstPreviewRenderKey` (2^60), which
the body allocator cannot reach, and they are **renderer resource keys, not
identities**: minted by the preview, never by the scene, never persisted, never
shown, never picked against, and restarting from the same base each session.
`previewRenderKeyIsReserved` exists so a test can assert the two ranges cannot
meet.

### The roundtrip diagnostic

`forgeshape_glb_roundtrip.{h,cpp}` exports through the real writer, reimports
with the independent reader and compares both against **domain truth** — the
primitive generator or the Frozen Sculpt Mesh, through `buildRenderMesh`, placed
by `modelMatrix()`. Not the exporter's captured arrays: if the exporter captured
the wrong geometry, the expected side still holds the right geometry and the
comparison fails, which is the point.

The identity under test is `modelMatrix() · p_local == M_node · p_file`, which
holds only if the bake, the node transform and the file all agree. Since R1 the
importer applies `M_node` itself, so the actual side is read straight out of
`ParsedGlbMesh::positions`. Tolerance is float32 quantization and nothing else:
relative to the coordinate's magnitude, with an absolute floor, because absolute
error in a float grows with the value.

`forgeshape_glb_import_fixture.{h,cpp}` builds the deterministic Nomad-like
compatibility fixture — a synthetic GLB with the structural feature set of an
external low-poly export. Every coordinate is an integer over a power of two, so
its bytes are identical on every platform and a hash of it can be evidence. It
is reachable only from a debug JNI test seam and no product path calls it.

## Current boundaries

What the architecture deliberately does **not** contain. Each is a stage of its
own, and naming them is what stops one arriving by accident.

- **No brush framework.** Four tools behind one kernel; a fifth is another enum
  case, another `apply*` and another button — a visible, deliberate cost. No
  remesh, subdivision, dynamic topology, symmetry, masking, layers or brush
  presets. Sculpt Undo is no longer on this list — `ARCH-OWNER-12` implemented
  it, as whole strokes in a bounded volatile per-body history, and nothing about
  it made brushes into data. Pressure and tilt are **carried** by the pointer
  boundary and read by nothing: no brush behaviour derives from them.
- **No mesh library.** `SculptTopology` carries adjacency for a fixed topology
  and nothing more — no edge collapse, split, flip or incremental update, because
  the only thing that can happen to a frozen mesh is that a vertex moves.
- **No primitive framework.** No base class, polymorphism, registry, property
  metadata, reflection or plugin surface. A further primitive costs another
  member, `PrimitiveKind` case, variant alternative and per-kind JNI method.
- **A closed set of object commands, and no hierarchy.** The scene adds,
  selects, deletes (`UI-OWNER-45`) and — since Stage 018A (`UI-OWNER-40`) —
  renames, shows/hides, locks/unlocks and duplicates bodies, one named row at a
  time. It does nothing else: no group, nesting, reorder, parent field,
  multi-select or drag and drop. Neither Delete nor Duplicate adds an ObjectId
  reuse policy, because neither rolls the allocator back. There is no *command*
  framework: undo is `ConstructionHistory`'s bounded step state, not a
  reversible-command object graph, and it covers Construction edits only. Both
  Objects hosts are **views** of that same flat list and carry exactly the verbs
  it has.
- **No snapping in the world viewport.** The world reference grid is a viewport
  reference only: nothing snaps to it, no cursor is quantised, no dimension is
  derived from it. The sketch grid is a different contract that does exist —
  `SketchSession::snap` quantises sketch points to an adaptive 1/2/5·10^k step
  and to existing endpoints (`CAD-R0-A1A2`, `CAD-A3`) — and it is not grown out
  of `forgeshape_grid.h`. The View group also holds a Selection Outline since
  `SEL-OUT-R1`, and it is an overlay on the grid's terms and nothing more. There
  is still no View Cube, camera focus, named views, blur/glass or any
  post-processing framework: the outline is one mask pass and one blended
  full-screen draw, not the first stage of one.
- **No Mirror, no shear, no custom pivot, no hierarchy.** `ConstructionTransform`
  is translation, rotation and a strictly positive per-axis scale, and nothing
  else. A negative factor would be a Mirror — inverted winding, wrong normals,
  wrong front-face picking — and is refused rather than clamped. There is no
  parent, no explicit coordinate space beyond World and Local, no pivot the user
  can move, no centre free-move, no arcball and no snapping.
- **No editable tessellation.** The capsule's cylindrical middle is a single band
  between its seam rings however long it is, as the cylinder's side wall is.
  Shape, bounds and picking stay exact, but that middle carries no interior rings,
  so a small brush placed there has very few vertices to capture — a fidelity
  limitation, not a correctness one.
- **No property-editor framework.** The Property Inspector is three hand-written
  bodies: no property model, binding layer, editor registry or reflection.
  `NumericPropertyRow` and `UnitChipsView` know what a labelled number and a unit
  are, and nothing about primitives.
- **No design system and no motion framework.** Themes are two styles over one set
  of semantic attributes; the Objects section is a flat list of rows, not an object
  browser. `ChromeMotion` is four shared decisions, not a transition system.
- **One project slot, one recovery checkpoint, and no project library.**
  Persistence is the `.forge` document, one app-private manual slot, one
  app-private recovery checkpoint, and transfer of that same document through
  the system document UI. There is still no Save As, no naming, no recent list,
  no project browser, no thumbnail, no multi-project library and no cloud.
  Presentation and session state are written nowhere — not the camera, the
  start choice, the theme, the grid, the display unit, the held tool or the
  brush — so a process kill still clears all of those. What survives is exactly
  what a Save put in the slot, or what autosave put in the checkpoint.
- **One interchange format, two one-way pipelines.** The GLB export is an early
  vertical slice: geometry, normals, per-body placement and one default material
  in one `.glb`. It has no UVs, no textures, no materials of the user's
  choosing, no hierarchy, no merge or unit options, no draco or other
  compression, and it writes no second file — no `.bin`, no `.gltf`, no image,
  no sidecar. GLB import (`IMPORT-01A`) reads the bounded STATIC subset above
  into durable objects and reads nothing else: no animation, no skinning, no
  morph targets, no hierarchy, no materials, no textures, no UVs, no colours.
  OBJ and FBX cannot be read or written. `.forge` remains ForgeShape's own
  project format and is not an interchange format for Blender, CAD or a game
  engine; a `.glb` is never a project and is never referenced by one — once an
  import has happened, the file it came from is not consulted again. No
  third-party interchange library is used or authorized:
  `forgeshape_gltf_export.cpp` writes the container and the JSON itself, and
  `forgeshape_gltf_import.cpp` reads them without sharing a line with it.
- **`RuntimeMesh` is not a Construction mesh format**, and the debug paths are
  not product. It carries positions, colours and indices and nothing else: no
  normals, UVs, material, adjacency or history. `forgeshape_demo_mesh` is the
  bootstrap cube's numbers and `forgeshape_mesh_fixtures` is DEBUG test
  infrastructure behind a hook that compiles to a no-op in release; neither is a
  primitive or a product feature, and both are absent from `PRODUCT.md`. That
  hook is not a parallel implementation: its primitive driver calls the same
  `applyPrimitive` entry point.
