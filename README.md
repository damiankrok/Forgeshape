# ForgeShape

Author-owned, offline-first 3D modeling/sculpting application for Android.
Android shell + ForgeShape-owned viewport + native C++17 Vulkan renderer.
No game engine, and no third-party runtime, rendering, math or input library.

- `PRODUCT.md` — what the app does, as verified at runtime.
- `ARCHITECTURE.md` — who owns what, and where the boundaries are.
- `PROJECT_STATUS.md` — current status, environment facts and the next stage.
- `CLAUDE.md` — durable repository rules.

## Required local tooling

| Tool | Version used |
| --- | --- |
| JDK | 21 (Android Studio JBR, `JAVA_HOME`) |
| Android SDK platform | android-36 (`compileSdk 36`) |
| Android build tools | 36.1.0 |
| Android NDK | 29.0.14206865 (pinned; install with `sdkmanager --install "ndk;29.0.14206865"`) |
| CMake | 3.22.1 (SDK-provided) |
| Gradle | 8.14.3 (wrapper) |
| Android Gradle Plugin | 8.13.2 |

`local.properties` must point at the Android SDK (`sdk.dir`). `minSdk` is 26,
`targetSdk` 36, and the debug build has an ABI filter of `x86_64` only (the
available emulator target).

Shaders are compiled ahead of time to SPIR-V by the `glslc` that ships with the
NDK (`$ANDROID_NDK/shader-tools/<host>/glslc`), invoked from CMake with
`-mfmt=c`; the emitted C initializer lists are `#include`d into the renderer.
No shader compiler is bundled at runtime and no SPIR-V asset is loaded.

## Build

```
gradlew.bat :app:assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

## Booting the emulator

**Never boot `ForgeShape_Stage006` (or any ForgeShape AVD) without an explicit
`-port`.** The emulator hands the first free port starting at `5554` to
whichever instance boots first, so an unpinned launch can land on the reserved
`emulator-5554` by pure allocation order — this happened during Stage 016-R.
The one supported launcher is:

```
scripts\start-forgeshape-emulator.ps1
scripts\start-forgeshape-emulator.ps1 -Avd ForgeShape_Stage006 -Port 5580
```

It rejects port `5554` before touching the OS or adb, checks occupancy of only
the requested port (never `5554`, never any other), BLOCKS with no automatic
fallback port if that port is taken, launches detached (WMI/CIM
`Win32_Process.Create` — the emulator dies if spawned as a child of this or
any tool shell), and confirms AVD identity with `adb -s <serial> emu avd name`
before reporting the device ready. If the AVD is already running on that port
from an earlier session, confirm identity the same way and reuse it; the
launcher itself will report the port BLOCKED rather than starting a duplicate.

## Test

```
gradlew.bat :app:testDebugUnitTest         # JVM: layout rules, UI state, units
```

**Never run bare `gradlew.bat :app:connectedDebugAndroidTest` when more than one
Android target could be attached.** That task enumerates every attached device
with no default and installs/runs the full suite on all of them — this is how
Stage 016 touched the reserved `emulator-5554`. The one supported path for the
instrumented suite is:

```
scripts\run-instrumented-tests.ps1 -Serial <serial>
```

It requires an explicit `-Serial`, refuses `emulator-5554` before contacting
any device, checks readiness with `adb -s <serial> get-state` (never a bare
`adb devices` enumeration), builds the debug app and test APKs, then installs
and instruments through `adb -s <serial>` only — every device operation is
scoped, mechanically, not by operator discipline. It uninstalls the test APK
when it finishes, matching `connectedDebugAndroidTest`'s own cleanup
behaviour, so reinstall the app before taking further runtime evidence.

`scripts\verify-device-guards.ps1` runs the DEV2-01..07 and DEV3-01..06
device-isolation guard checks without needing any device attached and without
ever contacting `emulator-5554` — safe to run any time as a quick sanity check
on the two scripts above.

DEV2 covers forbidden port/serial rejection and scoped instrumentation. DEV3
scans the executable workflow surface — `scripts\*.ps1`, any `.cmd`/`.bat`/
`.sh`, and the Gradle files — for an unscoped `adb` in **any** form (`adb …`,
`& adb …`, or an argument array that cannot be shown to carry `-s`), and for
an executable `connected*AndroidTest` fan-out. It reads through string
literals and comments, so documenting a forbidden command is not mistaken for
running one, and it reports which surfaces it scanned so a check that silently
covered nothing cannot pass.

To run one class:

```
scripts\run-instrumented-tests.ps1 -Serial <serial> -TestClass com.forgeshape.app.EditorWorkspaceLayoutTest
```

The layout suite logs its measurements as `FORGESHAPE_UI_VIEWPORT` lines, so the
unoccluded-viewport percentage for the window it ran in is in logcat, not only in
a pass/fail. It adapts to the window it finds itself in, so running it under a
display override is a genuine expanded-layout run:

```
adb -s <serial> shell wm size 1280x800 && adb -s <serial> shell wm density 160
... run the suite ...
adb -s <serial> shell wm density reset && adb -s <serial> shell wm size reset
```

Test-only dependencies (JUnit, `androidx.test` core/runner/ext-junit) are
`test`/`androidTest` scope and are packaged into the test APK only. The product
APK still has no runtime dependency of any kind.

## Install and run

When more than one emulator/device is attached, always target one explicitly
with `adb -s <serial>`; never rely on the default target.

```
adb -s <serial> install -r app\build\outputs\apk\debug\app-debug.apk
adb -s <serial> shell am start -n com.forgeshape.app/.ForgeShapeActivity
```

## Verify a run

```
adb -s <serial> logcat -G 16M          # do this FIRST
adb -s <serial> logcat -s ForgeShape:V
```

Enlarge the ring buffer before capturing startup evidence: the self-test suites
emit several hundred lines in a few milliseconds and the default buffer silently
drops the tail, which reads exactly like a self-test that stopped partway
through. That is a logging limit, not an app failure.

A clean debug launch emits **ten** `*_SELFTEST_OK` tokens, in this order, then
`FORGESHAPE_NATIVE_VIEWPORT_OK` once the first frame is presented:

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
```

Each suite reports `(<n> checks)` and fails as `<SUITE>_CASE_FAIL:<name>` plus
`<SUITE>_FAIL`; the viewport fails as `FORGESHAPE_NATIVE_VIEWPORT_FAIL:<reason>`.
Grepping for `FAIL` alone over-matches, because some passing check *names*
contain "fails" — match `_SELFTEST_FAIL` or `_FAIL:`. The self-tests are
debug-only and run once from `NativeViewport.start()`; none runs per frame.

### Checking that shading normals are not rebuilt per frame

Derived render geometry must be rebuilt only when the source mesh revision or the
Smooth/Faceted choice actually changed. One `FORGESHAPE_RENDER_MESH_BUILD` line is
emitted per rebuild and never per frame, and it carries the frame counter so the
two are directly comparable.

```
adb -s <serial> logcat -d -s ForgeShape:V | grep RENDER_MESH_BUILD
```

```
FORGESHAPE_RENDER_MESH_BUILD:8 src=482:2880 render=482:2880 shading=Smooth \
    rebuilds=2 frame=4448 ms=1.015
```

`src` is the authoritative `RuntimeMesh`; `render` is the derived mesh the GPU
holds, which differs wherever a hard edge forced a corner to split. To check the
policy, orbit the camera and switch Studio<->MatCap for a while, then confirm the
grep returns **nothing** — neither changes geometry. `rebuilds` should stay far
below `frame`; a `rebuilds` count that tracks `frame` means the gate is broken.

### Checking orientation and surface geometry

Two further tokens make the renderer's orientation chain auditable from a log.
Neither is per frame: one `FORGESHAPE_SURFACE_CONFIG` line is emitted per
swapchain creation, and one `FORGESHAPE_CAMERA_VIEWPORT` line per
`surfaceChanged`.

```
adb -s <serial> logcat -d -s ForgeShape:V | grep -E "SURFACE_CONFIG|CAMERA_VIEWPORT"
```

```
FORGESHAPE_SURFACE_CONFIG window=2400x1080 currentExtent=2400x1080 \
    currentTransform=0x2 supportedTransforms=0x1ff chosenExtent=2400x1080 preTransform=0x1
FORGESHAPE_CAMERA_VIEWPORT view=2400x1080 aspect=2.2222
```

`chosenExtent` must always equal the window size and `preTransform` must be `0x1`
(identity) — see the orientation convention in `ARCHITECTURE.md`. A
`currentTransform` of `0x2` or `0x8` simply means the display is rotated. Count
the lines as well as reading them: a handful per orientation change is healthy,
one per frame means the swapchain is being rebuilt in a loop.

## Interacting with the viewport

One finger drags to orbit, two fingers drag to pan, and pinching zooms. A short
single-finger tap on the object selects it (tinted orange); a tap on empty space
clears the selection. Dragging and multi-finger gestures never select.

In Sculpt Mode a one-finger gesture that goes down **on the mesh** is a brush
stroke with the active tool and owns the whole gesture; everything else is
unchanged. Such a Down is held *pending* rather than starting a stroke: it
becomes a stroke only on the first Move that travels 8 px while still
single-pointer, and is abandoned outright if a second finger arrives — which is
what makes a two-finger gesture whose first finger lands on the mesh
structurally unable to deform anything.

See `PRODUCT.md` for the full gesture contract.

## Editing the object

The Construction **Tool Rail** chooses what the **Property Inspector** edits:
*Shape* (Box, Cylinder, Sphere, Cone, Capsule or Plane, and that shape's dimensions) or
*Place* (position X/Y/Z, rotation X/Y/Z), each with its own Apply. Pick a display
unit (mm / cm / m) for lengths — rotation is always degrees — type the values,
and press the matching Apply. Nothing changes until then, including choosing a
primitive. Touches on any chrome surface never move the camera.

**Freeze to Sculpt** in the Global Toolbar copies the object's current
Construction mesh into a Frozen Sculpt Mesh and switches to Sculpt Mode, where
the rail carries **Grab**, **Clay**, **Smooth** and **Inflate**, and Radius and
Strength are edge sliders at the opposite side. Once a mesh exists that toolbar
button reads **Resume Sculpt**, which returns to it without re-freezing; only
*Freeze again…* in the Sculpt inspector discards prior deformation, and it
confirms first when there is deformation to lose. Radius and Strength are shared
by all four tools.

Every control has a stable semantic id in `res/values/ids.xml`. Drive UI-based
verification by resolving those ids from a live `uiautomator dump` and tapping
the resulting bounds — never by reusing coordinates from an older run, because
the workspace re-arranges itself per window. Note that `uiautomator dump` omits
views scrolled out of the Property Inspector's `ScrollView`: scroll the inspector
before looking for `apply_shape` or `apply_transform`.

The brush sliders are a custom control, so one `input tap` on the track sets the
value from its y position; the value native code actually kept is logged and is
the authority, not the tap position.

While the soft keyboard is up, a viewport tap must land clear of the inspector
and above the keyboard or it never reaches the `SurfaceView` — focus stays in the
text field and a following `keyevent` types a digit instead of doing what was
intended, which looks exactly like a command that did not run.

**Drive strokes on a dense primitive.** A brush that would capture no vertex
starts no stroke, so a stroke aimed at the centre of a face of a frozen **box**
(8 vertices, all at its corners) logs `STROKE_PENDING` →
`STROKE_ABANDONED:navigation` and never promotes. That is correct behaviour, not
a bug. Freeze a sphere (482 v) or a capsule (514 v) before driving a stroke for
evidence, or widen the radius.

## Log vocabulary

Construction applies log with a label (`ui` for the Property Inspector path):

```
FORGESHAPE_CONSTRUCTION_PRIMITIVE:<label> kind=<..> <params>       shape applied
FORGESHAPE_CONSTRUCTION_PUBLISHED:<rev>:<verts>:<indices> ...      new revision
FORGESHAPE_CONSTRUCTION_PRIMITIVE_UNCHANGED:<label> ...            already current
FORGESHAPE_CONSTRUCTION_PRIMITIVE_REJECTED:<label>:<why> ...       refused natively
FORGESHAPE_CONSTRUCTION_TRANSFORM[_UNCHANGED|_REJECTED:<why>] ...  placement
FORGESHAPE_MESH_UPLOAD_OK:<rev>:<verts>:<indices>:<reuse|grow> ...
```

Published counts alone say which primitive is current: box `8:36`, cylinder
`66:384`, sphere `482:2880`, cone `34:192`, capsule `514:3072` (or `482:2880`
when its total height equals its diameter). A transform apply carries
`rev=<current mesh revision>` precisely so it is visible that the revision did
**not** move: placement publishes no mesh revision and triggers no GPU upload.

Sculpt logs:

```
FORGESHAPE_SCULPT_FROZEN:<verts>:<indices> sculptRev=1 objectId=<..> from kind=<..>
FORGESHAPE_SCULPT_MODE:<construction|sculpt> meshRev=<..>
FORGESHAPE_SCULPT_MODE_REFUSED:nothing_frozen
FORGESHAPE_SCULPT_SOURCE_STALE:<label> sculptRev=<..>
FORGESHAPE_SCULPT_TOOL:<tool> requested=<n> known=<0|1>
FORGESHAPE_SCULPT_BRUSH:<tool> radiusPx=<..> strength=<..>
FORGESHAPE_SCULPT_STROKE_PENDING sculptRev=<..>
FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation sculptRev=<..>
FORGESHAPE_SCULPT_STROKE_BEGIN:<tool>:<capturedVertices> radiusLocal=<..>
FORGESHAPE_SCULPT_STROKE_MOVE:<tool> sculptRev=<..> meshRev=<..> local=(x,y,z)
FORGESHAPE_SCULPT_STROKE_END|_CANCEL sculptRev=<..>
```

A `PENDING` followed by `ABANDONED` with no `BEGIN` between them, with
`sculptRev` equal on both lines, is the whole proof that a gesture became
navigation without touching the mesh.

`STROKE_PENDING` appears only when the ray hits the current Frozen Sculpt Mesh,
so it doubles as the picking probe for the sculpt representation: a tap that
logs `PENDING` hit the sculpt geometry and one that logs `FORGESHAPE_PICK_MISS`
did not. Sweeping taps outward from the object's centre therefore measures the
pickable radius per direction, which is how "picking follows the deformation" is
checked at the pixel. `STROKE_MOVE` carries the displacement of the brush
**centre** — the highest-weight captured vertex — so summing `|local|` over a
stroke measures how much the tool did.

Camera, picking and selection log `FORGESHAPE_CAMERA_{ORBIT,PAN,ZOOM}_OK` and
`FORGESHAPE_CAMERA_STATE` on gesture end, `FORGESHAPE_PICK_HIT:<objectId>:<tri>`
or `FORGESHAPE_PICK_MISS` on a tap, and
`FORGESHAPE_SELECTION_CHANGED:<objectId>` / `FORGESHAPE_SELECTION_STATE:<id>`
(`0` means nothing selected). Moving the shape selector and switching display
units each produce **no** log line and no native call: both are presentation
only.

## Debug-only test drivers (DEBUG builds only)

These are test infrastructure, not product features; the native hook compiles to
a no-op in release.

| keyevent | effect |
| --- | --- |
| 8 / 9 / 10 | mesh fixture A (baseline cube) / B (same topology, deformed) / C (larger) |
| 11 | 60-revision repeated-update stress run |
| 12 | dump mesh / GPU / Construction / Sculpt diagnostics |
| 13 / 14 / 15 | box state A `2.0×1.0×0.5` / B `1.25×2.5×0.75` / C `3.333×0.42×1.125` m |
| 16 | deliberately invalid box update — must be rejected |
| 29 / 30 / 31 / 32 / 33 | Gate P1 heavy-mesh density fixture: publish a ~10k / 50k / 100k / 250k / 500k-vertex closed "spherified box" through the normal `MeshStore::publish` path |
| 34 | freeze the most recently published density tier directly into Sculpt (bypasses Construction) |

The box driver goes through the same native `applyPrimitive` entry point the
inspector uses, so it is a bounded driver rather than a parallel implementation. A
field keeps input focus after typing and swallows these number keys — tap the
viewport first to hand focus back.

```
FORGESHAPE_MESH_REVISION_PUBLISHED:<rev>:<verts>:<indices>
FORGESHAPE_MESH_STRESS_OK:<published>:<uploaded>:<coalesced> ...
FORGESHAPE_MESH_DIAG:<reason> ...                          the diagnostics dump
FORGESHAPE_CONSTRUCTION_STATE / _TRANSFORM_STATE / FORGESHAPE_SCULPT_STATE
FORGESHAPE_STRESS_MESH_TIER:<label> target=<n> v=<verts> i=<indices> genMs=<..> publishMs=<..> rev=<rev>
FORGESHAPE_STRESS_SCULPT_FROZEN:<label> freezeMs=<..> meshRev=<rev>
```

The dump reports both representations side by side. None of these is logged per
frame.

## Gate P1 tooling

**16 KB runtime target.** `scripts\start-forgeshape-emulator.ps1 -Avd ForgeShape_16K -Port <explicit>`
boots the x86_64 `google_apis_playstore_ps16k` AVD created for Gate P1 — same
isolation rules as `ForgeShape_Stage006` (explicit port, confirm identity by
`emu avd name`, never `5554`). Confirm the real page size before treating any
result as evidence:

```
adb -s <serial> shell getconf PAGE_SIZE      # must read 16384
```

**Vulkan validation layer.** No layer binary is ever bundled or committed —
see `CLAUDE.md`, "Android / Vulkan safety". To enable it ad hoc against a
running debug build:

```
adb -s <serial> push libVkLayer_khronos_validation.so /data/local/tmp/
adb -s <serial> shell run-as com.forgeshape.app cp /data/local/tmp/libVkLayer_khronos_validation.so .
adb -s <serial> shell settings put global enable_gpu_debug_layers 1
adb -s <serial> shell settings put global gpu_debug_app com.forgeshape.app
adb -s <serial> shell settings put global gpu_debug_layers VK_LAYER_KHRONOS_validation
adb -s <serial> shell settings put global gpu_debug_layer_app com.forgeshape.app
adb -s <serial> shell setprop debug.vulkan.khronos_validation.report_flags "error,warn,perf,info"
```

Relaunch the app, then `adb -s <serial> logcat -d | grep " VALIDATION:"` for the
layer's own messages (distinct from anything ForgeShape logs under the
`ForgeShape` tag). Delete the four `global` settings above when finished —
they are not scoped to a single run and persist otherwise.

## Injecting multi-touch for testing

`adb shell input swipe` is single-pointer only. Two-finger gestures can be
injected on an emulator by registering a virtual touchscreen with the on-device
`uinput` tool (`adb -s <serial> shell uinput - < gesture.json`), which delivers
real `MotionEvent`s through the normal input stack. The tool consumes a stream
of concatenated JSON objects — no enclosing array and no commas between them; an
array is rejected with `Error reading in object, ignoring.` Writing directly to
`/dev/input/event*` with `sendevent` requires root and does not work on Google
Play system images.

Capture screenshots through a POSIX shell (`>` in PowerShell re-encodes the
stream and corrupts the PNG):

```
adb -s <serial> exec-out screencap -p > artifacts\<name>.png
```
