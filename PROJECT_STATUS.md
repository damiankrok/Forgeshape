# ForgeShape — Project Status

**Status Version:** 0.19.0
**Updated:** 2026-08-21
**Result:** COMPLETE
**Current Phase:** Phase 1 — Native Viewport
**Workspace:** `D:\TRAVELAPPS\ForgeShape`
**Accepted implementation baseline:** Stage 015C-R — viewport surface readability
(inverted front-face culling fixed), on top of Stage 015C (shading), Platform Fix
P2 (rotated-landscape renderer orientation), Stage 015B (Editor Workspace),
Stage 014, the NDK r29 migration (Gate P0) and the owner decision baseline
**Next Stage:** Stage 016 — Plane + Primitive Coverage Cleanup

This is a current snapshot, not a chronology. Per-stage verification chapters,
superseded environment states and old next-stage recommendations live in Git
history and are deliberately not repeated here.

Stage 015C-R fixed the defect that made every convex primitive read as a hollow
interior: the graphics pipeline named `VK_FRONT_FACE_CLOCKWISE`, which inverted
back-face culling, so the viewport drew each solid's **far** walls instead of its
near ones. It also adds a ten-part direction test family (`NOR-01`..`NOR-10`) to
the render-shading suite, because every check that existed measured which axis a
normal lay on and none measured which way it pointed.

Stage 015C before it replaced the debug-looking per-vertex rainbow with readable
lit geometry: a **derived, render-only** normal layer between the authoritative
mesh and Vulkan, one centralized crease policy, two shading models (Studio Solid
and MatCap), Smooth/Faceted display, a compact display control in the Global
Toolbar, and a tenth native self-test suite. No Construction, sculpt, picking,
camera or transform behaviour changed in either stage. Platform Fix P2 closed the
rotated-landscape rendering defect, and Stage 015B replaced the two provisional
Android panels with the approved responsive **Editor Workspace**.

## Stage 015C-R — root cause, convention and evidence

**Root cause, proven by measurement rather than inspection.** With
`cullMode = BACK` and `frontFace = CLOCKWISE`, the three faces visible on the
default box measured luminance **0.3874 / 0.3331 / 0.2639**. Those are the
computed Studio Solid values for the **−X, −Z and −Y** faces (0.3864 / 0.3335 /
0.2647) — the three that face away from the camera. The three that should have
been visible are **+Y, +Z, +X** at 0.8324 / 0.5587 / 0.3095. For a closed convex
solid a far face can only reach a pixel if the near face was culled, so culling
was inverted. After the fix the same pixels measure **0.8327 and 0.5588**, which
match the +Y and +Z predictions to four decimals.

**Why it survived Stage 015C review.** Inverted culling does not blank the
viewport and does not change the silhouette — a closed solid fills exactly the
same outline either way. It only swaps which surface of that outline is drawn, so
it presents as a *shading* complaint ("the box looks concave") rather than as a
rasterizer defect, and it sends the investigation into the light rig.

**The obligatory winding / normal / raster convention, stated once.**

| Layer | Rule |
| --- | --- |
| Source triangles | counter-clockwise seen from **outside** the solid, right-handed world space |
| Geometric normal | `N = (v1 − v0) × (v2 − v0)`, points away from the solid |
| Render normals | derived one-way from a published `RuntimeMesh`; outward everywhere |
| Model → view normals | upper-left 3×3 of `view * model`, valid because both factors are rigid |
| Projection | `forgeshape_math.h` flips Y **in the matrix** for Vulkan clip space |
| Pipeline | `cullMode = VK_CULL_MODE_BACK_BIT`, `frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE` |
| Picking | front faces only, same rule, so what is pickable is what is drawn |

The projection's Y flip is **already applied** by the time Vulkan classifies a
triangle. Compensating for it a second time in `frontFace` is the mistake this
stage removed.

**Hypotheses tested and rejected**, each by a check that would have failed:
source winding wrong (`NOR-01`), render normal generation wrong or sign-flipped
(`NOR-02`..`NOR-07`), hard-edge duplication reordering a triangle (`NOR-08`),
model→view normal transform losing a sign or needing an inverse-transpose
(`NOR-09`), display modes mutating source truth or picking (`NOR-10`), and the
Studio light rig being internally inconsistent — its computed face values match
the rendered pixels to four decimals in **both** the broken and the fixed build,
which is what proves the rig was never the defect. **No geometry, no normal
generation and no light constant was changed.**

**Direction tests (`NOR-01`..`NOR-10`), all green.** They live in the
render-shading suite, which grew 158 → 207 checks. The family exists because
every pre-existing normal check measured an axis or a magnitude and therefore
passed unchanged on a mesh whose normals had all been negated;
`nor_outwardness_fails_on_global_normal_flip` asserts on all five primitives that
the new measurement does invert, so the suite cannot regress into that blind spot
again.

**Verification.** Ten self-test suites green (**1199 checks, zero failures**);
26 JVM tests green; **40 instrumented tests green, zero failures** — including
the IME test that was previously recorded as a pre-existing failure, so that
entry is retired. Runtime on `emulator-5558`: Box / Cylinder / Cone / Capsule
applied through the real touch path with the native `CONSTRUCTION_PUBLISHED` line
confirming the primitive before every capture, picking (`PICK_HIT` on a
near-side, front-facing point), Freeze, a real 11-move Grab stroke, Studio,
MatCap, Smooth, Faceted, HOME/resume and rotated landscape (`2400x1080`, aspect
2.2222, one swapchain rebuild).

**Performance.** A stationary run of **3371 presented frames** (frame 18989 →
22360) moved the render-data rebuild count **61 → 62**, and that single rebuild
is the Smooth toggle used to close the measurement. Zero rebuilds while
stationary, no revision minted by any display change, no buffer growth and no
swapchain churn. The fix is one pipeline enumerator and costs nothing.

**Visual evidence**, under `artifacts/stage015cr_*` (`emulator-5558`, foreground
confirmed before each capture): the before/after pair on the same default box
(`_00_box_studio_smooth_before` vs `_01_box_studio_smooth_after`), then Box
MatCap Smooth, Box Studio Faceted, Cylinder Studio Smooth, Cone Studio Smooth
after an explicit Apply, Capsule MatCap, the sculpted mesh after a real Grab, and
rotated landscape.

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

**DOC-OWNER-01 in detail.** The rule itself lives in `CLAUDE.md`; the fixed UI
vocabulary is *Editor Workspace*, *Global Toolbar*, *Tool Rail*, *Property
Inspector*, *Construction Body*, *Frozen Sculpt Mesh*. Existing working code is
not mass-renamed for it.

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
| ABI filter | `x86_64` only |
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
| `ForgeShape_Stage006` / `emulator-5558` | **Current ForgeShape-owned evidence target.** Isolated, own AVD definition and data dir. |

`emulator-5558` is pixel_6, 1080×2400, density 420, multi-touch, GPU host,
`x86_64`, API 36 (`google_apis_playstore`). Vulkan: loader instance 1.4.0,
physical device "Goldfish GFXStream (AMD Radeon RX 9070 XT)", device API 1.3.0,
swapchain format 37 (`R8G8B8A8_UNORM`), 4 images, FIFO.

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

## Architecture

`ARCHITECTURE.md` is the authoritative ownership map. In one paragraph: a
standalone Android application (`com.forgeshape.app`) with a Java shell that owns
no domain truth, a plain `SurfaceView` viewport, a JNI boundary that carries
whole sections and semantic pointer samples, and a platform-neutral C++17 domain
(`ConstructionObject` and its five primitives, `ConstructionTransform`,
`SculptSession` and its one brush kernel, `MeshStore`, picking, selection,
camera) beneath a native Vulkan renderer that owns no geometry truth. No Compose,
no AndroidX, no third-party runtime library, no engine.

## Verified capability matrix

Everything below is verified at runtime on `emulator-5558` through the real
Android touch path. `PRODUCT.md` owns the user-facing description.

| Capability | State |
| --- | --- |
| Vulkan viewport, swapchain, depth, pipeline, indexed draw, surface recreation | VERIFIED |
| Camera: orbit / pan / pinch, pointer-set re-anchoring, cancel handling | VERIFIED |
| Tap-to-select, tap-to-clear, drag and multi-touch never select | VERIFIED |
| CPU picking follows camera, dimensions, transform and sculpt deformation | VERIFIED |
| Dynamic mesh: immutable revisions, fail-closed validation, capacity reuse/growth | VERIFIED |
| One Construction Body with exact Box / Cylinder / Sphere / Cone / Capsule | VERIFIED |
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
| Editor Workspace: Global Toolbar, Tool Rail, Property Inspector, direct brush controls | VERIFIED |
| Adaptive layout: compact portrait, phone landscape, expanded/tablet, decided by window dp | VERIFIED |
| Landscape occlusion: 0 % unoccluded viewport → **60.1 %**, status line on screen | VERIFIED |
| Inspector collapse and chrome hide restore viewport area (57.5 % → 82.6 % → 100 %) | VERIFIED |
| Edge-to-edge with WindowInsets on chrome only; IME never resizes the Vulkan surface | VERIFIED |
| Every chrome surface consumes its own gesture; viewport pixel-identical across chrome drags | VERIFIED |
| Freeze / Resume wording follows whether a Frozen Sculpt Mesh exists | VERIFIED |
| Destructive re-Freeze confirms only when strokes would be discarded; Cancel is inert | VERIFIED |
| Stable semantic ids on every control; 32 instrumented + 22 JVM tests | VERIFIED |
| Correct geometric proportions in portrait, physical 90° landscape and a non-rotated wide window | VERIFIED |
| One orientation convention: identity pre-transform, swapchain image = window | VERIFIED |
| Rotation mutates no Construction or Sculpt state and triggers no mesh upload | VERIFIED |
| Studio Solid replaces the per-vertex rainbow as the default appearance | VERIFIED |
| Derived render-only normals; source RuntimeMesh and picking untouched | VERIFIED |
| One 40° crease policy gives all five primitives their hard/smooth contracts | VERIFIED |
| Smooth ↔ Faceted is presentation only, on the same source revision | VERIFIED |
| ForgeShape-generated MatCap from view-space normals; one preset, no asset file | VERIFIED |
| Studio ↔ MatCap rebuilds no geometry and uploads nothing | VERIFIED |
| Sculpt deformation relights immediately; no stale lighting, no NaN | VERIFIED |
| Selection stays obvious and form stays readable in both shading modes | VERIFIED |
| Display settings are native-owned and survive HOME/resume | VERIFIED |
| No per-frame normal or render-data rebuild: 4448 frames, 2 rebuilds | VERIFIED |
| 16 KB page-size runtime behaviour | **UNVERIFIED** — see Known Issues |

## Self-test suite

Ten debug-only native suites run once from `NativeViewport.start()` — never per
frame — and total **1199 checks, zero failures** at the accepted baseline under
NDK r29:

| suite token | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 38 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 94 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 94 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 75 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 232 |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 207 |

followed by `FORGESHAPE_MESH_UPLOAD_OK` and `FORGESHAPE_NATIVE_VIEWPORT_OK`.
The tenth suite covers the crease policy, all five primitives' Smooth contracts
and the capsule equality case, Faceted, NaN/Inf and fail-closed behaviour,
determinism, the render-data rebuild policy, the generated MatCap asset, the
display settings, and the proof that building render data leaves the
authoritative `RuntimeMesh` bit-identical.

Stage 015C-R added the **direction family** to it, `NOR-01`..`NOR-10`: source
winding on all five primitives, outward render normals per primitive, Faceted
orientation against the source triangle, duplication-preserves-winding, the
model→view normal transform under six representative placements, and display
modes proven inert against picking and revisions. Its guard check
`nor_outwardness_fails_on_global_normal_flip` asserts the measurement inverts
under a global `normal *= -1`, which is the property the rest of the suite
lacked.

Three further non-per-frame diagnostics exist. `FORGESHAPE_SURFACE_CONFIG` (one
per swapchain creation) and `FORGESHAPE_CAMERA_VIEWPORT` (one per
`surfaceChanged`) audit the orientation chain; `FORGESHAPE_RENDER_MESH_BUILD` (one
per accepted geometry or surface-shading change) reports source vs render counts,
the rebuild count and the frame counter. `README.md` documents how to read all
three.

## Android UI suites

Added by Stage 015B; the first automated tests the Android layer has ever had.
Build and verification commands are in `README.md`.

| suite | scope | tests |
| --- | --- | --- |
| `WorkspaceLayoutModeTest` (JVM) | breakpoints, placement, chrome sizing arithmetic | 9 |
| `EditorUiStateTest` (JVM) | what the UI may remember, and what it refuses | 8 |
| `LengthUnitTest` (JVM) | exact mm/cm/m round-tripping and parse refusal | 5 |
| `EditorWorkspaceControlsTest` | UI-01/02/03/04/05/06/13 — control sets, fields, validation, freeze wording, tools, presentation-only actions | 17 |
| `EditorWorkspaceLayoutTest` | UI-07/08/09/12 — measured viewport floor, landscape, expanded, collapse | 6 |
| `EditorWorkspaceGestureTest` | UI-10/11 — chrome gesture ownership, IME | 5 |
| `EditorWorkspaceLifecycleTest` | UI-14 — HOME/resume rebuilt from native truth | 3 |
| `DisplaySettingsContractTest` (JVM) | the shading/surface index contract across JNI | 4 |
| `EditorWorkspaceDisplayTest` | SHD-13/15/16 — display ids, presentation-only, resume | 8 |

**66 tests** (26 JVM, 40 instrumented). No Java test asserts a rendered pixel;
every control is reached by its stable semantic id and no assertion uses a screen
coordinate.

**All 40 instrumented tests pass as of Stage 015C-R** on `emulator-5558`.

One of them is **environment-dependent, in both directions**, and that is worth
keeping on the record rather than deleting now that it is green.
`EditorWorkspaceGestureTest.ui11_theImeLeavesTheFieldAndTheCommitPathUsableAndTheSurfaceUntouched`
opens with a precondition guard — "the soft keyboard did not appear, so this case
proves nothing" — and at the Stage 015C baseline that guard failed, because no
soft keyboard showed on that emulator session. It appeared in this one and the
case passed on its merits. Nothing in the test or the product changed between the
two runs, so treat a future failure of this one case as a harness symptom to
confirm against the current baseline before calling it a regression.

## Current evidence summary

- **Stage 015C-R acceptance** (`emulator-5558`, clean install): ten self-test
  suites green (**1199 checks, zero failures**, including `NOR-01`..`NOR-10`);
  26 JVM tests green; **40 instrumented tests green, zero failures**. The
  measured before/after and the stationary-frame proof are in the Stage 015C-R
  chapter at the top of this file. **The default appearance changed again**: a
  convex primitive now shows its near faces, so the cold-start box reads as a
  solid with a bright top, a mid front and a dark end instead of as a hollow
  corner (`artifacts/stage015cr_00_box_studio_smooth_before` vs
  `artifacts/stage015cr_01_box_studio_smooth_after`).
- **Stage 015C acceptance** (`emulator-5558`, clean install, empty crash buffer):
  ten self-test suites green (1150 checks at that baseline, zero failures);
  26 JVM tests green; 40 instrumented tests with the one environment-dependent
  IME case described above. **The default appearance changed**: the per-vertex rainbow is gone and the
  viewport comes up in neutral Studio Solid, with the old appearance still
  reachable in a debuggable build as the Debug chip (the before/after pair is
  `stage015c_00` vs `stage015c_03`). **Render counts, measured through the real
  touch path** and matching the self-test's predictions exactly: box
  `8:36 → 24:36`, cylinder `66:384 → 130:384`, sphere `482:2880 → 482:2880`, cone
  `34:192 → 66:192`, capsule `514:3072 → 514:3072`, faceted sphere
  `482:2880 → 2880:2880`. A fully smooth closed surface has no crease to split
  on, so its render mesh is exactly its source topology. **The no-per-frame-
  rebuild proof is a direct measurement**: `rebuilds=2 frame=4448` — over 4448
  presented frames including eight camera-orbit gestures and three shading-model
  changes, the render mesh was rebuilt twice (once at startup, once for an
  applied sphere), and grepping the log during camera motion and Studio↔MatCap
  churn returns **zero** `RENDER_MESH_BUILD` and **zero** `MESH_UPLOAD_OK` lines.
  A Smooth↔Faceted round trip on **one unchanged source revision** moved the
  render count 24 → 36 → 24 with `src=8:36` throughout. A real 5-move Grab stroke
  on a frozen sphere relit the pulled lobe immediately with a visible crease and
  no stale shading; the render count drifted 482 → 492 as the deformation created
  genuine creases, and **every one of the 54 uploads in the session was `reuse`
  with the buffer-grow count flat**. Selection stays unmistakable and the form
  stays readable in both modes. In rotated landscape the orientation chain is
  unchanged from P2 (`chosenExtent=2400x1080`, `preTransform=0x1`, viewport
  2400×1080, aspect 2.2222), `viewport_surface` is still full-bleed at
  `[0,0][2400,1080]`, the sphere renders circular and rotation triggered no
  rebuild and no upload. Screenshots are under `artifacts/stage015c_*`, with
  `artifacts/stage015c_shading_comparison.md` as the comparison sheet.
- **Platform Fix P2 acceptance** (`emulator-5558`, cold boot, clean install,
  empty crash buffer): nine self-test suites green (992 checks, zero failures);
  54 Android tests green (22 JVM, 32 instrumented). **Root cause proven, not
  assumed.** The recorded chain in rotated landscape was window 2400×1080,
  `currentExtent` 2400×1080, `currentTransform` `0x2` (ROTATE_90),
  `supportedTransforms` `0x1ff`, chosen extent 2400×1080, `preTransform` `0x2`,
  camera viewport 2400×1080, aspect 2.2222. The first inconsistent convention was
  the **swapchain extent space**, not the projection: declaring a 90° pre-
  transform obliges the image to be in pre-transform (panel) space, so
  SurfaceFlinger rotated the 2400×1080 buffer to 1080×2400 and stretched it back
  to the window, scaling by (2400/1080, 1080/2400). That model predicts a
  2 × 1 × 0.5 m box at 427 × 98 px; it measured **428 × 98**, against 218 × 192
  when correct. The fix requests an identity pre-transform. Measured with the
  same object and camera in every configuration: portrait 484 × 427 px
  (ratio 1.1335), rotated landscape **218 × 192** (1.1354), non-rotated
  1280 × 800 wide window 162 × 142 (1.1408) — the pose-fixed invariant
  `bboxWidth / screenHeight` agreeing at 0.2017 / 0.2019 / 0.2025, inside 0.5 %.
  Portrait and the wide window are **pixel-identical to their pre-fix
  measurements**. A default sphere measures **269 × 269 px in portrait and
  121 × 121 px in rotated landscape — ratio exactly 1.0000 in both**, against a
  predicted 121.05. Two landscape↔portrait round trips restored portrait
  bit-identically with no stale extent and no crash. Picking stays aligned with
  what is drawn: in rotated landscape a tap at the rendered box silhouette's
  centre hits local z = 0.2500, exactly the 0.5 m box's face, and a tap at the
  transformed sphere's centre lands 0.4975 m from its centre against an exact
  0.5 m radius (0.5 % inside, the expected 482-vertex faceting inset). Five
  orientation changes plus a HOME/resume published **no mesh revision, triggered
  no upload and started no stroke**, and left the rendered sculpted mesh
  pixel-identical. Real smoke through the touch path in rotated landscape:
  sphere applied (482 : 2880), a transform-only edit publishing nothing, Freeze,
  a real 66-move Grab stroke capturing 439 of 482 vertices with topology fixed
  and every upload `reuse`, Back to Construction and Resume Sculpt. The Editor
  Workspace lays out correctly in both orientations with `viewport_surface` still
  full-bleed at `[0,0][2400,1080]`. Screenshots are under
  `artifacts/platformfixp2_*`.
- **Stage 015B acceptance** (`emulator-5558`, clean install, empty crash
  buffer): nine self-test suites green; 54 Android tests green; the measured
  unoccluded viewport at 411×914 dp is **82.6 %** collapsed and 57.5 % with the
  inspector fully open, at 914×411 dp landscape **60.1 %** (against 0 % before),
  and at 1280×800 dp **69.0 %** open / 85.2 % collapsed; a sphere applied and a
  transform applied through the real touch path, the transform publishing **no
  revision and no upload**; unit switching mm→cm→m→m exact (0.5 m ↔ 500 mm ↔
  50 cm) with zero native calls; a real Grab stroke on a 482-vertex sphere
  (35 moves, topology fixed at 482:2880, every upload `reuse`); three chrome
  drags leaving the viewport region **pixel-identical** and minting no sculpt
  revision; the re-Freeze confirmation naming "1 sculpt stroke" with Cancel
  leaving the mesh pixel-identical and making no native call; Resume Sculpt
  preserving `sculptRev=36` with the stale-source warning shown; and HOME/resume
  returning a pixel-identical viewport with no re-upload. Screenshots are under
  `artifacts/stage015b_*`.
- **Stage 014 acceptance** (`emulator-5558`, one pid, empty crash buffer): the
  cone and capsule happy paths, closed-base and hemisphere picking measured at
  the pixel against the exact radius, the capsule relation rejection, no-op and
  invalid handling, transform-only edits publishing nothing, the five-kind round
  trip preserving `objectId=1` and every remembered parameter, and the
  stale-source policy across a capsule → cone change with a pixel-identical
  resumed sculpt.
- **Gate P0 (NDK r27 → r29)**: `compileDebugJavaWithJavac`, `buildCMakeDebug` and
  `packageDebug` all succeeded first time with **no migration fix of any kind** —
  the whole migration is one line. Nine suites / 992 checks green, plus an
  integration smoke (Construction Apply, transform-only edit, transformed cone
  picking inside the facet band, Freeze, one real `uinput` Grab stroke with
  fixed topology and `reuse` uploads throughout, Construction ↔ Sculpt
  preservation, and a byte-identical HOME/resume screenshot).
- **16 KB page size**: ELF LOAD alignment moved from `0x1000` (r27, not
  compatible) to `0x4000` (r29, compatible) on all three segments;
  `zipalign -c -P 16 -v 4` verification successful with the `.so` stored
  uncompressed; a source/config audit found no page-size assumption anywhere and
  no third-party native library in the APK. Normal (4 KB) runtime smoke PASS.
  **16 KB runtime remains UNVERIFIED.**
- Runtime evidence screenshots are retained under `artifacts/`.

## Display-control motion, and what was rejected

Motion research for the display popover used Mobbin Pro against creative and
canvas applications on iOS. Five patterns were worth having an opinion about:

| Pattern | Seen in | Decision |
| --- | --- | --- |
| Popover **grows from the control that opened it**, rather than sliding in from a screen edge | Craft (line style), Apple Mail (shapes), Freeform | **Adopted.** 120 ms scale-plus-fade from the anchor corner; it reads as belonging to the button instead of arriving from nowhere. |
| **Selection feedback in place** — the chip repaints, the surface does not move, re-animate or close | Freeform's alignment settings toggles checkmarks across three frames with the menu stationary | **Adopted.** This is the one that matters: comparing Studio against MatCap means switching repeatedly, and a panel that re-animated on each choice would make it feel like four separate acts. |
| **Non-modal, stays open** across several changes | Craft, Freeform | **Adopted.** Dismissed only by tapping Display again, or by hiding chrome. |
| **Grouped labelled chip rows** rather than one long list | eBay "Edit Scene" (Material / Shadow), Photoroom (Shadow / Backdrop / Size) | **Adopted as layout**, not as styling: two groups, Shading and Surface. |
| **Live preview thumbnails** with a check badge per option | Photoroom | **Rejected.** A rendered thumbnail per shading mode needs a second offscreen render target, for a two-option choice whose result is already filling the screen behind the panel. |
| **Bottom sheet with drag detents** | Canva, Play, Unfold | **Rejected for this control.** A sheet covers the model, which is the thing being judged. ForgeShape already has a Property Inspector for sheet-shaped content. |

Nothing decorative was taken. There is no continuous or looping model animation,
no motion on the viewport itself, and no animation anywhere on the path of a
pointer sample. Both animations are interruptible, and a zero system animator
duration scale — the platform's own reduce-motion signal — skips them outright
rather than shortening them.

**The §12 readability enhancement is deliberately deferred.** Neither candidate
was implemented. A selected-object outline needs either a second pass over the
geometry or a screen-space depth/normal edge filter, and the latter is the start
of the post-processing framework this stage is told not to build; cheap cavity
shading from a screen-space depth derivative is the same problem, and the
vertex-based alternative would expose triangle structure in Smooth mode, which
the stage explicitly forbids. Neither is a small addition to what exists, and the
required shading is complete without them.

## Known Issues / Blockers

- **16 KB page-size runtime behaviour is UNVERIFIED.** Static, ELF and APK
  evidence all pass, but no 16 KB Android runtime is available: `emulator-5558`
  reports `getconf PAGE_SIZE` = 4096 and the only installed system image is a
  4 KB one. Closing this needs
  `system-images;android-36.1;google_apis_playstore_ps16k;x86_64` and an AVD
  created from it, and installing a system image is not authorized. Not a defect
  — an unmeasured dimension.
- **The `EditorWorkspaceView` layout decision runs inside `onMeasure`.** That is
  deliberate and documented — running it in `onSizeChanged` measures newly added
  chrome against the previous pass and lays it out at zero height, which is
  exactly the bug that was hit and fixed during this stage — but mutating the
  view tree during measure is unusual enough to be worth naming. It is
  idempotent and converges in one traversal.
- **A horizontal drag along the Property Inspector header toggles it.** The
  header is a full-width clickable row, and Android's default click detection
  fires on `ACTION_UP` while the pointer is still inside the view's bounds,
  regardless of how far it travelled. Harmless — the gesture never reaches the
  viewport — but it reads as a stray toggle. Not fixed, to avoid hand-written
  touch handling on a surface whose touch-opacity is load-bearing.
- **Test-harness note, not a product defect:** a stroke driven at the centre of a
  face of a frozen **box** logs `STROKE_PENDING` → `STROKE_ABANDONED:navigation`
  and never promotes, because a box has 8 vertices, all at its corners, and a
  brush that would capture no vertex starts no stroke. Reproduced identically
  under r27 and r29. Drive evidence strokes on a dense primitive (sphere 482 v,
  capsule 514 v) or widen the radius.
- **A viewport tap while the soft keyboard is up** must land clear of the
  Property Inspector and above the keyboard or it never reaches the
  `SurfaceView`; focus stays in the text field and a following `keyevent` types a
  digit instead. Looks exactly like a command that did not run. Test-harness
  issue only.
- **`uiautomator dump` omits inspector content scrolled out of view**, so a
  script that looks for `apply_shape` without scrolling the inspector first gets
  `MISSING` and reads like a lost control. Scroll, then resolve the id.
- **`connectedDebugAndroidTest` uninstalls the app when it finishes**, and it has
  no `-s <serial>` equivalent. Reinstall before taking runtime evidence, and run
  it with exactly one target attached.
- **The default logcat ring buffer drops part of the startup self-test output.**
  Run `adb -s <serial> logcat -G 16M` first.
- **The Android emulator dies if launched as a child of a tool shell**; spawn it
  detached.
- Android input resampling can emit a one-finger MOVE between `ACTION_DOWN` and
  `ACTION_POINTER_DOWN`, so starting a two-finger gesture may orbit by a few
  pixels first. Correct for the contract, not a defect.
- The path-driven tools deposit in proportion to pointer travel measured in brush
  radii, so a **short stroke with a large brush** does very little — the one
  thing that reads as "the brush did nothing" on a first try. Documented
  behaviour, not a defect.
- The arbitration's remaining deliberate seam: a first finger that **drags more
  than 8 px** before a second one lands does commit a stroke. That is a gesture
  the user drove as a stroke; closing it completely needs a buffered or undoable
  stroke, which does not exist.
- On `emulator-5558`, `screencap` output can acquire a uniform whole-framebuffer
  black-level lift partway through a session. It is a system/compositing effect,
  not a rendering change — the swapchain format is unchanged and home/resume
  within one display state stays byte-identical — but it means screenshots from
  different points in a session may not compare.

## Technical Debt

**The selection tint costs about half the surface's form contrast, measured.**
Selection is a whole-object tint mixed over the final shaded colour at
`alpha = 0.55` (`kSelectedTint` in `forgeshape_renderer.cpp`). On the default box
that takes the three visible faces from luminance 0.832 / 0.559 / 0.310 —
a 2.69x spread with clear steps between adjacent faces — to 0.739 / 0.616 /
0.504, a 1.47x spread whose face-to-face steps drop from ~0.26 to ~0.11, with
every face pushed into the same saturated orange so hue carries no form cue
either. A Construction Body is selected for the whole time the user is editing
it, so this is the state the object is normally *worked* in.

This is **not** the Stage 015C-R defect and was not touched by it: the inverted
culling was a separate, larger fault that the numbers above are measured after
fixing. It is recorded because it is a real, quantified readability cost, and
because the obvious repair is small — compose selection as a per-channel gain on
the shaded colour (`shaded * mix(vec3(1.0), tint, a)`) instead of a lerp toward a
flat colour, which preserves the luminance ratios between faces exactly while
leaving the object unmistakably orange. That is a deliberate change to a
user-visible convention, so it belongs to a stage that owns it, not to a fix
whose scope was culling.

**Sculpt cost model.** Sculpt publication is synchronous and republishes the
whole mesh per move — O(vertices) regardless of how few the brush touched — and
Inflate's normal recompute is O(triangles) per move for the same reason. Measured
free at 482–514 vertices (1,170 uploads, no stall, no growth), so nothing was
redesigned pre-emptively; that measurement is the baseline a future partial or
asynchronous path must beat. The affected set is found by a linear scan at stroke
start, and picking is a linear scan too: both want the same missing spatial
acceleration.

**Sculpt state and feel.** Sculpt state is process-scoped with no save, load or
undo, so a stroke is unrecoverable the moment it lands and Freeze silently
discards the previous sculpt (the panel says so; the honest fix is undo). The
path-driven gain constants (`kNormalBrushGain` 0.35, `kSmoothGain` 1.0,
`kMaxSmoothLambda` 0.9) are chosen, not derived, and have never been tuned
against a real modelling session; if brush feel is tuned they should move
together. `FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation` names two different
causes — a gesture that really became navigation, and a promoted stroke that
captured no vertex — and the log cannot tell them apart; the fix is a second
token, not a behaviour change. Freeze regenerates the Construction mesh to copy
it rather than copying the store's current revision: correct (the store may hold
a debug fixture) and cheap, but a second generation of geometry that already
exists. The Sculpt panel's status line is written on refresh and does not update
during a stroke, because a live readout would need a native→Java notification
that does not exist.

**Primitive surface.** Adding a primitive costs four parallel edits — a member on
`ConstructionObject`, a `PrimitiveKind` case, a variant alternative, and a JNI
method plus its Java declaration. That is deliberate and visible rather than
hidden behind a registry, but at a sixth primitive it should be a decision rather
than a habit. `ConstructionObject::setPrimitive` still dispatches with an if-else
chain over the typed accessors rather than a `std::visit`, because the per-kind
members are not uniform; it is the one place a new primitive can be forgotten
without a compile error. Every primitive's parameters are always resident even
though one is active. Tessellation is a compile-time constant, so a very large
curved primitive shows its facets and nothing adapts; relatedly, the capsule's
cylindrical middle carries no interior rings however long it is, which makes
sculpt fidelity there coarse (a 120 px brush captured 8 vertices on a 514-vertex
capsule), and the float-resolvability check is the one validation coupled to that
tessellation constant. Capsule topology is a function of its parameters (514 :
3072, or 482 : 2880 at equality), which is deliberate and is also the only case
where a shape edit can change a vertex count and trigger a buffer growth.

**Android layer.** The workspace still builds its view trees in code — colours,
dimensions, strings and ids are resources, but there are no XML layouts, so the
structure is only readable by reading Java. The Property Inspector has two
detents (collapsed / expanded) rather than the three the UI architecture pack
proposed; the third would only matter once a body is long enough that a partial
peek is useful. Chrome regions are placed by nested `LinearLayout` weights rather
than a constraint solver, which is why the rail sits in a `ScrollView` instead of
being able to compress. There is no focus-on-selection and no camera framing
helper. Focus handling is minimal: a field keeps focus after a rejected Apply and
keeps swallowing hardware/`adb` number keys until the viewport is touched. The
inspector refreshes from native truth on resume and after any Apply, discarding a
half-typed edit — deliberate, but a future edit-session or undo feature will need
a model for it. `LengthUnit.format` strips trailing zeros, so 2.0 m displays as
`2`; exact and unambiguous, but a future significant-figures policy will replace
it. The Tool Rail's glyphs are Unicode geometric characters rather than drawable
assets, so they depend on the platform font.

**Android test infrastructure.** `androidTest` is the project's only AndroidX,
and `android.useAndroidX=true` is now set for it — a build-configuration change
that adds nothing to the product APK, which still has no runtime dependency of
any kind. The instrumented suites share one process, so native state (in
particular *whether anything has ever been frozen*) carries across tests; the
freeze-wording test therefore asserts the state machine from whatever state it
finds rather than assuming a pristine process, and the pristine branch is covered
by running the suite on a clean install. There is no camera read-back across JNI,
so "a chrome gesture did not move the camera" is proven by runtime screenshot
comparison rather than by an automated assertion.

**Transform and math.** `modelMatrix()` / `inverseModelMatrix()` recompute six
trig calls per frame and per pick, for a value that only changes on Apply. The
transform is rigid by design: the exact composed inverse and the "local distance
is world distance" shortcut in picking both depend on that, so adding scale is a
domain change rather than a matrix change. A large translation makes the float
round-trip residual grow linearly with `|p|` (about `|p| * 2^-23`); at kilometre
scale the derived `float` matrix, not the `double` domain, is the precision
limit.

**Mesh and renderer.** `MeshStore` publishes for exactly one object; there is no
multi-mesh registry. `MeshStore::publish` validates the data twice (before
taking the lock and again inside `createRuntimeMesh`) — negligible now, wasteful
at sculpt sizes. Retired GPU buffers are freed inline after the fence wait rather
than through a deferred-destruction queue, which is what makes the synchronous
wait necessary. One redundant swapchain rebuild occurs at startup and again on
each resume, because `surfaceChanged` arrives immediately after `surfaceCreated`
(cosmetic). The identity-pre-transform orientation convention costs one
compositor rotation while the display is rotated — the same cost every
non-pre-rotated Android application pays, and not measurable here — but on a
tiled mobile GPU true pre-rotation is the cheaper path, so this is the one
renderer decision that a future performance stage might revisit; it would have to
move the extent, the clip-space rotation and the camera aspect together.
`createSwapchain`'s fallback for a presentation engine that does **not** support
an identity transform is **UNVERIFIED**: it exists because declaring identity
when it is absent from `supportedTransforms` would be invalid usage, not because
anything can reach it — `emulator-5558` reports `supportedTransforms=0x1ff`, so
identity is always available and the branch has never executed. Its extent swap
for 90/270 is reasoned from the Vulkan pre-transform contract, not measured, and
it would present rotated content. Carried and untouched: static viewport and
scissor, no `oldSwapchain` handling,
no validation layers, a single global viewport, and a selection highlight that is
a whole-object tint rather than an outline.

**Documentation size.** Every core document is inside the 2000-line hard limit,
but three are over their preferred target budgets and this stage pushed all three
further: `ARCHITECTURE.md` 1529 against a 700–1000 target, `PRODUCT.md` 561
against 300–450, `README.md` 320 against 150–250. (`PROJECT_STATUS.md` 706 and
`CLAUDE.md` 180 are inside theirs.) Every addition here is a new fact about a new
subsystem rather than a historical chapter, so nothing was appended that Git
history should have held instead — but the overshoot is real and predates this
stage, and closing it means compacting prose that has nothing to do with shading.
That is a deliberate deferral, not an oversight: bundling a documentation rewrite
into a rendering stage is exactly the unrelated-debt mixing the rules forbid.

**Shading and render data.** The crease policy is a single global angle. It is
correct for every primitive ForgeShape has, but it is a *policy*, not a per-object
property: a future imported or sketched body that genuinely wants a different
threshold has nowhere to say so, and the honest fix is a per-object crease value
rather than a second constant. Render data is rebuilt whole on every accepted
change, including every sculpt move — 0.56–0.73 ms for a 482-vertex mesh, free at
these sizes, and the same O(vertices) shape as the sculpt publication it rides on;
both want the same missing partial-update path. The renderer keeps a per-frame
gate *and* `RenderMeshCache` keeps its own, so the cache's skipped-refresh counter
reads zero forever in production and is exercised only by the self-tests; it is
deliberately not logged, but the duplication is real. Colour still travels in
`RenderVertex` purely so the debug source-colour mode has something to draw,
costing 12 bytes per render vertex in a build where nothing else reads it — the
primitive generators' per-vertex rainbow is likewise now dead data on the product
path, retained rather than deleted so the before/after comparison stays one tap
away. Studio Solid and the MatCap are authored **directly in display space**,
because the swapchain is `R8G8B8A8_UNORM` and every colour in ForgeShape (the
clear colour, the debug vertex colours) already is; there is no linear workflow,
and introducing one is a PBR-stage decision that has to move all of them at once.
The MatCap is regenerated from scratch on every device creation rather than
cached, which is 64 KiB of trivial arithmetic and has never been measured, but it
is work repeated for a constant. Selection remains a whole-object tint rather
than an outline; the §12 readability enhancement (outline or cavity) was
**explicitly deferred** — see below.

**Naming and test infrastructure.** `kConstructionBoxObjectId` and
`kDemoCubeObjectId` are the same value under two names and are now doubly
misnamed: the object is not always a box and never was a demo cube. Renaming has
been deferred to avoid churning unrelated code. There is still no checked-in
`uinput` harness file, so the multi-touch gesture is regenerated per stage.

## Current Files / Modules

| Path | Ownership |
| --- | --- |
| `settings.gradle`, `build.gradle`, `gradle.properties`, `local.properties` | Gradle project config |
| `gradlew(.bat)`, `gradle/wrapper/*` | Gradle 8.14.3 wrapper |
| `app/build.gradle` | Android app module config, SDK/NDK/CMake/ABI pinning |
| `app/src/main/AndroidManifest.xml` | App/activity declaration, Vulkan feature requirement |
| `app/src/main/java/.../ForgeShapeActivity.java` | Android lifecycle, edge-to-edge window, resume refresh, DEBUG key hook |
| `app/src/main/java/.../ForgeShapeSurfaceView.java` | Viewport surface, forwards lifecycle + raw pointer state, takes focus back from an editor |
| `app/src/main/java/.../EditorWorkspaceView.java` | The whole editor UI: region composition, adaptive layout, window insets, chrome visibility, mode/tool wiring, and `syncFromNative()`. Owns no product state |
| `app/src/main/java/.../WorkspaceLayoutMode.java` | Window-dp breakpoints, inspector placement and chrome sizing, as arithmetic. No Android type |
| `app/src/main/java/.../EditorUiState.java` | The closed list of UI-owned state: display unit, draft kind, rail selection, detent per mode, chrome-hidden |
| `app/src/main/java/.../GlobalToolbarView.java` | Editing context, the three mutually exclusive mode transitions, reserved Export, the Display control, chrome hide, and the one status message |
| `app/src/main/java/.../DisplaySettingsPopoverView.java` | The compact display popover: Shading (Studio / MatCap / Debug) and Surface (Smooth / Faceted), with short interruptible open/close motion that honours the system animator scale. Owns no state |
| `app/src/main/java/.../ToolRailView.java` | The edge tool selector for either mode, including reserved entries. Selects; decides nothing |
| `app/src/main/java/.../BrushEdgeControlsView.java`, `VerticalSliderView.java` | Direct Radius and Strength, and the custom vertical control behind them. Own no brush value |
| `app/src/main/java/.../PropertyInspectorView.java` | Contextual, collapsible, scrolling container with a measured height cap. Owns no value |
| `app/src/main/java/.../ConstructionShapeEditorView.java` | Primitive chooser, that primitive's exact fields, unit chips, Apply Shape. Owns field text and a DRAFT kind only |
| `app/src/main/java/.../ConstructionPlacementEditorView.java` | Position/rotation fields, unit chips, Apply Transform. Owns field text only |
| `app/src/main/java/.../SculptContextView.java` | Frozen-mesh summary, stale-source warning, and the guarded re-Freeze |
| `app/src/main/java/.../InspectorHost.java`, `NumericPropertyRow.java`, `UnitChipsView.java`, `EditorControlStyles.java` | The four small shared pieces: what a body may ask of the workspace, one labelled exact field, the mm/cm/m selector, and the one place controls get their look |
| `app/src/main/java/.../LengthUnit.java` | Exact `BigDecimal` mm/cm/m ↔ meter conversion, parsing and formatting |
| `app/src/main/res/values/*` | `ids.xml` (the stable semantic id contract), `dimens.xml`, `colors.xml`, `strings.xml`, `themes.xml` (edge-to-edge) |
| `app/src/test/java/...` | JVM suites: layout arithmetic, UI-owned state, unit conversion |
| `app/src/androidTest/java/...` | Instrumented Editor Workspace suites plus `WorkspaceTestSupport` (native snapshots, drag consumption, exact chrome-union viewport measurement) |
| `app/src/main/java/.../NativeViewport.java` | JNI declarations, library load, `APPLY_*` / `SCULPT_*` status codes, `MODE_*`, `TOOL_*` |
| `app/src/main/cpp/forgeshape_jni.cpp` | JNI boundary, render thread, `ANativeWindow`, MotionEvent→`TouchAction`, camera + selection locking, stroke arbitration, `publishActiveRepresentation` |
| `app/src/main/cpp/forgeshape_input.h` | Platform-neutral touch event data (`TouchAction`, `TouchPointer`) |
| `app/src/main/cpp/forgeshape_camera.{h,cpp}` | Camera pose, projection, gesture state machine |
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all five primitives + transform), the five `Construction*` generators, shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation including `validateCapsuleMeters`, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
| `app/src/main/cpp/forgeshape_transform.{h,cpp}` | `ConstructionTransform`: authoritative double-meter position and double-degree rotation, THE axis/Euler convention, validation, atomic apply, derived model and inverse-model matrices |
| `app/src/main/cpp/forgeshape_sculpt.{h,cpp}` | `ProductMode`, `SculptTool`, `SculptSession` (mode + tool + brush + live stroke + the `hitsSculptMesh` probe), `SculptMesh`, `SculptTopology`, `computeVertexNormals`, `SculptStroke` (the one kernel plus one `apply*` per tool), sculpt publication |
| `app/src/main/cpp/forgeshape_picking.{h,cpp}` | Screen→world ray, `transformRayToLocal`, ray/triangle, nearest hit, winding check |
| `app/src/main/cpp/forgeshape_selection.{h,cpp}` | `ObjectId`, `SelectionController`, tap-vs-navigation, `pickScene` |
| `app/src/main/cpp/forgeshape_object_id.h` | `ObjectId` type and reserved values, shared by the mesh and selection layers |
| `app/src/main/cpp/forgeshape_mesh.{h,cpp}` | `RuntimeMesh` (immutable revision), `MeshStore`, validation, capacity policy, upload diagnostics including source-vs-render counts |
| `app/src/main/cpp/forgeshape_render_mesh.{h,cpp}` | Derived render geometry: `RenderVertex` (position + normal + colour), `SurfaceShading`, THE crease policy (`kCreaseAngleDegrees`), per-vertex crease grouping with render-only duplication, and `RenderMeshCache`'s rebuild gate. Presentation only |
| `app/src/main/cpp/forgeshape_matcap.{h,cpp}` | The one ForgeShape-owned MatCap, computed at device init from the closed-form model in that file. No asset, no decoder, one preset |
| `app/src/main/cpp/forgeshape_display.{h,cpp}` | `ShadingModel`, the process-scoped `DisplaySettingsStore`, and the UI index mapping. Presentation state, never truth |
| `app/src/main/cpp/forgeshape_renderer.{h,cpp}` | Vulkan renderer, frame loop, camera snapshot + model transform + selection highlight consumer |
| `app/src/main/cpp/forgeshape_math.h` | Minimal self-owned vec3/mat4. No GLM |
| `app/src/main/cpp/forgeshape_demo_mesh.{h,cpp}` | Baseline cube numbers; source data for the baseline debug fixture only |
| `app/src/main/cpp/forgeshape_mesh_fixtures.{h,cpp}` | DEBUG test fixtures (baseline / same-topology / larger / stress step) |
| `app/src/main/cpp/forgeshape_*_selftest.{h,cpp}` | The ten debug-only deterministic suites: camera, picking, mesh, construction (box), transform, primitive, sphere, cone/capsule, sculpt brush kernel, render shading |
| `app/src/main/cpp/shaders/surface.{vert,frag}` | GLSL source for the surface pipeline: view-space normals, Studio Solid, the MatCap lookup and the debug colour path. AOT compiled to SPIR-V by `glslc` in CMake |
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

**Stage 016 — Plane + Primitive Coverage Cleanup**

The Editor Workspace is in place and every control it needs for a sixth primitive
already exists — the chooser lays out in rows of three, so a Plane costs one more
chip, one more parameter row, one more `primitive_row_*` id and one more case in
`UI-02`, and no shell change at all. That is the point of having built the shell
first.

The debt this stage should settle while it is in the area is named under
*Primitive surface* above: `ConstructionObject::setPrimitive` still dispatches
with an if-else chain over typed accessors rather than a `std::visit`, which is
the one place a new primitive can be forgotten without a compile error, and a
Plane is exactly the primitive that would slip through it. A sixth primitive is
also the point at which "four parallel edits per primitive" should become a
decision rather than a habit.

The rotated-landscape rendering defect that previously blocked the area is
**closed** by Platform Fix P2 and is not part of this stage. Stage 016 is
primitives only: it must not touch the renderer's orientation convention.

Shading costs a Plane nothing extra. The crease policy is per-vertex and
primitive-agnostic, so a Plane inherits correct flat shading with no new case —
though it is the first primitive that is a **single flat sheet**, so Stage 016
should check what a Plane looks like from behind, where back-face culling means
it disappears. That is a culling question, not a shading one, and it is the one
interaction between the two areas worth naming in advance.
