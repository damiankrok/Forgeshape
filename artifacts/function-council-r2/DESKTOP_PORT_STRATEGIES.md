# FUNCTION-COUNCIL-R2 — Desktop (Windows / Linux) port strategies (Seat G, design only)

Tree `70a6c14`. **This is not a go-ahead.** A desktop client is H2: `ARCHITECTURE.md:195-201`
states there is no Windows client and none is authorized, and `ARCHITECTURE.md:216-218` forbids
building "a portability abstraction for a platform that has no target". `ARCH-OWNER-01`
(`PROJECT_STATUS.md` *Owner Decision Baseline*) forbids a cross-platform UI framework. Every option
below that adds a library is a **candidate requiring an OWNER licence + architecture gate**
(CLAUDE.md hard rule: "Any second third-party library needs its own gate"). Facts cited here are
measured in `PORTABILITY_MATRIX.md`.

## 1. What can be shared NOW (no code change)

| Asset | Size | Proof |
| --- | ---: | --- |
| Platform-neutral domain, 98 TUs (scene, history, CAD + kernel boundary, sketch, sculpt, freeform, surface, codec, GLB, camera, picking, input data, display policy, render recovery, outline policy) | ~57,800 lines product + 51,200 lines self-tests | E1: compiles with g++ 13 / clang 18, libstdc++ and libc++; host runs 25 suites 4392/0 |
| `.forge` format and corpus | 72 fixtures | E1: host decodes 72/72 with the expected verdicts; FRFM/SURF digests equal the committed files |
| Manifold 3.5.4 kernel build recipe | 22,380 lines vendored | E1: builds with g++ and clang, `MANIFOLD_PAR=-1` |
| SPIR-V shaders | 10 | E1: `glslc` + `spirv-val` |
| Renderer, minus its surface | 4,574 lines | E1: compiles on Linux with 5 stubbed Android declarations |
| Self-test harness | `scripts/host-native-selftests.sh` | E1 |
| Pure-Java policy (only for a JVM shell) | 22 files, 3,854 lines | E2; JVM-tested |

## 2. What must be abstracted LATER (only after an OWNER gate)

1. **Surface seam in the renderer** — the 4 `ANativeWindow_*` calls, `VK_KHR_android_surface`,
   `vkCreateAndroidSurfaceKHR`, and the window-size fallback (`renderer.cpp:262, 372, 443-448,
   499-501, 2105-2106`) become "the platform hands the renderer a `VkSurfaceKHR` + extent + the
   instance extensions it needs". The Android path stays as it is.
2. **Log sink** — `FS_LOGI/E` in `forgeshape_jni.cpp` and `forgeshape_renderer.cpp` (361 log sites
   in JNI). Diagnostic tokens are product evidence (`*_SELFTEST_OK`), so a sink must keep the
   strings byte-identical.
3. **Application core extracted from the JNI TU** — today the composition root and two product
   policies live inside `forgeshape_jni.cpp`: the gesture arbitration (`touchEvent`,
   `:9503-10319`) and the pre-load orchestration (`loadProject` cancels the sketch and pending
   gestures, `:8463-8479`). A second shell would otherwise re-implement both and drift.
4. **Document context** — the process-scoped singletons (`constructionScene()`,
   `constructionHistory()`, `sketchSession()`, `sculptSession()`, `gizmoSession()`,
   `freeformEditSession()`, `meshStore()`, `displaySettings()`, plus `g_camera`,
   `g_selection`) mean one document per process. Desktop multi-window needs a context object;
   the session classes already take scene/history by reference, so this is a wiring change,
   not a redesign. The forward-only `ObjectId` allocator (P8) must become per-document or the
   saved bytes of one window depend on another.
5. **Input vocabulary** — wheel/scroll, buttons, modifiers, hover as a first-class action, and
   ONE unit policy for thresholds (today: physical px vs reference units, §6 of the matrix).
6. **Platform services** — slot/checkpoint atomic replace (Windows `ReplaceFileW` /
   `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; POSIX `rename` + directory
   `fsync`), native open/save dialogs instead of SAF, a preference store applying the same
   forgiving-read rules as `AppPreferences.java`.
7. **FP model** — decide `-ffp-contract` (or tolerant oracles) before a third ABI (Windows/Linux
   x64 under MSVC does not contract by default; Windows-on-ARM / Apple-silicon-class CPUs do).

## 3. Shell options

### S1 — Native per-OS C++ shell, no new library (Win32 on Windows; Wayland/X11 on Linux)

| Aspect | Assessment |
| --- | --- |
| Licensing | none added (OS SDKs; Khronos Vulkan headers/loader are Apache-2.0 and already the platform's) |
| Ownership | ForgeShape owns window, input, DPI, IME, accessibility, menus, dialogs **and every widget** for two OSes (Wayland and X11 are two backends on Linux) |
| Reuse | domain + renderer + shaders + codec 100 %; Java chrome 0 % (29,354 + 3,854 lines re-expressed) |
| Packaging | Windows: MSI/MSIX, code signing; Linux: AppImage/Flatpak/deb — each with Vulkan-loader presence as a runtime requirement |
| Maintenance | highest: three platform layers, two of them for desktop |
| Trade-off | Only option that satisfies today's "no third-party UI library" rule without a new gate, at the cost of writing a widget toolkit or an immediate-mode chrome inside the renderer |

### S2 — JVM desktop shell over the existing JNI API

| Aspect | Assessment |
| --- | --- |
| Licensing | OpenJDK (GPLv2 + Classpath Exception) runtime bundled via `jlink`/`jpackage`; Swing/AWT ship with the JDK; JavaFX would be a separate gated dependency |
| Ownership | the 275-function `NativeViewport` contract is reused unchanged except `surfaceCreated(Surface)` (`NativeViewport.java:24`), which needs a desktop variant taking a native window handle (JAWT is part of the JDK) |
| Reuse | domain 100 %, JNI exports ~95 % after the `android/*` includes and log sink are split out, 22 pure-Java policy classes and their 20 JVM tests 100 %; the 63 Android View files are rewritten in Swing |
| Packaging | ~40-60 MB runtime image per OS (typical `jlink` size, E5); one JNI `.dll`/`.so` per OS/arch |
| Maintenance | one language pair (Java + C++) like today; Swing hi-DPI and Wayland support are uneven (E5) |
| Trade-off | Fastest path to parity of *policy*, but gesture arbitration still sits in JNI (§2.3) and the shell inherits a second UI toolkit to keep visually aligned with Android Views |

### S3 — Library-assisted shell (CANDIDATE ONLY — needs an OWNER licence + architecture gate)

Candidates, named so the gate can weigh them, not chosen: a windowing/input layer (SDL3 — zlib;
GLFW — zlib) plus a chrome layer (Dear ImGui — MIT; Qt 6 — LGPLv3/commercial). Any of them
conflicts with the CLAUDE.md hard rule "No third-party runtime, rendering, math or input
library" and with `ARCH-OWNER-01` ("not authorized … a cross-platform UI framework"). GLM is
explicitly prohibited and is not a candidate.

| Aspect | Assessment |
| --- | --- |
| Licensing | permissive for SDL/GLFW/ImGui; Qt LGPL requires dynamic linking or a commercial licence and relinking obligations |
| Ownership | window/input/DPI handled by the library; ForgeShape still owns gesture semantics |
| Reuse | domain + renderer 100 % (SDL/GLFW create a `VkSurfaceKHR` per OS) |
| Packaging | one more component to pin, vendor (like Manifold) and audit |
| Trade-off | lowest platform-layer effort, highest governance cost; an immediate-mode chrome would diverge from the Android shell's look and accessibility |

### Not a desktop port, but the cheapest parity step: the existing APK on large-screen / desktop-mode Android (ChromeOS, Android desktop windowing)

It reuses everything, but today a mouse can only orbit (pan/zoom need two pointers,
`camera.cpp:289-311`; wheel and buttons are dropped, `jni.cpp:404-413`), `configChanges` lacks
`keyboard` (Activity recreation on keyboard attach — hypothesis), and the manifest implies a
required touchscreen. Same input-vocabulary work as §2.5, on the one production platform.

## 4. Recommended future architecture BOUNDARY (not authorization)

```
           Android shell (Java Views)          Desktop shell (S1 / S2 / S3, gated)
                    │  JNI (275 fns)                     │  C++ or JNI
                    ▼                                    ▼
   ┌──────────────────────────────────────────────────────────────────────┐
   │ Application core (C++, platform-neutral, NEW boundary):               │
   │  document context (scene, history, sessions, camera, selection,       │
   │  allocator), gesture arbitration over TouchPointer[] + future wheel/  │
   │  button/hover samples, load/close orchestration, autosave decision    │
   │  (fingerprint + "should checkpoint now"), diagnostic token sink       │
   └──────────────────────────────────────────────────────────────────────┘
                    │                                    │
          Domain (unchanged, 98 TUs)          Renderer (unchanged except a
                                              surface + extent + log seam)
```

Rules for that boundary, derived from existing ones:
- nothing above it owns truth (`ARCH-OWNER-01`), so the core is the only writer;
- it speaks ForgeShape units and enums (`forgeshape_input.h` precedent), never OS codes;
- bytes, never paths, cross it (the existing `Uri` rule);
- one unit policy for pixel thresholds: reference units × density, pushed per window;
- the renderer takes a surface it does not create, keeping `ARCHITECTURE.md:214`'s intent (and
  fixing its wording — see `SEAT_G.md` G-03).

## 5. Window / input / render adapter options

| Adapter | Windows | Linux | Note |
| --- | --- | --- | --- |
| Window | Win32 `HWND` | Wayland `wl_surface` (+ xdg-shell) or X11 `Window` | two Linux backends unless a library is gated |
| Vulkan surface | `VK_KHR_win32_surface` | `VK_KHR_wayland_surface` / `VK_KHR_xlib_surface` / `VK_KHR_xcb_surface` | Wayland reports `currentExtent = 0xFFFFFFFF` → extent must come from the shell (today from `ANativeWindow`, `renderer.cpp:2104-2107`) |
| Present | FIFO (always supported) | FIFO | no change |
| Pointer | `WM_POINTER*` (pen pressure/tilt available), `WM_MOUSEWHEEL` | `wl_pointer`, `zwp_tablet_v2` | map into `TouchPointer` + a future wheel/button sample |
| DPI | per-monitor v2 awareness, `WM_DPICHANGED` | `wl_surface` scale / fractional-scale | today's global density atomic must become per window |
| Text input | IMM/TSF | text-input-v3 / XIM | numeric fields only today |
| Accessibility | UI Automation | AT-SPI | no shared code with Android's |

## 6. Test / build infrastructure

- **Now, without any port:** run `scripts/host-native-selftests.sh` in `CI FAST` (Linux, already
  the runner OS) so the "domain compiles off-Android" constraint of `ARCH-OWNER-01` is guarded
  rather than manual (G-02). Adding a second job with `-mfma` (or an aarch64 runner) would catch
  FP-contraction drift before a physical phone does (G-01).
- **Later:** a CMake option that builds the domain as a static library without Android (the
  product `CMakeLists.txt` already finds `glslc` on `PATH`; it fails only on three
  `find_library` results); CTest wrapping the 25 suites; a Windows runner with MSVC (MSVC
  compatibility is UNDETERMINED — no MSVC in this environment; the source grep found no GCC
  builtins, attributes, designated initializers, VLAs, `M_PI` or `long`-width assumptions).
- Emulator evidence is not desktop evidence, and desktop GPU evidence would be a third gate
  beside the x86_64 emulator and physical arm64.

## 7. Parity vs device-specific UX

Parity (must match): authored truth, refusals by name, `.forge` bytes, history semantics, the
fixed UI vocabulary (CLAUDE.md *Naming*), 48 dp-equivalent hit floors, "a control that cannot
succeed is not drawn". Device-specific (may differ): navigation bindings (wheel zoom, middle /
right drag pan / orbit), hover pre-highlight, keyboard shortcuts (none exist on Android),
context menus, multi-window, docking — with `UI-OWNER-02`'s "must not become desktop-CAD
clutter" as the open owner question for a desktop layout.

## 8. File-system and security differences

| Topic | Android today | Desktop implication |
| --- | --- | --- |
| Storage | app-private `getFilesDir()`; SAF for user files; no path ever reaches JNI | user-visible paths exist; keep paths in the shell, bytes below |
| Atomic save | fsync + `renameTo`, delete+rename fallback (`ProjectSlot.java:98-112`) | Windows rename does not replace; use replace APIs; POSIX needs a directory fsync for durability (hypothesis H-G-03) |
| Sandbox | per-app UID; no network permission | no sandbox by default; "sends nothing anywhere" must be preserved by construction (the domain has no socket code) |
| Untrusted input | `.forge` and `.glb` fail closed by name | same parsers; desktop users open arbitrary files more often, so the fail-closed decoders are the security boundary |
| Crash recovery | checkpoint + quarantine | same policy; checkpoint location per user profile |
