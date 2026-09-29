# Council findings (FUNCTION-COUNCIL-R1)

Six independent evidence passes over `a1b50f2`, each from one system boundary.
No personas and no scores: each seat below is a boundary, and each position is
tied to source or to a recorded run.

## 0. How the passes ran, honestly

| Seat | Boundary | Completion |
| --- | --- | --- |
| A | Product / CAD workflow | **partial** — stopped by a usage limit; its findings F1–F4 were re-verified in source by the coordinator |
| B | Sculpt workflow | **partial** — interim notes only; the load-bearing ones re-verified |
| C | Domain / persistence / histories | **complete** (first pass, recovered from its transcript); one claim was wrong and is corrected below |
| D | Android UI / input / lifecycle | **partial**; its candidate bug and dead-export count re-verified |
| E | Renderer / Vulkan / performance | **partial but substantial**; the render-loop and overlay findings re-verified |
| F | Test / CI / cost | **complete** (first pass) plus a partial second pass; timings replaced by measurements from the C4 aggregate log |

Two launches of the seats were lost to usage limits. Everything written as a
fact in these artifacts was either verified by the coordinator in source or is
marked as a seat's unverified report.

**One seat claim was wrong.** Seat C reported that the eight `CADB` v5 digests
are printed but not asserted. `CADVS_IO_22_every_v5_fixture_matches_the_independent_corpus_digest`
(`forgeshape_cad_feature_selftest.cpp:2398-2402`) compares all eight against
committed values. What is stale is `DATA_PACKAGE_SPEC.md:1124`, which still
calls that self-test UNVERIFIED.

## 1. Defects found (not fixed; this is an audit)

| # | Defect | Evidence | Severity | Likely owner | Runtime verification |
| --- | --- | --- | --- | --- | --- |
| D1 | **Import GLB is reachable and unguarded in Sculpt.** The import makes the first imported body active while the mode stays Sculpt, and records a Construction step while Construction history is otherwise refused in Sculpt | Project menu visible in Sculpt (`GlobalToolbarView.java:631`); no mode check in `onImportGlbRequested` / `applyImportedGlbBytes` (`EditorWorkspaceView.java`), `importGlbDurable` (`forgeshape_jni.cpp`) or `commitImportedGlbScene` (`forgeshape_import_commit.cpp:174` sets the active body) | **high** (an invalid mode/target combination) | `forgeshape_import_commit` + JNI guard; chrome withdrawal | needed: one device case, Sculpt → Import |
| D2 | **The navigator's Flip and ±90° do not move the camera.** | `beginSketchView` reads `sketchSession().frame()` (`forgeshape_jni.cpp:311`); `viewFrame()` (`forgeshape_sketch_session.cpp:372`) has no production caller; `SketchUxTest` asserts only the native bits | medium (a drawn control with no visible effect) | JNI camera adapter / `SKETCH-UX-R1` C | needed: assert the camera pose after Flip |
| D3 | **A palette change forgets that the project was saved.** | `persistedFingerprint` / `everPersisted` are view fields (`EditorWorkspaceView.java:250-251`), not in `EditorUiState`; `requestTheme` carries `EditorUiState` and calls `recreate()` (`ForgeShapeActivity.java`) | medium (a spurious Save/Discard question; never data loss) | `EditorWorkspaceView` / `EditorUiState` | needed |
| D4 | **Undo after an over-cap stroke can land on a state never on the branch.** | `SculptHistory::record` clears redo and keeps undo for a `NotRetained` stroke (`forgeshape_sculpt_history.cpp:88-97`); entries hold absolute positions | low–medium (dense meshes only: over 1 MiB per stroke) | `SculptHistory` | needed on a dense mesh |
| D5 | **The sketch overlay can stay stale after a pinch zoom.** | `SketchSession::overlay` rebuilds on a `worldPerUnit` change without bumping `overlayRevision_` (`forgeshape_sketch_session.cpp:1759-1761`); the renderer skips an upload when revision and vertex count match (`forgeshape_renderer.cpp:1600-1603`) | low–medium (grid step and markers lag until the next authoring touch) | `SketchSession` | needed |
| D6 | **Clear Mask can be drawn and then refused.** | `canClearMask` omits the size test (`forgeshape_sculpt.cpp:1687-1690`); `ARCHITECTURE.md:3265-3268` says the control is absent | low (masks over the entry cap) | `SculptSession` | not needed; source is exact |
| D7 | **`closeProject` does not refuse an open Construction edit**, while load does | `forgeshape_jni.cpp:6822-6844` vs `forgeshape_project_state.cpp:202-203` | low (no shell path reaches it mid-drag today) | JNI | not needed |

Recorded debts that the passes confirmed are still open (not new): the load
path can reopen a hidden sculpted body into Sculpt; `resolveWorldModel`
re-extracts profiles per frame for face-supported bodies (F-07); the
`SpatialSketchTest` order dependence; whole-mesh sculpt republish.

## 2. Material issues, seat by seat

### I-1 The process is the test fixture

- **F (tests).** Evidence: all 9 tests that stopped C1–C3 were test defects;
  `resetToBaselineConstruction` closes only a project without a Construction
  body; no test orchestrator. Concern: a partition change moves failures around
  (`SpatialSketchTest` passed in C4 only because its predecessors changed).
  Direction: close and reopen by default. Trade-off: a Tier 5 trigger, and
  about a second more per class.
- **C (domain).** Evidence: the process-scoped function-statics
  (`constructionScene()`, `sketchSession()`, `sculptSession()`, …). Concern:
  five project routes reset five different hand-written lists. Direction: one
  "reset every process-scoped session" function the routes share. Trade-off:
  touches the load path.
- **D (Android).** Evidence: `EditorWorkspaceObjectsTest` deliberately
  inherits. Concern: a blanket reset breaks the one class that wants
  accumulation. Direction: an explicit opt-out.

### I-2 Almost half of the device time proves chrome geometry

- **F.** 21 `EditorWorkspace*` classes, 328 tests, 29.5 of 65.4 measured
  minutes; two C1 failures were stale literal counts there. Direction:
  Tier 5, not per task. Trade-off: a chrome regression is caught later.
- **D.** They need a real `View` tree, but no GPU and no user. Direction: a
  JVM layout harness would make them cheap. Trade-off: a new test dependency
  (Robolectric or an inflate-only harness) — a decision, not a refactor.
- **A (product).** The chrome is where the OWNER's own review catches what
  matters (legibility, reach). Direction: Tier 4 carries the visual half.

### I-3 One coordinator owns too much of the Android layer

- **D.** `EditorWorkspaceView`: 5460 lines, ~224 methods, 78 fields, 20
  implemented listener interfaces, 97 distinct native calls, 27 child views,
  `syncFromNative()` from 22 call sites. D3 lives here because session memory
  is split between the view and `EditorUiState`. Direction: extract the
  project-lifecycle coordinator (save, open, recover, dirty guard) first; it
  has the clearest seam. Trade-off: the layout decision deliberately runs in
  `onMeasure` (recorded debt), so layout extraction is riskier than lifecycle.
- **C.** The native side mirrors it: `forgeshape_jni.cpp` is 8087 lines
  against the ~5900 recorded at DEEP-AUDIT-R1. Direction: split by family
  (sketch/CAD, sculpt, project, render loop); no behaviour change.

### I-4 CAD truth is single; its doors and dependency models are not

- **A.** The chain is the authority: `CadBody::applyState` validates and
  regenerates the whole chain and writes nothing on refusal. But a face sketch
  has two models (`TopoRef` for another body, `CadFeatureSupport` for the same
  body), a base rectangle has two editing doors (Shape panel `cadApply*` and
  Edit Sketch), and "the preview IS the candidate" holds by determinism.
  Direction: keep both support models (they mean different things), retire the
  second editing door in a later CAD task. Trade-off: the Shape panel is the
  fastest route to a typed depth.
- **E.** The first evaluation after a drag sample runs on the UI thread under
  the state lock (`forgeshape_jni.cpp:4840`); `PERF_NOTES.md` §3 says the render
  thread does it. Concern: a 16-feature chain on a phone has never been timed.
  Direction: read the evaluation's cached result from chrome, never trigger it.

### I-5 The renderer is derived-only, but it never rests

- **E.** The render loop waits only when no surface is attached
  (`forgeshape_jni.cpp:1750-1754`), so a static scene redraws at the display's
  rate; every upload drains the GPU; the render-mesh rebuild was 104 ms at 100k
  vertices on the physical S25 Ultra. No presentation value was found feeding
  back into truth. Direction: draw on change, and a dirty-range upload for
  sculpt. Trade-off: the evidence waits count presented frames, so "draw on
  change" needs the tests to request frames.
- **B (sculpt).** Today's ~500-vertex seed is free; density is where it bites.

### I-6 Persistence is correct and expensive to extend

- **C.** Five `CADB` versions, each added by hand in about seven files (spec,
  predicate, reader flag, version check, PowerShell encoder, fixtures,
  digests). No partial-apply path; every older fixture byte-identical.
  Direction: one ordered version table in the codec that drives both the
  writer's choice and the reader's flags. Trade-off: the independent PowerShell
  encoder is the point of the corpus and stays.
- **F.** Corpus parity costs 7 s in `CI FAST`; the cost is authoring, not CI.

### I-7 Validation cost and the claim "push and CI tests it"

- **F.** A task-branch push triggers neither workflow: both run on pushes to
  `main` and `infra/ci-cloud-r1` and on PRs to `main` only
  (`ci-fast.yml:12-17`, `ci-device.yml:10-15`), while `docs/CI_CLOUD.md` and
  `CLAUDE.md` say "push the task branch and let GitHub Actions test it". The
  native suites never run in `CI FAST`. Direction: the tiers in
  `LEAN_VALIDATION_POLICY.md`.
- **E.** Emulator frame times say nothing about a phone (SwiftShader).
  Performance acceptance stays Tier 4 on the physical device.

## 3. CONSENSUS

1. **Each durable fact has one owner**, and the Android layer holds no project
   truth: every refresh re-reads native. The exceptions are session memory (D3)
   and caches, not second truths.
2. **The two histories are separate and correctly bounded in scope**; the CAD
   chain is the single authority for a CAD body.
3. **The renderer is derived-only.** Nothing presentational reaches a `.forge`
   byte, a history step or the fingerprint.
4. **The last three aggregates found test defects, not product defects.**
   Ordinary feature work should get a Tier 1 + Tier 2 answer in ~12–20 minutes
   and go to the OWNER; the aggregate is for named situations.
5. **D1 is the one finding that should be fixed before the next Sculpt or
   Import feature**; D2 and D3 before the next CAD sketch or Settings task.
6. **OWNER hands-on review is required** for every visual, ergonomic and feel
   question; no emulator run closes a physical-device gate.

## 4. DISAGREEMENTS

1. **Where the chrome tests should go.** F wants them out of the per-task path
   now (Tier 5). D wants them made cheap (a JVM harness) so they can stay
   per-task. A says their real job is the OWNER's. The measured fact both sides
   accept: 29.5 minutes today.
2. **Harden the fixture first, or build features first.** F and C see test
   isolation as the recurring cost behind every aggregate. A sees the CAD
   precision gap (snapping, Through All) as what the OWNER will hit next. The
   tiers work either way if Tier 2 lists put the class under test first.
3. **Continuous rendering.** E counts it as waste (battery, thermal). The test
   seat depends on presented-frame counts that assume frames keep coming. A
   change needs both.
4. **One or two face-support models.** A (product) sees two concepts the user
   cannot tell apart ("sketch on this face" makes a second body or changes this
   one, depending on whose face). C (domain) sees two legitimately different
   dependencies. Today's UI decides by which body the face belongs to.

## 5. OWNER DECISION REQUIRED

Only product choices; implementation trivia is not listed.

1. **Adopt the lean validation tiers** (`LEAN_VALIDATION_POLICY.md`), including
   that FullSharded is no longer a per-feature gate and that visual-evidence
   classes leave the pass/fail path.
2. **Should a JVM layout test dependency be allowed** (for example Robolectric)
   so the chrome tests can become cheap? `CLAUDE.md` restricts runtime
   libraries; a test-only dependency still needs a decision.
3. **A face sketch on another body: New Body only, or Add/Cut into that body?**
   Today a sketch on another body's face makes a second, following body; on the
   same body's face it offers Add and Cut. Which does the OWNER expect?
4. **Next vertical slice** — the options are in `NEXT_VERTICAL_SLICE_OPTIONS.md`.
5. The standing questions are unchanged and still the OWNER's: OQ-01, OQ-02,
   the hover preview, `kMaskGain`, the Crease fractions, and the OWNER-LATER
   aesthetics lists.

## 6. SAFE INTERNAL CLEANUPS

No user-visible semantics change for any of these. Each is its own small task.

1. Make `beginSketchView` read `viewFrame()` — strictly this is D2's fix, and
   it changes what the user sees; listed here only because it is one line.
2. Bump `overlayRevision_` when `overlay()` rebuilds for a new `worldPerUnit` (D5).
3. Give `canClearMask` the size test, or correct `ARCHITECTURE.md` (D6).
4. Remove the six unused JNI exports and two dead helpers
   (`signedAreaTwice`, `forgeshape_sketch_region.cpp:50`;
   `pointStrictlyInside`, `forgeshape_sketch.cpp:151`).
5. Put the release-exported debug readers and the Imported Mesh Preview seam
   behind `#ifndef NDEBUG`, like the other debug entry points.
6. One shared "reset process-scoped sessions" function for the five project
   routes.
7. Documentation corrections: `DATA_PACKAGE_SPEC.md:1124` (the v5 digests are
   asserted); `PRODUCT.md:1954` ("There is no Mirror"), the Objects list "does
   nothing else", and "Nothing anywhere reads … hover"; `ARCHITECTURE.md`'s
   step size ("twenty doubles per body", `forgeshape_history.h:27` too), the
   Clear Mask sentence, the normal-matrix and blended-pipeline statements
   (renderer seat), the recessed-Export and "no scene delete" sentences (Android
   seat); `PROJECT_STATUS.md`'s `beginSketchView` line number (302 → 311) and
   its `SpatialSketchTest` prediction; `docs/CI_CLOUD.md`'s "push it; GitHub
   Actions tests it"; the fingerprint's sculpt proxy stated in `CLAUDE.md`.
