# Refactor sequence (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

No rewrite. Every step is bounded, preserves behaviour unless it says
otherwise, names a rollback point, and is validated by the repository's own
tiers (`artifacts/function-council-r1/LEAN_VALIDATION_POLICY.md`): Tier 1 =
`scripts/host-native-selftests.sh` + JVM tests; Tier 2 = `CI FAST` + one
focused `CI DEVICE` list; Tier 5 = a fresh FullSharded aggregate, only on its
named triggers (a format change, a change to shared test support). Nothing
here is authorised by this audit; it is the order the evidence supports.

Estimates of code moved or removed are `INFERRED` from the measurements in
`CODE_ARCHITECTURE_MAP.md`.

## SAFE NOW — no user-visible semantics

| # | Goal | Files likely touched | Invariants preserved | Migration | Tests | Rollback | Risk | Size |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| S1 | Remove the six dead exports and the three dead helpers | `forgeshape_jni.cpp` (`sculptUndo/Redo/RedoAvailable`, `supportChooserSelect/Confirm`, `sketchToggleRegion`), `NativeViewport.java`, `forgeshape_sketch_region.cpp:50`, `forgeshape_sketch.cpp:151`, `forgeshape_cad_feature.cpp:371` | 200 ⇔ 200 declaration parity becomes 194 ⇔ 194 | delete; `jni_xcheck`-style grep confirms no orphan | Tier 1; `CI FAST` | one commit | none | −~150 lines |
| S2 | Guard the 18 unguarded test-only exports, or move them to a debug TU | `forgeshape_jni.cpp:2970, :4155, :4182, :6534-6760, :6678, :6862, :6885-6910`; `NativeViewport.java`; test callers | debug-only behaviour unchanged in debug builds | `#ifndef NDEBUG` first (one commit); the debug TU is step B4 | Tier 1; `CI FAST` release guard (`ci-release-selftest-guard.sh`) extended to the new symbols | one commit | low | 0 moved now |
| S3 | One `metersPerPixel` for the extrude overlay and the HUD | `forgeshape_jni.cpp:1859-1862` (read at `anchors.base`, or ask the session), `forgeshape_sketch_session.cpp:2004-2010` comment | the head/hit/glyph numbers are one number at the anchor depth | compute `cadExtrudeControlScale(camera, anchors.base, h)` once per frame and hand it to `overlay()` | `CADUXS1_09_f` gains a check that both call sites feed one `mpp`; `Ui3dStateCorrectionTest.ui3dc1_11` on a FACE sketch (new case) | one commit | low; **visible only where perspective + face sketch differed before**, which is the defect | +~20 |
| S4 | Bump `overlayRevision_` on a `worldPerUnit` rebuild (council D5) | `forgeshape_sketch_session.cpp:1759-1761` | uploads stay revision-keyed | call `touchOverlay()` | `SketchUxTest` pinch-then-check (new) | one commit | none | +1 |
| S5 | Move the saved-state baseline into `EditorUiState` (council D3) | `EditorWorkspaceView.java:250-251`, `EditorUiState.java` | one baseline, survives `recreate()` | move the two fields; `AutosaveController` keeps its checkpoint baseline (different meaning) | `SettingsPreferencesTest` gains "save, change palette, no unsaved question" | one commit | low | 0 |
| S6 | Restate the history step size and the other misleading comments | `forgeshape_history.h:27`, `cad_body.h:1-2, :18-23, :56`, `sketch_session.h:1-18`, `sketch.h:300-301`, `CadFeatureEditorView.java:10-18, :120-124`, `cad_extrude_tool.h:22-35, :163-164`, `selection_pulse.h:23,42`, `EditorWorkspaceView.java:2437`, `ARCHITECTURE.md:160`, `CLAUDE.md:1216`, `PRODUCT.md:757-768, :1957`, `region.h:30`, `sketch_session.h:394, :606`, `MODULE_MAP.md` counts | none | docs only | `CI FAST` whitespace check | one commit | none | 0 |
| S7 | Share one anchored value editor | new `AnchoredValueEditor.java`; `CadExtrudeCanvasView`, `BodyDimensionLabelsView`, `SketchDimensionLabelView` | every id, every description, every submit path; adds the missing `EditText` label once | extract the `TextView`+`EditText`+Apply+parse block; the three views delegate | JVM: a parse/format test for the editor; device: `CadCanvasExtrudeTest e2eCaduxs1_04`, `SketchUxTest` dimension, `BodyDimension*` (existing) | one commit per view | low | −~400 |
| S8 | Delete the duplicated profile-listing loop | `SketchEditorView.java:399-412`, `EditorWorkspaceView.java:3609-3618` | same list | one helper on `SketchEditorView` or the coordinator | `SketchExtrudeTest` | one commit | none | −~15 |

## BEFORE THE NEXT CAD FEATURE — debt that otherwise compounds

Order matters: B1 and B2 change what the OWNER sees and should follow the
OWNER decisions in `OWNER_DECISIONS.md`; B3–B6 are pure engineering.

| # | Goal | Files likely touched | Invariants preserved | Migration | Tests | Rollback | Risk | Size |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| B1 | **Region selection = union of atomic regions** (`PROFILE_SELECTION_AUDIT.md` §5) — needs OWNER D2 | `forgeshape_sketch_region.{h,cpp}` (drop the share-a-loop refusal `:329-334`; add `mergeSelectedRegions` by the parity rule), `forgeshape_sketch_session.cpp:1294-1333` (pure toggle), `forgeshape_cad_feature.cpp:290-374` (one prism per merged component; face table per component), hatch, `DATA_PACKAGE_SPEC.md` §7f wording | `ProfileRegionRef` unchanged; `ProfileRegionMismatch` unchanged; touch/cross refusals unchanged; **no `CADB` byte changes**; the single-simple-region float path bit-identical | behind the pure function; `CADVS_REG_06/10` expectations flip from refused to a measured merged area | native: new `CADVS_REG_*` for R+A (one component, one hole), all three (no hole), A+B with R unselected; device: `owner_rectangle_circle_region` toggle semantic; corpus parity unchanged | the pure function is one file; revert restores the refusal | medium (semantics) | +~200 / −~30 |
| B2 | **Extrude annotation as a `Dimension` leader; drawn ≠ hit** (`EXTRUDE_HUD_AUDIT.md` §4 C) — needs OWNER D1, and S3 first | `forgeshape_cad_extrude_tool.{h,cpp}` (leader anchors, per-control anchor points), `forgeshape_sketch_session.cpp:1765-2075` (emit the leader into the `Dimension` range), `forgeshape_jni.cpp:4811-4902` (more slots or a second read), `CadExtrudeCanvasView`, `CadHudPresentation` (band constants; `glyphDp` widened; `hitDp` unchanged), `ViewportAnchorSpace` (drop the vestigial scale parameter) | truth, history, codec, fixtures, the 48 dp HIT floor, hidden-never-guessed, one projection | keep the cluster code path until the new one passes the same device class, then delete it | JVM `CadHudPresentationTest` (new band); native `CADUXS1_09_*` (new band, leader emission); device `CadVerticalSliceTest.compact_extrude_hud` and `Ui3dStateCorrectionTest.ui3dc1_11` re-pinned to the new numbers; OWNER physical-device review is the acceptance (Tier 4) | the old view class stays until the switch commit | medium (visual) | ±~600 |
| B3 | `CADB` version table | `forgeshape_project_document.cpp:329-392, 1097-1100, 1685-1688, 1928-1946` | every fixture byte-identical; the PowerShell encoder untouched | one ordered `{version, needs, readerFlags}` table replaces five predicates and the disjunction | corpus parity (44/44) + `FORGESHAPE_PROJECT_GOLDEN_SHA256*` + `CADVS_IO_22` | one commit | low | −~80 |
| B4 | JNI split by family, debug TU first | `forgeshape_jni.cpp` → `_debug`, `_selftests`, `_render_thread`, `_input`, `_sketch_cad`, `_scene`, `_sculpt`, `_project`, `_display`, `_lifecycle` + `jni_common.h`; `CMakeLists.txt` source list; `NativeViewportDebug.java` (debug source set) | every export name and code; `g_stateMutex` lock order; the render-thread handshake | one TU per commit, in the order listed (debug and self-tests carry zero product behaviour, so they go first); `touchEvent` moves whole before it is restructured | Tier 1 after each commit; the CI DEVICE startup capture (23 tokens) after `_selftests`; `CadVerticalSliceTest` after `_sketch_cad` | each commit is a rollback point | low per commit; the `_input` move is the one to do alone | 0 removed, ~8000 moved |
| B5 | One process-scoped session reset + native Sculpt seed | new `forgeshape_session_reset.{h,cpp}`; `closeProject` (`jni:6835`), `loadProject` (`jni:6330`), `loadProjectDocument` (`project_state.cpp:199-377`), `commitFirstCadProject`, new `seedSculptProject` beside it; `EditorWorkspaceView.java:2233-2262` shrinks to one call | close writes nothing; load all-or-nothing; a refused seed creates nothing; the bracket records nothing | write the reset function from the UNION of the five lists (close's list is the superset); the load path gains what it lacked (chooser, gizmo, selection gesture) — a deliberate behaviour tightening, stated | `HomeFlowTest`, `ProjectProcessDeathTest`, `ProjectAutosaveRecoveryTest`; native `PROJECT` suite | one commit for the function, one per route | low–medium (load path) | −~120 |
| B6 | `EditorWorkspaceView` → `ProjectLifecycleCoordinator` first | `EditorWorkspaceView.java:2299-2456, :4294-5030`; new coordinator; `EditorUiState` (after S5) | every listener contract; every id; the dirty-guard decision table | move methods with their fields; the view keeps a delegate; no layout code moves | JVM: a dirty-guard table test (pure); device: `ProjectTransferTest`, `ProjectAutosaveRecoveryTest`, `HomeFlowTest`, `EditorWorkspaceControlsTest` | one commit | low (no layout dependency in that region) | ~1200 moved |

## ONLY WHEN TOUCHING THAT AREA — deferred cleanup

| # | Goal | Trigger | Notes |
| --- | --- | --- | --- |
| T1 | Retire or generalise the base-only `cadState`/`cadApply*` door (OWNER D5) | the next Shape-panel or precision-surface task | keep the typed depth; route size edits through Edit Sketch, or add a `featureId` parameter; drop `g_lastCadApplyStatus` |
| T2 | `sketch_overlay.h` out from under `gizmo.h` | the next overlay or renderer change | the overlay type beside `render_mesh`; removes the camera/render ⇄ sketch module cycle |
| T3 | `constructionTransform()` accessor into `scene.cpp` | the next transform change | the leaf stops reaching up |
| T4 | `touchEvent` as an arbiter with one pointer-ownership state | the next new pointer consumer (Directional Scale, Through All picking, projected edges) | after B4 moved it whole; the state machine is the change, not the move |
| T5 | `SketchCadChromeCoordinator` and `ChromeRefreshScheduler` | the next sketch/CAD chrome task | after B6 proved the pattern |
| T6 | Read the cached candidate evaluation from chrome; evaluate only on the render thread | the first 16-feature chain measurement on a phone | `jni:4840` |
| T7 | A history step byte budget | the next `ConstructionHistory` change | mirror `SculptHistory`'s cap |
| T8 | Draw on change | needs the frame-counter tests to request frames explicitly; do with a renderer task, measured on the physical device | council E1 |
| T9 | Feature-id high-water mark in `CadBodyState` | the first delete-feature API (`NEXT_VERTICAL_SLICE_OPTIONS.md` Option 5) | `CADB` v6 rides with sketch identity |
| T10 | The hardware-key debug console out of `ForgeShapeActivity` | the next Activity change | `onKeyDown:402-426` |

## DO NOT DO

| Refactor | Why not |
| --- | --- |
| A planar-arrangement region model (splitting touching or crossing loops into faces) | changes what every v1–v5 `CADB` record means; needs a robust 2D arrangement kernel; O2 does not need it (`PROFILE_SELECTION_AUDIT.md` §5.3) |
| Storing the merged boundary (`[{rect, {B}}]`) instead of the selected atomic regions | breaks the "a loop appeared inside my selection" detection that `ProfileRegionMismatch` gives for free, and makes two encodings of one solid |
| Renderer-drawn text for the value or captions | a font in the renderer with no third-party library; unreadable at zoom-out unless banded anyway; loses TalkBack; `EXTRUDE_HUD_AUDIT.md` §4 B |
| Rendering the HUD as `setScale`d Android views | scales the hit area with the glyph — the exact coupling that produced O1 in reverse |
| Merging the two face-support models (`TopoRef` and `CadFeatureSupport`) into one struct | they answer two different questions (where does this BODY stand / where does this FEATURE's sketch stand); the product ambiguity is the OWNER's rule to set (D4), not a struct to fold |
| A Java-side feature/sketch tree model | a second truth; the feature list already re-reads native per refresh, which is right |
| Replacing snapshot history steps with typed inverse commands | the recorded reason stands (`history.h:40-52`); a byte budget is the proportionate fix |
| Splitting `forgeshape_jni.cpp` by line count rather than by family | a split that leaves `g_stateMutex` semantics ambiguous is worse than the monolith |
| A "big bang" `EditorWorkspaceView` decomposition | the layout decision runs in `onMeasure` (recorded debt); extract the lifecycle coordinator, prove the pattern, then the sketch/CAD chrome |
| Removing the `*ForTest` accessors on `EditorWorkspaceView` | they are the price of "locate by id, never by coordinate"; keep them, keep them package-private |
