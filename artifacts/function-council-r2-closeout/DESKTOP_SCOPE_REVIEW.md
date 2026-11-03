# FUNCTION-COUNCIL-R2-CLOSEOUT-R1: Desktop scope review (proposal only, H2)

Audit only. This review changes no product file. It re-checks the host claims behind
`artifacts/function-council-r2/PORTABILITY_MATRIX.md` and `DESKTOP_PORT_STRATEGIES.md`
(seat G) against the current tree. `app/src/main` is at `f3db953` (product `70a6c14`).
All commands, exit codes and raw outputs are in `diagnostics/desktop/` (`CMD.txt`,
`probe.out`, `boundary.out`, `headless.out`, `results/`). Binaries were built only in
`/tmp/claude-0/fc2c-scratch/desktop/` and are not committed.

**Horizon: still H2. This is a proposal, not an authorisation.** `ARCHITECTURE.md:195-201`
says no Windows client exists and none is authorised. The OWNER source archive puts
`PORT-R0` (cross-platform spike) at **DEFERRED** ("Trigger: first Apple/desktop client is
actually scheduled … Do not prebuild hypothetical ports",
`sources/FORGESHAPE_MASTER_PLAN_v4.1.0.md:93`). The Council R2 addendum asks for a matrix
and adapter variants "**without** implementation, kernel replacement or a new runtime
licence" (`sources/FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.66.md:23`, translated from Polish).
The archive is historical and partial. The canonical v4.1.61 plan is absent and nothing
here is reconstructed from it.

**Windows evidence: none.** This environment has no MSVC, no `clang-cl` on PATH, no
Windows SDK or UCRT, no MSVC STL headers and no mingw-w64 sysroot. Running
`echo '#include <vector>' | clang++ --target=x86_64-pc-windows-msvc -fsyntax-only -x c++ -`
(`diagnostics/desktop/CMD.txt` §4) stops at once. Re-run during the repair of this review,
the output was:

```
<stdin>:1:10: fatal error: 'vector' file not found
    1 | #include <vector>
      |          ^~~~~~~~
1 error generated.
rc=1
```

That output is quoted here only; no `.out` file under `diagnostics/desktop/` records it,
because this repair was limited to this one file. Every Windows statement below is therefore **BLOCKED** or E5. **No sentence
in this review, or anything derived from it, may call a desktop port "easy", "cheap" or
"mostly done".** All host evidence is Linux x86_64. None of it is Windows evidence, and the
emulator is not physical-device evidence.

## 1. Re-verification of the seat-G host claims

Confidence key: E1 reproduced here with output, E2 source `file:line`, E3 historic run,
E4 doc only, E5 inference.

| # | Claim (seat G) | Closeout result | Class | Conf. |
| --- | --- | --- | --- | --- |
| P1 | The 98 platform-neutral TUs (the `host-native-selftests.sh` rule: `forgeshape_*.cpp` minus `jni`/`renderer`) compile with `-std=c++17 -Wall -Wextra -Wpedantic` | **g++ 13.3: 98/98, 0 errors, 26 warnings** (`probe.out` `P1a`). `probe.out`'s category tally names 25 of them (24 `missing-field-initializers`, 1 `unused-but-set-variable`): its `grep -o '\[-W[a-z-]*\]'` cannot match a flag containing digits. The 26th is in the uncommitted scratch log `/tmp/claude-0/fc2c-scratch/desktop/logs/gxx_forgeshape_cad_feature_selftest.cpp.log`, read during this repair: `forgeshape_cad_feature_selftest.cpp:3996:20: warning: identifier 'requires' is a keyword in C++20 [-Wc++20-compat]` (source line 3996 is `const auto requires = [](const CadSketch& sketch) {`). No committed `.out` file holds the raw g++ warning lines; `results/p1_gxx.txt` holds per-TU counts only (warnings in `cad_feature_selftest` 1, `demo_mesh` 8, `picking_selftest` 9, `render_mesh_selftest` 7, `scene_selftest` 1). **clang 18 + libstdc++: 98/98, 0 errors, 82 warnings** (44 `unused-variable`, 24 `missing-field-initializers`, 10 `sometimes-uninitialized`, 3 `unused-function`, 1 `unused-but-set-variable`). Seat G ran clang only, so the g++ row is new | REPRODUCED | E1 |
| P1b | The 10 `-Wsometimes-uninitialized` warnings are in self-tests only | All 10 are in `forgeshape_cad_feature_selftest.cpp` and `forgeshape_sketch_ux_selftest.cpp` (`results/clang_sometimes_uninitialized.txt`) | REPRODUCED | E1 |
| P2 | The same 98 TUs compile against the NDK's libc++ | clang 18, `-nostdinc++ -isystem $NDK/.../c++/v1`: **98/98, 0 errors, 82 warnings** | REPRODUCED | E1 |
| P3 | Header self-containment is 100 of 101 | **g++: 100/101. clang: 100/101.** The one failure in both is `forgeshape_renderer.h` (`native_window.h` not found) | REPRODUCED | E1 |
| P4 | `forgeshape_renderer.cpp` compiles on Linux with 5 stubbed Android declarations plus the NDK Khronos headers | **g++: rc=0, 0 warnings, 0 errors. clang: rc=0, 0 warnings, 0 errors.** Control: without the stubs, clang stops at `native_window.h' file not found` (rc=1) | REPRODUCED | E1 |
| P4c | (new control) The JNI TU is not neutral | With the same stubs, `forgeshape_jni.cpp` stops at `native_window_jni.h' file not found` (rc=1, 1 error) | REPRODUCED | E1 |
| P5 | 10 shaders pass `glslc --target-env=vulkan1.0` and `spirv-val` | **10/10** | REPRODUCED | E1 |
| P6 | The product CMake configure on Linux fails only on the three Android libraries | `ANDROID_LIB`, `LOG_LIB` and `VULKAN_LIB` are NOTFOUND. With glslc on PATH it was found (`results/p6_cmake_excerpt.txt`) | REPRODUCED | E1 |
| P7 | Headless shell: load, capture and re-encode is byte-identical in a fresh process; centre pick hits 6 of 7; a 100 px Mouse drag gives −0.5000 rad | One process per fixture: **7/7 `load=Ok`, 7/7 `reencode=identical`, `centre_hit=1` on 6/7 (`cad_rectangle_v1` = 0), all `mouse_100px_yaw_delta_rad=-0.5000`, `truncated_v1` `decode=Truncated`** | REPRODUCED | E1 |
| P8 | Several loads in one process change the re-encoded bytes (process-scoped `ObjectId` allocator) | One process, 8 fixtures: the last 4 are `reencode=different`, and their **fingerprints also differ** from the fresh-process values (e.g. `freeform_box_v1` `4fe78c11c803d614` fresh vs `252d6b1d7b45476e` sequential). `seq_reencode mixed_cad_v1 → freeform_box_v1`: `final reencode=different`. The byte offset was not re-dumped here; seat G recorded SCNE `nextObjectId` 2→4 at offset 56 (E3). The fingerprint difference is a new observation | REPRODUCED (bytes); offset E3 | E1/E3 |
| P9/P10 | FMA contraction makes 2 host checks fail and arm64 emits `fmadd` | Not re-run by this seat, but reproduced in this closeout as `CR-07-S2`..`S4` (`CRITICAL_REPRO_MATRIX.tsv`, `diagnostics/CR-07/`). `host-runs/rcs.txt`: `clang-fma-on rc=1`, `clang-fma-fast rc=1`, `gcc-O2-fma-fast rc=1`, `clang-fma-on-O0 rc=1`; `clang-fma-off`, `gcc-fma-fast`, `clang-nofma` rc=0. Each rc=1 run prints `FAIL CONSTRUCTION_HISTORY: eviction_is_deterministic_oldest_first` and `FAIL CAD_FEATURE: CADFC2_PF_03b_BEFORE_the_lens_taps_to_a_whole_circle_by_anchor_order`. `contract_probe.out`: `probe_aarch64-O0.s:  fmadd d0, d0, d1, d2;` (NDK arm64 fuses even at -O0). `CMakeLists.txt` and `app/build.gradle` still contain no `-ffp-contract` or `-mfma` (grep, 0 hits) | REPRODUCED (host) | E1 host failures and arm64 `fmadd` assembly; arm64 device outcome E2 for `S2` (asm) and E5 for `S3`, as the matrix states; physical device unverified |
| G-02 | No workflow runs `host-native-selftests.sh` | `grep -rn host-native-selftests .github/` returns 0 hits. All three workflows use `ubuntu-24.04` | SOURCE-CONFIRMED | E2 |

**Correction to the matrix wording.** "275 JNI exports" holds per build variant. The source
contains **276** `JNIEXPORT` definitions because `debugInjectDeviceLoss` is defined twice:
one real body and one stub returning `JNI_FALSE`, in the two branches of a preprocessor
conditional at `forgeshape_jni.cpp:9074` and `:9083`. That leaves **275 distinct names**,
equal to the **275 `static native`** declarations in `NativeViewport.java`. The two name
sets are identical (`boundary.out` B1f `yes`). There are no `Java_` exports outside
`NativeViewport` (0).

## 2. Platform boundaries (verified counts, `boundary.out`)

| Boundary | Fact | Conf. |
| --- | --- | --- |
| JNI surface | 275 exports in ONE class (`NativeViewport`). `NativeViewport.java` is the only Java file that declares `native`. **0** upcalls (`FindClass`, `GetMethodID`, `Call*Method`, `RegisterNatives`, `AttachCurrentThread`): the binding is pull-only | E1 |
| Android headers in C++ | `<android/…>` appears in exactly 3 files: `forgeshape_jni.cpp`, `forgeshape_renderer.cpp`, `forgeshape_renderer.h`. `<jni.h>` appears only in `forgeshape_jni.cpp` | E1 |
| Renderer Android-only Vulkan | `VK_USE_PLATFORM_ANDROID_KHR` (`renderer.h:11-12`), `VK_KHR_ANDROID_SURFACE_EXTENSION_NAME` (`renderer.cpp:262`), `VkAndroidSurfaceCreateInfoKHR`, `VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR` and `vkCreateAndroidSurfaceKHR` (`:442-446`). The renderer requests no other extensions beyond `VK_KHR_surface` (`:261`) and `VK_KHR_swapchain` (`:552`, `:599`) | E1/E2 |
| `ANativeWindow` | Mentions: renderer.h 5, renderer.cpp 10, jni.cpp 11. Renderer calls: `acquire` `:372`, `getWidth`/`getHeight` `:448`, `:2105-2106` (the extent fallback when `currentExtent == UINT32_MAX`), `:2211`, `release` `:500`. JNI calls: `fromSurface` `:9405`, `getWidth`/`getHeight` `:9412`, `:9417`, `release` `:9430`, `:9482` | E1 |
| Log sink | `__android_log_print` / `FS_LOG*`: 362 sites in jni.cpp, 43 in renderer.cpp, 0 elsewhere | E1 |
| Renderer → process singletons | 0 calls to `constructionScene()`, `sketchSession()` or the others. The renderer reads only the snapshots it is handed | E1 |
| Input | `translateAction` (`jni.cpp:404-413`) drops hover, scroll and button events (`default: return false`). `PointerToolType::Mouse = 4` exists (`input.h:55`). Pixel constants: `kOrbitRadiansPerPixel 0.005` (`camera.h:101`), `kPinchLogPerPixel 0.0035` (`:110`), `kTapSlopPixels 24` (`selection.h:37`) | E2 |
| Java | 85 files. 63 import `android.*`. 22 import nothing from Android (`AppPreferences`, `AppTheme`, `CadHud*Presentation`, `ExactExpression`, `LengthUnit`, `SketchChromePolicy`, `ToolbarRowBudget` …). 33,208 lines in total | E1 |
| Manifest | `android.hardware.vulkan.version` required. `configChanges="orientation\|screenSize\|smallestScreenSize\|keyboardHidden\|screenLayout\|density"`: no `keyboard`, `uiMode` or `navigation` | E2 |

**Wording drift (repeats seat G's G-03, still open).** `ARCHITECTURE.md:214` says
"`Renderer` never creates or releases" an `ANativeWindow`. The renderer acquires at `:372`
and releases at `:500`. It creates none. The "*what would break*" test (`ARCHITECTURE.md:216-218`)
also fails for `forgeshape_renderer.h` (P3). Both are documentation accuracy issues for the
OWNER, not defects in the H0 product. Conf. E1/E2.

**MSVC-sensitive source grep (indicative only, not a compile).** Over the neutral product
and self-test sources there are 0 files using `__builtin_`, `__attribute__`, `M_PI`,
`#pragma GCC`, `__int128`, `typeof(`, `alloca(`, `__restrict`, `ssize_t`, `<unistd.h>`,
`<pthread.h>` or `<sys/…>`. There are 0 designated initializers. 100 of the 100 neutral
headers use `#pragma once`. `long` appears as a type only in two **self-tests**
(`cad_revolve_selftest.cpp:117-129`, `gltf_import_selftest.cpp:125-126`). On LLP64 Windows
`long` is 32-bit, which looks harmless for the values involved (E5). **A clean grep is not
compile evidence**: MSVC compatibility of the neutral domain and of vendored Manifold 3.5.4
is **BLOCKED** (E0 here).

## 3. Variants: cost drivers, adapters, the test that would prove each, licence risk

None of the three is recommended or ranked as cheap. Sizes are measured facts. Effort is
deliberately not estimated, because no Windows compile, link or run exists.

### V1: Native C++ shell per OS (Win32; Wayland and/or X11)

- **Cost drivers.** The whole widget, chrome and accessibility layer is rewritten. The
  Android chrome is about 29k lines in 63 files, plus 22 pure-Java policy files (3,854
  lines per seat G) that would have to be re-expressed in C++ and kept in step. Linux needs
  two windowing backends unless one is chosen. IME (TSF/IMM, text-input-v3) and UI
  Automation / AT-SPI share nothing with Android.
- **Adapters needed.**
  1. Renderer surface seam: a `VkSurfaceKHR` plus extent plus instance-extension list
     handed in, replacing the 5 Android symbols and the `ANativeWindow` extent fallback
     (`:2105`), which Wayland's `0xFFFFFFFF` extent would hit.
  2. A log sink that keeps `*_SELFTEST_OK` tokens byte-identical.
  3. Extraction of the gesture arbitration (`touchEvent`, `jni.cpp:9503-…`) and the
     load/close orchestration out of the JNI TU into a neutral application core, so that
     two shells do not drift.
  4. A wheel/button/modifier/hover input vocabulary.
  5. Per-window density instead of the one process-global density atomic.
  6. Atomic file replace per OS (`ReplaceFileW` or `MoveFileExW`; POSIX `rename` +
     directory `fsync`).
- **Compile test that would prove it.** A CMake target that builds the 63 product TUs plus
  Manifold as a static library with **MSVC `/std:c++17 /W4 /permissive-`** and with
  clang-cl. A minimal Win32 executable that creates `VK_KHR_win32_surface` and runs the
  renderer's existing `initialize/drawFrame` path on a clear colour. CTest running the 25
  suites on Windows with the 4392-check count and 0 failures. Then the 72-fixture corpus
  verdicts byte-identical to Linux. Today every one of these steps is **BLOCKED**.
- **Licence risk.** No new library. OS SDKs and the Khronos loader/headers (Apache-2.0)
  only. Lowest licence risk and highest ownership cost.

### V2: JVM desktop shell over the existing JNI API

- **Cost drivers.** About 63 Android View files rewritten in Swing (or JavaFX, which is a
  separate dependency and needs its own gate). Two UI toolkits must be kept visually aligned.
  Swing's hi-DPI and Wayland behaviour is uneven (E5). A `jlink` runtime image ships per OS.
- **Adapters needed.**
  1. A desktop variant of `surfaceCreated(Surface)` taking a native handle via JAWT (JDK).
     The JNI TU loses its `android/*` includes (`native_window_jni.h`, `log.h`) behind a
     seam.
  2. The same renderer surface seam, log sink and input vocabulary as V1.
  3. The 22 pure-Java policy classes are reused directly. Of the 275 JNI functions, all but
     the surface entry are reusable **by signature**. Whether they are reusable **by
     behaviour** depends on extracting the JNI-resident policies (V1 adapter 3).
- **Compile test that would prove it.** Compile `forgeshape_jni.cpp` against desktop
  `jni.h` with the Android includes behind a seam, as a `.so` on Linux and a `.dll` with
  MSVC. A JUnit desktop smoke that calls `NativeViewport.start()` headless and round-trips
  `encodeProject`/`loadProject` bytes for the corpus. Today it is BLOCKED by P4c
  (`native_window_jni.h`) on Linux and by the absence of MSVC on Windows.
- **Licence risk.** OpenJDK runtime redistribution (GPLv2 + Classpath Exception) inside
  `jlink` images. JavaFX would be an additional gated library. Medium risk, and the OWNER
  must review it.

### V3: UI-library-assisted shell (candidate only, needs an OWNER licence and architecture gate)

- **Candidates (named, not chosen).** SDL3 or GLFW for window, input and Vulkan surface
  (both zlib), with Dear ImGui (MIT) or Qt 6 (LGPLv3 or commercial) for chrome. Each
  conflicts with the CLAUDE.md hard rule "No third-party runtime, rendering, math or input
  library" (only Manifold is approved; "Any second third-party library needs its own gate"),
  with ARCH-OWNER-01's ban on a cross-platform UI framework, and with the v4.1.0 line "do not
  preselect … shared UI framework now" (`sources/…v4.1.0.md:185`). GLM remains prohibited.
- **Cost drivers.** Governance first: vendoring, pinning and a `--verify` script modelled on
  `scripts/vendor-manifold.sh`. Then an immediate-mode or Qt chrome that diverges from
  Android's look and accessibility (ImGui has no platform accessibility tree, E5). The
  gesture-semantics extraction is still needed (V1 adapter 3).
- **Adapters needed.** The renderer surface seam (the library supplies the `VkSurfaceKHR`),
  the log sink, the input mapping into `TouchPointer` plus the future wheel/button sample,
  and the neutral application core.
- **Compile test that would prove it.** A pinned, hash-verified vendored copy. A CMake build
  of library + domain + renderer on MSVC and GCC/Clang. One window drawing the renderer's
  clear frame, then a corpus round-trip through the shell's open/save path.
- **Licence risk.** zlib/MIT are permissive but still each need a gate. Qt LGPL brings
  dynamic-linking and relinking obligations, or a commercial licence. Highest governance
  risk.

### Not a desktop port: the existing APK in Android desktop or windowed mode

This reuses everything, but mouse input is currently orbit-only. Pan and zoom need two
pointers, and wheel and button events are dropped at `translateAction` (`jni.cpp:404-413`).
`configChanges` lacks `keyboard`. This is the same input-vocabulary work as V1 adapter 4, on
the one production platform. Conf. E2. Behaviour on ChromeOS or desktop windowing is
UNVERIFIED (E0).

## 4. Shared prerequisites, whichever variant is chosen (proposal)

1. **Guard what is already true (H0/H1, no port needed).** Run
   `scripts/host-native-selftests.sh` in `CI FAST`, adding P1 (g++ and clang) and P3. The
   neutral-domain property this review reproduced is guarded by no workflow today (G-02,
   E2).
2. **An FP-contraction policy before any third ABI** (seat G P9/P10; reproduced on host as `CR-07-S2`..`S4`, E1; flag absence E2; arm64 device outcome E2/E5).
3. **A Windows compile spike as the first evidence step**, and only after `PORT-R0` is
   triggered: MSVC + clang-cl on the 63 product TUs + Manifold. Until it exists, every cost
   statement stays E5.
4. Neutral application-core extraction and a per-document context (the singletons, P8) are
   architecture changes. They need an OWNER decision (`OWNER-SEMANTICS-REQUIRED`) and are
   not proposed for any current stage.

## 5. Method notes (METHOD only)

These are taken as **method** from the SolidCraft reference. No code, UX, kernel or Rust
was copied, and none is proposed.
- **Oracles before ports.** Make the cross-platform contract executable: the 72-fixture
  corpus verdicts, golden digests and the 25-suite check count are the oracle a desktop
  build must reproduce byte-for-byte before any UI work counts.
- **Fail-closed semantics are the security boundary.** On desktop, users open arbitrary
  files more often. The decoders' refuse-by-name behaviour is what keeps that safe, so a
  shell must never "repair" a refusal.
- **Command routing in one place.** Every user act should route through one neutral command
  layer (today partly inside `forgeshape_jni.cpp`), so two shells cannot implement the same
  act two ways.
- **Persistence tests at the byte level.** Save → reopen → re-encode identity, including
  the multi-load-per-process case (P8), is the test that tells whether a multi-window desktop
  shell is safe.

## 6. Verdict

| Item | Classification |
| --- | --- |
| Neutral-domain compile on Linux (g++, clang, libstdc++, libc++), header containment, renderer-with-stubs, shaders, CMake failure points, headless reuse | REPRODUCED (E1) |
| 275 JNI exports = 275 Java natives (276 source definitions, one preprocessor pair) | REPRODUCED (E1), with the wording corrected |
| FMA hazard | REPRODUCED on host (E1, `CR-07-S2`..`S4`); arm64 device outcome E2 (`S2`, asm) / E5 (`S3`), physical unverified |
| Any Windows / MSVC / clang-cl compile, link or run | **BLOCKED (E0)**: no toolchain, SDK or headers |
| Choice of V1/V2/V3, a new library, the application-core extraction, multi-document context | OWNER-SEMANTICS-REQUIRED (H2, `PORT-R0` DEFERRED) |
| Port effort or ease | Not stated. No evidence supports calling any variant easy |
