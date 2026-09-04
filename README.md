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
`targetSdk` 36, and the ABI filter is `x86_64` **and** `arm64-v8a` — the emulator
target and physical devices (Gate P1). No other ABI is built or packaged, so the
debug APK carries `lib/x86_64/` and `lib/arm64-v8a/libforgeshape_native.so`.

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
Stage 016 touched the reserved `emulator-5554`. The one supported runner for
instrumented tests is:

```
scripts\run-instrumented-tests.ps1 -Serial <serial>
```

That no-filter command is the supported monolithic full-suite path. The
supported authoritative exhaustive-sharded full-suite path is:

```
scripts\run-instrumented-tests.ps1 -Serial <serial> -FullSharded -ShardCount 5
```

`-FullSharded` asks AndroidJUnitRunner to discover the complete live test-APK
inventory, verifies its sequential and advertised totals, assigns every
discovered class to exactly one deterministic class-atomic shard, and proves
zero missing, duplicate, or unexpected tests before execution. The configurable
partition greedily assigns the largest remaining class to the lightest shard
(stable name/index tie-breaks). Every shard must then return exactly one `OK`
result with its assigned count. The aggregate prints discovery, per-shard,
union, missing/duplicate, failure/abort, and PASS/FAIL fields and emits
`FULL_SHARDED_SUITE_PASS` only after a complete successful union.

It requires an explicit `-Serial`, refuses `emulator-5554` before contacting
any device, checks readiness with `adb -s <serial> get-state` (never a bare
`adb devices` enumeration), builds the debug app and test APKs, then installs
and instruments through `adb -s <serial>` only — every device operation is
scoped, mechanically, not by operator discipline. It uninstalls the test APK
when it finishes, matching `connectedDebugAndroidTest`'s own cleanup
behaviour, so reinstall the app before taking further runtime evidence.

To prove that ForgeShape work survives the process dying — both work the user
saved (E2ER1A-02/03) and work they never did (E2ER1B-01/02) — use:

```
scripts\run-project-persistence-e2e.ps1 -Serial <serial>
```

Instrumentation runs inside the app process, so a test cannot kill that process
and keep asserting. This script runs the "build and save" half of each case,
kills the app with `am force-stop`, confirms by PID that no ForgeShape process
remains, and then runs the "open and verify" half in a fresh one. It requires an
explicit `-Serial`, refuses `emulator-5554` before contacting any device, and
confirms the AVD's own name with `adb -s <serial> emu avd name` rather than
trusting the port. Every stage is driven through the runner above, so no device
call here is unscoped. It prints `PROJECT_PERSISTENCE_E2E=PASS` only when all
**eight** stages passed and all four process deaths were observed.

Stages 5-8 are the harder claim: nothing was saved. Autosave alone protected the
work, and the recovery question on a genuinely cold launch is what offers it
back — no test seam, no cleared flag, exactly what a user meets after a crash.

The permanent `.forge` golden corpus in `testdata/forge/v1/` is written and
verified with:

```
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1 -VerifyOnly
```

That script is a second, independent implementation of the `.forge` v1 encoder,
written from `DATA_PACKAGE_SPEC.md`. It needs no device and no Android tooling.
Its digests must match the ones the native `FSR1A-12`, `IMP01A-19`,
`IMP01B-11`/`IMP01B-12`, `CADR0-33`/`34`/`36` and `CADA3-46..51` cases assert
and the ones a debug launch prints as `FORGESHAPE_PROJECT_GOLDEN_SHA256`,
`..._IMPORTED`, `..._IMPORTED_SCULPT`, `..._CAD` and `..._CAD_V2`; a mismatch
means the encoder and the specification have parted company. The six `CADB` v2
fixtures include the lineage token, which the script computes from the rule
`DATA_PACKAGE_SPEC.md` §7c states rather than from the C++ — that parity is
what found the mistyped FNV basis in `CAD-A3-C1`. The six `CADB` v3 curve
fixtures are pinned the same way, by `CADUXR1-38`. Seven of the twenty-eight
fixtures are packaged into the test APK's assets as well, so
`ImportedMeshDurableTest` can prove the independent encoder's bytes actually LOAD
on a device rather than only hashing the same.

To run the GLB export suite and pull the two exported sample models off the
device, use:

```
scripts\run-glb-export-evidence.ps1 -Serial <serial>
```

It runs `GlbExportTest` through the runner above and then pulls
`construction_sentinel.glb` and `sculpt_sentinel.glb` into `artifacts\e2er1c\`,
re-checking the GLB header on the host copy and printing each file's SHA-256. It
deletes any sentinel from a previous run first, so a pulled file is always from
the run that just passed, and it produces nothing at all from a failing one. It
prints `GLB_EXPORT_EVIDENCE=PASS` only when both files are present and valid.

Correctness itself is asserted on the device by `GlbExportTest`, which reads
every export back through `GlbDocument` — a test-only glTF 2.0 reader written
from the specification that shares no code with the exporter. What this script
adds is the artefact, so the files can be opened in another program without
reproducing a test run.

To run the GLB import/roundtrip diagnostic and pull its reports off the device,
use:

```
scripts\run-glb-import-evidence.ps1 -Serial <serial>
```

It runs `GlbImportPreviewTest` through the runner above and pulls the
world-geometry comparison reports and the Source/Imported screenshot pair into
`artifacts\glb-import-r0\`, printing each report's verdict. It deletes anything
from a previous run first and produces nothing at all from a failing one. It
prints `GLB_IMPORT_EVIDENCE=PASS` only when every file is present and carries a
verdict.

The comparison exports the live scene through the real writer, reads the bytes
back with `forgeshape_gltf_import` — a parser that shares no line with the
exporter — and compares both against DOMAIN truth re-derived from the primitive
generator or the Frozen Sculpt Mesh. It reports the maximum world-space position
delta, which body and vertex produced it, and the maximum normal disagreement in
degrees. **The imported mesh preview is a diagnostic and is session-only**: what
it draws is never a body, never saved and never autosaved, and since
`IMPORT-01A` it has no user-facing control at all — the suites reach it through
JNI. The product's *Import GLB…* is durable import, which creates real objects;
`ImportedMeshDurableTest` covers it, `ImportedMeshSculptTest` covers sculpting
one (`IMPORT-01B`), and `ObjectsDeleteTest` covers removing one
(`UI-OWNER-45`). `SketchExtrudeTest` covers the sketch-to-CAD-Body flow
(`CAD-R0-A1A2`, `E2E-CADR0-01..16`): New Sketch, a plane, real drags and taps
on the viewport, Finish Sketch, a typed depth, Extrude, Undo/Redo, later edits,
save/reopen, the gizmo on a CAD Body, and the sculpt workflow beside it.
`SpatialSketchTest` covers the CAD-A3 spatial support flow (`E2E-CADA3`): New
Sketch landing directly in the viewport-first support pick, the by-name plane
fallback, every world plane by real taps, a synthesized stylus hover that
highlights without committing, a tap-tap on a planar cap and on a side face, the
cylindrical-side refusal, a face-supported dependent that follows a producer
edit, the producer-delete refusal, the dependency surviving a save/reopen, and
the adaptive grid moving with a real pinch while a typed value stays exact.
`HomeFlowTest` covers `APP-H1` (`E2E-APPH1-01..12`): cold-launch Home with no
project behind it, New Project → CAD through a spatial plane to the first
durable body, New Project → Sculpt, Open File from Home (a saved dependency
project, cancel, a corrupt file), the unsaved-changes guard's Cancel, Discard
and Save paths, deterministic Back in every phase, and recreation — asserting on
every journey that native never read an active body while no project was open.
`CadA3VisualEvidenceTest` captures the ten-frame screenshot journey;
`scripts\collect-cad-a3-evidence.ps1 -Serial <serial>` runs it, pulls the
frames and builds `artifacts\cad-a3-app-h1\OWNER_CONTACT_SHEET.png` and
`VISUAL_EVIDENCE.md`.

For the widened external-GLB subset (`GLB-IMPORT-R1`) use:

```
scripts\run-glb-import-r1-evidence.ps1 -Serial <serial>
```

It runs `GlbImportExternalR1Test` through the same runner and pulls the
Nomad-like compatibility fixture's fingerprint and a screenshot of it into
`artifacts\glb-import-r1\`, printing the fixture's SHA-256. The fixture is a
**synthetic** file built by `forgeshape_glb_import_fixture.cpp` that reproduces
the structural feature set of an external low-poly export — a node matrix, seven
TRIANGLES primitives over one shared POSITION accessor, no NORMAL, ignored
colour and UV attributes, a double-sided material. It is not, and is never
described as, any owner asset. Every coordinate in it is an integer over a power
of two, so the same build produces the same bytes anywhere and the hash means
something. It prints `GLB_IMPORT_R1_EVIDENCE=PASS` only when every file is
present and the fingerprint carries a digest.

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

`-TestClass` is focused/subset evidence only and can never emit the
full-sharded PASS marker. If infrastructure crashes or aborts during
`-FullSharded`, recover the isolated AVD and restart the entire command from
shard 1. Individual class or shard reruns are supplementary only; they never
repair or complete a failed aggregate. The runner-level `THR1-01..10` checks
can be run without a device via `scripts\test-instrumented-sharding.ps1`.

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
adb -s <serial> logcat -G 64M          # do this FIRST; 16M now drops suites
adb -s <serial> logcat -s ForgeShape:V
```

Enlarge the ring buffer before capturing startup evidence: the self-test suites
emit several hundred lines in a few milliseconds and the default buffer silently
drops the tail, which reads exactly like a self-test that stopped partway
through. That is a logging limit, not an app failure.

A clean debug launch emits **twenty** `*_SELFTEST_OK` tokens, in this order, then
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
FORGESHAPE_SCENE_SELFTEST_OK
FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK
FORGESHAPE_GIZMO_SELFTEST_OK
FORGESHAPE_PROJECT_SELFTEST_OK
FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK
FORGESHAPE_GLTF_EXPORT_SELFTEST_OK
FORGESHAPE_GLTF_IMPORT_SELFTEST_OK
FORGESHAPE_CAD_SELFTEST_OK
FORGESHAPE_CAD_A3_SELFTEST_OK
FORGESHAPE_SKETCH_UX_SELFTEST_OK
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

While the Construction **Transform** tool is held, the active body wears Move,
Rotate or Scale handles, in World or Local axes. A finger or stylus that goes
down **on a handle** owns the whole gesture and transforms the body instead of
moving the camera; a gesture that starts anywhere else orbits, pans, zooms and
selects exactly as above. A second finger during a handle drag cancels it and
puts all nine placement values back.

In Sculpt Mode a one-finger gesture that goes down **on the mesh** is a brush
stroke with the active tool and owns the whole gesture; everything else is
unchanged. Such a Down is held *pending* rather than starting a stroke: it
becomes a stroke only on the first Move that travels 8 px while still
single-pointer, and is abandoned outright if a second finger arrives — which is
what makes a two-finger gesture whose first finger lands on the mesh
structurally unable to deform anything.

See `PRODUCT.md` for the full gesture contract.

## Editing the object

Add a body from the **Objects capsule** at the bottom leading edge: its `+` opens
**Add Primitive**, and the shape you pick there is what the new body is. Tapping
the body name beside it opens the scene list.

Holding **Transform** in the Tool Rail also draws the handles, with two controls
beside the rail: one choosing **Move handles**, **Rotate handles** or **Scale
handles** (ids `transform_mode_move` / `_rotate` / `_scale`), and one choosing
**World axes** or **Local axes** (`transform_space_world` / `_local`). The space
control is absent while Scale is held, because a scale is always about the
body's own axes. Move offers three axis shafts and three plane squares, Rotate
three rings, and Scale three axis cubes, three plane squares and a centre cube
for uniform scaling. Dragging one is the direct way to place a body; the exact
values below are the precise way, and both write the same placement.

The Construction **Tool Rail** chooses what the **precision surface** edits, and
the small control attached under the rail is what opens it: *Shape* (Box,
Cylinder, Sphere, Cone, Capsule or Plane, and that shape's dimensions) or
*Transform* (position X/Y/Z, rotation X/Y/Z, scale X/Y/Z), each with its own
Apply. Pick a display unit (mm / cm / m) for lengths — rotation is always
degrees and scale is always a bare multiplier — type the values, and press the
matching Apply. Nothing changes until then, including
choosing a primitive. Touches on any chrome surface never move the camera.

**Nothing is on screen at rest but the model and the edge controls.** The exact
values are absent until the precision toggle is pressed, so a verification script
must open that surface before looking for a field in it.

**Start Sculpting** in the Global Toolbar (id `freeze_to_sculpt` — the ids are
the implementation's and did not change with the wording) copies the object's
current Construction mesh into a Frozen Sculpt Mesh and switches to Sculpt Mode,
where the rail carries **Grab**, **Clay**, **Smooth** and **Inflate**, and Radius
and Strength are edge sliders at the opposite side. Once a mesh exists that
toolbar button reads **Resume Sculpt**, which returns to it without re-freezing;
only **Reset Sculpt from Shape…** (id `freeze_again`) discards prior deformation,
and it confirms first when there is deformation to lose. Radius and Strength are
shared by all four tools.

**Neither `+` exists in Sculpt Mode.** `sceneAddBody()` refuses while sculpting,
so `objects_capsule_add` and `add_body` are `GONE` there and will not appear in a
`uiautomator dump`. Leave Sculpt before driving a creation step.

Every control has a stable semantic id in `res/values/ids.xml`. Drive UI-based
verification by resolving those ids from a live `uiautomator dump` and tapping
the resulting bounds — never by reusing coordinates from an older run, because
the workspace re-arranges itself per window. Note that `uiautomator dump` omits
views scrolled out of the precision surface's `ScrollView`, and omits every
surface that is closed: tap `precision_toggle` first, then scroll, before looking
for `apply_shape` or `apply_transform`. Likewise `objects_capsule_active` opens
the scene list and `objects_capsule_add` opens Add Primitive — the rows and the
shape tiles do not exist in a dump until they do.

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
a bug. Start Sculpting on a sphere (482 v) or a capsule (514 v) before driving a
stroke for evidence, or widen the radius.

**The status capsule is transient, and it reports only events.** Since UI-R4B it
clears itself — 5 s for an acknowledgement, 10 s for a rejection — and is `GONE`
when it has nothing to say, so a `uiautomator dump` taken a few seconds after an
action will not contain `status_message`. Read it immediately after the step that
wrote it, or read the `FORGESHAPE_*` log line, which is the authority either way.
Since UI-R4C it carries no instruction at all: answering the start question and
switching between `tool_rail_shape` and `tool_rail_place` write **nothing**, so a
script must not wait for a message after either.

**`back_to_construction` may read `← Construction` on a very narrow row.** The
toolbar sizes the mode transition against the row it is in, and only where the
full wording genuinely does not fit does it take the short form. The id and the
content description (*Back to Construction*) are unchanged in both, so locate it
by id — never by its text.

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

Gizmo logs:

```
FORGESHAPE_SESSION_INIT_BEGIN                                      seeding a session
FORGESHAPE_SESSION_INIT_END undo=<n> redo=<n>                      both must be 0
FORGESHAPE_GIZMO_ACTIVE:<0|1>                                      handles offered
FORGESHAPE_GIZMO_REFUSED:in_sculpt_mode
FORGESHAPE_GIZMO_MODE:<move|rotate|scale> accepted=<0|1>
FORGESHAPE_GIZMO_MODE_REJECTED:<index>
FORGESHAPE_GIZMO_SPACE:<world|local> accepted=<0|1>
FORGESHAPE_GIZMO_SPACE_REJECTED:<index>
FORGESHAPE_GIZMO_UPLOAD_OK vertices=<n> bytes=<..> move=[a,b) rotate=[c,d) scale=[e,f)
FORGESHAPE_GIZMO_DRAG_BEGIN:<move|rotate|scale> axis=<handle> objectId=<..>
FORGESHAPE_GIZMO_DRAG_COMMIT:<recorded|no_change> updates=<n> solve=<..> undo=<n>
FORGESHAPE_GIZMO_DRAG_CANCEL updates=<n> undo=<n>
FORGESHAPE_GIZMO_HANDLES:<mode>/<space> pivot=<px>,<py> ...        keyevent 35 only
FORGESHAPE_GIZMO_HANDLES:absent
```

`axis=` carries the **handle**, not only an axis: `x`, `y`, `z` for the three
axis handles, `xy`, `xz`, `yz` for the plane handles, and `uniform` for Scale's
centre cube. The mode decides which set exists — Move offers axes and planes,
Rotate the three rings, Scale axes, planes and uniform — and Scale refuses
`world`, because a world-axis scale of a turned body is a shear.

A `DRAG_COMMIT` with `updates=` in the hundreds and `undo=` one higher than
before it is the whole proof that a long drag is one history step; `no_change`
with `undo=` unmoved is the same proof for a tap. `solve=` names which path the
solver took — `resolved`, `plane_fallback` or `unresolvable` — so a degenerate
viewpoint is visible rather than inferred. `GIZMO_UPLOAD_OK` appears **once per
device** and names all three vertex ranges: a second occurrence in one session is
direct evidence that something re-uploaded geometry a drag must never touch.

`SESSION_INIT_END` reporting anything but `undo=0 redo=0` means a session seed
leaked into the user's history.

Project persistence logs:

```
FORGESHAPE_RENDER_DEVICE_LOSS_INJECTED                              DEBUG seam only
FORGESHAPE_RENDER_DEVICE_LOST:<where> attempt=<n>
FORGESHAPE_RENDER_DEVICE_REBUILT:attempt=<n> completed=<n>
FORGESHAPE_RENDER_RESTART_REQUIRED:<where>[:<kind>]
FORGESHAPE_PROJECT_ENCODED:<bytes> kind=<Construction|Sculpt> bodies=<n>
FORGESHAPE_PROJECT_ENCODE_FAIL:<why>
FORGESHAPE_PROJECT_LOADED:<n> bodies sculptMeshes=<n> active=<id> kind=<..> meshRev=<..> undo=0 redo=0
FORGESHAPE_PROJECT_LOAD_REJECTED:<why> bytes=<n>
FORGESHAPE_PROJECT_LOAD_FAIL:<no_data|empty>
FORGESHAPE_PROJECT_GOLDEN_SHA256 construction=<sha> sculpt=<sha>
FORGESHAPE_PROJECT_GOLDEN_SHA256_IMPORTED imported_only=<sha> construction_imported=<sha> mixed_imported=<sha>
FORGESHAPE_GLB_EXPORTED:<bytes> kind=<Construction|Sculpt> bodies=<n>
FORGESHAPE_GLB_EXPORT_FAIL:<why>
FORGESHAPE_IMPORTED:<bytes> objects=<n> vertices=<n> triangles=<n> batches=<n> firstObjectId=<id> activeObjectId=<id>
FORGESHAPE_IMPORT_FAIL:<why> bytes=<n>
FORGESHAPE_GLB_IMPORTED:<bytes> meshes=<n> vertices=<n> triangles=<n>    diagnostic preview
FORGESHAPE_GLB_IMPORT_FAIL:<why> bytes=<n>    diagnostic preview
FORGESHAPE_GLB_PREVIEW_VISIBLE:<0|1>
FORGESHAPE_GLB_PREVIEW_CLEARED
```

A `DEVICE_LOST` followed by a `DEVICE_REBUILT` and a second
`FORGESHAPE_NATIVE_VIEWPORT_OK` is the whole proof that the viewport came back
from a lost GPU device; a `RESTART_REQUIRED` instead is the fail-closed branch,
and the project is checkpointed either way. The first-present token appearing
more than once in a session is therefore expected after a rebuild and is not a
duplicate startup.

Autosave, recovery and transfer do **not** log to logcat. They record into the
bounded local diagnostic ring instead, which is what `Share Diagnostics…`
writes out — see the sample in `artifacts/e2er1b/`. That is deliberate: those
events are the user's to read and share, not a stream for a shell to tail.

`<why>` is the codec's own vocabulary — `ChecksumMismatch`, `Truncated`,
`UnsupportedMajor`, `InvalidSemanticValue` and the rest, listed in
`DATA_PACKAGE_SPEC.md`. A `LOAD_REJECTED` line is the whole proof that a refused
Open changed nothing: it is emitted before any live state is touched, and no
`FORGESHAPE_CONSTRUCTION_PUBLISHED` follows it. `LOADED` always reports
`undo=0 redo=0`, because a loaded document starts a fresh session history.

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
| 35 | log where the gizmo's pivot and every handle the current mode offers are on screen right now, as `FORGESHAPE_GIZMO_HANDLES:<mode>/<space> pivot=…`. Reports only — it grabs nothing and mutates nothing. It exists because a handle's pixel is a live function of the camera, the window and the body's placement, so a walkthrough that wrote one down would be recording something true for exactly one run |

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
