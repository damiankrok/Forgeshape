# ForgeShape — Project Status

**Status Version:** 0.30.0
**Updated:** 2026-08-25
**Result:** PARTIAL — INPUT-R1 carries stylus pointer semantics across the
whole boundary and is green on build, native self-test authoring and the JVM
suite; the on-device instrumented run is UNVERIFIED because the ForgeShape-owned
emulator would not boot
**Current Phase:** Phase 1 — Native Viewport
**Workspace:** `D:\TRAVELAPPS\ForgeShape`
**Accepted implementation baseline:** UI-R1C2 — world grid + adaptive workspace,
with INPUT-R1 (pointer semantics) implemented on top but NOT yet accepted, on
top of UI-R1C1 (motion + selection feedback), UI-R1B2 (theme system + Light),
UI-R1B1 (visual foundation + start flow), Stage 017 (multi-object scene +
hierarchy foundation), the Pre-017 Correctness Repair (active representation
sidedness + re-Freeze guard + device verifier), Gate P1 (physical ARM64
closure), Stage 016-R2, Stage 016 (Plane), Stage 015D (camera projection),
Stage 015C-R (front-face culling), Stage 015C (shading), Platform Fix P2,
Stage 015B, Stage 014, the NDK r29 migration (Gate P0) and the owner decision
baseline.
**Next Stage:** UI-R2 — Workspace Composition Redesign.

## INPUT-R1 — Pointer Semantics Foundation (IMPLEMENTED, on-device run UNVERIFIED)

**What crosses the boundary now.** A `TouchPointer` carries a stable id and
view-local pixels as it always did, plus three things it did not: a
platform-neutral tool type, a contact pressure and a two-angle tilt. The path is
`MotionEvent` → `ForgeShapeSurfaceView` → `PointerSemantics` → `NativeViewport.touchEvent`
→ `forgeshape_jni.cpp` → `forgeshape::TouchPointer`, and no Android constant, axis
id or Java value exists past the Android layer.

| field | units / range | fallback |
| --- | --- | --- |
| `toolType` | `PointerToolType`: Unknown, Finger, Stylus, Eraser, Mouse | `Unknown` — an ordinary contact pointer, never a dropped event |
| `pressure` | `[0, 1]`, normalised; 1 = the device's full force | `1.0` when non-finite or unreported; clamped otherwise |
| `tiltRadians` | radians `[0, pi/2]` from perpendicular | `0` when non-finite; clamped otherwise |
| `tiltOrientationRadians` | radians `(-pi, pi]` in the screen plane, 0 = screen -y | `0` when non-finite or when tilt is 0; **wrapped**, not clamped |

**Full pressure is the no-sensor default, not zero.** A finger on a screen with
no force sensor IS in full contact, and a `0.0` default would make a future
pressure-driven brush do nothing on most hardware. That is also why every
pre-existing `TouchPointer{id, x, y}` call site still compiles and still means
exactly what it meant: the struct's defaults are Finger, full pressure, no tilt.

**Pressure and tilt are CARRIED, not CONSUMED, and that is asserted.** No brush,
camera or selection rule reads them. The sculpt suite drives the same stroke
geometry at both ends of the pressure range, with opposite tilts, for all four
tools on freshly frozen meshes, and compares vertices **bit-exactly** — a
tolerance would have hidden precisely the small modulation the check exists to
catch. Eraser switches no tool; Mouse gets no wheel, hover or context behaviour.

**Hover stays deferred.** `translateAction` still drops hover, scroll and button
actions rather than forwarding them, so hover has no representation below the
boundary at all. Giving it one is a consumer-driven question and belongs to
whichever stage first has a consumer.

**The tilt model is two angles because that is the smallest one that keeps
direction.** Android reports exactly these two axes; an Apple Pencil's
altitude/azimuth converts into them with arithmetic alone
(`tilt = pi/2 - altitude`). It is deliberately not a full stylus pose — no
barrel rotation, no hover distance, no button state.

**Ownership is split once, each way.** The Android tool-type constants stop at
`PointerSemantics`, which maps them onto ForgeShape's own wire codes; ranges and
non-finite fallbacks are owned natively in `forgeshape_input.h` and applied in
`forgeshape_jni.cpp`. Neither side repeats the other's rule, so they cannot
disagree. The four stylus arrays are individually optional: pass null and every
pointer keeps its documented default, which is exactly the pre-stylus behaviour
and is the path the older instrumented touch helpers now take.

**`debugLastPointerEvent` is the observation seam** — DEBUG-only, bounded to
`kMaxTrackedPointers`, read-only, compiled to a `-1` stub in a release build. It
exists so an instrumented test can prove transport without a debug overlay and
without Java ever owning pointer data. It is snapshotted BEFORE arbitration, so
what a test reads is what the camera, the selection and the sculpt arbitration
are about to be handed.

**Nothing about existing behaviour moved.** No renderer, scene, geometry,
`ObjectId`, `SceneSnapshot`, `MeshRevision` or brush-algorithm change is in this
stage, and the 8 px sculpt promotion threshold, the pending-then-promote rule,
the two-finger navigation rule, the tap slop and the 6-pointer bound are
untouched.

**What is UNVERIFIED.** The instrumented suite and the runtime walkthrough did
not run: `ForgeShape_Stage006` failed to reach a ready state on two launch
attempts and the owner elected to defer device testing. The 13 new instrumented
cases compile and are packaged into the test APK, but no on-device result exists
for them, for the 45 + 14 new native checks, or for the walkthrough. Real stylus
hardware remains UNVERIFIED as it was before this stage.

## UI-R1C2 — World Grid + Adaptive Workspace (COMPLETE)

Two questions closed together because both are about **context**: what the
viewport gives the eye besides the model, and what a large window gives the user
besides padding.

**The grid is a viewport reference, and that is a contract, not a description.**
It has no `ObjectId`, never enters `ConstructionScene` or a `SceneSnapshot`, is
not a `RuntimeMesh`, is invisible to picking, takes no part in Freeze, Resume or
a sculpt stroke, is not exported and is **not** a snap target. A future Sketch
grid — the one that snaps, drawn on a sketch plane — is a different contract with
its own approval and must not be grown out of `forgeshape_grid.h`.

**Visibility lives in `DisplaySettingsStore`**, beside the shading model, the
viewport background and the reduced-motion bool, because it is the same class of
value. It is a plain `bool` rather than an index: there are two answers and no
third is coming. Process-scoped, so it survives rotation, an Activity recreation
and HOME/resume for free and a process kill returns it to the default, which is
**on** — an empty viewport with no floor gives the eye no scale, no horizon and
no origin, and that is the first question a modelling application has to answer.

**The technique is the smallest one that works: a static line list.** 82 lines /
164 vertices / **2624 bytes**, generated from compile-time constants and uploaded
**exactly once** with the Vulkan device, through the mesh path's own staging
buffer. There is no cache to invalidate and no revision to follow because there
is no input that can change. Showing or hiding the grid therefore decides only
whether **one** already-built draw call is recorded — `vkCmdDraw`, non-indexed,
after every body. `FORGESHAPE_GRID_UPLOAD_OK` is logged once per device, so a
second occurrence in a session log is direct evidence that something re-uploaded
a constant.

*The tier is in the buffer and the colour is not.* Each vertex carries a
`GridLineTier` (minor / major / X axis / Z axis) as a float; the four colours ride
in push constants. So switching Dark to Light rewrites four `vec4`s and
re-uploads nothing at all. The block is 128 bytes exactly — the guaranteed
minimum, the same budget `surface.vert` works inside — which is also why a fifth
line weight is not something this grid can grow.

*Spacing and extent are chosen against the camera, not taste.* Minor lines every
**1 m**, major every **5 m**, half-extent **20 m**. The initial orbit distance is
8.2 m at a 60° vertical field of view, so the default framing is about 9.5 m tall
and a one-metre cell puts roughly ten cells across the viewport — and because the
spacing IS the unit the Property Inspector shows, counting cells is counting
metres. The extent is fixed on purpose; a grid that re-tessellated as the camera
dollied would be the multi-decade CAD system this stage is told not to build.

*A radial fade is what lets a bounded grid read as an open plane.* Without it the
floor ends at a hard square border 20 m out and reads as a table top. The fade
runs from 55 % of the half extent to nothing at the border. **It must be computed
per FRAGMENT**, and that is not a performance choice: a grid line's only two
vertices are its far endpoints, both fully faded, so a per-vertex fade
interpolates zero across the whole line and every line disappears at every
radius. It was written per-vertex first and did exactly that.

*Both appearances follow three rules* — weight comes from **alpha**, not from a
brighter colour; the two axes are separated by a faint warm/cool lean rather than
by saturated primaries, so X and Z are distinguishable without turning the floor
into a diagram; and every value is authored against its own background, because
"darker" and "lighter" are opposite instructions in the two themes. The
self-tests assert those as *relationships* — lines lift off the near-black ground
and sit down into the cream one, minor < major < axis, nothing above 0.6 alpha —
rather than pinning RGB literals that would break on any deliberate restyle.

**The coplanar case is settled with depth state, never by moving anything.** A
Construction Plane at world y = 0 shares the grid's plane exactly. The grid is
depth-**tested** so the model occludes it, depth-**write off** so it contributes
nothing a later draw could be occluded by, drawn **after** every body, and pushed
`1e-4` of the depth range further from the eye **in the vertex shader**. The
domain Plane keeps its y, its transform and its 4:6 topology; nothing in the
Construction Source moves for a presentation problem. The nudge is in the shader
because Vulkan's `depthBiasEnable` is defined for **polygon** fragments and would
have done nothing at all to a line list — it would have read as the fix while
being inert.

**The adaptive pass promoted `railDocked()`, which had a tested meaning and had
never once been called.** Its semantics were audited first and were sound, so it
was activated rather than replaced. "Docked" means what it already meant for the
Property Inspector: the surface is drawn as **part of the layout** — flush,
opaque, no elevation — instead of as a raised translucent card standing on the
picture. On an expanded window the card is a lie, because there is room beside the
model and the rail is in it.

**Objects gets its own persistent column on a window that has earned one.** The
gate is arithmetic, not a fourth breakpoint: EXPANDED is necessary and not
sufficient, and a window qualifies when a **180 dp** Objects column, the rail and
the inspector still leave a central viewport at least **480 dp** wide — "as wide
as a phone screen", the absolute half of the floor the expanded layout has always
been held to. Deriving it means a later change to any column width moves the
answer automatically instead of silently violating the floor. So 840 dp — the
bottom of the expanded range, which is a large phone in landscape or a small
tablet — deliberately does **not** get one; three permanent chrome columns there
is precisely the desktop-CAD clutter UI-OWNER-02 rules out. The threshold lands
near 1012 dp.

*One `ObjectsSectionView`, re-parented — never a second list.* A second Java
Objects view would be a second place for "which body is active" to be remembered,
and that answer lives in exactly one place, below JNI. Because the same view
moves, a viewport pick, a row tap and Add Body all still end at the same one
native fact and the same one `refreshFromNative()`, in every window. The move is
**instant**: it runs inside `onMeasure`, and starting an animation from a measure
pass is how a re-parented surface ends up laid out at zero height. `ChromeMotion`
is untouched.

*Scroll ownership.* Docked, the list lives in its **own** `ScrollView` and is no
longer nested inside the inspector's, so the two cannot fight over a drag. The
UI-R1B1 rail tap-versus-scroll rule is untouched — docking changes what the rail
looks like, not how it negotiates a gesture.

**The `SurfaceView` is the whole window in every mode.** Docking, un-docking and
re-parenting chrome rearrange chrome and never the render target. The only
swapchain events observed during the walkthrough came from a genuine window
resize (rotation), and the grid was **not** re-uploaded across it, because its
buffer is device-scoped.

**Two defects found in this stage's own new code, both by runtime evidence
rather than by review**, and both worth recording because the code looked
correct: `VK_POLYGON_MODE_LINE` was named for a line list, which describes
*polygon* rasterization and would only have required the `fillModeNonSolid`
device feature ForgeShape does not request; and the alpha blend wrote each line's
own weight into the destination **alpha** channel, which the Android compositor
honours, so the viewport would have been punched translucent along every grid
line. Both are fixed; the alpha channel is now explicitly preserved.

**Scope.** No Selection Outline, no snap-to-grid, no Sketch grid, no View Cube,
no camera focus or named views, no object commands (delete / duplicate / hide /
lock / rename / group), no parent/child, no Undo, no Sketch/Extrude, no
import/export, no stylus pressure, no glass or blur, no post-processing
framework, no new dependency, no Compose, and no NDK/Gradle/AGP change.

**Verification.** Native **1631/1631** across eleven suites, zero failures — the
render-shading suite took 264 to **325** with the `r1c2_*` family. JVM **50/50**
(was 43); `WorkspaceLayoutModeTest` went 9 to 16. Instrumented **111 run, 110
green** (was 98/97), and the suite was run **twice** — once in the default
compact phone window and once under an overridden 1600 x 2560 @ 240 dpi expanded
window, so the docked branches are genuinely exercised rather than simulated. The
one failure in both runs is the documented `ui11` IME case, which fails its own
precondition guard with a character-identical message before reaching any product
assertion. Both ABIs build. `DEV2-01..07` and `DEV3-01..06` PASS.

**Runtime**, on `ForgeShape_Stage006` / `emulator-5580`, confirmed by AVD name.
Four real Grid toggles through the real chips produced **zero**
`MESH_UPLOAD_OK`, **zero** `RENDER_MESH_BUILD`, **zero** `GRID_UPLOAD_OK`, zero
`SURFACE_CONFIG` and zero `CAMERA_VIEWPORT`. A Plane 2 x 1.25 m applied at world
y = 0 — coplanar with the grid — rendered as a clean unbroken sheet with the grid
stopping at its edges, and **two consecutive frames of that static scene were
byte-identical**, which is the z-fight evidence: a fight is per-pixel and would
differ. It stays clean at a near-coplanar grazing angle, which is the worst case.
Dark and Light were both confirmed readable by eye, and Perspective and
Orthographic both correct (parallel lines stay parallel in ortho). Grid Off
survived a HOME/resume with the chip reading back Off from native truth. In the
expanded window the docked Objects column measured **270 px at 1.5x = exactly
180 dp**, sat beside a docked Property Inspector with the model between them,
Add Body worked from the column, and the scene was grown to **20 bodies** which
scrolled in the column's own container with Add Body still reachable at the
bottom.

**R1C2 criteria.**

| ID | Verdict | Evidence |
| --- | --- | --- |
| R1C2-AC01 | PASS | clean `4a476a9` audited before any change; final tree clean |
| R1C2-AC02 | PASS | `r1c217_*` x2: the View group is one working chip pair, no disabled promises, 44 dp measured |
| R1C2-AC03 | PASS | `r1c2_01_*`/`r1c2_02_*`; `DisplaySettingsStore::setGridVisible`, process-scoped |
| R1C2-AC04 | PASS | `r1c2_04_every_grid_vertex_is_on_world_y_zero`; see *the grid is a viewport reference* |
| R1C2-AC05 | PASS | `r1c2_03_*`: real scene snapshotted either side of four real toggles, identical |
| R1C2-AC06 | PASS | `r1c2_06_*`, `r1c219_*`; runtime zero upload / zero rebuild across four toggles |
| R1C2-AC07 | PASS | 2624 bytes uploaded once per device; `GRID_UPLOAD_OK` appeared exactly once |
| R1C2-AC08 | PASS | `r1c2_07_*` palette relationships; runtime screenshots, Dark and Light |
| R1C2-AC09 | PASS | `r1c2_09_*`; runtime Perspective and Orthographic both correct |
| R1C2-AC10 | PASS | `r1c2_08_plane_source_topology_is_still_4_6`; two byte-identical frames of a coplanar Plane |
| R1C2-AC11 | PASS | `R1C2-11`, `r1c227_*`; compact keeps Objects in the shape editor |
| R1C2-AC12 | PASS | `R1C2-12`: medium docks nothing new and gains no breakpoint |
| R1C2-AC13 | PASS | `R1C2-13`, `r1c227_*`; runtime expanded column measured at 180 dp |
| R1C2-AC14 | PASS | `R1C2-14`, `r1c234_*`; `railDocked()` audited then promoted, elevation 0 when docked |
| R1C2-AC15 | PASS | one `ObjectsSectionView`, re-parented; `r1c237_*` asserts one parent and one row per body |
| R1C2-AC16 | PASS | `r1c229_*`, `r1c230_*`; runtime Add Body from the docked column added Body #5 |
| R1C2-AC17 | PASS | `r1c232_*`; runtime 20 bodies scrolled and selectable |
| R1C2-AC18 | PASS | `r1c236_*`: the SurfaceView equals the workspace in every mode and orientation |
| R1C2-AC19 | PASS | `r1c235_*`: selection publishes nothing with the grid on or off; pulse constants untouched |
| R1C2-AC20 | PASS | `r1c234_*`; the UI-R1B1 rail rule is unchanged |
| R1C2-AC21 | PASS | no Outline, snap, View Cube or object command — see *Scope*, asserted by `r1c217_*` |
| R1C2-AC22 | PASS | native 1631/1631, JVM 50/50, instrumented 110/111 twice, both ABIs |
| R1C2-AC23 | PASS | picking suite still 128; `SIDE`, `REFR`, `NOR`, `CAMPROJ`, `PLN` unchanged |
| R1C2-AC24 | PASS | every core doc re-measured and under 2000; this file and `ARCHITECTURE.md` compacted — see below |
| R1C2-AC25 | PASS | one focused commit |
| R1C2-AC26 | PASS | clean tree |

**Core document sizes at acceptance** (hard cap 2000 each), every one
**re-measured** rather than carried forward — the counts recorded at UI-R1C1 were
stale in both directions, so no number here was inferred from the previous report:

| file | at UI-R1C2 baseline | now |
| --- | --- | --- |
| `PROJECT_STATUS.md` | 1385 | **1319** |
| `ARCHITECTURE.md` | 1982 | **1989** |
| `PRODUCT.md` | 762 | **805** |
| `README.md` | 403 | 403 |
| `CLAUDE.md` | 215 | 215 |

This file **shrank while gaining a stage chapter**: the UI-R1C1 chapter was cut
from 160 lines to a 30-line durable entry, and Technical Debt, the suite
sections, Known Issues and the closed-stage entries were compacted, retiring
about 330 lines of superseded prose against roughly 260 added.

`ARCHITECTURE.md` is the honest exception and is recorded as such. It began this
stage at **1982**, already at 99 % of the hard cap and roughly double its own
700–1000 target — pre-existing debt this file has carried for several stages. It
had to gain the grid-renderer and adaptive-workspace ownership it now owns, and
paying for that took a compaction of fourteen sections (the layer map, the
appearance, verification, JNI, camera, sculpt, shading, threading and boundary
prose). It ends at **1989**: under the cap, but by 11 lines. **The ≤1850
aspiration was not safely achievable here**, because the remaining content is
dense ownership statements rather than narrative, and cutting it would delete
durable facts. Closing it properly means a dedicated compaction pass — which is
exactly the unrelated-debt rewrite the rules forbid bundling into a feature
stage. **The next stage to touch `ARCHITECTURE.md` should budget for that pass
before adding anything**, because 11 lines is not a working margin.

**Result: COMPLETE.** R1C2-AC01..26 PASS.

## Closed stages — durable facts only

Full narrative for every stage below lives in Git history. What is kept here is
only what still constrains the code.

**UI-R1C1 — motion language + selection feedback (COMPLETE).** Selection is an
acknowledgement, not a costume: becoming selected raises the tint to alpha
**0.55** and decays it over **220 ms** to a resting **0.20**, on a smoothstep,
monotone and bounded. The hue is unchanged (`1.00, 0.62, 0.10`). **A pulse
acknowledges a CHANGE in selection truth**, not a tap. It costs one float per body
per draw — no animator, no invalidate, no geometry, no revision, no upload.
Per-body pulse state lives in `BodyRenderResources`, keyed by the same stable
ObjectId as that body's GPU buffers, which is what makes A's pulse structurally
unable to touch B's; the renderer is still never told *which* object is selected.
The math is a pure function over an explicit frame delta in
`forgeshape_selection_pulse.{h,cpp}`, not a clock read, and a delta over 100 ms is
a resume or a stall and is clamped.

*`ChromeMotion` is four things and must not become a framework:* the two durations
(**120 ms** arriving, **90 ms** leaving), the reduced-motion question, the
cancel-first rule and one alpha helper. No transition type, no registry, no way to
describe motion as data. Reduced motion crosses JNI as one bool, and
`setReducedMotion` deliberately does **not** advance `changeCount_` — it is a
platform preference arriving, not a display setting chosen. `ChromeMotion.duration`
returns **0** rather than a small number and every caller branches on the zero to
*land* on the final state; in the viewport reduced motion goes straight to the
**resting** tint.

*Chrome hide/restore is alpha only* — the `SurfaceView` is full-bleed, so there is
no size for a transition to change. The inspector's two directions are sequenced
rather than symmetric: its size changes **once** per toggle and the body fades and
slides either side of it, because animating the height would re-run the adaptive
layout decision (which lives in `onMeasure`) every frame. Only a user act
animates. **A viewport gesture outranks motion:** while a pointer is down on the
model every chrome detent change is instant.

**UI-R1B2 — theme system + Light mode (COMPLETE).** Two appearances, chosen
explicitly; a theme is presentation all the way down, so switching one mints no
revision, publishes nothing, uploads nothing, and leaves every body, id,
parameter, placement and frozen mesh bit-identical.

*The mechanism is theme attributes, not duplicated components.* A role is declared
once in `attrs.xml`, given a value once per theme in `themes.xml`, and referenced
as `?attr/fs*`. Backgrounds are `res/drawable` state lists and content colours
`res/color` state lists; the one imperative path is
`EditorControlStyles.themeColor`, for text roles and the two Canvas-drawn brush
sliders. **No colour is written in Java and no component knows which theme it is
in** — one `bg_control.xml`, one `chip()`, no `if (light)` anywhere, so a third
theme would touch two resource files. `fsAccentFill` (what an ACTIVE control is
filled with) and `fsPrimaryFill` (what a PRIMARY COMMIT is filled with, carrying
`fsTextOnPrimary`) are two roles because on light an active chip wants a pale tint
with dark text while Apply wants a solid accent with white.

*Dark is unchanged* and is what a fresh process wears; a process kill returns to
it. *Light* uses a warm off-white viewport, `#E6E1D9` — paper rather than screen,
deliberately not `#FFFFFF`, which makes a neutral clay render read as grey. **The
light chrome surface is fully opaque where the dark one is 95 %**: over cream, a
near-white translucent panel loses the edge that separates it from the model.

*The renderer seam is one closed enum.* `ViewportBackground` (`NeutralDark`,
`WarmLight`) lives in the native display store; what crosses JNI is its index,
refused if unrecognised. The clear value is written into the render pass every
frame anyway, so a switch touches no swapchain, pipeline, descriptor set or
buffer. The two float triples are pinned by self-tests because they are duplicated
in `colors.xml` as the Android *window* background and drift there is a launch
flash.

*Applying a theme recreates the Activity, and that is free,* because **`onDestroy`
skips `NativeViewport.stop()` while `isChangingConfigurations()`**: the render
thread, the Vulkan device and every GPU buffer survive, and `start()` returns early
rather than re-running the self-tests. Without that guard a theme change would tear
down the device and re-upload the whole scene. `EditorUiState` is handed to the
incoming workspace, so changing colour does not reset the display unit, the
inspector detent or the Tool Rail.

**UI-R1B1 — visual foundation + start flow (COMPLETE).** Fifteen local vector
drawables on one 24 dp grid with one 2 dp round stroke replaced the Unicode glyphs
the chrome used as icons; they are drawn white and tinted at use from a state
list, so one drawable serves idle, active and disabled and an icon inside a
composed control cannot disagree with its parent. No icon dependency was added.

*The look is resources, not Java.* Pressed and active states come from the platform
instead of a repaint call, instances share one parsed `ConstantState` instead of
allocating a `GradientDrawable` per control on every rail rebuild, and UI-R1B2 was
able to add a second theme without touching a component tree. Corner radius is
three semantic levels (control / floating surface / sheet); type is five roles;
floating surfaces carry a small elevation and the **docked** inspector deliberately
carries none, because it sits beside the model rather than over it.

*Two defects, both proven pre-existing.* A `LinearLayout` that has run out squeezes
its LAST child, so Display and Hide UI measured **33 dp and 35 dp** on a 411 dp
window, under the 44 dp floor; the editing-context label is now the weighted child,
and `R1B1-10b` **measures** rather than trusting the declared size. And the Display
popover's first open grew from the wrong corner because `setOpen` reads
`getWidth()` before the panel has ever been laid out; the pivot is now also set in
`onSizeChanged`.

*Tool Rail gesture ownership.* The rail lives in a `ScrollView`, which takes a
gesture as soon as it passes the platform slop — including from a control whose job
is to be tapped. An entry now disallows interception on Down and allows it again
once travel passes **twice** the slop, at which point the platform's own
`ACTION_CANCEL` prevents the click. Small drift selects; a real scroll selects
nothing. Viewport gesture arbitration is untouched.

*The start flow.* `StartChooserView` asks **New Project** over the live viewport and
offers exactly two answers, once per process. *Construction* has nothing to build:
native state exists before any view, so the default Body is already there and the
answer only stops asking. *Sculpt* makes exactly the two calls a user would make by
hand — `applyConstructionSphere` with the diameter **read back from native state**
rather than a constant invented in Java, then the existing `freezeToSculpt`. **No
Freeze logic is duplicated**, so Back finds the exact sphere and Resume returns the
same frozen mesh for the ordinary reasons. A refusal leaves the product in
Construction, unchanged, and says so.

**Stage 017 — multi-object scene + hierarchy foundation (COMPLETE).** A
platform-neutral `ConstructionScene` owns an ordered collection of Construction
Bodies; each `SceneObject` owns its own `ConstructionObject`, `MeshStore` and
`FrozenSculpt`.

*The three accessors survived their names and changed their meaning.*
`constructionObject()`, `meshStore()` and `sculptSession()` are defined in
`forgeshape_scene.cpp` as **"the ACTIVE body's"**. That is what kept the migration
small: every caller meaning "the object the user is editing" kept working, and only
code meaning "every body" — the renderer and scene picking — was rewritten. They
live there rather than beside their own types because the answer is a scene
question, and defining them in their own translation units would make those units
depend on the scene, which depends on them.

*ObjectId* is monotonic, scene-minted, never reused, and never derived from a
collection index, a `MeshRevision` or a GPU resource. It survives primitive edits,
transform edits, Freeze/Resume/re-Freeze and selection changes. The first body
keeps `kConstructionBoxObjectId`, so **startup is byte-for-byte the single-object
product's** (`S17-01`).

*Publication is per body*, so revisions are per-body chains starting at 1 and an
edit to A cannot replace, invalidate or renumber B's mesh. `snapshot()` returns an
immutable `SceneSnapshot` — per item an ObjectId, a `RuntimeMeshPtr`, a model and
inverse-model matrix, and a selection flag — copying a `shared_ptr` and two
matrices per body and **no geometry**, so it is cheap under the state mutex and
usable with that mutex released; old snapshots stay valid because they hold the
revisions they name alive.

*Per-body GPU state* lives in `BodyRenderResources`, keyed by stable ObjectId —
never by scene index, which would rebind a body's buffers to a different body if
the collection were reordered. Staging, the upload command buffer and the fence
stay shared: transient scratch inside one upload. `FORGESHAPE_RENDER_MESH_BUILD`
and `FORGESHAPE_MESH_UPLOAD_OK` carry an appended `body=` field.

*Picking iterates the scene.* `pickScene` intersects each item with **its own**
transform and **its own** sidedness and keeps the nearest positive hit; ties keep
the earlier body in scene order. Distances compare across bodies because every
Construction transform is rigid.

*Sculpt: what is per body and what is not.* **Per body** (`FrozenSculpt`): the
Frozen Sculpt Mesh and its stale flag. **Global** (one `SculptSession`): the product
mode, the held tool, radius, strength, the stroke in progress and the
session-lifetime stroke count. `sculptSession()` re-points the session at the active
body's `FrozenSculpt` on every access — one pointer write that removes the whole
class of bug where the session points at the body the user left. A per-body session
was tried and was wrong twice over: the product mode became ambiguous, and the
documented "Radius and Strength are shared" contract would have broken silently.
Body switching is refused while in Sculpt mode.

*Scope.* Flat root-level collection only: deterministic insertion order, stable
enumeration, no parent/child, groups, reparenting, rename, reorder or speculative
parent field; no delete, duplicate, hide or lock.

*The scene suite builds its OWN `ConstructionScene` per case* rather than touching
the process-scoped one — the Stage 016-R2 lesson applied to the scene. Teeth were
verified by reverting: a global "something is selected" fails both S17-08 checks,
and first-hit-wins fails both S17-10 checks.

**Pre-017 Correctness Repair (COMPLETE).** *Sidedness ownership.* Four separate
places disagreed about it. **The rule now: sidedness is a property of the active
published representation**, carried `ConstructionMesh::renderBothSides` →
`SculptMesh::renderBothSides()` → `RuntimeMesh::renderBothSides()`, and read from
there by render, selection picking and the Sculpt hit-test alike — never from a
`PrimitiveKind`, which says nothing about a *different* body's geometry and is
wrong in both directions once a frozen mesh outlives its Source.
`forgeshape_selection.cpp` deliberately does not include
`forgeshape_construction.h`. Asserted by `SIDE-01`..`09`.

*The destructive re-Freeze guard* read the session-lifetime stroke count, which is
never reset by a Freeze, so every later re-Freeze of an untouched mesh raised a
dialog with nothing behind it. **The predicate is now `SculptMesh::hasEdits()`** —
`revision > kFrozenSculptRevision`, restarted by every `freezeFrom` and advanced
only when a stroke actually moved a vertex. Exposed as `SCULPT_HAS_EDITS`; the
dialog quotes no number, because any count available describes meshes that no
longer exist. Asserted by `REFR-01`..`07`. UI-OWNER-05 intact.

*The device verifier* gained `DEV3-01`..`06` on top of `DEV2-01`..`07`: it
recognises a bare `adb` in any form, scans `.ps1`/`.cmd`/`.bat`/`.sh` and the
Gradle files, detects an executable `connected*AndroidTest` fan-out, proves an
argument array actually carries `-s` instead of trusting its variable name, and
reports which surfaces it scanned so a check that covered nothing cannot pass. It
also fixed a latent vacuous pass: `Where-Object` returning exactly one object has
no `.Count`, so the script printed "All checks PASS" whenever exactly one check
failed.

**Gate P1 — physical ARM64 (COMPLETE).** Closed on a Samsung Galaxy S25 Ultra
(`SM-S938B`, Snapdragon 8 Elite, Android 16 / API 36, 1440×3120) over Wi-Fi adb;
`arm64-v8a` only, `primaryCpuAbi=arm64-v8a` after install, `getconf PAGE_SIZE`
**4096**. The 16 KB dimension was closed separately on a dedicated 16 KB target
(`ForgeShape_16K`), so the two pieces of evidence are complementary.

*The ARM64 picking defect, and why the tolerance exists.* Six checks failed on the
physical device against 1421/1421 green on x86_64 from identical source.
`intersectRayTriangle` tested barycentric containment with exact bounds, and a ray
landing on an edge two triangles SHARE has a coordinate that is mathematically
exactly 0 — its sign decided purely by rounding, and if it rounds negative for one
triangle it does for its neighbour too, so both reject a ray that geometrically
hits. The rounding is ABI-dependent: no `-ffp-contract=off` is set, so Clang
contracts the dot/cross products into fused multiply-adds on arm64-v8a, which
baseline x86-64 cannot. An on-device probe measured the failing ray at
**u = -9e-9**. Product-visible: the centre of a Plane *is* the shared diagonal of
its two triangles, so tapping the middle of a Plane selected nothing. Fixed by
`kBarycentricEpsilon = 1e-6f` applied to both containment bounds — widening rather
than pinning the FP model, which would only re-hide the same knife-edge geometry.
**Do not remove or tighten it.**

*Heavy-mesh ladder, measured on that phone.* Single-run figures from one device;
existence evidence that the mandatory tiers work on physical ARM64, not a supported
capacity limit.

| tier | vertices | triangles | gen ms | publish ms | render build ms | pick | PSS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ~10k | 10086 | 19200 | 2.763 | 1.695 | 19.577 | tri 15592 | 219 MB |
| ~50k | 49686 | 97200 | 17.776 | 7.880 | 72.204 | tri 78824 | 231 MB |
| ~100k | 99846 | 196608 | 32.405 | 15.426 | 104.387 | tri 159210 | 248 MB |

Sculpt at ~100k: freeze 110.126 ms building 592896 adjacency entries, two real Grab
strokes, every post-stroke upload `reuse`, PSS 264 MB. No crash, OOM, ANR or thermal
symptom at any tier. The last stable mandatory tier is ~100k; 250k/500k were not
run. Stylus/S Pen remains **UNVERIFIED** — closing it needs a person physically
moving a pen.

*Also closed under Gate P1:* arm64-v8a packaged beside x86_64, both `.so`s at
0x4000 ELF `LOAD` alignment with `zipalign -P 16 -c` OK; a debug-only Vulkan
validation session with **zero ForgeShape-caused messages** of any severity (the
layer was adb-pushed through Android's own per-app GPU debug layer settings and
never bundled, never committed, and removed afterwards); and the debug-only
`buildStressMesh` density fixture behind `ForgeShapeActivity` keys A–F.

**Stage 016-R2.** Self-tests must not depend on live process-scoped state: three
cases were reading whatever primitive or transform a prior UI test had left, and
now publish their own fixture. `scripts\start-forgeshape-emulator.ps1` takes an
explicit `-Avd`/`-Port`, hard-rejects port 5554 before any OS or adb call, and
BLOCKS rather than falling back when the requested port is occupied. Every adb call
in every repo script is scoped to an explicit serial.

**Stage 016 — the Plane.** The sixth and final Construction MVP primitive: a flat,
zero-thickness rectangular sheet authored by width (local X) and depth (local Z),
centred on the local origin at `y = 0`, canonical front along local `+Y`. Source
topology is exactly **4 vertices, 2 triangles, 6 indices** whatever the dimensions,
CCW seen from the front. Two-sided render and pick are one bounded, named exception
carried by the published mesh. `ConstructionObject::setPrimitive` `std::visit`s the
requested spec, so a kind added to the variant with no matching overload is a
compile error rather than a silently skipped branch.

**Stage 015D — camera projection.** Orthographic is a true parallel projection
(`mat4Orthographic`, `m[11] = 0`), not a narrow FOV or a huge distance. The existing
60° Perspective camera was audited and left unchanged. Switching converts the
framing rather than resetting it (`orthoHalfHeight = distance × tan(fovY/2)` and its
inverse), so the frame never jumps. Orthographic pinch changes the world span and
deliberately leaves the orbit distance alone, because moving the eye along its own
axis changes nothing in a parallel projection. Picking is structurally different per
projection — Perspective keeps one origin with fanning directions, Orthographic
shares one direction with a per-pixel origin — and the orthographic view plane is
pulled back to `kFarPlane/2`, so `snapshot.eye` is **not** `target + dir × distance`
in Orthographic. The sculpt brush radius must not scale with depth in Orthographic.

**Stage 015C-R — winding and culling.** The pipeline named
`VK_FRONT_FACE_CLOCKWISE`, which double-counted the projection's Y flip and inverted
back-face culling, so every convex primitive drew its far walls and read as a hollow
interior. The convention is now `VK_CULL_MODE_BACK_BIT` +
`VK_FRONT_FACE_COUNTER_CLOCKWISE`, owned by `ARCHITECTURE.md`. The `NOR-01`..`10`
direction family exists because every earlier normal check measured an axis or a
magnitude and so passed unchanged on a mesh whose normals had all been negated.

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
| 16 KB page size *and* ARM64 in one target | **UNVERIFIED** — see Known Issues |
| Stylus / S Pen tool type and pressure on real hardware | **UNVERIFIED** — needs a person physically moving an S Pen |

## Self-test suite

Eleven debug-only native suites run once from `NativeViewport.start()` — never
per frame — and total **1690 checks** (1631 with zero failures as of UI-R1C2;
the 59 INPUT-R1 checks are authored and compile but have no on-device run yet):

| suite token | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 173 |
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

**The render-shading suite owns PRESENTATION**, which is why selection feedback
(UI-R1C1, `r1c1_01..06`) and the world grid (UI-R1C2, `r1c2_*`, 264 → **325**)
live there rather than in the picking or selection suites — those own *which*
object is selected and what is in the scene, not how any of it is drawn. Both
families are driven with explicit values rather than a clock or a GPU, so a whole
220 ms pulse costs microseconds and nothing in either can be flaky. Neither
asserts a rendered pixel.

The suite also covers the crease policy, all six primitives' Smooth contracts,
the capsule equality case, Faceted, NaN/Inf and fail-closed behaviour,
determinism, the render-data rebuild policy, the generated MatCap, the display
settings, the proof that building render data leaves the authoritative
`RuntimeMesh` bit-identical, and (Stage 016) the Plane's two-sided render
duplication and its inertness to display switching (`PLN-09`/`10`/`20`). Stage 016
also added `PLN-01`..`08` to the primitive suite, `PLN-11`..`16` to picking and
`PLN-17`..`19` to sculpt.

**The projection family `CAMPROJ-01`..`14`** is split across three suites by
module ownership: camera carries `01`..`08` and `12`..`14`, picking `09`/`10`,
sculpt `11`. **The direction family `NOR-01`..`10`** covers source winding,
outward render normals, Faceted orientation, duplication-preserves-winding, the
model→view normal transform and display-mode inertness. Both families include a
member that would fail if the behaviour collapsed to a constant —
`nor_outwardness_fails_on_global_normal_flip` asserts the measurement inverts
under a global `normal *= -1`, which is the property the rest of the suite lacked.

**The picking suite also owns the POINTER BOUNDARY** (`128` → `173`), because it
is the suite that already owns `TouchPointer` and the selection rules the same
events drive. Those 45 checks cover the tool-type wire codes and their Unknown
fallback, the struct defaults, pressure and tilt range/wrap/non-finite behaviour,
per-index packing, the unchanged 6-pointer bound, and the proof that a stylus
resolves the same tap and orbits the same distance as a finger. The Android half
of the tool-type mapping is deliberately not here: it is JVM and instrumented,
where `MotionEvent` exists. The sculpt suite's 14 added checks (`302` → `316`)
are the pressure-independence teeth, four tools → three assertions each.

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

**180 tests** (56 JVM, 124 instrumented). The JVM set is green at 56/56; the
instrumented set has no run for this stage — see INPUT-R1 above. No Java test asserts a rendered pixel;
every control is reached by its stable semantic id and no assertion uses a screen
coordinate. The foundation and theme suites deliberately assert no colour
literal, radius or shadow — those are judged by eye and by runtime evidence, and
pinning them would break on every deliberate restyle. What the theme suite
asserts instead is *relational*, plus WCAG contrast ratios computed in the test.

**110 of the 111 instrumented tests passed at UI-R1C2**; the one failure is the
`ui11` IME case described below. Pre-017 took the count 46 → 50, Stage 017
50 → 56, UI-R1B1 56 → 71, UI-R1B2 71 → 88, UI-R1C1 88 → 98, UI-R1C2 98 → 111,
INPUT-R1 111 → 124 (unrun).

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

**The scene is process-scoped and there is no delete, so bodies ACCUMULATE across
a run**, and the product may be left in Sculpt mode by an earlier case. No
instrumented test may assume a body count, which body sits at the origin, or
which mode is current: each establishes what it needs and asserts relative to what
it found. That is the Stage 016-R2 lesson applied to the instrumentation.

**A view that has just been made visible reports a size of 0 until a traversal has
run.** Measuring a control in the same block that opened its container is the same
mistake the display popover's own first-open pivot bug was; settle the layout in
between. Found again during UI-R1C2, on the new Grid chips.

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

- **Native self-tests:** eleven suites, **1631 checks, zero failures** on a clean
  launch. The Gate P1 picking suite is unchanged at 128, so the ARM64 shared-edge
  fix is intact; `SIDE`, `REFR`, `NOR`, `CAMPROJ` and `PLN` all still green.
- **JVM:** 50/50.
- **Instrumented:** 111 run, **110 green**, twice — once compact, once expanded —
  through `scripts\run-instrumented-tests.ps1 -Serial emulator-5580`. The one
  failure is the documented `ui11` precondition guard.
- **Device guards:** `DEV2-01`..`07` and `DEV3-01`..`06` all PASS, with no device
  attached and zero `emulator-5554` interaction.
- **Physical ARM64 (Gate P1):** closed on a Galaxy S25 Ultra —
  `primaryCpuAbi=arm64-v8a`, `PAGE_SIZE` 4096, the mandatory ~10k/~50k/~100k
  ladder and Sculpt at 100k measured on real hardware. Stylus stays UNVERIFIED.
- **Runtime:** the UI-R1C2 walkthrough is summarised in the chapter at the top of
  this file. Earlier walkthroughs are in Git history.

**One caveat about capturing self-test evidence.** On both the emulator and the
physical phone the logcat ring buffer intermittently drops whole suites from the
*middle* of a startup capture, which reads exactly like a suite that never ran.
The reliable signal is that the suites which do appear always report their full
expected check counts and `_SELFTEST_FAIL` is always absent; read a partial
capture as "no failures observed", and re-run until one capture is complete before
quoting a total.

## Display-control research, and what was rejected

Motion research for the display popover used Mobbin Pro against creative and
canvas applications on iOS. **Adopted:** the popover grows from the control that
opened it rather than sliding in from a screen edge (Craft, Apple Mail,
Freeform); selection feedback happens **in place**, so the surface does not move,
re-animate or close on a choice — the one that matters, because comparing two
options means switching repeatedly; non-modal, staying open across several
changes; and grouped labelled chip rows rather than one long list (eBay,
Photoroom), adopted as layout and not as styling.

**Rejected:** live preview thumbnails with a check badge per option (Photoroom) —
a rendered thumbnail per shading mode needs a second offscreen render target, for
a choice whose result is already filling the screen behind the panel; and a bottom
sheet with drag detents (Canva, Play, Unfold) — a sheet covers the model, which is
the thing being judged, and ForgeShape already has a Property Inspector for
sheet-shaped content.

Nothing decorative was taken. There is no continuous or looping model animation,
no motion on the viewport itself, and no animation anywhere on the path of a
pointer sample. Both animations are interruptible, and a zero system animator
duration scale skips them outright rather than shortening them.

## Known Issues / Blockers

- **INPUT-R1 has no on-device verification.** `ForgeShape_Stage006` failed to
  reach a ready state on two consecutive detached launches at port 5580 (the
  emulator process stayed alive but never came up on adb; the first attempt's
  process later died outright), and the owner elected to defer device testing
  rather than keep retrying. So the 59 new native checks, the 13 new
  instrumented cases and the INPUT-R1 runtime walkthrough are UNVERIFIED. The
  build, the packaged test APK and the JVM suite are green. This is a
  verification debt against the current tree — run
  `scripts\run-instrumented-tests.ps1 -Serial <serial>` on a ForgeShape-owned
  target and record the result before UI-R2 is accepted.
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
or a screen-space edge filter, and remains a separate decision. The §12
readability enhancement (outline or cavity) was **explicitly deferred**: both
candidates start the post-processing framework the shading stage was told not to
build, and the vertex-based alternative would expose triangle structure in
Smooth mode.

**Documentation size, and one file that is now urgent.** Every core document is
inside the 2000-line hard cap, but several are over their preferred targets.
**`ARCHITECTURE.md` sits at 1989 of 2000** after UI-R1C2 compacted fourteen of its
sections to pay for the ownership it had to gain — an 11-line margin, which is not
a working one. The next stage that touches it must compact **before** adding, and
the remaining content is dense ownership prose rather than narrative, so that is a
dedicated pass rather than another round of trimming. This file is over its
500–800 target at 1319 and should keep retiring closed-stage detail into Git
history. **Always re-measure before quoting a count** — the numbers recorded at
UI-R1C1 were stale in both directions.

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
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all five primitives + transform), the five `Construction*` generators, shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation including `validateCapsuleMeters`, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
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
| `app/src/main/cpp/forgeshape_*_selftest.{h,cpp}` | The ten debug-only deterministic suites: camera, picking, mesh, construction (box), transform, primitive, sphere, cone/capsule, sculpt brush kernel, render shading |
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

**UI-R2 — Workspace Composition Redesign**

The Editor Workspace has grown one control group at a time — Global Toolbar,
Tool Rail, Property Inspector, the Objects section, the display popover, the
theme control, the Grid chips — and each addition was judged against its own
stage rather than against the whole. UI-R2 is where the composition is decided
as one thing: what the workspace looks like when every group that exists today
has to share one window, at both breakpoints, in both appearances.

Nothing about the pointer boundary INPUT-R1 just built is UI-R2's to change.

**Before UI-R2 is accepted, INPUT-R1 still needs an on-device run.** The
implementation is complete and the JVM suite is green, but the instrumented
suite, the 59 new native checks and the runtime walkthrough have no device
result — see INPUT-R1 above. That is a verification debt against this tree, not
a design question.

**Still out:** Selection Outline — the expensive half of selection feedback,
needing either a second geometry pass or a screen-space edge filter and its own
decision; snap-to-grid and the Sketch grid, which are a different contract from
the world reference grid this stage shipped; a View Cube, camera focus or named
views; blur or glass of any kind; a post-processing framework; persistence; an
automatic system theme; hierarchy and object commands; Sketch/Extrude; Undo; and
import/export.
