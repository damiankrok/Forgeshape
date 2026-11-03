# FUNCTION-COUNCIL-R2 — Desktop proof-of-concept GATE (Seat G, plan only, no code)

Tree `70a6c14`. A desktop client is H2 and unauthorized (`ARCHITECTURE.md:195-201`,
`ARCH-OWNER-01`). This document is the bounded plan an OWNER could approve; nothing here is
started. It reuses only what this audit measured (`PORTABILITY_MATRIX.md` §5).

## G0 — Preconditions (owner decisions, before any code)

| ID | Decision needed | Why |
| --- | --- | --- |
| G0-1 | Authorize a desktop **spike** as research (no product claim, no release) and name its OS (Linux first is cheapest: CI runners are Linux; Windows needs a runner and MSVC) | `ARCHITECTURE.md:216-218` forbids an abstraction for a platform without a target |
| G0-2 | Choose the shell family: S1 native, S2 JVM, or S3 library-assisted (S3 needs a licence/architecture gate per library) | CLAUDE.md "Any second third-party library needs its own gate" |
| G0-3 | Decide the FP-contraction policy (pin `-ffp-contract=off` everywhere, or make every exact oracle tolerant) | P9/P10: results already differ between arm64 and x86_64 (G-01) |
| G0-4 | Decide whether a re-save after Open must be byte-identical regardless of process history (the forward-only allocator, P8) | multi-document desktop makes this visible |

## G1 — Must compile (no window yet)

| Step | Pass criterion | Already met? |
| --- | --- | --- |
| G1-1 | All 98 platform-neutral TUs + Manifold build as one static library with the desktop toolchain | **Met on Linux** (g++ 13, clang 18; libstdc++ and libc++) — E1. Windows/MSVC: not measured |
| G1-2 | 25 suites run on the desktop toolchain: `HOST_SELFTESTS_OK (4392 checks, 0 failed)` | **Met on Linux x86-64 without FMA**; **fails 2 checks with FMA contraction** (G-01) |
| G1-3 | 72-fixture corpus verdicts identical to the Android build's | Met on Linux (fc2 `host_corpus_verdicts.txt`) |
| G1-4 | Renderer compiles with the surface seam replaced by the desktop surface extension and a log sink | Met as a syntax probe with stubs (P4); not linked, not run |
| G1-5 | Shaders compile with a desktop `glslc` | Met with the NDK `glslc` on Linux (P5) |

## G2 — Minimal viewport bridge (the one thing a PoC proves)

Scope, deliberately small: open ONE `.forge` file given on the command line, draw it, orbit /
pan / zoom it, pick a body, close. No chrome, no editing, no save.

1. Window: one top-level window with a Vulkan surface from the chosen shell family.
2. Renderer: the existing `Renderer` with a surface handed in (`VkSurfaceKHR` + extent); FIFO
   present; rebuild on `VK_ERROR_OUT_OF_DATE_KHR` and on resize only (same rule as Android).
3. Domain: decode → `loadProjectDocument` → `viewSceneSnapshot()` → renderer, exactly the path
   the headless probe P7 drove.
4. Input: mouse mapped into `TouchPointer` with `PointerToolType::Mouse`; wheel zoom and a pan
   binding need a vocabulary decision first (G0), so the PoC may emulate pan/zoom with a
   synthetic second pointer — recorded as a PoC shortcut, not a design.
5. Density: one value pushed per window into `setGizmoPixelsPerReferenceUnit` (global today).

## G3 — Test seam

| Seam | Use |
| --- | --- |
| Host suites (25) | unchanged; must pass on the desktop toolchain (G1-2) |
| Headless driver (the P7 shape: decode, load, capture, encode, `pickScene`, camera) | a desktop CI job asserting byte-identical re-encode and a deterministic pick at a fixed pose for N fixtures |
| Renderer smoke | the existing `FORGESHAPE_NATIVE_VIEWPORT_OK` token, emitted through the log sink, plus a frame-count assertion; a software Vulkan (e.g. a CPU ICD) would be a further gated dependency — candidate only |
| Input | replay recorded `TouchPointer` sequences into the extracted gesture arbitration (requires G2.3/§2.3 of the strategies) |

## G4 — Acceptance (PoC)

| ID | Criterion | Evidence required |
| --- | --- | --- |
| A1 | The desktop build runs the 25 suites with 0 failures | E1 log on the desktop runner |
| A2 | The 72 fixtures give the same verdicts as Android | E1 diff of verdict lists |
| A3 | A fixture opened, orbited and closed re-encodes byte-identical to the input | E1 |
| A4 | `pickScene` at the window centre names the same `ObjectId` as the host probe for the same pose and size | E1 |
| A5 | Resize, minimize/restore and DPI change keep rendering with no swapchain rebuild per frame | E1 log of rebuild count |
| A6 | Window close releases the surface before the window is destroyed (desktop analogue of the `surfaceDestroyed` rule) | E1 |
| A7 | No Android artifact changes: APK bytes and the Android startup tokens are identical before/after the seam split | E1 (release guard, CI DEVICE) |
| A8 (negative) | A truncated or corrupt `.forge` passed on the command line is refused by name and the window shows nothing loaded | E1 (`truncated_v1` → `Truncated`, already seen in P7) |

## G5 — Gates and stop rules

- Stop if the seam split changes any Android byte, token or test result (A7).
- Stop if the PoC needs a library that has not passed its own OWNER gate.
- Stop before chrome, editing, saving or file dialogs: those are the next stage, not this one.
- Emulator, desktop and physical-device evidence stay three separate claims; none closes another.
- Budget: the PoC is bounded to G1 + G2 + G3 + A1-A8; any new product behaviour is out of scope.
