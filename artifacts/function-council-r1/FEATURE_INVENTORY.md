# Feature inventory (FUNCTION-COUNCIL-R1)

What ForgeShape does at `a1b50f2`, one row per user-facing function. Read-only:
nothing was built or run for this table. A class is given from **source plus
recorded runtime evidence**; code alone never earns `COMPLETE`.

| Class | Meaning here |
| --- | --- |
| `COMPLETE` | implemented, and a named device test or recorded device run exercises the user path |
| `WORKS_WITH_LIMITATIONS` | works on the device; a stated limit or rough edge remains |
| `IMPLEMENTED_BUT_POORLY_EXPOSED` | works, but the user path to it is hard to find or misleading |
| `PARTIAL` | part of the function exists |
| `SOURCE_ONLY / RUNTIME_UNVERIFIED` | code exists; no device evidence for the claim made |
| `DEBT / RISK` | works in the tested path, but source shows a defect or an unguarded path |

"Feel" (brush response, glyph legibility, tint strength, one-handed reach) is
never classified here: it is OWNER review (Tier 4) for every row.

## Project / shell

| Function | Class | Evidence |
| --- | --- | --- |
| Home (no project open) | COMPLETE | `ConstructionScene(NoProjectTag)`; `hasProject()` the one answer; `HomeFlowTest` (12) and `debugActiveBodyMisuseCount` stays 0 |
| New Project → CAD (first sketch is the bootstrap) | COMPLETE | `commitFirstCadProject` (`forgeshape_project_bootstrap.cpp:19-88`) through the load path; `HomeFlowTest` |
| New Project → Sculpt (seeded sphere) | COMPLETE | `onNewSculptProjectChosen` inside `begin/endSessionInitialization`; `HomeFlowTest` |
| Save to the one slot / Open Saved Project | COMPLETE | `ProjectSlot` temp + fsync + rename; `ProjectProcessDeathTest` (8), `EditorWorkspaceProjectActionsTest` (7) |
| Open File… / Save Copy… (Storage Access Framework) | COMPLETE | `ProjectTransfer` is the whole `Uri` boundary; `ProjectTransferTest` (13) including "the codec never sees a Uri" |
| Close project / Back to Home | COMPLETE | `closeProject` writes nothing (`forgeshape_jni.cpp:6822-6844`); `HomeFlowTest` |
| Unsaved-changes question (Save / Discard / Cancel) | DEBT / RISK | works in `HomeFlowTest`; but the saved-state memory (`persistedFingerprint`, `everPersisted`, `EditorWorkspaceView.java:250-251`) is not carried across `recreate()`, which a palette change performs (`ForgeShapeActivity.requestTheme`), so a just-saved project reads unsaved afterwards. Source-confirmed, runtime unverified |
| Recovery checkpoint and question | WORKS_WITH_LIMITATIONS | `ProjectAutosaveRecoveryTest` (14), no sleeps (`awaitIdle`); the candidate is decoded on the UI thread at cold launch (`offerRecoveryIfPresent`, `EditorWorkspaceView.java:893`), which for a 16-feature chain runs the kernel booleans synchronously |
| Autosave | COMPLETE | coalesced, off the UI thread, fingerprint-gated; `ProjectAutosaveRecoveryTest` |
| Undo / Redo (one pair of controls, native picks the history) | COMPLETE | `historyUndo/Redo` → `runHistoryStep` by product mode; `EditorWorkspaceHistoryTest` (20), `SculptUndoTest` (10) |
| Settings page (palette, handedness, gizmo size and weight, Tool Labels) | COMPLETE | `SettingsPreferencesTest` (15) including byte-identical project bytes; `AppPreferencesTest` (JVM) |
| Diagnostics report | WORKS_WITH_LIMITATIONS | bounded ring (`DiagnosticLogTest`, JVM); `DiagnosticsAndRendererLossTest` writes the file; the crash throwable branch has no test (recorded debt) |
| Export GLB | COMPLETE | `GlbExportTest` (24) reads the bytes back with an independent reader; one-way, one file |
| Renderer device-loss recovery | SOURCE_ONLY / RUNTIME_UNVERIFIED for a real loss | injected loss is tested (`DiagnosticsAndRendererLossTest`); a real GPU loss has never been observed (recorded debt) |

## Objects / Construction

| Function | Class | Evidence |
| --- | --- | --- |
| Add body (six primitives) and Shape apply (six remembered sets) | COMPLETE | `CONS` v1; `EditorWorkspaceHistoryTest`, `EditorWorkspaceControlsTest` |
| Move / Rotate / Scale gizmo, World and Local | COMPLETE | one drag = one transaction; `EditorWorkspaceGizmoTest` (46) over the `GIZMO` suite |
| Exact transform editing | COMPLETE | `EditorWorkspaceHistoryTest`; one Apply = one step |
| Show / Hide, Lock / Unlock, Rename, Duplicate | WORKS_WITH_LIMITATIONS | `SCNE` v2; ONE device journey covers all four (`ObjectCommandsTest`, 1 test) and per-command refusals rest on native suites; the four sit behind a row overflow |
| Delete (one Undo, same object back) | COMPLETE | `deleteSceneBody`; `ObjectsDeleteTest` (8) |
| Mirror across XY / XZ / YZ | IMPLEMENTED_BUT_POORLY_EXPOSED | proper-rotation reflection, one Undo; `MirrorSmokeTest` (1) + `MIRROR` suite. Behind the row overflow, second line of the strip; `PRODUCT.md:1954` still says "There is no Mirror" |
| Selection outline and its toggle | COMPLETE | `SelectionOutlineTest` (19) + the pixel-counting visual class; session-only toggle |
| Dimensions and Relative Scale | WORKS_WITH_LIMITATIONS | Construction Bodies only (refuses Imported and CAD by design); `BodyDimensionsSmokeTest` (1) + `BODY_DIMENSIONS` suite; OWNER retest still pending |
| Sculpt-mode guards for object acts | DEBT / RISK | creation, delete, body switch and history are refused in Sculpt; **Import GLB is not** (see Imported Mesh) and typed primitive/transform Apply has no Sculpt refusal below JNI (Sculpt seat, `forgeshape_jni.cpp:1141-1310`, not re-verified line by line) |

## CAD

| Function | Class | Evidence |
| --- | --- | --- |
| New Sketch; first sketch opens directly on XY | COMPLETE | `HomeFlowTest`, `SketchExtrudeTest` |
| Support choice: spatial chooser (world planes, planar faces) and by-name fallback | COMPLETE | `SpatialSketchTest` (8): aiming taps, cap and side faces, curved side refused, dependent survives producer edit and reopen |
| Line, Polyline, Rectangle, Circle | COMPLETE | `SketchExtrudeTest` (9); `CAD` suite |
| Arc (three authored points) and Spline (interpolated points) | COMPLETE | `SketchUxTest` (14); `SKETCH_UX` suite |
| Line dimension (edit a line's length) | WORKS_WITH_LIMITATIONS | `SketchUxTest`; P0 fixed, no solver; a polygon's points are not numerically editable |
| Sketch snapping | PARTIAL | sketch-grid snap and endpoint snap exist; no inference (parallel, midpoint, horizontal) and no projected edges; no constraints by design |
| Orientation navigator: support plane while empty | COMPLETE | `SketchUxTest` |
| Orientation navigator: Flip and ±90° view turns | **DEBT / RISK — defect** | `beginSketchView` frames the camera on `sketchSession().frame()` (`forgeshape_jni.cpp:311`), the AUTHORING frame; `SketchSession::viewFrame()` has no production caller. Flip and the turns change a label and native bits but not the view. Tests assert only the bits. Recorded as "observed, not investigated" in `PROJECT_STATUS.md`; source-confirmed here |
| Regions with holes; tap to choose; nothing auto-chosen with more than one | COMPLETE | `CadVerticalSliceTest` `owner_rectangle_circle_region`, volume = ring area × depth |
| Extent: One Side, Symmetric, Two Sides; Flip (One Side only) | COMPLETE | `CadExtrudeExtentTest` (5), `CadCanvasExtrudeTest` (9) |
| Extrude arrow drag and the compact HUD | WORKS_WITH_LIMITATIONS | real-drag device tests; a dragged value is shown at full double precision (`POST_AUDIT.md` A3) |
| New Body / Add / Cut on the same body | COMPLETE | `CadVerticalSliceTest` `new_body_then_add_same_body`, `same_body_cut` (4 → 4.32 and 3.68 m³) |
| Live preview of the candidate | WORKS_WITH_LIMITATIONS | tinted per operation; Add tints the whole body (A4); the candidate is evaluated on the UI thread per move (`cadExtrudeToolState`, `forgeshape_jni.cpp:4840`) — CAD seat |
| Feature list; reopen a feature; upstream edit carries downstream | COMPLETE | `CadVerticalSliceTest` `feature_edit_roundtrip` (6.16 m³) |
| Edit Sketch (staged, one Finish = one Undo) | COMPLETE | `SketchUxTest`; `DependentFaceLost` refusal |
| Size editing of a rectangle/circle base from the Shape panel | IMPLEMENTED_BUT_POORLY_EXPOSED | a second route beside Edit Sketch (`cadApplyRectangle/Circle`, `CadFeatureEditorView.java:315-322`); same all-or-nothing `applyState`, so no second truth, but two ways to change one thing |
| Delete, suppress or reorder a feature | absent | `PRODUCT.md` "Not yet implemented" |
| Sketch on a Cut's pocket faces | absent by design | pocket faces are ineligible supports (A5) |
| Persistence and regeneration (`CADB` v1–v5) | COMPLETE | 44-fixture corpus, independent encoder, `CADVS_IO_22` asserts all eight v5 digests; `CadVerticalSliceTest` save/reopen byte-identical |
| CAD → Sculpt | absent by design | refused by name (`CadBodyNotSculptable`) |

## Imported Mesh

| Function | Class | Evidence |
| --- | --- | --- |
| Import GLB as durable objects (one Undo) | **DEBT / RISK** | `ImportedMeshDurableTest` (12). But the import is reachable in Sculpt (the Project menu stays visible, `GlobalToolbarView.java:631`) and neither the Java path (`onImportGlbRequested`) nor native (`importGlbDurable`, `commitImportedGlbScene`) refuses it; the commit makes the first imported body active (`forgeshape_import_commit.cpp:174`) while the mode stays Sculpt. Source-confirmed, runtime unverified |
| Transform an imported body | COMPLETE | `ImportedMeshDurableTest` |
| Persist (`IMPT`) | COMPLETE | `ImportedMeshDurableTest`, `PROJECT` suite digests |
| Selection and rendering | COMPLETE | `SelectionOutlineTest` (imported capture) |
| Sculpt an imported body; Back to / Reset from Imported Mesh | COMPLETE | `ImportedMeshSculptTest` (9); imported arrays immutable |
| The owner's own real-world `.glb` | SOURCE_ONLY / RUNTIME_UNVERIFIED | recorded as not yet tried |
| Imported Mesh Preview | diagnostic, no user control | 37 device tests drive it (`GlbImportPreviewTest`, `GlbImportExternalR1Test`) |

## Sculpt

| Function | Class | Evidence |
| --- | --- | --- |
| Start Sculpting, Back to Construction / Imported Mesh, Resume | COMPLETE | `EditorWorkspaceSculptRetentionTest` (2), `Stage027SculptWorkflowTest` (4) |
| Grab, Clay, Smooth, Inflate | COMPLETE (feel: OWNER) | `SculptUndoTest`, `SCULPT_BRUSH_KERNEL` (598 checks) |
| Flatten, Crease | COMPLETE (feel: OWNER) | `SculptBrushStage025Test` (8); `kCreaseInwardFraction/PinchFraction` put to the OWNER |
| Mask and Clear Mask | WORKS_WITH_LIMITATIONS | `SculptBrushStage025Test`; runtime-local by construction. Clear Mask is drawn and then refused (`EntryTooLarge`) for a mask over the per-entry cap, because `canClearMask` omits the size test (`forgeshape_sculpt.cpp:1687`) while `ARCHITECTURE.md` says the control is absent |
| Radius / Strength | WORKS_WITH_LIMITATIONS | session state, process lifetime; "visually heavy" (recorded debt) |
| Sculpt Undo / Redo | DEBT / RISK | one stroke = one entry, `SculptUndoTest`. A stroke over the 1 MiB entry cap applies and is not recorded, but earlier undo entries stay (`forgeshape_sculpt_history.cpp:88-97`) and store absolute positions, so the next Undo can produce a mix that was never on the branch. Dense meshes only; runtime unverified |
| History navigator (a jump is repeated Undo) | COMPLETE | `SculptHistoryNavigatorTest` (9) |
| Isolate | COMPLETE | `Stage027SculptWorkflowTest` pixel reads (behind a fixed 600 ms capture delay) |
| Visibility and mode guards (hidden body, viewport tap) | WORKS_WITH_LIMITATIONS | GUARD-1 and GUARD-2 on the device; the LOAD path can still reopen a hidden sculpted body into Sculpt (recorded debt) |
| Stylus pressure, hover, tilt | absent / BLOCKED | `STYLUS-G1` needs physical hardware; nothing reads pressure |
| Density above ~500 vertices | SOURCE_ONLY / RUNTIME_UNVERIFIED | whole-mesh republish per move; on the physical S25 Ultra the render-mesh rebuild was 104 ms at 100k vertices (`PROJECT_STATUS.md` ladder) |
