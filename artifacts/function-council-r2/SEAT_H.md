# FUNCTION-COUNCIL-R2 — Seat H: Renderer, picking, performance, memory, concurrency

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` identical to `f3db953`;
the audit branch head `c84d4fb` adds only `artifacts/`). Research only. This seat modified
nothing under `app/`, `testdata/`, `scripts/`, `.github/`, the Gradle files or the owner
documents, and ran no Gradle. It wrote this file, `work/req_H.tsv`
(66 rows, `RND-001`..`RND-066`) and, in Round 2, `work/delib_A_to_H.md`. Finding IDs
`H-09` and `H-12` are unassigned: they were dropped while drafting, no surviving record
holds their content, and this seat asserts no finding under either ID. Scratch diagnostics are in
`/tmp/claude-0/fc2-scratch/H/` (`COMMANDS.txt`, `*.cpp`, `out_*.txt`), linked against the
objects of this audit's fresh host build and never added to the build.

## Scope

- Vulkan ownership (`forgeshape_renderer.{h,cpp}`) and the frame loop.
- Swapchain rebuild policy: OUT_OF_DATE vs SUBOPTIMAL, explicit resize.
- Device-loss policy (`forgeshape_render_recovery.{h,cpp}`).
- The selection outline mask pass and composite.
- Draw order and depth rules: grid, gizmo, sketch overlay.
- Scene snapshot, mesh revisions and the upload gate, staging, buffer growth.
- Shader constant parity, cold start, and Android surface integration
  (`surfaceCreated` / `surfaceChanged` / `surfaceDestroyed`).
- CPU picking after C2, mapping a triangle to a semantic face, and picking cost.
- The JNI locking model and synchronous native work on the UI thread.
- The autosave worker, and how the render thread and the UI thread share data.
- Memory caps.

Product UX for Display/Selection belongs to other seats. This seat read it only where a render or pick contract depends on it.

## Sources read

| Path | Revision / section |
| --- | --- |
| `artifacts/function-council-r2/BEFORE.md` | audit preflight |
| `CLAUDE.md` | Hard rules: GPU resources, SEL-OUT-R1, CAD-VERTICAL-SLICE-R1 preview, CAD-UX-S1 overlay revision, Android/Vulkan safety, autosave, Stage 018A hidden, Stage027 isolate |
| `PROJECT_STATUS.md` @70a6c14 | Durable constraints §2433 (ARM64 tolerance, Gate P1 S25 table); Owner Decision Baseline §2486; Known Issues §4018; Technical Debt §4216 (F-07, FC-R1 D2–D7, PF-S1 startup benchmark) |
| `artifacts/function-council-r1/COUNCIL_FINDINGS.md` | D5, I-4, I-5 (re-checked, not trusted) |
| `artifacts/cad-r2-c2-picking-seam/{PERF,TESTS}.md` | C2 picking change and ABBA perf |
| `artifacts/cad-r2-c1-quality-gate/PERF_ANALYSIS.md` | x86 code-placement (JCC erratum) attribution |
| Fresh run outputs of this audit | `evidence/local/{host_selftests,release_guard,run_status}.txt` and the scratchpad `fc2/host_selftests.txt` (`HOST_SELFTESTS_OK (4392 checks, 0 failed)`), `fc2/release_guard.txt` (`RELEASE_SELFTEST_GUARD=PASS`; this file was written at 00:57, after `fc2/run_status.txt` recorded `guard_exit=2` for the first scripted run at 00:54:48, so it is a later local rerun whose exit is not recorded in `run_status`; the independent fresh source is CI FAST `40796992679` `release-selftest-guard.md`, `RELEASE_SELFTEST_GUARD=PASS`), CI DEVICE `40797031704` artifact (`fc2/ci/dev1`: `summary.json` PASS 25/25 tokens, 32/32 tests; `startup-forgeshape.txt`; `vulkan-evidence.txt`) |

## Inspected code paths

| File:lines | Function / fact |
| --- | --- |
| `forgeshape_renderer.cpp:3751-3876` | `drawFrame`. Rebuild gate at :3756. Sync order: scene, overlay, gizmo. The acquire classification is at :3796-3810 and the present classification at :3851-3860. |
| `forgeshape_renderer.cpp:2098-2250` | `createSwapchain`: FIFO, `minImageCount+1`, and `expectSuboptimal_ = preTransform != currentTransform` at :2225. |
| `forgeshape_renderer.cpp:357-430` | `rebuildDeviceAfterLoss` and `handleFrameResult` (the policy-driven actions). |
| `forgeshape_render_recovery.cpp:38-117` | `classifyRenderResult` and `RenderRecoveryPolicy`. The bound is 2 rebuilds; the terminal state is sticky. |
| `forgeshape_renderer.cpp:433-506` | `attachSurface` / `detachSurface`, and window ownership. |
| `forgeshape_renderer.cpp:682-1170` | Upload objects, `waitForMeshBuffersIdle` (UINT64_MAX by default), `ensureStagingCapacity`, `ensureMeshCapacity`, `uploadRenderMesh`, `releaseBodiesAbsentFromScene` (bounded at 100 ms) and the `syncBody` revision+shading gate at :1119. |
| `forgeshape_renderer.cpp:1586-1735` | `syncSketchOverlay`: revision+count gate and the 65536 cap. `recordSketchOverlayDraw`. |
| `forgeshape_renderer.cpp:3320-3412` | `recordCommandBuffer` order: optional mask pass, bodies, grid, outline composite, gizmo, overlay. |
| `forgeshape_renderer.cpp:3614-3738`, `:2799-2995`, `:3032-3236` | Outline: active test, mask pass, composite, render-pass external dependencies, and mask/composite pipelines. Main and mask both cull BACK with CCW and depth LESS (:2463-2475, :3086-3103). |
| `forgeshape_selection_outline.h:56-66, 128-130`; `shaders/outline.frag` | Width policy, and tap constants mirrored by hand. |
| `forgeshape_renderer.cpp:167`; `shaders/grid.vert:43` | Grid depth nudge, mirrored by hand. |
| `forgeshape_jni.cpp:150-230` | `ViewportThread`, lifecycle/frame/outline atomics, and `g_stateMutex`. |
| `forgeshape_jni.cpp:1695-1784` | `applyCadOperationPreviewLocked`: preview key `1<<61`, revision base `1<<62`. Calls `evaluateCandidate()` on the render thread. |
| `forgeshape_jni.cpp:1786-2069` | `renderThreadMain`. The wait predicate includes `running`, so the loop is continuous. The snapshot, gizmo and overlay are built under `g_stateMutex`, and display settings are read outside it. The `!drew` path is at :2049-2066. |
| `forgeshape_jni.cpp:2081-2127` | `NativeViewport.start`. Every self-test runs while `g_viewport.mutex` is held. |
| `forgeshape_jni.cpp:9404-9488` | `surfaceCreated` and `surfaceChanged` (viewport mutex, then state mutex). `surfaceDestroyed` waits for the detach ack with `wait_for(5 s)`. |
| `forgeshape_jni.cpp:4400-4420` | `sketchFinish`, which runs under `g_stateMutex` on the UI thread. |
| `forgeshape_jni.cpp:8387-8420, 8936-8947` | `encodeProject` and `projectFingerprint`: two separate lock acquisitions. |
| `forgeshape_jni.cpp:10036-10040` | Sculpt stroke move → `publishSculptRepresentation` (whole mesh). |
| `forgeshape_picking.cpp:46-292` | `buildPickRay`, `transformRayToLocal` (direction not renormalised), `intersectRayTriangle` (C2 double precision, absolute det threshold `kRayTriangleEpsilon` at :225-231), `pickTriangleMesh` (linear scan). |
| `forgeshape_selection.cpp:14-97` | `pickScene` reads `viewSceneSnapshot()`, the same list the renderer draws. Also `pickSceneSnapshot` and `pickMesh`. |
| `forgeshape_support_chooser.cpp:150-245` | Plane vs scene hit. A nearer surface occludes a plane. Triangle → `CadFaceRange` comes from the body's cached regeneration. |
| `forgeshape_cad_body.cpp:1559-1589` | `generateMesh` copies `regenerated()->mesh`, so the published index order equals the face-tag order. |
| `forgeshape_scene.cpp:277-372, 629-636` | `resolveWorldModelDepth` re-derives the face frame on every snapshot (F-07). `snapshot` enforces hidden and isolate in one place, and leaves out an unresolvable face-supported body. |
| `forgeshape_sketch_session.cpp:2987-3029, 3752-3779` | `evaluateCandidate` (latest-only cache) and `overlay()`, where a camera change bumps `overlayRevision_`. |
| `AutosaveController.java:145-200` | `performCheckpoint` calls `projectFingerprint()`, then `encodeProject()`. |
| `ForgeShapeActivity.java:326-382`; `EditorWorkspaceView.java:6147-6155` | `checkRendererLifecycle` is called from `onResume` only. |

## Test coverage of the domain (what is actually asserted)

### Host native (fresh run, this audit)

`HOST_SELFTESTS_OK (4392 checks, 0 failed)` at `70a6c14`, Grade E1.

| Suite | Checks | What it asserts for this domain |
| --- | --- | --- |
| RENDER_RECOVERY | 24 | The pure policy (`FSR1B_16_*`): SUBOPTIMAL expected vs unexpected; OUT_OF_DATE rebuilds even when SUBOPTIMAL is expected; DEVICE_LOST is rebuilt twice and the third loss stops; a failed rebuild stops at once; a terminal state never revives. It does not drive Vulkan. |
| RENDER_SHADING | 412 | The render-mesh derivation, the outline CPU reference edge rule (`seloutr1_02_*`, `seloutr1_20_*`), outline colours per ground (`seloutr1_28/31_*`), the width clamp and the pulse. |
| PICKING | 185 | Ray build in both projections, local-ray distance under scale, culling, and `PICK_C2_01..11` (far orthographic seam, 39 diagonal points, origins 50–500 m, NaN/degenerate fail-closed). |
| CAD_FEATURE | 638 | Includes `PICK_C2_12..17`: a scene pick on the C1 pocket floor, the chooser naming `(feature 3, CapFar, lineage)` and never the plane behind, and the fingerprint unchanged. Also `MF_PERF_01/02` (128 and 3969 faces select and extrude). |
| SKETCH_UX | 118 | Includes `CADFC1_S4_a_a_zoom_rebuild_advances_the_overlay_revision` and an overlay bound check. |
| DYNAMIC_MESH | 91 | Revision/publish rules and capacity growth. |
| SCENE | 175 | The snapshot: hidden and isolate. |
| GLTF_IMPORT | 189 | Preview render keys are reserved. |

### Instrumented (androidTest; historic unless named)

| Class | Methods | What they assert |
| --- | --- | --- |
| `DiagnosticsAndRendererLossTest` | `e2er1b07_aLostDeviceCostsTheProjectNothing` | `nativeSnapshot` and `encodeProject` are bit-identical across an injected loss, and the renderer is either rebuilt or RestartRequired. The test itself calls `workspace.onRendererRestartRequired()` (:226-229), so the product's own noticing is not tested. |
| `DiagnosticsAndRendererLossTest` | `e2er1b07_theProjectStaysEditableAfterTheDeviceComesBack` | The project can still be edited after the device comes back. |
| `SelectionOutlineTest` | 19 methods `seloutr1_01..36`, `e2eSeloutr1_*` | Representation-neutral outline. A switch allocates nothing (`seloutr1_08`). The toggle stops both passes. Project truth is untouched. Device rebuild (`seloutr1_26`). Extent-only allocation (`seloutr1_27`). |
| `RendererCounterContinuityTest` | `outlineCountersNeverRunBackwardsAcrossARenderThreadRestart` | Outline counters never run backwards across a render-thread restart. |
| `SelectionOutlineVisualEvidenceTest` | — | Re-run in this audit's CI DEVICE `40797031704`, 32/32 tests: Grade E1 on the emulator. |
| `CadFillPickR2Test` | `devR2_01..06` | Jittered taps, tap order, arrow head vs shaft, and attribution tokens. |
| C2 device cases D19–D22 | — | Historic CI DEVICE runs `40776968165` and `40782861799`: Grade E3. |

### JVM and fixtures

- JVM: `DisplaySettingsContractTest` and `PointerSemanticsTest` are the only JVM classes in this domain. Renderer behaviour has no JVM test.
- Fixtures: none are render-specific. `.forge` truth is not this seat's.

### Not tested at all

- Stroke frame rate at density.
- UI-thread stall at the arrangement cap on a phone.
- The fingerprint/encode interleaving.
- The mid-session RestartRequired notice.
- Shader/C++ constant parity.
- Overlay revision collision between producers.

## Confirmed findings

### H-01 — P2 — A renderer that stops mid-session is not reported until the next resume

- **Where.**
  - `ForgeShapeActivity.java:326-338`: `onResume` → `checkRendererLifecycle`, the only production caller.
  - `ForgeShapeActivity.java:371-382` and `EditorWorkspaceView.java:6147-6155`.
  - The render loop's `!drew` path, `forgeshape_jni.cpp:2049-2066`, only stores an atomic. Its own comment (`:2049-2055`) says the intent is to "let the Android layer checkpoint the project and tell the user".
  - `checkRendererLifecycle`'s javadoc (`ForgeShapeActivity.java:363-369`) says it is "polled at the two moments it can matter — a resume, and a debug injection's follow-up". The second caller does not exist in `app/src/main` (grep: the only production call is `:336`); the only other caller of `onRendererRestartRequired()` is the test itself. The javadoc is stale.
- **Minimal repro (source).** While the app stays in the foreground, the render thread returns `!drew` (an Unrecoverable result or a third DEVICE_LOST). It detaches the surface and sets `g_rendererLifecycle = 2`. No Java code reads that atomic until `onResume`. No status message is shown and no immediate checkpoint is requested. The only tested path calls `onRendererRestartRequired()` by hand (`DiagnosticsAndRendererLossTest.java:226-229`).
- **Impact.** The user looks at a dead viewport with no explanation until they leave and return. This is the black viewport CLAUDE.md says the policy exists to avoid. Normal autosave still protects edits, so no data is lost.
- **Evidence.** E2.
- **Verdict.** DEFECT-CONFIRMED (E2) (Round 2, conceded to seat A O-1: the code contradicts the H0 CLAUDE.md GPU rule and its own comments). Severity stays P2, because autosave still protects edits.
- **Isolated fix.** Poll `NativeViewport.rendererLifecycle()` in the existing refresh path (`syncFromNative` / the world-anchored refresh), or on a low-rate `Choreographer` tick, and keep the "reported once" latch.

### H-02 — P3 — The autosave fingerprint and the checkpoint bytes are read under two different locks

- **Where.** `AutosaveController.java:157` (`projectFingerprint()`) and `:173` (`encodeProject()`). On the native side, `forgeshape_jni.cpp:8936-8947` and `:8387-8410` each take `g_stateMutex` separately.
- **Minimal repro (interleaving, source-proven).**
  1. The worker reads fingerprint F1.
  2. The UI thread applies an edit (F2) and schedules a checkpoint with a 900 ms debounce.
  3. The worker encodes F2's bytes, writes them, and records `checkpointedFingerprint = F1`.
  4. The user presses Undo within the debounce, so the live project is F1 again.
  5. The coalesced checkpoint task sees F1 == recorded F1 and skips.

  The recovery checkpoint now holds the edit the user undid.
- **Impact.** After a crash, Recover could offer a state that differs from what the user last had: an undone edit reappears. The window is narrow: the edit must land during an encode, then be undone within about 0.9 s. Nothing is silently destroyed, because the user is asked before any recovery.
- **Evidence.** E2. Not runtime-reproduced.
- **Verdict.** DEFECT-CONFIRMED (source).
- **Isolated fix.** One JNI call returns `{fingerprint, bytes}` from inside one `g_stateMutex` scope.

### H-03 — P3 — The on-device `FORGESHAPE_CAD_FEATURE_PERFORMANCE` line is truncated at 1023 characters

- **Where.** `forgeshape_jni.cpp:608` (`FS_LOGI("... %s", cadFeaturePerformanceReport())`). `FS_LOGI` is `__android_log_print`, which uses a 1024-byte buffer.
- **Repro (E1, this audit, reproduced twice).** In the startup log of CI DEVICE `40797031704` and of CI DEVICE `40800569636` (`fc2/ci/dev{1,2}/startup-forgeshape.txt`), the line (from the token on) is exactly 1023 characters; it ends in `parametric_ch` in the first run and in `par` in the second (the values differ in length). The host line is 1243 characters. These keys therefore never reach the device log:
  - `parametric_chain16_regen_us`, `parametric_upstream_edit_us`, `parametric_chain16_triangles`
  - `cad_r2_keep_us`, `cad_r2_reuse_us`, `cad_r2_delete_us`
  - `cad_r2_grid36_arrangement_us`, `cad_r2_inventory16_us`
  - `CAD_R2_ORACLE_PASS`

  The host line carries all of them.
- **Impact (narrowed in Round 2, seat A O-7).**
  - Every key CLAUDE.md names for this suite survives on the device: region, Add, Add + Cut, codec round trip, `arrangement_cap_us`.
  - Only the `parametric_*`, `cad_r2_*` and `CAD_R2_ORACLE_PASS` keys are lost. The oracle checks themselves still run and count in the suite total.
  - No repo dossier cites device values for the lost keys (grep over `artifacts/`; the CAD-R2 perf files are host same-machine runs), so nothing published is wrong. Device evidence for those keys simply cannot exist until this is fixed.
- **Evidence.** E1.
- **Verdict.** DEFECT-CONFIRMED.
- **Fix.** Emit the report as several lines with a shared prefix, each under 1000 characters.

### H-04 — P3 — `PROJECT_STATUS.md` still lists FC-R1 D5 (stale sketch overlay after a pinch zoom) as open

- **Where.**
  - `PROJECT_STATUS.md` Technical Debt (§4216, "D5 (low–medium) … `overlayRevision_` is not bumped").
  - The fix is in code at `forgeshape_sketch_session.cpp:3777-3779` (commit `5da5d85`, 2026-09-30).
- **Evidence.**
  - E2 for the code.
  - E1 for the guarding check: `CADFC1_S4_a_a_zoom_rebuild_advances_the_overlay_revision` passes in this audit's host run (SKETCH_UX 118/0).
- **Verdict.** DOC-DIVERGENCE.
- **Fix.** Mark D5 closed in `PROJECT_STATUS.md` and cite the check.

### H-05 — P2 (risk) — Heavy CAD derivation runs synchronously under `g_stateMutex` on the UI thread or the render thread

- **Where.**
  - `sketchFinish` (`forgeshape_jni.cpp:4400-4420`, UI thread).
  - Entity placement through `touchEvent`.
  - `evaluateCandidate()`, whichever thread asks first: `applyCadOperationPreviewLocked` (`forgeshape_jni.cpp:1733`) runs on the render thread, and `cadExtrudeToolState` (`:5098`) runs on the UI thread.
- **Measured (E1).**

  | Measurement | Where | Value |
  | --- | --- | --- |
  | `SketchSession::finish()` alone, 63×63 grid / 3969 faces | Host x86 `-O1` (`out_finish.txt`) | 275–322 ms over 2 runs |
  | One entity placement near the cap | Host | up to 19.4–20.7 ms |
  | `evaluateCandidate` with every face selected | Host | 25.5–25.7 ms |
  | Camera-only overlay rebuild at that state | Host | 4.2–4.5 ms per rebuild (taken every orbit/zoom frame, on the render thread, under the lock) |
  | `finish_us` = `Driver::finishOn(gridSketch(63, 63))`: 125 entity placements **plus** Finish, not one UI block (`cad_multiface_selftest.cpp:76-85, 818-821`) | CI DEVICE `40797031704` debug SwiftShader emulator | 4,227,606 µs (7,130,056 µs in `40800569636`) |
  | `eval_all_us` | Same emulator run | 91,586 µs |

- **Impact.**
  - The render thread waits on the same mutex, so the viewport freezes for the duration.
  - Release-build timings on an ARM phone are unmeasured. An at-cap Finish of several hundred ms to seconds on the UI thread is a jank/ANR risk (OWNER-PHYSICAL-UNVERIFIED).
- **Verdict.** The measurements stand as a P2 risk. The requirement row (RND-037) is **BLOCKED-OWNER**: no repo record states a UI-thread or render-thread budget; its source was the FC-R1 I-4 recommendation (Round 2, conceded to seat A O-2).
- **Fix direction.**
  - A bounded off-thread derivation with a "deriving" state, or
  - at least a measured release-ARM budget and a refusal past it.

### H-06 — P2 (risk) — Per stroke sample a whole-mesh publish; per frame during a stroke a full render-mesh rebuild, upload and GPU drain

- **Where.**
  - Per sample: `forgeshape_jni.cpp:10036-10040` → `publishSculptRepresentation` (`:1170-1180`) → `publishSculptMesh` (`forgeshape_sculpt.cpp:611`). This publishes into the MeshStore only; it does not call the renderer.
  - Per rendered frame, only when the revision moved: `syncBody` (`renderer.cpp:1119-1122`) runs `RenderMeshCache::refresh` (all normals), then `uploadRenderMesh` (`:868`), which waits on every in-flight fence (`:749`) and on its own upload fence. Several samples between two frames therefore cost one rebuild, not several (Round 2 wording, conceded to seat A O-3).
- **Measured (E1, host x86 `-O1`, `out_mesh.txt`).**

  | Vertices | Triangles | publish | smooth refresh | faceted refresh |
  | --- | --- | --- | --- | --- |
  | 99,846 | 196,608 | 2.1–2.2 ms | 20.0–20.3 ms | 12.3–13.4 ms |
  | 131,424 | — | 2.8–3.1 ms | 25.6–29.9 ms | — |

  The recorded S25 Ultra Gate P1 render build at about 100k vertices is 104 ms (E3).
- **Impact.** The frame time during a stroke on a 100k mesh on that phone is plausibly 100 ms or more per frame (E5). It has never been measured. The recorded debt "whole-mesh sculpt republish" is still open.
- **Verdict.** P2 risk. The requirement row (RND-019) is **BLOCKED-OWNER**: PROJECT_STATUS "Sculpt cost model" (`:4534`) records the cost model and a baseline, no budget (Round 2, seat A O-3).
- **Fix direction.** An incremental normal refresh over the stroke's affected set, plus a dirty-range upload.

### H-07 — P2 (risk) — The render loop never idles while a surface is attached

- **Where.** `forgeshape_jni.cpp:1822-1826`: the wait predicate is true whenever `running`.
- **Behaviour.** Every vsync the loop retakes `g_stateMutex`, rebuilds `viewSceneSnapshot()` (one `resolveWorldModel` per body) and asks every overlay producer. It also contends with UI-thread JNI.
- **Status.** This was FC-R1 I-5, and it is still true at this tree (E2).
- **Impact.** Battery and thermal cost are OWNER-PHYSICAL-UNVERIFIED.
- **Verdict.** **BLOCKED-OWNER** (H2 until decided; Round 2, seat A O-4). No owner record requires an idle loop. An owner decision is needed, because the evidence captures depend on presented-frame counts.

### H-08 — P2 (debt, still open) — F-07: face-supported bodies re-derive their face frame on every frame

- **Where.** `forgeshape_scene.cpp:281-326` calls `cadFeatureTopologySignature` and `resolveCadFeatureFace` per face-supported body per snapshot, under the lock.
- **Recorded.** 4.35 ms at chain depth 32 (DEEP-AUDIT-R1, E3).
- **Code path.** Unchanged (E2).
- **Verdict.** Recorded debt, H2: PROJECT_STATUS `:4679` holds F-07 "as debt (reported not implemented)". RND-039 is FUTURE-NOT-DUE, still present in code (E2+E3) (Round 2, seat A O-5).
- **Fix.** Cache the resolved frame keyed by the producer's `CadBodyState`.

### H-10 — P3 (risk) — Triangles with twice-area at or below 1e-8 m² cannot be picked

- **Where.** `forgeshape_picking.cpp:225-231`: an absolute `kRayTriangleEpsilon = 1e-8` on `det = e1·(d×e2)`.
- **Repro (E1, `out_pick.txt`).** At normal and at 60° incidence, a right triangle with legs of 1.5e-4 m hits at its centroid and one with legs of 1.0e-4 m misses.
- **Impact.**
  - The orthographic zoom floor (`kMinOrthoHalfHeightMeters = 0.02`) gives about 0.017 mm per pixel at 2400 px. A sub-0.1 mm face can therefore cover several pixels and still refuse taps.
  - Under body scale, the local direction length also scales `det`.
  - This matters for mm-scale models only. No current fixture hits it.
- **Verdict.** The E1 measurement stands as an observation. The requirement row (RND-046, "any triangle visible at the closest zoom is pickable") was this seat's own and is recorded nowhere: **BLOCKED-OWNER** (owner decision 4) (Round 2, seat A O-9).
- **Fix.** Make the threshold relative, for example `ε·|e1||e2||d|`.

### H-11 — P3 — `surfaceDestroyed` waits at most 5 s; CLAUDE.md says it "must block until native code has released the ANativeWindow"

- **Where.** `forgeshape_jni.cpp:9487` (`toCaller.wait_for(lock, 5 s, …)`), which logs `acked=0` on timeout.
- **Why it is safe today.** The renderer holds its own `ANativeWindow` reference, so the object outlives the Surface.
- **Divergence.** The rule text and the code differ.
- **Evidence.** E2 for the bound; E2+E5 for "safe today" (a reasoning chain: the renderer's own window reference, plus `SurfaceLost → Continue` at `render_recovery.cpp:75-79`) (Round 2, seat A O-10).
- **Verdict.** DOC-DIVERGENCE.
- **Fix.** State the bound and the refcount argument in CLAUDE.md, or wait without a bound.

### H-13 — P3 — The PROJECT suite is exactly at its device result cap (256/256)

- **Where.** `forgeshape_jni.cpp:821` (`kMaxProjectChecks = 256`) and `Recorder::check` (`forgeshape_project_selftest.cpp:21-32`), which silently drops records past the cap.
- **Repro (E1).**
  - The host harness (8192 cap) counts 256.
  - The device reports `FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)` (CI DEVICE `40797031704`).
  - The next PROJECT check would run on the host and be silently skipped on the device. This is the failure mode `jni:593-597` already records for CAD_FEATURE.
  - CAMERA is at 119/128.
  - At this tree the suite has exactly 256 checks on both sides, so **no check is dropped today**; the hazard is the zero headroom plus the silent drop.
- **Verdict.** PARTIAL (latent test-infrastructure risk, E1) (Round 2, conceded to seat A O-8; was DEFECT-CONFIRMED).
- **Fix.**
  - Raise the caps.
  - Treat `count == cap` as a suite failure.

### H-14 — P3 — Cold start rebuilds an equal-extent swapchain once

- **Where.** `surfaceChanged` always sets `resizeRequested` (`forgeshape_jni.cpp:9439-9444`), which calls `requestResize()`, which sets an unconditional rebuild flag.
- **Repro (E1, emulator).** CI DEVICE `40797031704` logs:
  - `Swapchain created … 1080x2400` (58.493)
  - `created` again (58.753)
  - `Swapchain rebuilt` (58.991)
  - first present (59.014)

  That is about 0.5 s of first-frame latency on SwiftShader.
- **Verdict.** PASS-REPRODUCED, with an improvement available. The requirement row RND-065 ("first frame presents promptly") was this seat's own: **BLOCKED-OWNER**; the E1 timings stay as observations (Round 2, seat A O-9).
- **Fix.** Skip the rebuild when the requested extent equals `swapchainExtent_` and nothing else is pending.

### H-15 — INFO — The debug cold start blocks the UI thread on the self-tests while holding `g_viewport.mutex`

- **Where.** `forgeshape_jni.cpp:2083-2120`.
- **Measured (E1, CI DEVICE `40797031704`).**
  - Camera OK at 49.537, Surface OK at 57.941: about 8.4 s in total.
  - CAD_FEATURE alone takes 7.5 s.
- **Scope.** Release excludes every suite (`RELEASE_SELFTEST_GUARD=PASS`, CI FAST `40796992679`, E1).
- **Status.** Already recorded as debt ("PF-S1 at-cap benchmark still runs on every debug start").
- **Verdict.** PASS-REPRODUCED. Debug only.

### H-16 — P3 — Stale comments about rigid transforms

- **Where.** `forgeshape_selection.cpp:42-47` and `:90-94` say "every Construction transform is rigid". The distances are correct anyway, because `transformRayToLocal` does not renormalise the local direction (`forgeshape_picking.cpp:154-183`).
- **Evidence.** E2.
- **Verdict.** DOC-DIVERGENCE (code comment).

### H-17 — P3 — Shader/C++ constant parity is enforced by comments only

- **Where.**
  - `outline.frag` mirrors `forgeshape_selection_outline.h:128-130` (8 / 4 / 0.5).
  - `grid.vert:43` mirrors `renderer.cpp:167` (1e-4).
- **Status.**
  - Both pairs are equal at this tree (E2).
  - No script or test compares them.
- **Verdict.** IMPLEMENTED-UNTESTED.
- **Fix.** A grep check in CI FAST.

## Hypotheses (unproven)

### HYP-H1 — Overlay revision collision

- **What.**
  - The sketch session, the support chooser and the Dimensions session each count overlay revisions from 0 or 1. Only Freeform has its own base (`0xF0F0…`).
  - The renderer skips an upload when the revision and the vertex count both equal the last upload (`renderer.cpp:1601-1604`).
  - A direct switch between two producers with equal revision and equal count would therefore keep drawing the old producer's vertices.
- **Plausible path.** Chooser select straight into a new sketch, with no empty overlay frame between them, because an empty frame resets the gate.
- **Status.** Not reproduced. Equal counts are unlikely.
- **Fix.** Disjoint high-bit bases, or a pointer-identity compare.

### HYP-H2 — RestartRequired is not enforced on a later attach (split in Round 2, seat A O-16)

- **E2 fact (no longer a hypothesis).** The attach path never consults the recovery state: `forgeshape_jni.cpp:1852-1856` sets `running = true` on any successful `attachSurface`, and `Renderer::attachSurface` (`renderer.cpp:433-486`) does not read `recovery_`. `drawFrame` and `handleFrameResult` consult the policy only on a failure result (`renderer.cpp:394-430`). `g_rendererLifecycle` is re-stored from the sticky policy state every frame (`jni:2018`). `rendererFailed` is written (`jni:168, 1790, 1860, 2063`, and reset at `:2089`) and never read.
- **Still a hypothesis.** That the viewport visibly returns while the notice says a restart is needed. It needs a stop with a still-usable device (an Unrecoverable result), which requires a real driver fault. For a stop after the third device loss the next failing frame would stop again.

### HYP-H3 — Mask depth-attachment hazard

- **What.** The outline mask's depth attachment is shared across the 2 frames in flight. The external subpass dependency (`renderer.cpp:~2929-2943`) covers the colour attachment's write-after-read.
- **Not verified.** That it covers the depth attachment's write-after-write at early fragment tests. No validation-layer session is recorded after SEL-OUT-R1. The last recorded session is Gate P1, before the outline existed.

### HYP-H4 — Picking cost on large imported meshes

- **What.** A large Imported Mesh (up to `kMaxImportedMeshVertices` = 4M) would cost a linear scan of up to about 8M triangles per tap. The host rate is about 38 ns per triangle, so roughly 0.3 s per tap (E5).

## Adversarial counterexample attempts

1. **Exhaust the sketch overlay cap with a full selection at the arrangement bound.**
   - Expectation if broken: `buildOverlay` clears everything above 65536 and the whole sketch vanishes.
   - Outcome: **not broken**. With all 3969 faces selected the overlay has 1468 vertices; the union hatch is merged (`out_overlay.txt`, E1).
2. **A preview revision colliding with a body store revision**, which would make the renderer skip uploading the preview or the restored mesh.
   - Outcome: **not broken**. The preview revisions start at `1<<62` and the key at `1<<61` (`jni:1711-1712`). `candidateRevision_` is monotonic across sessions (`cancel()` increments it, `sketch_session.cpp:269`) (E2).
3. **A lock-order deadlock between `surfaceChanged`/`start` (viewport then state) and the render thread.**
   - Outcome: **not broken**. The render thread takes `g_stateMutex` and `g_viewport.mutex` only in disjoint scopes (`jni:1820-1866`, `2003-2008`, `2059-2064`) (E2).
4. **The smallest pickable triangle.**
   - Outcome: **broken**. The threshold is at legs of about 0.1 mm (H-10, E1).
5. **Device-side completeness of the perf evidence.**
   - Outcome: **broken**. The CAD_FEATURE perf line is truncated at 1023 characters (H-03, E1).
6. **Whether FC-R1 D5 (stale overlay after zoom) still holds on this tree.**
   - Outcome: **refuted**. The code is fixed and the check passes (H-04).
7. **Whether C2's double-precision change broke the triangle → face mapping.**
   - Outcome: **not broken**. `generateMesh` copies the cached regeneration, so the index order equals the face-tag order (`cad_body.cpp:1559-1589`). `PICK_C2_13` passes (E1).

## Positive claims with evidence

| Claim | Evidence |
| --- | --- |
| SUBOPTIMAL is a rebuild request only when it is not the identity-preTransform steady state. OUT_OF_DATE always rebuilds. | Policy half: `classifyRenderResult` (`render_recovery.cpp:36-60`), RENDER_RECOVERY 24/0 host and device token (E1). Renderer half: decided INLINE at `renderer.cpp:3803-3809` and `:3858-3860` (`!expectSuboptimal_`), not through `classifyRenderResult`, so the suite does not exercise it; the audit's emulator runs are portrait. Renderer half graded E2 + Gate P1 "both orientations" (PROJECT_STATUS `:2971`, E3) → PASS-SOURCE-AND-RECORDED-RUNTIME (Round 2, seat A O-11) |
| Device loss is bounded (2 rebuilds), terminal, and costs no project byte. | `render_recovery.cpp:62-117` (E1 suite); `DiagnosticsAndRendererLossTest.e2er1b07_*` in `CI FULL SHARDED 36607747079` @`fa0b6b4` (E3; PROJECT_STATUS `:420`, `cad-vertical-slice-r1/FULLSHARDED_C4.md` §6). Transfer: `git log fa0b6b4..70a6c14` shows no commit to `forgeshape_renderer.{h,cpp}`, `forgeshape_render_recovery.{h,cpp}`, `DiagnosticsAndRendererLossTest.java`, `ForgeShapeActivity.java`, `AutosaveController.java` or `Diagnostics.java`; `forgeshape_jni.cpp` and `EditorWorkspaceView.java` did change, but the `renderThreadMain` hunks add only Freeform gizmo/overlay producers and the extrude view facts, not the attach / `drawFrame` / `!drew` path, and no hunk touches `onRendererRestartRequired` (Round 2, seat A O-12) |
| The C2 double-precision Möller–Trumbore closes the far orthographic seam crack for any origin from 50 to 500 m and for both triangle orders. | `PICK_C2_01..11` in PICKING 185/0 (E1); C2 DEVICE D19–D22 (E3) |
| A picked triangle maps to a semantic face through the published mesh's own face tags. The triangle index is never stored. | `support_chooser.cpp:199-243`, `cad_body.cpp:1559-1589` (E2); `PICK_C2_13`, CAD_A3 66/0 (E1) |
| Hidden and Isolate are enforced in one snapshot, and the renderer and `pickScene` read that same snapshot (`viewSceneSnapshot`). The support chooser picks through `scene.snapshot()` (`support_chooser.cpp:183`), equivalent because Isolate exists only in Sculpt and the chooser is not offered there (E2+E5). | `scene.cpp:328-372`, `selection.cpp:14-21` (E2); SCENE 175/0, OBJ018A-03, S027 (E1) |
| The outline mask is pixel-consistent with the shaded draw: same buffers, same MVP composition, same cull/depth state. Off costs one boolean. | Same buffers / MVP / cull / depth: `renderer.cpp:3645-3704, 2463-2475, 3086-3103, 3614-3643` — **E2 only**. `SelectionOutlineVisualEvidenceTest` (1 test in 32/32, CI DEVICE `40797031704`, emulator) asserts only band presence, a thickness of 1..8 px and one ground-family colour (`SelectionOutlineVisualEvidenceTest.java:454-464`) — E1 for those. No validation session has run since the outline existed (HYP-H3) (Round 2, seat A O-13) |
| Per-body revision+shading gating: a steady frame does no geometry work. Editing one body never re-uploads another. | `renderer.cpp:1119-1122, 718` (E2). DYNAMIC_MESH 91/0 tests the MeshStore, not `Renderer::syncBody` (no self-test calls it). Runtime trace: 130 `FORGESHAPE_RENDER_MESH_BUILD … rebuilds=` lines in `fc2/ci/dev1/test-logcat.txt` — a trace, not an assertion → PASS-SOURCE-AND-RECORDED-RUNTIME (Round 2, seat A O-14) |
| The camera-caused sketch overlay rebuild advances the revision (FC-R1 D5 closed in code). | `sketch_session.cpp:3777-3779`; `CADFC1_S4_a` (E1) |
| Frame synchronisation is correct for 2 frames in flight: a `renderFinished` semaphore per image and an image fence wait before reuse. | `renderer.cpp:3812-3846` (E2); Gate P1 debug Vulkan validation session with zero ForgeShape-caused messages (PROJECT_STATUS `:2482`, E3, pre-outline). `NATIVE_VIEWPORT_OK` proves startup only → PASS-SOURCE-AND-RECORDED-RUNTIME (Round 2, seat A O-14) |
| Release carries no self-test code. | `RELEASE_SELFTEST_GUARD=PASS`, SelfTests symbols 0 / strings 0 for both ABIs: CI FAST `40796992679` `release-selftest-guard.md` (E1), and the later local rerun in `fc2/release_guard.txt` (the first scripted run exited 2, `fc2/run_status.txt`) |

## Journeys proposed for END_TO_END_JOURNEYS

| Id | Journey | Negative? | Expected | Today |
| --- | --- | --- | --- | --- |
| J-H1 | Open a project → inject device loss (debug seam) → wait for the renderer to settle → encode and edit. | yes (fail-closed) | Healthy with rebuilds+1, or RestartRequired. Native snapshot and `.forge` bytes are identical either way. Editing still publishes. | `e2er1b07` (E3) |
| J-H2 | Renderer stops while the app stays foreground, with no pause/resume. | yes | Within one refresh the status line says a restart is required, a checkpoint is written, and a diagnostic report exists. | FAILS by source (H-01, DEFECT-CONFIRMED E2). Needs a test that does not call `onRendererRestartRequired()` itself. Seat A adopts it as J-H2. |
| J-H3 | CAD: draw a 63×63 grid → Finish → select all 3969 faces → orbit for 5 s → drag the extrude arrow → commit, on a physical phone with a release build. | no | Record the UI-thread block for Finish, the frame-time distribution while orbiting (overlay rebuild) and while dragging (latest-only evaluation). No ANR. | Unmeasured (OWNER-PHYSICAL-UNVERIFIED) |
| J-H4 | Select A → select B via a viewport tap → toggle the outline off and on → Delete B → Undo. | no | Outline follows; mask allocations unchanged; composite draws stop while off; no stale outline after Delete. | `SelectionOutlineTest` (E3); visual evidence (E1, emulator) |
| J-H5 | Orthographic view zoomed far out → tap a CAD pocket floor exactly on a triangle seam inside the support chooser. | no | The floor face `(feature, CapFar)` is chosen, never the XY plane behind it. | C2 D19–D22 (E3) |
| J-H6 | Edit → while the checkpoint encodes, a second edit → Undo within 0.9 s → kill the process → relaunch → Recover. | yes | The recovered state equals the last live state, not the undone edit. | At risk by source (H-02; cross-referenced by seat A under J-A3 with A-02 / A-13) |
| J-H7 | Sculpt a 100k-vertex body (debug ladder tier C → F) with continuous Grab strokes for 10 s on a phone. | no | The frame time per stroke sample is recorded against an owner budget. | Unmeasured (H-06) |

## Owner decisions needed

1. **Frame pacing.** Keep continuous rendering, or draw on change with explicit frame requests for evidence (H-07; FC-R1 disagreement 3).
2. **Performance budgets on a physical device.**
   - The maximum UI-thread block for Finish / Edit commit at the arrangement cap.
   - The sculpt stroke frame time at the ~100k tier.
   - Whether the arrangement and candidate derivation should move off the UI and render threads (H-05, H-06).
3. **Whether RestartRequired must be surfaced immediately mid-session.** CLAUDE.md implies yes (H-01).
4. **The minimum supported feature size for picking.** A relative threshold changes no format (H-10).

## Unresolved dependencies and open questions

- **ARM64 timings are absent.**
  - All host numbers here are x86 Xeon family 6 model 85 at `-O1`. CAD-R2-C1 showed that this CPU's JCC erratum moves identical code by about 25 % with link layout, so they are relative indicators only.
  - The C2 picking cost (+2.4 % to +3.0 %) is also x86-host only and is not upgraded to ARM64 here.
  - The emulator numbers are debug-build SwiftShader on a CI VM.
- **No Vulkan validation-layer session is recorded since SEL-OUT-R1.** The mask/composite pass and the depth attachment shared across frames have never run under validation (HYP-H3).
- **Device evidence for CAD-R2 parametric timings cannot exist** until H-03 is fixed.
- **Whether the 5 s `surfaceDestroyed` bound was an owner-approved trade** is not recorded anywhere this seat found.
- **The canonical Master Plan and the OWNER_DECISIONS files are not available.** Horizons are reconstructed from CLAUDE.md, PROJECT_STATUS and the artifacts.

Completeness: **PARTIAL**.

- The renderer, picking and concurrency sources were read end to end for the paths listed, and every finding has a source or runtime grade.
- No physical-device, release-ARM64, GPU-validation or thermal evidence could be produced in this environment, and the canonical plan sources are missing. Every performance conclusion therefore stops at host/emulator plus historic E3.

## Round 2 — objections from seat A and responses

Seat H re-verified each objection against source at `70a6c14` and the fresh outputs in
`scratchpad/fc2/` (`host_selftests.txt`, `run_status.txt`, `release_guard.txt`,
`ci/dev1`, `ci/dev2`, `ci/fast`). Nothing under `app/`, `testdata/`, `scripts/` or the
owner documents was touched. Corrections were applied to `SEAT_H.md` and
`work/req_H.tsv` (pre-Round-2 copies kept in this session's scratchpad as
`SEAT_H.before_r2.md` / `req_H.before_r2.tsv`).

| # | Objection (short) | Re-verification | Response | Change made | Status |
| --- | --- | --- | --- | --- | --- |
| O-1 | H-01 / RND-006 should be DEFECT-CONFIRMED | `ForgeShapeActivity.java:336` is the only production call of `checkRendererLifecycle`; javadoc `:363-369` names a "debug injection's follow-up" caller that does not exist in `app/src/main`; `onRendererRestartRequired` is otherwise called only by `DiagnosticsAndRendererLossTest.java:228`; `jni.cpp:2049-2055` comment states intent to checkpoint and tell the user, `:2056-2064` only stores atomics | CONCEDE | H-01 and RND-006 → DEFECT-CONFIRMED (E2), P2 kept; stale javadoc added as evidence; J-H2 marked adopted by seat A | RESOLVED |
| O-2 | RND-037 horizon is an audit recommendation; relabel `finishOn` figure | `PROJECT_STATUS.md` has no UI-thread / render-thread budget (grep "budget": only test and toolbar budgets). The device key is `finish_us` = `Driver::finishOn(gridSketch(63,63))` = 125 placements + `finish()` (`cad_multiface_selftest.cpp:76-85, 818-821`) | CONCEDE | RND-037 → H2 / BLOCKED-OWNER; H-05 kept as P2 risk; table row relabelled (`finish_us`, 125 placements + Finish, debug SwiftShader; second run 7,130,056 µs added) | RESOLVED |
| O-3 | H-06 mechanism misstated; RND-019 horizon | `jni.cpp:10036-10040` → `publishSculptRepresentation` (`:1170-1180`) publishes into the MeshStore only; `syncBody` gate `renderer.cpp:1119-1122` runs once per frame and only when the revision moved; `PROJECT_STATUS.md:4534` records the cost model with a baseline and no budget | CONCEDE | H-06 retitled and reworded ("per sample, a whole-mesh publish; per frame during a stroke, a full render-mesh rebuild, upload and GPU drain"); impact now per frame; RND-019 → H2 / BLOCKED-OWNER; P2 risk kept | RESOLVED |
| O-4 | RND-038 (idle loop) has no owner record | Wait predicate includes `running` (`jni.cpp:1822-1826`); no repo record requires idling | CONCEDE | H-07 and RND-038 → BLOCKED-OWNER (H2 until decided) | RESOLVED |
| O-5 | RND-039 / F-07 is recorded debt | `PROJECT_STATUS.md:4679` "held as debt (reported not implemented)" | CONCEDE | RND-039 → H2, FUTURE-NOT-DUE (recorded debt, still present in code, E2+E3) | RESOLVED |
| O-6 | H-02 confirmed; overlap with A-02 / A-13 | No change requested | ACKNOWLEDGE | J-H6 notes seat A's cross-reference under J-A3 | RESOLVED |
| O-7 | H-03: cite both runs, narrow impact | Device line from the token on is 1023 chars in `ci/dev1` (`40797031704`, ends `parametric_ch`) and `ci/dev2` (`40800569636`, ends `par`); host 1243. Device keys run through `drafting_dims100_segments`, so region / Add / Add+Cut / codec / `arrangement_cap_us` survive. Grep over `artifacts/` (excluding this council) found the lost keys only in host same-machine perf files | CONCEDE | H-03 and RND-058 cite both runs; impact narrowed to `parametric_*`, `cad_r2_*`, `CAD_R2_ORACLE_PASS`; "nothing published is wrong" recorded | RESOLVED |
| O-8 | H-13 / RND-057 overstated | Host `HOST_SUITE_OK PROJECT (256 checks, 0 failed)` under an 8192 cap (`host-native-selftests.sh:104`) = device 256; `Recorder::check` silently drops past the cap (`project_selftest.cpp:25-31`) | CONCEDE | H-13 and RND-057 → PARTIAL (latent test-infrastructure risk, E1); "no check dropped today" stated | RESOLVED |
| O-9 | RND-046, RND-051, RND-065 requirements were seat-invented | Sources in `req_H.tsv` were "seat H counterexample", "seat H perf", "seat H cold start"; no repo record. `PROJECT_STATUS.md:4534` notes the linear pick scan as wanting acceleration, without a budget | CONCEDE | All three → H2 / BLOCKED-OWNER; E1 measurements kept as observations (H-10, H-14) | RESOLVED |
| O-10 | H-11 "safe today" is reasoning, grade E2+E5 | `render_recovery.cpp:75-79` `SurfaceLost → Continue`; argument also relies on the renderer's own window reference | CONCEDE | H-11 evidence line now E2 (bound) / E2+E5 (safety) | RESOLVED |
| O-11 | RND-002 renderer half graded too high | `renderer.cpp:3803-3809` and `:3858-3860` decide SUBOPTIMAL inline (`!expectSuboptimal_`), independent of `classifyRenderResult` (`render_recovery.cpp:36-60`); audit emulator runs are portrait; Gate P1 "both orientations" `PROJECT_STATUS.md:2971` | CONCEDE | RND-002 → PASS-SOURCE-AND-RECORDED-RUNTIME (policy E1, renderer E2+E3); positive-claim row split | RESOLVED |
| O-12 | Name the E3 run for RND-005 / RND-063 | `36607747079` @`fa0b6b4` recorded in `PROJECT_STATUS.md:420` and `cad-vertical-slice-r1/FULLSHARDED_C4.md` §6. `git log fa0b6b4..70a6c14` on renderer, recovery, `DiagnosticsAndRendererLossTest`, `ForgeShapeActivity` → 0 commits; also 0 diff on `AutosaveController`/`Diagnostics`. Refinement: `forgeshape_jni.cpp` and `EditorWorkspaceView.java` DID change, but the `renderThreadMain` hunks add only Freeform gizmo/overlay producers and extrude view facts, and no hunk touches `onRendererRestartRequired` | CONCEDE (with refinement to seat A's transfer argument) | RND-005, RND-063 and positive claim 2 name the run and the transfer reasoning | RESOLVED |
| O-13 | Outline "pixel-consistent" claim exceeds what the E1 test asserts | `SelectionOutlineVisualEvidenceTest.java` has 1 `@Test`; asserts `:454-464` are band present, median thickness 1..8 px, no other-family pixels | CONCEDE | Claim 6 split: buffers / MVP / cull / depth E2 only; E1 covers presence, width, colour on the emulator; linked to HYP-H3 | RESOLVED |
| O-14 | Claims 7 and 9 lean on weak tokens | No self-test calls `syncBody` (grep); 130 `FORGESHAPE_RENDER_MESH_BUILD` lines in `ci/dev1/test-logcat.txt` are a trace; `NATIVE_VIEWPORT_OK` proves startup; Gate P1 validation session zero messages `PROJECT_STATUS.md:2482` (pre-outline) | CONCEDE | RND-014 and RND-012 → PASS-SOURCE-AND-RECORDED-RUNTIME | RESOLVED |
| O-15 | Claim 5 wording: chooser uses `scene.snapshot()` | `support_chooser.cpp:183` confirmed; `selection.cpp:14-21` `pickScene` uses `viewSceneSnapshot` | CONCEDE | Claim 5 now names `pickScene`; equivalence argument (Isolate is Sculpt-only, the chooser is not offered in Sculpt) graded E2+E5 | RESOLVED |
| O-16 | Part of HYP-H2 is already an E2 fact | `jni.cpp:1852-1856` sets `running = true` on any successful attach; `Renderer::attachSurface` (`renderer.cpp:433-486`) never reads `recovery_`; policy consulted only on failure (`:394-430`); `jni.cpp:2018` re-stores lifecycle every frame; `rendererFailed` also written at `:1790` and reset at `:2089`, never read | CONCEDE | HYP-H2 split: "no lifecycle gate on attach" is E2 fact; "viewport visibly returns while the notice says restart" stays a hypothesis; RND-007 UNDETERMINED-SOURCE-GAP → PARTIAL | RESOLVED |
| O-17 | H-09 / H-12 skipped without record | No file in `artifacts/function-council-r2/` or the seat scratch dir mentions either ID | CONCEDE | Header states both IDs are unassigned, dropped while drafting, with no surviving content and no finding asserted under them | RESOLVED |
| O-18 | Release-guard provenance contradiction | `fc2/run_status.txt`: `guard_exit=2` at 00:54:48; `fc2/release_guard.txt` written 00:57 with `RELEASE_SELFTEST_GUARD=PASS` (later rerun, exit not in `run_status`); CI FAST `40796992679` `release-selftest-guard.md` PASS | CONCEDE | Sources table, H-15 and claim 10 now name the rerun and CI FAST `40796992679` as provenance and state the first scripted run exited 2 | RESOLVED (seat A's half is seat A's to apply) |
| O-19 | Seat A to add J-H2 / J-H3 | Directed at seat A; no contradiction | AGREE | None needed in SEAT_H (J-H2, J-H3 already proposed here) | RESOLVED |

Tally: 17 conceded with a change (O-1..O-5, O-7..O-18), 2 acknowledged with no change
needed (O-6, O-19), 0 rebutted, 0 unresolved between the seats. The horizon
reclassifications (RND-019, -037, -038, -046, -051, -065) remain open as OWNER
decisions (budgets, frame pacing, minimum pickable feature), not as seat disagreements.
