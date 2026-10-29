# FUNCTION-COUNCIL-R2 — Portability matrix (Seat G, desktop readiness, design only)

Tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` = `f3db953`, unchanged at the
audit HEAD). Research only: no product, test, script, Gradle, workflow or owner-doc change.
Every probe ran in `/tmp/claude-0/fc2-scratch/G/` and is never built into the product; exact
commands are in `/tmp/claude-0/fc2-scratch/G/COMMANDS.txt`.

**Horizon.** A Windows or Linux desktop client is **H2 (future, not authorized)**:
`ARCHITECTURE.md:195-201` says there is no Apple, Windows, web or cloud client and none is
authorized; `PROJECT_STATUS.md` *Owner Decision Baseline* `ARCH-OWNER-01` makes portability a
*constraint on arrangement* (domain platform-neutral, Android an adapter, input semantic,
platform services narrow, renderer surface coupling explicit) and forbids a cross-platform UI
framework. So the ARCH-OWNER-01 constraints are **H0/H1 requirements checked below**; a desktop
port itself is **FUTURE-NOT-DUE** and nothing absent from it is a defect.

Evidence grades: E1 reproduced in this audit, E2 source, E3 recorded run, E4 doc claim, E5
inference.

## 1. Portability classes

| Class | Meaning |
| --- | --- |
| **N — Neutral** | Compiles and runs off-Android today with no change (proved by a host compile/link/run in this audit). |
| **N\* — Neutral with a platform assumption** | Compiles off-Android, but carries a numeric or lifecycle assumption that a desktop shell would have to supply or change (pixel units, one process = one document, FP model). |
| **S — Seam** | Platform-bound but the coupling is small, named and local; a second backend would sit *beside* it. |
| **A — Android-bound adapter** | Android API throughout; would be rewritten, not ported, by a non-Android shell. |
| **T — Toolchain/infra** | Build, CI, packaging. |

## 2. Module × class × evidence

Line counts are `wc -l` at this tree (`.cpp` + `.h`).

| Module (files) | Lines | Class | Platform facts found | Evidence |
| --- | ---: | --- | --- | --- |
| Math (`forgeshape_math.h`) | 333 | N | header-only, no intrinsics | E1: standalone header compile (101-header probe, 100 pass) |
| Camera (`forgeshape_camera.{h,cpp}`) | 834 | N\* | orbit `kOrbitRadiansPerPixel = 0.005` (`camera.h:101`) and pinch `kPinchLogPerPixel = 0.0035` (`camera.h:110`) are per **physical pixel**; pan+zoom need TWO pointers (`camera.cpp:289-311`) | E1: headless probe, a 100 px Mouse drag → yaw −0.5000 rad at 1920×1080 regardless of density; E2 |
| Input data (`forgeshape_input.{h,cpp}`) | 264 | N | `TouchAction`, `PointerToolType` incl. `Mouse = 4`, pressure, tilt, view-local px; **no hover, wheel, button or modifier fields** (`input.h:27-34, 50-56, 145-155`) | E1 (probe feeds `PointerToolType::Mouse`); E3 host `PICKING` `pointer_tool_code_*`; JVM `PointerSemanticsTest` |
| Picking / selection (`forgeshape_picking`, `forgeshape_selection`) | 856 | N\* | `kTapSlopPixels = 24` physical px (`selection.h:37`); FP-contraction-sensitive barycentric epsilon is a recorded ARM64 hazard (`PROJECT_STATUS.md:2438-2449`) | E1: `pickScene` hit at 960,540 on 6 of 7 fixtures in the headless probe |
| Construction / transform / body commands / dimensions / mirror / delete | ~6,000 | N | pure value functions | E1: host suites CONSTRUCTION_*, BODY_DIMENSIONS, MIRROR pass (fresh fc2 host run 4392/0) |
| Scene, history, mesh store (`forgeshape_scene`, `_history`, `_mesh`) | 2,603 | N\* | composition root as **function-local statics**: `constructionScene()` `scene.cpp:406`, `meshStore()` `:604`, `sculptSession()` `:638`, `constructionHistory()` `history.cpp:448`, `sketchSession()` `sketch_session.cpp:4323`, `gizmoSession()` `gizmo.cpp:1843`, `freeformEditSession()` `freeform_session.cpp:827`, `displaySettings()` `display.cpp:269`; the classes themselves take scene/history **by reference** (injectable) | E2; E1 probe (process-scoped allocator carried across loads, §5) |
| ObjectId allocator | — | N\* | forward-only across loads in one process (`project_state.cpp:410-415`), so re-encoded bytes depend on earlier activity in the process | E1: `seq_reencode` (SCNE `nextObjectId` 2 → 4) |
| Sculpt (+ history, mask) | 3,443 | N | — | E1 host SCULPT_BRUSH_KERNEL |
| Sketch, region, arrangement, drafting, snap, overlay, session | ~25,000 | N\* | tap slop `kSketchTapSlopPixels = 24` physical px (`sketch_session.h:210`); hit tolerance via `gizmoPixelsPerReferenceUnit()` (`sketch_session.cpp:3248`) — two unit systems in one module | E1 host SKETCH_UX/CAD*; E2 |
| CAD body/feature/face/revolve/timeline/inventory/extrude tool | ~15,000 | N\* | sizes in reference units × one process-global density | E1 host CAD, CAD_A3, CAD_FEATURE |
| CAD kernel boundary (`forgeshape_cad_kernel.cpp` + Manifold 3.5.4) | 22,380 (vendored) | N | only `forgeshape_cad_kernel.cpp` includes a Manifold header (grep); `MANIFOLD_PAR=-1`, no iostream/filesystem | E1: kernel built with g++ 13 and clang 18 (-O2, -mfma) on host |
| Freeform, Surface | ~9,000 | N | — | E1 host FREEFORM/SURFACE; their 6 golden digests equal the committed fixture SHA-256s |
| Codec (`forgeshape_project_document`, `_bytes`, `_state`, `_freeform`, `_surface`) | ~10,000 | N | bytes in, bytes out; no filesystem; fixed-width LE | E1: 72/72 corpus verdicts on host (fc2 `host_corpus_verdicts.txt`); probe load→capture→encode byte-identical for 7 fixtures in a fresh process |
| GLB export/import + JSON | ~5,000 | N | no `Uri`, path or stream | E1 host GLTF_EXPORT / GLTF_IMPORT |
| Display settings, gizmo, grid, matcap, selection outline/pulse, render mesh, render recovery | ~5,400 | N\* | gizmo density is ONE process-global `std::atomic<float>` (`gizmo.cpp:15`); outline width in px of the short side, clamped [2, 5] px | E1 host GIZMO, RENDER_SHADING, RENDER_RECOVERY |
| Self-test suites (70 files) | 51,151 | N | 10 `-Wsometimes-uninitialized` in two suites (§6) | E1 host run; E1 clang pedantic |
| **Renderer** (`forgeshape_renderer.{h,cpp}`) | 4,574 | **S** | Android touch-points: `#define VK_USE_PLATFORM_ANDROID_KHR` + `<android/native_window.h>` (`renderer.h:9-16`); `<android/log.h>` + `FS_LOGI/E` (`renderer.cpp:3, 46-50, 3871`); instance extension `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME` (`:262`); `vkCreateAndroidSurfaceKHR` (`:443-446`); `ANativeWindow_acquire/release/getWidth/getHeight` (`:372, 448, 499-501, 2105-2106, 2211`). Everything else is core Vulkan 1.0/1.1, FIFO present, identity pre-transform | **E1: `forgeshape_renderer.cpp` compiles on Linux with 5 stubbed declarations and the NDK's Khronos Vulkan headers, 0 errors, 0 warnings** |
| **JNI binding** (`forgeshape_jni.cpp`) | 10,337 | **A** (with domain policy inside) | 275 `Java_com_forgeshape_app_NativeViewport_*` exports = 275 `static native` declarations in ONE class; **no** `FindClass`/`GetMethodID`/callbacks (pull-only); globals: `g_viewport` (render thread, `:151-176`), `g_camera`, `g_selection`, `g_stateMutex` (250 lock sites), gesture state `g_grabbing`, `g_strokePending`, chooser tap state (`:228-307`); **`touchEvent` (`:9503-10319`, ~816 lines) is the gesture arbitration** between sketch, chooser, gizmo, freeform, sculpt and camera; `loadProject` (`:8440-8480`) cancels the sketch and gestures before `loadProjectDocument` | E2 |
| Shaders (`shaders/*`, 10 files) | 644 | N | GLSL, `--target-env=vulkan1.0`, push constants only, no `#extension` | E1: 10/10 `glslc` + `spirv-val` |
| Java shell — Android-bound (63 files) | 29,354 | A | Views, `SurfaceView`, `MotionEvent`, SAF (`ProjectTransfer.java:96-155`), `SharedPreferences`, `getFilesDir()` + fsync + rename (`ProjectSlot.java:81-123`), IME (60 sites), accessibility (80 sites), `WindowInsets` | E2 |
| Java shell — pure Java policy (22 files) | 3,854 | N for a JVM shell, **duplicate for a C++ shell** | `ExactExpression`, `LengthUnit`, `CadHudPresentation`, `CadHud3dPresentation`, `SketchChromePolicy`, `ProjectDrawerPolicy`, `ToolbarRowBudget`, `WorkspaceLayoutMode`, `AppPreferences`, `DiagnosticLog` … | E2; JVM tests (20 classes) — fresh fc2 Gradle run `BUILD SUCCESSFUL` |
| Resources | — | A | 801 strings (one locale), 446 ids, 101 VectorDrawables (70 `ic_*`), 130 colours, 5 palettes via `attrs.xml` | E2 |
| Manifest | 41 | A | `android.hardware.vulkan.version 0x400003 required`; no `uses-permission` (no network); no explicit touchscreen `uses-feature`; `configChanges` lacks `keyboard`/`uiMode`/`navigation` | E2 |
| Build: CMake | 205 | T/S | glslc found by NDK hints **or PATH**; links `android`, `log`, `vulkan` | E1: host configure fails only on `ANDROID_LIB`, `LOG_LIB`, `VULKAN_LIB` NOTFOUND |
| Build: host script (`scripts/host-native-selftests.sh`) | 167 | T | the only off-Android build; hand-rolled, not CMake | E1 (fresh run 4392/0) |
| CI (`ci-fast.yml`, `ci-device.yml`, `ci-full-sharded.yml`) | — | T | Linux runners; **no workflow runs `host-native-selftests.sh`**; device CI is x86_64 emulator only | E2 (grep: no reference in `.github/`) |

## 3. Dependency graph

```mermaid
flowchart TD
  subgraph AndroidShell["Android shell (Java, A)"]
    Act[ForgeShapeActivity] --> EWV[EditorWorkspaceView 6,655 lines]
    EWV --> SV[ForgeShapeSurfaceView<br/>MotionEvent -> neutral codes]
    EWV --> Pure[22 pure-Java policy classes]
    EWV --> Plat[ProjectSlot / ProjectCheckpoint / ProjectTransfer / AppPreferencesStore / AutosaveController]
  end
  SV --> NV[NativeViewport: 275 static natives]
  EWV --> NV
  Plat -->|byte[] only| NV
  NV --> JNI[forgeshape_jni.cpp A<br/>globals, render thread, gesture arbitration]
  JNI --> R[forgeshape_renderer S<br/>ANativeWindow + VK_KHR_android_surface]
  JNI --> Root[process-scoped accessors N*<br/>constructionScene / history / sketchSession / sculptSession / gizmoSession / freeformEditSession / displaySettings]
  Root --> Dom[Domain N: scene, history, CAD, sketch, sculpt, freeform, surface, codec, GLB, picking, camera, input]
  R -->|SceneSnapshot, overlay, camera snapshot| Dom
  Dom --> K[forgeshape_cad_kernel.cpp] --> M[Manifold 3.5.4 static]
  R --> SPV[10 SPIR-V includes from glslc]
```

Direction is strictly downward: the domain includes no renderer, JNI or Android header (100 of
101 headers compile standalone; the one that does not is `forgeshape_renderer.h`), and the
renderer reads only snapshots handed to it (it calls none of the process-scoped accessors —
`grep` count 0 in `forgeshape_renderer.*`).

## 4. Responsibility map (who would own what in a future desktop shell)

| Responsibility | Today (Android) | Shareable now | Must be abstracted later (H2) |
| --- | --- | --- | --- |
| Authored truth, regeneration, validation, history | domain C++ | yes, as-is | a *document context* if more than one document per process is wanted (the singletons) |
| `.forge` bytes | codec C++ | yes | — |
| Where bytes live (slot, checkpoint, transfer) | Java `ProjectSlot`/`ProjectCheckpoint`/`ProjectTransfer` | policy text only | atomic replace per OS (`ReplaceFileW` / `rename` + dir fsync), native file dialogs |
| Autosave debounce + fingerprint gate + quarantine | Java `AutosaveController` (900 ms, `AutosaveController.java:60`) + native fingerprint | native fingerprint | the scheduling policy (re-implement or move below the adapter) |
| Gesture arbitration (which tool owns a pointer) | `forgeshape_jni.cpp` `touchEvent` `:9503-10319` | **no** (it is inside the JNI TU) | extract into a neutral function over `TouchPointer[]` |
| Camera / picking | domain C++ (`g_camera` instance lives in JNI) | yes | mouse wheel / button vocabulary; px→reference-unit policy |
| Pixel density | Java pushes `density` each refresh (`EditorWorkspaceView.java:3014`) into one global atomic | yes | per-window density if several windows/monitors |
| Rendering | `Renderer` + Android surface | everything but the surface | a surface-creation seam (Win32 / Xlib / Wayland surface extension), extent source, log sink |
| Chrome (toolbar, rail, precision surface, HUD, labels) | Android Views + pure-Java policy | the policy classes only with a JVM shell | the whole widget layer |
| Diagnostics tokens | `__android_log_print` in JNI/renderer | the token strings | a log sink |
| Self-tests | JNI debug runner + host script | yes (host script proves it) | a CMake/CTest target |

## 5. Empirical results of this seat (E1)

| # | Probe | Result |
| --- | --- | --- |
| P1 | 98 neutral TUs, `clang++ 18 -std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic`, libstdc++ | 98/98 compile; 0 errors; warnings only `unused-variable` (44), `missing-field-initializers` (24), `sometimes-uninitialized` (10, self-tests only), `unused-function` (3) |
| P2 | Same 98 against the NDK's **libc++** headers on the host | 98/98 compile |
| P3 | 101 headers standalone | 100/101; `forgeshape_renderer.h` fails on `android/native_window.h` |
| P4 | `forgeshape_renderer.cpp` with stubbed `android/native_window.h` (4 functions) + `android/log.h` (1 function) + NDK Vulkan headers | compiles, 0 errors, 0 warnings |
| P5 | 10 shaders: `glslc --target-env=vulkan1.0` + `spirv-val` | 10/10 |
| P6 | Product `CMakeLists.txt` configured on Linux (NDK `glslc` on PATH) | fails only on `ANDROID_LIB`, `LOG_LIB`, `VULKAN_LIB` NOTFOUND |
| P7 | Headless shell probe (product objects only, no self-tests): decode → `loadProjectDocument` into the process singletons → capture → encode; camera at 1920×1080; `pickScene` at centre; Mouse drag | 7 fixtures load `Ok`; re-encode **identical** for each in a fresh process; centre pick hits on 6 of 7; `truncated_v1` refused `Truncated`; 100 px Mouse drag = −0.5000 rad yaw |
| P8 | Load `mixed_cad_v1` then `freeform_box_v1` in one process, re-encode | **different**: SCNE `nextObjectId` 2 → 4 (file offset 56) and the SCNE CRC; by design of `project_state.cpp:410-415` |
| P9 | All 25 suites, clang −O1 with FMA contraction (`-mfma`, default `-ffp-contract=on`) | **2 FAIL**: `CONSTRUCTION_HISTORY: eviction_is_deterministic_oldest_first`, `CAD_FEATURE: CADFC2_PF_03b_BEFORE_the_lens_taps_to_a_whole_circle_by_anchor_order`; controls without FMA and with `-ffp-contract=off` pass 4392/0; an arm64-like build (product −O0, kernel −O2, FMA) fails the same 2 |
| P10 | NDK clang, `--target=aarch64-none-linux-android26`, the Debug flags from `.cxx/Debug/*/arm64-v8a/compile_commands.json` (no `-O`) | `a*b+c` → `fmadd` at −O0 and −O2 on arm64, `mulsd`+`addsd` on x86_64; `forgeshape_history_selftest.cpp` arm64 asm has the loop value `0.5 + 0.01*i` as `fmadd` (`.s:3967`); `fma(0.01,7,0.5) = 0.56999999999999995` ≠ unfused `0.57000000000000006` |

## 6. Leaking assumptions (desktop view)

| Area | Assumption found | Where | Grade |
| --- | --- | --- | --- |
| Units | Thresholds in two unit systems: physical px (`kTapSlopPixels`, `kSketchTapSlopPixels`, `kChooserTapSlopPixels` = 24; orbit/pinch per px) vs reference units × density (`kGizmoHitSlopUnits`, `kFreeformTapSlopUnits` = 24) | `selection.h:37`, `sketch_session.h:210`, `jni.cpp:307`, `camera.h:101,110`, `gizmo.h:231`, `freeform_session.h:45` | E2 + E1 (P7) |
| Pointer | One pointer always orbits; pan and zoom need two pointers; wheel/scroll/secondary-button/hover events are dropped (`translateAction`, `jni.cpp:404-413`); `PointerSemantics.java:41` says Mouse is "carried only: no wheel, hover or context behaviour" | — | E2 |
| Hover | Stylus hover bypasses the input vocabulary: `onHoverEvent` → `supportChooserHover(x, y)` (`ForgeShapeSurfaceView.java:143-153`), no tool type | consistent with `ARCHITECTURE.md:211` ("hover … outside the vocabulary") | E2 |
| Keyboard | No product shortcuts; debug-only keys 1-9, A-G in `ForgeShapeActivity.onKeyDown` (`:402-430`), JNI no-op under `NDEBUG` (`jni.cpp:2267-2271`) | — | E2 |
| High-DPI / multi-monitor | density pushed on every refresh (good), but stored in ONE process-global atomic (`gizmo.cpp:15`) | — | E2 |
| Documents/windows | one document per process (function-local statics) | §2 | E2 |
| Files | internal `getFilesDir()`, fsync then `renameTo`, delete+rename fallback; no directory fsync | `ProjectSlot.java:81-123`, `ProjectCheckpoint.java:89-122` | E2 |
| Uri/path | none crosses JNI — only `byte[]` (`encodeProject`, `loadProject`, `validateProject`, `exportGlb`, `importGlb*`) and one `Surface` | `NativeViewport.java:24, 1186-1414` | E2 |
| Lifecycle | `surfaceDestroyed` blocks until native releases the window; `configChanges` keeps the Activity across rotation/resize/density | `ForgeShapeSurfaceView.java:209-212`, `AndroidManifest.xml:31` | E2 |
| Clipboard / context menus | none in the product | grep 0 hits | E2 |
| Fonts | system `Typeface` only (1 file) | — | E2 |
| Accessibility | content descriptions / node info on 33 files; Android-only API | — | E2 |
| GPU | Vulkan 1.0 core + swapchain; `R8G8B8A8`/`B8G8R8A8_UNORM` preferred else `formats[0]`; `compositeAlpha` OPAQUE else INHERIT; extent from `ANativeWindow` when `currentExtent` is `0xFFFFFFFF` | `renderer.cpp:2098-2200` | E2 |
| FP model | no `-ffp-contract` policy; arm64 contracts, x86_64 does not | `CMakeLists.txt` (no flag), P9/P10 | E1 |
| Network | none (no permission, no socket code) | manifest; grep | E2 |
