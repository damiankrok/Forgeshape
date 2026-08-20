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

## Test

```
gradlew.bat :app:testDebugUnitTest         # JVM: layout rules, UI state, units
gradlew.bat :app:connectedDebugAndroidTest # on-device: Editor Workspace UI
```

The instrumented suite needs exactly one attached target and installs its own
APKs; when several are attached, detach or stop the others first, because the
Gradle task has no `-s <serial>` equivalent. It **uninstalls the app when it
finishes**, so reinstall before taking runtime evidence.

To run one class:

```
gradlew.bat :app:connectedDebugAndroidTest "-Pandroid.testInstrumentationRunnerArguments.class=com.forgeshape.app.EditorWorkspaceLayoutTest"
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

A clean debug launch emits **nine** `*_SELFTEST_OK` tokens, in this order, then
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
```

Each suite reports `(<n> checks)` and fails as `<SUITE>_CASE_FAIL:<name>` plus
`<SUITE>_FAIL`; the viewport fails as `FORGESHAPE_NATIVE_VIEWPORT_FAIL:<reason>`.
Grepping for `FAIL` alone over-matches, because some passing check *names*
contain "fails" — match `_SELFTEST_FAIL` or `_FAIL:`. The self-tests are
debug-only and run once from `NativeViewport.start()`; none runs per frame.

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
*Shape* (Box, Cylinder, Sphere, Cone or Capsule, and that shape's dimensions) or
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

The box driver goes through the same native `applyPrimitive` entry point the
inspector uses, so it is a bounded driver rather than a parallel implementation. A
field keeps input focus after typing and swallows these number keys — tap the
viewport first to hand focus back.

```
FORGESHAPE_MESH_REVISION_PUBLISHED:<rev>:<verts>:<indices>
FORGESHAPE_MESH_STRESS_OK:<published>:<uploaded>:<coalesced> ...
FORGESHAPE_MESH_DIAG:<reason> ...                          the diagnostics dump
FORGESHAPE_CONSTRUCTION_STATE / _TRANSFORM_STATE / FORGESHAPE_SCULPT_STATE
```

The dump reports both representations side by side. None of these is logged per
frame.

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
