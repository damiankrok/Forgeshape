# ForgeShape — Project Status

**Status Version:** 0.22.0
**Updated:** 2026-08-22
**Result:** PARTIAL — Gate P1 blocked on missing physical ARM64 hardware; see
the Gate P1 chapter below for exactly what is and is not closed
**Current Phase:** Phase 1 — Native Viewport
**Workspace:** `D:\TRAVELAPPS\ForgeShape`
**Accepted implementation baseline:** Stage 016-R2 — explicit emulator port
isolation + deterministic native self-test fixtures, on top of Stage 016
(Plane + primitive coverage cleanup), Stage 015D (camera projection), Stage
015C-R (front-face culling), Stage 015C (shading), Platform Fix P2, Stage
015B, Stage 014, the NDK r29 migration (Gate P0) and the owner decision
baseline. Gate P1's arm64-v8a build support, 16 KB runtime verification and
Vulkan validation path are real, evidenced, additive work on top of this
baseline (see below) but Gate P1 itself is not accepted as complete.
**Next Stage:** Gate P1 closure — obtain access to a physical ARM64 Android
device (explicit adb serial, provided by the owner) and re-run P1-A (launch/
lifecycle smoke) and the mandatory P1-E mesh-density ladder (~10k/~50k/~100k
vertices) on it. Every other Gate P1 requirement is already closed.

This is a current snapshot, not a chronology. Per-stage verification chapters,
superseded environment states and old next-stage recommendations live in Git
history and are deliberately not repeated here.

## Gate P1 — Physical ARM64 + 16 KB Runtime + Vulkan Validation + Heavy-Mesh Baseline (PARTIAL)

No product behaviour changed. Closed: arm64-v8a build support, real 16 KB
runtime verification, an active debug-only Vulkan validation path, and a new
debug/test-only heavy-mesh density fixture. Blocked: no physical ARM64
device was made available this Gate, so P1-A (physical launch/lifecycle
smoke) and the *mandatory* P1-E density ladder — which this Gate requires to
run on physical ARM64, not an emulator — could not be executed. P1-D
(stylus) is UNVERIFIED for the same reason: no physical device, no real
stylus input.

**ABI.** `app/build.gradle`'s `abiFilters` now lists `'x86_64', 'arm64-v8a'`;
CMake and the native sources needed no ABI-specific changes (no intrinsics,
no `#ifdef __x86_64__`/`__aarch64__` anywhere in `app/src/main/cpp`). Both
`.so`s build clean and package into the debug APK
(`lib/arm64-v8a/libforgeshape_native.so`, `lib/x86_64/libforgeshape_native.so`),
both report `0x4000` (16384-byte) ELF `LOAD` alignment via `llvm-readelf -l`,
and `zipalign -c -P 16 -v 4` on the packaged APK reports both libraries `OK`.
x86_64 regression is unaffected (below).

**16 KB runtime — real, not just static alignment.** With the owner's
authorization, `system-images;android-36.1;google_apis_playstore_ps16k;x86_64`
was installed via `sdkmanager` and a new isolated AVD, `ForgeShape_16K`, was
created from it (GPU host mode and the same pixel_6 profile as
`ForgeShape_Stage006`; not authorized for physical-ARM64 evidence — it is
x86_64, not arm64 — but it is a real Linux kernel built with a genuine 16384
page size, which is exactly the dimension Gate P0 left unmeasured).
`adb -s emulator-5590 shell getconf PAGE_SIZE` reports **16384**. On that
target: a clean launch reports all ten self-test suites green, 1421 checks,
zero failures; the Plane primitive applies and picks (front and, after a
180° transform-only rotate, the two-sided back-face case) correctly;
Orthographic and MatCap both switch correctly; Freeze → Back to Construction
→ Sphere apply produces the stale-source warning with the frozen mesh
untouched; Freeze again onto the Sphere (482:2880) needs no confirmation
(nothing to lose) and a real 23-move Grab stroke completes
(`STROKE_PENDING`→`STROKE_BEGIN:grab:142`→`STROKE_END`); HOME/resume
reproduces the same `ActivityRecord` with no re-init; rotated landscape holds
`chosenExtent=2400x1080 preTransform=0x1`. One reproducible environment
symptom: this AVD's SystemUI hit a persistent "System UI isn't responding"
ANR under rapid scripted UI automation partway through evidence collection; a
full guest reboot cleared it and the retried sequence completed cleanly with
zero `FORGESHAPE_*_SELFTEST_FAIL` tokens throughout. Recorded as a real
16 KB-runtime stability observation, not a ForgeShape defect — the native
render thread and self-tests were unaffected throughout and the symptom was
specifically SystemUI's input dispatch, not `com.forgeshape.app`. Screenshots
under `artifacts/gatep1_16k_*`.

**Vulkan validation.** No validation-layer binary existed anywhere on this
machine; with the owner's authorization, the official
`android-binaries-1.4.357.0.zip` was downloaded from
`github.com/KhronosGroup/Vulkan-ValidationLayers` releases (the source
Android's own developer documentation names) and used exclusively through
Android's first-party per-app GPU debug layer mechanism: `adb push` to
`/data/local/tmp`, `run-as` copy into the app's own data directory, and
`adb shell settings put global enable_gpu_debug_layers 1` /
`gpu_debug_app` / `gpu_debug_layers=VK_LAYER_KHRONOS_validation` /
`gpu_debug_layer_app`. The binary was never bundled into the APK, never
placed under `jniLibs`, and never committed to the repository — purely an
ad-hoc, adb-pushed debug tool, cleaned up (device settings deleted) after
evidence collection. `adb logcat` confirms
`Loaded layer VK_LAYER_KHRONOS_validation` and, with
`debug.vulkan.khronos_validation.report_flags=error,warn,perf,info`, the
layer's own `I VALIDATION:` banner: "Current Validation Enabled: Core
Checks, Stateless Parameter, Object lifetime, Thread Safety, Handle
Wrapping." Exercised across Construction/Plane apply, front/back picking, a
transform-only edit, both projections, both shading models, Freeze → Sphere
→ Freeze again → a real Grab stroke, and self-tests (1421 checks green
throughout, unaffected). Result: **zero ForgeShape-caused validation
messages of any severity.** The only message the layer emitted at all is one
`Validation Information: [ WARNING-cache-file-error ]` — the layer's own
shader-validation-cache file not existing yet at
`/tmp/shader_validation_cache-<uid>.bin` on first run — which is the layer's
internal bookkeeping, not a finding against ForgeShape's Vulkan usage.

**Heavy-mesh density fixture (test/debug-only).** The existing sphere
generator's vertex count is fixed by the Construction contract (482:2880 at
any diameter) and is not reusable for a variable density ladder without
changing that contract, which this Gate forbids. Added
`buildStressMesh(uint32_t targetVertexCount)` to
`forgeshape_mesh_fixtures.{h,cpp}`, generalizing the existing
`buildFixtureLarge` "spherified box" (closed, deterministic, canonical
outward winding, uint32-safe indices) to a caller-chosen density instead of
its fixed `kFixtureLargeSubdivisions`, and reused it via debug-only key hooks
(`ForgeShapeActivity` keys A–E publish ~10k/50k/100k/250k/500k-vertex tiers
through the same `MeshStore::publish` path the other fixtures use; key F
freezes the most recently published tier directly into Sculpt through the
real `SculptSession::freezeToSculpt`, bypassing Construction, for stroke
measurement at density). Not a Construction primitive, not reachable from
product UI, compiled out in release exactly like the existing fixtures 1–9.
**Harness-validated only** on `emulator-5590` (x86_64, 16 KB runtime) at the
10k/50k/100k tiers — publish, GPU upload/growth, picking, Sculpt freeze
(adjacency build) and a real Grab stroke all worked correctly with zero
failures — but this is proof the *harness* is correct, not P1-E mandatory-
tier evidence, which this Gate requires on physical ARM64. Measured there:
10086 v / 57600 i (gen 4.4 ms, publish 1.3 ms); 49686 v / 291600 i (gen
13.0 ms, publish 4.2 ms); 99846 v / 589824 i (gen 22.0 ms, publish 9.9 ms,
render-mesh build 120.8 ms, a real pick landing at triangle 159210, Sculpt
freeze in 80.2 ms building 592896 adjacency entries, a real Grab stroke
capturing 3792 vertices). Process PSS after the 100k tier plus a Sculpt
freeze and stroke: ~75 MB, no crash.

**Regression.** 26/26 JVM tests; ten native self-test suites, 1421 checks,
zero failures on a clean launch (confirmed on both `ForgeShape_Stage006` and
the new 16 KB target); 46/46 instrumented tests on `ForgeShape_Stage006` /
`emulator-5580` through the corrected wrapper. Zero interaction with
`emulator-5554` anywhere in this Gate. One pre-existing script bug was found
and fixed while booting the 16 KB AVD:
`scripts/start-forgeshape-emulator.ps1`'s boot-wait poll redirected
`adb get-state`'s stderr under `$ErrorActionPreference = 'Stop'`, which
PowerShell 5.1 turns into a terminating `NativeCommandError` on the expected
"device not found" response while the emulator is still booting — wrapped in
try/catch; `DEV2-01..07` re-verified green afterward.

**GP1 criteria.**

| ID | Verdict | Evidence |
| --- | --- | --- |
| GP1-01 | PASS | clean tree at `f156ed9` confirmed before any change |
| GP1-02 | PASS | arm64-v8a + x86_64 both build, package, and 16 KB-align; x86_64 self-test/JVM/instrumented regression green |
| GP1-03 | BLOCKED | no physical ARM64 device provided |
| GP1-04 | BLOCKED | requires P1-A physical target |
| GP1-05 | BLOCKED | requires P1-A physical target |
| GP1-06 | BLOCKED | requires P1-A physical target |
| GP1-07 | PASS | `getconf PAGE_SIZE` = 16384 on `emulator-5590` |
| GP1-08 | PASS | 1421/1421 self-test checks, full Construction/picking/projection/shading/Freeze/Resume/Sculpt smoke, zero failures on the 16 KB target |
| GP1-09 | PASS | both `.so`s at 0x4000 ELF alignment; `zipalign -P 16 -c` OK on the packaged APK |
| GP1-10 | PASS | `VK_LAYER_KHRONOS_validation` loaded and proven active (layer's own "Current Validation Enabled" banner) |
| GP1-11 | PASS | zero ForgeShape-caused validation messages of any severity across the exercised path |
| GP1-12 | PASS | the one message seen (`WARNING-cache-file-error`) classified as the layer's own internal bookkeeping, not a ForgeShape finding |
| GP1-13 | UNVERIFIED | no physical device, no real stylus available |
| GP1-14 | BLOCKED | ~10k tier requires physical ARM64; harness-validated only on x86_64/16K |
| GP1-15 | BLOCKED | ~50k tier requires physical ARM64; harness-validated only on x86_64/16K |
| GP1-16 | BLOCKED | ~100k tier requires physical ARM64; harness-validated only on x86_64/16K |
| GP1-17 | N/A | no mandatory-tier run to evaluate (see GP1-14..16) |
| GP1-18 | N/A | same |
| GP1-19 | N/A | same — Sculpt-heavy-mesh metrics were gathered only on the harness-validation target |
| GP1-20 | N/A | same |
| GP1-21 | N/A | 250k/500k not attempted; mandatory tiers themselves are blocked |
| GP1-22 | PASS | no triangle-count/performance claim made anywhere in this report |
| GP1-23 | PASS | JVM 26/26, native self-test 1421/1421 (both targets), instrumented 46/46 |
| GP1-24 | PASS | every adb call this Gate used `-s <serial>`; zero `emulator-5554` interaction; no auto-port; no `connected*AndroidTest` fan-out |
| GP1-25 | PASS | no Sketch/Extrude/booleans/persistence/export/new tools/pressure/remesh/PBR/Apple work; heavy-mesh fixture is debug-only, not a product feature |
| GP1-26 | PASS | see doc line counts below |
| GP1-27 | PASS | one focused commit, clean tree |

**Result: PARTIAL.** GP1-01/02/07..12/22..27 PASS; GP1-13 UNVERIFIED
(permitted); GP1-03..06 and GP1-14..21 BLOCKED for the single reason above.

## Stage 016-R2 — explicit emulator port isolation + deterministic test harness

Closes Stage 016 with **zero product behaviour changes**: only emulator/test
scripts and native self-test fixtures changed.

**Root cause, self-test determinism.** `NativeViewport.start()`'s
thread-joinable guard only blocks a *concurrent* start; it does not make the
self-test run a true one-time process event. `ForgeShapeActivity.onDestroy()`
calls `NativeViewport.stop()`, which joins the render thread, so a later
`onCreate()` — a real Activity recreation within one still-alive process, the
normal shape of an `am instrument` run moving between test classes — passes
the guard and reruns all ten suites. Three self-tests assumed the live,
process-scoped `MeshStore` / `ConstructionTransform` / `ConstructionObject`
still held their first-launch defaults: `testDemoCubePicking`
(`forgeshape_picking_selftest.cpp`, using the implicit
`pickScene(camera, x, y, w, h)` overload, which reads
`constructionTransform()` and `constructionObject().kind()`) and two picks in
`forgeshape_construction_selftest.cpp` / `forgeshape_mesh_selftest.cpp` that
already published their own known fixture into the store but still picked
through that same implicit, transform-dependent overload. Fixed by publishing
`demoCubeMeshView()` directly in the picking self-test and switching all three
to the existing explicit-transform `pickScene(..., model, inverseModel,
frontFacesOnly)` overload with an identity transform, removing every
dependency on live global state. `publishActiveRepresentation` already
restores the real product mesh after all self-tests finish, so this was
already read-only with respect to what the user sees; the fix makes it
input-independent too.

**Verified live, on `ForgeShape_Stage006` / `emulator-5580`.** Mutated the
process-global Construction primitive via the real UI (Box → Plane 2×1.25 m →
Sphere), then ran the full instrumented suite: the render thread started and
stopped **46** times in one process (one cycle per test method), all ten
self-test suites reran on nearly every cycle, and grep across the full
captured log found **zero** `_SELFTEST_FAIL` / `_FAIL:` tokens. One cycle's
`startup_after_selftests` republish logged `kind=plane w=2.0 d=1.25` — direct
proof self-tests passed with a mutated, non-default primitive and transform
still live in process state. A clean cold launch after the whole session still
reports **1421 checks, zero failures**, digit-for-digit the Stage 016
baseline.

**Emulator/adb hygiene.** `scripts\start-forgeshape-emulator.ps1` is a new,
minimal launcher: explicit `-Avd`/`-Port` (default `ForgeShape_Stage006` /
`5580`), hard-rejects port `5554` before any OS or adb call, checks occupancy
of only the requested port, BLOCKS with no automatic fallback port, launches
detached, and confirms AVD identity by name before reporting ready.
`scripts\run-instrumented-tests.ps1`'s one bare `adb devices` enumeration
(used only to confirm the given serial was attached) is replaced with
`adb -s <serial> get-state`, so every adb call in the repo's scripts is now
scoped to an explicit serial with no exception. `scripts\verify-device-guards.ps1`
is new: `DEV2-01`..`07` all PASS, including two checks (`DEV2-01`, `DEV2-05`)
that invoke the real scripts' real reject paths as child processes (not
mocks) and one (`DEV2-02`) that proves the occupied-port BLOCK against a real
dummy TCP listener, never against `5554` or `5580`.

**Verification.** Ten self-test suites green (1421 checks, zero failures,
including after 46 in-process reruns under mutated state); 26 JVM tests green;
46 instrumented tests green on `emulator-5580` only; DEV2-01..07 PASS; zero
adb interaction with `emulator-5554` anywhere in this stage. Runtime
re-verification repeated the Stage 016 Plane contract on the real touch path:
Apply (2×1.25 m), front-face pick, a 180° transform-only rotation (`rev`
unchanged) with a confirmed back-face pick (the two-sided Plane exception),
Orthographic + MatCap, Freeze, Back to Construction, a Sphere apply while
frozen (`FORGESHAPE_SCULPT_SOURCE_STALE`, frozen mesh untouched at "4
vertices"), Freeze again onto the Sphere (482:2880, no confirmation needed —
nothing to lose), a real 24-move Grab stroke
(`STROKE_PENDING`→`STROKE_BEGIN:grab:131`→24×`STROKE_MOVE`→`STROKE_END`),
HOME/resume with no re-init (identical `ActivityRecord`, no self-test rerun),
and rotated landscape (`chosenExtent=2400x1080 preTransform=0x1`). Screenshots
are under `artifacts/stage016r2_*`.

## Stage 016 — Plane + primitive coverage cleanup

**Plane is the sixth and final Construction MVP primitive**: a flat,
zero-thickness rectangular sheet, authored by width (local X) and depth
(local Z), centred on the local origin at `y = 0` with its canonical front
along local `+Y`. It is not a solid, not a Sketch plane and not an infinite
grid. Source topology is exactly **4 vertices, 2 triangles, 6 indices**,
independent of the requested dimensions, CCW seen from the front, with exact
bounds `X = [-w/2, +w/2]`, `Y = [0, 0]`, `Z = [-d/2, +d/2]`. Every existing
Construction contract applies unchanged: exact double-meter parameters,
transactional Apply (`Applied`/`Unchanged`/`Rejected`), the mm/cm/m display
contract, transform, Freeze/Resume, stale-source, and both projections.

**Two-sided editor usability is one bounded, explicitly named exception, not a
global culling change.** A Plane has no interior, so unlike a closed solid
there is no far wall a two-sided pick or render could wrongly reach. Both
mechanisms key off one fact — `RuntimeMesh::renderBothSides()` /
`ConstructionMesh::renderBothSides`, true only for a Plane, carried with the
published mesh rather than re-derived from `PrimitiveKind` in either
consumer:

- **Rendering** (`forgeshape_render_mesh.cpp`): when set, the ordinary
  one-sided render result is duplicated once more — every vertex repeated
  with its normal negated, every triangle repeated with reversed winding — so
  the *existing* global `VK_CULL_MODE_BACK_BIT` /
  `VK_FRONT_FACE_COUNTER_CLOCKWISE` pipeline draws the duplicate from the far
  side while culling it from the near side. No pipeline, culling or material
  change anywhere; the authoritative `RuntimeMesh` is never touched. Render
  counts double (4:6 Smooth source builds to 8:12 render) — a second,
  distinct reason a render count can exceed a source count, alongside the
  existing crease-split one.
- **Picking**: `pickScene` passes `!(constructionObject().kind() ==
  PrimitiveKind::Plane)` as `frontFacesOnly`, so only a Plane picks from both
  sides. The explicit-transform `pickScene` overload gained an optional
  `frontFacesOnly` parameter (default `true`) so the self-test-only overload
  stays free of process-scoped state.

**Primitive coverage cleanup, bounded to what a sixth kind actually
revealed.** `ConstructionObject::setPrimitive` no longer dispatches its
update half with an if-else chain over typed accessors — the debt Stage 015D
named. It `std::visit`s the requested `PrimitiveSpec` payload against two
small overload sets (`parametersDiffer` / `writeParameters`, one overload per
kind), so a kind added to the variant with no matching overload is now a
**compile** error, not a silently-skipped branch — the property the existing
`validateParameters` family already had via its own `std::visit`. The Java
shape editor's two-field-primitive constants (`DIAMETER`/`AXIAL`) are renamed
`FIELD_0`/`FIELD_1`: a Plane's width/depth pair is neither, and the rename is
the direct, minimal fix a sixth kind revealed. No reflection, registry or
property-bag framework was introduced.

**Verification.** Ten self-test suites green, **1421 checks, zero
failures** (up from 1316): picking 112→124 (`PLN-11`..`16` — the two-sided
exception, front/back, outside-rectangle, transformed, both projections),
primitive 75→125 (`PLN-01`..`08` — topology, bounds, winding, rejection,
apply semantics, the six-kind round trip), sculpt 250→276 (`PLN-17`..`19` —
Freeze on an open 4:6 mesh, Resume, stale-source), render shading 207→224
(`PLN-09`/`10`/`20` — the two-sided render duplication and its inertness to
display switching). 26 JVM tests green. **46 instrumented tests, 46 green**
(up from 44), including the `ui11` IME case that has moved in both
directions across recent stages.

**Runtime**, on `ForgeShape_Stage006` (booted this session on port 5556
rather than its usual 5558 — confirmed by AVD name, not by port, before any
command targeted it): a non-default Plane (2.0 × 1.25 m) applied through the
real touch path (`FORGESHAPE_CONSTRUCTION_PUBLISHED:8:4:6`), an exact mm/cm/m
round trip (4 m → 400 cm → 4000 mm → 4 m, digit for digit), a 180°
`ConstructionTransform` rotation applied as a transform-only edit (revision
unchanged), a real tap-to-select hit landing on the rotated (far) side of the
sheet (`FORGESHAPE_PICK_HIT` at local `y=0.0000`), Perspective and
Orthographic, Studio and MatCap, Freeze → Sculpt
(`FORGESHAPE_SCULPT_FROZEN:4:6`) → Back to Construction → Resume Sculpt
(`freezes=1` unchanged, topology still 4:6), a Construction edit after Freeze
producing `FORGESHAPE_SCULPT_SOURCE_STALE` with the frozen mesh untouched and
the Sculpt panel naming "4 vertices", a real 23-move Grab stroke on a
re-frozen 482-vertex sphere (the sculpt-regression carve-out, not the sparse
Plane: `STROKE_PENDING` → `STROKE_BEGIN:grab:137` → 23 `STROKE_MOVE`s →
`STROKE_END`), HOME/resume with no re-upload, and rotated landscape
(`chosenExtent=2400x1080 preTransform=0x1`, the P2 convention intact).
Screenshots are under `artifacts/stage016_*`; the Plane reads as a true flat
sheet throughout, never as a thick box.

**One incidental fix, found only by running the real touch path.**
`forgeshape_jni.cpp`'s `describeSpec` — the JNI-local log-line formatter, a
different if-else chain from the one the cleanup above targeted, exercised
only by the JNI wrapper functions the native self-tests never call (they call
the namespaced `forgeshape::applyPrimitive` directly) — had no Plane branch
and logged `kind=plane unknown`. Fixed by adding the branch. No self-test
would have caught this; it is the concrete argument for the runtime pass
beyond what the self-tests already prove.

**One environment mishap, disclosed rather than hidden.** The first
`connectedDebugAndroidTest` run was launched without pinning a target device,
and Gradle's task runs against every attached device with no default: it
installed the debug APK and ran the full 46-test suite against
`emulator-5554` (`Medium_Phone_API_36.1`), the AVD `CLAUDE.md` reserves for
another program — confirmed via `adb -s emulator-5554 emu avd name` after the
fact. No further command touched that device. The owner was told before the
stage continued, and chose to proceed with every later command scoped to
`-s emulator-5556` / `ANDROID_SERIAL=emulator-5556`, confirmed by AVD name to
be `ForgeShape_Stage006`.

**Remediated in Stage 016-R, then hardened further in Stage 016-R2** (see that
chapter above for the explicit-port launcher, the `adb devices`→`get-state`
fix and the self-test determinism fix), without touching `emulator-5554` at
all: the
bare, unscoped Gradle task is now a documented anti-pattern in `CLAUDE.md` and
`README.md`, and `scripts\run-instrumented-tests.ps1 -Serial <serial>` is the
one supported instrumented-test path — it requires an explicit serial, refuses
`emulator-5554` before any device is contacted, and drives every install and
instrumentation step through `adb -s <serial>` only. Re-verification on
`ForgeShape_Stage006` (that session booted it on `emulator-5580`, since even
`5554` itself is not a stable identifier for any one AVD — see `CLAUDE.md`)
reproduced the Stage 016 baseline: ten self-test suites green on a clean
launch (1421 checks, zero failures), 26 JVM tests green, 46 instrumented tests
green on that one serial, and the Plane/projection/shading/Freeze-Resume/
stale-source/real-sculpt/lifecycle contracts held on the real touch path.

Stage 015D added a mathematically correct **Orthographic** projection beside the
existing Perspective one, so exact Construction geometry can be judged without
near parts of a solid being enlarged for being near. Perspective remains the
default and was **audited, not changed** — its 60° vertical field of view, its
aspect source and its pinhole form are all unchanged.

Stage 015C-R before it fixed the defect that made every convex primitive read as
a hollow interior: the graphics pipeline named `VK_FRONT_FACE_CLOCKWISE`, which
inverted back-face culling, so the viewport drew each solid's **far** walls
instead of its near ones. It also added the ten-part direction test family
(`NOR-01`..`NOR-10`).

Stage 015C before it replaced the debug-looking per-vertex rainbow with readable
lit geometry: a **derived, render-only** normal layer between the authoritative
mesh and Vulkan, one centralized crease policy, two shading models (Studio Solid
and MatCap), Smooth/Faceted display, a compact display control in the Global
Toolbar, and a tenth native self-test suite. Platform Fix P2 closed the
rotated-landscape rendering defect, and Stage 015B replaced the two provisional
Android panels with the approved responsive **Editor Workspace**.

## Stage 015D — camera projection

**The existing Perspective camera was audited first and left alone.** The
vertical field of view is **60°** (`kFovYRadians = 1.0471976`), the aspect comes
from the one `CameraController` viewport truth, orbit changes neither distance
nor FOV, there is no screen-axis non-uniform scale, and the matrix is a standard
pinhole (`proj.m[11] = -1`). `CAMPROJ-01` asserts each of those against the
matrix rather than by inspection, including that `|m[0]/m[5]|` is exactly
`1/aspect` — the check that would fail if anything ever stretched one axis to
"fix" a rotated display. No evidence of non-rigid geometry distortion was found
and no projection constant was changed.

**Orthographic is a true parallel projection**, `mat4Orthographic` in
`forgeshape_math.h`, sharing every convention with `mat4Perspective`:
right-handed view space, Vulkan `[0, 1]` depth, the Y flip in the matrix. Its
defining property is `m[11] = 0`, so `w` is 1 for every vertex and nothing is
divided by depth. It is **not** a narrow FOV, a huge camera distance, a model
scale or a shader trick, and `CAMPROJ-02`/`03` assert the parallel behaviour
directly: two equal segments parallel to the image plane measure the same screen
size at depths 80 m apart, and a box's front and back faces project to the same
width.

**The orthographic scale is an explicit world length.**
`orthoHalfHeightMeters` is half the world height the viewport shows, in meters at
the target plane — finite, positive, and clamped to `[0.02, 250] m`, which
brackets the same range of apparent sizes the perspective distance clamps do. The
snapshot carries it in both modes, so it is never a stale leftover.

**Switching preserves the framing**, converting rather than resetting:
`orthoHalfHeight = distance × tan(fovY/2)` and its inverse. Target, yaw and pitch
are untouched. Measured at runtime: the first switch produced
`orthoHalfHeightMeters=4.7343` from `distance=8.2000` (8.2 × tan 30° = 4.73427),
and after an ortho pinch to `0.3809` the switch back produced `distance=0.6598`
(0.3809 / tan 30° = 0.65977). `CAMPROJ-05`/`06` hold the on-screen scale at the
target plane to 1e-4 in NDC and prove the round trip returns to its origin.

**Pinch had to be projection-aware, and this is the load-bearing part.** Moving
the eye along its own axis changes nothing in a parallel projection, so a
distance-based ortho zoom would look dead. Orthographic pinch therefore changes
the span and deliberately leaves the orbit distance alone — measured at runtime:
a real two-finger spread moved the span `4.7343 → 0.3809` with `distance=8.2000`
unchanged.

**Picking is structurally different per projection, not a tweaked constant.**
Perspective keeps one origin (the eye) with fanning directions; Orthographic
shares one direction (the view axis) with an origin that slides across the view
plane per pixel. Keeping a perspective origin under an orthographic image agrees
with the picture only at the screen centre and drifts toward every edge, so the
picking suite probes off-centre pixels and **round-trips each hit back through
the same matrices to the pixel it came from**. The orthographic view plane is
pulled back to `kFarPlane/2` (250 m) so the depth slab is centred on the target;
that costs nothing visually (a parallel projection is translation-invariant along
its axis) and buys the guarantee that every drawn surface is in front of the
pick-ray origin. The consequence to know is that `snapshot.eye` is not
`target + dir × distance` in Orthographic and a reported pick distance is
measured from that plane.

**Sculpt needed one change: the brush radius must not scale with depth in
Orthographic.** `worldPerPixelAtDepth` still reads `proj.m[5]` in both modes but
applies the depth factor only in Perspective. `CAMPROJ-11` measures both halves —
pushing the object ±3 m along the view axis leaves the orthographic radius
identical and does move the perspective one — so a brush frozen to a constant
could not pass either.

**Verification.** Ten self-test suites green (**1316 checks, zero failures**, up
from 1199); 26 JVM tests green; **44 instrumented tests, 43 green**. The one
failure is `EditorWorkspaceGestureTest.ui11_...` failing its own precondition
guard ("the soft keyboard did not appear, so this case proves nothing") and is
**proven pre-existing**: the same test was run alone on a stashed, unmodified
`171c7ae` tree and failed with the identical message. See the environment note
under *Android UI suites*.

**Runtime**, on `emulator-5558` through the real touch path, with the native
`CONSTRUCTION_PUBLISHED` line confirming the primitive before every capture:
Box 2 × 1 × 0.5, orbit in both projections, a real multi-touch pinch in both,
pan, picking, a transform-only edit (`rev=9` unchanged, no publish, no upload),
Freeze (482 : 2880), a real Grab in Perspective (51 moves) and a real Grab in
Orthographic (52 moves) with `src=482:2880` fixed and every upload `reuse`,
HOME/resume, portrait and rotated landscape.

*Picking measured in both modes at three pixels on a 1 m diameter × 2 m
cylinder.* Orthographic hits landed on the exact surface — `y = 1.0000` on the
top cap and radius `0.4976` on the wall against an exact 0.5 (the expected
faceting inset) — and the centre pixel returned the **identical** point
`(0.3336, 1.0000, -0.0600)` in both projections, which is the one ray the two
share. Off-centre pixels correctly returned *different* points.

*Multi-touch navigation still cannot mutate the sculpt mesh in Orthographic.*
The injected pinch logged `STROKE_PENDING → STROKE_ABANDONED:navigation` with
`sculptRev` held at 104.

*Rotated landscape holds the P2 convention under Orthographic*: window 2400×1080,
`chosenExtent=2400x1080`, `preTransform=0x1`, camera viewport 2400×1080, aspect
2.2222. `CAMPROJ-14` additionally asserts in **both** modes and **both**
orientations that a 1 m world square occupies the same pixel count horizontally
and vertically.

*HOME/resume preserved the projection*: re-requesting Orthographic after the
resume reported `changed=0` with `orthoHalfHeightMeters=4.7343` and
`distance=8.2000` intact. The projection is process-scoped state on the one
`CameraController`, so this needs no save/restore code in the Android layer.

**Visual evidence**, under `artifacts/stage015d_*` (foreground confirmed before
each capture). The three required pairs share a camera pose exactly, because the
switch preserves it: `_01`/`_02` the oblique three-face box, `_03`/`_04` the box
seen nearly along its 2 m axis, `_05`/`_06` a 1 dia × 2 m cylinder. In the
Perspective members the near end is visibly larger and parallel edges converge —
the cylinder's silhouette tapers. In the Orthographic members the box's top face
is a true parallelogram, near and far vertical edges are equal, and the
cylinder's sides are parallel with matching top and bottom ellipses.
Foreshortening from *orientation* remains in both and is correct. `_07`/`_08`
bracket the HOME/resume, and `_09` is rotated landscape.

**Performance.** Nothing was added to the frame path. A projection change writes
two floats under the existing `g_stateMutex` and mints no revision; no
`RENDER_MESH_BUILD` and no `MESH_UPLOAD_OK` line follows one. The projection
matrix is built in `snapshot()`, which was already built per frame.

## Stage 015C-R — root cause, convention and evidence

**Root cause, proven by measurement.** With `cullMode = BACK` and
`frontFace = CLOCKWISE`, the default box's visible faces measured the Studio
Solid luminance of its **far** walls, not its near ones — the culling was
inverted, double-compensating for the projection's Y flip, which is already
applied by the time Vulkan classifies a triangle. Inverted culling does not
blank the viewport or change the silhouette, only which surface of it is
drawn, so it presented as a shading complaint rather than a rasterizer defect.
After the fix the near-face pixels matched their Studio Solid predictions to
four decimals. The winding/normal/raster convention itself is owned by
`ARCHITECTURE.md` (*Canonical winding and culling*).

**The direction family (`NOR-01`..`NOR-10`)** closes the blind spot that let
this hide: every pre-existing normal check measured an axis or a magnitude and
so passed unchanged on a mesh whose normals had all been negated.
`nor_outwardness_fails_on_global_normal_flip` asserts the new measurement does
invert on a global flip, so the suite cannot regress into that blind spot
again. Ten suites green (1199 checks); 26 JVM tests; 40 instrumented tests. A
stationary 3371-frame run moved the render-data rebuild count by exactly one
(the Smooth toggle used to close the measurement). Screenshots are under
`artifacts/stage015cr_*`.

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

## Architecture

`ARCHITECTURE.md` is the authoritative ownership map. In one paragraph: a
standalone Android application (`com.forgeshape.app`) with a Java shell that owns
no domain truth, a plain `SurfaceView` viewport, a JNI boundary that carries
whole sections and semantic pointer samples, and a platform-neutral C++17 domain
(`ConstructionObject` and its six primitives, `ConstructionTransform`,
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
| One Construction Body with exact Box / Cylinder / Sphere / Cone / Capsule / Plane | VERIFIED |
| Plane: 4:6 open source topology, exact bounds, two-sided render and pick as one bounded, named exception | VERIFIED |
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
| One 40° crease policy gives all six primitives their hard/smooth contracts | VERIFIED |
| Smooth ↔ Faceted is presentation only, on the same source revision | VERIFIED |
| ForgeShape-generated MatCap from view-space normals; one preset, no asset file | VERIFIED |
| Studio ↔ MatCap rebuilds no geometry and uploads nothing | VERIFIED |
| Sculpt deformation relights immediately; no stale lighting, no NaN | VERIFIED |
| Selection stays obvious and form stays readable in both shading modes | VERIFIED |
| Display settings are native-owned and survive HOME/resume | VERIFIED |
| No per-frame normal or render-data rebuild: 4448 frames, 2 rebuilds | VERIFIED |
| 16 KB page-size runtime behaviour (x86_64) | VERIFIED (Gate P1) |
| 16 KB page-size runtime behaviour (physical ARM64) | **UNVERIFIED** — see Known Issues |

## Self-test suite

Ten debug-only native suites run once from `NativeViewport.start()` — never per
frame — and total **1421 checks, zero failures** at the accepted baseline under
NDK r29:

| suite token | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 124 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 94 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 125 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 276 |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 224 |

followed by `FORGESHAPE_MESH_UPLOAD_OK` and `FORGESHAPE_NATIVE_VIEWPORT_OK`.
The tenth suite covers the crease policy, all six primitives' Smooth contracts
and the capsule equality case, Faceted, NaN/Inf and fail-closed behaviour,
determinism, the render-data rebuild policy, the generated MatCap asset, the
display settings, the proof that building render data leaves the
authoritative `RuntimeMesh` bit-identical, and (Stage 016) the Plane's
two-sided render duplication and its inertness to display switching
(`PLN-09`/`10`/`20`). Stage 016 also added `PLN-01`..`08` to the primitive
suite, `PLN-11`..`16` to the picking suite, and `PLN-17`..`19` to the sculpt
suite.

Stage 015D added the **projection family**, `CAMPROJ-01`..`CAMPROJ-14`, split
across three suites by module ownership rather than kept in one file: the camera
suite carries `CAMPROJ-01`..`08` and `12`..`14` (the perspective audit, the
orthographic matrix, depth-independence versus foreshortening, both framing
conversions, orbit invariance, ortho pinch and clamps, purity, resume, and the
P2 orientation convention in both modes on a non-square viewport); the picking
suite carries `CAMPROJ-09`/`10`; the sculpt suite carries `CAMPROJ-11`. Each
family member has a counterpart that would fail if the behaviour collapsed to a
constant — `CAMPROJ-04` against `03`, and the perspective depth check against the
orthographic one — which is the property the `NOR` family was added for.

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
| `EditorWorkspaceControlsTest` | UI-01/02/03/04/05/06/13 — control sets, fields, validation, freeze wording, tools, presentation-only actions; `PLN-07`/`08` — the six-kind round trip and the Plane chip's no-native-call contract | 19 |
| `EditorWorkspaceLayoutTest` | UI-07/08/09/12 — measured viewport floor, landscape, expanded, collapse | 6 |
| `EditorWorkspaceGestureTest` | UI-10/11 — chrome gesture ownership, IME | 5 |
| `EditorWorkspaceLifecycleTest` | UI-14 — HOME/resume rebuilt from native truth | 3 |
| `DisplaySettingsContractTest` (JVM) | the shading/surface index contract across JNI | 4 |
| `EditorWorkspaceDisplayTest` | SHD-13/15/16 — display ids, presentation-only, resume; PROJ-12/13/14 — projection ids, inertness against domain state, refused index, resume | 12 |

**72 tests** (26 JVM, 46 instrumented). No Java test asserts a rendered pixel;
every control is reached by its stable semantic id and no assertion uses a screen
coordinate.

**46 of the 46 instrumented tests pass as of Stage 016** on `ForgeShape_Stage006`.

`EditorWorkspaceGestureTest.ui11_theImeLeavesTheFieldAndTheCommitPathUsableAndTheSurfaceUntouched`
is the one case that has moved in both directions across recent stages: it
opens with a precondition guard — "the soft keyboard did not appear, so this
case proves nothing" — and fails on that guard alone, never reaching an
assertion about product behaviour, whenever the keyboard is slow to appear. It
failed at the Stage 015C baseline, passed at Stage 015C-R, failed again at
Stage 015D (proven pre-existing there against a stashed, unmodified `171c7ae`
tree), and **passed at Stage 016**. Treat a future failure of this one case as
a harness symptom to confirm against the current baseline before calling it a
regression.

## Current evidence summary

- **Gate P1 (PARTIAL)**: arm64-v8a build support, real 16 KB runtime
  verification (x86_64), an active Vulkan validation path (zero
  ForgeShape-caused messages) and a debug-only heavy-mesh density fixture are
  all closed and evidenced. Physical ARM64 launch/lifecycle (P1-A) and the
  mandatory 10k/50k/100k mesh-density ladder (P1-E) are BLOCKED pending a
  physical ARM64 device; stylus (P1-D) is UNVERIFIED for the same reason.
  Full detail, the GP1 criteria table and screenshots (`artifacts/gatep1_*`)
  are in the Gate P1 chapter at the top of this file.
- **Stage 016-R2 acceptance** (`ForgeShape_Stage006` / `emulator-5580`): device
  and test-harness remediation only, zero product behaviour change. Ten
  self-test suites green (1421 checks, zero failures) both on a clean launch
  and after 46 in-process reruns under UI-mutated Construction state during
  the full instrumented suite; 26 JVM tests green; 46 instrumented tests
  green; `DEV2-01`..`07` PASS; zero `emulator-5554` interaction. Full detail in
  the Stage 016-R2 chapter at the top of this file. Screenshots are under
  `artifacts/stage016r2_*`.
- **Stage 016 acceptance** (`ForgeShape_Stage006`): ten self-test suites green
  (**1421 checks, zero failures**, including `PLN-01`..`PLN-20`); 26 JVM tests
  green; **46 instrumented tests, 46 green**. The Plane contract, the two-sided
  render/pick exception, the primitive-coverage cleanup and the full runtime
  walkthrough (Apply, unit round-trip, transform, front/back pick, both
  projections, both shading models, Freeze/Resume/stale-source, a real sculpt
  stroke on a dense primitive, HOME/resume, rotated landscape) are in the
  Stage 016 chapter at the top of this file. Screenshots are under
  `artifacts/stage016_*`.
- **Stage 015D acceptance** (`emulator-5558`): ten self-test suites green
  (**1316 checks, zero failures**, including `CAMPROJ-01`..`CAMPROJ-14`); 26 JVM
  tests green; 44 instrumented tests with the one environment-dependent IME case
  described above, proven pre-existing against a stashed `171c7ae` tree. The
  measured framing conversions, the ortho pinch, the picking comparison and the
  runtime smoke are in the Stage 015D chapter at the top of this file.
  **The default appearance did not change**: Perspective is still the default and
  its 60° field of view is untouched, so a cold start is identical to the
  Stage 015C-R baseline. Screenshots are under `artifacts/stage015d_*`.
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
  IME case described above. **The default appearance changed**: the per-vertex
  rainbow is gone and the viewport comes up in neutral Studio Solid, with the old
  appearance still reachable in a debuggable build as the Debug chip. The
  per-primitive source-vs-render counts it established are in the *Shading cost
  record* below. **The no-per-frame-rebuild proof is a direct measurement**:
  `rebuilds=2 frame=4448` — over 4448 presented frames including eight
  camera-orbit gestures and three shading-model changes, the render mesh was
  rebuilt twice, and the log carries **zero** `RENDER_MESH_BUILD` and **zero**
  `MESH_UPLOAD_OK` lines during camera motion and Studio↔MatCap churn. A
  Smooth↔Faceted round trip on **one unchanged source revision** moved the render
  count 24 → 36 → 24 with `src=8:36` throughout, and every one of the 54 uploads
  in the session was `reuse` with the buffer-grow count flat. Screenshots are
  under `artifacts/stage015c_*`, with `artifacts/stage015c_shading_comparison.md`
  as the comparison sheet.
- **Platform Fix P2 acceptance** (`emulator-5558`, cold boot, clean install,
  empty crash buffer): nine self-test suites green (992 checks, zero failures);
  54 Android tests green (22 JVM, 32 instrumented). **Root cause proven, not
  assumed.** The first inconsistent convention was the **swapchain extent
  space**, not the projection: declaring a 90° pre-transform obliges the image to
  be in pre-transform (panel) space, so SurfaceFlinger rotated the 2400×1080
  buffer to 1080×2400 and stretched it back to the window. That model predicted a
  2 × 1 × 0.5 m box at 427 × 98 px; it measured **428 × 98**, against 218 × 192
  when correct. The fix requests an identity pre-transform, after which the
  pose-fixed invariant `bboxWidth / screenHeight` agreed at 0.2017 / 0.2019 /
  0.2025 across portrait, rotated landscape and a non-rotated wide window, inside
  0.5 %, and a default sphere measured square in both orientations. Picking
  stayed aligned with what is drawn, and five orientation changes plus a
  HOME/resume published **no mesh revision, triggered no upload and started no
  stroke**. The resulting convention is owned by `CLAUDE.md` and
  `ARCHITECTURE.md`; the full measurement tables are in Git history. Screenshots
  are under
  `artifacts/platformfixp2_*`.
- **Stage 015B acceptance** (`emulator-5558`, clean install, empty crash
  buffer): nine self-test suites green; 54 Android tests green; the measured
  unoccluded viewport in landscape moved from **0 % to 60.1 %** (82.6 %
  collapsed at 411×914 dp, 85.2 % at 1280×800 dp). Chrome drags left the viewport
  region **pixel-identical** and minted no sculpt revision; the re-Freeze
  confirmation named "1 sculpt stroke" with Cancel making no native call; unit
  switching was exact with zero native calls; and HOME/resume returned a
  pixel-identical viewport with no re-upload. The full measurement tables are in
  Git history. Screenshots are under `artifacts/stage015b_*`.
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

- **16 KB page-size runtime behaviour is VERIFIED (Gate P1)**, on the x86_64
  `ForgeShape_16K` AVD (`getconf PAGE_SIZE` = 16384): full self-test suite,
  Construction/picking/projection/shading/Freeze/Resume/Sculpt smoke all
  green. **Still open: real 16 KB behaviour on physical ARM64 hardware**,
  which needs the owner to provide a physical ARM64 device (an explicit adb
  serial) — see the Gate P1 chapter above for the exact blocker.
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

**Primitive surface.** Adding a primitive still costs four parallel edits — a
member on `ConstructionObject`, a `PrimitiveKind` case, a variant alternative,
and a JNI method plus its Java declaration. That remains deliberate and visible
rather than hidden behind a registry. Stage 016 paid down the one instance of
it that dispatched with an if-else chain over typed accessors:
`ConstructionObject::setPrimitive`'s update half now `std::visit`s the
requested payload against two small per-kind overload sets
(`parametersDiffer` / `writeParameters`), so a kind added to the variant with
no matching overload is a compile error rather than a branch that is silently
never taken — the same property the pre-existing `validateParameters` overload
family already had. The plain `switch (kind_)` statements elsewhere
(`spec()`, `generateMesh()`, `primitiveKindName`) were left as they were: each
is one line per kind, visually complete at a glance, and — Stage 016 found one
real instance of exactly this risk in `forgeshape_jni.cpp`'s separate
`describeSpec` if-else chain, which had no Plane branch until the runtime pass
caught it — a `switch` with a trailing fallback is a smaller, more visible
version of the same gap than an if-else chain was, not a solved one; a future
stage that wants it closed for these too has a proven pattern to reuse. Every
primitive's parameters are always resident even though one is active.
Tessellation is a compile-time constant, so a very large
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
but four are over their preferred target budgets: `ARCHITECTURE.md` 1714
against a 700–1000 target, `PROJECT_STATUS.md` 1061 against 500–800,
`PRODUCT.md` 609 against 300–450, `README.md` 320 against 150–250.
(`CLAUDE.md` 180 is inside its.) Stage 016 paid part of this back rather than
only adding to it: the Stage 015C-R chapter here was compacted from 49 lines to
15, keeping its conclusions and moving its measurement detail to Git history,
the same pattern Stage 015D used on the chapters before it. The remaining
overshoot predates this stage and is concentrated in `ARCHITECTURE.md`, where
closing it means compacting prose about shading, sculpt and layout that a
primitive-only stage does not own. That is a deliberate deferral, the same one
Stage 015D recorded: bundling a documentation rewrite into a stage whose scope
is one primitive is the unrelated-debt mixing the rules forbid.

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

**Camera and projection.** Three things are deliberate but worth naming. The
orthographic view plane sits a fixed `kFarPlane/2` in front of the target, which
makes `snapshot.eye` mean something different in the two modes and makes a
reported orthographic pick distance ~250 m rather than a distance from the orbit
eye; the field is only ever used as a ray origin and a view reference, so this is
correct, but it is a name that no longer describes both cases equally well and a
future stage that adds a second camera should rename it. The ortho depth slab is
a fixed 500 m rather than fitted to the scene — free at these sizes because ortho
depth is linear, but it is a constant, not a policy. And
`kInitialOrthoHalfHeightMeters` is a literal because `std::tan` is not
`constexpr`; a self-test asserts it still equals `kInitialDistance × tan(fovY/2)`
so the two cannot drift, but a computed constant would be better than a checked
one. Separately, the camera still has no read-back of pose across JNI, so
"a chrome gesture did not move the camera" is still proven by screenshot rather
than by assertion — the new projection getter is the first camera value the Java
layer can read at all.

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
| `app/src/main/java/.../DisplaySettingsPopoverView.java` | The compact display popover: Shading (Studio / MatCap / Debug), Surface (Smooth / Faceted) and Projection (Perspective / Orthographic), with short interruptible open/close motion that honours the system animator scale. Owns no state |
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
| `app/src/main/cpp/forgeshape_camera.{h,cpp}` | Camera pose, the `ProjectionMode` enum and the orthographic world span, both projections, the framing-preserving switch, gesture state machine |
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all five primitives + transform), the five `Construction*` generators, shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation including `validateCapsuleMeters`, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
| `app/src/main/cpp/forgeshape_transform.{h,cpp}` | `ConstructionTransform`: authoritative double-meter position and double-degree rotation, THE axis/Euler convention, validation, atomic apply, derived model and inverse-model matrices |
| `app/src/main/cpp/forgeshape_sculpt.{h,cpp}` | `ProductMode`, `SculptTool`, `SculptSession` (mode + tool + brush + live stroke + the `hitsSculptMesh` probe), `SculptMesh`, `SculptTopology`, `computeVertexNormals`, `SculptStroke` (the one kernel plus one `apply*` per tool), sculpt publication |
| `app/src/main/cpp/forgeshape_picking.{h,cpp}` | Screen→world ray for **both** projections (perspective: one origin, fanning directions; orthographic: one direction, per-pixel origin), `transformRayToLocal`, ray/triangle, nearest hit, winding check |
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

**Gate P1 — Physical ARM64 + 16 KB Runtime + Vulkan Validation + Heavy-Mesh
Baseline**

Construction is now complete at six primitives, and every product-side
verification (self-tests, JVM, instrumented, real touch-path runtime) has been
run only on `x86_64` emulators. Gate P1 is the platform-evidence gate deferred
since Gate P0: build and run on **physical ARM64 hardware**, close the **16 KB
page-size runtime** dimension left `UNVERIFIED` under *Known Issues* (static/
ELF/APK evidence already passes; only a real 16 KB device or system image can
close it, and installing one is not yet authorized), enable the **Vulkan
validation layers** for at least one full session to catch anything the
current always-`x86_64`, no-validation-layer runtime evidence could not, and
establish a **heavy-mesh baseline** — sculpt and render-data-rebuild cost
measurements at a vertex count well past the current 482–514 range, so a
future performance stage has a real number to beat instead of an assumption.
This is a platform/verification gate, not a feature stage: no new primitive,
no Sketch/Extrude, no camera or shading change, and no product behaviour is
expected to differ on ARM64 — the gate exists to prove that, not to change it.
