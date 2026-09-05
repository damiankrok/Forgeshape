# Comment audit — DEEP-AUDIT-R1

## Policy applied

KEEP a comment that carries a WHY, an ownership/threading boundary, a unit, a lifecycle rule, an invariant, a safety reason, a numeric/format/matrix fact, or an API contract (`@param`/`@return`). COMPRESS or REMOVE narration of the code's own steps, stage journeys ("before UI-R4B …", "Stage 012 resolved it optimistically"), superseded designs kept as archaeology, and statements no longer true. A compressed comment must state the same durable fact in fewer lines. **No runtime behavior changed** — the compression commit changes only comment lines; the audit inventory reports the production code-line count identical before and after (32 749), and the NDK clang `-fsyntax-only` pass plus `javac` both pass on the compressed tree.

## Stale statements corrected in place (not merely shortened)

| File | Was | Now |
| --- | --- | --- |
| `forgeshape_history.h` | "Sculpt has no undo" | points to the per-body `SculptHistory` (`ARCH-OWNER-12`) |
| `forgeshape_construction.h` | "exactly ONE active object … no create or delete" | one body's Construction Source; the scene owns the collection |
| `forgeshape_scene.h` | "a Construction Body or an Imported Mesh" | three representations |
| `forgeshape_object_id.h` | "the bootstrap demo object … the product Construction box" | historical aliases; identity minted by the scene; monotonic, never rolled back |
| `forgeshape_renderer.h` | "Stage 003 … present one indexed cube" | presents the scene snapshot |
| `forgeshape_selection.h` | "Stage 005 … exactly one selectable object" | one body selected at a time |
| `forgeshape_math.h` | "only what one perspective cube needs" | what the viewport, picking and the geometry domain need |
| `forgeshape_sculpt.h` | "the two representations … Construction Source / Frozen Sculpt Mesh" | source (Construction/Imported) vs Frozen Sculpt Mesh, per body |
| `forgeshape_jni.cpp` header | camera/picking/selection only | the current full responsibility list |
| `forgeshape_import_commit.h`, `forgeshape_imported_mesh.h` | "IMPORT-01A has no Start Sculpting" | `IMPORT-01B` sculpts an imported body |
| `forgeshape_project_bootstrap.h` | "New Project → CAD enters the spatial world-plane chooser and then the sketch" | opens directly on XY (`SKETCH-UX-R1`) |
| `forgeshape_body_delete.h` | last-body reasoning as one-object-product | project-is-never-empty / Home |
| `forgeshape_project_document.h` | "each Construction Body carries exactly one feature" | one of `CONS`/`IMPT`/`CADB` per body |
| Java: `NativeViewport`, `GlobalToolbarView`, `EditorWorkspaceView`, `ProjectActionsPopoverView`, `PropertyInspectorView`, `AnchoredSurfaceView`, `ChromeMotion`, `DisplaySettingsPopoverView`, `ObjectsCapsuleView`/`ObjectsSectionView`/`ObjectsPopoverView`, `AddPrimitivePaletteView`, `BrushEdgeControlsView`, `NumericPropertyRow`, `PrecisionScrollView` | stage-diary framing ("before UI-R4B", "four copies", "the owner review named"), two-representation and no-sculpt claims | durable statement of the same rule; five surfaces (not four); three representations; Undo in both modes |

## Metrics

| Scope | Baseline comment lines | After | Blocks > 15 | Blocks > 30 |
| --- | ---: | ---: | ---: | ---: |
| ALL 225 files | 28 473 (27.9 %) | 28 101 (27.6 %) | 214 → 206 | 53 → 41 |
| Production 135 files | 18 652 (36.3 %) | 18 253 (35.7 %) | 177 → 169 | 49 → 37 |
| Java production 51 files | 7 906 (45.4 %) | 7 613 (44.5 %) | — | — |

Code lines unchanged (production 32 749 before and after; the small ALL total moved only because comment lines fell). Net: 372 comment lines removed across ALL, 399 across production, with several long blocks compressed rather than deleted. No quota was targeted; density fell where narration was densest.

## Long-block whitelist (every remaining production comment block > 15 lines)

169 blocks remain over 15 lines. Each was inspected and kept because it carries a durable fact in one of the policy categories. Distribution: 32 FILE_CONTRACT (module headers stating platform-neutrality + ownership), 57 INVARIANT_OWNERSHIP, 31 FORMAT_LAYOUT (`.forge`/GLB byte layout, crease policy, MatCap formula, transform matrices), 18 UNITS_MATH, 14 THREADING, 14 API_CONTRACT (`@param`/`@return`), 3 RATIONALE. The full row-by-row list follows.

| File:line | Lines | Category | First line |
| --- | ---: | --- | --- |
| `cpp/forgeshape_body_delete.h:1` | 28 | FILE_CONTRACT | Removing one body from the project (`UI-OWNER-45`). |
| `cpp/forgeshape_cad_body.h:1` | 43 | FILE_CONTRACT | The CAD Body: a body whose geometry is a sketch extruded along its |
| `cpp/forgeshape_cad_face.h:1` | 38 | FILE_CONTRACT | The semantic face topology of a CAD extrusion (`CAD-A3`, `ARCH-OWNER-13`). |
| `cpp/forgeshape_construction.cpp:729` | 18 | FORMAT_LAYOUT | Vertex layout, fixed and shared with the index table below: |
| `cpp/forgeshape_construction.h:1` | 36 | FILE_CONTRACT | ForgeShape Construction domain — the exact primitives and the one active object. |
| `cpp/forgeshape_construction.h:91` | 17 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_construction.h:644` | 23 | INVARIANT_OWNERSHIP | A complete, order-free copy of everything a Construction Body's Construction |
| `cpp/forgeshape_display.h:1` | 28 | FILE_CONTRACT | ForgeShape viewport display settings — how the object is DRAWN, never what it |
| `cpp/forgeshape_display.h:51` | 16 | INVARIANT_OWNERSHIP | What the viewport is CLEARED to, behind everything. |
| `cpp/forgeshape_gizmo.h:1` | 64 | FILE_CONTRACT | The Construction transform gizmo — direct manipulation of a Body placement, |
| `cpp/forgeshape_gizmo.h:425` | 19 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_gizmo.h:460` | 18 | UNITS_MATH | --------------------------------------------------------------------------- |
| `cpp/forgeshape_glb_import_fixture.h:1` | 41 | FILE_CONTRACT | A deterministic external-GLB fixture, structurally like a low-poly sculpt |
| `cpp/forgeshape_glb_roundtrip.h:1` | 25 | FILE_CONTRACT | The GLB roundtrip diagnostic (`GLB-IMPORT-R0`). |
| `cpp/forgeshape_gltf_export.h:1` | 71 | FILE_CONTRACT | Early GLB 2.0 export — the current project as one binary glTF file. |
| `cpp/forgeshape_gltf_import.h:1` | 29 | FILE_CONTRACT | The bounded glTF 2.0 binary reader (`GLB-IMPORT-R0/R1`, `ARCH-OWNER-08/09`). |
| `cpp/forgeshape_grid.h:1` | 24 | FILE_CONTRACT | ForgeShape world reference grid — a viewport REFERENCE, never model geometry. |
| `cpp/forgeshape_history.h:1` | 43 | FILE_CONTRACT | The Construction transaction boundary and the Undo/Redo history. |
| `cpp/forgeshape_history.h:169` | 27 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------- |
| `cpp/forgeshape_import_commit.h:1` | 27 | FILE_CONTRACT | Turning a parsed GLB file into durable project objects (`IMPORT-01A`). |
| `cpp/forgeshape_import_commit.h:82` | 17 | THREADING | Creates one durable Imported Mesh body per supported top-level mesh node. |
| `cpp/forgeshape_import_preview.h:1` | 39 | FILE_CONTRACT | The Imported Mesh Preview — session-only diagnostic renderer state. |
| `cpp/forgeshape_imported_mesh.h:1` | 25 | FILE_CONTRACT | The Imported Mesh — a durable, non-parametric project representation |
| `cpp/forgeshape_input.h:1` | 19 | FILE_CONTRACT | Platform-neutral pointer event data shared by every native consumer. |
| `cpp/forgeshape_input.h:96` | 22 | UNITS_MATH | --------------------------------------------------------------------------- |
| `cpp/forgeshape_jni.cpp:1` | 17 | FILE_CONTRACT | The JNI boundary for the ForgeShape native viewport: the ONE place Android |
| `cpp/forgeshape_jni.cpp:170` | 32 | THREADING | ------------------------------------------------------------------------- |
| `cpp/forgeshape_jni.cpp:1781` | 19 | FORMAT_LAYOUT | Reads the AUTHORITATIVE Construction primitive state for display. |
| `cpp/forgeshape_jni.cpp:1962` | 16 | THREADING | Start Sculpting. |
| `cpp/forgeshape_jni.cpp:2283` | 16 | THREADING | Removes one body from the project (`UI-OWNER-45`). |
| `cpp/forgeshape_jni.cpp:2344` | 31 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_jni.cpp:2576` | 20 | INVARIANT_OWNERSHIP | Reads the authoritative gizmo state for display and verification. Nothing here |
| `cpp/forgeshape_jni.cpp:4834` | 20 | INVARIANT_OWNERSHIP | Reads the authoritative sculpt state for display. Nothing here is measured |
| `cpp/forgeshape_jni.cpp:5335` | 21 | INVARIANT_OWNERSHIP | ------------------------------------------------------------------- |
| `cpp/forgeshape_jni.cpp:5432` | 19 | INVARIANT_OWNERSHIP | ------------------------------------------------------------------- |
| `cpp/forgeshape_json.h:1` | 28 | FILE_CONTRACT | A small, bounded, read-only JSON parser. |
| `cpp/forgeshape_matcap.cpp:10` | 31 | UNITS_MATH | --------------------------------------------------------------------------- |
| `cpp/forgeshape_matcap.h:1` | 22 | FILE_CONTRACT | ForgeShape MatCap asset — generated, not downloaded. |
| `cpp/forgeshape_math.h:258` | 25 | INVARIANT_OWNERSHIP | Vulkan-style ORTHOGRAPHIC (parallel) projection, sharing every convention with |
| `cpp/forgeshape_mesh.h:1` | 24 | FILE_CONTRACT | ForgeShape runtime mesh ownership. |
| `cpp/forgeshape_picking.cpp:23` | 23 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_picking.cpp:132` | 22 | FORMAT_LAYOUT | --------------------------------------------------------------------------- |
| `cpp/forgeshape_picking.h:23` | 23 | THREADING | --------------------------------------------------------------------------- |
| `cpp/forgeshape_picking.h:82` | 24 | UNITS_MATH | Tolerance on the barycentric containment test, so a ray that lands exactly on |
| `cpp/forgeshape_picking.h:108` | 21 | INVARIANT_OWNERSHIP | Builds the world-space ray through a view-local pixel. |
| `cpp/forgeshape_project_bootstrap.h:1` | 38 | FILE_CONTRACT | The start of a project (`APP-H1`): Home, and the transient CAD bootstrap. |
| `cpp/forgeshape_project_bytes.h:1` | 20 | FILE_CONTRACT | Explicit little-endian byte plumbing for the portable `.forge` project file. |
| `cpp/forgeshape_project_document.h:1` | 52 | FILE_CONTRACT | The `.forge` project document: ForgeShape's own portable, versioned project |
| `cpp/forgeshape_project_document.h:401` | 20 | FORMAT_LAYOUT | Reads any supported version into `out`, writing nothing on failure. |
| `cpp/forgeshape_project_state.h:1` | 28 | FILE_CONTRACT | The bridge between the live ForgeShape project and the portable `.forge` |
| `cpp/forgeshape_project_state.h:87` | 24 | THREADING | Replaces the live project with `document`, atomically or not at all. |
| `cpp/forgeshape_project_state.h:115` | 25 | INVARIANT_OWNERSHIP | A cheap fingerprint of everything a `.forge` document would contain. |
| `cpp/forgeshape_render_mesh.h:1` | 49 | FILE_CONTRACT | ForgeShape derived render geometry — shading normals and the crease policy. |
| `cpp/forgeshape_render_mesh.h:82` | 32 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_render_mesh.h:147` | 20 | THREADING | Builds render geometry from source mesh data. |
| `cpp/forgeshape_render_mesh.h:171` | 16 | THREADING | --------------------------------------------------------------------------- |
| `cpp/forgeshape_render_recovery.h:1` | 18 | FILE_CONTRACT | What the renderer does when the GPU goes away, expressed without Vulkan. |
| `cpp/forgeshape_renderer.cpp:134` | 21 | UNITS_MATH | How far the grid is pushed AWAY from the eye in depth, in NDC, to settle the |
| `cpp/forgeshape_renderer.cpp:950` | 16 | THREADING | Once, and only when something is actually going to be destroyed: a |
| `cpp/forgeshape_renderer.cpp:1844` | 21 | FORMAT_LAYOUT | ------------------------------------------------------------------------ |
| `cpp/forgeshape_renderer.cpp:2167` | 20 | THREADING | Canonical ForgeShape convention (forgeshape_picking.h): triangles are |
| `cpp/forgeshape_renderer.h:333` | 28 | FORMAT_LAYOUT | Called once per frame, before recording. For EACH body in the scene, |
| `cpp/forgeshape_scene.cpp:325` | 18 | UNITS_MATH | --------------------------------------------------------------------------- |
| `cpp/forgeshape_scene.h:442` | 36 | FORMAT_LAYOUT | Builds the LOCAL triangle mesh a Frozen Sculpt Mesh would be frozen FROM. |
| `cpp/forgeshape_sculpt_history.h:1` | 34 | FILE_CONTRACT | The Sculpt Undo/Redo history: one bounded, volatile, per-body stack of |
| `cpp/forgeshape_sculpt_history.h:46` | 19 | FORMAT_LAYOUT | --------------------------------------------------------------------------- |
| `cpp/forgeshape_sculpt_history.h:103` | 18 | FORMAT_LAYOUT | --------------------------------------------------------------------------- |
| `cpp/forgeshape_sculpt.cpp:26` | 21 | INVARIANT_OWNERSHIP | World meters per screen pixel, for the brush being worked at `depth` in front |
| `cpp/forgeshape_sculpt.cpp:874` | 16 | INVARIANT_OWNERSHIP | --- Smooth: relax each vertex toward its 1-ring neighbour average ----------- |
| `cpp/forgeshape_sculpt.h:1` | 37 | FILE_CONTRACT | ForgeShape Sculpt domain — the Frozen Sculpt Mesh, the product mode, and the |
| `cpp/forgeshape_sculpt.h:158` | 23 | UNITS_MATH | --------------------------------------------------------------------------- |
| `cpp/forgeshape_sculpt.h:183` | 18 | UNITS_MATH | The LOCAL step that displaces a vertex by `worldMeters` along the direction a |
| `cpp/forgeshape_sculpt.h:204` | 21 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_sculpt.h:578` | 21 | INVARIANT_OWNERSHIP | --------------------------------------------------------------------------- |
| `cpp/forgeshape_selection_pulse.h:53` | 16 | INVARIANT_OWNERSHIP | Advances one body's selection presentation by one frame and returns the alpha |
| `cpp/forgeshape_sketch_session.h:1` | 43 | FILE_CONTRACT | The sketch edit session: the bounded, VOLATILE state between "New Sketch" |
| `cpp/forgeshape_sketch.h:1` | 36 | FILE_CONTRACT | The 2D sketch domain: entities, validation, closed-profile extraction and |
| `cpp/forgeshape_sketch.h:523` | 18 | INVARIANT_OWNERSHIP | Reads every closed profile out of a VALID sketch. |
| `cpp/forgeshape_transform.h:1` | 35 | FILE_CONTRACT | ForgeShape Construction transform — where the body is and how big it is |
| `cpp/forgeshape_transform.h:49` | 31 | THREADING | --------------------------------------------------------------------------- |
| `cpp/forgeshape_transform.h:180` | 29 | UNITS_MATH | Decomposes a pure rotation matrix back into the authoritative Euler degrees, |
| `cpp/forgeshape_workplane.h:1` | 27 | FILE_CONTRACT | The three principal workplanes a sketch can be drawn on, and the one mapping |
| `java/com/forgeshape/app/AddPrimitivePaletteView.java:12` | 29 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/AnchoredSurfaceView.java:6` | 40 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/AppTheme.java:3` | 20 | UNITS_MATH |  |
| `java/com/forgeshape/app/AutosaveController.java:11` | 41 | THREADING |  |
| `java/com/forgeshape/app/BodyLabels.java:5` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/BoundedScrollView.java:6` | 24 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/BrushEdgeControlsView.java:11` | 26 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/CadFeatureEditorView.java:10` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ChooserSurfaceView.java:13` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ChromeMotion.java:10` | 36 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ChromeMotion.java:94` | 17 | UNITS_MATH |  |
| `java/com/forgeshape/app/ChromeMotion.java:202` | 16 | API_CONTRACT |  |
| `java/com/forgeshape/app/ConstructionPlacementEditorView.java:10` | 20 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ConstructionPlacementEditorView.java:227` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ConstructionShapeEditorView.java:10` | 17 | UNITS_MATH |  |
| `java/com/forgeshape/app/DiagnosticLog.java:9` | 33 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/Diagnostics.java:15` | 25 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/DisplaySettingsPopoverView.java:10` | 33 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorControlStyles.java:14` | 27 | UNITS_MATH |  |
| `java/com/forgeshape/app/EditorControlStyles.java:75` | 18 | API_CONTRACT |  |
| `java/com/forgeshape/app/EditorControlStyles.java:176` | 18 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/EditorControlStyles.java:279` | 25 | API_CONTRACT |  |
| `java/com/forgeshape/app/EditorControlStyles.java:530` | 16 | UNITS_MATH |  |
| `java/com/forgeshape/app/EditorUiState.java:3` | 17 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/EditorUiState.java:22` | 19 | UNITS_MATH |  |
| `java/com/forgeshape/app/EditorUiState.java:67` | 16 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/EditorUiState.java:153` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:21` | 38 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:788` | 18 | API_CONTRACT |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:1184` | 20 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:1547` | 20 | FORMAT_LAYOUT | ----------------------------------------------------------------------- |
| `java/com/forgeshape/app/EditorWorkspaceView.java:1667` | 19 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:1706` | 25 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:2095` | 16 | UNITS_MATH |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:2140` | 19 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:2261` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:2380` | 19 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:2957` | 19 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:3264` | 16 | API_CONTRACT |  |
| `java/com/forgeshape/app/EditorWorkspaceView.java:3311` | 18 | API_CONTRACT |  |
| `java/com/forgeshape/app/ForgeShapeActivity.java:64` | 22 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ForgeShapeActivity.java:187` | 23 | THREADING |  |
| `java/com/forgeshape/app/GlobalToolbarView.java:14` | 29 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/GlobalToolbarView.java:313` | 16 | FORMAT_LAYOUT | Export WORKS now, for exactly one format. |
| `java/com/forgeshape/app/GlobalToolbarView.java:431` | 17 | INVARIANT_OWNERSHIP | ----------------------------------------------------------------------- |
| `java/com/forgeshape/app/GlobalToolbarView.java:551` | 18 | UNITS_MATH |  |
| `java/com/forgeshape/app/GlobalToolbarView.java:732` | 19 | API_CONTRACT |  |
| `java/com/forgeshape/app/GlobalToolbarView.java:848` | 30 | INVARIANT_OWNERSHIP | ----------------------------------------------------------------------- |
| `java/com/forgeshape/app/GlobalToolbarView.java:986` | 16 | UNITS_MATH |  |
| `java/com/forgeshape/app/HomeView.java:6` | 25 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/NativeViewport.java:35` | 31 | API_CONTRACT |  |
| `java/com/forgeshape/app/NativeViewport.java:205` | 23 | API_CONTRACT |  |
| `java/com/forgeshape/app/NativeViewport.java:836` | 16 | API_CONTRACT |  |
| `java/com/forgeshape/app/NativeViewport.java:1053` | 18 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/NativeViewport.java:1156` | 19 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/NativeViewport.java:1446` | 19 | API_CONTRACT |  |
| `java/com/forgeshape/app/NativeViewport.java:1470` | 25 | API_CONTRACT |  |
| `java/com/forgeshape/app/NativeViewport.java:1533` | 17 | API_CONTRACT |  |
| `java/com/forgeshape/app/NativeViewport.java:1555` | 17 | API_CONTRACT |  |
| `java/com/forgeshape/app/NewProjectChooserView.java:6` | 20 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/NumericPropertyRow.java:18` | 43 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ObjectsCapsuleView.java:13` | 30 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ObjectsPopoverView.java:11` | 19 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ObjectsSectionView.java:12` | 32 | THREADING |  |
| `java/com/forgeshape/app/PrecisionScrollView.java:8` | 31 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ProjectActionsPopoverView.java:9` | 34 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ProjectCheckpoint.java:10` | 31 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ProjectSlot.java:10` | 26 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ProjectTransfer.java:14` | 38 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/PropertyInspectorView.java:13` | 25 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/PropertyInspectorView.java:40` | 16 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/PropertyInspectorView.java:225` | 20 | RATIONALE |  |
| `java/com/forgeshape/app/RecoveryPromptView.java:12` | 24 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/SculptContextView.java:12` | 24 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/SculptContextView.java:149` | 21 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/SketchDimensionLabelView.java:20` | 32 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/SketchEditorView.java:11` | 25 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/SketchOrientationNavigatorView.java:11` | 34 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/StartPageView.java:15` | 27 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ToolRailView.java:15` | 26 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/ToolRailView.java:70` | 22 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/ToolRailView.java:259` | 30 | INVARIANT_OWNERSHIP |  |
| `java/com/forgeshape/app/TrailingClusterColumn.java:7` | 25 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/UnsavedChangesPromptView.java:6` | 17 | RATIONALE |  |
| `java/com/forgeshape/app/VerticalSliderView.java:9` | 16 | RATIONALE |  |
| `java/com/forgeshape/app/WorkspaceLayoutMode.java:3` | 18 | FORMAT_LAYOUT |  |
| `java/com/forgeshape/app/WorkspaceLayoutMode.java:130` | 18 | INVARIANT_OWNERSHIP |  |
