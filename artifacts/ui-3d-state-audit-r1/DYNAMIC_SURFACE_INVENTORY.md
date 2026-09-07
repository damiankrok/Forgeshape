# UI-3D-STATE-AUDIT-R1 — Dynamic Surface Inventory

**Audit start HEAD:** `dfcab1afd1361d37b6dfe4607772a5597f31d043` (clean, local-only repo, no remote)

**Built from source**, not from runtime visibility: `View.VISIBLE`/`GONE` in the
shipped code is what the audit MEASURES, never what it takes as the contract.

## How a surface is classified

| field | meaning |
| --- | --- |
| **semantic owner** | the class that decides whether the surface is shown and where it stands |
| **owning context** | the object / body / session / mode the surface belongs to; when that context ends, the surface must end with it |
| **anchor** | `SCREEN_FIXED` (laid out by the Android layout pass), `WORLD_ANCHORED` (positioned from a projected world point), `FEATURE_ANCHORED` (positioned from a projected CAD-feature point), `OVERLAY_GEOMETRY` (drawn by the Vulkan renderer in world space), `NONE` (full-window page) |
| **refresh path** | the call that re-reads native truth and re-places it |
| **dismissal** | what must make it disappear |

The **refresh path** column is the load-bearing one for this audit: a surface
whose anchor is `WORLD_ANCHORED` or `FEATURE_ANCHORED` is only correct if its
refresh path is driven by *every* event that can move its anchor — a body
transform, a camera change, a viewport resize.

There are exactly three refresh drivers in the shipped shell
(`EditorWorkspaceView`):

- **D1 `syncFromNative()` / `onNativeStateChanged()`** — a full chrome re-read.
  Driven by chrome acts (a control pressed, a panel opened, a value applied) and
  by `onViewportGestureSettled()` **only when** the sketch state, the active
  `ObjectId`, the sculpt undo depth or the gizmo committed-drag count changed
  (`app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java:493-529`).
- **D2 `onViewportGestureMoved()`** — fires on every pointer sample of a
  viewport gesture and refreshes **exactly one** surface,
  `cadExtrudeCanvas.refreshFromNative()`
  (`app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java:465-476`).
- **D3 the native render loop** — redraws `OVERLAY_GEOMETRY` every frame from
  the domain, so it cannot go stale by construction.

Nothing else re-places a chrome view against the camera. There is no per-frame
Android-side reprojection and no `Choreographer` callback in the shell.

## 1 — Full-window pages (no project / settings)

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S01 | Home page | `home_surface` | `HomeView` / `refreshShellPhase` | "no project open" (`NativeViewport.projectOpen()`) | NONE | D1 | a project becomes open |
| S02 | New Project chooser | `new_project_chooser` | `NewProjectChooserView` | the Home to New Project step | NONE | D1 | Back, or the first CAD extrude / Sculpt seed commits |
| S03 | Settings page | `settings_page` | `SettingsPageView` | app preferences, no project context | NONE | D1 | Back |
| S04 | Unsaved-changes question | `unsaved_prompt` | `UnsavedChangesPromptView` | a dirty project being left | SCREEN_FIXED (modal over live project) | D1 | Save / Discard / Cancel / System Back |
| S05 | Recovery question | `recovery_prompt` | `RecoveryPromptView` | a decodable autosave checkpoint at launch | SCREEN_FIXED (modal) | D1 | Recover / Dismiss |

## 2 — Resting editor chrome (screen-fixed)

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S06 | Global Toolbar | `global_toolbar` | `GlobalToolbarView` | the editor | SCREEN_FIXED | D1 | chrome hidden; Home |
| S07 | Tool Rail | `tool_rail` | `ToolRailView` | product mode (Construction / Sculpt / Sketch) | SCREEN_FIXED | D1 | mode change re-populates it |
| S08 | Objects capsule | `objects_capsule` | `ObjectsCapsuleView` | the active body | SCREEN_FIXED | D1 | docked-column windows withdraw it |
| S09 | history capsule (Undo / Redo / navigator) | `history_group` children | `EditorWorkspaceView` | the history the mode names | SCREEN_FIXED | D1 | — |
| S10 | restore-chrome chip | `restore_ui_chip` | `EditorWorkspaceView` | "chrome hidden and not at Home" | SCREEN_FIXED | D1 | chrome restored |

## 3 — Primary contextual surfaces (anchored, screen-space)

All five are mutually exclusive by the UI-LAYOUT-R1 primary-surface rule
(`dismissPrimarySurfacesExcept`).

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S11 | Objects popover | `objects_popover` | `ObjectsPopoverView` | the scene | SCREEN_FIXED (anchored growth) | D1 | another primary surface; System Back; Sculpt/Sketch |
| S12 | Add Primitive palette | `add_primitive_palette` | `AddPrimitivePaletteView` | Construction, project open | SCREEN_FIXED | D1 | Sculpt or a sketch opening; another primary surface |
| S13 | precision surface (Property Inspector) | `property_inspector` | `PropertyInspectorView` | the active body plus the held Tool Rail entry | SCREEN_FIXED | D1 | its own toggle; another primary surface; Dimensions opening |
| S14 | Display popover | `display_settings_popover` | `DisplaySettingsPopoverView` | session display settings | SCREEN_FIXED | D1 | another primary surface |
| S15 | Project actions popover | `project_actions_popover` | `ProjectActionsPopoverView` | the project | SCREEN_FIXED | D1 | another primary surface |

## 4 — Row-level contextual surfaces inside Objects

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S16 | row command strip | `object_row_commands` | `ObjectsSectionView` | ONE Objects row | SCREEN_FIXED | D1 | Sculpt or sketch (`showObjectCommandsAvailable(false)`); another row overflow |
| S17 | mirror plane chooser | `object_mirror_planes` | `ObjectsSectionView` | the same row, replacing S16 | SCREEN_FIXED | D1 | System Back; the act; leaving the row |
| S18 | rename field | `object_rename_field` | `ObjectsSectionView` | the same row | SCREEN_FIXED | D1 | commit or cancel |

## 5 — Construction Body Dimensions (Stage 020M)

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S19 | dimension labels X/Y/Z | `body_dimension_label_x/_y/_z` | `BodyDimensionLabelsView` | Dimensions mode plus the ACTIVE measurable body | **WORLD_ANCHORED** (`NativeViewport.bodyDimensionLabelPoint(axis)`) | **D1 only** (`refreshSketchViewportSurfaces` calls `bodyDimensionLabels.refreshFromNative()`) | mode closed; body not measurable |
| S20 | dimension inline editor | `body_dimension_editor` | same | one axis of S19 | WORLD_ANCHORED | D1 only | `closeEditor()` |
| S21 | dimension leaders (extension lines, dimension line, ticks) | renderer | `forgeshape_body_dimension_overlay` | Dimensions mode plus the body | **OVERLAY_GEOMETRY** | D3 | mode closed |
| S22 | anchor chips (negative / centre / positive) | `dimension_anchor_group` | `WorkspaceTrailingHostView` | Dimensions mode | SCREEN_FIXED | D1 | mode closed |
| S23 | Relative Scale editor | `relative_scale_editor` | `RelativeScaleEditorView` | the active body, opened explicitly | SCREEN_FIXED | D1 | closes Dimensions mode when it opens |

## 6 — Transform gizmo

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S24 | transform gizmo | renderer | `forgeshape_gizmo` | Construction plus Transform tool plus an unlocked, non-face-supported active body | OVERLAY_GEOMETRY | D3 | Sculpt; Sketch; Dimensions mode; locked body |
| S25 | transform mode / space selectors | `transform_mode_group`, `transform_space_group` | `WorkspaceTrailingHostView` | the Transform rail entry | SCREEN_FIXED | D1 | Shape entry; Sculpt; Sketch |

## 7 — Selection

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S26 | selection outline | renderer | `forgeshape_selection_outline` plus the mask/composite passes | the selected body | OVERLAY_GEOMETRY | D3 | toggle off; body hidden; selection moved |
| S27 | selection acknowledgement pulse | renderer | `forgeshape_selection_pulse` | the moment of selection | OVERLAY_GEOMETRY | D3 | 220 ms decay |

## 8 — CAD sketch session

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S28 | sketch editor panel | `sketch_editor` | `SketchEditorView` | the open `SketchSession` | SCREEN_FIXED | D1 | Cancel / Extrude / commit |
| S29 | sketch profile chooser | `sketch_profile_chooser` | `SketchEditorView` | sketch `Ready` state | SCREEN_FIXED | D1 | leaving `Ready` |
| S30 | orientation navigator | `sketch_orientation_navigator` | `SketchOrientationNavigatorView` | the open sketch | SCREEN_FIXED (upper trailing) | D1 | sketch ends |
| S31 | selected-Line dimension label | `sketch_dimension_label` | `SketchDimensionLabelView` | ONE selected straight Line in the sketch | **WORLD_ANCHORED** (`sketchLineDimension` anchor) | **D1 plus `onSketchGestureSettled`** | selection is not a straight Line; sketch ends |
| S32 | selected-Line dimension geometry (extension lines, dimension line, ticks) | renderer | `SketchOverlayStyle::Dimension` range | the same selected Line | **OVERLAY_GEOMETRY** | D3 | selection change |
| S33 | spatial support chooser highlight | renderer plus `sketch_plane_chooser` | `supportChooser*` | the chooser step before a sketch | OVERLAY_GEOMETRY plus SCREEN_FIXED | D1/D3 | confirm or cancel |

## 9 — CAD staged extrusion (`CAD-UX-S1`, `CAD-EXT-R1`)

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S34 | extrude arrow (shaft plus head) | renderer, `Entities` range | `forgeshape_cad_extrude_tool` | sketch `Ready` | OVERLAY_GEOMETRY | D3 | leaving `Ready` |
| S35 | canvas extrude cluster (extent selector, distance, Flip, `New Body` badge) | `cad_extrude_canvas` cluster | `CadExtrudeCanvasView` | sketch `Ready` | **FEATURE_ANCHORED** (`CAD_EXTRUDE_LABEL_X/Y` plus `CAD_EXTRUDE_SCALE`) | **D1 plus D2** | `CAD_EXTRUDE_ACTIVE == 0`; anchor off screen |
| S36 | second-side value cluster (Side B) | `cad_extrude_second_value` | same | Two Sides mode only, second anchor on screen | **FEATURE_ANCHORED** (`CAD_EXTRUDE_SECOND_LABEL_X/Y`) | D1 plus D2 | not Two Sides; second anchor off screen |
| S37 | distance editors | `cad_extrude_depth_editor`, `cad_extrude_second_editor` | same | one open editor over one cluster | FEATURE_ANCHORED | D1 plus D2 | Apply; a live drag closes them |
| S38 | retained-sketch `Edit Sketch` chip | `cad_canvas_edit_sketch` | same | a COMMITTED CAD Body that is active, visible and whose anchor projects | **FEATURE_ANCHORED** (`cadBodySketchAnchor`) | D1 plus D2 | not a CAD body; hidden; anchor off screen; a session opens |
| S39 | CAD feature editor (exact sketch/extrude values) | `cad_editor` | `CadFeatureEditorView` | the active CAD body | SCREEN_FIXED | D1 | not a CAD body |

## 10 — Sculpt

| # | surface | id | semantic owner | owning context | anchor | refresh path | dismissal |
| --- | --- | --- | --- | --- | --- | --- | --- |
| S40 | brush Radius / Strength controls | `brush_edge_controls` | `BrushEdgeControlsView` | Sculpt mode | SCREEN_FIXED | D1 | Construction |
| S41 | Sculpt context surface (mesh summary, Clear Mask, Reset, stale-source warning) | `sculpt_mesh_summary` and siblings | `SculptContextView` | Sculpt mode plus the active body Frozen Sculpt Mesh | SCREEN_FIXED | D1 | Construction |
| S42 | Sculpt History navigator | `history_navigator` | `SculptHistoryNavigatorView` | Sculpt mode plus the active body `SculptHistory` | SCREEN_FIXED (anchored growth) | D1 | Construction; its own control |
| S43 | Sculpt mask viewport tint | renderer | fragment-stage mix on `RenderVertex::mask` | the active body mask | OVERLAY_GEOMETRY | D3 | mask cleared; Freeze/Reset |

## Exclusions, with reason

| excluded | reason |
| --- | --- |
| Imported-Mesh preview chrome | diagnostic; it has **no user-facing control at all** since `IMPORT-01A` and is reached only from the verification suites, so it is not a shipped dynamic surface |
| status line | reports verdicts with a lifecycle of its own; it carries no spatial anchor and no owning object, and its lifecycle is already owned by `EditorWorkspaceCorrectionTest` |
| soft keyboard / IME | a platform surface, not a ForgeShape one |
| `AnchoredSurfaceView` growth animation | motion, not state; owned by `ChromeMotionTest` |
| system bars | platform |

**Count: 43 classified surfaces, 5 explicit exclusions.**

## Immediate source-level observations (candidates, not yet runtime findings)

1. **S19/S20 (body dimension labels) are refreshed by D1 alone.** A camera
   orbit, pan or zoom is a viewport gesture that changes none of the four
   conditions `onViewportGestureSettled` tests, so neither D1 nor D2 runs for
   it. Candidate for a `CAMERA_PROJECTION` finding — measured as UI3D-05.
2. **S31 (sketch line dimension label) is refreshed by D1 and by
   `onSketchGestureSettled`**, which does run for every settled gesture while a
   sketch is open — but not per pointer sample.
3. **S32 uses `SketchOverlayStyle::Dimension`**, which the repository already
   records as rendering fully transparent (`PROJECT_STATUS.md` Known Issues;
   `app/src/main/cpp/forgeshape_renderer.cpp:1720-1741`). S21 active-axis leader
   is named in the same record. Adjudicated as UI3D-07 and the renderer
   visibility check, **not** assumed.
