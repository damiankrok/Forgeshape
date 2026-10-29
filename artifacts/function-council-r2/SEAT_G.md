# FUNCTION-COUNCIL-R2 — Seat G: Future desktop (Windows / Linux) readiness — design only

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` identical at the audit HEAD
`c84d4fb`: `git diff --stat 70a6c14 HEAD -- app scripts .github` is empty). Research only:
nothing under `app/`, `testdata/`, `scripts/`, `.github/`, Gradle files or the owner docs was
modified; no Gradle was run by this seat. Scratch: `/tmp/claude-0/fc2-scratch/G/` (exact commands
in `/tmp/claude-0/fc2-scratch/G/COMMANDS.txt`; never part of the build). Requirement rows:
`work/req_G.tsv` (75 rows, `PORT-001`..`PORT-075`). Companion deliverables:
`PORTABILITY_MATRIX.md`, `DESKTOP_PORT_STRATEGIES.md`, `DESKTOP_POC_GATE.md`.

## Scope

Portability of every module toward a FUTURE Windows/Linux desktop shell, as a design question:
the platform-neutral C++17 domain (math, geometry, representations, history, codec, scene,
picking, sketch, CAD kernel boundary), the JNI binding, the Android Java shell, the Vulkan
renderer and its surface seam, the NDK/CMake toolchain, shaders, resources and CI. A desktop
client is **H2**: `ARCHITECTURE.md:195-201` ("no Apple, Windows, web or cloud client … none is
authorized") and `ARCHITECTURE.md:216-218` ("This is deliberately not an abstraction layer").
What is due now (H0/H1) is `ARCH-OWNER-01` (`PROJECT_STATUS.md` *Owner Decision Baseline*): the
domain stays platform-neutral, Android types stop at `forgeshape_jni.cpp`, input crosses as
semantic samples, platform services stay narrow, renderer surface coupling stays explicit. No
port code was written and no dependency is proposed as decided.

## Sources read

| Path | Revision (last commit touching it) | What was used |
| --- | --- | --- |
| `artifacts/function-council-r2/BEFORE.md` | `9cfc3f5` | snapshot, inventory, environment |
| `CLAUDE.md` | `911f5c3` | hard rules (no engine, no third-party library, NDK pin, domain neutrality, `Uri` rule, render rules, naming) |
| `ARCHITECTURE.md` | `9144169` | `:150`, `:195-218` (platform constraints), `:211` (hover outside vocabulary), `:214` (renderer coupling) |
| `PROJECT_STATUS.md` | `70a6c14` | Owner Decision Baseline (`ARCH-OWNER-01`, `UI-OWNER-02`, `INPUT-OWNER-01`) `:2555-2600`; ARM64 FP hazard `:2438-2449`, `:4153-4157`; Gate P1 `:2971-2974`, `:3856-3863`; toolchain table `:2624-2626` |
| `README.md` | `7e02240` | `:440-465` host self-tests |
| `docs/CI_CLOUD.md` | `3e2443f` | CI scope |
| `DATA_PACKAGE_SPEC.md` | `94a21dd` | §3 section header, §5 `SCNE` layout (`:196-222`), `:41-44` |
| `.github/workflows/{ci-fast,ci-device,ci-full-sharded}.yml` | tree | which steps run |
| `scripts/host-native-selftests.sh` | tree | host build recipe |
| `app/src/main/cpp/CMakeLists.txt`, `app/build.gradle`, `app/src/main/AndroidManifest.xml` | tree | toolchain, ABIs, features |
| Fresh audit outputs: `scratchpad/fc2/{host_selftests,host_corpus_verdicts,gradle,release_guard,run_status}.txt` | this audit | host 4392/0, 72 verdicts, `BUILD SUCCESSFUL`, `RELEASE_SELFTEST_GUARD=PASS` |
| `artifacts/function-council-r1/*` | older tree `a1b50f2` | checked: contains no portability seat; nothing carried over |

Not available (per `BEFORE.md`): Master v4.1.61, OWNER_DECISIONS_*, MASTER_PLAN_HISTORY_*,
PROJECT_STATE. Horizons are reconstructed from the repo records above.

## Inspected code paths

| File:lines | Function / fact |
| --- | --- |
| `forgeshape_renderer.h:1-16, 136-150, 236-248, 610` | `VK_USE_PLATFORM_ANDROID_KHR`, `<android/native_window.h>`, `attachSurface(ANativeWindow*)`, `rebuildDeviceAfterLoss`, `window_` |
| `forgeshape_renderer.cpp:3, 46-50` | `<android/log.h>`, `FS_LOGI/E` |
| `forgeshape_renderer.cpp:240-275` | `createInstance`: extensions `VK_KHR_SURFACE`, `VK_KHR_ANDROID_SURFACE` (`:262`), apiVersion 1.0/1.1 |
| `forgeshape_renderer.cpp:357-380` | `rebuildDeviceAfterLoss`: `ANativeWindow_acquire` (`:372`) |
| `forgeshape_renderer.cpp:433-448` | `attachSurface`: `vkCreateAndroidSurfaceKHR` |
| `forgeshape_renderer.cpp:488-503` | `detachSurface`: `ANativeWindow_release` (`:499-501`) |
| `forgeshape_renderer.cpp:2098-2200` | `createSwapchain`: extent fallback `:2104-2107`, identity preTransform, format choice, compositeAlpha `:2175-2178`, FIFO |
| `forgeshape_jni.cpp:19-35` | includes: `android/log.h`, `native_window.h`, `native_window_jni.h`, `jni.h`, `<thread>`, `<mutex>` |
| `forgeshape_jni.cpp:151-307` | `ViewportThread`, `g_viewport`, renderer atomics, `g_camera`, `g_selection`, `g_stateMutex`, gesture globals, `kChooserTapSlopPixels` (`:307`) |
| `forgeshape_jni.cpp:404-413` | `translateAction` — hover/scroll/button dropped |
| `forgeshape_jni.cpp:2261-2271` | `debugMeshCommand` — `NDEBUG` no-op |
| `forgeshape_jni.cpp:3886-3896` | `setGizmoPixelScale` |
| `forgeshape_jni.cpp:8440-8480` | `loadProject` — gesture drop, sketch cancel, `loadProjectDocument` |
| `forgeshape_jni.cpp:9404-9500` | `surfaceCreated/Changed/Destroyed` |
| `forgeshape_jni.cpp:9503-10319` | `touchEvent` — the gesture arbitration (≈816 lines) |
| `forgeshape_input.h:27-159` | `TouchAction`, `PointerToolType` (incl. `Mouse`), pressure/tilt sanitizers, `TouchPointer`, `kMaxTrackedPointers = 6` |
| `forgeshape_camera.h:101, 110, 199-204`; `forgeshape_camera.cpp:278-311` | per-pixel orbit/pinch; one pointer orbits, two pan+pinch |
| `forgeshape_selection.h:37, 60-61` | `kTapSlopPixels`, `pickScene` |
| `forgeshape_sketch_session.h:210`; `.cpp:1106-1268, 3248, 4323-4326` | sketch tap slop px; hit tolerance in units; `sketchSession()` |
| `forgeshape_gizmo.cpp:12-15, 387-398, 1843`; `forgeshape_gizmo.h:231, 319-324` | global density atomic and its bounds; `gizmoSession()` |
| `forgeshape_freeform_session.h:45`; `.cpp:827` | slop in units; `freeformEditSession()` |
| `forgeshape_scene.cpp:19-193, 406-411, 604-641`; `forgeshape_history.cpp:448-450`; `forgeshape_display.cpp:269` | allocator; process-scoped accessors |
| `forgeshape_project_state.cpp:138, 398-416, 753` | capture writes `nextObjectId`; load pushes the allocator forward only; fingerprint mixes it |
| `forgeshape_history_selftest.cpp:463-483` | `eviction_is_deterministic_oldest_first` (exact `==` on doubles) |
| `forgeshape_cad_feature_selftest.cpp:5378-5398` | `CADFC2_PF_03b_BEFORE_…` (equal-area tie broken by anchor order) |
| Java: `ForgeShapeSurfaceView.java:60-161, 209-212`; `PointerSemantics.java:35-66`; `ForgeShapeActivity.java:387-430`; `EditorWorkspaceView.java:3007-3014`; `ProjectSlot.java:80-125`; `ProjectCheckpoint.java:56-185`; `ProjectTransfer.java:96-155`; `AutosaveController.java:60-191`; `ExactExpression.java:1-30`; `NativeViewport.java:24, 1079, 1186-1414, 2212` | input translation, hover, debug keys, density push, file writes, SAF, autosave, Java-held numeric policy, JNI signatures |

## Test coverage of the domain (what each actually asserts)

| Test | Asserts | Portability relevance |
| --- | --- | --- |
| `scripts/host-native-selftests.sh` (25 suites, fresh run `HOST_SELFTESTS_OK (4392 checks, 0 failed)`) | every suite's checks on Linux x86-64 g++ 13, no FMA | the only executable proof that the domain is platform-neutral; **not run by any CI workflow** (G-02) |
| Host `PICKING` checks `pointer_tool_code_maps_{finger,stylus,eraser,mouse}`, `pointer_tool_code_zero_is_unknown`, `pointer_tool_future_code_falls_back_to_unknown`, `pointer_tool_negative_code_falls_back_to_unknown`, `pointer_tool_code_round_trips`, `pointer_tool_name_is_never_null` (`forgeshape_picking_selftest.cpp:998-1018`) | wire codes of the neutral tool enum | the input boundary's stability |
| Host `PROJECT` (256 checks) incl. golden digests (`FSR1A_12_…`), FREEFORM/SURFACE digests | codec bytes equal the committed corpus | cross-toolchain byte determinism (verified: 6 FRFM/SURF digests = `sha256sum testdata/forge/v1/*`) |
| Host `RENDER_RECOVERY` | Vulkan-free device-loss policy | renderer policy testable without a GPU |
| fc2 `host_corpus_verdicts.txt` (orchestrator) | 72 fixtures: valid → `Ok cad_bodies=n regenerated=n`, corrupt → named status | codec off-Android |
| JVM `PointerSemanticsTest` (6 tests) | `MotionEvent.TOOL_TYPE_*` → neutral codes; unknown/negative → `TOOL_UNKNOWN`; codes 0..4 pinned | Android half of the input boundary |
| JVM `AppPreferencesTest`, `DiagnosticLogTest`, `LengthUnitTest`, `CadHudPresentationTest`, `CadHud3dPresentationTest` … (20 classes; fresh Gradle `BUILD SUCCESSFUL`) | pure-Java policy | the part a JVM desktop shell could reuse |
| androidTest `EditorWorkspacePointerTest` `inr111`..`inr121` | finger/stylus/unknown arrive as neutral types; pressure clamped not dropped; tilt in radians; multi-pointer data stays with its pointer; tap selection, camera and sculpt pending unchanged; two-pointer navigation never sculpts; pressure does not change a stroke | recorded on the x86_64 emulator (C2 DEVICE unions `40776968165`, `40782861799`) — E3 |
| androidTest `SpatialSketchTest.stylusHoverHighlightsATargetWithoutCommitting`, `CadRetainedSketchOwnerTest.d22_a_hover_on_the_seam_…` | hover highlights, never commits | hover path outside the input vocabulary |
| `scripts/ci-release-selftest-guard.sh` (fresh `RELEASE_SELFTEST_GUARD=PASS`) | 0 self-test symbols/strings in release `.so` on both ABIs | a desktop build would need the same guard |
| **None** | that no domain TU includes an Android header (only the manual host script) | G-02 |
| **None** | arm64 numeric behaviour of the suites (CI DEVICE is x86_64 only) | G-01 |

## Confirmed findings

### G-01 — P2 — Two native self-test oracles fail under FP contraction, which is the NDK's arm64 default
- **Path:** `app/src/main/cpp/forgeshape_history_selftest.cpp:474-480` (`eviction_is_deterministic_oldest_first`); `app/src/main/cpp/forgeshape_cad_feature_selftest.cpp:5392-5394` (`CADFC2_PF_03b_BEFORE_the_lens_taps_to_a_whole_circle_by_anchor_order`); `app/src/main/cpp/CMakeLists.txt` (no `-ffp-contract` policy).
- **Minimal repro (host, this audit):** build the 98 neutral TUs + the generated 25-suite main with `clang++ -O1 -mfma` (default `-ffp-contract=on`) → `HOST_SELFTESTS_FAIL (4392 checks, 2 failed)` naming exactly those two checks. Controls: same build without `-mfma` → 4392/0; with `-mfma -ffp-contract=off` → 4392/0. An arm64-like build (product −O0 as in the Debug `compile_commands.json`, kernel −O2, FMA) fails the same two.
- **Root cause (history check, proven):** NDK clang `--target=aarch64-none-linux-android26` with the Debug flags (no `-O`) emits `fmadd d0, d0, d1, d2` for the loop value `0.5 + 0.01*i` (`arm64_forgeshape_history_selftest.s:3967`, the only `fmadd` in the TU), while the oracle `oldestRetained` is folded at compile time unfused; `fma(0.01, 7, 0.5) = 0.56999999999999995` vs `0.57000000000000006`, compared with `==`. x86_64 NDK output for the same pattern is `mulsd`+`addsd`. The lens check is an equal-area tie broken by anchor order; under contraction the two areas differ by rounding and the tie flips (mechanism E5, failure E1).
- **Impact (split, Round 2):** the history check is expected to print `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_FAIL` on any arm64-v8a debug launch at this tree (H-G-01a, arm64 assembly proof E2 + host E1); the lens check (`FORGESHAPE_CAD_FEATURE_SELFTEST_FAIL`) is proven only under host emulation (`host_arm64like.txt`), so its arm64 device outcome is E5 (H-G-01b). A self-test FAIL only logs (`forgeshape_jni.cpp:526-534`, `:605-613` call `FS_LOGE` and do not stop the viewport), so the product is unaffected; the harm is polluted evidence (expected FAIL lines on arm64 can hide a real one); CI cannot see it because `CI DEVICE` runs the x86_64 emulator only. Product geometry is not shown to be wrong; the defect is ABI-fragile oracles, which also blocks any future desktop ABI that contracts (Windows-on-ARM, AArch64 Linux). The physical outcome itself is OWNER-PHYSICAL-UNVERIFIED (H-G-01).
- **Evidence grade:** E1 (host reproduction under the same FP semantics) + E2 (arm64 assembly); device E5.
- **Verdict:** DEFECT-CONFIRMED.
- **Isolated fix proposal:** compute each oracle through the same runtime expression as the value (or compare with a 1-ULP / 1e-12 tolerance), and give the lens check a non-tied fixture; add a CI FAST job running the host suites with `-mfma` (no device needed). Whether to pin `-ffp-contract=off` is an OWNER decision: `PROJECT_STATUS.md:2447-2449` already argues that pinning "would only re-hide the same knife-edge geometry".

### G-02 — P2 — Domain platform-neutrality is not guarded by CI
- **Path:** `.github/workflows/ci-fast.yml` (steps `:42-223`), `ci-device.yml`, `ci-full-sharded.yml` — `grep -rn host-native .github` → 0; `ARCHITECTURE.md:208` "that is asserted throughout … and must stay asserted".
- **Repro:** add `#include <android/log.h>` to any domain TU: the NDK build still succeeds, every CI workflow stays green; only a manual `bash scripts/host-native-selftests.sh` fails. (Reasoned from the workflow steps; not executed against a modified tree, which this audit may not create.)
- **Impact:** the `ARCH-OWNER-01` constraint is held by convention only; a leak would be found at port time.
- **Evidence:** E2. **Verdict:** PARTIAL. **Fix:** one CI FAST step `bash scripts/host-native-selftests.sh` on the existing Ubuntu runner (it needs only g++).

### G-03 — P3 — `ARCHITECTURE.md:214` misstates who releases the `ANativeWindow`
- **Path:** `ARCHITECTURE.md:214` ("`Renderer` never creates or releases one") vs `forgeshape_renderer.cpp:440` (takes ownership), `:499-501` (`ANativeWindow_release` in `detachSurface`), `:372` (`ANativeWindow_acquire` in `rebuildDeviceAfterLoss`); `forgeshape_renderer.h:241-243` documents the acquire.
- **Repro:** read the three lines. **Impact:** the one document describing the seam a second backend would sit beside gives the wrong ownership rule for the surface handle — exactly what a desktop surface seam would copy.
- **Evidence:** E2. **Verdict:** DOC-DIVERGENCE. **Fix:** reword: JNI acquires the window (`ANativeWindow_fromSurface`) and transfers its reference; `Renderer` owns and releases it in `detachSurface`, and re-acquires one across a device rebuild; `Renderer` never creates a window.

### G-04 — P3 — Self-tests read uninitialized floats on a failed projection
- **Path:** `forgeshape_cad_feature_selftest.cpp:5548-5550` and `:5594`; `forgeshape_sketch_ux_selftest.cpp:1474-1480` (10 `-Wsometimes-uninitialized` from `clang++ 18 -Wall -Wextra`; g++ `-Wall` in the host script does not flag them).
- **Repro:** `clang++ -std=c++17 -fsyntax-only -Wall -Wextra <file>`. The read happens only after the check is already false, so no verdict flips, but it is undefined behaviour in the evidence code — and MSVC (C4701) or a sanitizer build would flag it on a desktop toolchain.
- **Evidence:** E1. **Verdict:** DEFECT-CONFIRMED. **Fix:** initialise `bx, by, px, py, tx, ty` to 0.

### G-05 — P3 — DOC-DIVERGENCE: Freeform's tap slop is density-scaled while its comment says it is the sketch's (regraded Round 2)
- **Round 2 correction:** the gizmo is removed from the comparison (`kGizmoHitSlopUnits`, `forgeshape_gizmo.h:229-231`, is a hit-corridor half-width, in reference units on purpose per `UI-PREF-R1`). A requirement does exist: CLAUDE.md `CAD-V6-S2-CORRECTION-FILL-PICK-R2` "a finger inside the 24 px tap slop orbits nothing" (H1). The real mismatch is `kFreeformTapSlopUnits` (`forgeshape_freeform_session.h:43-45`, documented as "the sketch's own tap slop") × density (`forgeshape_freeform_session.cpp:640`) vs the sketch's raw 24 px (`forgeshape_sketch_session.h:208-210`, `.cpp:1106,1268`): 2.75× apart at density 2.75. **Evidence E2; Verdict DOC-DIVERGENCE (P3).** The desktop-DPI part below is FUTURE-NOT-DUE. Original text follows for the record.
- **Path:** physical px: `forgeshape_selection.h:37` (`kTapSlopPixels = 24`), `forgeshape_sketch_session.h:210` (`kSketchTapSlopPixels = 24`), `forgeshape_jni.cpp:307` (`kChooserTapSlopPixels = 24`), `forgeshape_camera.h:101,110` (orbit/pinch per px); reference units × density: `forgeshape_gizmo.h:231` (`kGizmoHitSlopUnits = 24`), `forgeshape_freeform_session.h:45` (`kFreeformTapSlopUnits = 24`).
- **Repro:** headless probe: a 100 px Mouse drag rotates −0.5000 rad at any density; at Android density 2.75 a sketch tap slop is ~8.7 dp while a Freeform tap slop is 24 dp.
- **Impact:** on a 1.0-density desktop monitor the px-based slops become ~3× larger physically and orbit ~4× faster per inch than on a phone; on Android today two tools disagree about what a "tap" is. No requirement states which unit is intended.
- **Evidence:** E2 + E1. **Verdict:** UNDETERMINED-SOURCE-GAP. **Fix:** one stated unit policy (reference units × pushed density) — an OWNER/UX decision first.

### G-06 — FUTURE-NOT-DUE / OWNER question — Re-encoded `.forge` bytes depend on what the process loaded before (regraded Round 2)
- **Round 2 correction:** no cited source requires the behaviour (`DATA_PACKAGE_SPEC.md:42` is about collisions; `:1563-1566` requires byte identity only for the same semantic document and encode→decode→encode); after a load into a process with a higher allocator the captured document is a different semantic document, and CLAUDE.md ("the `ObjectId` allocator is never rolled back") makes that intended. PORT-010's source is relabelled "derived (seat G)", horizon H2, verdict FUTURE-NOT-DUE (multi-document desktop impact). Same root cause as SEAT_J J-03; owner decision 3 below is merged with OD-J3. Original text follows for the record.
- **Path:** `forgeshape_project_state.cpp:410-415` (`reserveObjectIdsThrough(document.scene.nextObjectId - 1)`, forward-only), `:138` (capture writes the live `nextObjectId`), `:753` (fingerprint mixes it).
- **Repro:** `seq_reencode mixed_cad_v1.forge freeform_box_v1.forge` → `final reencode=different`; `cmp` shows SCNE `nextObjectId` at file offset 56 = 4 instead of 2, plus the SCNE CRC (offsets 45-48). The same fixture alone re-encodes identical.
- **Impact:** on Android, Open → Save after a larger project in the same process writes different bytes from the opened file (dirty tracking is unaffected — the baseline fingerprint is taken after the load). For a multi-document desktop shell one window's allocator would leak into another's saved bytes. Deliberate per the code comment; no rule says the bytes must match.
- **Evidence:** E1. **Verdict:** UNDETERMINED-SOURCE-GAP. **Fix:** an OWNER decision; if byte stability matters, a per-document allocator inside a future document context.

### G-07 — INFO — Product policy that a second shell would need is inside the JNI translation unit
- **Path:** `forgeshape_jni.cpp:9503-10319` (`touchEvent`: arbitration among sketch, chooser, gizmo, freeform, sculpt, camera, selection), `:8463-8479` (load orchestration), `:228-307` (gesture globals), `:230` (`g_stateMutex`, 250 lock sites).
- **Impact:** a desktop shell cannot reuse the gesture semantics without extracting them; today this is allowed by `ARCH-OWNER-01` (Android types do stop at JNI; the samples are neutral).
- **Evidence:** E2. **Verdict:** FUTURE-NOT-DUE.

### G-08 — INFO — One document per process; one display density per process
- **Path:** function-local static singletons (`scene.cpp:406`, `:604`, `:638`; `history.cpp:448`; `sketch_session.cpp:4323`; `gizmo.cpp:1843`; `freeform_session.cpp:827`; `display.cpp:269`), `gizmo.cpp:15` (`std::atomic<float> g_pixelsPerReferenceUnit`).
- **Mitigation already present:** the session classes take scene/history by reference and every suite builds its own (E1, host run).
- **Evidence:** E2. **Verdict:** FUTURE-NOT-DUE.

### G-09 — INFO — A mouse can only orbit
- **Path:** `forgeshape_jni.cpp:404-413` (hover/scroll/button events return false), no `onGenericMotionEvent` in the Java shell, `forgeshape_camera.cpp:289-311` (pan and zoom need two pointers), `PointerSemantics.java:41` ("carried only").
- **Impact:** with a mouse (desktop-mode Android today, or a future desktop) pan and zoom are unreachable. Deliberate and out of the phone+tablet scope (`UI-OWNER-02`).
- **Evidence:** E2 + E1 (Mouse drag orbits in the probe). **Verdict:** FUTURE-NOT-DUE.

## Hypotheses (unproven)

- **H-G-01a** — A debug launch on a physical arm64 phone at this tree prints `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_FAIL` (arm64 assembly E2 + host E1; device OWNER-PHYSICAL-UNVERIFIED).
- **H-G-01b** — The same launch prints `FORGESHAPE_CAD_FEATURE_SELFTEST_FAIL` (host emulation only; arm64 outcome E5).
- Neither is refuted by Gate P1's "full self-test suite VERIFIED" (`PROJECT_STATUS.md:2971`): `PROJECT_STATUS.md:4158-4163` records that the physical phone drops whole suites from logcat, so that run can show at most "no failures observed".
- **H-G-02** — Attaching or detaching a hardware keyboard recreates `ForgeShapeActivity` (and rebuilds the Vulkan surface) because `configChanges` (`AndroidManifest.xml:31`) lists `keyboardHidden` but not `keyboard`/`navigation`. Not reproduced (no device).
- **H-G-03** — `ProjectSlot`/`ProjectCheckpoint` fsync the file but not the directory, so on POSIX the rename may not be durable after power loss. Not reproduced; Android shares the risk. Same as SEAT_J HJ-2.
- **H-G-04** — On a desktop surface that supports neither `OPAQUE` nor `INHERIT` composite alpha, `createSwapchain` (`renderer.cpp:2175-2178`) would pass an unsupported value. No desktop surface exists to test.
- **H-G-05** — FP contraction could flip a near-tolerance accept/refuse in sketch/region validation, so a file that opens on one ABI is refused on another. Probed: the 72-fixture verdicts built with `clang -O1 -mfma` (contraction on) are IDENTICAL to the baseline (`verdicts_fma.txt` vs fc2 `host_corpus_verdicts.txt`), so no committed fixture flips; user geometry near a tolerance remains unproven.

## Adversarial counterexample attempts

| # | Claim attacked | Attempt | Outcome |
| --- | --- | --- | --- |
| 1 | "Everything below `forgeshape_jni.cpp` compiles with no Android" (`ARCHITECTURE.md:216`) | 98 TUs with clang 18 + libstdc++, clang 18 + NDK libc++, `-Wpedantic`; 101 headers standalone | **Failed to break**: 98/98, 98/98, 100/101 (only `forgeshape_renderer.h`) |
| 2 | "Renderer coupling is explicit and local" | compile `forgeshape_renderer.cpp` on Linux with only 5 stubbed Android declarations + Khronos headers | **Failed to break**: 0 errors, 0 warnings |
| 3 | "Host self-tests are ABI-independent evidence" | rebuild the 25 suites with FMA contraction (arm64's default) | **Broke**: 2 checks fail (G-01); controls pass |
| 4 | "Load → save is a pure function of the document" | load two fixtures sequentially in one process, re-encode | **Broke** byte identity (G-06), by documented design |
| 5 | "`Renderer` never creates or releases an `ANativeWindow`" | read `detachSurface` / `rebuildDeviceAfterLoss` | **Broke** (G-03) |
| 6 | "No Uri/path crosses JNI" | list every `static native` taking `String`/`byte[]`/`Object` | **Failed to break**: only names, status tokens, `byte[]` documents, and `Surface` |
| 7 | "FP contraction changes which files open" (H-G-05) | decode + regenerate all 72 fixtures with an FMA-contracted build | **Failed to break**: 72/72 verdicts identical to the baseline |

## Positive claims with their evidence

| Claim | Evidence |
| --- | --- |
| The domain (98 TUs, ~57.8k product lines) is platform-neutral and links into a working non-Android program | E1: host suites 4392/0; headless probe links product objects only and runs |
| The codec is byte-deterministic across g++ 13 and clang 18 (`-O1`, with/without `-mfma`), both with libstdc++ | E1: FRFM/SURF digests from g++ x86_64 = committed SHA-256s; 72/72 verdicts; probe re-encodes 7 fixtures identically in a fresh process. **libc++ (NDK STL) was only syntax-checked (`-fsyntax-only`); no libc++ encode ran in this audit — the STL claim is E5 (Round 2).** |
| The renderer's Android surface coupling is 4 `ANativeWindow_*` functions, one surface extension and one log function | E1 stub probe; E2 line list |
| Shaders are portable SPIR-V 1.0 with no extension | E1 `glslc` + `spirv-val` 10/10 |
| JNI is a pull-only, single-class contract (275 exports = 275 natives, no callbacks) | E2 grep counts |
| No prohibited dependency (Qt, SDL, GLFW, GLM, Metal, MoltenVK) and no network code | E2 grep (`-w`, outside `third_party/manifold`) |
| Release builds exclude self-tests on both ABIs | E1 fresh `RELEASE_SELFTEST_GUARD=PASS` from the 00:57:29 rerun (`release_guard.txt`), not the first attempt (`run_status.txt` 00:54:48 `guard_exit=2`, an `exit 2` path of `ci-release-selftest-guard.sh:29,37,58`). The guard's debug rows read `app-debug.apk` built 2026-10-28 21:20 (`assembleDebug` UP-TO-DATE, `gradle.txt:56`) |
| Both `.so` files carry 0x4000 LOAD alignment | E1 (Round 2): `readelf -lW` on the fresh release APK, every LOAD 0x4000 on arm64-v8a and x86_64; 16 KB-page runtime OWNER-PHYSICAL-UNVERIFIED |

## Journeys proposed for END_TO_END_JOURNEYS

| ID | Title | Negative? | Steps | Expected | Test today | Status |
| --- | --- | --- | --- | --- | --- | --- |
| J-G-01 | Headless open, orbit, pick, re-encode | no | decode fixture → `loadProjectDocument` → camera 1920×1080 → Mouse drag 100 px → `pickScene` at centre → capture → encode | load `Ok`; yaw −0.5 rad; centre hit names an ObjectId; bytes identical | seat-G `headless_shell_probe` (scratch only) | PASS-REPRODUCED (E1); not in CI |
| J-G-02 | Corrupt project is refused before anything changes | **yes** | feed `truncated_v1.forge` (and the other refusal fixtures) to the same driver | named refusal (`Truncated`, …); scene untouched | host corpus verdicts; probe | PASS-REPRODUCED (E1) |
| J-G-03 | arm64 debug launch emits all 25 `*_SELFTEST_OK` tokens | **yes** (regression guard) | build Debug arm64-v8a, launch on a physical phone with `logcat -G 64M`, or run the host suites with `-mfma` | 25 OK + `FORGESHAPE_NATIVE_VIEWPORT_OK` | none automated | DEFECT-CONFIRMED on host under arm64 FP semantics (G-01); device OWNER-PHYSICAL-UNVERIFIED (H-G-01) |
| J-G-04 | Platform-neutrality gate catches a leak | **yes** | CI runs `scripts/host-native-selftests.sh`; a domain TU that includes an Android header fails the job | job red | none (G-02) | PARTIAL (constraint held manually; no CI gate) |
| J-G-05 | Density change keeps the 48-unit hit floor | no | change display size (Android) / move window (desktop) → refresh → grab a gizmo handle | `setGizmoPixelScale` receives the new density; handle hit area tracks it | host GIZMO suite (scale arithmetic) | IMPLEMENTED-UNTESTED for the density-change path (E2) |

## Owner decisions needed

1. Authorize (or not) a desktop **spike** as research, its first OS, and its shell family (S1 native / S2 JVM / S3 library-assisted; S3 needs a licence/architecture gate per library) — `DESKTOP_POC_GATE.md` G0-1, G0-2.
2. FP-contraction policy: pin `-ffp-contract=off`, or require tolerant oracles and an FMA CI job (G-01; `PROJECT_STATUS.md:2447-2449` argues against pinning).
3. Whether an allocator-only difference is a different document — for Open → Save bytes (G-06) and for dirty tracking (SEAT_J J-03). **One decision with OD-J3.**
4. A desktop-DPI unit policy for interaction thresholds (FUTURE-NOT-DUE); the Android Freeform/sketch slop mismatch is a DOC-DIVERGENCE fix, not an owner decision (G-05).
5. Whether desktop-mode Android (mouse + keyboard on ChromeOS / DeX / desktop windowing) is in scope, which would bring wheel/button/hover into `forgeshape_input.h` (G-09, `UI-OWNER-02`).
6. Whether an arm64 automated run (runner or emulation job) is wanted, since CI DEVICE is x86_64 only (PORT-032: H2 owner decision, no gap).

## Unresolved dependencies and open questions

- MSVC compatibility: INACCESSIBLE here (no MSVC); source grep found no GCC-only constructs.
- Physical arm64 at this tree: no run available (H-G-01).
- Whether Gate P1's physical self-test run included `eviction_is_deterministic_oldest_first`: moot — even if it did, `PROJECT_STATUS.md:4158-4163` limits that run to "no failures observed" (Round 2). (The repository is also shallow.)
- RESOLVED (Round 2): `guard_exit=2` was the first attempt (00:54:48); `release_guard.txt` PASS was rewritten at 00:57:29 by a later rerun.

**Completeness: COMPLETE for the seat's design-only scope** — every module class was mapped with
E1/E2 evidence, portability was verified empirically (compile, link, run, FP experiment) and all
three design deliverables plus 75 requirement rows are written; it is limited by
`PARTIAL-PLAN-SOURCE-GAP` (Master plan unavailable), no MSVC, and no physical arm64 device, which
are recorded as INACCESSIBLE / OWNER-PHYSICAL-UNVERIFIED rather than assumed.
## Round 2 — objections from seat J and responses

Re-verification commands (read-only; scratch in `/tmp/claude-0/fc2-scratch/G/`):
`sed -n` over `ProjectSlot.java:95-155`, `ProjectCheckpoint.java:100-125`, `forgeshape_jni.cpp:520-536,600-615`,
`forgeshape_freeform_session.{h:40-46,cpp:636-642}`, `forgeshape_sketch_session.h:205-211`, `forgeshape_gizmo.h:225-232`,
`forgeshape_history_selftest.cpp:460-482`, `PROJECT_STATUS.md:2968-2973,4155-4165`;
`grep -rn '\.pending\|PENDING' app/src/main/java`; `ls -l --time-style=full-iso` on the fc2 scratchpad and APK outputs;
`git diff --stat fa0b6b4 70a6c14 -- AutosaveController.java ProjectSlot.java ProjectCheckpoint.java forgeshape_project_state.cpp`;
`unzip -o -q app/build/outputs/apk/release/app-release-unsigned.apk 'lib/*' -d /tmp/claude-0/fc2-scratch/G/rel && readelf -lW .../lib/<abi>/libforgeshape_native.so | grep LOAD`
(release APK sha256 `3b4d3194c68cf19725fa360e73d297b6dbf52fef1c55d549ed3534ba250c614c`). Pre-Round-2 copies: `SEAT_G.before_r2.md`, `req_G.before_r2.tsv` in the scratch dir.

| # | Objection (seat J) | Re-verification | Response | Change made | Status |
| --- | --- | --- | --- | --- | --- |
| 1 | G-01 stands; split device impact; cite `PROJECT_STATUS.md:4158-4163`; FAIL only logs | `history_selftest.cpp:466-480` (`overflow` is `const size_t`, oracle folded); `jni.cpp:526-534`, `:605-613` only `FS_LOGE`; `PROJECT_STATUS.md:4158-4163` logcat drops whole suites | **CONCEDE** | G-01 Impact split: history = E2 arm64 asm + E1 host; lens = E1 host emulation, arm64 E5. H-G-01 → H-G-01a / H-G-01b. Gate P1 cannot refute (4158-4163), replacing the weaker shallow-repo argument. Impact restated as evidence pollution, product unaffected; P2 kept. PORT-031 evidence cell split | RESOLVED |
| 2 | G-02 confirmed | — | Agreed | none | RESOLVED |
| 3 | G-03 confirmed | — | Agreed | none | RESOLVED |
| 4 | G-04 confirmed, no verdict flips | — | Agreed | none | RESOLVED |
| 5 | G-05 / PORT-028: drop gizmo; 24 px requirement exists; real mismatch Freeform vs sketch | `gizmo.h:225-231` hit-corridor comment; `freeform_session.h:43-45` "the sketch's own tap slop"; `.cpp:640` `× gizmoPixelsPerReferenceUnit()`; `sketch_session.h:208-210` `kSketchTapSlopPixels = 24.0f` | **CONCEDE** | G-05 retitled DOC-DIVERGENCE P3 (E2), gizmo removed, CLAUDE.md `CAD-V6-S2-CORRECTION-FILL-PICK-R2` cited as H1 source; desktop-DPI part FUTURE-NOT-DUE. PORT-028 narrowed and regraded; owner decision 4 rewritten | RESOLVED |
| 6 | G-06 / PORT-010: requirement invented; overlap with J-03 | `DATA_PACKAGE_SPEC.md:42`, `:1563-1566` as quoted by J; SEAT_J J-03 (`project_state.cpp:753`) | **CONCEDE** | PORT-010 source = "derived (seat G)", H2, FUTURE-NOT-DUE, no gap; cross-ref J-03; owner decision 3 merged with OD-J3 | RESOLVED |
| 7 | PORT-048 PASS contradicts J-04 and source | `ProjectSlot.java:110-114` `slot.delete() && pending.renameTo(slot)`; `ProjectCheckpoint.java:107-122` same; `read` (:130-150) reads the slot only; grep: no reader of `*.pending` | **CONCEDE** — my "a failed write keeps the previous valid file" was wrong for the delete-then-retry path | PORT-048 → PARTIAL, P3, E2, owning finding SEAT_J J-04; expected_behaviour "keeps a READABLE valid file"; evidence "recorded" removed | RESOLVED |
| 8 | PORT-048/-050 cite "recorded" with no run id | SEAT_J:126-129, :377: `FULL_SHARDED_SUITE_PASS` `36607747079` on `fa0b6b4` (`ProjectAutosaveRecoveryTest` FSR1B-01..09). `git diff --stat fa0b6b4 70a6c14`: the three Java persistence files are identical; `forgeshape_project_state.cpp` changed (+231/−14) | **CONCEDE** (named the run) | PORT-050 names `36607747079` @`fa0b6b4`, grade "E2 + E3 (older tree)"; Java scheduling unchanged since that run, native fingerprint changed so that part is E2 only; HJ-1 cross-referenced (statement unchanged). PORT-048 handled in #7 | RESOLVED |
| 9 | PORT-003/005/011/013/053/054/069 PASS-REPRODUCED on E2 | grep-only evidence, no runtime | **CONCEDE** | all seven → IMPLEMENTED-UNTESTED, E2 | RESOLVED |
| 10 | PORT-072 can be E1 | My own `readelf -lW` on the fresh release APK: every LOAD 0x4000 on arm64-v8a and x86_64 (reproduces J) | **CONCEDE** | PORT-072 → PASS-REPRODUCED, E1 for alignment; 16 KB runtime OWNER-PHYSICAL-UNVERIFIED; positive-claims row added | RESOLVED |
| 11 | "byte-deterministic across STLs" overstated | `COMMANDS.txt` #2 is `-fsyntax-only` for libc++; #7/#8/#11 libstdc++ | **CONCEDE** | positive claim narrowed to g++13 / clang18 with libstdc++ (E1); STL claim E5 (no libc++ encode, no named device digest run cited) | RESOLVED |
| 12 | Resolve `guard_exit=2` | `run_status.txt` 00:54:48; `release_guard.txt` mtime 00:57:29 > `gradle_unit_rerun.txt` 00:57:27; `app-debug.apk` mtime 2026-10-28 21:20:06 | **CONCEDE** | positive claim states the PASS is from the rerun, and that the debug rows read the 21:20 debug APK; open question marked RESOLVED | RESOLVED |
| 13 | Horizons: PORT-031 → H0; PORT-032 → H2 no gap; PORT-056 → H2; PORT-067 E3 vs E1 | CLAUDE.md "Commands"; `PROJECT_STATUS.md:2624`; no requirement found for PORT-056; `TEST_COVERAGE_MATRIX.md:19` grades CI FAST `40796992679` E1 | **CONCEDE** | PORT-031 H0; PORT-032 H2, BLOCKED-OWNER, severity "-"; PORT-056 H2, FUTURE-NOT-DUE, severity "-" (hypothesis H-G-02 kept); PORT-067 E1 | RESOLVED |
| 14 | G-07/08/09, H-G-03, H-G-05 no objection; link H-G-03 to HJ-2 | — | Agreed | H-G-03 now names SEAT_J HJ-2 | RESOLVED |

Tally: 14 objections, 14 conceded (4 of them agreements with no change), 0 rebutted, 0 UNRESOLVED.
