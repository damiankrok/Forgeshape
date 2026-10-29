# FUNCTION-COUNCIL-R2 — Architecture health

Audited tree `70a6c14f703bfda2ab689b855126c1d12a541e41` (C2 head; `app/src/main`
byte-identical to `f3db953`). Research only: nothing under `app/`, `scripts/`,
`.github/` or the owner documents was changed to produce this file.

**Inputs.** `SEAT_A.md` … `SEAT_J.md` (with Round 2), `work/req_*.tsv`,
`work/delib_*.md`, `work/VERIFICATION.md`, `SOURCE_REGISTER.md`,
`COUNCIL_DELIBERATION.md`, `COUNCIL_DEBT_RECHECK.md`, `CONTRACT_CONFLICTS.md`,
`PORTABILITY_MATRIX.md`, `evidence/`. Fresh run ids and their records are owned
by `TESTS.md`. Lines marked **(synth)** were re-read or re-grepped on this tree
for this file; the exact commands are in §8.

**Verification status.** `work/VERIFICATION.md` (the adversarial pass, three
lenses per finding, run `wf_8b7c4db9-993`) panel-verified the top 30 of 38
P0–P2 findings and **all 30 survived** (23 distinct root causes after
de-duplication). Every panel-verified defect cited here therefore carries both
its seat's post-Round-2 verdict and a surviving adversarial verdict; of the
ids this file cites, the panel-verified ones are A-01, A-02, A-04, A-05, A-06,
B-01, B-02 (= E-01), B-03, C-01 (= F-02), D-01, E-02, E-03, E-04, F-03, F-04,
F-05, G-01, G-02, H-05, H-06. Every other id cited here (C-03, D-05, D-07,
D-08, E-05, E-06, E-07, E-12, E-13 = H-02, G-03, G-04, G-05, G-06, G-07, H-03,
H-07, H-08, H-11, H-13, H-17, I-01, I-03, I-05, I-10, I-14, J-03, J-05) keeps
its seat grade only and is not panel-verified. `VERIFICATION.md` also judged **10 of 12** strongest
positive claims *overstated at their stated grade* — they remain supported at
E2 (source) but are not PASS-REPRODUCED. Three of those touch this file and are
qualified where they appear: Hidden enforced by one predicate in
`ConstructionScene::snapshot` (seat B), `.forge` load fail-closed with no partial
load (seat E), and the encoder canonical and matching the corpus (seat E).
`CONTRACT_CONFLICTS.md` now exists and owns the contract-level documentation
drift; the DOC-DIVERGENCE rows here are the architecture-side view of it.

Plan-source gap: the canonical Master Plan, `OWNER_DECISIONS_*`,
`MASTER_PLAN_HISTORY_*` and `PROJECT_STATE` are unavailable (`BEFORE.md`
"Authoritative sources"). Architecture rules below are taken from `CLAUDE.md`
and `ARCHITECTURE.md` only (**PARTIAL-PLAN-SOURCE-GAP**).

---

## 1. Module ownership — `ARCHITECTURE.md` versus the code

`ARCHITECTURE.md` "Ownership" (`:91-192`) assigns one owner per concern. The
rows below are the concerns where the seats or this synthesis compared that
assignment with the code. Rows not listed were not contradicted by any seat.

| Concern | Owner per `ARCHITECTURE.md` / `CLAUDE.md` | Found in code | Standing | Grade |
| --- | --- | --- | --- | --- |
| Domain is platform-neutral C++ (`ARCHITECTURE.md` "Platform boundary" `:194-220`) | everything below `forgeshape_jni.cpp` | 98 neutral TUs compile with clang 18 and against NDK libc++ on the host; 100 of 101 headers compile standalone (the one that does not is `forgeshape_renderer.h`); no `Uri`/`ContentResolver`/`content://` in `cpp/` | matches | E1 (`PORTABILITY_MATRIX.md` §5 P1-P3; seat A grep) |
| The ONE file that includes Manifold (`CLAUDE.md` hard rule) | `forgeshape_cad_kernel.cpp` | `grep -rl '#include.*manifold'` outside `third_party` → only that file **(synth)** | matches | E2 |
| `ANativeWindow` ownership (`ARCHITECTURE.md:214` "`Renderer` never creates or releases one") | JNI | `Renderer` takes ownership (`forgeshape_renderer.cpp:440`), releases in `detachSurface` (`:499-501`), re-acquires in `rebuildDeviceAfterLoss` (`:372`) | **DOC-DIVERGENCE** (G-03) | E2 |
| `surfaceDestroyed` "blocks until native code has released the `ANativeWindow`" (`CLAUDE.md` Android safety; `ARCHITECTURE.md` "Lifecycle contract") | JNI | `toCaller.wait_for(lock, 5 s, …)` (`forgeshape_jni.cpp:9487`), logs `acked=0` on timeout; safe today only because the renderer holds its own reference | **DOC-DIVERGENCE** (H-11) | E2 (safety argument E2+E5) |
| What the render thread does (`ARCHITECTURE.md` "Threading" `:4475-4476`: "Vulkan work, presentation and every mesh buffer create/copy/destroy"; CAD "Threading" `:3078-3083`) | render thread = GPU only; regeneration "at commit and at an Apply" | the render thread runs `SketchSession::evaluateCandidate()` (Manifold boolean included) inside `applyCadOperationPreviewLocked` (`forgeshape_jni.cpp:1725-1733` **(synth)**), rebuilds the camera-only overlay (4.2–4.5 ms host at 63×63), and re-resolves every face-supported placement per frame (`forgeshape_scene.cpp:281-326`) | **DOC-DIVERGENCE** (synthesis, from H-05/H-07/H-08); `artifacts/cad-vertical-slice-r1/PERF_NOTES.md:56` repeats the narrower claim | E2 (+E1 host timings, seat H) |
| Process-scoped objects (`ARCHITECTURE.md` "Lifecycle contract" `:4513-4516` names "the `ConstructionObject`") | — | the process-scoped object is `ConstructionScene` (`forgeshape_scene.cpp:406`); the text predates the scene | DOC-DIVERGENCE (stale noun, no behavioural consequence) | E2 **(synth)** |
| A face-supported body's world placement is DERIVED and the body is NOT independently transformable (`CLAUDE.md` `CAD-A3`) | `ConstructionScene::resolveWorldModel`, `SceneObject::isFaceSupportedCad` | `isFaceSupportedCad` is read only by Duplicate (`forgeshape_body_commands.cpp:205`) and a JNI verification seam (`forgeshape_jni.cpp:7676-7679`) **(synth)**; GLB export reads `body.transform()` (`forgeshape_gltf_export.cpp:178-180` **(synth)**); transform write and gizmo are not refused | **DEFECT-CONFIRMED** (E-01 P1, B-01 P2); `ARCHITECTURE.md:2606-2608` states the guard as present (B-03 DOC-DIVERGENCE) | E1 (host diags, re-run by seat I) + E2 |
| Which state may be lost on recreation (`ARCHITECTURE.md` Ownership: `EditorUiState` "every field is safe to lose") | `EditorUiState` for disposable state | `persistedFingerprint` / `everPersisted` are View fields (`EditorWorkspaceView.java:271-272` **(synth)**) outside `EditorUiState.carryAcrossRecreation`, and they are NOT safe to lose (A-05, A-06). `recoveryResolved` does live in `EditorUiState` (a static, `EditorUiState.java:151` **(synth)**) but is set only when a candidate existed (E-02), so the same recreation boundary leaks through it | ownership gap → DEFECT-CONFIRMED (RC-04) | E2 |
| Resetting process sessions when a project is established or closed | no single owner named | `loadProject` and `closeProject` reset different hand-written lists (`forgeshape_jni.cpp:8462-8480`, `:8968-8990`); `freeformEditSession()` is reset by neither; `supportChooser()` only by close | ownership gap → DEFECT-CONFIRMED P3 (E-07, E-12); chooser/Dimensions after load UNDETERMINED-SOURCE-GAP (U-2) | E1 (`diag_d2`) + E2 |
| Recovery checkpoint file I/O (`ARCHITECTURE.md` Ownership: `ProjectCheckpoint` "written atomically") | `ProjectCheckpoint` + `AutosaveController` | `AutosaveController` synchronises on its own instance (`AutosaveController.java:117-289` **(synth)**), `ProjectCheckpoint.write` is static, unlocked, and shares one `.pending` path across controllers (`ProjectCheckpoint.java:47, 85-125`); fingerprint and bytes are read under two different lock holds (H-02 = E-13) | PARTIAL ("atomic for a single writer"); HYP-E6/E7 open | E2 (reachability E5) |
| `.forge` codec ownership (`ARCHITECTURE.md` Ownership: `forgeshape_project_bytes` + `forgeshape_project_document`) | those two | `FRFM` and `SURF` sections live in `forgeshape_project_freeform.{h,cpp}` (169 lines) and `forgeshape_project_surface.{h,cpp}` (232 lines), which the Ownership table does not name (they appear only as section names at `ARCHITECTURE.md:5481, 5498`); `forgeshape_object_id.h` is likewise unnamed | DOC-DIVERGENCE (omission, no behavioural consequence) | E2 **(synth)** |
| Gesture arbitration (`ARCHITECTURE.md` Ownership: "the sketch arbitration block in `forgeshape_jni.cpp`") | JNI TU | `touchEvent` `forgeshape_jni.cpp:9503-10319` arbitrates sketch, chooser, gizmo, freeform, sculpt, camera and selection | matches the document; it is product policy inside the Android adapter TU (G-07, FUTURE-NOT-DUE for a second shell) | E2 |
| Sculpt history (`CLAUDE.md` `ARCH-OWNER-12`) | `SculptHistory` | one owner, but `record` clears `redo_` and returns `NotRetained` without clearing `undo_` (`forgeshape_sculpt_history.cpp:91-97`) | DEFECT-CONFIRMED P2 (D-01 = FC-R1 D4) | E1 (`diag_d3`) |
| Hidden is enforced in exactly ONE place (`CLAUDE.md` Stage 018A) | `ConstructionScene::snapshot` | renderer and picker consume `viewSceneSnapshot()`; no seat found a second predicate | matches at source; the "single predicate" positive claim was judged overstated by `work/VERIFICATION.md` (not upgraded to PASS-REPRODUCED) | E2 (seat B), E1 host SCENE 175/0 |

## 2. Coupling hot spots

### 2.1 File sizes (`wc -l`, this tree **(synth)**)

Native: 100 `.cpp` + 101 `.h`, 123,905 lines excluding `third_party`; of these
the self-test suites are 51,151 lines (`PORTABILITY_MATRIX.md` §2), so the
product native code is about 72,750 lines. Java: 85 files, 33,208 lines.

| File | Lines | FC-R1 (`a1b50f2`) | Role | Note |
| --- | ---: | ---: | --- | --- |
| `forgeshape_jni.cpp` | 10,337 | 8,087 | JNI surface, render thread, gesture arbitration, load/close orchestration, debug runners | +28 % since FC-R1 (`COUNCIL_DEBT_RECHECK.md` §3) |
| `EditorWorkspaceView.java` | 6,655 | 5,460 | shell phase, surfaces, Back, Save/Open/Recover flows, availability of every control | +22 %; 99 single-line field declarations at class indentation **(synth, regex count — approximate)** |
| `forgeshape_sketch_session.cpp` + `.h` | 4,328 + 1,197 | — | sketch state machine, candidate, overlay, view frames, extrude/revolve drags | owns `viewFrame()` that has no production caller (C-01) |
| `forgeshape_renderer.cpp` | 3,878 | — | Vulkan | one platform seam (§5) |
| `NativeViewport.java` | 3,505 | — | 275 `static native` declarations | one Java class mirrors the whole JNI surface |
| `forgeshape_project_document.cpp` | 3,041 | — | codec | |
| `forgeshape_sketch_arrangement.cpp` | 2,475 | — | planar arrangement | |
| `CadExtrudeCanvasView.java` | 1,311 | — | HUD chrome | one of three key-listener sites in RC-09 |

### 2.2 The JNI surface

| Measure | Value | Grade |
| --- | --- | --- |
| Unique `Java_com_forgeshape_*` symbols in `forgeshape_jni.cpp` | 275 (`BEFORE.md:36` says 276: 276 is the count of matching lines, 275 unique — `COUNCIL_DELIBERATION.md` D-9) | E2 **(synth)** |
| Callback direction | pull-only: no `FindClass`/`GetMethodID` callbacks (`PORTABILITY_MATRIX.md` §2) | E2 |
| Java files that call `NativeViewport.*` | 43 **(synth)** | E2 |
| `EditorWorkspaceView` → natives | 162 distinct natives, 475 call sites **(synth)** | E2 |
| `g_stateMutex` occurrences | 250 **(synth)**; one global lock for camera, selection, scene, history, all sessions | E2 |
| Exports nothing in product Java calls | 8: `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`, `supportChooserSelect`, `supportChooserConfirm`, `freeformSelect`, `freeformClearSelection`, `sketchToggleSelectEntity`; `sketchToggleRegion` is test-only (J-05, `COUNCIL_DEBT_RECHECK.md` §2.3) | E2 |
| `debug*` exports present in the release `.so` with `#ifdef NDEBUG` stub bodies | 13 (seat I `llvm-readelf`); 35 `NDEBUG` conditionals in the TU **(synth)** | E1 (seat I) / E2 |
| Mode guards restated per entry point | `inSculptMode()` 37 sites, `hasProject()` 30, sketch-active checks 17 **(synth)** | E2 — see §3 |

### 2.3 Process-scoped state

Function-local static singletons (`PORTABILITY_MATRIX.md` §2, re-grepped
**(synth)**): `constructionScene()` (`forgeshape_scene.cpp:406`), `meshStore()`
(`:604`), `sculptSession()` (`:638`), `constructionHistory()`
(`forgeshape_history.cpp:448`), `sketchSession()`
(`forgeshape_sketch_session.cpp:4323`), `supportChooser()`
(`forgeshape_support_chooser.cpp:323`), `gizmoSession()`
(`forgeshape_gizmo.cpp:1842`), `freeformEditSession()`
(`forgeshape_freeform_session.cpp:826`), `displaySettings()`
(`forgeshape_display.cpp:269`); plus one process-global density
`std::atomic<float> g_pixelsPerReferenceUnit` (`forgeshape_gizmo.cpp:15`).

JNI-file globals (`forgeshape_jni.cpp:176-401` **(synth)**): `g_viewport`,
`g_camera`, `g_selection`, `g_stateMutex`, renderer lifecycle atomics, the
sculpt gesture flags `g_grabbing` / `g_strokePending` / `g_pendingDown*`, the
saved sketch pose `g_sketchSavedPose` / `g_sketchPoseSaved`, chooser tap state
`g_chooser*`, `g_featureViewPending`, `g_lastPointers`.

Health reading:
- **Positive (E1).** Every self-test suite builds its own scene, history and
  camera; the 25 suites pass in reverse order with identical counts (seat I,
  `rev_order.cpp`). The session classes take scene/history by reference, so the
  singletons are a composition root, not a hidden dependency of the domain.
- **Negative (E1 + E2).** Ten-plus pieces of process state have no single reset
  owner; that is the mechanism of RC-07 (Freeform session survives a load, E1
  `diag_d2`; a refused load has already cancelled the sketch, E2 E-12). The
  allocator's forward-only high-water mark crosses loads (G-06 / J-03,
  FUTURE-NOT-DUE / BLOCKED-OWNER OD-J3).

### 2.4 God objects

| Object | Evidence of concentration | Defects whose mechanism sits there |
| --- | --- | --- |
| `forgeshape_jni.cpp` | 275 exports, render loop, gesture arbitration (~816 lines in `touchEvent`), load/close orchestration, debug runners and result caps (`:567, 821, 902, 923`) | RC-02 (import guards sculpt only, `:8586-8604`), RC-03 (`beginSketchView` uses `frame()`, `:313-319`), RC-07 (reset lists), RC-13 (caps, 1023-char log line `:608`) |
| `EditorWorkspaceView.java` | shell phase, every surface, Back routing, Save/Open/Recover, availability of controls, recreation carry | RC-04 (View-field memory), RC-06 (A-01, A-02), RC-08 (Back order `:1112-1117`, `:1177-1201`), RC-10 (availability predicates) |
| `SketchSession` | sketch + candidate + overlay + three view frames + two drag manipulators | RC-03 (view frame unused), C-03 (16-slot offer), H-05 (candidate cost on whichever thread asks) |

Inference (E5): four of the twenty-one root causes in
`COUNCIL_DELIBERATION.md` §4 (RC-02, RC-04, RC-07, RC-08) are "a rule restated
in N places and one place missed" inside these two files. No seat measured
churn or defect density per file; this is a reading of where the mechanisms
were found, not a metric.

## 3. Invariants — enforced in one place versus scattered

### 3.1 Enforced in one place (no seat found a bypass)

| Invariant | The one place | Evidence |
| --- | --- | --- |
| Hidden = not drawn and not picked | `ConstructionScene::snapshot` / `viewSceneSnapshot()` | E2 (seat B); host SCENE 175/0 (E1). Positive claim judged overstated at its grade by `work/VERIFICATION.md`: supported at E2, not PASS-REPRODUCED |
| Body delete / rename / hide / lock / duplicate / mirror are representation-neutral one-step acts | `forgeshape_body_delete.{h,cpp}`, `forgeshape_body_commands.{h,cpp}` | host SCENE, MIRROR (E1); `ObjectsDeleteTest`, `ObjectCommandsTest`, `MirrorSmokeTest` in FC2-D3 `40805804385` (E1, emulator) |
| Resize arithmetic | `forgeshape_body_dimensions.{h,cpp}` (pure function) | host BODY_DIMENSIONS (E1) |
| Euler ↔ matrix bridge | `forgeshape_transform.h` | host CONSTRUCTION_TRANSFORM, GIZMO (E1) |
| `.forge` layout, all-or-nothing load | codec + `loadProjectDocument` | 72/72 host corpus verdicts (E1), CI FAST corpus parity 72/72 (`evidence/fast-run-40796992679/forge-corpus-parity.txt`); 34,615-input fuzz with CRC recomputed: every input refused by name or loaded Ok (seat E, E1). Exceptions: E-05 (decode/validate accept, load refuses) and E-06 (two encodings of one face token), both P3 E1. Seat E's "fail-closed, no partial load" and "encoder canonical" positive claims were judged overstated by `work/VERIFICATION.md` (proven for the codec, not for every load entry point): supported at E2, not PASS-REPRODUCED |
| Whole-state CAD validation; preview = candidate | `validateCadBodyState`, `SketchSession::evaluateCandidate` | host CAD_FEATURE, CAD_R2 (E1) |
| Construction transaction boundary | `ConstructionHistory` / `ScopedConstructionEdit` | host CONSTRUCTION_HISTORY 147/0 (E1) |
| Manifold isolation | `forgeshape_cad_kernel.cpp` | E2 **(synth)** |
| Self-test code absent from release | CMake `NDEBUG` gating + release guard | CI FAST `40796992679` `RELEASE_SELFTEST_GUARD=PASS`, 0 symbols / 0 strings on both release ABIs (E1); local rerun `RELEASE_SELFTEST_GUARD=PASS` with `ANDROID_HOME=/root/android-sdk` (`TESTS.md` L5, `evidence/local/release_guard.txt`, E1; U-1 closed) |

### 3.2 Scattered — and where the scatter already produced a defect

| Invariant | Where it is restated | Missed instance | Grade |
| --- | --- | --- | --- |
| "While a sketch is being drawn … creation … is refused below JNI and withdrawn above it" (`CLAUDE.md` `CAD-R0-A1A2`) | one guard per JNI entry (sketch-active checks 17, `inSculptMode` 37 **(synth)**) plus one availability predicate per control | `importGlbDurable` asks only `inSculptMode()` (`forgeshape_jni.cpp:8586-8604`); domain `commitImportedGlbScene` guards only `editInProgress` (RC-02); Resume Sculpt has no native sketch guard (D-07) | E1 (A, D diags) + E2 |
| Face-supported bodies are not independently transformable; consumers use the derived placement | every placement consumer separately | export, transform write, gizmo (RC-01) | E1 + E2 |
| A project establishment resets every session | `loadProject`, `closeProject`, Java post-load path | Freeform session, support chooser on load, order of sketch cancel vs refusal (RC-07) | E1 + E2 |
| "A control that cannot succeed is not drawn" | Java availability predicates mirroring native refusals | Clear Mask over the per-entry cap (D-05), Symmetry/level over budget (D-08), Add/Cut on a full sketch table (C-03) (RC-10) | E2 (C-03 E2 after Round 2) |
| System Back is innermost-first (`ForgeShapeActivity.java:68-72`) | `hasDismissibleSurface` and `dismissTopmostSurface` in `EditorWorkspaceView` | bootstrap order (F-03), no revolve term (F-04) (RC-08) | E2 |
| Session memory survives recreation | `EditorUiState.carryAcrossRecreation`, an `EditorUiState` static, plus ad-hoc View fields | saved baseline in View fields (A-05); recovery-resolved flag set only on one path (E-02) (RC-04) | E2 |
| Numeric entry accepts the locale decimal separator | `ExactExpression` parser vs three `DigitsKeyListener("0123456789.-")` sites (`CadExtrudeCanvasView:644`, `SketchDimensionLabelView:112`, `BodyDimensionLabelsView:127`) | all three (RC-09) | E2 (parser E1) |
| Tap slop unit | 24 physical px in selection, sketch, chooser; 24 reference units × density in gizmo and Freeform | Freeform comment says it is "the sketch's own tap slop" (G-05) | E2 |
| Device self-test result capacity | four caps in `forgeshape_jni.cpp` (`:567, 821, 902, 923`) | PROJECT at 256/256, silent drop past the cap (I-01 = H-13) | E1 |
| Shader ↔ C++ constants | comments in `outline.frag` / `grid.vert` | none drifted at this tree; nothing compares them (H-17) | E2 |
| `sculptSession()` only under `g_stateMutex` (`ARCHITECTURE.md:4484-4491`) | every call site; checked by `artifacts/post-audit-harden-r1/tools/lock_context.js` | no violation reported; the script is not wired into any workflow (`grep -rn lock_context .github` → 0 **(synth)**) | E2 |
| `CADB` version additions | about seven files per version (FC-R1 I-6) | no defect recorded from it; v1..v8 + `FRFM`/`SURF` v1 | E2 / E4 |

## 4. Layering

**No include-level violation found.** The domain includes no renderer, JNI or
Android header (100/101 headers standalone, the renderer header being the
exception by design); the renderer calls none of the process-scoped accessors
(`grep` count 0 in `forgeshape_renderer.*`, `PORTABILITY_MATRIX.md` §3); only
`byte[]` and one `Surface` cross JNI. E1/E2.

Responsibility leaks that are not include violations:

| Leak | Where | Standing | Grade |
| --- | --- | --- | --- |
| Domain derivation on the frame loop under the global lock | candidate evaluation (`forgeshape_jni.cpp:1733`), camera-only overlay rebuild, per-frame face-frame resolution (`forgeshape_scene.cpp:281-326`) | risk (H-05, H-07, H-08); requirement BLOCKED-OWNER (no budget recorded) | E2 + E1 host timings |
| Durable session facts held by an Android View | `EditorWorkspaceView.java:271-272` | DEFECT-CONFIRMED (RC-04) | E2 |
| Product interaction policy inside the adapter TU | `touchEvent` arbitration | allowed by `ARCH-OWNER-01`; FUTURE-NOT-DUE for a second shell (G-07) | E2 |
| Pure-Java policy that a C++ shell would have to duplicate | 22 files, 3,854 lines (`ExactExpression`, `LengthUnit`, `CadHudPresentation`, `SketchChromePolicy`, `ProjectDrawerPolicy`, …) | FUTURE-NOT-DUE | E2 |
| Chrome restates domain refusals | availability predicates (RC-10) | by design (`CLAUDE.md`: "removing a control is not removing a guard"); the drift between the two copies is the defect class | E2 |

## 5. Platform seams

Owned by `PORTABILITY_MATRIX.md` (module × class × evidence, dependency graph,
responsibility map, leaking assumptions) and `DESKTOP_PORT_STRATEGIES.md` /
`DESKTOP_POC_GATE.md`; not repeated here. Summary only: one renderer seam
(`VK_USE_PLATFORM_ANDROID_KHR`, `ANativeWindow`, `android/log.h`; compiles with
four stubbed functions, `PORTABILITY_MATRIX.md` §5 P4), one adapter TU
(`forgeshape_jni.cpp`), Java storage adapters, and the numeric/lifecycle
assumptions listed there (px units, one document per process, FP contraction).
Desktop work is H2 (FUTURE-NOT-DUE); nothing in this file marks it missing.

## 6. Build health

| Check | Result | Grade / source |
| --- | --- | --- |
| Gradle debug + release + androidTest assemble | `BUILD SUCCESSFUL` (124 tasks) | E1 `evidence/local/gradle_build_tail.txt` |
| JVM unit tests | 20 classes / 196 tests / 0 failures (re-run: 22 tasks executed) | E1 `evidence/local/gradle_unit_rerun_tail.txt` |
| Gradle deprecations | "Deprecated Gradle features were used … incompatible with Gradle 9.0" (8.14.3). Not acted on: `CLAUDE.md` forbids broad AGP/Gradle upgrades as a side effect | E1 (observation) |
| Host native suites | `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 suites | E1 `evidence/local/host_selftests.txt` |
| Corpus | host verdicts 72/72 as expected (E1); CI FAST parity `committed=72 generated=72 mismatches=0` | E1 `evidence/fast-run-40796992679/forge-corpus-parity.txt` |
| Release self-test guard | CI FAST `40796992679`: PASS on both release ABIs (`evidence/fast-run-40796992679/release-selftest-guard.md`). Local: first run `guard_exit=2` (`ANDROID_HOME` not set, environment error, `evidence/local/run_status.txt`); rerun of `bash scripts/ci-release-selftest-guard.sh` with `ANDROID_HOME=/root/android-sdk` → `RELEASE_SELFTEST_GUARD=PASS` (release x86_64 3,256,192 B / arm64-v8a 2,916,104 B, 0 `SelfTests` symbols, 0 self-test strings) | E1 (CI and local; `TESTS.md` L5, `evidence/local/release_guard.txt`); U-1 closed (`COUNCIL_DELIBERATION.md` §2.2) |
| 16 KB page alignment | every `LOAD` segment 0x4000 on arm64-v8a and x86_64 release libs | E1 (seat G reproducing seat J, `readelf -lW`); runtime on a 16 KB-page device OWNER-PHYSICAL-UNVERIFIED |
| NDK pin | `ndkVersion '29.0.14206865'` (`app/build.gradle:9`) | E2 **(synth)** |
| Product C++ warning flags | product target gets `-std=c++17` only (`app/build.gradle:31`); Manifold `-O2 -w` (`CMakeLists.txt:179`); no `-Wall` in the product build; the host script compiles with `-Wall -Wextra` (`scripts/host-native-selftests.sh:56`) | E2 **(synth)** |
| Warnings under clang 18 `-Wall -Wextra -Wpedantic` | 98/98 TUs compile; 10 `-Wsometimes-uninitialized` in two self-test files (G-04, DEFECT-CONFIRMED P3) | E1 (seat G) |
| FP-contraction policy | none (`CMakeLists.txt`); NDK arm64 emits `fmadd` at -O0; two oracles fail under `-mfma` (G-01, DEFECT-CONFIRMED P2); x86_64 CI cannot see it | E1 host + E2 arm64 asm; device E5 |
| Shaders | 10/10 `glslc --target-env=vulkan1.0` + `spirv-val` | E1 (seat G) |
| CI coverage of the native layer | CI FAST runs no host suites and no corpus-verdict diff (I-05 = G-02); task-branch pushes trigger no workflow (`ci-fast.yml:12-17`, I-03) | E2 |
| Device evidence on this product tree | FC2-D1 `40797031704` 32/32, FC2-D2 `40800569636` 77/77, FC2-D3 `40805804385` 90/90 (each 25/25 tokens in order, 0 failure lines; emulator, SwiftShader); C2 `40776968165` 59/59 + `40782861799` 67/67 (E3). 28 of 70 classes have an E1 run, 11 a C2 run, 31 rest on older trees; no `FULL_SHARDED_SUITE_PASS` on this tree (last `36607747079` on `fa0b6b4`) | E1 (emulator only; 199/199 across 28 classes, `TESTS.md` §1; evidence `evidence/device-run-40797031704/`, `evidence/device-run-40800569636/`, `evidence/device-run-40805804385/`) / E3 — `FEATURE_INVENTORY.md` §0; head SHAs in `TESTS.md`. No physical-device gate is closed by these runs |
| Release APK executed | never, by any tier (I-10) | E2 |

## 7. Evidenced technical debt

Only items with a named mechanism and an evidence grade. Risk ratings live in
`RISK_REGISTER.md` (column "R-").

| Debt | Mechanism | Grade | Recorded already? | R- |
| --- | --- | --- | --- | --- |
| No single "establish/close project" reset | §3.2 row 3 | E1 + E2 | FC-R1 I-1; not adopted | R-10 |
| Placement consumers bypass `resolveWorldModel` | §1 face-supported row | E1 + E2 | new in FC-R2 | R-03, R-13 |
| Mode guards restated per entry point | §3.2 row 1 | E1 + E2 | FC-C1-D1 recorded import-during-sketch | R-14 |
| Session memory in View fields | §1 recreation row | E2 | FC-R1 D3 (narrower) | R-08 |
| Checkpoint writer coordination | static unlocked `ProjectCheckpoint.write`, two-lock fingerprint/bytes, delete-then-rename fallback | E2 (reach E5) | new in FC-R2 | R-04 |
| CAD derivation and overlay on UI/render thread under one lock | §4 row 1 | E2 + E1 host | FC-R1 I-4 | R-26 |
| Whole-mesh sculpt republish / full render-mesh rebuild per frame during a stroke | H-06 | E1 host + E3 (S25 104 ms) | `PROJECT_STATUS.md` "Sculpt cost model" | R-27 |
| Render loop never idles | `forgeshape_jni.cpp:1822-1826` | E2 | FC-R1 I-5 | R-28 |
| Per-frame face-frame re-derivation (F-07) | `forgeshape_scene.cpp:281-326`; 4.35 ms at depth 32 | E2 + E3 | `PROJECT_STATUS.md:4679` "held as debt" | R-29 |
| Orphan natives and dead helpers | 8 exports, `signedAreaTwice` (`forgeshape_sketch_region.cpp:50`), `pointStrictlyInside` (`forgeshape_sketch.cpp:235`) | E2 | FC-R1 (6 exports then) | R-50 |
| ABI-fragile oracles, uninitialised reads in self-tests | G-01, G-04 | E1 | new | R-33 |
| Device result caps, 1023-char log line, no count comparison | I-01, H-03, I-14 | E1 / E2 | new | R-32 |
| Two unit systems for tap slop | G-05 | E2 | new | R-22 |
| `SketchSession::viewFrame()` with no production caller | C-01 | E1 + E2 | FC-R1 D2 | R-15 |
| Documentation drift | §1 DOC-DIVERGENCE rows; `PROJECT_STATUS.md:382`, `:4245-4246` still list FC-R1 D5 open although closed (`5da5d85`) | E2 | partly | R-51 |
| Structure growth of the two largest files | §2.1 | E2 (sizes) + E5 (consequence) | FC-R1 I-3 | R-50 |

## 8. Commands run for the **(synth)** marks

All from `/home/user/Forgeshape`, read-only:

```
find app/src/main/cpp -maxdepth 1 \( -name '*.cpp' -o -name '*.h' \) | xargs wc -l | sort -rn | head -30
find app/src/main/java -name '*.java' | xargs wc -l | sort -rn | head -15
grep -o 'Java_com_forgeshape_[A-Za-z0-9_]*' app/src/main/cpp/forgeshape_jni.cpp | sort -u | wc -l     # 275
grep -c 'g_stateMutex' app/src/main/cpp/forgeshape_jni.cpp                                            # 250
grep -nE '^(static |std::|[A-Za-z_:<>]+ )g_[A-Za-z]+' app/src/main/cpp/forgeshape_jni.cpp
grep -nE '&\s*(constructionScene|constructionHistory|sketchSession|sculptSession|gizmoSession|supportChooser|freeformEditSession|meshStore|displaySettings)\(\)' app/src/main/cpp/*.cpp
grep -rn isFaceSupportedCad app/src/main/cpp --include=*.cpp --include=*.h | grep -v selftest
grep -rln '#include.*manifold' app/src/main/cpp | grep -v third_party
grep -c 'hasProject()' / 'inSculptMode()' / 'sketchSession().active()\|sketchSession().isActive()' app/src/main/cpp/forgeshape_jni.cpp   # 30 / 37 / 17
grep -o 'NativeViewport\.[a-zA-Z]*' app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java | sort -u | wc -l   # 162
grep -c 'NativeViewport\.' app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java                # 475
grep -rl 'NativeViewport\.' app/src/main/java | wc -l                                                   # 43
sed -n 170,190p app/src/main/cpp/forgeshape_gltf_export.cpp
sed -n 1725,1740p app/src/main/cpp/forgeshape_jni.cpp
sed -n 4467,4535p ARCHITECTURE.md
grep -rn 'lock_context\|host-native\|corpus-verdicts' .github/workflows/*.yml
grep -nE 'ndkVersion|cppFlags' app/build.gradle; grep -nE 'compile_options' app/src/main/cpp/CMakeLists.txt
for f in app/src/main/cpp/*.h: grep -c <basename> ARCHITECTURE.md   # unnamed non-test modules: forgeshape_object_id, forgeshape_project_freeform, forgeshape_project_surface
```
