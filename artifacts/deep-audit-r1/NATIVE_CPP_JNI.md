# Native C++ and the JNI boundary — DEEP-AUDIT-R1

## Ownership and lifetime (Part B)

| Object | Owner | Lifetime | Notes |
| --- | --- | --- | --- |
| `ConstructionScene` (bodies, allocator, active id) | process-scoped static in `forgeshape_scene.cpp` (`constructionScene()`) | process | starts `NoProjectTag` (empty); `unique_ptr<SceneObject>` per body |
| `SceneObject` (representation variant, `MeshStore`, `FrozenSculpt`, `ConstructionTransform`, name) | scene, or `ConstructionHistory` while detached (`holdDetachedBody`) | until no history step names it | non-copyable; identity = `ObjectId` |
| `ConstructionHistory` (64 snapshots, detached bodies) | process-scoped static | process; `clear()` on load/close | snapshot per step, never mesh bytes |
| `SculptSession` (mode, tool, brush, in-flight `SculptStroke`, borrowed `FrozenSculpt*`) | process-scoped static; rebinds to the active body on every `sculptSession()` call | process | `target_` is a borrowed pointer — see F-02 |
| `SketchSession`, `SupportChooser`, `GizmoSession`, `CameraController`, `SelectionController` | process-scoped statics in the JNI file / scene.cpp | process | all mutated only under `g_stateMutex` |
| `Renderer` + `BodyRenderResources` per `ObjectId` + `RenderMeshCache` | render thread (`g_viewport`) | surface lifetime; device resources rebuilt on loss | releases bodies absent from the snapshot with a bounded (100 ms) fence wait |
| `MeshStore` published `RuntimeMesh` revisions | per body; inner `mutex_` | body | revision chain; `publish` validates twice (known debt) |
| JNI byte/float/long arrays | Java caller | per call | copied in/out with `Get/Set*ArrayRegion`; no pinned pointers held |

Raw-pointer inventory (production): `FrozenSculpt* SculptSession::target_` (borrowed; fixed by F-02 guard), `const SceneObject*` returned by `findBody` (used within one locked call), `SceneObject*` returned by `addCadBody`/`addImportedBody` (used within one transaction), `ANativeWindow*` (owned by JNI, released in `surfaceDestroyed` after the render thread's detach ack). No `new`/`delete` pairs outside `std::unique_ptr` construction (`forgeshape_scene.cpp`). No `reinterpret_cast` on file bytes: every `.forge`/GLB field goes through `ByteReader`/bounded accessor reads.

## Integer and floating-point safety

* `.forge`: counts are read as `u32`/`u64` and checked against `kMax*` bounds BEFORE allocation (`ImpossibleCount`, proven by `FSR1A_08`); section lengths are checked against the remaining file; CRC-32 covers every payload; `nextObjectId > highest` is validated. Gap (ARCH-HEALTH-01, still open, P3): `nextObjectId` is not bounded below the preview render-key range `2^60`.
* GLB: every accessor read is bounded by `byteLength`/`byteOffset`/stride checks; component counts are checked against the buffer; the JSON parser bounds depth (15) and value count (1M) and index-addresses its tree (no dangling refs) — read in full, no defect found.
* Sketch: entity count ≤ 256, polyline ≤ 256 vertices, spline ≤ 32 points, profile ≤ 1024 vertices; every coordinate is checked finite and in range (`NonFinite`, `OutOfRange`); the arc's circle is refused for collinear points; the ear clipper's `isEar` is O(n) so triangulation is O(n³) worst case at n ≤ 1024 (bounded, ≈ 1 s hostile worst case on the emulator — P3 perf note, see `PERFORMANCE_MEMORY.md`).
* Sculpt: vertex indices are `uint32_t`, bounds-checked in `vertexPosition`/`setVertexPosition` (out-of-range is a no-op / zero); history entries are refused when indices are unsorted or repeated (`SculptHistory::record`).
* Transform: `applyTransformValues` refuses non-finite and non-positive scale; `mat4Finite` guards every matrix consumed by picking and the brush.
* FP: the ARM64 `fmadd` contraction hazard is recorded in PROJECT_STATUS (Known Issues) and no exact-equality geometry comparison was found on a product path; exact equality is used only in the self-tests' determinism checks (same binary, same inputs).

## Error paths

Every fallible domain call returns a named status (`CadStatus` 31 values, `ProjectCodecStatus`, `GlbImportStatus`, `ImportCommitStatus`, `DeleteBodyStatus`, `MeshValidation`, `SculptHistoryStatus`); no exceptions are thrown or caught in production C++ (grep: zero `throw`/`try`). JNI translates each to an `int` code the Java side maps to a string; refusals change nothing (`FSR1A_07`, `IMP01A_20`, `CADUXR1_33` and the load path's all-or-nothing commit prove it).

## Sanitizers / static analysis

NOT AVAILABLE in this build: no ASan/UBSan/TSan configuration exists in `CMakeLists.txt` or `build.gradle`, no clang-tidy configuration, no host compiler. The NDK clang `-fsyntax-only` pass over the 35 production translation units passes with no diagnostics at the project's warning level (recorded in `TEST_RESULTS.md`). Adding a sanitizer build type is a follow-up (`INDEX.md` §top-10), not an audit-scope change.

## JNI matrix (Part D)

155 `static native` declarations in `NativeViewport.java`; 155 `Java_com_forgeshape_app_NativeViewport_*` definitions in `forgeshape_jni.cpp`; the two sets are identical (mechanical diff, `artifacts/deep-audit-r1/tools/jni_matrix.js`). Thread: every entry is called on the UI thread except `projectOpen`, `projectFingerprint` and `encodeProject`, which the autosave `HandlerThread` also calls (all three take `g_stateMutex`). `surfaceCreated/Changed/Destroyed` are UI-thread calls that hand off to the render thread through `g_viewport.mutex` and its condition variables. Ownership: no JNI entry retains a Java reference; arrays are copied by region within the call. Failure result: the last column lists the distinct `return` expressions found in the body.

Columns: Java method | native method | lock | guards / transfer | failure results.

| Java method | native | lock | guards | returns |
| --- | --- | --- | --- | --- |
| `void start()` | `Java_..._start` | g_stateMutex | debug-only  |  |
| `int debugLastPointerEvent(float[] outState)` | `Java_..._debugLastPointerEvent` | g_stateMutex | debug-only ExceptionCheck, array/string xfer | -1 | static_cast<jint>(sawAnyEvent ? count : -1) |
| `boolean debugMeshCommand(int command)` | `Java_..._debugMeshCommand` | g_stateMutex | debug-only  | JNI_FALSE | JNI_TRUE |
| `void constructionPrimitive(double[] outState)` | `Java_..._constructionPrimitive` | none |  array/string xfer |  |
| `int applyConstructionBox(double widthMeters, double heightMeters, double depthMeters)` | `Java_..._applyConstructionBox` | none |   |  |
| `int applyConstructionCylinder(double diameterMeters, double heightMeters)` | `Java_..._applyConstructionCylinder` | none |   |  |
| `int applyConstructionSphere(double diameterMeters)` | `Java_..._applyConstructionSphere` | none |   |  |
| `int applyConstructionCone(double bottomDiameterMeters, double heightMeters)` | `Java_..._applyConstructionCone` | none |   |  |
| `int applyConstructionCapsule(double diameterMeters, double totalHeightMeters)` | `Java_..._applyConstructionCapsule` | none |   |  |
| `int applyConstructionPlane(double widthMeters, double depthMeters)` | `Java_..._applyConstructionPlane` | none |   |  |
| `void boxTransform(double[] outPlacement)` | `Java_..._boxTransform` | g_stateMutex |  array/string xfer |  |
| `int applyBoxTransform(double positionXMeters, double positionYMeters, double positionZMeters, double rotationXDegrees, double rotationYDegrees, double rotationZDegrees, double scaleX, double scaleY, double scaleZ)` | `Java_..._applyBoxTransform` | none |   | transformResultToJni(applyBoxTransform("ui", requested)) |
| `int productMode()` | `Java_..._productMode` | none |  inSculptMode | forgeshape::sculptSession().inSculptMode() ? 1 : 0 |
| `int freezeToSculpt()` | `Java_..._freezeToSculpt` | g_stateMutex |  hasProject, sketch.active | kSculptFailedFreeze | kSculptRefusedCadBody | kSculptOk |
| `int enterConstructionMode()` | `Java_..._enterConstructionMode` | g_stateMutex |   | kSculptOk |
| `int enterSculptMode()` | `Java_..._enterSculptMode` | g_stateMutex |   | kSculptNothingFrozen | kSculptOk |
| `int sceneBodyCount()` | `Java_..._sceneBodyCount` | g_stateMutex |   |  |
| `int sceneBodyIds(long[] outIds)` | `Java_..._sceneBodyIds` | g_stateMutex |  array/string xfer | 0 | static_cast<jint>(count) |
| `long sceneActiveBodyId()` | `Java_..._sceneActiveBodyId` | g_stateMutex |   |  |
| `String sceneBodyName(long objectId)` | `Java_..._sceneBodyName` | g_stateMutex |   | newJavaString(env, name) |
| `boolean sceneBodyIsImported(long objectId)` | `Java_..._sceneBodyIsImported` | g_stateMutex |   |  |
| `boolean sceneActiveBodyIsImported()` | `Java_..._sceneActiveBodyIsImported` | g_stateMutex |  hasProject |  |
| `int sceneSelectBody(long objectId)` | `Java_..._sceneSelectBody` | g_stateMutex |  inSculptMode, sketch.active | kSculptFailedFreeze | kSculptNothingFrozen | kSculptOk |
| `long sceneAddBody()` | `Java_..._sceneAddBody` | g_stateMutex |  inSculptMode, sketch.active | static_cast<jlong>(forgeshape::kNoObject) | static_cast<jlong>(created) |
| `int sceneDeleteBody(long objectId)` | `Java_..._sceneDeleteBody` | g_stateMutex |  inSculptMode, sketch.active | kDeleteRefusedInSculpt | kDeleteUnknownBody | kDeleteRefusedLastBody | kDeleteRefusedEditInProgress |
| `boolean constructionUndoAvailable()` | `Java_..._constructionUndoAvailable` | g_stateMutex |   |  |
| `boolean constructionRedoAvailable()` | `Java_..._constructionRedoAvailable` | g_stateMutex |   |  |
| `long constructionMeshRevision()` | `Java_..._constructionMeshRevision` | none |   |  |
| `void setGizmoActive(boolean active)` | `Java_..._setGizmoActive` | g_stateMutex |  inSculptMode |  |
| `boolean setGizmoMode(int mode)` | `Java_..._setGizmoMode` | g_stateMutex |   | JNI_FALSE | accepted ? JNI_TRUE : JNI_FALSE |
| `boolean setGizmoPixelScale(float scale)` | `Java_..._setGizmoPixelScale` | none |   | accepted ? JNI_TRUE : JNI_FALSE |
| `boolean setGizmoSpace(int space)` | `Java_..._setGizmoSpace` | g_stateMutex |   | JNI_FALSE | accepted ? JNI_TRUE : JNI_FALSE |
| `void gizmoState(double[] out)` | `Java_..._gizmoState` | g_stateMutex |  array/string xfer |  |
| `int gizmoHitTest(float x, float y)` | `Java_..._gizmoHitTest` | g_stateMutex |   |  |
| `boolean gizmoHandlePoint(int handle, float[] out)` | `Java_..._gizmoHandlePoint` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE | static_cast<jint>(forgeshape::cadStatusCode(status)) |
| `int sketchBegin(int workplane)` | `Java_..._sketchBegin` | g_stateMutex |  inSculptMode, editInProgress | cadCode(forgeshape::CadStatus::InvalidWorkplane) | cadCode(status) | status | static_cast<jint>(forgeshape::workplaneIndex(c.plane)) |
| `boolean supportChooserBegin(boolean allowFaces)` | `Java_..._supportChooserBegin` | g_stateMutex |  inSculptMode, editInProgress, sketch.active | JNI_FALSE | JNI_TRUE |
| `int supportChooserHover(float x, float y)` | `Java_..._supportChooserHover` | g_stateMutex |   | -1 | supportKindCode(at) |
| `int supportChooserSelect(float x, float y)` | `Java_..._supportChooserSelect` | g_stateMutex |   | -1 | supportKindCode(at) |
| `int supportChooserSelectedKind()` | `Java_..._supportChooserSelectedKind` | g_stateMutex |   | supportKindCode(forgeshape::supportChooser().selected()) |
| `int supportChooserConfirm()` | `Java_..._supportChooserConfirm` | g_stateMutex |   | cadCode(status) |
| `void supportChooserCancel()` | `Java_..._supportChooserCancel` | g_stateMutex |   |  |
| `boolean supportChooserActive()` | `Java_..._supportChooserActive` | g_stateMutex |   | forgeshape::supportChooser().active() ? JNI_TRUE : JNI_FALSE |
| `boolean debugProjectWorld(double x, double y, double z, float[] out)` | `Java_..._debugProjectWorld` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE |
| `void sketchCancel()` | `Java_..._sketchCancel` | g_stateMutex |  sketch.active |  |
| `boolean sketchSetTool(int tool)` | `Java_..._sketchSetTool` | g_stateMutex |   | JNI_FALSE | accepted ? JNI_TRUE : JNI_FALSE |
| `int sketchTool()` | `Java_..._sketchTool` | g_stateMutex |   |  |
| `void sketchState(double[] out)` | `Java_..._sketchState` | g_stateMutex |  array/string xfer |  |
| `int sketchFinish()` | `Java_..._sketchFinish` | g_stateMutex |   | cadCode(status) |
| `void sketchBackToEditing()` | `Java_..._sketchBackToEditing` | g_stateMutex |   |  |
| `int sketchSelectProfile(long anchorEntityId)` | `Java_..._sketchSelectProfile` | g_stateMutex |   | cadCode(status) |
| `int sketchSetExtrude(double depthMeters, int direction)` | `Java_..._sketchSetExtrude` | g_stateMutex |   | cadCode(forgeshape::CadStatus::InvalidExtrudeDirection) | cadCode(status) |
| `long sketchCommit()` | `Java_..._sketchCommit` | g_stateMutex |  hasProject | static_cast<jlong>(forgeshape::kNoObject) | static_cast<jlong>(created) |
| `int sketchLastStatus()` | `Java_..._sketchLastStatus` | g_stateMutex |   | cadCode(forgeshape::sketchSession().lastStatus()) |
| `int sketchDeleteSelected()` | `Java_..._sketchDeleteSelected` | g_stateMutex |   | cadCode(status) |
| `boolean sketchSelectEntity(long entityId)` | `Java_..._sketchSelectEntity` | g_stateMutex |   | JNI_TRUE |
| `boolean sketchSelectedEntity(double[] out)` | `Java_..._sketchSelectedEntity` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE |
| `int sketchApplyRectangle(long entityId, double widthMeters, double heightMeters)` | `Java_..._sketchApplyRectangle` | g_stateMutex |   | cadCode(status) |
| `int sketchApplyCircle(long entityId, double radiusMeters)` | `Java_..._sketchApplyCircle` | g_stateMutex |   | cadCode(status) |
| `int sketchApplyLine(long entityId, double x0, double y0, double x1, double y1)` | `Java_..._sketchApplyLine` | g_stateMutex |   | cadCode(status) |
| `int sketchProfiles(long[] out)` | `Java_..._sketchProfiles` | g_stateMutex |  array/string xfer | static_cast<jint>(ids.size()) |
| `boolean sketchProfileInfo(long anchorEntityId, double[] out)` | `Java_..._sketchProfileInfo` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE |
| `boolean sketchScreenPoint(double u, double v, float[] out)` | `Java_..._sketchScreenPoint` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE |
| `double sketchGridStep()` | `Java_..._sketchGridStep` | g_stateMutex |   | forgeshape::sketchSession().gridStep() |
| `void sketchViewState(double[] out)` | `Java_..._sketchViewState` | g_stateMutex |  array/string xfer |  |
| `int sketchSetSupportPlane(int workplane)` | `Java_..._sketchSetSupportPlane` | g_stateMutex |   | cadCode(forgeshape::CadStatus::InvalidWorkplane) | cadCode(status) |
| `int sketchSetViewFlipped(boolean flipped)` | `Java_..._sketchSetViewFlipped` | g_stateMutex |   | cadCode(status) |
| `int sketchRotateView(int quarterTurns)` | `Java_..._sketchRotateView` | g_stateMutex |   | cadCode(status) |
| `boolean sketchLineDimension(double[] out)` | `Java_..._sketchLineDimension` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE |
| `int sketchApplyLineLength(long entityId, double lengthMeters)` | `Java_..._sketchApplyLineLength` | g_stateMutex |   | cadCode(status) |
| `int sketchBeginEdit(long bodyId)` | `Java_..._sketchBeginEdit` | g_stateMutex |  hasProject, inSculptMode, editInProgress | cadCode(status) |
| `long sketchEditingBodyId()` | `Java_..._sketchEditingBodyId` | g_stateMutex |   |  |
| `int sketchCommitEdit()` | `Java_..._sketchCommitEdit` | g_stateMutex |   | cadCode(status) |
| `String cadStatusToken(int code)` | `Java_..._cadStatusToken` | none |  array/string xfer | env->NewStringUTF("unknown") | env->NewStringUTF(forgeshape::cadStatusName(status)) |
| `int sceneBodyRepresentation(long objectId)` | `Java_..._sceneBodyRepresentation` | g_stateMutex |   |  |
| `boolean sceneActiveBodyIsCad()` | `Java_..._sceneActiveBodyIsCad` | g_stateMutex |  hasProject |  |
| `boolean sceneActiveBodyIsFaceSupportedCad()` | `Java_..._sceneActiveBodyIsFaceSupportedCad` | g_stateMutex |  hasProject |  |
| `boolean cadState(double[] out)` | `Java_..._cadState` | g_stateMutex |  hasProject, sketch.active, array/string xfer | JNI_FALSE | JNI_TRUE | changed ? kApplyApplied : kApplyUnchanged | kApplyRejectedNotFinite |
| `int cadApplyExtrude(double depthMeters, int direction)` | `Java_..._cadApplyExtrude` | none |   |  |
| `int cadApplyRectangle(double widthMeters, double heightMeters, double depthMeters, int direction)` | `Java_..._cadApplyRectangle` | none |   |  |
| `int cadApplyCircle(double radiusMeters, double depthMeters, int direction)` | `Java_..._cadApplyCircle` | none |   |  |
| `int cadLastStatus()` | `Java_..._cadLastStatus` | g_stateMutex |   | cadCode(g_lastCadApplyStatus) |
| `void debugCameraPose(float[] out)` | `Java_..._debugCameraPose` | g_stateMutex | debug-only array/string xfer |  |
| `boolean debugSetCameraPose(float yaw, float pitch, float distance)` | `Java_..._debugSetCameraPose` | g_stateMutex | debug-only  | JNI_FALSE |
| `void beginSessionInitialization()` | `Java_..._beginSessionInitialization` | g_stateMutex |   |  |
| `void endSessionInitialization()` | `Java_..._endSessionInitialization` | g_stateMutex |   |  |
| `void debugResetConstructionHistory()` | `Java_..._debugResetConstructionHistory` | g_stateMutex | debug-only  |  |
| `int constructionUndoDepth()` | `Java_..._constructionUndoDepth` | g_stateMutex |   |  |
| `int constructionRedoDepth()` | `Java_..._constructionRedoDepth` | g_stateMutex |  inSculptMode, sketch.active | kHistoryRefusedInSculpt | kHistoryNothingToDo | kHistoryOk |
| `int constructionUndo()` | `Java_..._constructionUndo` | none |   | runHistoryStep("undo", false) |
| `int constructionRedo()` | `Java_..._constructionRedo` | none |   | runHistoryStep("redo", true) |
| `boolean sculptUndoAvailable()` | `Java_..._sculptUndoAvailable` | g_stateMutex |   |  |
| `boolean sculptRedoAvailable()` | `Java_..._sculptRedoAvailable` | g_stateMutex |   |  |
| `int sculptUndoDepth()` | `Java_..._sculptUndoDepth` | g_stateMutex |   |  |
| `int sculptRedoDepth()` | `Java_..._sculptRedoDepth` | g_stateMutex |   |  |
| `long sculptHistoryBytes()` | `Java_..._sculptHistoryBytes` | g_stateMutex |   |  |
| `long sculptHistoryNotRetainedCount()` | `Java_..._sculptHistoryNotRetainedCount` | g_stateMutex |   |  |
| `long sculptHistoryEvictedCount()` | `Java_..._sculptHistoryEvictedCount` | g_stateMutex |   |  |
| `int sculptUndo()` | `Java_..._sculptUndo` | none |   | runSculptHistoryStep("undo", false) |
| `int sculptRedo()` | `Java_..._sculptRedo` | none |   | runSculptHistoryStep("redo", true) |
| `boolean historyUndoAvailable()` | `Java_..._historyUndoAvailable` | g_stateMutex |  inSculptMode |  |
| `boolean historyRedoAvailable()` | `Java_..._historyRedoAvailable` | g_stateMutex |  inSculptMode |  |
| `int historyUndo()` | `Java_..._historyUndo` | g_stateMutex |  inSculptMode |  |
| `int historyRedo()` | `Java_..._historyRedo` | g_stateMutex |  inSculptMode |  |
| `boolean beginConstructionEdit()` | `Java_..._beginConstructionEdit` | g_stateMutex |   |  |
| `boolean commitConstructionEdit()` | `Java_..._commitConstructionEdit` | g_stateMutex |   | recorded ? JNI_TRUE : JNI_FALSE |
| `void cancelConstructionEdit()` | `Java_..._cancelConstructionEdit` | g_stateMutex |   | kProjectOk | kProjectNotAProject | kProjectUnsupportedVersion | kProjectDamaged |
| `byte[] encodeProject()` | `Java_..._encodeProject` | g_stateMutex |  hasProject, inSculptMode, array/string xfer | nullptr | out |
| `int loadProject(byte[] bytes)` | `Java_..._loadProject` | g_stateMutex |  sketch.active, array/string xfer | kProjectNoData | projectStatusCode(status) | kProjectOk |
| `byte[] exportGlb()` | `Java_..._exportGlb` | g_stateMutex |  inSculptMode, array/string xfer | nullptr | out | 0 on success |
| `int importGlbDurable(byte[] bytes)` | `Java_..._importGlbDurable` | g_stateMutex |  array/string xfer | static_cast<jint>(forgeshape::GlbImportStatus::NoData) | static_cast<jint>(why) | kImportCommitStatusBase + static_cast<jint>(committed) | 0 |
| `String glbCommitStatusToken(int status)` | `Java_..._glbCommitStatusToken` | none |  array/string xfer | env->NewStringUTF("unknown") |
| `int glbCommitStatusCategory(int status)` | `Java_..._glbCommitStatusCategory` | none |   | static_cast<jint>(forgeshape::GlbImportCategory::Unreadable) |
| `int importGlbPreview(byte[] bytes)` | `Java_..._importGlbPreview` | g_stateMutex |  array/string xfer | static_cast<jint>(forgeshape::GlbImportStatus::NoData) | static_cast<jint>(why) | 0 | forgeshape::GlbImportStatus::NoData |
| `String glbImportStatusToken(int status)` | `Java_..._glbImportStatusToken` | none |  array/string xfer |  |
| `int glbImportStatusCategory(int status)` | `Java_..._glbImportStatusCategory` | none |   |  |
| `void clearGlbPreview()` | `Java_..._clearGlbPreview` | g_stateMutex |   |  |
| `boolean glbPreviewLoaded()` | `Java_..._glbPreviewLoaded` | g_stateMutex |   |  |
| `void setGlbPreviewVisible(boolean visible)` | `Java_..._setGlbPreviewVisible` | g_stateMutex |   |  |
| `boolean glbPreviewVisible()` | `Java_..._glbPreviewVisible` | g_stateMutex |   |  |
| `void glbPreviewCounts(int[] out)` | `Java_..._glbPreviewCounts` | g_stateMutex |  array/string xfer |  |
| `boolean glbPreviewBounds(float[] out)` | `Java_..._glbPreviewBounds` | g_stateMutex |  array/string xfer | JNI_FALSE | JNI_TRUE |
| `boolean debugPreviewRendersBothSides()` | `Java_..._debugPreviewRendersBothSides` | g_stateMutex |   | JNI_FALSE | JNI_TRUE |
| `byte[] nomadLikeGlbFixture()` | `Java_..._nomadLikeGlbFixture` | none |  array/string xfer | nullptr | out |
| `String glbRoundtripReport()` | `Java_..._glbRoundtripReport` | g_stateMutex |  inSculptMode | newJavaString(env, report) |
| `String glbCompareReport(byte[] bytes)` | `Java_..._glbCompareReport` | g_stateMutex |  inSculptMode, array/string xfer | newJavaString(env, report) |
| `int validateProject(byte[] bytes)` | `Java_..._validateProject` | none |  array/string xfer | kProjectNoData | projectStatusCode(status) | kProjectDamaged | kProjectOk |
| `long projectFingerprint()` | `Java_..._projectFingerprint` | g_stateMutex |  hasProject, inSculptMode | 0 |
| `boolean projectOpen()` | `Java_..._projectOpen` | g_stateMutex |  hasProject |  |
| `void closeProject()` | `Java_..._closeProject` | g_stateMutex |  sketch.active |  |
| `long debugActiveBodyMisuseCount()` | `Java_..._debugActiveBodyMisuseCount` | none |   |  |
| `int rendererLifecycle()` | `Java_..._rendererLifecycle` | none |   | kRendererRecovering | kRendererRestartRequired | kRendererHealthy |
| `int debugRendererDeviceRebuilds()` | `Java_..._debugRendererDeviceRebuilds` | none | debug-only  | g_rendererDeviceRebuilds.load(std::memory_order_relaxed) |
| `boolean debugInjectDeviceLoss()` | `Java_..._debugInjectDeviceLoss` | g_viewport.mutex |   | JNI_TRUE |
| `boolean debugInjectDeviceLoss()` | `Java_..._debugInjectDeviceLoss` | none |   | JNI_FALSE |
| `void sculptState(double[] outState)` | `Java_..._sculptState` | none |  inSculptMode, array/string xfer |  |
| `void setSculptBrush(double radiusPixels, double strength)` | `Java_..._setSculptBrush` | g_stateMutex |   |  |
| `int setSculptTool(int tool)` | `Java_..._setSculptTool` | g_stateMutex |   |  |
| `int sculptTool()` | `Java_..._sculptTool` | none |   |  |
| `int setShadingModel(int model)` | `Java_..._setShadingModel` | none |   |  |
| `int shadingModel()` | `Java_..._shadingModel` | none |   |  |
| `int setSurfaceShading(int shading)` | `Java_..._setSurfaceShading` | none |   |  |
| `int surfaceShading()` | `Java_..._surfaceShading` | none |   |  |
| `int setViewportBackground(int background)` | `Java_..._setViewportBackground` | none |   |  |
| `int viewportBackground()` | `Java_..._viewportBackground` | none |   |  |
| `boolean setGridVisible(boolean visible)` | `Java_..._setGridVisible` | none |   | settings.gridVisible() ? JNI_TRUE : JNI_FALSE |
| `boolean gridVisible()` | `Java_..._gridVisible` | none |   |  |
| `void setReducedMotion(boolean reduced)` | `Java_..._setReducedMotion` | none |   |  |
| `boolean reducedMotion()` | `Java_..._reducedMotion` | none |   |  |
| `int setProjectionMode(int mode)` | `Java_..._setProjectionMode` | g_stateMutex |   | static_cast<jint>(forgeshape::projectionModeIndex(active)) |
| `int projectionMode()` | `Java_..._projectionMode` | g_stateMutex |   |  |
| `void surfaceCreated(Surface surface)` | `Java_..._surfaceCreated` | g_viewport.mutex |   |  |
| `void surfaceChanged(int width, int height)` | `Java_..._surfaceChanged` | g_viewport.mutex |   |  |
| `void surfaceDestroyed()` | `Java_..._surfaceDestroyed` | g_viewport.mutex |   | g_viewport.detachAcked |
| `void touchEvent(int action, int actionPointerId, int pointerCount, int[] ids, float[] xs, float[] ys, int[] toolTypes, float[] pressures, float[] tilts, float[] tiltOrientations, int viewWidth, int viewHeight)` | `Java_..._touchEvent` | g_stateMutex | debug-only inSculptMode, ExceptionCheck, array/string xfer |  |
| `void stop()` | `Java_..._stop` | g_viewport.mutex |   |  |
### Reading the matrix

* **41 entries take no lock.** 27 of them are pure readers of atomics or constants (`productMode`, `shadingModel`, status-token lookups, `rendererLifecycle`, debug counters). Six are unlocked readers of scene state on the UI thread (`constructionPrimitive`, `constructionMeshRevision`, `sculptState`, `sculptTool`, `sculptState`'s `sculptSession()` rebind) — see `THREADING_CONCURRENCY.md` F-08 (P2). The `applyConstruction*`/`applyBoxTransform`/`cadApply*`/`constructionUndo`/`sculptUndo` entries delegate to a locked helper (`applyPrimitiveLocked`, `runConstructionStep`), which the regex does not see; verified by reading.
* `touchEvent` reads three `Get*ArrayRegion` before one `ExceptionCheck` — a JNI-spec nit (P3, F-11); the Java caller always passes arrays of length 6 ≥ `pointerCount`, so it is unreachable in the product.
* `sceneBodyName`, `glbRoundtripReport`, `glbCompareReport` now cross through `newJavaString` (UTF-16) — F-03.
