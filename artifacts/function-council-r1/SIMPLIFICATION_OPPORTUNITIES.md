# Simplification opportunities (FUNCTION-COUNCIL-R1)

Recommendations only; nothing was implemented. Each entry names the exact
owner, the evidence, what it costs the user and the engineer today, a
direction, and whether it is **SAFE_TO_DEFER** or should be settled **BEFORE
THE NEXT FEATURE** that touches the area.

## A. Duplicate responsibility

**A1 — Five routes establish or end a project, each with its own reset list.**
- Owner: `forgeshape_project_state.cpp` (`loadProjectDocument`),
  `forgeshape_project_bootstrap.cpp` (`commitFirstCadProject`),
  `EditorWorkspaceView.onNewSculptProjectChosen`, `closeProject`
  (`forgeshape_jni.cpp:6822-6844`), and the test seams.
- Evidence: load does not cancel the support chooser or the gizmo, close does;
  the CAD bootstrap relies on its JNI caller for both; close clears the history
  without checking an open edit, load refuses one (`OWNERSHIP_MAP.md` §4).
- User impact: none observed today. Engineering: every new process-scoped
  session must be added to each list by hand.
- Direction: one native "return every process-scoped session to rest" called
  by all five routes.
- **SAFE_TO_DEFER.**

**A2 — Two doors to edit a CAD body's base rectangle or circle.**
- Owner: `CadFeatureEditorView.java:315-322` (`cadApplyRectangle/Circle/Extrude`)
  and the staged Edit Sketch (`sketchBeginEdit` / `sketchCommitEdit`).
- Evidence: both end in `CadBody::applyState`, so there is one truth
  (`forgeshape_jni.cpp:5643-5686`).
- User impact: two ways to do one thing in one panel. Engineering: two
  candidate builders, a process-scoped `g_lastCadApplyStatus` for one of them.
- Direction: keep the typed depth; route size edits through Edit Sketch.
- **SAFE_TO_DEFER** (decide with the next CAD UI task).

**A3 — Two face-support dependency models.**
- Owner: `TopoRef` + `resolveWorldModel` (another body's face, `CAD-A3`) and
  `CadFeatureSupport` (an earlier feature of the same body).
- Evidence: two lineage checks, two refusal families.
- User impact: whether a face sketch makes a second body or changes this one
  depends on whose face it is (`COUNCIL_FINDINGS.md`, OWNER decision 3).
- Direction: an OWNER decision first; the code can keep both.
- **SAFE_TO_DEFER.**

**A4 — Two saved-state baselines in Java.**
- Owner: `EditorWorkspaceView.persistedFingerprint` / `everPersisted` and
  `AutosaveController.checkpointedFingerprint`.
- Evidence: both updated by hand on Save, Open and Recover; the first is lost on
  `recreate()` (defect D3).
- Direction: move the first into `EditorUiState`, beside the other session
  memory the Activity already carries across recreation.
- **BEFORE THE NEXT FEATURE** that touches Settings or the project shell (D3).

## B. Historical scaffolding

**B1 — Test seams exported from the release library.**
- Owner: `forgeshape_jni.cpp`: `debugProjectWorld` (`:4155`),
  `debugViewSceneBodyIds` (`:2970`), `debugViewportSelection` (`:4182`),
  `debugPreviewRendersBothSides` (`:6665`), `debugActiveBodyMisuseCount`
  (`:6849`), `debugRendererDeviceRebuilds` / `debugRendererFramesPresented`
  (`:6872-6877`), and the Imported Mesh Preview entry points
  (`importGlbPreview` and the `glbPreview*` family, `nomadLikeGlbFixture`,
  `glbRoundtripReport`, `glbCompareReport`).
- Evidence: no `#ifndef NDEBUG` guard, unlike the other `debug*` entries; no
  production caller; the render loop checks preview visibility every frame.
- Direction: guard them as the others are, or move the preview into a debug
  source set.
- **SAFE_TO_DEFER.**

**B2 — Six JNI exports nothing calls, and two dead helpers.**
- `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`, `supportChooserSelect`,
  `supportChooserConfirm`, `sketchToggleRegion` (0 callers in main or
  androidTest); `signedAreaTwice` (`forgeshape_sketch_region.cpp:50`) and
  `pointStrictlyInside` (`forgeshape_sketch.cpp:151`), both unused.
- **SAFE_TO_DEFER.**

**B3 — A hardware-key test console in production code.**
- Owner: `ForgeShapeActivity.onKeyDown` maps keys to `debugMeshCommand` (stress
  tiers, fixtures); a no-op in release.
- **SAFE_TO_DEFER.**

**B4 — Self-tests re-run on every non-configuration relaunch** (Android seat,
`forgeshape_jni.cpp:1993-2030`, on the UI thread while holding the viewport
mutex). Debug only. **SAFE_TO_DEFER.**

## C. Over-broad coordinators

**C1 — `EditorWorkspaceView`** (5460 lines; ~224 methods; 78 fields; 20
listener interfaces; 97 distinct native calls; 27 child views;
`syncFromNative()` from 22 sites).
- Engineering impact: every feature adds to it; D3 exists because its session
  memory is split between the view and `EditorUiState`.
- Direction: extract the project lifecycle (save, open, recover, dirty guard,
  import) first — it has the clearest seam and D1 and D3 both live there — then
  the sketch/CAD chrome coordination.
- **SAFE_TO_DEFER**, but each new feature there raises the cost.

**C2 — `forgeshape_jni.cpp`** (8087 lines, 200 exports, the render loop, input
arbitration, every family's adapter). Recorded at ~5900 lines in DEEP-AUDIT-R1
(F-14).
- Direction: split by family into separate translation units behind the same
  exports; no behaviour change.
- **SAFE_TO_DEFER.**

## D. Hidden coupling

**D-1 — Tests share one process.** No orchestrator, and
`resetToBaselineConstruction` reuses the inherited project unless it has no
Construction body (`WorkspaceTestSupport.java:99-103`). This is what made C2's
Mirror failure and the `SpatialSketchTest` order dependence.
- Direction: close and reopen by default; `EditorWorkspaceObjectsTest` opts out.
- **BEFORE the next aggregate** (it is a Tier 5 trigger itself).

**D-2 — One test leaks a device-global setting on failure.**
`EditorWorkspaceLegibilityTest` sets `animator_duration_scale 0` (`:783`) and
restores it in the test body (`:798`), not in `@After`. **SAFE_TO_DEFER.**

**D-3 — Import GLB ignores the product mode** (defect D1). **BEFORE THE NEXT
FEATURE** that touches Sculpt or import.

**D-4 — The navigator's view state never reaches the camera** (defect D2).
**BEFORE THE NEXT FEATURE** that touches the sketch view.

**D-5 — The sculpt fingerprint proxy.** The fingerprint hashes a sculpt mesh's
revision (`forgeshape_project_state.cpp:621-629`), so Undo back to the saved
geometry still reads unsaved in Sculpt but not in Construction. **SAFE_TO_DEFER**
(state it in `CLAUDE.md`).

## E. Derived work done too often

**E1 — The render loop never rests.** It waits only while no surface is
attached (`forgeshape_jni.cpp:1750-1754`), so a still scene is redrawn at the
display rate. Each frame takes a fresh scene snapshot and, for a face-supported
CAD body, re-derives its chain geometry twice (recorded F-07).
- User impact: battery and heat on a phone (unmeasured).
- Direction: draw on change (a dirty flag or revision), keeping a frame
  request for the evidence waits.
- **SAFE_TO_DEFER**; measure on the physical device first.

**E2 — The CAD candidate is first evaluated on the UI thread per move**
(`cadExtrudeToolState`, `forgeshape_jni.cpp:4840`), holding the state lock the
render thread needs. **SAFE_TO_DEFER** at today's chain sizes; time a
16-feature chain on the phone before long chains become common.

**E3 — Sculpt republishes and re-derives the whole mesh per move**, and every
upload drains the GPU (renderer seat, `forgeshape_renderer.cpp:868-986`). The
render-mesh rebuild was 104 ms at 100k vertices on the S25 Ultra. **SAFE_TO_DEFER**
until density is a goal (`NEXT_VERTICAL_SLICE_OPTIONS.md` Option 6).

**E4 — A swapchain rebuild recreates five pipelines** (no pipeline cache, no
dynamic viewport), and the outline-mask allocation counter moves on a plain
re-attach too, which the `SEL-OUT-R1` rule says it does not (renderer seat).
**SAFE_TO_DEFER.**

**E5 — The recovery candidate is validated on the UI thread at cold launch**
(`EditorWorkspaceView.java:893`), which for a 16-feature chain runs the kernel.
**SAFE_TO_DEFER.**

## F. Persistence and version complexity

**F1 — Each `CADB` version is added by hand in about seven places.**
Five `cadDocumentNeedsV*` predicates, a monotone writer chain, `v2..v5`
reader flags, a `versionOk` disjunction, the PowerShell encoder, new fixtures
and digests (domain seat, `forgeshape_project_document.cpp:329-392,
1097-1100, 1685-1688, 1928-1946`).
- Correct: no partial-apply path, and every older fixture is byte-identical.
- Direction, no bytes changed: one ordered version table in the codec that
  drives the writer's choice and the reader's flags; read the fixture count in
  `ci-fast.yml` from the directory rather than a literal `44/44`.
- **SAFE_TO_DEFER**; worth doing before the next version bump (Options 2, 3
  and 5 all need `CADB` v6).

**F2 — A history step copies the whole `CadBodyState` both sides with no byte
cap**, while `forgeshape_history.h:27,40-42` and `ARCHITECTURE.md:1831-1836`
still say "about twenty doubles per body". Real sketches are small.
**SAFE_TO_DEFER**; correct the statement and consider a byte budget like
`SculptHistory`'s.
