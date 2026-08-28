# CLAUDE.md — durable rules for this repository

Short and durable. Not a stage diary.

## Workspace

`D:\TRAVELAPPS\ForgeShape` — standalone Android app, Java shell + native C++17
Vulkan renderer.

## Commands

```
gradlew.bat :app:assembleDebug
adb -s <serial> install -r app\build\outputs\apk\debug\app-debug.apk
adb -s <serial> shell am start -n com.forgeshape.app/.ForgeShapeActivity
adb -s <serial> logcat -s ForgeShape:V
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

A clean debug launch emits **thirteen** `*_SELFTEST_OK` tokens, then
`FORGESHAPE_NATIVE_VIEWPORT_OK`. All thirteen, in emission order:

```
FORGESHAPE_CAMERA_SELFTEST_OK
FORGESHAPE_PICKING_SELFTEST_OK
FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK
FORGESHAPE_CONE_CAPSULE_SELFTEST_OK
FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK
FORGESHAPE_RENDER_SHADING_SELFTEST_OK
FORGESHAPE_SCENE_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK
FORGESHAPE_GIZMO_SELFTEST_OK
```

Failures: `FORGESHAPE_NATIVE_VIEWPORT_FAIL:*` and the matching `*_SELFTEST_FAIL`.
Grep for `FAIL` alone over-matches: some passing check *names* contain "fails"
(`invalid_revision_fails_closed`). Match `_SELFTEST_FAIL` or `_FAIL:`.

Enlarge the log ring buffer (`adb -s <serial> logcat -G 16M`) before capturing
startup evidence: the default buffer drops part of the self-test output and it
looks like a suite that stopped partway.

Camera, picking, dynamic-mesh, Construction-box, sculpt, render-shading,
Construction-history and gizmo self-tests are debug-only and run once from
`NativeViewport.start()`. They must never run per frame. Each builds the domain
objects it needs — the scene, history and gizmo suites build their own
`ConstructionScene` — rather than reading process-scoped state, so a suite's
result never depends on what a live session left behind.

## Hard rules

- **No engine.** Godot, GDExtension, Unity, Unreal or any general-purpose engine
  that owns the viewport or render loop is prohibited in the product.
- **No third-party runtime, rendering, math or input library.** Math lives in
  `app/src/main/cpp/forgeshape_math.h`. No GLM.
- **Git is local only.** The root repository exists (initialized in Gate P0 with
  owner approval). Commits and branches are allowed. **No remote, no push, no
  GitHub repository, and no global Git config changes** — the committer identity
  lives in `.git/config` alone. Never commit generated build output.
- **The NDK is pinned to `29.0.14206865`** in `app/build.gradle`. Do not bump it,
  do not use an r30 beta, and do not broadly upgrade AGP, Gradle, the JDK or
  CMake as a side effect of anything else.
- **Read `PROJECT_STATUS.md` first, and update it yourself** before reporting a
  stage complete. It records only verified current facts and names exactly one
  next stage.
- **No scope expansion.** Implement what the stage asks for and stop. Do not
  start the next stage.
- **Preserve existing owner changes.** Modify only what the task requires; do not
  opportunistically refactor the renderer or bundle unrelated debt fixes.
- **One brush kernel, no brush framework.** A sculpt tool is a deformation rule
  inside `SculptStroke`, selected by a closed enum and a switch. Do not add a
  brush base class, registry, plugin surface or reflection: brushes are code, not
  data, until a stage pays for making them data.
- **A gesture that becomes multi-touch navigation must never mutate the sculpt
  mesh.** No vertex written, no `SculptRevision` minted, no stroke committed.
- **The Construction Source is never written by sculpting.** The object has two
  representations — the exact primitive plus its parameters and placement, and
  the Frozen Sculpt Mesh. A sculpt edit may never change a primitive parameter,
  a `PrimitiveKind` or a transform, and no Construction parameter may ever be
  reconstructed from sculpt vertices. Adopting a changed Construction shape into
  the sculpt mesh is always an explicit user act, never automatic.
- **There is one Construction history, it is native, and a user act is one
  transaction.** `ConstructionHistory` owns undo/redo and the transaction
  boundary; no Java-side mirror scene, snapshot list or depth counter may become
  a second answer. A step holds bounded Construction-domain state — never mesh
  bytes, never a sculpt vertex or `SculptRevision`. A multi-mutation act opens
  ONE edit around its mutations rather than recording each; a commit that finds
  nothing different records nothing and must leave the redo stack alone. The
  `ObjectId` allocator is never rolled back: an undone creation's id is restored
  by name, never reused. **Sculpt has no undo**, and Construction undo may never
  move a sculpt vertex — the only sculpt state a restore touches is the existing
  stale-source flag. **Seeding a session is not a user act**: what a new session
  does to reach the state the user starts from runs inside
  `beginSessionInitialization`/`end`, records nothing, and leaves an empty
  history both ways. That bracket is reachable only from the start answer —
  never from Back to Construction, a rotation or a resume — and the debug-only
  `clear()` stays a test seam, never production behaviour.
- **A viewport handle has one owner, one scale and no transform of its own.**
  Direct manipulation writes the active body's authoritative
  `ConstructionTransform` throughout the drag, so the renderer, the picker and
  the exact-value editors never disagree; no second solver, pivot or placement
  may exist above JNI. The pivot is the body's Construction placement origin —
  never a mesh AABB centre, a screen centroid or a camera-facing proxy. Drawing
  and hit-testing share ONE camera-derived scale, so what is seen and what can
  be grabbed cannot differ. One drag is one captured `pointerId` and one
  transaction: a second pointer or a cancel restores the pre-drag state and
  records nothing, and a gesture that starts off a handle still navigates
  exactly as before. A degenerate viewpoint holds the last good value rather
  than guessing — no NaN, no jump. The camera alone sizes the instrument: the
  BODY's scale never reaches it, and a drag's basis is frozen at the pointer
  down and is scale-free.
- **A rotation is composed as a matrix and stored as Euler degrees.** A handle
  drag may never add its angle to one Euler component — that is only correct
  when the other two are zero. Every sample composes from the IMMUTABLE start
  orientation (`Relem·R_start` for World, `R_start·Relem` for Local) and comes
  back through the one branch-continuous decomposition in
  `forgeshape_transform.h`, which is the ONLY bridge between the two forms. A
  correct rotation on a mixed orientation legitimately moves more than one Euler
  field, so a test asserts an ORIENTATION and never "one ring, one field".
- **Scale is a transform multiplier, never a dimension.** It is unitless, has no
  display unit, is strictly positive (zero is singular and negative is a Mirror
  this product does not have — both are refused, never clamped), and it is
  LOCAL-only, because a world-axis scale of a turned body is a shear no diagonal
  `S` can express. `Model = T·Rz·Ry·Rx·S`; it writes no primitive parameter and
  publishes no `MeshRevision`. Anything consuming the transform must be
  non-uniform-scale correct: normals ride `R·S⁻¹`, and picking's local ray
  direction stays un-normalized so its parameter is still world distance.
- **Shading is presentation, never truth.** Normals, the derived render mesh and
  every display setting are one-way products of a published `RuntimeMesh`.
  Nothing may read a dimension, a Construction parameter, a sculpt deformation,
  picking topology or an `ObjectId` back out of them, and no display change may
  mint a revision. Render-only vertex duplication for hard edges is expected, so
  render and source counts differ and a diagnostic must say which it means.
- **The Vulkan viewport is full-bleed; only chrome is inset.** The `SurfaceView`
  gets the whole window, and no layout decision, window inset or IME may inset,
  pad or resize it — that would change the render target and rebuild the
  swapchain for a problem about where buttons are drawn. Insets go on the chrome
  containers.
- **No surface owns the resting workspace.** The viewport is the workspace; the
  exact-value panel and every context surface are opened from a control, grow out
  of it, and are absent otherwise. Nothing may reintroduce a permanently visible
  panel anchored to a window edge — a collapsed panel is still one. A surface may
  stand on the *model*; it may never partially cover another live control, and
  moving it in front by z-order is not a fix — the control is still under it and
  still taking touches.
- **Chrome reports, it does not instruct.** The status line carries verdicts and
  standing faults only; a message that is always true is a permanent surface
  whatever its timeout says, and the rail entry, the control that opened a panel
  and the panel's own title already answer where the user is. And **critical
  navigation is never abbreviated where the row can carry it**: a transition's
  width is arithmetic on its row, not a constant, and the only approved second
  form is `← Construction`, with the full wording kept as the content
  description.
- **Nothing unimplemented is drawn as a tool or as a creation action.** The one
  approved exception is the global `Export`, drawn recessed and labelled as not
  implemented. A control that looks like it works and does not is worse than an
  absent one.
- **Every UI control has a stable semantic id, and verification uses it.** Ids
  live in `res/values/ids.xml` and name what a control *does*. No test and no
  evidence script may locate a control by screen coordinate: the workspace
  re-arranges itself per window, so a coordinate is only ever true for one run.
  An id names the act the code performs, not the label, so copy never renames one.
- **48 dp is the interactive floor for user-operated chrome, and it is the HIT
  AREA.** Glyphs stay the size they read at; the floor is reached with padding,
  never by growing a drawn box.
- **A control that cannot succeed is not drawn.** Where the domain refuses an act
  in some state — creation while sculpting — the control is absent there rather
  than shown and then refused. The domain guard stays: removing a control is not
  removing a guard.
- **A control's corner is concentric with its host's** (`inner = outer − gap`),
  or a crescent of the host shows at each end and reads as a rendering fault.
- **The domain is platform-neutral; the Android layer is an adapter.** Android is
  the first and only production platform, and no Apple target, Metal backend,
  MoltenVK dependency, Xcode project or cross-platform UI framework is
  authorized. But Construction, geometry, sculpt, picking, camera and selection
  code stays platform-neutral C++: no `View`, `Activity`, `MotionEvent`,
  `Surface`, `jobject` or other Android/JNI type may become domain truth, they
  stop at `forgeshape_jni.cpp`, input crosses the boundary as platform-neutral
  semantic samples (`forgeshape_input.h`), and the renderer's platform-surface
  coupling stays one explicit seam. Do not build a portability abstraction for a
  platform that has no target — keep the seams where they are.

## Naming and comments

- **Owner-facing documentation** uses plain descriptive names first, with any
  shorthand in parentheses after it. A reader must never need project-history
  jargon.
- **Production code** uses responsibility-based names that say what the type or
  function owns or does. No stage numbers in production names. Avoid `Manager`,
  `Handler`, `Layer`, `Thing` and `Generic` unless the full name makes the
  responsibility genuinely clear. One domain concept has one canonical term.
- **The UI vocabulary is fixed:** *Editor Workspace* (the whole editor UI),
  *Global Toolbar* (mode-independent top/global controls), *Tool Rail* (the edge
  tool selector), *Objects capsule* (the resting scene control: the active body's
  name plus creation), *Add Primitive* (the six-shape creation surface),
  *precision surface* (the on-demand exact-value panel, implemented by
  *Property Inspector*), *anchored surface* (any panel that grows out of the
  control that opened it; `AnchoredSurfaceView` owns the growth for all of them),
  *Construction Body* (an editable CAD-like object), *Frozen Sculpt Mesh* (the
  polygon mesh `SculptMesh::freezeFrom` creates), *history capsule* (the bottom
  trailing capsule holding Undo and Redo), *transform mode selector* (Move /
  Rotate / Scale) and *coordinate-space selector* (World / Local) — two capsules
  in one *selector row* under the Tool Rail, contextual to Transform and absent
  everywhere else.
- **The user never reads "Freeze".** *Freeze*, *re-Freeze* and *Frozen Sculpt
  Mesh* stay in the C++, the view ids and the architecture docs, because they name
  what the operation does. Every user-facing string says **Start Sculpting**,
  **Reset Sculpt from Shape…** and **sculpt mesh** instead; **Back to
  Construction** and **Resume Sculpt** are unchanged. Two readers, two
  vocabularies, and neither may be renamed into the other. `UIR4B-15` enforces
  the user-facing half over every `R.string`.
- **Comments explain why**, plus ownership, units, lifecycle and constraints —
  never obvious syntax. Worth a comment: why the Android UI must not become
  geometry truth, why a transform-only edit publishes no `MeshRevision`, why a
  pointer gesture is consumed before JNI, why a platform-neutral input boundary
  exists.
- Do not mass-rename working code to satisfy this. Rename when already touching
  the area, or when an ambiguity is a real maintenance risk.

## Documentation ownership

One durable fact has exactly one primary owner.

| File | Owns |
| --- | --- |
| `PROJECT_STATUS.md` | current status, verified capability, environment facts, next stage |
| `ARCHITECTURE.md` | current production architecture and module ownership |
| `PRODUCT.md` | runtime-verified user-visible behaviour only |
| `README.md` | required tooling, build/run/verify instructions |
| `CLAUDE.md` | these rules |

Never document a feature as implemented unless it is runtime-verified. Unverified
paths are reported as UNVERIFIED, not as behaviour.

**Keep documentation navigable and current.** There is no raw line-count cap. A
document is too long when it is hard to navigate or carries text that is no
longer true — never merely because of its physical line count. When adding a
fact, replace the superseded or duplicated prose rather than appending a new
chapter beside it, and delete migration rationale once the invariant it explains
stands on its own. Use Git history for old stage detail; do not create a shadow
history Markdown file. Do not compact by deleting ownership or invariant detail
the code depends on: that is a regression, not a saving.

## Android / Vulkan safety

- The emulator dies if launched as a child of a tool shell. Spawn it detached
  (e.g. WMI `Win32_Process.Create`).
- When more than one Android target is connected, every `adb` command must use
  an explicit `adb -s <serial> ...`. Never rely on the default target.
- `AVD Medium_Phone_API_36.1` / `emulator-5554` is **reserved by another program**
  and must not be used, started, stopped, modified, installed to, logged,
  screenshotted or sent input by ForgeShape work, until the owner lifts this.
- `AVD ForgeShape_Stage004` / `emulator-5556` is **contended**: another program
  runs on it, steals the foreground and injects input. Do not use it for
  authoritative runtime evidence, and do not stop, wipe or reconfigure it.
- Runtime evidence is taken on a ForgeShape-owned isolated AVD (currently
  `ForgeShape_Stage006`, port varies — see below). Before any evidence-sensitive
  input or screenshot, confirm ForgeShape is the resumed activity, and
  invalidate any run contaminated by foreign input. Creating a new AVD from
  already-installed tooling is allowed; installing or updating SDK/NDK/JDK/
  system images is not.
- **Never trust a port or serial number to say which AVD is behind it.** The
  emulator assigns the first free port starting at `5554` to whichever
  instance boots first, so `ForgeShape_Stage006` can itself land on
  `emulator-5554` by pure allocation order — confirmed directly during Stage
  016-R. Always confirm with `adb -s <serial> emu avd name` before treating a
  serial as safe. Boot it only with `scripts\start-forgeshape-emulator.ps1`
  (defaults `-Avd ForgeShape_Stage006 -Port 5580`): it hard-rejects port
  `5554` before touching the OS or adb at all, checks occupancy of ONLY the
  requested port, BLOCKS with no automatic fallback port if that port is
  already taken, launches detached via WMI/CIM `Win32_Process.Create`, and
  confirms AVD identity with `emu avd name` before reporting ready. An
  unavailable port is BLOCKED, never a reason to try another one.
- **Bare, unscoped `connected*AndroidTest` is forbidden whenever more than one
  Android target could be attached.** That Gradle task enumerates every
  attached device with no default and installs/runs on all of them — this is
  exactly how Stage 016 touched the reserved `emulator-5554`. The one
  supported runner is `scripts\run-instrumented-tests.ps1 -Serial <serial>`,
  which requires an explicit serial, refuses `emulator-5554` before any device
  is contacted, checks readiness with `adb -s <serial> get-state` (never a
  bare `adb devices` enumeration), and drives `adb -s <serial>` explicitly for
  every install and instrumentation step — see `README.md`. No repo script
  under `scripts\` issues a bare, unscoped `adb` call;
  `scripts\verify-device-guards.ps1` checks this and the port/serial guards
  above mechanically (`DEV2-01`..`07`) and needs no device attached.
- Instrumented full-suite policy: runner execution with no filter is the
  supported monolithic full-suite path; `-FullSharded` is the supported
  authoritative exhaustive-sharded full-suite path. `-FullSharded` must use
  live AndroidJUnitRunner discovery, prove a deterministic exactly-once union,
  require a valid successful result from every shard, and emit
  `FULL_SHARDED_SUITE_PASS` only for the complete aggregate. `-TestClass` is
  focused/subset evidence and can never emit that marker. After any
  infrastructure crash/abort during `-FullSharded`, recover the isolated AVD
  and rerun the whole `-FullSharded` command from shard 1; class or shard-only
  reruns are supplementary and cannot repair an aggregate.
- `surfaceDestroyed` must block until native code has released the
  `ANativeWindow`. Never let the render thread touch a destroyed window.
- **One orientation convention: render in Android window orientation.** The
  swapchain requests an identity `preTransform` and takes its extent from the
  window, so window size, camera viewport, projection aspect, swapchain image and
  picking all share one coordinate space. Do not pre-rotate, do not transpose an
  extent, and do not add a display rotation to a matrix. The convention makes
  `VK_SUBOPTIMAL_KHR` the expected steady state on a rotated display, so the
  frame loop must **not** rebuild the swapchain on it — rebuild on
  `VK_ERROR_OUT_OF_DATE_KHR` and on the explicit resize request only. Ignoring
  this rebuilds the swapchain every frame while the device is rotated.
- Shaders are compiled ahead of time by the NDK `glslc` in CMake. No shader
  compiler ships at runtime.
- **A Vulkan validation layer is never bundled into the APK or committed to
  the repo.** When debugging needs it, fetch the official Khronos build
  (`github.com/KhronosGroup/Vulkan-ValidationLayers` releases) into a
  scratch/temp location, `adb push` it and enable it through Android's
  first-party per-app GPU debug layer settings
  (`adb shell settings put global enable_gpu_debug_layers 1` /
  `gpu_debug_app` / `gpu_debug_layers` / `gpu_debug_layer_app`), then delete
  those settings afterward. Never add it under `jniLibs`, never make it a
  Gradle dependency: it must stay a session-scoped, adb-only debug tool with
  zero footprint in the built product.
