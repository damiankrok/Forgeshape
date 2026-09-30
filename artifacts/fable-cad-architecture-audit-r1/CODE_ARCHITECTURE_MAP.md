# Code architecture map (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Read-only. Baseline `main = 509de02`. Paths: `cpp/` = `app/src/main/cpp`,
`java/` = `app/src/main/java/com/forgeshape/app`. Numbers are scripted counts
over the baseline tree (`SOURCE_CONFIRMED` unless tagged). Git history is
shallow: the oldest commit (`504f97d`, 2026-09-01) imports
`EditorWorkspaceView.java` at 3411 lines and `forgeshape_jni.cpp` at 3788, so
"growth" below is growth over the 29 days the repository records.

## 1. Scale

| Tree | Lines |
| --- | --- |
| Native, non-self-test (`cpp/forgeshape_*` minus `*_selftest*`) | 50,165 |
| Native self-tests (23 files) | 34,800 (41 % of all native code) |
| Java `app/src/main` | 25,536 |
| Java JVM tests (`app/src/test`) | 1,791 |
| Java device tests (`app/src/androidTest`) | 44,508 |
| Three coordinators (`forgeshape_jni.cpp` + `EditorWorkspaceView` + `NativeViewport`) | 16,376 = 22 % of the first-party tree |

## 2. Module dependency map (native, 53 units, include graph)

State ownership legend: **T** durable truth (reaches `.forge`), **S** session /
runtime-local, **D** derived.

```
                         ┌──────────────────────────────────────────────┐
                         │  JNI  forgeshape_jni.cpp  (33 headers, all   │  S: 28 g_* (arbitration flags,
                         │  13 families, render thread, touchEvent)     │     saved sketch pose, preview
                         └───┬──────┬──────┬──────┬──────┬──────┬───────┘     cache, g_lastCadApplyStatus)
                             │      │      │      │      │      │
   ┌───────────┐  ┌──────────▼──┐ ┌─▼─────────┐ ┌▼──────────┐ ┌▼────────┐ ┌─▼──────────────┐
   │ renderer  │◄─┤ camera/gizmo│ │ sketch    │ │ cad       │ │ sculpt  │ │ project        │ T: the .forge codec,
   │ (only JNI │  │ picking/    │ │ session   │ │ body/     │ │ session │ │ document/state/│    fingerprint
   │ includes  │  │ selection/  │ │ region    │ │ feature/  │ │ mask    │ │ bootstrap      │ S: load staging
   │ it)   D   │  │ display  S  │ │ overlay S │ │ face/     │ │ history │ └──┬─────┬───────┘
   └─────┬─────┘  └──┬───┬──────┘ └──┬───┬────┘ │ kernel/   │ │      S  │    │     │
         │           │   │           │   │      │ extrude   │ └────┬────┘    │     │
         │           │   │           │   │      │ tool  T/D │      │         │     │
         │           │   │           │   │      └──┬────────┘      │         │     │
         ▼           ▼   │           ▼   │         ▼               ▼         ▼     ▼
   ┌────────────────────────────────────────────────────────────────────────────────────┐
   │ scene  ConstructionScene / SceneObject  (identity, order, active, allocator, name,  │ T
   │        hidden, locked, placement; representation ownership; snapshot = the ONE list)│
   └───────┬────────────────────┬──────────────────────┬───────────────────────┬─────────┘
           ▼                    ▼                      ▼                       ▼
   ┌──────────────┐   ┌──────────────────┐   ┌──────────────────┐   ┌─────────────────┐
   │ construction │   │ history          │   │ imported_mesh /  │   │ body commands   │
   │ primitives T │   │ ConstructionHist │   │ import_commit /  │   │ delete/mirror/  │
   └──────┬───────┘   │ (never serialized│   │ gltf_* / preview │   │ dimensions/     │
          ▼           │  ) S             │   │ T by exception   │   │ commands  (pure)│
   ┌──────────────┐   └──────────────────┘   └──────────────────┘   └─────────────────┘
   │ mesh / render│
   │ _mesh  D     │
   └──────┬───────┘
          ▼
   ┌──────────────┐
   │ math /       │
   │ transform    │
   └──────────────┘
```

Arrows and the include that causes each (`cpp/forgeshape_*`):

| From → To | Causing include |
| --- | --- |
| JNI → every module | `forgeshape_jni.cpp:15-70` |
| renderer → scene, sketch overlay | `forgeshape_renderer.h:32,35` (no `cad_*` header directly; CAD reaches it only as a `SceneSnapshot` draw item) |
| scene → construction, sculpt, cad, imported_mesh, mesh | `forgeshape_scene.h:20-26` |
| history → scene | `forgeshape_history.h:54` |
| sketch_session → cad_extrude_tool, cad_body, cad_face, camera, gizmo, picking, history, input | `forgeshape_sketch_session.h:50` and neighbours |
| sketch_overlay → gizmo | `forgeshape_sketch_overlay.h:21` (for the vertex/range TYPES only) |
| cad_body → sketch, sketch_region, workplane, construction | `forgeshape_cad_body.h` |
| cad_extrude_tool → camera, gizmo, picking; `.cpp` → sketch_session | `forgeshape_cad_extrude_tool.cpp:6` |
| support_chooser → sketch_session, sketch_overlay, scene, camera, gizmo, picking, selection | `forgeshape_support_chooser.{h,cpp}` |
| project_document → cad_body, cad_face, imported_mesh; project_state → history, sculpt; project_bootstrap → sketch_session | `forgeshape_project_*.h` |
| gltf_export / glb_roundtrip / import_commit → project_document, sculpt, render_mesh | |
| body_dimension_overlay → sketch_overlay | `forgeshape_body_dimension_overlay.h:28` (reuses the overlay line list — the seam `EXTRUDE_HUD_AUDIT.md` §4 C reuses again) |
| gizmo → history, scene; selection → scene | `forgeshape_gizmo.h:71` |
| mesh → picking; sculpt → camera, picking | `forgeshape_mesh.h:34`, `forgeshape_sculpt.h:46,51` |
| transform.cpp → construction, scene | `forgeshape_transform.cpp:10-11` (the `constructionTransform()` accessor reads the active body) |

Direct answers: the renderer includes NO CAD header and NO sketch session
(only `sketch_overlay.h`, the intended seam); `sketch_session` never includes
the renderer; `project_document` does not include sculpt (`project_state`
does); only `forgeshape_jni.cpp` and `forgeshape_renderer.cpp` include
`forgeshape_renderer.h`. The graph is **header-acyclic**. Three unit 2-cycles
are closed on the `.cpp` side only (`cad_body ⇄ cad_feature`,
`cad_extrude_tool ⇄ sketch_session`, `construction ⇄ transform`).

Edges worth moving (module-level cycles or leaf-reaching-up):

- `sketch_overlay.h → gizmo.h`: the overlay type depends on the gizmo only
  for `GizmoVertex`/range types; with `renderer.h → sketch_overlay.h` this
  makes camera/render ⇄ sketch a module cycle. The overlay line-list type
  belongs beside `render_mesh`.
- `transform.cpp → scene.h`: a leaf reaching the active body; the accessor
  could live in `scene.cpp` beside the other three, which the scene file
  already does deliberately for `sculptSession()` and friends (`scene.cpp:352-366`).
- `gizmo.h → history.h`: `GizmoSession` owns a `ScopedConstructionEdit`; it is
  really a body-command unit that happens to draw.

Fan-in: `math` 21, `object_id` 19, `scene` 18, `mesh` 16, `construction` 11.
Fan-out: `jni` 33, `renderer` 14, `sketch_session` 12, `support_chooser` 9,
`project_bootstrap` 9.

## 3. State each module owns

| Module | Durable truth | Session / runtime-local | Derived |
| --- | --- | --- | --- |
| math / transform | — (`ConstructionTransform` values are serialized by project) | `constructionTransform()` alias to the active body | Euler ⇄ matrix bridge |
| mesh / render_mesh | — | `MeshStore` per body (own mutex), `MeshRevision` | `RuntimeMesh`, pick topology, render vertices |
| construction | primitive kind + six parameter sets | remembered sets | generated meshes |
| scene | identity, order, active, allocator high-water, name, hidden, locked, placement, representation ownership | `activeBody()` counted null object, `g_activeBodyMisuse` | `SceneSnapshot` (hidden / isolate resolved once, `scene.cpp:264-300`), `resolveWorldModel` for face-supported bodies (re-derived every snapshot) |
| history | — (never serialized) | steps (whole `CadBodyState` copies), detached-body holds, session-initialization bracket | — |
| sculpt | Frozen Sculpt Mesh positions and topology (via project) | `SculptSession`, mask, `SculptHistory`, isolate | normals, adjacency |
| sketch | authored entities of a committed CAD body (via cad / project) | `SketchSession` draft, view flip/turns, candidate cache, overlay | loops, regions, tessellation, overlay line list |
| cad | the feature chain (`CadBodyState`) | `SupportChooser`, extrude-tool drag basis | kernel meshes, face tags, face frames, dependents' placement |
| project | the `.forge` codec, fingerprint | load staging | — |
| import / export | `ImportedMesh` arrays (truth by exception) | `ImportedMeshPreview` (diagnostic) | GLB bytes, baked export geometry |
| body commands | — (write scene truth through history) | `BodyDimensionSession` | dimension leaders, mirror arithmetic |
| camera / gizmo / picking / selection / display | — | `g_camera`, `g_selection` (globals in JNI `:221-222`), `GizmoSession`, `DisplaySettingsStore` | GPU resources, outline mask, render meshes |
| input | — | — | pointer samples |
| JNI | — | 28 `g_*` (arbitration flags `:269-272, :296-299, :363, :382`; sketch saved pose `:292`; preview cache `:1642`; stress mesh `:2045`; `g_lastCadApplyStatus` `:5624`) | log tokens |

## 4. The two large coordinators, measured

### 4.1 `cpp/forgeshape_jni.cpp` — 8100 lines

| Measure | Value |
| --- | --- |
| `JNIEXPORT` | 201 (200 unique symbols; `debugInjectDeviceLoss` has an `NDEBUG`/else pair) |
| Non-JNI helpers | 70 (54 anonymous-namespace, 10 `static`, 6 external) |
| Process globals `g_*` | 28 |
| `lock_guard`s / `hasProject()` checks / `FS_LOG*` lines | 179 / 26 / 313 |
| Comment lines | 2012 |
| Largest function | `touchEvent` `:7370-8085`, **715 lines** (8.8 % of the file): gizmo, sketch, support chooser, sculpt pending-then-promote, orbit, tap-select in ONE function |
| Render thread | `renderThreadMain` `:1714-1988`, `ViewportThread g_viewport` `:169`, atomics `:177-207` |
| Self-test wrappers | 22 `run*SelfTestsAndLog` `:434-1002`, 23 `*_selftest.h` includes (23 of the file's 56 ForgeShape includes) |
| Commits since 09-01 | 33; net +4312 lines, ~63 % from CAD commits (`c9670f8` +849, `b3a7ff0` +504) |

Exports by responsibility family (13 families in one translation unit):

| Family | Exports | Representative |
| --- | --- | --- |
| Sketch / CAD session | 56 | `sketchBegin` `:3993`, `sketchCommit` `:4337`, `cadExtrudeToolState` `:4811`, `sketchBeginEditFeature` `:5438`, `cadApplyExtrude` `:5731` |
| Scene / object commands | 21 | `sceneAddBody` `:3121`, `sceneDeleteBody` `:3194`, `sceneMirrorBody` `:3504` |
| Debug / test seams | 18 | §6 |
| Sculpt + sculpt history | 17 | `sculptState` `:6981`, `sculptJumpToHistoryCursor` `:6012` |
| Construction history | 14 | `constructionUndo` `:5909`, `historyUndo` `:6133` |
| Transform / gizmo | 13 | `setGizmoActive` `:3710`, `gizmoHitTest` `:3910` |
| Import / export | 13 | `exportGlb` `:6373`, `importGlbDurable` `:6427` |
| Display settings | 13 | `:7080-7265` |
| Lifecycle / surface | 10 | `start` `:1993`, `touchEvent` `:7370`, `surfaceCreated` `:7271`, `stop` `:8085` |
| Construction primitives | 7 | `:2318-2413` |
| Body dimensions | 7 | `:2509-2699` |
| Product mode / isolate | 6 | `freezeToSculpt` `:2773`, `enterSculptMode` `:2890` |
| Project persistence | 6 | `encodeProject` `:6254`, `loadProject` `:6307`, `closeProject` `:6835` |

Why it grew (structural, `INFERRED` from the section list and the `g_*`
inventory): every feature that needs a process-scoped session lands its JNI
adapter, its `touchEvent` arbitration branch and its log tokens here, because
this file owns `g_stateMutex` (`:223`) and the only render-thread handshake.
Its 24 titled `// ----` sections (`:210` … `:7069`) are already a table of
contents for a by-family split.

### 4.2 `java/EditorWorkspaceView.java` — 5504 lines

| Measure | Value |
| --- | --- |
| Methods | 226 (incl. a 451-line constructor `:471-921`) |
| Fields | 89 (77 instance; 46 child views / hosts; 26 primitives / arrays) |
| Listener interfaces implemented | 20 (`:59-78`) |
| Anonymous classes / lambdas | 8 / 2 |
| `NativeViewport.*` call sites / distinct natives / families | 171 / 97 / **12 of 13** (every family but debug) |
| `refresh*` methods | 8 (`refreshShellPhase` `:1829`, `refreshTransformGizmo` `:2683`, `refreshHistoryControls` `:2825`, `refreshHistoryNavigator` `:2854`, `refreshSketchViewportSurfaces` `:3265`, `refreshWorldAnchoredUi` `:3357`, `refreshExtrudeReadiness` `:3595`, `refreshDisplaySettings` `:5189`) |
| The master resync | `syncFromNative()` `:2478`, 157 lines, called from **22 sites**, fanning out to `refreshFromNative()` on **12 child views** |
| `refreshShellPhase` callers | 10 |
| Methods ≥ 80 lines | `syncFromNative` 157, `refreshSketchViewportSurfaces` 92, `onToolSelected` 82, `dismissTopmostSurface` 80 |
| Commits since 09-01 | 22; net +2093 (Home/bootstrap +556, preferences +318, sketch UX +262, dimensions +256, CAD +226, history navigator +165, vertical slice +165, anchored chrome +109) |

Method families (`INFERRED` from names and line ranges): surface/dismiss stack
and insets/layout (~30, `:922-1300, :1531-1760`); Home / New Project /
Settings / preferences / dirty guard (~45, `:1829-2456`); sync and
transform/history chrome (~15, `:2478-2996`); status line and precision
surface (~12, `:2997-3172`); sketch/CAD chrome (~35, `:3189-3860`);
dimensions (~8, `:3882-4029`); tool rail and mode transitions (~8,
`:4038-4254`); project save/open/recover/autosave (~20, `:4294-4614`);
export/import/diagnostics/SAF (~20, `:4632-5030`); display settings (~8,
`:5116-5204`); test accessors (~30, `:5284-5498`).

### 4.3 `java/NativeViewport.java` — 2772 lines

200 `static native` + 1 helper; 388 `static final int` wire constants; 1835
comment lines (66 %). Pure declaration file, no state, no logic. Every child
view is single-family; `EditorWorkspaceView` is the only multi-family caller
(171 sites over 12 families; next largest: `ObjectsSectionView` 16 sites, one
family).

## 5. Extraction seams

Already existing (native): the 11 process-scoped accessors
(`constructionScene()` `scene.cpp:344`, `meshStore()` `:498`,
`sculptSession()` `:532`, `constructionHistory()` `history.cpp:398`,
`gizmoSession()` `gizmo.cpp:1836`, `sketchSession()` `sketch_session.cpp:2075`,
`supportChooser()` `support_chooser.cpp:261`, `bodyDimensionSession()`
`body_dimension_overlay.cpp:242`, `displaySettings()` `display.cpp:269`,
`importedMeshPreview()` `import_preview.cpp:154`, `constructionTransform()`
`transform.cpp:395`); the pure-function policy units (`body_dimensions`,
`body_mirror`, `body_commands`, `body_delete`, `cad_extrude_tool`,
`render_recovery.h`, `selection_outline.h`, `project_bootstrap`).
Already existing (Java): `ViewportAnchorSpace`, `CadHudPresentation`,
`SketchChromePolicy`, `WorkspaceLayoutMode`, `EditorUiState`,
`AutosaveController`, `ProjectTransfer`, `ProjectSlot` / `ProjectCheckpoint`,
`AppPreferencesStore`, `AnchoredSurfaceView`, 12 child views with their own
`refreshFromNative()`.

Should exist (each is an ownership boundary, not a line split; see
`TARGET_MODULAR_ARCHITECTURE.md` and `REFACTOR_SEQUENCE.md`):

1. `forgeshape_jni_input.cpp` — `touchEvent` + the arbitration globals: "one
   Android event becomes exactly one of {gizmo drag, sketch stroke, chooser
   tap, sculpt stroke, orbit/pan/zoom, tap-select}".
2. `forgeshape_jni_render_thread.cpp` — `ViewportThread`, `renderThreadMain`,
   surface callbacks, atomics, `CadPreviewCache`; the only code touching
   `forgeshape_renderer.h`.
3. `forgeshape_jni_sketch_cad.cpp` — the 56 exports `:3970-5760` plus the
   sketch-view camera policy `:306-380` and the frame resolvers `:4736, :4909`.
4. `forgeshape_jni_selftests.cpp` — the 22 wrappers and their 23 includes;
   zero product behaviour.
5. `forgeshape_session_reset.{h,cpp}` — ONE "return every process-scoped
   session to rest", shared by the five routes in §7.
6. Java `ProjectLifecycleCoordinator` — save/open/recover/dirty-guard/SAF/
   import/export/autosave (`:2299-2456, :4294-5030`, ~60 methods; it holds
   `persistedFingerprint`/`everPersisted` `:250-251`, defect D3).
7. Java `SketchCadChromeCoordinator` — `:3189-3860`, 35 methods, the
   `nativeSketch`/`nativeExtrude` scratch arrays, `lastReportedSketchStatus`,
   `lastReportedRegionSelection`.
8. Java `ChromeRefreshScheduler` — owns `syncFromNative` and the
   `anchoredRefreshPending/Retries` state so 22 call sites become one
   "request resync" with a reason.

## 6. Duplicates, dead exports, test seams

**Java holds no second truth.** Enums cross as wire ints validated by
`*FromIndex` (never `static_cast`): `ExtrudeExtentMode` ↔
`EXTENT_*` (`NativeViewport.java:2409-2411`, `extrudeExtentModeFromIndex`
`jni:5079`), `CadFeatureOperation` ↔ `OPERATION_*` (`:2260-2262`,
`jni:4669`), `Workplane` ↔ `WORKPLANE_*`, `SketchTool` ↔ `SKETCH_TOOL_*`.
`EditorUiState` holds only UI memory (`EditorUiState.java:41-167`).
`ViewportAnchorSpace` is the ONE anchor conversion (four anchored views); no
Java projection matrix exists. Whether any self-test pins the wire index
values: `UNVERIFIED`.

Real duplicates (`SOURCE_CONFIRMED`):

| Duplicate | Where | Status |
| --- | --- | --- |
| Saved-state baseline | `EditorWorkspaceView.java:250-251` (`persistedFingerprint`, `everPersisted`) vs `AutosaveController.checkpointedFingerprint`; the first is lost on `recreate()` | council D3, still open |
| Profile-listing loop | `SketchEditorView.java:399-412` and `EditorWorkspaceView.java:3609-3618` each call `sketchProfiles(null)` → allocate → `sketchProfiles` → `sketchProfileInfo` loop | |
| Two doors to the base feature | `cadApplyRectangle/Circle/Extrude` (`jni:5731-5752`, candidate builders `:5688-5716`, process global `g_lastCadApplyStatus` `:5624`; called by `CadFeatureEditorView.java:315-327`) vs staged `sketchBeginEdit`/`sketchCommitEdit`; both end in `CadBody::applyState` | pre-chain API, base-only |
| Two region APIs | `sketchSelectProfile` `:4304` (R0, called by `SketchEditorView.java:509`) vs `sketchToggleRegion` `:4647` (no caller; the Ready tap toggles inside `touchEvent`) | |
| Three Undo families | `constructionUndo/Redo` (tests only), `sculptUndo/Redo/sculptRedoAvailable` (nobody), `historyUndo/Redo` (production, mode-deciding) | |
| Three anchored value editors | `CadExtrudeCanvasView`, `BodyDimensionLabelsView`, `SketchDimensionLabelView` each carry `EditText` setup, IME, parse-and-report, `placeAt` | `EXTRUDE_HUD_AUDIT.md` §2 |
| Geometry helpers | `orientation`/`onSegment` in `sketch_region.cpp:9-19` and `sketch.cpp:137-149`; `pointStrictlyInside` `sketch.cpp:151` dead; `signedAreaTwice` `sketch_region.cpp:50` dead | |
| Five project reset lists | §7 | |

**JNI surface**: 200 exports ⇔ 200 declarations, no orphan. **6 with no
caller anywhere** (`sculptUndo`, `sculptRedo`, `sculptRedoAvailable`,
`sketchToggleRegion`, `supportChooserConfirm`, `supportChooserSelect`). **54
(27 %) with no production caller** (tests only), including the Imported Mesh
Preview family (7 entry points + 3 report/fixture probes), the
construction-history probes, gizmo probes, all 13 `debug*`, `cadBodyMeasure`,
`sketchCandidateMeasure`, `sketchEditingFeatureId`, `selectionOutlineStats`.

**Test seams, classified** (compiled-in-release status from `CMakeLists.txt:104-163`
guards; `app/build.gradle:48-53` declares only a `debug` build type, so the
release consequence is `INFERRED`):

| Seam | Location | Guard | Classification |
| --- | --- | --- | --- |
| 22 self-test runners + 23 includes | `jni:434-1002, :15-70` | `#ifndef NDEBUG`; sources excluded from non-Debug | legitimate diagnostic seam; belongs in its own TU |
| `debugInjectDeviceLoss` → `Renderer::injectDeviceLossForTest` | `jni:6931-6953`; `renderer.h:171-181` | guarded both sides | legitimate (CLAUDE.md forbids a real device loss on the emulator) |
| `debugLastPointerEvent`, `debugCameraPose`, `debugSetCameraPose`, `debugResetConstructionHistory` | `:2120, :5766, :5787, :5840` | guarded | legitimate test seams |
| `debugMeshCommand` + `ForgeShapeActivity.onKeyDown` key map | `:2175-2295`; Activity `:402-426` | no-op stub in release; the Activity switch ships | debug console in production Activity |
| `debugRendererFramesPresented` / `DeviceRebuilds` | `:6885-6890` | **unguarded** | evidence-wait counters; harmless, but they are the reason "draw on change" cannot land without test changes |
| `debugProjectWorld`, `debugViewSceneBodyIds`, `debugViewportSelection`, `debugPreviewRendersBothSides`, `debugActiveBodyMisuseCount`, `selectionOutlineStats` | `:4155, :2970, :4182, :6678, :6862, :6910` | **unguarded** | test-only readers shipped: accidental |
| Imported Mesh Preview (7) + `nomadLikeGlbFixture`, `glbRoundtripReport`, `glbCompareReport` | `:6534-6760` | **unguarded**; `touchEvent` reads `importedMeshPreview().visible()` on every tap (`:7927`) | test-only production pollution: a second import destination with no UI, kept alive for two test classes |
| `ConstructionHistory::clear()` | `history.h:257-259` | product | NOT a test seam despite CLAUDE.md's wording: `closeProject` and `loadProjectDocument` use it; `debugResetConstructionHistory` is the test door |
| `beginSessionInitialization`/`end` | `jni:5810-5817` | product | legitimate; its only production use besides the Sculpt bootstrap is `ensureConstructionProjectForTest` |
| `*ForTest` methods + ~30 package-private accessors on `EditorWorkspaceView` | `:2132, :2419, :2440, :4025, :4511, :4529, :4664, :4900, :5284-5498` | shipped (Java has no build-type split) | the price of "locate by id, never by coordinate" |
| Process-shared test fixture | `WorkspaceTestSupport.resetToBaselineConstruction:95-106` reuses the inherited project | — | architecture that depends on tests: the five reset routes exist partly so tests can put the process back without a restart |

Verdict on "architecture that should not depend on tests" (`INFERRED`): the
54-export test-only surface and the render-thread counters are the two
places where the product's shape is set by the instrumentation strategy —
every probe is a native export because the domain is reachable only through
`NativeViewport`. A debug-only `NativeViewportDebug` (own JNI TU under
`#ifndef NDEBUG`) removes 27 % of the production surface without touching a test.

## 7. Five routes establish or end a project, five hand-written reset lists

| Route | Where | Resets |
| --- | --- | --- |
| `closeProject` | `jni:6835-6862` | `g_grabbing`, `g_strokePending`, chooser cancel, sketch cancel + `endSketchView`, sculpt `cancelStroke` + `enterConstruction`, gizmo off, `g_selection.resetGesture`, `history.clear()`, `scene.closeProject()` |
| `loadProject` + `loadProjectDocument` | `jni:6330-6346`; `project_state.cpp:199-377` | JNI: `g_grabbing`, `g_strokePending`, sketch cancel + `endSketchView`; domain: refuses `editInProgress`, `cancelStroke`, detach/insert, rebind sculpt, `history.clear()`. **Chooser, gizmo and selection gesture untouched.** |
| CAD bootstrap `commitFirstCadProject` | `jni:4353`; `project_bootstrap.h:70` | through the load path; the JNI caller adds chooser cancel + `endSketchView` |
| Sculpt bootstrap | **Java** `EditorWorkspaceView.onNewSculptProjectChosen:2233-2262` | `beginSessionInitialization` → `sceneAddBody` → `constructionPrimitive` → `applyConstructionSphere` → `freezeToSculpt` → `endSessionInitialization`; on refusal `closeProject`; "UNCHANGED is a success" special case `:2244-2247` |
| Test seeding | `ensureConstructionProjectForTest:2440-2452`, `showHomeAsFirstLaunchForTest:2419`, `debugResetConstructionHistory` `jni:5840` | chooser cancel, sketch cancel, bracket + `sceneAddBody` |

The asymmetry is the finding: the CAD first project is one native
all-or-nothing document; the Sculpt first project is a five-call Java
choreography over process-scoped state.

## 8. Comment debt (production Java + C++, excluding `third_party` and self-tests)

| Token | Count |
| --- | --- |
| `Stage 0NN` / `StageNNN` | 110 + 15 (`Stage 018A` 35, `Stage 020M` 33, `Stage027` 14) |
| `UI-OWNER-…` / `ARCH-OWNER-…` | 49 / 28 |
| `this stage` / `before this stage` | 53 / 8 |
| `no longer` / `used to` / `legacy` / `previously` | 63 / 36 / 43 / 4 |
| `TODO` / `FIXME` / `XXX` | 0 / 0 / 0 (only in vendored Manifold) |

Misleading samples: `history.h:27` "twenty doubles per body" (a CAD step
copies a whole `CadBodyState`); `transform.cpp:5-9` two stage ids to explain
one include; `EditorWorkspaceView.java:2437` "the default Box the product used
to start from" (removed by `APP-H1`); seven "used to be" narrations of removed
layouts in that one file (`:34, :287, :308, :773, :2546, :3010, :3035`);
`selection_pulse.h:23,42` describes the tint the outline replaced;
`ViewportAnchorSpace.java:181` documents a parameter every caller passes as
1.0; `NativeViewport.java` explains 55 slots only by a stage or gate id.
The debt is narrative, not TODOs: the code says WHAT CHANGED more often than
WHAT IS.

## 9. Prior audits, re-verified against the baseline

| Claim | Verdict |
| --- | --- |
| Six dead exports and two dead helpers (`FUNCTION-COUNCIL-R1` B2) | **STANDS** |
| D1 Import GLB unguarded in Sculpt | **FIXED** on `main` (`jni:6461` refuses in Sculpt; `EditorWorkspaceView.importRefusedWhileSculpting:4808`) |
| D2 `beginSketchView` reads `frame()` not `viewFrame()` (`jni:311`) | **STANDS** |
| D3 `persistedFingerprint`/`everPersisted` are view fields | **STANDS** |
| D5 overlay rebuild without `touchOverlay()` (`sketch_session.cpp:1759-1761`) | **STANDS** |
| D6 `canClearMask` omits the size test (`sculpt.cpp:1687-1690`) | **STANDS** |
| `architecture-health-review` structural numbers (91 natives, 3427-line view, `bodies_.front()` fallback, bodies never deleted) | **STALE** (200/200, 5504, counted null object, `deleteSceneBody`); all grown 2–2.4× in 29 days |
| `MODULE_MAP.md` "17 native suites" | stale: 23 |
| `MODULE_MAP.md` "the domain never includes the renderer" | still true |

## 10. Tests, by layer

| Layer | What | Runs where |
| --- | --- | --- |
| Native self-tests | 23 suites, 3757 checks, `scripts/host-native-selftests.sh` on the host, and once per debug launch on the device | **not in `CI FAST`** (`ci-fast.yml` has no host-selftest step); on the device via `CI DEVICE` startup capture |
| JVM unit tests | 11 classes, 1791 lines (`CadHudPresentationTest`, `SketchChromePolicyTest`, `AppPreferencesTest`, …) | `CI FAST` |
| Device tests | 57 classes, 629 tests, 44,508 lines; the CAD ones: `CadVerticalSliceTest`, `CadCanvasExtrudeTest`, `CadExtrudeExtentTest`, `SketchExtrudeTest`, `SketchUxTest`, `SpatialSketchTest` | `CI DEVICE` (focused list), `CI FULL SHARDED` (manual) |
| Corpus parity | 44 `.forge` fixtures vs the PowerShell encoder | `CI FAST` |
