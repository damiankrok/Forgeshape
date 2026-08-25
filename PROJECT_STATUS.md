# ForgeShape — Project Status

**Status Version:** 0.33.0
**Updated:** 2026-08-25
**Result:** COMPLETE — DOC-R1 compacted the core documentation and closed the one
formal blocker left by UI-R2. Every core document is inside its 2000-line hard cap
with a working margin, and UI-R2 is formally complete.
**Current Phase:** Phase 1 — Native Viewport
**Workspace:** `D:\TRAVELAPPS\ForgeShape`
**Accepted implementation baseline:** UI-R2 (workspace composition redesign) on
top of INPUT-R1 (pointer semantics foundation), UI-R1C2 (world grid + adaptive
workspace), UI-R1C1 (motion + selection feedback), UI-R1B2 (theme system +
Light), UI-R1B1 (visual foundation + start flow), Stage 017 (multi-object scene),
the Pre-017 Correctness Repair, Gate P1 (physical ARM64 closure), Stage 016-R2,
Stage 016 (Plane), Stage 015D (camera projection), Stage 015C-R (front-face
culling), Stage 015C (shading), Platform Fix P2, Stage 015B, Stage 014, the NDK
r29 migration (Gate P0) and the Owner Decision Baseline. Per-stage narrative
lives in Git history; only what still constrains the code is kept here.
**Next Stage:** **OWNER DECISION** — schedule Selection Outline or choose the next
roadmap stage. See *Next Stage*.

## Current state

A standalone Android application (`com.forgeshape.app`) that owns its own Vulkan
viewport: a Java shell owning no domain truth, a plain `SurfaceView`, a JNI
boundary carrying whole sections and semantic pointer samples, and a
platform-neutral C++17 domain beneath a native renderer that owns no geometry
truth. No Compose, no AndroidX, no third-party runtime library, no engine.

A scene holds several Construction Bodies, each an exact primitive (Box, Cylinder,
Sphere, Cone, Capsule, Plane) with authoritative double-meter dimensions and a
rigid double-degree placement; any body can be frozen to a Frozen Sculpt Mesh and
deformed with four brush tools. Two appearances, two shading models, two
projections, a world reference grid, and an Editor Workspace that re-composes
itself per window. `ARCHITECTURE.md` owns the ownership map and every invariant;
`PRODUCT.md` owns the user-visible description; `README.md` owns build/run/verify.

**Blockers: none.** The documentation-size blocker recorded at UI-R2 is closed.
Known costs and accepted debt are in *Technical Debt*; environment hazards are in
*Known Issues*.

**Core document sizes** (hard cap 2000 physical lines each, measured at DOC-R1):
`ARCHITECTURE.md` 1915 (was 2032), `PROJECT_STATUS.md` 927 (was 1345),
`PRODUCT.md` 814, `README.md` 404, `CLAUDE.md` 216. Always re-measure before
quoting a count.

**Stylus / S Pen on real hardware remains UNVERIFIED.** Tool type, pressure and
tilt are carried end to end and verified synthetically (`MotionEvent.obtain` with
a tool type and an `AXIS_TILT` value, the same path an S Pen drives); closing it
needs a person physically moving a pen. Nothing consumes those fields, so no
behaviour depends on the gap.

## Durable constraints from closed stages

Only facts that still constrain the code. Everything else is in Git history, and
every architectural invariant is owned by `ARCHITECTURE.md`.

**The ARM64 barycentric tolerance must not be removed or tightened.**
`intersectRayTriangle` widens both containment bounds by
`kBarycentricEpsilon = 1e-6f`. A ray landing on an edge two triangles *share* has
a barycentric coordinate that is mathematically exactly 0, and its sign is decided
purely by rounding; if it rounds negative for one triangle it does for its
neighbour too, so both reject a ray that geometrically hits. The rounding is
ABI-dependent — no `-ffp-contract=off` is set, so Clang contracts dot/cross
products into fused multiply-adds on arm64-v8a where baseline x86-64 cannot. An
on-device probe measured the failing ray at `u = -9e-9`. Product-visible: the
centre of a Plane *is* the shared diagonal of its two triangles, so tapping the
middle of a Plane selected nothing. Pinning the FP model instead would only
re-hide the same knife-edge geometry.

**Self-tests and instrumented tests must not depend on live process-scoped
state.** Every native case publishes its own fixture, and the scene suite builds
its own `ConstructionScene`. The scene is process-scoped with no delete, so bodies
accumulate across an instrumented run and the product may be left in Sculpt by an
earlier case: no instrumented test may assume a body count, which body sits at the
origin, or which mode is current.

**The device verifier is mechanical and needs no device.**
`scripts\verify-device-guards.ps1` runs `DEV2-01`..`07` and `DEV3-01`..`06`: it
recognises a bare `adb` in any form, scans `.ps1`/`.cmd`/`.bat`/`.sh` and the
Gradle files, detects an executable `connected*AndroidTest` fan-out, proves an
argument array actually carries `-s` rather than trusting its variable name, and
reports which surfaces it scanned so a check that covered nothing cannot pass.

**Heavy-mesh ladder, measured on a physical Galaxy S25 Ultra (Gate P1).**
Single-run figures from one device; existence evidence that the mandatory tiers
work on physical ARM64, not a supported capacity limit.

| tier | vertices | triangles | gen ms | publish ms | render build ms | pick | PSS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ~10k | 10086 | 19200 | 2.763 | 1.695 | 19.577 | tri 15592 | 219 MB |
| ~50k | 49686 | 97200 | 17.776 | 7.880 | 72.204 | tri 78824 | 231 MB |
| ~100k | 99846 | 196608 | 32.405 | 15.426 | 104.387 | tri 159210 | 248 MB |

Sculpt at ~100k: freeze 110.126 ms building 592896 adjacency entries, two real
Grab strokes, every post-stroke upload `reuse`, PSS 264 MB. No crash, OOM, ANR or
thermal symptom at any tier. The last stable mandatory tier is ~100k; 250k/500k
were not run. Also closed under Gate P1: `arm64-v8a` packaged beside `x86_64`,
both `.so`s at 0x4000 ELF `LOAD` alignment with `zipalign -P 16 -c` OK, and a
debug-only Vulkan validation session with **zero ForgeShape-caused messages** of
any severity (the layer was adb-pushed through Android's own per-app GPU debug
layer settings, never bundled, never committed, and removed afterwards).

## Owner Decision Baseline

Active owner decisions are identified by `UI-OWNER-*`, `ARCH-OWNER-*`,
`INPUT-OWNER-*` and `DOC-OWNER-*`. Recorded 2026-08-20.

**Implemented by Stage 015B:** UI-OWNER-01 (the shell), UI-OWNER-02 (compact /
medium / expanded), UI-OWNER-03 (Export as a global action, drawn and reserved),
UI-OWNER-05 (the destructive re-Freeze guard) and UI-OWNER-06 (stylus-friendly,
no pressure). **Still decisions only, not behaviour:** UI-OWNER-04 — Sketch and
Extrude have visible, disabled homes in the Tool Rail and no implementation
whatsoever.

The historical bare `D1`–`D6` tables from Stage 015A and Stage 015A-R are
**superseded and non-authoritative**, and the two tables did not even mean the
same things, which is why bare `D` numbers were retired. No stage gate,
acceptance table or preflight may cite a bare `D` number again.

| ID | Decision | Approved value |
| --- | --- | --- |
| UI-OWNER-01 | Shell family | **Forge Shell** — a modernized viewport-first shell: direct edge controls and a Tool Rail for Sculpt, the same shell language plus a contextual exact-value Property Inspector for Construction. One shell; mode and tool decide content, never structure. |
| UI-OWNER-02 | Device scope | **phone + tablet** — Stage 015B implements adaptive compact / medium / expanded behaviour now. A tablet may dock more surfaces but must not become desktop-CAD clutter. |
| UI-OWNER-03 | Export placement | **global action** — Construction, Sculpt and UV remain editing contexts; Export is not one of them and later opens its own output surface. |
| UI-OWNER-04 | Sketch + Extrude | **approved as an iterative CAD workflow**, not implemented and not part of Stage 015B geometry. See below. |
| UI-OWNER-05 | Confirmation before a destructive re-Freeze | **yes**, and **only** when existing Frozen Sculpt Mesh edits would actually be replaced. A normal Resume Sculpt has no confirmation — it destroys nothing, and guarding it would train the user to dismiss the guard that matters. |
| UI-OWNER-06 | Stylus pressure in Stage 015B | **no** — deferred to a dedicated Sculpt stage. 015B must still be stylus-friendly and preserve a clean path for pressure, tilt and hover. |
| ARCH-OWNER-01 | Future Apple portability | **required architectural constraint**. See below. |
| INPUT-OWNER-01 | Stylus-first interaction | **required product/architecture constraint**. See below. |
| DOC-OWNER-01 | Clear naming and code comments | **required documentation/maintainability rule**, recorded durably in `CLAUDE.md`. |

**UI-OWNER-04 in detail.** A Construction Body may begin either from an exact
primitive **or** from a 2D sketch: standard sketch planes (later on planar
Construction faces), multiple sketches on different planes, Line/Polyline,
Rectangle, Circle, select/delete, grid and snap, exact numeric entry, closed-
profile detection and validation, and repeated Sketch → Extrude workflows. The
first Extrude vertical slice supports **New Body**; Extrude **Add** and **Cut**
are required integration **after** boolean infrastructure exists. The result stays
Construction source and history until an explicit Freeze to Sculpt. **This
approval is not permission for** a geometric constraint solver, assemblies,
NURBS, engineering drawings, Revolve, Fillet, Chamfer or Shell — each needs its
own approval.

**ARCH-OWNER-01 in detail.** Android remains the **first production platform**.
Construction, Geometry, Sculpt and domain code stay platform-neutral C++; Android
`View`/`Activity`/JNI types never become domain truth; the Android UI is a
platform shell/adapter; input crossing the boundary moves toward semantic,
platform-neutral pointer/tool samples; future file and platform services sit
behind narrow boundaries; renderer/platform-surface coupling stays explicit.
**Not authorized now:** an iOS/iPadOS application, an Xcode project, a Metal
backend, a MoltenVK dependency, or a cross-platform UI framework. A future Apple
UI may be native to Apple — sharing Android View code is not a goal — and the
Apple render path is deliberately undecided, belonging to a later evidence-based
spike. The architectural consequences are owned by `ARCHITECTURE.md`.

**INPUT-OWNER-01 in detail.** ForgeShape must stay comfortable for finger input,
for an Android stylus/S Pen, for tablets and for a future Apple Pencil: touch
targets practical for both a stylus tip and a fingertip, a shell that does not
unnecessarily cover the model, explicit gesture ownership between viewport,
chrome and tool, and a semantic input boundary able to carry pressure, tilt,
hover and tool type. Stage 015B does **not** change Sculpt deformation from
pressure.

**DOC-OWNER-01 in detail.** Owned by `CLAUDE.md`, including the fixed UI
vocabulary; not restated here.

## Product Direction

ForgeShape is an author-owned offline 3D modeling/sculpting application.
Production architecture must NOT use Godot, GDExtension, Unity, Unreal or any
other general-purpose engine that owns the viewport or render loop.

## Environment and toolchain

| | pinned / installed |
| --- | --- |
| JDK | 21.0.9 (Android Studio JBR, `JAVA_HOME`) |
| Android SDK | `C:\Users\damia\AppData\Local\Android\Sdk`; platforms 33/34/35/36/36.1 |
| Build tools | 36.1.0 in use (27.0.0, 33.0.3, 35.0.0, 35.0.1 also installed) |
| **NDK** | **pinned to `29.0.14206865`** in `app/build.gradle`. 25.2.9519653 and 27.2.12479018 are also installed but unused. An r30 beta is prohibited. |
| CMake | 3.22.1 in use (4.1.2 also installed) |
| Gradle / AGP | 8.14.3 wrapper / 8.13.2 |
| SDK levels | compileSdk 36, targetSdk 36, minSdk 26 |
| ABI filter | `x86_64` (emulator) + `arm64-v8a` (physical devices, Gate P1) |
| C++ / STL | C++17, `c++_static`; one packaged `.so`, `lib/x86_64/libforgeshape_native.so` |
| Shaders | `glslc` at `<ndk>/shader-tools/windows-x86_64/glslc.exe`, AOT from CMake |
| System images | only `system-images;android-36.1;google_apis_playstore;x86_64` |

No Godot/GDExtension files remain in the workspace.

**Git.** The root repository is initialized, **local only**: no remote, nothing
pushed, and no global Git config — the committer identity lives in `.git/config`
alone. Baseline commit `5d386f03e7b33de47ea717a4a1d629231b073b56`
(`baseline: accepted Stage 014`, 171 files). `.gitignore` excludes Gradle/CMake/
NDK output, `.cxx`, `.gradle`, IDE state, `local.properties` and APK/AAB output.
It deliberately does **not** ignore `artifacts/`, which holds the runtime
evidence screenshots cited by past stage acceptance.

### Android runtime targets

| AVD / serial | Status |
| --- | --- |
| `Medium_Phone_API_36.1` / `emulator-5554` | **Reserved by another program.** ForgeShape must not use, start, stop, wipe, reconfigure, install to, send input to, log or screenshot it until the owner lifts this. |
| `ForgeShape_Stage004` / `emulator-5556` | **Contended, non-authoritative.** Another program runs `com.damian.wlochyikafalonia.claude.debug` on it, steals the foreground and injects taps that reach ForgeShape. Do not use it for authoritative evidence; do not stop, wipe or reconfigure it. |
| `ForgeShape_Stage006` / port varies, boot with `scripts\start-forgeshape-emulator.ps1` (default port `5580`) | **Current ForgeShape-owned evidence target.** Isolated, own AVD definition and data dir. Always confirm identity with `adb -s <serial> emu avd name`, never by port alone; this AVD has been seen on `5556`, `5558` and `5580` across sessions purely by allocation order. |
| `ForgeShape_16K` / port varies, boot with `scripts\start-forgeshape-emulator.ps1 -Avd ForgeShape_16K -Port <explicit>` | **Gate P1's 16 KB runtime target.** x86_64, `google_apis_playstore_ps16k`; `getconf PAGE_SIZE` = 16384. Not a substitute for physical ARM64 — same isolation rules as `ForgeShape_Stage006` apply (explicit port, confirm identity by name, never `5554`). |
| **Physical ARM64 phone** — owner-supplied, attached over Wi-Fi adb; serial supplied per session and deliberately not recorded in this repo | **Gate P1's physical ARM64 evidence target.** Samsung Galaxy S25 Ultra (`SM-S938B`), Snapdragon 8 Elite (`SM8750`), Android 16 / API 36, 1440×3120, `arm64-v8a` only, `getconf PAGE_SIZE` = 4096. Vulkan: swapchain format 37, 5 images, FIFO. `/system/bin/uinput` is usable from the adb shell (the shell user is in the `uhid` group), so real multi-touch injection works. Same rules as every other target: explicit `-s <serial>` on every command, confirm ForgeShape is resumed before evidence, never a bare `adb devices`. |

`ForgeShape_Stage006` is pixel_6, 1080×2400, density 420, multi-touch, GPU host,
`x86_64`, API 36 (`google_apis_playstore`). Vulkan: loader instance 1.4.0,
physical device "Goldfish GFXStream (AMD Radeon RX 9070 XT)", device API 1.3.0,
swapchain format 37 (`R8G8B8A8_UNORM`), 4 images, FIFO. `ForgeShape_16K` shares
the same profile and Vulkan stack on the `google_apis_playstore_ps16k` image.

Rules that apply to every run:

- Every `adb` command names its target explicitly: `adb -s <serial> ...`.
- Launch the emulator **detached** (e.g. WMI `Win32_Process.Create`); it dies if
  spawned as a child of a tool shell.
- Before any evidence-sensitive input or screenshot, confirm ForgeShape is the
  resumed activity; invalidate any run contaminated by foreign input.
- Creating a new AVD from already-installed tooling is allowed; installing or
  updating SDK/NDK/JDK/system images is not.
- `adb root` is unavailable on Play Store images, so `sendevent` injection is
  impossible; multi-touch is injected with the on-device `uinput` tool, which
  consumes concatenated JSON objects (no array, no commas).

## Verified capability matrix

Everything below is verified at runtime on a ForgeShape-owned AVD through the
real Android touch path, most recently `ForgeShape_Stage006` / `emulator-5580`.
`PRODUCT.md` owns the user-facing description.

| Capability | State |
| --- | --- |
| Vulkan viewport, swapchain, depth, pipeline, indexed draw, surface recreation | VERIFIED |
| Camera: orbit / pan / pinch, pointer-set re-anchoring, cancel handling | VERIFIED |
| Perspective (60° FOV) and true Orthographic projection, switchable | VERIFIED |
| Switching projection preserves target-plane framing; the frame does not jump | VERIFIED |
| Orthographic pinch changes the world span, not the orbit distance | VERIFIED |
| Picking correct in both projections; ortho ray origin moves per pixel | VERIFIED |
| Sculpt hit and brush radius correct in both; ortho radius is depth-independent | VERIFIED |
| Projection change mints no revision and touches no geometry truth | VERIFIED |
| Projection mode and framing survive HOME/resume and surface recreation | VERIFIED |
| Tap-to-select, tap-to-clear, drag and multi-touch never select | VERIFIED |
| CPU picking follows camera, dimensions, transform and sculpt deformation | VERIFIED |
| Dynamic mesh: immutable revisions, fail-closed validation, capacity reuse/growth | VERIFIED |
| SEVERAL Construction Bodies in one scene, each with exact Box / Cylinder / Sphere / Cone / Capsule / Plane | VERIFIED |
| Stable per-body ObjectIds, independent of collection index, mesh revision and GPU allocation | VERIFIED |
| Add Body appends and selects; deterministic insertion order across edits and selection | VERIFIED |
| Primitive and transform edits reach only the active body; A↔B round-trips the exact spec and placement | VERIFIED |
| Per-body mesh publication: each body its own revision chain; A's edit cannot replace B's mesh | VERIFIED |
| Renderer draws every body with its own transform and its own GPU buffers | VERIFIED |
| Editing A rebuilds and uploads nothing for B | VERIFIED |
| Only the selected body is highlighted | VERIFIED |
| Becoming selected gives a short acknowledgement pulse that decays to a much lower resting tint | VERIFIED |
| A tap on an already-selected body does not re-pulse; only a change in selection truth does | VERIFIED |
| Selection feedback mints no revision, rebuilds no render mesh and uploads nothing, in either appearance | VERIFIED |
| Two bodies' pulse states are independent; deselecting one does not disturb the other | VERIFIED |
| The selected body stays obvious and the model's form stays readable in Dark and in Light | VERIFIED |
| A world reference grid on the XZ plane at y = 0, switchable from the Display popover's View group | VERIFIED |
| The grid is ON by default and its choice survives rotation, Activity recreation and HOME/resume | VERIFIED |
| Grid lines at 1 m, a 5 m major rhythm, a 20 m extent that fades radially rather than ending at a border | VERIFIED |
| The X and Z axes are told apart by a faint warm/cool lean, not by saturated primaries | VERIFIED |
| The grid is readable in Dark and in Light, and never competes with the model for the eye | VERIFIED |
| The grid is correct in Perspective and in Orthographic, and changes neither projection nor camera pose | VERIFIED |
| A Construction Plane at world y = 0 shows no z-fighting; two consecutive static frames are byte-identical | VERIFIED |
| The grid never enters the scene, a snapshot, picking, Freeze, Sculpt or a revision | VERIFIED |
| Toggling the grid rebuilds no render mesh, uploads nothing and mints no revision | VERIFIED |
| The grid's vertices are uploaded exactly once per Vulkan device and survive rotation and resume | VERIFIED |
| Expanded windows give Objects a persistent column beside a docked Property Inspector | VERIFIED |
| Compact and medium keep the current viewport-first model; Objects stays in the shape editor | VERIFIED |
| The Tool Rail is drawn flush and level when docked, raised and translucent when floating | VERIFIED |
| One Objects section, re-parented — no second Java list and no second selection truth | VERIFIED |
| Row tap, viewport pick and Add Body stay in sync from whichever surface Objects is on | VERIFIED |
| ~20 bodies stay listed, scrollable in the column's own container, and selectable | VERIFIED |
| The SurfaceView is the whole window in every layout mode; docking never resizes the render target | VERIFIED |
| Reduced motion goes straight to the resting tint and runs no pulse at all | VERIFIED |
| Chrome hide/restore and the inspector detent are short, interruptible and always settle at a legitimate resting state | VERIFIED |
| A chrome transition never resizes the viewport or rebuilds the swapchain | VERIFIED |
| A viewport gesture outranks chrome motion: a detent change during a real stroke is instant | VERIFIED |
| Scene picking returns the nearest hit's correct ObjectId; a viewport pick re-points the editors | VERIFIED |
| Sidedness is per body: a Plane body does not make its neighbour two-sided | VERIFIED |
| Per-body Freeze / Resume / stale-source / current-mesh edit predicate, independent across bodies | VERIFIED |
| A→B→A returns A's own Frozen Sculpt Mesh, revision and edits, without re-freezing | VERIFIED |
| Mode, held tool and brush Radius/Strength are session-wide and unchanged by switching bodies | VERIFIED |
| Objects list holds no Java-side model selection; every refresh re-reads native state | VERIFIED |
| Plane: 4:6 open source topology, exact bounds, two-sided render and pick as one bounded, named exception | VERIFIED |
| Sidedness is owned by the active published representation; render, selection picking and Sculpt hit-test all read that one value | VERIFIED |
| Frozen Plane renders, picks and **sculpts** from both sides; a real stroke lands on the underside | VERIFIED |
| A stale frozen solid never inherits two-sidedness from a Construction Source later changed to a Plane (and the converse) | VERIFIED |
| Destructive re-Freeze confirms only for edits on the CURRENT frozen mesh; historical strokes never raise it | VERIFIED |
| The re-Freeze message states no stroke count, because no honest per-mesh count exists to state | VERIFIED |
| Exact dimensions in meters; mm/cm/m display unit above JNI only, lossless | VERIFIED |
| Apply Shape: Applied / Unchanged / Rejected, atomic, kind change is a change | VERIFIED |
| Every primitive's parameters remembered independently across kind changes | VERIFIED |
| Capsule relation (`totalHeight >= diameter`) rejected in words, object unchanged | VERIFIED |
| Capsule equality case generated as a sphere (482 : 2880) | VERIFIED |
| Position/rotation transform, degrees, right-hand rule, local X→Y→Z | VERIFIED |
| Transform-only edit publishes no revision and triggers no GPU upload | VERIFIED |
| Freeze to Sculpt / Back to Construction / Resume Sculpt without re-freezing | VERIFIED |
| Stale-source policy: Construction change never touches the sculpt mesh | VERIFIED |
| Four sculpt tools (Grab, Clay, Smooth, Inflate) on one shared kernel | VERIFIED |
| Shared Radius and Strength, clamped, unchanged by tool switching | VERIFIED |
| Clay ≠ Inflate, measured (start-normal vs current-normal) | VERIFIED |
| Pending-then-promote: multi-touch navigation cannot mutate the sculpt mesh | VERIFIED |
| Sculpt topology fixed; buffers reused, never reallocated during a stroke | VERIFIED |
| Construction Source bit-identical after sculpting with all four tools | VERIFIED |
| Lifecycle: shape, placement, identity, unit, mode, tool, sculpt, camera and selection survive home/resume with no re-upload | VERIFIED |
| Two explicit appearances, Dark and Light, chosen in the Display popover's Appearance group | VERIFIED |
| Dark is the product default and what a fresh process wears; a process kill returns to it | VERIFIED |
| Light uses a warm off-white VIEWPORT (`#E6E1D9`), not only light chrome, and not a stark white canvas | VERIFIED |
| A theme switch publishes no mesh, mints no revision and causes zero GPU upload or render-mesh rebuild | VERIFIED |
| Scene, active ObjectId, primitive spec, placement, product mode and Frozen Sculpt Mesh survive the switch bit-identically | VERIFIED |
| The appearance survives rotation and HOME/resume; the start chooser does not reappear because of it | VERIFIED |
| The UI session — display unit, inspector detent, Tool Rail entry — survives the recreation that applies a theme | VERIFIED |
| Icons, pressed feedback, active-not-by-colour-alone and 44 dp targets all hold in both appearances | VERIFIED |
| Property Inspector values, labels and verdicts meet WCAG AA contrast on the light theme; its surfaces stay opaque | VERIFIED |
| Start chooser: New Project offers exactly Construction/CAD and Sculpt, over the live viewport | VERIFIED |
| The start question is asked once per process; rotation, HOME/resume and Activity recreation do not re-ask; a process kill does | VERIFIED |
| Choosing Construction creates no body and changes no active body — the default Body is already there | VERIFIED |
| Choosing Sculpt lands on a sphere-derived Frozen Sculpt Mesh (482 : 2880) through the existing apply + Freeze path, with `freezes=1` | VERIFIED |
| After a direct Sculpt start, Back shows the exact sphere Source and Resume returns the same frozen mesh without re-freezing | VERIFIED |
| Every icon is a local vector drawable; no chrome control is a Unicode glyph | VERIFIED |
| Chips, rail entries, Objects rows and icon buttons show immediate pressed feedback | VERIFIED |
| Active state is fill + thicker border + brightened label together, never colour alone | VERIFIED |
| Icon-only controls measure at or above the 44 dp touch floor in a compact window | VERIFIED |
| A small drift on a Tool Rail entry selects that tool; a real scroll selects nothing | VERIFIED |
| Three-level corner radius and depth on floating surfaces only; the docked inspector is flush | VERIFIED |
| Editor Workspace: Global Toolbar, Tool Rail, Property Inspector, direct brush controls | VERIFIED |
| Adaptive layout: compact portrait, phone landscape, expanded/tablet, decided by window dp | VERIFIED |
| Landscape occlusion: 0 % unoccluded viewport → **60.1 %**, status line on screen | VERIFIED |
| Inspector collapse and chrome hide restore viewport area (57.5 % → 82.6 % → 100 %) | VERIFIED |
| Edge-to-edge with WindowInsets on chrome only; IME never resizes the Vulkan surface | VERIFIED |
| Every chrome surface consumes its own gesture; viewport pixel-identical across chrome drags | VERIFIED |
| Freeze / Resume wording follows whether a Frozen Sculpt Mesh exists | VERIFIED |
| Destructive re-Freeze confirms only when the current mesh's edits would be discarded; Cancel is inert | VERIFIED |
| Stable semantic ids on every control; 32 instrumented + 22 JVM tests | VERIFIED |
| Correct geometric proportions in portrait, physical 90° landscape and a non-rotated wide window | VERIFIED |
| One orientation convention: identity pre-transform, swapchain image = window | VERIFIED |
| Rotation mutates no Construction or Sculpt state and triggers no mesh upload | VERIFIED |
| Studio Solid replaces the per-vertex rainbow as the default appearance | VERIFIED |
| Derived render-only normals; source RuntimeMesh and picking untouched | VERIFIED |
| One 40° crease policy gives all six primitives their hard/smooth contracts | VERIFIED |
| Smooth ↔ Faceted is presentation only, on the same source revision | VERIFIED |
| ForgeShape-generated MatCap from view-space normals; one preset, no asset file | VERIFIED |
| Studio ↔ MatCap rebuilds no geometry and uploads nothing | VERIFIED |
| Sculpt deformation relights immediately; no stale lighting, no NaN | VERIFIED |
| Selection stays obvious and form stays readable in both shading modes | VERIFIED |
| Display settings are native-owned and survive HOME/resume | VERIFIED |
| No per-frame normal or render-data rebuild: 4448 frames, 2 rebuilds | VERIFIED |
| 16 KB page-size runtime behaviour (x86_64) | VERIFIED (Gate P1) |
| Physical ARM64 execution: `primaryCpuAbi=arm64-v8a`, full self-test suite, smoke, lifecycle and both orientations | VERIFIED (Gate P1) |
| Picking is watertight across a shared triangle edge, and identical on arm64-v8a and x86_64 | VERIFIED (Gate P1) |
| Heavy-mesh ladder ~10k / ~50k / ~100k on physical ARM64: publish, render build, GPU upload, pick, lifecycle | VERIFIED (Gate P1) |
| Sculpt at ~100k vertices on physical ARM64: freeze, adjacency, real strokes, buffer reuse | VERIFIED (Gate P1) |
| Real injected multi-touch on physical hardware never mutates the sculpt mesh | VERIFIED (Gate P1) |
| The scene list is one view with one owner and two hosts; no window shows it twice and no Java copy of ObjectId or selection exists | VERIFIED (UI-R2) |
| A compact window reaches the scene in one tap and gives the viewport back in one more; closed, the panel costs the model nothing | VERIFIED (UI-R2) |
| Opening the scene panel, selecting a body and collapsing the inspector publish no mesh and mint no revision | VERIFIED (UI-R2) |
| Tool type, pressure and tilt cross MotionEvent → SurfaceView → JNI → native intact, and stay with the right pointer in a multi-pointer event | VERIFIED (INPUT-R1) |
| Carrying stylus data changes no brush result: same stroke, opposite pressure and tilt, bit-identical vertices for all four tools | VERIFIED (INPUT-R1) |
| 16 KB page size *and* ARM64 in one target | **UNVERIFIED** — see Known Issues |
| Stylus / S Pen tool type and pressure on real hardware | **UNVERIFIED** — verified synthetically end to end in INPUT-R1; closing it needs a person physically moving an S Pen |

## Self-test suite

Eleven debug-only native suites run once from `NativeViewport.start()` — never
per frame — and total **1691 checks, zero failures**:

| suite token | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 174 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 94 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 125 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 316 |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 325 |
| `FORGESHAPE_SCENE_SELFTEST_OK` | 79 |

followed by `FORGESHAPE_MESH_UPLOAD_OK`, `FORGESHAPE_GRID_UPLOAD_OK` and
`FORGESHAPE_NATIVE_VIEWPORT_OK`.

**Suite ownership is by module, and it decides where a check lives.** The
render-shading suite owns PRESENTATION — selection feedback (`r1c1_*`) and the
world grid (`r1c2_*`) live there rather than in picking or selection, which own
*which* object is selected and what is in the scene. It also covers the crease
policy, all six primitives' Smooth contracts, the capsule equality case, Faceted,
NaN/Inf and fail-closed behaviour, determinism, the render-data rebuild policy,
the generated MatCap, the display settings, the proof that building render data
leaves the authoritative `RuntimeMesh` bit-identical, and the Plane's two-sided
render duplication and its inertness to display switching. Nothing anywhere is
driven by a clock or a GPU, so a whole 220 ms pulse costs microseconds and no
check can be flaky; none asserts a rendered pixel.

Three cross-cutting families are split across suites by that same rule. The
projection family `CAMPROJ-01`..`14`: camera `01`..`08` and `12`..`14`, picking
`09`/`10`, sculpt `11`. The direction family `NOR-01`..`10`: source winding,
outward render normals, Faceted orientation, duplication-preserves-winding, the
model→view normal transform and display-mode inertness. The Plane family: `PLN-
01`..`08` in the primitive suite, `PLN-09`/`10`/`20` in render shading, `PLN-11`..
`16` in picking, `PLN-17`..`19` in sculpt. Each family includes a member that
would fail if the behaviour collapsed to a constant —
`nor_outwardness_fails_on_global_normal_flip` asserts the measurement inverts
under a global `normal *= -1`.

**The picking suite also owns the POINTER BOUNDARY**, because it already owns
`TouchPointer` and the selection rules the same events drive: 46 checks over the
tool-type wire codes and their Unknown fallback, the struct defaults, pressure and
tilt range/wrap/non-finite behaviour, per-index packing, the 6-pointer bound, and
the proof that a stylus resolves the same tap and orbits the same distance as a
finger. The Android half of the tool-type mapping is deliberately not here: it is
JVM and instrumented, where `MotionEvent` exists. The sculpt suite's 14
pressure-independence checks are its teeth, four tools → three assertions each.

Three non-per-frame diagnostics exist. `FORGESHAPE_SURFACE_CONFIG` (one per
swapchain creation) and `FORGESHAPE_CAMERA_VIEWPORT` (one per `surfaceChanged`)
audit the orientation chain; `FORGESHAPE_RENDER_MESH_BUILD` (one per accepted
geometry or surface-shading change) reports source vs render counts, the rebuild
count and the frame counter. `FORGESHAPE_GRID_UPLOAD_OK` is one per Vulkan
device. `README.md` documents how to read them.

## Android UI suites

| suite | scope | tests |
| --- | --- | --- |
| `WorkspaceLayoutModeTest` (JVM) | breakpoints, placement, chrome sizing, the Objects dock and rail dock decisions | 16 |
| `EditorUiStateTest` (JVM) | what the UI may remember and what it refuses | 11 |
| `LengthUnitTest` (JVM) | exact mm/cm/m round-tripping and parse refusal | 5 |
| `AppThemeTest` (JVM) | the default, the two appearances, and that choosing one moves nothing else | 10 |
| `ChromeMotionTest` (JVM) | the durations, and that reduced motion returns 0 rather than a short duration | 4 |
| `DisplaySettingsContractTest` (JVM) | the shading/surface index contract across JNI | 4 |
| `EditorWorkspaceControlsTest` | control sets, fields, validation, freeze wording, tools, the six-kind round trip | 23 |
| `EditorWorkspaceLayoutTest` | measured viewport floor, landscape, expanded, collapse, the UI-R1C2 adaptive pass | 10 |
| `EditorWorkspaceGestureTest` | chrome gesture ownership, IME | 5 |
| `EditorWorkspaceLifecycleTest` | HOME/resume rebuilt from native truth | 3 |
| `EditorWorkspaceDisplayTest` | display/projection ids, presentation-only, resume, refused index, the View/Grid group | 17 |
| `EditorWorkspaceObjectsTest` | rows by ObjectId, viewport pick sync, the docked surface, 20-body scalability | 10 |
| `EditorWorkspaceStartFlowTest` | the start question, and the direct Sculpt path's Freeze reuse | 8 |
| `EditorWorkspaceFoundationTest` | icons, pressed feedback, touch floor, rail tap-vs-scroll, viewport floor, popover | 7 |
| `EditorWorkspaceThemeTest` | the control, the switch, state preservation across the recreation, contrast | 17 |
| `EditorWorkspaceMotionTest` | popover preserved, inspector interruptibility, chrome hide/restore, viewport stability, reduced motion, gesture priority | 10 |
| `PointerSemanticsTest` (JVM) | the Android tool-type mapping and its Unknown fallback | 6 |
| `EditorWorkspacePointerTest` | synthetic stylus transport, per-pointer association, and that tap / navigation / sculpt arbitration are unchanged | 13 |
| `EditorWorkspaceCompositionTest` | the UI-R2 role split: viewport dominance, the scene panel, one list with one owner, inspector-names-its-body, and that composition rebuilds no geometry | 10 |

**190 tests** (56 JVM, 134 instrumented), all green. No Java test asserts a rendered pixel;
every control is reached by its stable semantic id and no assertion uses a screen
coordinate. The foundation and theme suites deliberately assert no colour
literal, radius or shadow — those are judged by eye and by runtime evidence, and
pinning them would break on every deliberate restyle. What the theme suite
asserts instead is *relational*, plus WCAG contrast ratios computed in the test.

**The suite is run in TWO windows** — the default compact phone window and an
overridden 1600 x 2560 @ 240 dpi expanded window. The adaptive cases read the
window they are actually in and assert the contract that belongs to it, so an
overridden run is a genuine expanded-layout run rather than a simulation, and it
is the only thing that exercises the docked Objects and docked rail branches.

**`EditorWorkspaceMotionTest` asserts a RESTING state and never a frame of an
animation.** A case that sampled a transition part-way through would be a test of
the device's frame timing. It writes `animator_duration_scale` through the
instrumentation's own shell — the product holds no `WRITE_SECURE_SETTINGS` and
must never ask for one — and restores it in **both** `@Before` and `@After` so a
case that dies part-way cannot leave animation off for every suite that follows.
`EditorWorkspaceDisplayTest` restores the display defaults for the same reason.

**A theme switch recreates the Activity**, so `EditorWorkspaceThemeTest.switchTo`
drives the real chip and polls until a workspace reports the new appearance and
has been laid out; setting the field directly would prove a boolean changed and
nothing about whether the workspace survives being rebuilt. Every case leaves the
process in Dark.

**The start question is asked once per process, so every case that is not ABOUT
it answers it first.** `resetToBaselineConstruction` dismisses it;
`EditorWorkspaceObjectsTest` — which deliberately does not use that baseline —
dismisses it in its own `@Before`. Cases that *are* about it call
`showStartChooserAsFirstLaunch()`.

**A view that has just been made visible reports a size of 0 until a traversal has
run.** Measuring a control in the same block that opened its container is a
recurring mistake; settle the layout in between.

`EditorWorkspaceGestureTest.ui11_theImeLeavesTheFieldAndTheCommitPathUsableAndTheSurfaceUntouched`
opens with a precondition guard — "the soft keyboard did not appear, so this case
proves nothing" — and fails on that guard alone, never reaching an assertion about
product behaviour, whenever the keyboard is slow to appear. It has failed and
passed across recent stages (proven pre-existing at Stage 015D and UI-R1B1 by
stashing every change and reproducing the identical message on an untouched tree)
and the message has been character-identical since. Treat a future failure of this
one case as a harness symptom to confirm against the current baseline before
calling it a regression.

## Current evidence summary

Latest acceptance run, on `ForgeShape_Stage006` / `emulator-5580` unless stated:

- **Native self-tests:** eleven suites, **1691 checks, zero failures** on a clean
  launch. The Gate P1 picking assertions are intact inside the now-174-check
  picking suite; `SIDE`, `REFR`, `NOR`, `CAMPROJ` and `PLN` all still green.
- **JVM:** 56/56.
- **Instrumented:** 134 run, **134 green**, twice — once compact, once at an
  overridden 1600 × 2560 @ 240 dpi — through
  `scripts\run-instrumented-tests.ps1 -Serial emulator-5580`.
- **Device guards:** `DEV2-01`..`07` and `DEV3-01`..`06` all PASS, with no device
  attached and zero `emulator-5554` interaction.
- **Physical ARM64 (Gate P1):** closed on a Galaxy S25 Ultra —
  `primaryCpuAbi=arm64-v8a`, `PAGE_SIZE` 4096, the mandatory ~10k/~50k/~100k
  ladder and Sculpt at 100k measured on real hardware. Stylus stays UNVERIFIED.
- **Runtime walkthrough (UI-R2):** cold start, tap selection, orbit, Freeze,
  pending-then-promote, a committed Grab stroke, the scene panel in both
  Construction and Sculpt, HOME/resume and rotation — bounded swapchain rebuilds,
  zero while idle. Earlier walkthroughs are in Git history.
- **Documentation:** DOC-R1 is a documentation-only change; no product, test,
  build or script file was touched, so the runtime evidence above still stands.

**One caveat about capturing self-test evidence.** On both the emulator and the
physical phone the logcat ring buffer intermittently drops whole suites from the
*middle* of a startup capture, which reads exactly like a suite that never ran.
The reliable signal is that the suites which do appear always report their full
expected check counts and `_SELFTEST_FAIL` is always absent; read a partial
capture as "no failures observed", and re-run until one capture is complete before
quoting a total.

## Display-control research, and what was rejected

**Adopted:** the popover grows from the control that opened it rather than sliding
in from a screen edge; selection feedback happens **in place**, so the surface
does not move, re-animate or close on a choice — the one that matters, because
comparing two options means switching repeatedly; non-modal, staying open across
several changes; grouped labelled chip rows rather than one long list, as layout
and not as styling.

**Rejected:** live preview thumbnails with a check badge per option — a rendered
thumbnail per shading mode needs a second offscreen render target, for a choice
whose result is already filling the screen behind the panel; and a bottom sheet
with drag detents — a sheet covers the model, which is the thing being judged, and
ForgeShape already has a Property Inspector for sheet-shaped content.

Nothing decorative was taken. There is no continuous or looping model animation,
no motion on the viewport itself, and no animation anywhere on the path of a
pointer sample. Both animations are interruptible, and a zero system animator
duration scale skips them outright rather than shortening them.

## Known Issues / Blockers

- **A crashed emulator session can leave the quickboot snapshot corrupt, and
  every later launch then hangs before `adbd` starts.** Seen on
  `ForgeShape_Stage006` after the INPUT-R1 session: QEMU runs and
  `adb -s <serial> emu avd name` answers, so the AVD looks alive, but the serial
  never leaves `offline` and `start-forgeshape-emulator.ps1` reports BLOCKED at
  its 180 s deadline. The tell is `snapshots\default_boot\ram.img.dirty` plus a
  `ram.bin` of a few hundred bytes against a multi-GB `ram.img`. Deleting
  `snapshots\default_boot` fixes it — the emulator cold-boots and rebuilds the
  snapshot on its next clean exit. `userdata-qemu.img.qcow2`, `config.ini` and
  the AVD definition are untouched, so nothing authored is lost.
- **16 KB page size *and* ARM64 in the same target is UNVERIFIED.** 16 KB page
  behaviour is VERIFIED on the x86_64 `ForgeShape_16K` AVD (`PAGE_SIZE` 16384) and
  ARM64 is VERIFIED on a 4 KB-page phone, so each dimension is proven but not both
  at once. This needs 16 KB-page ARM64 hardware — a device-availability matter, not
  a code one; both `.so`s already carry 0x4000 ELF `LOAD` alignment.
- **Floating-point rounding is ABI-dependent, so exact FP comparisons in geometry
  code are an ARM64 hazard.** No `-ffp-contract=off` is set, so Clang contracts
  multiply-adds into `fmadd` on arm64-v8a where baseline x86-64 cannot — the direct
  cause of the shared-edge picking defect Gate P1 found. Anything comparing a
  computed geometric quantity against an exact bound should be assumed to differ
  between the emulator and a real phone until measured on both.
- **The physical phone drops self-test lines from the logcat ring buffer, and no
  capture method fully prevents it.** It caps the buffer at 5 MiB (`logcat -G 16M`
  is silently reduced) and whole suites vanish from the *middle* of a capture,
  which reads exactly like a suite that never ran. Confirmed across four methods
  during Gate P1, each dropping a *different* subset. Read a partial capture as "no
  failures observed" and re-run until one is complete before quoting a total.
- **The `EditorWorkspaceView` layout decision runs inside `onMeasure`.** That is
  deliberate and documented — running it in `onSizeChanged` measures newly added
  chrome against the previous pass and lays it out at zero height, which is exactly
  the bug that was hit and fixed — but mutating the view tree during measure is
  unusual enough to be worth naming. It is idempotent and converges in one
  traversal. It is also why the UI-R1C2 Objects re-parent is instant rather than
  animated: starting an animation from a measure pass reintroduces the same defect.
- **A horizontal drag along the Property Inspector header toggles it.** The header
  is a full-width clickable row and Android's default click detection fires on
  `ACTION_UP` while the pointer is still inside the view's bounds, however far it
  travelled. Harmless — the gesture never reaches the viewport — but it reads as a
  stray toggle. Not fixed, to avoid hand-written touch handling on a surface whose
  touch-opacity is load-bearing.
- **Test-harness note, not a product defect:** a stroke driven at the centre of a
  face of a frozen **box** logs `STROKE_PENDING` → `STROKE_ABANDONED:navigation`
  and never promotes, because a box has 8 vertices, all at its corners, and a brush
  that would capture no vertex starts no stroke. Drive evidence strokes on a dense
  primitive (sphere 482 v, capsule 514 v) or widen the radius.
- **A viewport tap while the soft keyboard is up** must land clear of the Property
  Inspector and above the keyboard or it never reaches the `SurfaceView`; focus
  stays in the text field and a following `keyevent` types a digit instead. Looks
  exactly like a command that did not run.
- **`uiautomator dump` omits inspector content scrolled out of view**, so a script
  that looks for `apply_shape` without scrolling the inspector first gets `MISSING`
  and reads like a lost control. Scroll, then resolve the id.
- **`connectedDebugAndroidTest` uninstalls the app when it finishes**, and it has no
  `-s <serial>` equivalent. Reinstall before taking runtime evidence.
- **The default logcat ring buffer drops part of the startup self-test output.** Run
  `adb -s <serial> logcat -G 16M` first.
- **The Android emulator dies if launched as a child of a tool shell**; spawn it
  detached.
- **PowerShell `>` redirection corrupts binary output.** `adb exec-out screencap -p
  > file.png` writes a BOM and re-encodes, producing an invalid PNG; use
  `adb shell screencap -p /sdcard/x.png` followed by `adb pull`.
- Android input resampling can emit a one-finger MOVE between `ACTION_DOWN` and
  `ACTION_POINTER_DOWN`, so starting a two-finger gesture may orbit by a few pixels
  first. Correct for the contract, not a defect.
- The path-driven tools deposit in proportion to pointer travel measured in brush
  radii, so a **short stroke with a large brush** does very little — the one thing
  that reads as "the brush did nothing" on a first try.
- The arbitration's remaining deliberate seam: a first finger that **drags more than
  8 px** before a second one lands does commit a stroke. That is a gesture the user
  drove as a stroke; closing it completely needs a buffered or undoable stroke,
  which does not exist.
- On some emulator sessions `screencap` output can acquire a uniform
  whole-framebuffer black-level lift partway through. It is a system/compositing
  effect, not a rendering change, but it means screenshots from different points in
  a session may not compare.

## Technical Debt

Durable constraints and known-but-accepted costs. Narrative for how each was
found lives in Git history.

**Selection composition.** Selection is composed as a lerp toward a flat colour
rather than as a per-channel gain on the shaded colour
(`shaded * mix(vec3(1.0), tint, a)`), which would preserve face-to-face luminance
ratios exactly. At the resting 0.20 the flattening is about a fifth of what the
old permanent 0.55 cost, so the gain formulation is now an optional refinement
rather than a fix. Selection is still a whole-object **tint** rather than an
outline; the outline is the expensive half, needs either a second geometry pass
or a screen-space edge filter, and remains an owner decision. The readability
enhancement (outline or cavity) was **explicitly deferred**: both candidates start
the post-processing framework the shading stage was told not to build, and the
vertex-based alternative would expose triangle structure in Smooth mode.

**Documentation size.** Every core document is inside the 2000-line hard cap after
DOC-R1, and `ARCHITECTURE.md` has a working margin again (1915). It is still over
its 700–1000 preferred target and this file is still over its 500–800 one; both
are dense ownership statements rather than narrative, so further reduction means
retiring facts, not trimming prose. A stage that adds architecture should retire
superseded prose in the same pass rather than appending. **Always re-measure
before quoting a count** — recorded numbers have been stale in both directions.

**`scripts\run-instrumented-tests.ps1` aborts when javac emits a note.** It runs
under `$ErrorActionPreference = 'Stop'`, and Windows PowerShell 5.1 wraps a
native command's stderr in a `NativeCommandError`, so the first run after any
source change fails before reaching the device on nothing worse than "Note: Some
input files use or override a deprecated API". The workaround is to build first
(`gradlew :app:assembleDebug :app:assembleDebugAndroidTest`) and then run the
script. Not fixed here because the script is what `DEV2`/`DEV3` verify
mechanically, and changing its error handling is a device-safety change that
deserves its own attention.

**The Android layer still calls deprecated platform APIs.**
`setSystemUiVisibility` in `ForgeShapeActivity` and the `getSystemWindowInset*`
accessors in `EditorWorkspaceView` are inside `SDK_INT` branches for API 26–30,
which is correct, and are what produces the javac note above.

**"Debug-only" code is proven debug-*guarded*, not proven absent from a release
binary.** Every self-test and mesh-fixture entry point is behind `#ifndef NDEBUG`
or an equivalent guard, verified by reading the call sites. But those translation
units sit on the CMake source list unconditionally and no release `.so` has ever
been inspected to confirm the linker drops the symbols. Closing this means
examining a real release artifact and possibly moving the files behind a CMake
condition.

**Reduced motion is pushed on refresh, not observed.** `syncFromNative` reads
`ANIMATOR_DURATION_SCALE` and hands the answer to native code, covering every
resume and state change. A user who changes the setting while ForgeShape is in
the foreground and then immediately selects a body can get one pulse decided by
the previous value. Closing it needs a `ContentObserver` or a read on the pointer
path, and neither is worth putting there for one frame of a 220 ms decay.

**Sculpt cost model.** Sculpt publication is synchronous and republishes the whole
mesh per move — O(vertices) regardless of how few the brush touched — and
Inflate's normal recompute is O(triangles) per move. Measured free at 482–514
vertices (1,170 uploads, no stall, no growth); that measurement is the baseline a
future partial or asynchronous path must beat. The affected set is found by a
linear scan at stroke start, and picking is a linear scan too: both want the same
missing spatial acceleration.

**Sculpt state and feel.** Process-scoped with no save, load or undo, so a stroke
is unrecoverable the moment it lands and Freeze silently discards the previous
sculpt (the panel says so; the honest fix is undo). The path-driven gain constants
(`kNormalBrushGain` 0.35, `kSmoothGain` 1.0, `kMaxSmoothLambda` 0.9) are chosen,
not derived, and have never been tuned against a real modelling session; if brush
feel is tuned they should move together.
`FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation` names two different causes — a
gesture that really became navigation, and a promoted stroke that captured no
vertex — and the log cannot tell them apart; the fix is a second token. Freeze
regenerates the Construction mesh to copy it rather than copying the store's
current revision: correct (the store may hold a debug fixture) and cheap, but a
second generation of geometry that already exists. The Sculpt panel's status line
does not update during a stroke, because a live readout would need a native→Java
notification that does not exist.

**Primitive surface.** Adding a primitive still costs four parallel edits — a
member on `ConstructionObject`, a `PrimitiveKind` case, a variant alternative, and
a JNI method plus its Java declaration — deliberately visible rather than hidden
behind a registry. `ConstructionObject::setPrimitive`'s update half `std::visit`s
the requested payload, so a kind added with no matching overload is a compile
error; the plain `switch (kind_)` statements elsewhere (`spec()`,
`generateMesh()`, `primitiveKindName`) were left as they were, and Stage 016 found
one real instance of exactly that risk in a `describeSpec` if-else chain with no
Plane branch. A future stage that wants those closed has a proven pattern to
reuse. Every primitive's parameters stay resident even though one is active.
Tessellation is a compile-time constant, so a very large curved primitive shows
its facets; relatedly the capsule's cylindrical middle carries no interior rings
however long it is, which makes sculpt fidelity there coarse. Capsule topology is
a function of its parameters (514 : 3072, or 482 : 2880 at equality) and is the
only case where a shape edit can change a vertex count and trigger a buffer
growth.

**Android layer.** The workspace builds its view trees in code — colours,
dimensions, strings and ids are resources, but there are no XML layouts, so the
structure is only readable by reading Java. The Property Inspector has two
detents rather than three. Chrome regions are placed by nested `LinearLayout`
weights rather than a constraint solver, which is why the rail sits in a
`ScrollView` instead of being able to compress. There is no focus-on-selection
and no camera framing helper. A field keeps focus after a rejected Apply and keeps
swallowing hardware/`adb` number keys until the viewport is touched. The inspector
refreshes from native truth on resume and after any Apply, discarding a half-typed
edit — deliberate, but a future edit-session or undo feature needs a model for it.
`LengthUnit.format` strips trailing zeros, so 2.0 m displays as `2`. A **compact**
window drops the rail's icons and keeps only its labels, so the rail reads
differently in a short landscape window than anywhere else. **The primitive
chooser's three-column chips clip their labels in a narrow docked inspector**
("Cylinder" → "Cyli"): pre-existing, a function of `CHOOSER_COLUMNS = 3` against
`sideDockWidthDp`, and visible in any expanded window.

**Android test infrastructure.** `androidTest` is the project's only AndroidX and
`android.useAndroidX=true` is set for it — a build-configuration change that adds
nothing to the product APK, which still has no runtime dependency of any kind. The
instrumented suites share one process, so native state (in particular *whether
anything has ever been frozen*, and how many bodies exist) carries across tests;
no instrumented test may assume a body count, which body is at the origin, or
which mode is current. There is no camera read-back across JNI, so "a chrome
gesture did not move the camera" is proven by runtime screenshot rather than by
assertion.

**Transform and math.** `modelMatrix()` / `inverseModelMatrix()` recompute six trig
calls per frame and per pick for a value that only changes on Apply. The transform
is rigid by design: the exact composed inverse, picking's "local distance is world
distance" shortcut and the renderer's plain-matrix normal transform all depend on
it, so adding scale is a domain change rather than a matrix change. A large
translation makes the float round-trip residual grow linearly with `|p|` (about
`|p| * 2^-23`); at kilometre scale the derived `float` matrix, not the `double`
domain, is the precision limit.

**Mesh and renderer.** Each body owns a `MeshStore` and the renderer keeps a
`BodyRenderResources` per body, so a scene of N bodies is N independent
publication chains and N buffer pairs — correct, and deliberately not pooled or
batched. `MeshStore::publish` validates the data twice. Retired GPU buffers are
freed inline after the fence wait rather than through a deferred-destruction
queue, which is what makes the synchronous wait necessary. **One redundant
swapchain rebuild occurs at startup and again on each resume or rotation**,
because `surfaceChanged` arrives immediately after `surfaceCreated` (cosmetic;
observed twice per rotation in the UI-R1C2 walkthrough). The identity-pre-transform
orientation convention costs one compositor rotation while the display is rotated
— the same cost every non-pre-rotated Android application pays — but on a tiled
mobile GPU true pre-rotation is cheaper, so this is the one renderer decision a
future performance stage might revisit; it would have to move the extent, the
clip-space rotation and the camera aspect together. `createSwapchain`'s fallback
for a presentation engine that does **not** support an identity transform is
**UNVERIFIED**: `emulator-5558` reports `supportedTransforms=0x1ff`, so the branch
has never executed, and its extent swap for 90/270 is reasoned from the
pre-transform contract rather than measured. Carried and untouched: static viewport
and scissor, no `oldSwapchain` handling, no validation layers, and a single global
viewport.

**Shading and render data.** The crease policy is a single global angle — correct
for every primitive ForgeShape has, but a *policy*, not a per-object property; a
future imported or sketched body wanting a different threshold has nowhere to say
so. Render data is rebuilt whole on every accepted change including every sculpt
move (0.56–0.73 ms for a 482-vertex mesh, free at these sizes); both it and the
sculpt publication it rides on want the same missing partial-update path. The
renderer keeps a per-frame gate *and* `RenderMeshCache` keeps its own, so the
cache's skipped-refresh counter reads zero forever in production — real
duplication, deliberately not logged. Colour still travels in `RenderVertex`
purely so the debug source-colour mode has something to draw, costing 12 bytes per
render vertex; the generators' per-vertex rainbow is likewise dead data on the
product path, retained so the before/after comparison stays one tap away. Studio
Solid, the MatCap and the grid palette are authored **directly in display space**,
because the swapchain is `R8G8B8A8_UNORM` and every colour in ForgeShape already
is; there is no linear workflow, and introducing one is a PBR-stage decision that
has to move all of them at once. The MatCap is regenerated from scratch on every
device creation rather than cached — 64 KiB of trivial arithmetic, never measured,
but work repeated for a constant.

**Grid.** The extent is a fixed 20 m, so zooming far out leaves the model on a
small patch of floor and zooming far in shows one cell; that is the deliberate
alternative to a multi-decade CAD grid and is the first thing a future stage
should revisit if the fixed extent is felt. The fade constants and the four
palettes are chosen, not derived. The grid is the one **blended** pipeline in the
renderer, so it is also the one place a future depth-sorted or translucent
feature would find an existing precedent — that is not an invitation to reuse it
as a general overlay path.

**Camera and projection.** The orthographic view plane sits a fixed `kFarPlane/2`
in front of the target, which makes `snapshot.eye` mean something different in the
two modes and makes a reported orthographic pick distance ~250 m rather than a
distance from the orbit eye; the field is only ever used as a ray origin and a view
reference, so this is correct, but a future stage adding a second camera should
rename it. The ortho depth slab is a fixed 500 m rather than fitted to the scene.
`kInitialOrthoHalfHeightMeters` is a literal because `std::tan` is not `constexpr`;
a self-test asserts it still equals `kInitialDistance × tan(fovY/2)`. The camera
still has no read-back of pose across JNI.

**Naming.** `kConstructionBoxObjectId` and `kDemoCubeObjectId` are the same value
under two names and are both misnamed: the object is not always a box and never
was a demo cube. Renaming is deferred to avoid churning unrelated code. There is
still no checked-in `uinput` harness file, so the multi-touch gesture is
regenerated per stage.

## Current Files / Modules

| Path | Ownership |
| --- | --- |
| `settings.gradle`, `build.gradle`, `gradle.properties`, `local.properties` | Gradle project config |
| `gradlew(.bat)`, `gradle/wrapper/*` | Gradle 8.14.3 wrapper |
| `app/build.gradle` | Android app module config, SDK/NDK/CMake/ABI pinning |
| `app/src/main/AndroidManifest.xml` | App/activity declaration, Vulkan feature requirement |
| `app/src/main/java/.../ForgeShapeActivity.java` | Android lifecycle, edge-to-edge window, resume refresh, DEBUG key hook |
| `app/src/main/java/.../ForgeShapeSurfaceView.java` | Viewport surface, forwards lifecycle + raw per-pointer state (id, position, tool type, pressure, tilt), takes focus back from an editor |
| `app/src/main/java/.../PointerSemantics.java` | The ONE place an Android `MotionEvent.TOOL_TYPE_*` constant becomes a neutral wire code, plus that mapping's Unknown fallback |
| `app/src/main/java/.../EditorWorkspaceView.java` | The whole editor UI: region composition, adaptive layout, window insets, chrome visibility, mode/tool wiring, and `syncFromNative()`. Owns no product state |
| `app/src/main/java/.../WorkspaceLayoutMode.java` | Window-dp breakpoints, inspector placement and chrome sizing, as arithmetic. No Android type |
| `app/src/main/java/.../EditorUiState.java` | The closed list of UI-owned state: display unit, draft kind, rail selection, detent per mode, chrome-hidden |
| `app/src/main/java/.../GlobalToolbarView.java` | Editing context, the three mutually exclusive mode transitions, reserved Export, the Display control, chrome hide, and the one status message |
| `app/src/main/java/.../DisplaySettingsPopoverView.java` | The compact display popover: Shading (Studio / MatCap / Debug), Surface (Smooth / Faceted) and Projection (Perspective / Orthographic), with short interruptible open/close motion that honours the system animator scale. Owns no state |
| `app/src/main/java/.../ToolRailView.java` | The edge tool selector for either mode, including reserved entries. Selects; decides nothing |
| `app/src/main/java/.../BrushEdgeControlsView.java`, `VerticalSliderView.java` | Direct Radius and Strength, and the custom vertical control behind them. Own no brush value |
| `app/src/main/java/.../PropertyInspectorView.java` | Contextual, collapsible, scrolling container with a measured height cap. Owns no value |
| `app/src/main/java/.../ConstructionShapeEditorView.java` | Primitive chooser, that primitive's exact fields, unit chips, Apply Shape. Owns field text and a DRAFT kind only |
| `app/src/main/java/.../ConstructionPlacementEditorView.java` | Position/rotation fields, unit chips, Apply Transform. Owns field text only |
| `app/src/main/java/.../SculptContextView.java` | Frozen-mesh summary, stale-source warning, and the guarded re-Freeze |
| `app/src/main/java/.../InspectorHost.java`, `NumericPropertyRow.java`, `UnitChipsView.java`, `EditorControlStyles.java` | The four small shared pieces: what a body may ask of the workspace, one labelled exact field, the mm/cm/m selector, and the one place controls get their look |
| `app/src/main/java/.../LengthUnit.java` | Exact `BigDecimal` mm/cm/m ↔ meter conversion, parsing and formatting |
| `app/src/main/java/.../StartChooserView.java` | The New Project question: two ways to begin, over the live viewport. Owns no state, makes no native call |
| `app/src/main/res/values/*` | `ids.xml` (the stable semantic id contract), `dimens.xml` (radius/type/depth scales), `colors.xml` (role names, dark values), `strings.xml`, `themes.xml` (edge-to-edge) |
| `app/src/main/java/.../AppTheme.java` | The two appearances: the Android style each applies, and the viewport appearance each hands to native code |
| `app/src/main/java/.../ChromeMotion.java` | The four rules every chrome transition follows: the two durations, the reduced-motion question, cancel-first, and one alpha helper. Not a framework and must not become one |
| `app/src/main/res/values/attrs.xml`, `themes.xml` | The semantic roles, and the one place each is given a value per theme. Adding a theme touches these two files and nothing else |
| `app/src/main/res/drawable/*` | 15 icon vector drawables on one 24 dp grid, plus the `bg_*` background state lists every control's look comes from, all written in `?attr/fs*` |
| `app/src/main/res/color/*` | `control_content_tint.xml` — the one state list an icon and its label both read, so they cannot disagree |
| `app/src/test/java/...` | JVM suites: layout arithmetic, UI-owned state, unit conversion |
| `app/src/androidTest/java/...` | Instrumented Editor Workspace suites plus `WorkspaceTestSupport` (native snapshots, drag consumption, exact chrome-union viewport measurement) |
| `app/src/main/java/.../NativeViewport.java` | JNI declarations, library load, `APPLY_*` / `SCULPT_*` status codes, `MODE_*`, `TOOL_*`, `POINTER_SAMPLE_*` slots and the DEBUG-only `debugLastPointerEvent` observation seam |
| `app/src/main/cpp/forgeshape_jni.cpp` | JNI boundary, render thread, `ANativeWindow`, MotionEvent→`TouchAction`, pointer sanitization and unpacking, camera + selection locking, stroke arbitration, `publishActiveRepresentation` |
| `app/src/main/cpp/forgeshape_input.{h,cpp}` | Platform-neutral pointer event data: `TouchAction`, `PointerToolType`, `TouchPointer` (id, position, tool type, pressure, tilt), and THE range/wrap/non-finite sanitizers those fields are defined by |
| `app/src/main/cpp/forgeshape_camera.{h,cpp}` | Camera pose, the `ProjectionMode` enum and the orthographic world span, both projections, the framing-preserving switch, gesture state machine |
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all six primitives + transform), the six `Construction*` generators, shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation including `validateCapsuleMeters`, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
| `app/src/main/cpp/forgeshape_transform.{h,cpp}` | `ConstructionTransform`: authoritative double-meter position and double-degree rotation, THE axis/Euler convention, validation, atomic apply, derived model and inverse-model matrices |
| `app/src/main/cpp/forgeshape_sculpt.{h,cpp}` | `ProductMode`, `SculptTool`, `SculptSession` (mode + tool + brush + live stroke + the `hitsSculptMesh` probe), `SculptMesh`, `SculptTopology`, `computeVertexNormals`, `SculptStroke` (the one kernel plus one `apply*` per tool), sculpt publication |
| `app/src/main/cpp/forgeshape_picking.{h,cpp}` | Screen→world ray for **both** projections (perspective: one origin, fanning directions; orthographic: one direction, per-pixel origin), `transformRayToLocal`, ray/triangle, nearest hit, winding check |
| `app/src/main/cpp/forgeshape_selection.{h,cpp}` | `ObjectId`, `SelectionController`, tap-vs-navigation, `pickScene` |
| `app/src/main/cpp/forgeshape_object_id.h` | `ObjectId` type and reserved values, shared by the mesh and selection layers |
| `app/src/main/cpp/forgeshape_mesh.{h,cpp}` | `RuntimeMesh` (immutable revision), `MeshStore`, validation, capacity policy, upload diagnostics including source-vs-render counts |
| `app/src/main/cpp/forgeshape_render_mesh.{h,cpp}` | Derived render geometry: `RenderVertex` (position + normal + colour), `SurfaceShading`, THE crease policy (`kCreaseAngleDegrees`), per-vertex crease grouping with render-only duplication, and `RenderMeshCache`'s rebuild gate. Presentation only |
| `app/src/main/cpp/forgeshape_matcap.{h,cpp}` | The one ForgeShape-owned MatCap, computed at device init from the closed-form model in that file. No asset, no decoder, one preset |
| `app/src/main/cpp/forgeshape_display.{h,cpp}` | `ShadingModel`, `ViewportBackground`, grid visibility, the reduced-motion bool, the process-scoped `DisplaySettingsStore`, and the UI index mapping. Presentation state, never truth |
| `app/src/main/cpp/forgeshape_grid.{h,cpp}` | The world reference grid's CONTRACT: the XZ plane at y = 0, the 1 m / 5 m / 20 m spacing and extent, `GridLineTier`, one pure vertex generator and the per-appearance palette. No ObjectId, no revision, not in the scene, not pickable |
| `app/src/main/cpp/forgeshape_selection_pulse.{h,cpp}` | How a SELECTED body is drawn, never which one is: the peak, the resting alpha, the decay, and one pure function over an explicit frame delta. Holds no ObjectId and reads no clock |
| `app/src/main/cpp/forgeshape_renderer.{h,cpp}` | Vulkan renderer, frame loop, camera snapshot + model transform + selection highlight consumer |
| `app/src/main/cpp/forgeshape_math.h` | Minimal self-owned vec3/mat4. No GLM |
| `app/src/main/cpp/forgeshape_demo_mesh.{h,cpp}` | Baseline cube numbers; source data for the baseline debug fixture only |
| `app/src/main/cpp/forgeshape_mesh_fixtures.{h,cpp}` | DEBUG test fixtures (baseline / same-topology / larger / stress step) |
| `app/src/main/cpp/forgeshape_*_selftest.{h,cpp}` | The eleven debug-only deterministic suites: camera, picking, mesh, construction (box), transform, primitive, sphere, cone/capsule, sculpt brush kernel, render shading, scene |
| `app/src/main/cpp/shaders/surface.{vert,frag}` | GLSL source for the surface pipeline: view-space normals, Studio Solid, the MatCap lookup and the debug colour path. AOT compiled to SPIR-V by `glslc` in CMake |
| `app/src/main/cpp/shaders/grid.{vert,frag}` | GLSL source for the grid pipeline: world→clip with no model matrix, the tier→colour choice, the depth nudge that settles the coplanar Plane, and the PER-FRAGMENT radial fade |
| `app/src/main/cpp/CMakeLists.txt` | Native build + glslc shader step |
| `artifacts/` | Runtime evidence screenshots from accepted stages, plus `stage015c_shading_comparison.md`, the Stage 015C comparison sheet |
| `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md` | Stage 015A-R UI proposal. **Proposal only** — the Owner Decision Baseline above is authoritative, not the pack |
| `README.md`, `ARCHITECTURE.md`, `PRODUCT.md`, `CLAUDE.md` | See the ownership table in `CLAUDE.md` |

## Shading cost record

Not a benchmark stage; these are the numbers a future change has to beat, taken
from `FORGESHAPE_RENDER_MESH_BUILD` on `emulator-5558`.

| Mesh | Source (v:i) | Render (v:i) | Rebuild |
| --- | --- | --- | --- |
| Box | 8:36 | 24:36 | 0.03 ms |
| Cylinder | 66:384 | 130:384 | 0.55 ms |
| Sphere | 482:2880 | 482:2880 | 0.66–1.25 ms |
| Cone | 34:192 | 66:192 | 0.07 ms |
| Capsule | 514:3072 | 514:3072 | 0.78 ms |
| Sphere, Faceted | 482:2880 | 2880:2880 | 0.30 ms |
| Sculpt mesh, per accepted move | 482:2880 | 491–492:2880 | 0.56–0.73 ms |

A rebuild happens per accepted geometry change, not per frame, so none of this is
on the frame budget: 4448 frames cost 2 rebuilds. **MatCap costs no more than
Studio Solid** and if anything less — it is one texture fetch against Studio's two
Lambert terms, a hemisphere lerp, a Blinn-Phong power and a rim power — and both
remained normally interactive throughout, with orbiting, sculpting and rotation
indistinguishable from the pre-stage build by eye. No frame-time instrumentation
was added and no marketing claim is made.

## Next Stage

**OWNER DECISION — schedule Selection Outline, or choose the next roadmap stage.**

DOC-R1 closed the documentation-size blocker and with it UI-R2. No implementation
stage is currently scheduled, and none may be invented here.

The only feature the repo currently records as a candidate is **Selection
Outline** — the expensive half of selection feedback, needing either a second
geometry pass or a screen-space edge filter. It is a *candidate awaiting owner
decision*, not an approved stage: today's whole-object tint is the shipped
behaviour, and the readability enhancement (outline or cavity) was explicitly
deferred because both candidates start the post-processing framework the shading
stage was told not to build. Nothing may be built against it until the owner
schedules it.

**Still out** and unchanged: snap-to-grid and the Sketch grid, a different
contract from the world reference grid; a View Cube, camera focus or named views;
blur or glass of any kind; a post-processing framework; persistence; an automatic
system theme; hierarchy and object commands; Sketch/Extrude; Undo;
pressure-driven sculpting; and import/export.
