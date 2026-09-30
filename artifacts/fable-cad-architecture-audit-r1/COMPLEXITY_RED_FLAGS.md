# Complexity red flags (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Read-only. Baseline `main = 509de02`. Classifications: `APPROPRIATELY_SIMPLE`,
`JUSTIFIED_COMPLEXITY`, `OVERENGINEERED`, `UNDERENGINEERED`, `DUPLICATED`,
`LEGACY_DRIFT`, `HIDDEN_COUPLING`, `UNVERIFIED`. Evidence tags as in the other
artifacts. Answers required conclusions **C7, C8, C9** and the OWNER concern
**O4** ("we keep adding, correcting and wrapping things").

## 1. Workflow classification

| Workflow | Class | Why (evidence) |
| --- | --- | --- |
| Sketch entities, loop extraction, fail-closed refusals | `APPROPRIATELY_SIMPLE` | One walker, one refusal vocabulary, one anchor rule (`cpp/forgeshape_sketch.cpp:1076-1149`); every refusal has a name and a test. |
| Region nesting (`extractSketchRegions`) | `JUSTIFIED_COMPLEXITY` | Two n×n matrices and a parent rule are the minimum for even/odd nesting without a planar arrangement (`sketch_region.cpp:170-260`). |
| Region SELECTION rule (`validateRegionSelection` + `toggleRegion`) | `UNDERENGINEERED` | The one sentence "a selection is a set of atomic regions, extruded as their union" is missing; instead a refusal (`:329-334`) plus a silent drop in the session (`sketch_session.cpp:1318-1327`) stand in for it. Failure mode: the OWNER cannot express rect + A, and a tap changes a region the user did not touch (`PROFILE_SELECTION_AUDIT.md` §3.3). Missing invariant: the parity-merge rule. Boundary: `forgeshape_sketch_region` (derive the merged boundary loops). |
| Multi-region prism build | `UNDERENGINEERED` (latent) | One prism per region concatenated, no union, `components = chosen.size()` (`cad_body.cpp:650, :717-719`); safe only because the selection rule forbids adjacency. Once adjacency is allowed the merge must happen in 2D first (same fix as above). |
| Extrude extent (`OneSide` / `Symmetric` / `TwoSides`) | `JUSTIFIED_COMPLEXITY` | Two positive distances, one canonical form per mode, one pure transition function (`cad_body.h:100-215`); every vendor stores this differently and ForgeShape's rule is the least ambiguous (`CAD_BENCHMARK.md` §4). |
| Extrude direction (Flip) | `APPROPRIATELY_SIMPLE` | A side, never a sign; refused outside One Side (`sketch_session.cpp:1451-1465`). |
| Feature chain regeneration | `JUSTIFIED_COMPLEXITY` | Ordered, atomic, one kernel boolean per later feature, named failure id (`cad_body.cpp:586-729`). The double-precision prism-then-boolean design is what makes flush faces work. |
| Support faces (two mechanisms: cross-body `TopoRef`, same-body `CadFeatureSupport`) | `JUSTIFIED_COMPLEXITY` at the code level, `HIDDEN_COUPLING` at the product level | Both are semantic, lineage-checked, fail-closed. But the user cannot tell from the canvas whether a face sketch will make a NEW following body or Add/Cut into THIS one: it depends on whose face was tapped (`OWNER_DECISIONS.md` D4). |
| Feature retention and reopening | `APPROPRIATELY_SIMPLE` | One staged path (`beginEdit` forwards to `beginEditFeature`, `sketch_session.cpp:226-229`), one commit, one refusal vocabulary. |
| Retained sketches | `UNDERENGINEERED` | No sketch identity, no visibility, no viewport presence after commit (`SKETCH_FEATURE_HISTORY_AUDIT.md` §3.5, §5). Failure mode: nothing to point at; reuse impossible by construction. Missing invariant: "a sketch is an object the body owns; a feature consumes it by reference". Boundary: `CadBodyState.sketches[]` + `CADB` v6. |
| Feature list UI | `UNDERENGINEERED` (discoverability) | Exists only in the precision surface's Shape body and is withdrawn for one-feature bodies (`CadFeatureEditorView.java:237-240`); no canvas path, no face → feature index. |
| Base-only typed fields (`cadState` / `cadApply*`) | `LEGACY_DRIFT` + `DUPLICATED` | Pre-chain API still live as a second door to feature 1 only (`jni:5574-5600, :5731-5752`; `CadFeatureEditorView.java:315-327`), with a process global `g_lastCadApplyStatus` (`jni:5624`); a later feature's depth has no typed field. |
| Extrude HUD (anchors, scale band, manipulator) | `JUSTIFIED_COMPLEXITY` | Pure functions, camera-free anchors, one projection, frozen drag basis (`forgeshape_cad_extrude_tool`). |
| Extrude HUD (Android cluster) | `OVERENGINEERED` for what it delivers, and `HIDDEN_COUPLING` | 1228 lines of `CadExtrudeCanvasView` to place four controls; the 48 dp hit container doubles as the drawn box, so an accessibility floor became the visual policy (`EXTRUDE_HUD_AUDIT.md` §2 Q1). The overlay reads `metersPerPixel` at the world origin and the HUD at the anchor base (`jni:1859-1862` vs `:4854`): one formula, two inputs. |
| On-canvas exact values (three anchored views) | `DUPLICATED` | `CadExtrudeCanvasView`, `BodyDimensionLabelsView`, `SketchDimensionLabelView` each re-implement `EditText` + IME + parse + submit + `placeAt`; the anchor contract is single. |
| Viewport anchors (`ViewportAnchorSpace`, `projectWorldToScreen`) | `APPROPRIATELY_SIMPLE` | One native projection, one Java placement, runtime `getLocationInWindow` (no constants). |
| Candidate preview ("the preview IS the candidate") | `JUSTIFIED_COMPLEXITY`, with one `HIDDEN_COUPLING` | Latest-only evaluation keyed by revision, render-thread substitution (`jni:1655-1709`). The first evaluation after a drag sample runs on the UI thread under the state lock (`cadExtrudeToolState`, `jni:4840`), which `PERF_NOTES.md` §3 attributes to the render thread. Untimed for a 16-feature chain on a phone. |
| Add / Cut commit | `APPROPRIATELY_SIMPLE` | One `ScopedConstructionEdit` around one `applyState` on the same `SceneObject` (`sketch_session.cpp:1681-1737`). |
| Sketch editing (staged, Finish = one transaction) | `APPROPRIATELY_SIMPLE` | |
| Project save / open / recover | `JUSTIFIED_COMPLEXITY` | All-or-nothing decode, temp+fsync+rename, quarantine of a corrupt candidate. |
| `CADB` versioning | `OVERENGINEERED` in mechanism (not in policy) | Each version is added by hand in about seven places (`project_document.cpp:329-392, 1097-1100, 1685-1688, 1928-1946`; spec; PowerShell; fixtures; digests). Simpler: one ordered version table drives the writer's choice and the reader's flags; no bytes change. |
| Dirty-state tracking | `DUPLICATED` | Two baselines (`EditorWorkspaceView.java:250-251` vs `AutosaveController.checkpointedFingerprint`), the first lost on `recreate()` (D3, open). |
| Construction history steps | `JUSTIFIED_COMPLEXITY`, `LEGACY_DRIFT` in its statement | Snapshot steps buy atomicity for creation/deletion; but a CAD step now copies a whole `CadBodyState` while `history.h:27` still says "twenty doubles per body". No byte cap. |
| Sculpt history | `JUSTIFIED_COMPLEXITY` | Bounded, per body, delta-based, two caps, navigator derived from the deques. |
| Sculpt stroke publishing | `UNDERENGINEERED` (deferred by policy) | Whole-mesh republish per move and every upload drains the GPU (`renderer.cpp:868-986`); fine at ~500 vertices, 104 ms at 100k on the S25 Ultra (recorded). |
| Render loop | `UNDERENGINEERED` (deferred by policy) | Never rests while a surface is attached (`jni:1750-1754`); a still scene redraws at display rate; face-supported bodies re-derive their chain geometry per snapshot (`scene.cpp:219-262`). |
| Renderer uploads and revision gates | `APPROPRIATELY_SIMPLE`, one defect | Revision-keyed uploads; the overlay rebuild on `worldPerUnit` change skips `touchOverlay()` (D5, `sketch_session.cpp:1759-1761`). |
| Input arbitration | `OVERENGINEERED` by accretion | One 715-line `touchEvent` (`jni:7370-8085`) decides gizmo, sketch, chooser, sculpt pending-then-promote, orbit and tap with 8 arbitration globals. Simpler: an arbiter unit with one "who owns this pointer" state machine; the per-consumer handlers already exist below it. |
| Project reset / test reset infrastructure | `DUPLICATED` + `HIDDEN_COUPLING` | Five hand-written reset lists (`CODE_ARCHITECTURE_MAP.md` §7); the load path resets less than close; the Sculpt bootstrap is Java-choreographed; tests reuse the inherited process. |
| Test-only JNI surface | `OVERENGINEERED` | 54 of 200 exports exist for tests; 18 test-only exports ship unguarded; the Imported Mesh Preview is a second import destination with no UI kept for two test classes. |
| `EditorWorkspaceView` coordination | `OVERENGINEERED` by accretion | 226 methods, 20 listeners, 97 natives over 12 families, `syncFromNative` from 22 sites fanning out to 12 views. Every feature adds to it. |
| `NativeViewport` slot contracts | `JUSTIFIED_COMPLEXITY` | A `double[32]` with a documented slot list is the cheapest zero-allocation wire for a per-frame read; 388 constants are the price. |
| Renderer / control coupling | `APPROPRIATELY_SIMPLE` | The renderer sees a `SceneSnapshot`, a `SketchOverlay` line list and push constants; no CAD header, no Java. |

## 2. OVERENGINEERED items — the simpler design, what disappears, what remains

| Item | Simpler design | What code disappears | What behaviour remains |
| --- | --- | --- | --- |
| Extrude HUD cluster as 48 dp drawn boxes | Drawn glyph child sized by the band, transparent 48 dp hit proxy, value oriented along a `Dimension`-style leader (`EXTRUDE_HUD_AUDIT.md` §4 C) | the row capsule, `valueCentreOffset`/`clampedStart`/`paletteCentreY` cluster arithmetic (`CHP:292-327`), the "60 dp pill" history comments; ~300 lines of `CECV` | every act, every description, every test-id, the 48 dp floor as a HIT floor |
| `CADB` per-version hand edits | One ordered `{version, needsPredicate, readerFlags}` table in the codec | five `cadDocumentNeedsV*` predicates, the `versionOk` disjunction, per-version reader flag plumbing | identical bytes for all 44 fixtures; the PowerShell encoder stays as the independent implementation |
| `touchEvent` monolith | `forgeshape_jni_input.cpp` with one pointer-ownership state (`None / Gizmo / Sketch / Chooser / SculptPending / Sculpt / Camera`) and a dispatch table | the 8 arbitration globals become one struct; ~715 lines move, ~150 disappear as duplicated "cancel the others" sequences | every gesture rule (multi-touch never mutates sculpt, sketch owns the single finger, Ready navigates) |
| Test-only JNI surface | `NativeViewportDebug` in its own `#ifndef NDEBUG` TU | 54 exports leave the product library; the preview family and the report probes leave with them; `touchEvent:7927` loses its per-tap preview check | every test, unchanged in intent (callers re-point to the debug class) |
| `EditorWorkspaceView` as the one coordinator | `ProjectLifecycleCoordinator`, `SketchCadChromeCoordinator`, `ChromeRefreshScheduler` (`TARGET_MODULAR_ARCHITECTURE.md`) | nothing disappears; ~2000 lines move; 22 `syncFromNative` sites become one call with a reason | every refresh, every listener contract |
| Five reset lists | `forgeshape_session_reset.{h,cpp}` `resetProcessScopedSessions()` | four of five lists; the Sculpt bootstrap's Java choreography becomes `seedSculptProject()` beside `commitFirstCadProject` | "close writes nothing", "load is all-or-nothing", "a refusal creates nothing" |
| Three anchored value editors | one `AnchoredValueEditor` (a `TextView` + swap-in `EditText` + Apply, parse-and-report, IME) shared by the three views | ~400 lines across `CECV`, `BDLV`, `SDLV` | every submit path; adds the missing `EditText` accessibility label once for all three |

## 3. UNDERENGINEERED items — failure mode, missing invariant, proper boundary

| Item | Failure mode | Missing invariant | Proper boundary |
| --- | --- | --- | --- |
| Region selection semantics | rect + A cannot be expressed; a tap silently drops the ring; the same combination loads as `OverlappingRegions` from a file | "a selection is any subset of atomic regions; what is extruded is their union, derived by the parity rule" | `forgeshape_sketch_region` (merge), `SketchSession::toggleRegion` (pure toggle), `appendCadFeatureSolid` (one prism per merged component) |
| Multi-region prisms without a 2D merge | coincident inner walls the moment adjacency is allowed; `components` counts regions, not shells | "prisms are built from merged boundary components" | same as above |
| Retained sketch identity | cannot reuse a sketch; cannot show or point at a later feature's sketch; the canvas chip reopens only the base | "a sketch is an owned object with an id; a feature references one" | `CadBodyState.sketches[]`, `CadFeature.sketchId`, `CADB` v6, a sketch visibility flag (session or truth — OWNER decision D3) |
| Origin-vs-base `metersPerPixel` | in perspective, a face sketch draws its arrow head at one scale and hit-tests and sizes glyphs at another | "one `mpp`, read once at `anchors.base`, feeds the overlay, the hit test and the HUD" | the JNI render loop (`jni:1859-1862`) must ask the session for its anchor depth, or the session must compute its own scale |
| `nextCadFeatureId` from `laterFeatures.back()` | id reuse the day a delete-feature API exists (`cad_body.cpp:349-352`) | "feature ids are never reused" (a stored high-water mark, as `ObjectId` already has) | `CadBodyState` |
| Regeneration cost on the UI thread | a long chain stalls the UI on the first sample after a drag (`jni:4840`); untimed on a phone | "chrome reads the cached evaluation; only the render thread evaluates" | `SketchSession::evaluateCandidate` callers |
| Dirty-state baseline | a palette change makes a saved project read unsaved (D3) | "one saved-state baseline, in the object that survives `recreate()`" | `EditorUiState` |
| Overlay revision on zoom | grid step and markers lag after a pinch (D5) | "every overlay rebuild bumps the revision" | `SketchSession::overlay` |
| Render loop never rests | battery and heat on a still scene; unmeasured on a phone | "draw on change, with an explicit frame request for evidence waits" | the render thread + the two frame counters the tests use |
| History step size | a 16-feature chain with dense splines makes each step large; no cap | "a step has a byte budget" (as `SculptHistory` already does) | `ConstructionHistory` |

## 4. DUPLICATED / LEGACY_DRIFT / dead — the C7 list

| Item | Where | Verdict |
| --- | --- | --- |
| `cadState`, `cadApplyExtrude/Rectangle/Circle`, `g_lastCadApplyStatus` | `jni:5574-5600, :5624, :5731-5752`; `CadFeatureEditorView.java:315-327` | pre-chain base-only door; retire into staged Edit Sketch, or generalise with a `featureId` (OWNER decision D5) |
| `applyCadExtrude/Rectangle/Circle` in `cad_body.cpp:808-855` | no production caller (`INFERRED`) | dead in production |
| `sketchSelectProfile` (R0) beside `sketchToggleRegion` (no caller) | `jni:4304`, `:4647`; `SketchEditorView.java:509` | one should go |
| `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`, `supportChooserConfirm`, `supportChooserSelect`, `sketchToggleRegion` | 0 callers anywhere | dead exports (already found by `FUNCTION-COUNCIL-R1`, still present) |
| `constructionUndo/Redo/Depth/Available`, `cancelConstructionEdit`, `constructionMeshRevision`, gizmo probes, `cadBodyMeasure`, `sketchCandidateMeasure`, … (54 total) | test-only | move to a debug TU |
| `signedAreaTwice`, `pointStrictlyInside`, `(void) loops` | `sketch_region.cpp:50`, `sketch.cpp:151`, `cad_feature.cpp:371` | dead |
| `orientation`/`onSegment` | `sketch_region.cpp:9-19` and `sketch.cpp:137-149` | duplicated helpers |
| Profile-listing loop | `SketchEditorView.java:399-412`, `EditorWorkspaceView.java:3609-3618` | duplicated |
| `measureAndPlace(…, scale)` | `ViewportAnchorSpace.java:176-190` | vestigial parameter |
| Three `EditText` editors | `CECV`, `BDLV`, `SDLV` | duplicated |
| Imported Mesh Preview family + `touchEvent:7927` check | `jni:6534-6760` | diagnostic kept alive in the product library with no UI |
| Stale comments | `cad_body.h:1-2, :18-23, :56`; `sketch_session.h:1-18`; `sketch.h:300-301`; `CadFeatureEditorView.java:10-18, :120-124`; `tool.h:22-26, :33-35, :163-164`; `session.cpp:2004-2010` (contradicted by code); `history.h:27`; `selection_pulse.h:23,42`; `EditorWorkspaceView.java:2437` and seven "used to be"; `ARCHITECTURE.md:160`; `CLAUDE.md:1216`; `PRODUCT.md:757-768, :1957`; `MODULE_MAP.md` counts; `region.h:30`; `sketch_session.h:394, :606` | drift; none is a truth-owner error, all are readability cost |

## 5. Hard vs easy — are we solving the wrong problem?

Columns: user value (H/M/L), real algorithmic difficulty (H/M/L), current
implementation complexity (H/M/L), risk, simpler alternative, verdict.

| # | Behaviour | Value | Real difficulty | Current complexity | Risk | Simpler alternative? | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | On-canvas exact values | H | L | H (three views, 1800 lines) | duplication drift; a11y gap | one shared anchored editor | **simplify** |
| 2 | Viewport anchors | H | L | L | none | — | keep |
| 3 | Extrude arrow + drag | H | M | M | origin/base `mpp` split | one `mpp` source | keep, fix |
| 4 | HUD visual scaling | H | L | H (six floors, fused hit/draw) | the OWNER's O1 | decouple drawn from hit; `Dimension` leader | **redesign** (presentation only) |
| 5 | Extrude direction (Flip) | M | L | L | none | — | keep |
| 6 | Extent modes | M | L | M (pure transitions) | none | — | keep |
| 7 | Region extraction (nesting) | H | M | M | none | — | keep |
| 8 | Nested holes | H | M | M | none | — | keep |
| 9 | Multi-region selection | H | L (parity merge over disjoint regions) | M, wrong sentence | O2; a tap drops the ring | parity-union of atomic regions | **redesign** (one rule) |
| 10 | Add / Cut | H | H (a real boolean) | M (vendored kernel behind one file) | kernel behaviour on coincident faces `UNVERIFIED` | — | keep |
| 11 | Feature regeneration | H | M | M | whole-chain regen per edit, untimed at 16 | cache per feature (later) | keep |
| 12 | Support faces | H | H (topological naming) | M (semantic tokens + lineage) | two models, one canvas | OWNER D4 | keep (decide the product rule) |
| 13 | Feature list | H | L | L | hidden two surfaces deep | canvas → feature index; card list | **simplify / expose** |
| 14 | Retained sketches | H | M | L (by value) | cannot reuse or show | sketch identity + visibility | **redesign** (data model, v6) |
| 15 | Sketch editing (staged) | H | M | M | none | — | keep |
| 16 | Project save / open | H | M | M | none | — | keep |
| 17 | `CADB` versioning | M | L | H (seven hand edits per version) | v6 is coming (three options need it) | version table | **simplify** before v6 |
| 18 | Dirty-state tracking | M | L | M (two baselines) | D3 | one baseline in `EditorUiState` | **simplify** |
| 19 | Render-loop updates | M | L | L (never rests) | battery | draw on change | simplify (measure first) |
| 20 | Renderer uploads | M | M | M | whole-mesh sculpt republish | dirty ranges | keep until density is a goal |
| 21 | Sculpt stroke publishing | M | M | M | same | same | keep until density is a goal |
| 22 | Construction history | H | M | M | step size unstated | byte budget | keep, restate |
| 23 | Sculpt history + navigator | H | M | M | none | — | keep |
| 24 | Test reset infrastructure | M | L | H (five lists, process fixture) | order-dependent tests | one reset function; close-and-reopen fixture | **simplify** |
| 25 | Input arbitration | H | M | H (715-line function) | every new consumer edits it | one arbiter unit | **simplify** |
| 26 | Test-only JNI surface | L | L | H (54 exports) | ships unguarded | debug TU | **remove from product** |

Two patterns fall out. **Easy behaviours solved through many layers:** the
HUD (four controls, six floors, three duplicate editors), the CAD codec
versioning, the reset lists, the test surface. **Hard geometry hidden behind
a shortcut:** none of the geometry is a shortcut — the kernel is real, the
regions are real, the lineage is real. The shortcuts are in the SENTENCES
around the geometry: "two regions may not share a loop" instead of "extrude the
union", "the hit box is the drawn box" instead of two sizes, "a sketch is a
field of a feature" instead of an object.

## 6. Test-driven distortion (E)

- **Legitimate seams**: the 23 self-test suites (debug-only, host-runnable),
  `debugInjectDeviceLoss`, the camera pose seams, `debugResetConstructionHistory`.
- **Test-only production pollution**: the Imported Mesh Preview family and
  its per-tap check; the seven unguarded `debug*` readers; `selectionOutlineStats`;
  the GLB report/fixture probes; `constructionUndo/Redo` beside `historyUndo`.
- **Debug export accidentally shipped**: the 18 unguarded ones above
  (`INFERRED` for a release build, none has been made).
- **Architecture that should not depend on tests**: the render-thread frame
  counters as the ONLY reason "draw on change" cannot land; the five reset
  routes shaped by an in-process fixture; the ~30 package-private accessors on
  `EditorWorkspaceView` (the acceptable price of id-based location).
