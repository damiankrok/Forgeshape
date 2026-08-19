# ForgeShape

Author-owned, offline-first 3D modeling/sculpting application for Android.
Android shell + ForgeShape-owned viewport + native C++ Vulkan renderer. No game engine.

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

`local.properties` must point at the Android SDK (`sdk.dir`).

Shaders are compiled ahead of time to SPIR-V by the `glslc` that ships with the NDK
(`$ANDROID_NDK/shader-tools/<host>/glslc`). No shader compiler is bundled at runtime.

## Build

```
gradlew.bat :app:assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

## Install and run

When more than one emulator/device is attached, always target one explicitly with
`adb -s <serial>`; do not rely on the default target.

```
adb -s <serial> install -r app\build\outputs\apk\debug\app-debug.apk
adb -s <serial> shell am start -n com.forgeshape.app/.ForgeShapeActivity
```

## Interacting with the viewport

One finger drags to orbit, two fingers drag to pan, and pinching zooms. A short
single-finger tap on the box selects it (the box is tinted orange); a tap on
empty space clears the selection. Dragging and multi-finger gestures never
select. In Sculpt Mode a one-finger gesture that starts **on the mesh** is a
brush stroke with the active tool instead; everything else is unchanged. See `PRODUCT.md` for the full
gesture contract and `ARCHITECTURE.md` for who owns what.

## Editing the object

The panel across the top of the viewport edits the object's **shape** (Box,
Cylinder or Sphere, and that shape's dimensions) and its **placement** (position
X/Y/Z and rotation X/Y/Z), with a separate Apply for each. Pick a display unit
(mm / cm / m) for the lengths — rotation is always degrees — type the values, and
press the matching Apply; nothing changes until then, including moving the shape
selector. Touches on the panel never move the camera; touches anywhere else
navigate as usual.

Driving it from `adb` (view is 1080x2400 at density 420):

```
adb -s <serial> shell input tap 174 64                   # shape: Box=174, Cylinder=326, Sphere=544
adb -s <serial> shell input tap 238 316                  # unit: mm=78 cm=238 m=383
adb -s <serial> shell input tap 197 193                  # focus Width  (Height 539, Depth 881)
adb -s <serial> shell input tap 287 193                  # focus Diameter (cylinder Height 800)
adb -s <serial> shell input tap 540 193                  # focus Diameter (sphere; full-width field)
adb -s <serial> shell input tap 197 530                  # focus Pos X  (Pos Y  539, Pos Z  881)
adb -s <serial> shell input tap 197 720                  # focus Rot X  (Rot Y  539, Rot Z  881)
adb -s <serial> shell input keyevent 123                 # move to end of field
adb -s <serial> shell input keyevent 67                  # backspace (repeat to clear)
adb -s <serial> shell input text "333,3"                 # '.' or ',' both work; '-' allowed
adb -s <serial> shell input tap 896 315                  # Apply Shape
adb -s <serial> shell input tap 862 842                  # Apply Transform
adb -s <serial> shell input tap 214 989                  # Freeze to Sculpt
adb -s <serial> shell input tap 584 989                  # Resume Sculpt (only once frozen)
adb -s <serial> shell input tap 200 1400                 # viewport: return focus
```

## Sculpting the object

**Freeze to Sculpt** copies the object's current Construction mesh into a Frozen
Sculpt Mesh and switches to Sculpt Mode, which replaces the Construction panel
with a compact strip: a four-tool selector (**Grab**, **Clay**, **Smooth**,
**Inflate**), a Radius slider, a Strength slider and **Back to Construction**.
**Resume Sculpt** returns to the frozen mesh without re-freezing, so prior
deformation is kept; only Freeze discards it.

In Sculpt Mode, one finger going down **on the mesh** is a brush stroke with the
active tool and owns the whole gesture; one finger going down anywhere else
orbits, and two fingers pan and zoom, exactly as in Construction Mode.

A one-finger Down on the mesh is held **pending** rather than starting a stroke
immediately: it becomes a stroke only on the first Move that travels 8 px while
still single-pointer, and is abandoned outright if a second finger arrives. That
is what makes a two-finger gesture whose first finger lands on the mesh
structurally unable to deform anything.

Radius and Strength are shared by all four tools; there is no per-tool setting.

Driving it from `adb` (Sculpt panel, view 1080x2400 at density 420):

```
adb -s <serial> shell input tap 836 84                   # Back to Construction
adb -s <serial> shell input tap 158 226                  # tool: Grab
adb -s <serial> shell input tap 413 226                  # tool: Clay
adb -s <serial> shell input tap 666 226                  # tool: Smooth
adb -s <serial> shell input tap 920 226                  # tool: Inflate
adb -s <serial> shell input tap <x> 372                  # Radius slider   (x 82..1002)
adb -s <serial> shell input tap <x> 473                  # Strength slider (x 82..1002)
adb -s <serial> shell input swipe 540 1194 880 1194 900  # a stroke on the mesh
```

The Sculpt panel gained a tool row in Stage 013, so its bottom edge is now at
about **y 610** (it was 470) and the sliders moved from y 229 / 330 to
**y 372 / 473**. Stage 012 scripts need the new coordinates.

**Stroke the mesh on a dense primitive.** The swipe above logs
`STROKE_PENDING` → `STROKE_ABANDONED:navigation` on a frozen **box**, and that
is correct behaviour, not a bug: a box has 8 vertices, all at its corners, so a
120 px brush at the centre of a face captures none, and a brush that would
capture no vertex starts no stroke. Freeze a sphere (482 v) or a capsule
(514 v) before driving a stroke for evidence, or widen the radius. Verified
under both NDK r27 and r29, so it is not toolchain-dependent.

Tapping a `SeekBar` sets it, so a slider is driven with a single `input tap`; the
value native code actually kept is logged and is the authority, as is the tool:

```
FORGESHAPE_SCULPT_BRUSH:<tool> radiusPx=<..> strength=<..>
FORGESHAPE_SCULPT_TOOL:<tool> requested=<n> known=<0|1>
```

A gesture logs its arbitration, then one begin, one line per move that changed
geometry, and one end:

```
FORGESHAPE_SCULPT_STROKE_PENDING sculptRev=<..>
FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation sculptRev=<..>
FORGESHAPE_SCULPT_STROKE_BEGIN:<tool>:<capturedVertices> radiusLocal=<..>
FORGESHAPE_SCULPT_STROKE_MOVE:<tool> sculptRev=<..> meshRev=<..> local=(x,y,z)
FORGESHAPE_SCULPT_STROKE_END sculptRev=<..>
FORGESHAPE_SCULPT_STROKE_CANCEL sculptRev=<..>
```

A `PENDING` followed by `ABANDONED` with no `BEGIN` between them is the whole
proof that a gesture became navigation without touching the mesh; compare
`sculptRev` on the two lines.

`SCULPT_STROKE_PENDING` appears only when the ray hits the current Frozen Sculpt
Mesh, so it doubles as the picking probe for the sculpt representation: a tap
that logs `PENDING` hit the sculpt geometry, and one that logs
`FORGESHAPE_PICK_MISS` did not. Sweeping a line of taps outward from the object's
centre therefore measures the pickable radius in each direction, which is how
"picking follows the deformation" is checked at the pixel. A tap on the mesh is a
pending stroke that is abandoned on Up, so it costs no revision.

`SCULPT_STROKE_MOVE` carries the displacement of the brush **centre** — the
highest-weight captured vertex — so summing `|local|` over a stroke is a direct
measure of how much the tool did, and is how the Radius and Strength effects are
quantified.

Mode changes and the Freeze itself log:

```
FORGESHAPE_SCULPT_FROZEN:<verts>:<indices> sculptRev=1 objectId=<..> from kind=<..> <params>
FORGESHAPE_SCULPT_MODE:<construction|sculpt> meshRev=<..>
FORGESHAPE_SCULPT_MODE_REFUSED:nothing_frozen
FORGESHAPE_SCULPT_SOURCE_STALE:<label> sculptRev=<..>
```

`SCULPT_SOURCE_STALE` is the stale-source policy in action: a Construction change
while a frozen mesh exists marks it and changes nothing else. The diagnostics
dump reports both representations side by side, as `FORGESHAPE_SCULPT_STATE`
next to `FORGESHAPE_CONSTRUCTION_STATE`.

The box, cylinder and sphere rows occupy the same place and swap with the
selector, so those field coordinates are for whichever row is currently visible.
Only one of the three is ever on screen. The Construction panel gained a mode row
in Stage 012, so its bottom edge is now at **y 1132** (it was 985), and the safe
band for a viewport tap while the keyboard is up is roughly **y 1170-1500**. The
Sculpt panel is much shorter — its bottom edge is at about y 470 — so the whole
viewport below that is available while sculpting.

A field keeps input focus after typing, which swallows the debug number keys
below. Tap the viewport to hand focus back before sending them — but stay in the
band **below the panel and above the soft keyboard**. A tap inside the keyboard
area never reaches the SurfaceView, focus stays in the field, and the following
`keyevent 12` types a digit instead of dumping diagnostics.

An Apply from the UI logs with the label `ui`:

```
FORGESHAPE_CONSTRUCTION_PRIMITIVE:ui kind=<box|cylinder|sphere> ... shape applied
FORGESHAPE_CONSTRUCTION_PUBLISHED:<rev>:<verts>:<indices> ... reason=ui
FORGESHAPE_CONSTRUCTION_PRIMITIVE_UNCHANGED:ui ...               already current
FORGESHAPE_CONSTRUCTION_PRIMITIVE_REJECTED:ui:<why> ...          refused natively

FORGESHAPE_CONSTRUCTION_TRANSFORM:ui pos=(..)m rot=(..)deg ...   placement applied
FORGESHAPE_CONSTRUCTION_TRANSFORM_UNCHANGED:ui ...               already current
FORGESHAPE_CONSTRUCTION_TRANSFORM_REJECTED:ui:<why> ...          refused natively
```

A box publishes `8:36`, a cylinder `66:384` (2N+2 vertices and 12N indices at
N = 32 fixed radial segments) and a sphere `482:2880` (15 rings of 32 plus two
poles, and 2N(S-1) triangles at N = 32 meridians and S = 16 stacks), so the
published counts alone say which primitive is current.

A transform apply carries `rev=<current mesh revision>` precisely so it is
visible that the revision did **not** move: placement publishes no mesh revision
and triggers no GPU upload. The diagnostics dump reports both truths side by
side, as `FORGESHAPE_CONSTRUCTION_STATE` and
`FORGESHAPE_CONSTRUCTION_TRANSFORM_STATE`.

Moving the shape selector produces **no** log line and no native call: like a
unit change, it is presentation only.

Input the UI can already name a problem with (blank, non-numeric, and
non-positive for a dimension) is reported in the panel and never reaches native
code, so it produces no log line at all.

Switching display units produces **no** log line and no native call: it is
presentation only, and it never touches the rotation fields.

## Verify the viewport

```
adb -s <serial> logcat -G 16M          # do this FIRST; see the note below
adb -s <serial> logcat -s ForgeShape:V
```

Enlarge the ring buffer before capturing startup evidence. The eight self-test
suites emit roughly 830 lines within a few milliseconds, and the default buffer
silently drops the tail — which reads exactly like a self-test that stopped
partway through. It is a logging limit, not an app failure.

A successful run logs `FORGESHAPE_NATIVE_VIEWPORT_OK` after the first frame is
presented. Failures are logged as `FORGESHAPE_NATIVE_VIEWPORT_FAIL:<reason>`.

The debug build also runs the camera controller self-checks once at startup and
logs `FORGESHAPE_CAMERA_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_CAMERA_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_CAMERA_SELFTEST_FAIL` if any check fails. First use of each gesture
logs `FORGESHAPE_CAMERA_ORBIT_OK`, `FORGESHAPE_CAMERA_PAN_OK` and
`FORGESHAPE_CAMERA_ZOOM_OK`; the camera pose is logged as
`FORGESHAPE_CAMERA_STATE ...` when a gesture ends.

The picking and selection self-checks also run once at startup and log
`FORGESHAPE_PICKING_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_PICKING_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_PICKING_SELFTEST_FAIL`. The runtime mesh checks log
`FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_DYNAMIC_MESH_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_DYNAMIC_MESH_SELFTEST_FAIL`. The Construction-box checks log
`FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_FAIL`. The Construction-transform checks
log `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_FAIL`. The active-object and cylinder
checks log `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_FAIL`. The typed-primitive-boundary
and sphere checks log `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK (<n> checks)`,
or `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_FAIL`. The cone and capsule checks log
`FORGESHAPE_CONE_CAPSULE_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_CONE_CAPSULE_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_CONE_CAPSULE_SELFTEST_FAIL`. The Freeze-to-Sculpt, adjacency,
normal, brush-kernel and four-tool checks log
`FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK (<n> checks)`, or
`FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_CASE_FAIL:<name>` plus
`FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_FAIL`. That suite replaces the Stage 012
`FORGESHAPE_SCULPT_GRAB_SELFTEST_*` tokens and still contains the Grab checks. No
self-test suite ever runs per frame.

The startup geometry is the Construction object, logged as
`FORGESHAPE_CONSTRUCTION_PUBLISHED:<rev>:<vertices>:<indices> kind=<..> <params>
objectId=<..> reason=<..>`. Those parameter values are the authoritative
double-meter numbers, not measurements taken from the mesh.

Resolving a tap logs `FORGESHAPE_PICK_HIT:<objectId>:<triangleIndex> ...` or
`FORGESHAPE_PICK_MISS`, and a change of selected object logs
`FORGESHAPE_SELECTION_CHANGED:<objectId>` (`0` means nothing selected). The
selected id is also reported as `FORGESHAPE_SELECTION_STATE:<objectId>` whenever
a gesture ends, which is how selection persistence is checked without picking.

### Driving the runtime mesh update path (DEBUG builds only)

The debug build publishes mesh revisions in response to the number keys, so the
dynamic mesh path can be exercised without any product UI and without
rebuilding or reinstalling the APK. This is test infrastructure, not a feature;
the native hook compiles to a no-op in a release build.

```
adb -s <serial> shell input keyevent 8    # Fixture A - baseline cube
adb -s <serial> shell input keyevent 9    # Fixture B - same topology, deformed
adb -s <serial> shell input keyevent 10   # Fixture C - larger replacement
adb -s <serial> shell input keyevent 11   # 60-revision repeated-update stress run
adb -s <serial> shell input keyevent 12   # dump mesh / GPU / Construction diagnostics
```

### Driving the Construction box dimensions (DEBUG builds only)

Keys 6-9 drive the object's **authoritative** box width/height/depth in meters
through the same native entry point the Construction panel uses. This is a
bounded test driver with four fixed states, not the product edit path; the native
hook compiles to a no-op in a release build.

```
adb -s <serial> shell input keyevent 13   # state A - 2.0   x 1.0  x 0.5   m (default)
adb -s <serial> shell input keyevent 14   # state B - 1.25  x 2.5  x 0.75  m
adb -s <serial> shell input keyevent 15   # state C - 3.333 x 0.42 x 1.125 m
adb -s <serial> shell input keyevent 16   # invalid update - must be rejected
```

An applied change logs `FORGESHAPE_CONSTRUCTION_PRIMITIVE:<label> ...` followed
by a `FORGESHAPE_CONSTRUCTION_PUBLISHED` line. Re-applying the same state logs
`FORGESHAPE_CONSTRUCTION_PRIMITIVE_UNCHANGED:<label> ...` and publishes nothing.
An invalid update logs
`FORGESHAPE_CONSTRUCTION_PRIMITIVE_REJECTED:<label>:<reason> ...` with the
retained parameters and the unchanged revision. The diagnostics dump also reports
`FORGESHAPE_CONSTRUCTION_STATE:<reason> ...`.

Publishing logs `FORGESHAPE_MESH_REVISION_PUBLISHED:<rev>:<vertices>:<indices>`,
the render thread logs
`FORGESHAPE_MESH_UPLOAD_OK:<rev>:<vertices>:<indices>:<reuse|grow>` with the
current capacities, the stress run finishes with
`FORGESHAPE_MESH_STRESS_OK:<published>:<uploaded>:<coalesced> ...`, and the
diagnostics dump is `FORGESHAPE_MESH_DIAG:<reason> ...`. None of these are
logged per frame.

### Injecting multi-touch for testing

`adb shell input swipe` is single-pointer only. Two-finger gestures can be
injected on an emulator by registering a virtual touchscreen with the on-device
`uinput` tool (`adb -s <serial> shell uinput - < gesture.json`), which delivers
real `MotionEvent`s through the normal input stack. Writing directly to
`/dev/input/event*` with `sendevent` requires root and does not work on Google
Play system images.

## ABI

The debug build currently targets `x86_64` only (the available emulator target).
