# ForgeShape — Architecture

Current production architecture only. No roadmap, no history.

ForgeShape is a standalone Android application that owns its own viewport and
renderer. No game engine (Godot/GDExtension, Unity, Unreal) and no third-party
runtime, rendering, math or input library participates in the running product.

## Layer map

```
ForgeShapeActivity            Android lifecycle, edge-to-edge window
        |
        +-- EditorWorkspaceView   the whole editor UI: adaptive layout, window
        |        |                insets, chrome visibility, and the surfaces
        |        |                for the mode NATIVE code is in
        |        |  EditorUiState      drafts / presentation / layout ONLY
        |        |  WorkspaceLayoutMode window-dp breakpoints (no Android type)
        |        |
        |        +-- GlobalToolbarView   context, mode seam, status, Export (rsvd)
        |        +-- ToolRailView        sculpt brushes | Construction editors
        |        +-- BrushEdgeControlsView  direct Radius + Strength (Sculpt)
        |        +-- PropertyInspectorView  contextual, collapsible, scrolling
        |                 +-- ConstructionShapeEditorView   what the object IS
        |                 +-- ConstructionPlacementEditorView  where it SITS
        |                 +-- SculptContextView   mesh state + guarded re-Freeze
        |        |  LengthUnit    exact BigDecimal mm/cm/m <-> meters
        |        |
ForgeShapeSurfaceView         Surface lifecycle + raw pointer forwarding
        |                     (no camera state, no matrices, no Vulkan)
NativeViewport (JNI decls)
        |  JNI  -- Android/JNI types stop HERE --
forgeshape_jni.cpp            render thread, ANativeWindow ownership,
        |                     MotionEvent -> TouchAction translation,
        |                     Camera + Selection ownership + locking,
        |                     stroke-vs-navigation arbitration,
        |                     publishActiveRepresentation
        |
        +--> CameraController -> CameraSnapshot (view, proj, eye, target, pose)
        |
        +--> SelectionController -- on a valid tap --> pickScene() -> Picking
        |         |                                        ^  (ray build,
        |         v                                        |   ray/triangle)
        |    ObjectId or none -> bool "draw highlight"     | current snapshot
        |                                                  |
        |    ConstructionObject                 SculptSession
        |      ObjectId, PrimitiveKind,           ProductMode, SculptTool,
        |      the five Construction*             SculptStroke (one kernel)
        |      primitives,                          |
        |      ConstructionTransform                |
        |        |                                  |
        |        generateMesh() [LOCAL] --Freeze--> SculptMesh [LOCAL copy,
        |        |                                  own SculptRevision]
        |        |                                  |
        |        +---- the ACTIVE one is published -+
        |        v                                  |
        |    MeshStore (immutable revisions, monotonic, latest-wins)
        |        |                                  |
        |    modelMatrix() / inverseModelMatrix()   |
        v        v                                  v
      Renderer  (Vulkan, frame loop, upload; owns no geometry truth)
        |
   ANativeWindow -> VkSurfaceKHR -> swapchain
```

## Ownership

| Concern | Owner | Explicitly NOT an owner |
| --- | --- | --- |
| Activity lifecycle, edge-to-edge window | `ForgeShapeActivity` | — |
| Which surfaces are on screen, adaptive layout, window insets, chrome visibility | `EditorWorkspaceView` | it decides no mode — `syncFromNative()` *reads* `NativeViewport.productMode()` and builds from that |
| Display unit, the *draft* primitive kind, the Construction rail selection, inspector detent, chrome-hidden | `EditorUiState` | every field is safe to lose; none of them can change the model |
| The window-dp breakpoints and chrome sizing rules | `WorkspaceLayoutMode` | it holds no Android type and reads no state; it is arithmetic |
| Shape/transform field text and input validation messages | `ConstructionShapeEditorView`, `ConstructionPlacementEditorView` | neither owns a parameter, a kind, a transform, a mesh or a publish decision |
| Brush slider positions, which rail entry looks active, the sculpt mesh summary | `BrushEdgeControlsView`, `ToolRailView`, `SculptContextView` | none owns a vertex, a brush value, a tool or a mode; all four are read back from native state |
| The active product mode, the one Frozen Sculpt Mesh, the active tool, the brush and the live stroke | `SculptSession` (`forgeshape_sculpt.{h,cpp}`) | Java owns none of these; the renderer owns no sculpt truth |
| Frozen local vertex/index data and its `SculptRevision` | `SculptMesh` | it is NOT Construction truth and no parameter is ever read back out of it |
| 1-ring adjacency and incident triangles of the frozen mesh | `SculptTopology` | built once per Freeze, never per move; it is not a half-edge mesh and cannot change topology |
| Area-weighted vertex normals from current positions | `computeVertexNormals` + `SculptMesh`'s dirty-flagged cache | derived data, not truth; no brush computes its own |
| The stroke kernel: hit, affected set, falloff, radius resolution, lifecycle | `SculptStroke` | it restates no FOV or aspect — it reads the `CameraSnapshot`'s own matrices |
| What each tool does to the vertices it captured | one `SculptStroke::apply*` per `SculptTool` | there is no brush base class, registry or plugin surface |
| Whether a one-finger Down becomes a stroke or a navigation | `g_strokePending` in `forgeshape_jni.cpp`, using `SculptSession::hitsSculptMesh` | Java decides none of it; the probe cannot mutate the mesh |
| mm/cm/m ↔ meter conversion and number formatting | `LengthUnit` | the domain never sees a display unit |
| Identity, which primitive is active, every primitive's parameters, and placement | `ConstructionObject` (`forgeshape_construction.{h,cpp}`) | it is ONE object, not a registry, a list, a scene graph or a hierarchy |
| Each primitive's exact parameters and its deterministic **local**-space mesh | `ConstructionBox` (W/H/D), `ConstructionCylinder` and `ConstructionSphere` (diameter…), `ConstructionCone` (bottom diameter, height), `ConstructionCapsule` (diameter, **total** height) | no JNI/Android/Vulkan/renderer/UI types. Tessellation counts are fixed, not parameters. A radius, and the capsule's cylindrical middle, are derived and never stored. The cone's apex radius is zero by definition: no top diameter, no frustum. The mesh, the GPU and the picker own no parameter |
| The one radial-segment and latitude-stack count every round primitive is drawn with | `kPrimitiveRadialSegments` / `kPrimitiveLatitudeStacks` (`forgeshape_construction.h`) | no generator writes down a segment count of its own |
| The capsule's `totalHeight >= diameter` relation | `validateCapsuleMeters` | the UI restates none of it; it is a domain rule, not an input check |
| Which parameters belong to which primitive | `PrimitiveSpec`'s payload variant | no caller reads a primitive's numbers as another's; the kind is derived from the payload, not stored beside it |
| Authoritative primitive update **and** its mesh publication | `applyPrimitive` (`forgeshape_construction.{h,cpp}`) | JNI and Java restate none of this rule |
| Authoritative placement (double-meter position, double-degree rotation) and the derived model/inverse matrices | `ConstructionTransform` (`forgeshape_transform.{h,cpp}`) | no JNI/Android/Vulkan/renderer/UI types; it cannot publish a mesh because it cannot reach `MeshStore` |
| The axis and Euler convention | `forgeshape_transform.h` | the renderer and the picker define none of their own |
| World ray → local object ray | `transformRayToLocal` (`forgeshape_picking.{h,cpp}`) | no Vulkan state is consulted |
| Surface create/change/destroy | `ForgeShapeSurfaceView` | native code does not touch Android views |
| Raw pointer ids + coordinates | `ForgeShapeSurfaceView` | it interprets nothing |
| MotionEvent action decoding | `forgeshape_jni.cpp` | camera module never sees Android constants |
| Render thread, `ANativeWindow` | `forgeshape_jni.cpp` | renderer never creates/releases the window |
| Camera pose, gestures, projection | `CameraController` | renderer and Java own none of it |
| Platform-neutral touch event data | `forgeshape_input.h` | one shared type, not an input framework |
| Tap-vs-navigation decision, selected `ObjectId` | `SelectionController` | renderer and camera own no selection |
| Screen ray, ray/triangle, nearest hit | `forgeshape_picking.{h,cpp}` | no JNI/Android/Vulkan/renderer types |
| Current CPU mesh, revisions, validation | `MeshStore` / `RuntimeMesh` (`forgeshape_mesh.{h,cpp}`) | no JNI/Android/Vulkan types; owns no GPU resource |
| Baseline cube numbers, and the DEBUG mesh fixtures | `forgeshape_demo_mesh.{h,cpp}`, `forgeshape_mesh_fixtures.{h,cpp}` | test infrastructure, not product geometry, and not on any startup path |
| Vulkan, presentation, and every mesh buffer, staging and upload | `Renderer` | it owns no CPU mesh and mutates none, interprets no input and owns no identity |
| Vector/matrix math | `forgeshape_math.h` | no GLM or other third-party math |

## Platform boundary

Android is the **first production platform and the only one that exists**. There
is no Apple target, no Xcode project, no Metal backend, no MoltenVK and no
cross-platform UI framework, and none is authorized. What follows is a constraint
on how this codebase is arranged, not a claim about where it runs.

**The domain is platform-neutral C++ and the Android layer is an adapter over
it.**

| | rule |
| --- | --- |
| Domain code | Construction, geometry, sculpt, picking, camera and selection stay platform-neutral C++17. `forgeshape_camera`, `forgeshape_construction`, `forgeshape_transform`, `forgeshape_picking`, `forgeshape_selection`, `forgeshape_mesh` and `forgeshape_sculpt` contain no JNI, Android, Vulkan, renderer or UI type — that is asserted throughout the ownership table above and must stay asserted |
| Android types | `View`, `Activity`, `MotionEvent`, `Surface`, `jobject` and every other Android or JNI type may never become domain truth. They reach exactly as far as `forgeshape_jni.cpp` and stop |
| The Android UI | is a platform shell/adapter. It owns draft, presentation and layout state and nothing else, and reads authoritative state back from native code rather than assuming it |
| Input | crosses the boundary as **semantic, platform-neutral** data. `forgeshape_input.h`'s `TouchAction`/`TouchPointer` is that boundary today; it is the type that would grow pressure, tilt, hover and tool type, and the reason a pointer sample is translated out of Android's vocabulary in `forgeshape_jni.cpp` rather than carried inward |
| Platform services | future file, storage and system services get narrow boundaries of their own, for the same reason input has one |
| Renderer coupling | the renderer's dependency on a platform surface stays **explicit and local**: `forgeshape_jni.cpp` owns the `ANativeWindow` and hands it over, and `Renderer` never creates or releases one. That single visible seam is what a second backend would be added beside |

The practical test is one question: *if this file had to compile on a platform
that has no Android, what would break?* For everything below `forgeshape_jni.cpp`
the answer must stay "nothing".

This is deliberately **not** an abstraction layer. No renderer interface, no
platform façade, no `#ifdef` for an operating system that has no target — a
portability layer built before there is a second platform is a guess, and a wrong
one is more expensive than the port. What is required is only that the seams stay
where they are and that nothing new crosses them.

## Android layer

`ForgeShapeSurfaceView` is a plain `android.view.SurfaceView` (no Compose, no
AndroidX). Its whole contribution to navigation is `onTouchEvent`, which copies
the masked action, the id of any lifting pointer, and each pointer's stable id
and view-local x/y into preallocated arrays, then makes one JNI call.
`GestureDetector` / `ScaleGestureDetector` are deliberately unused: they would
move camera semantics into the Android layer.

### Editor Workspace composition

The Activity's content view is `EditorWorkspaceView`, a `FrameLayout` with three
children in z-order:

1. the `SurfaceView`, at the **whole window size**;
2. `chromeRoot`, a transparent, non-clickable vertical `LinearLayout` holding
   every interactive surface;
3. `overlayRoot`, holding only the restore chip that survives chrome being
   hidden.

Inside `chromeRoot`: `GlobalToolbarView` at the top, then a weighted horizontal
row carrying `BrushEdgeControlsView` (leading edge, Sculpt only), a weighted gap
where the model lives, and the `ToolRailView` inside a `ScrollView` (trailing
edge). `PropertyInspectorView` is placed either after that row (bottom sheet) or
inside it (side placement) — see below. All are plain framework views built in
code from `res/values` resources; no Compose, no AndroidX in the product, no
design system, no drawer.

**The Vulkan viewport is full-bleed and stays that way.** No layout decision
insets, pads or resizes the `SurfaceView`; window insets are applied to
`chromeRoot` and `overlayRoot` only. Nothing in the Android layer can therefore
cause a swapchain rebuild, and renderer ownership of the surface is untouched.

Which surfaces exist is decided by **native state**: `syncFromNative()` reads
`NativeViewport.productMode()`, the sculpt state and the active tool, and builds
from that — never from what was last tapped. In Sculpt Mode the shape and
transform editors are therefore not merely disabled but **absent**, and in
Construction the brush controls are absent, so nothing on screen can edit the
representation that is not being worked on.

Every chrome surface swallows every touch inside its bounds that none of its own
controls takes (`onTouchEvent` returns `true`). Because the viewport is a
sibling *below* them, and Android never offers a consumed event to a sibling
underneath, a touch on chrome provably cannot orbit the camera or — in Sculpt
Mode — deform the model while reaching for a slider. In the other direction,
`ACTION_DOWN` on the viewport pulls focus and the soft keyboard away from any
field being edited, so navigation never happens "through" a focused editor.

### Adaptive layout and window insets

`WorkspaceLayoutMode` is the whole adaptive decision, as arithmetic on **window**
dp — never display size and never orientation, so a rotation, a split-window
resize and a free-form drag all take one path. It holds no Android type and is
unit-tested on the JVM.

| window | class | inspector |
| --- | --- | --- |
| width < 600 dp | `COMPACT` | bottom sheet, capped at 30 % of window height |
| 600–839 dp, or any width with height < 480 dp | `MEDIUM` | bottom sheet, or **side overlay** when height < 480 dp |
| ≥ 840 dp wide **and** ≥ 480 dp tall | `EXPANDED` | docked side panel, ≤ 28 % of width |

The height gate is what fixes the landscape failure, twice over: a short window
never gets a bottom sheet, and a 914 × 411 dp phone in landscape is not
classified as a tablet merely because it is wide. The inspector's body always
scrolls, its bottom-sheet height is capped in `onMeasure`, and a side placement
narrows to its toggle when collapsed — a panel that hid only its body would give
the model back nothing.

The decision runs at the top of `EditorWorkspaceView.onMeasure`, not in
`onSizeChanged`: a surface added or re-parented during the layout pass is
measured against the previous pass and laid out at zero height. It is idempotent,
so it converges within one traversal. `configChanges` is kept and widened with
`smallestScreenSize` so no window change destroys the Vulkan surface;
`onConfigurationChanged` discards the cached window and re-runs the decision.

The app is edge-to-edge (`Theme.ForgeShape`, `setDecorFitsSystemWindows(false)`),
replacing the deprecated fullscreen theme that merely hid the system bars.
`setOnApplyWindowInsetsListener` applies `systemBars | displayCutout` — and the
`ime()` inset, which replaces rather than adds to the navigation bar — as padding
to the chrome containers. `windowSoftInputMode` is `adjustResize`: with
decor-fits off the window is **not** resized, so the keyboard arrives as an inset
the chrome absorbs and the surface is untouched.

### Property Inspector ownership boundary

`EditorUiState` is the closed list of what the UI may remember: display unit,
draft primitive kind, which Construction editor the rail points at, inspector
detent per mode, and chrome-hidden. Every field is safe to lose — kill the
process and the object is exactly what it was. Anything that would change the
model if it were wrong belongs in native code instead.

The primitive chooser is a **draft**: it swaps which parameter fields are on
screen and nothing else. The object's kind changes only when Apply Shape reads
the drafted primitive's own fields and calls **that primitive's own native
method**, so there is no window in which the object is a cylinder carrying box
dimensions. `refreshFromNative` resets the draft to the object's real kind, so
the chooser can never be left claiming a shape the object is not. Exactly one
parameter row is on screen at a time and it is always the drafted kind's; every
primitive's fields are kept populated and converted, including the hidden ones,
so an inactive draft does not silently change meaning while it is off screen.

Shape and placement live in **separate inspector bodies with separate Apply
buttons** — *Apply Shape* and *Apply Transform* — because they are separate
truths with different consequences: one republishes the mesh, the other cannot.
One button doing both would hide that. The Construction Tool Rail chooses which
body is on screen; that choice is UI layout state, makes no native call, and
deliberately does not refresh the editors, so a half-typed value in the other
section survives.

Consequently:

- typing changes no geometry and no placement, publishes no revision and uploads
  nothing;
- switching display units makes **no native call at all** — it is a decimal point
  shift on text, applied to the size and position fields and never to rotation,
  which is degrees in every unit;
- the UI never clamps, rounds or repairs an invalid value. It refuses input it
  can already name a problem with (blank, non-numeric, and non-positive *for a
  dimension only*) so it can report a useful message, and everything else goes to
  native validation, which is the final authority.

The positivity rule is deliberately not applied to placement: zero and negative
are ordinary for a coordinate and an angle, so the only thing the UI refuses
there is text that is not a number.

`LengthUnit` performs every conversion as an exact `BigDecimal` point shift
(`10^3` mm, `10^2` cm, `10^0` m), never a floating-point multiply, so repeated
unit switching is lossless and a value re-entered in a different unit produces
the identical `double` — which is why re-applying the same box in a different
unit reports `Unchanged`. Meter values are rendered through
`BigDecimal.valueOf(double)` (shortest round-tripping decimal), shifted, and
printed with trailing zeros stripped and no grouping separator. Parsing accepts
`.` or `,` as the decimal separator; the fields use a numeric IME via
`setRawInputType` plus a `DigitsKeyListener` accepting `0123456789.,-`. The minus
matters twice over: a negative coordinate or angle is an ordinary value that must
be enterable, and a negative *dimension* must be enterable so it can be visibly
refused rather than being unreachable.

### The destructive-act guard

Three mode transitions live in the Global Toolbar and exactly one is on screen:
*Freeze to Sculpt* while no Frozen Sculpt Mesh exists, *Resume Sculpt* once one
does, *Back to Construction* while sculpting. None is guarded, and none needs to
be: the first can discard nothing, and the second and third discard nothing.

The one irreversible act in the product is **re-Freeze**, which rebuilds the
sculpt mesh from the current Construction shape and throws away what was
sculpted into the old one. It lives in the Sculpt inspector as *Freeze again…*
and it confirms **only when the native stroke count is non-zero** — re-freezing a
mesh no stroke has touched replaces a copy with an identical copy, and confirming
that would train the user to dismiss the dialog that matters. Cancel makes no
native call at all.

### Android UI verification boundary

The native self-tests own geometry and math; the runtime smoke owns the
Vulkan/input bridge; the Android suites own shell behaviour, control visibility
and the UI→native call contract. **No Java test asserts a rendered pixel.**

- `src/test` (JVM, JUnit only): `WorkspaceLayoutMode`, `EditorUiState` and
  `LengthUnit` — the three pieces deliberately free of Android types.
- `src/androidTest` (instrumentation): every control is reached by its stable
  semantic id from `res/values/ids.xml`; no assertion depends on a screen
  coordinate. Espresso is deliberately absent — the assertions are view state,
  measured geometry and touch consumption, read directly from the view tree.

Two techniques carry most of the weight. "A UI action changed nothing" is
asserted by comparing a **bit-identical** snapshot of the native primitive,
transform and sculpt arrays across the action. "Chrome does not leak a gesture"
is asserted by dispatching a drag to the surface and requiring it to return
`true`, which is the guarantee, plus the native sculpt revision and stroke count
being unchanged. The camera has no read-back across JNI, so camera immobility is
proven at runtime instead, by a pixel-identical viewport region across a chrome
drag.

## JNI boundary

Five lifecycle methods, one input method, eight Construction methods and eight
sculpt methods on `NativeViewport`:

```
start()
surfaceCreated(Surface)
surfaceChanged(int width, int height)
surfaceDestroyed()          // blocks until ANativeWindow is released
stop()
touchEvent(int action, int actionPointerId, int pointerCount,
           int[] ids, float[] xs, float[] ys, int viewWidth, int viewHeight)

constructionPrimitive(double[] outState)                   // read shape
applyConstructionBox(double widthMeters,                   // submit a box
                     double heightMeters, double depthMeters)
applyConstructionCylinder(double diameterMeters, double heightMeters)
applyConstructionSphere(double diameterMeters)
applyConstructionCone(double bottomDiameterMeters, double heightMeters)
applyConstructionCapsule(double diameterMeters, double totalHeightMeters)

boxTransform(double[] outPositionRotation)                 // read placement
applyBoxTransform(double px, double py, double pz,         // submit placement
                  double rx, double ry, double rz)

productMode()                                  // 0 = Construction, 1 = Sculpt
freezeToSculpt()                               // copy the local mesh, enter Sculpt
enterConstructionMode()                        // keep the sculpt mesh, show the source
enterSculptMode()                              // resume WITHOUT re-freezing
sculptState(double[] outState)                 // read mode/mesh/tool/brush state
setSculptBrush(double radiusPixels, double strength)   // shared by every tool
setSculptTool(int tool)                        // request a tool, be told the active one
sculptTool()                                   // 0 grab, 1 clay, 2 smooth, 3 inflate
```

Reading and writing mirror each other: read the whole section, or submit the
whole section. The shape read reports **every** primitive's parameters plus the
active kind, so the panel can populate an inactive draft without inventing
defaults; each slot of that array has one fixed meaning whatever the active kind
is, so nothing there is positional-by-kind either.

**The shape write is one method per primitive.** There is deliberately no generic
`kind + a + b + c` entry point: a signature whose third double means "depth, or
nothing, depending on an int" is a boundary that only documentation can keep
correct. Each method's parameter list *is* that primitive's parameter list, so
Java cannot ask for a cylinder while sending box dimensions, cannot put a height
where a depth belongs, and has no spare number to get wrong when asking for a
sphere.

Every length crosses in meters and every angle in degrees, so no display unit and
no radian ever reaches native code. Writing carries a whole section in one call
and returns a status code (`APPLY_APPLIED`, `APPLY_UNCHANGED`, or one of the
`APPLY_REJECTED_*` reasons). There is no `setWidth`, no `setRotationX`, no
`setKind`, no separate `publish`, and no way for Java to observe or produce a
half-applied state. Nothing is ever measured back from the mesh, from GPU data or
from a model matrix.

The sculpt methods follow the same shape: Java may **request** a mode or a tool
and is then told what is actually active, and it reads the brush back rather than
assuming its slider mapping was honoured. There is no method that sets a vertex,
no method that starts a stroke and no method that carries geometry across the
boundary in either direction — a stroke is driven entirely by the existing
`touchEvent` path, and the geometry it produces reaches the GPU through
`MeshStore` exactly as every other revision does. In particular there is no
method for the arbitration: whether a Down becomes a stroke is decided below the
boundary, where the mesh actually is.

Below the boundary all five shape methods build the matching typed
`PrimitiveSpec` and go straight into `applyConstructionPrimitive`, so the
per-kind split is a boundary shape only: the update-then-publish rule still has
exactly one implementation. The JNI layer adds logging and the status code and
nothing else — it does not decide validity, does not decide whether to publish,
and never touches `MeshStore` or the transform. The DEBUG primitive driver calls
the same function.

The transform path reuses the same `APPLY_*` vocabulary.
`APPLY_REJECTED_NOT_POSITIVE` simply cannot occur there, because zero and
negative are ordinary coordinates and angles.

One JNI call carries one complete `MotionEvent`, never one call per pointer.
`forgeshape_jni.cpp` translates Android's masked action constants into
`forgeshape::TouchAction`; unrecognised actions (hover, scroll, button) are
dropped rather than forwarded.

## Camera ownership

`CameraController` (`forgeshape_camera.{h,cpp}`) is the single source of camera
truth and contains no JNI, Android or Vulkan types. It owns `target` (orbit
centre), `yaw`, `pitch`, `distance`; FOV, near and far planes; viewport
width/height and therefore projection aspect; and the gesture state machine
(mode, tracked pointer ids, anchors). It produces a `CameraSnapshot` — view
matrix, projection matrix, eye, target and pose scalars — which is the only thing
the renderer ever sees.

Gesture rule: every touch event recomputes the set of pointers that are still
down, sorted by pointer id. If that set differs from the tracked one, the
controller re-anchors and applies **no** delta; deltas are applied only on Move
events whose pointer set is unchanged. This is what makes 1↔2 pointer transitions
and MotionEvent index reordering jump-free.

## Canonical winding and culling

A triangle's vertices are ordered **counter-clockwise when the triangle is viewed
from outside the surface**, in right-handed world space. Equivalently the
geometric normal `N = (v1 - v0) x (v2 - v0)` points away from the solid, and a
triangle is front-facing to a ray when `dot(N, rayDirection) < 0`.

The projection in `forgeshape_math.h` flips Y for Vulkan clip space. That flip
lives **in the matrix** and is already applied by the time Vulkan classifies a
triangle, so it must not be compensated for a second time in the pipeline enum.
The pipeline uses

```
cullMode  = VK_CULL_MODE_BACK_BIT
frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE
```

CPU picking accepts front faces only, under the same rule, so what can be picked
is exactly what the rasterizer draws. The self-tests assert the convention
triangle by triangle (`NOR-01`), assert that render normals and the model→view
normal transform stay outward (`NOR-02`..`NOR-09`), and assert that a ray fired
from inside a closed solid misses under front-face-only picking; that is what
keeps the data, the rasterizer and the picker from drifting apart.

**Naming this `VK_FRONT_FACE_CLOCKWISE` double-counts the Y flip** and inverts
culling. It does not blank the viewport — a closed solid still fills exactly the
same silhouette — it draws the solid's far walls instead of its near ones, so
every convex primitive renders as the inside of itself and reads as a concave
interior corner. It also silently splits the renderer from the picker, which
stays correct. Stage 015C-R fixed exactly this; the measurement is in
`PROJECT_STATUS.md`.

## Construction domain

`forgeshape_construction.{h,cpp}` owns the product geometry. It contains no JNI,
Android, Vulkan, renderer or UI type, and it holds no GPU resource.

### The one active object

`ConstructionObject` is the single owner of everything that makes the product's
object what it is:

- a stable `ObjectId`;
- a `PrimitiveKind` — `Box`, `Cylinder`, `Sphere`, `Cone` or `Capsule` — saying
  which primitive is **active**;
- a `ConstructionBox`, a `ConstructionCylinder`, a `ConstructionSphere`, a
  `ConstructionCone` and a `ConstructionCapsule`, each holding its own exact
  parameters;
- a `ConstructionTransform` holding its placement.

There is exactly one of these. It is emphatically **not** a registry: no
container, no list, no parent, no child, no create and no delete, and the
process-scoped `constructionObject()` is the only instance the product has.

`constructionTransform()` returns `constructionObject().transform()` rather than
owning a singleton of its own, so identity, shape and placement are one object's
state and cannot drift apart.

**Every** primitive's parameters are retained across a kind change. Switching
Box → Cylinder → Sphere → Box does not silently forget the box's dimensions; only
`kind` decides which set is authoritative for the geometry right now. That is
also what lets the panel show an inactive primitive's remembered values without
inventing defaults of its own.

### The typed primitive payload

A primitive *request* is a `PrimitiveSpec`, and its payload is a
`std::variant<BoxDimensionsMeters, CylinderDimensionsMeters,
SphereDimensionsMeters, ConeDimensionsMeters, CapsuleDimensionsMeters>` — a real
tagged union, not a struct carrying all five groups at once. A spec built for a
cylinder physically does not contain box or sphere values, so there is no
"inactive parameter group" riding along beside the active one and nothing for a
caller to read by mistake.

This matters more with five primitives than it did with three: a cone's
`(diameter, height)` and a capsule's `(diameter, totalHeight)` are the same two
numbers in the same order and mean different things, and the payload is what
makes reading one as the other impossible rather than merely discouraged.

`kind()` is **derived** from the payload's alternative index rather than stored
next to it, so the tag and the data cannot disagree; a `static_assert` pins the
variant's alternative order to `PrimitiveKind`'s enumerator order. Access is
typed — `box()`, `cylinder()`, `sphere()`, `cone()` and `capsule()` each return a
pointer that is null unless the spec really is that primitive — and specs are
built only through the matching `forBox` … `forCapsule`, so a request always says
what it is by construction and reading the wrong one is a null check away rather
than silent nonsense. `ConstructionObject::spec()` therefore returns only the
**active** primitive's parameters; the remembered parameters of the inactive ones
are reachable only by asking for that primitive by name, which is exactly what
the panel's draft display does and nothing else needs.

### Source-of-truth hierarchy

```
ConstructionObject           kind + that kind's parameters
    |                        (double meters — the ONLY authoritative dimensions)
    -> generateMesh()        (derived float RuntimeMesh data, LOCAL space)
        -> MeshStore         (immutable revision)
            -> CPU picking + Renderer
                -> Vulkan buffers

ConstructionTransform        (double-meter position, double-degree rotation)
    -> modelMatrix() / inverseModelMatrix()   -> Renderer / picking
```

These directions are the only ones that exist. Nothing infers a parameter from
mesh vertices, from a bounding box, from a model matrix or from GPU data; the
parameters are read, the mesh is regenerated, and the mesh is republished.

The two branches are independent, and that independence is the point: a shape
change republishes the mesh and leaves the placement alone, while a placement
change changes one 4×4 matrix and publishes nothing.

### Unit contract

The internal Construction length unit is the **meter**, carried as `double` and
named `...Meters` at every boundary (`Meters`, `widthMeters()`,
`diameterMeters()`, `BoxDimensionsMeters`, `CylinderDimensionsMeters`).
`RuntimeMesh` positions are **derived `float`** data.

Curved primitives are authored by **diameter**, because that is what a drawing
and a caliper give you. `radiusMeters()` exists but is derived on demand and
never stored — as is the capsule's cylindrical middle.

mm/cm/m exist **only** above the JNI boundary, in `LengthUnit` and the two
Construction inspector bodies. Every value crossing into native code is already in
meters, and no native type, function or log line names a display unit. The unit a
dimension was typed in is therefore unrecoverable from domain state — which is
correct: it is not part of what the object is.

### Primitive representation

`N = kPrimitiveRadialSegments` (32) and `S = kPrimitiveLatitudeStacks` (16) are
the one owner of how finely every round primitive is divided; each generator
names those constants rather than writing down a count of its own, so a capsule's
hemispherical end is provably the same tessellation as a sphere's —
`kCapsuleHemisphereBands` is literally `S / 2`. `R` below is the capsule's ring
count, `S`.

| Primitive | Authoritative parameters | Axis and origin | Tessellation | Vertices : indices |
| --- | --- | --- | --- | --- |
| Box | width, height, depth | axis-aligned, centred on local origin | none — a fixed index table | 8 : 36 |
| Cylinder | diameter, height | axis local +Y, centred | `N` radial segments | `2N+2` = 66 : `12N` = 384 |
| Sphere | diameter | centred | `N` meridians × `S` stacks | `(S-1)N+2` = 482 : `2N(S-1)`·3 = 2880 |
| Cone | bottom diameter, height | axis local +Y, base at `-h/2`, apex at `+h/2` | `N` radial segments | `N+2` = 34 : `6N` = 192 |
| Capsule | diameter, **total** height | axis local +Y, centred | `N` meridians × `R` rings | `RN+2` = 514 : `6NR` = 3072 |

Invariants every generator shares:

- **Bounds are exact, not approximate.** The four cardinal directions (and the
  sphere's and capsule's equator/seam rings) are **written down rather than
  computed**: `cos(pi/2)` in floating point is about 6.1e-17, not 0, and that
  error would make the generated bounds not quite the radius. `N` is divisible by
  four so those directions always land on real vertices, and `S` is even so
  exactly one ring falls **on** the equator plane rather than straddling it. The
  two exact factors are multiplied in `double` before the single conversion to
  `float`. The self-tests assert exact equality on all six bounds, not a
  tolerance, across shapes as extreme as 40 m × 0.01 m.
- **Winding is the canonical convention above** — side normals radially outward,
  cap normals along the axis — so sides and caps alike survive back-face culling,
  and closure is asserted directly (every directed edge exactly once with its
  reverse present, and `V - E + F = 2`, which fails on a hole, a duplicated
  triangle or a triangle wound the wrong way round). **No degenerate triangles**:
  a pole or an apex is one vertex fanned to its adjacent ring, never a collapsed
  ring, because the collapsed form produces `N` zero-area triangles that are
  invisible but real and noise in every winding, area and validation check.
- **Generation is a pure deterministic function of the parameters**, so identical
  parameters always produce identical vertices.
- **Tessellation is fixed and not exposed.** It is a rendering detail of an exact
  shape, not a property of it; exposing it would put the approximation into
  authored state, where it would have to be persisted, versioned and validated
  like a real parameter. Between two ring vertices the surface is a flat facet,
  so a picked point on a curved side lies between `radius * cos(pi/N)` and
  `radius` from the axis (`radius * cos(pi/N)^2` on a sphere). That is the
  tessellation showing through; the *parameters* stay exact, and no dimension is
  ever reconstructed from mesh floats.
- **Vertex colour is derived presentation data**, not Construction truth: it
  exists because `RuntimeMesh` carries a colour channel and a gradient makes
  orientation readable, and no dimension is ever recovered from it. `RuntimeMesh`
  carries **no normals** and the renderer does no lighting, so per-primitive
  shading hardness is not expressible today.

Three cases need more than the table says:

**The cone's base is closed** and its apex is a true point. The apex radius is
zero by definition and is not a parameter: no top diameter and no frustum,
because a truncated cone is a different shape rather than a cone with an extra
number. Concretely the side triangle is the cylinder's `(b0, t1, b1)` with the
top ring collapsed to the apex; its `(b0, t0, t1)` partner is the one that would
have been degenerate, so it simply does not exist. The base fan is the cylinder's
bottom cap unchanged, and `V - E + F = 2` is what would fail if the base were
left open.

**The capsule's cylindrical middle is not a special case in the index loop.**
Rings run from the north pole downward; the first `kCapsuleHemisphereBands` are
the top hemisphere and the rest the bottom, and the middle is simply the band
between the two seam rings, generated by the same code as every other band. It is
centred on the origin, so its ends sit at `±(totalHeight-diameter)/2`, and the
poles are written down from the authoritative **total** height rather than
accumulated as `middle/2 + radius`.

**The capsule equality case is a sphere, and is generated as one.**
`totalHeight == diameter` is valid and describes a capsule with no middle: the
two seam rings are **the same ring**, `R` is one smaller, that band does not
exist, and the result is exactly the sphere's 482 : 2880. That is what keeps the
degenerate case free of a duplicated zero-length ring and of zero-area triangles,
and a `static_assert` pins it rather than leaving it to coincidence. Capsule
topology is therefore a deterministic *function* of the parameters rather than a
constant — the two sides of one exact `double` comparison with nothing in
between — which also makes the capsule the one primitive whose edit can change a
vertex count and so trigger a buffer growth.

### The capsule relation, and float resolvability

The capsule is the only primitive whose two parameters are **related** rather
than independent, so it is the only one with validation beyond the per-length
rule. `validateCapsuleMeters` owns both halves:

- `totalHeight >= diameter`, because the two hemispherical ends alone are already
  `diameter` tall. Violating it reports `DimensionValidation::RelationInvalid`, a
  reason distinct from `NotPositive` because "0.5 m is not a length" and "0.5 m is
  too short to be this capsule's total height" are different problems and deserve
  different messages. It surfaces at the JNI boundary as
  `APPLY_REJECTED_RELATION`.
- **float resolvability**: the generated positions must not merely be finite,
  they must be far enough apart to describe the shape. A 1e30 m capsule 1e-6 m
  across is finite in every coordinate and yet its whole hemisphere rounds to a
  single float, which would be a mesh of zero-area triangles. The check compares
  the pole against the first ring below it — the smallest latitude step anywhere
  on a hemisphere, so resolving it resolves all of them — and refuses the request
  as `NotRepresentable`. This is the one validation coupled to the tessellation
  constant, deliberately: the question it asks is "can this shape be built at this
  fidelity".

The UI restates neither half. It refuses only what it can name from the text
alone (blank, non-numeric, non-positive), and the relation is reported back to it
as an ordinary rejection.

### Update and invalid-update behaviour

`ConstructionObject::setPrimitive` reports `Applied`, `Unchanged` or `Rejected`:

- **Rejected** — a parameter that is not finite, not positive, whose derived
  `float` half-extent would be zero or non-finite, or that violates the capsule
  relation. This **fails closed**: every value the request carries is validated
  before anything at all is written, so a bad depth cannot leave a half-applied
  width behind *and a bad diameter cannot leave the kind switched*. The previous
  kind, parameters, transform and mesh revision all stand. The only limits are
  physical validity and float representability — there is no arbitrary product
  size policy.
- **Unchanged** — the request is valid, the kind matches and every relevant
  parameter matches. Nothing is written and no revision is published, so a
  redundant edit cannot cost a mesh revision or a GPU upload.
- **Applied** — the kind and/or the parameters changed, and a new mesh revision
  is published.

A **kind change is always a change**, even when the target primitive already held
exactly those parameters: the object is a different shape afterwards.

### The one apply entry point

`applyPrimitive(object, store, spec)` is the single place where a shape change
becomes a mesh revision. It validates, updates and publishes, and returns a
`PrimitiveApplyResult` carrying the status, the rejection reason, the
authoritative spec after the call, the published vertex/index counts, the store's
resulting revision and whether anything was actually published.

`PrimitiveSpec` carries the typed payload described above, so `applyPrimitive`
never branches on a kind field that could disagree with the numbers beside it: it
validates and compares whichever parameters the request actually contains.

Callers therefore decide nothing. The product UI path (for all five primitives)
and the DEBUG driver all go through it, so there is exactly one implementation of
the rule in the process. `publishConstructionObject` remains separately callable
for the startup publish, which changes no parameter.

The atomicity that matters is preserved by construction: nothing is written
unless every relevant value is valid, and no caller can observe a changed shape
without the matching published revision.

**The transform is never read, written or reset by this call**, in any outcome.
That is what makes "change the shape, keep the placement" true by construction
rather than by remembering to preserve it.

The `ObjectId` is fixed at construction and is independent of the kind, the
parameters and the mesh, so no shape change, primitive change, mesh revision or
buffer reallocation can alter what is selected. Turning the box into a cylinder
does not create a new object; it changes what this object is.

## Construction transform

`forgeshape_transform.{h,cpp}` owns **where** the object is; the primitive
parameters own **what** it is. They are separate truths on purpose, and neither
reads the other. Both are held by the same `ConstructionObject`, so they cannot
become inconsistent about which object they describe.

```
active primitive parameters (double meters) -> LOCAL geometry -> MeshStore -> GPU
ConstructionTransform P (double meters)     -> modelMatrix()  -> Renderer
                      R (double degrees)    -> inverseModel() -> picking
```

The published `RuntimeMesh` is the object's geometry in **object space**. The
transform never touches it. That is the whole point: moving or rotating the
object changes one derived 4×4 matrix and nothing else — no vertex is rewritten,
no `MeshRevision` is published and no GPU upload happens. `ConstructionTransform`
could not publish one if it wanted to; it has no access to `MeshStore`. The
independence runs both ways: a shape change republishes the mesh and leaves the
placement exactly as it was, including across any switch among the five kinds.

### Unit contract

Position is `double` **meters**, rotation is `double` **degrees**, named
`...Meters` and `...Degrees` at every boundary. Degrees are authoritative because
degrees are what the product exposes; radians exist only inside the derived
trigonometry. mm/cm/m applies to position exactly as it applies to a dimension,
and never to rotation.

### Axis and Euler convention

Right-handed world space, **+Y up**, column-vector math (`p' = M * p`), storage
column-major. A positive angle rotates by the **right-hand rule**. Rotations
compose in **local X → Y → Z** order, which for column vectors is written right
to left:

```
Model    = T * Rz * Ry * Rx
Model^-1 = Rx(-x) * Ry(-y) * Rz(-z) * T(-p)
```

so `Rx` acts on the object first and the translation last — a rotated object at
(2, 0, 0) is still centred at (2, 0, 0), because the rotation does not turn its
own translation.

There is exactly one convention in ForgeShape. The renderer consumes
`modelMatrix()` and the picker consumes `inverseModelMatrix()`; neither builds a
rotation itself, so they cannot drift apart. The inverse is composed from the
authoritative values rather than inverted numerically, which is exact for a rigid
transform and cannot disagree with the model it undoes.

The self-tests assert the convention directly: `Rx(+90)` takes +Y to +Z,
`Ry(+90)` takes +Z to +X, `Rz(+90)` takes +X to +Y, the X-then-Y composition
lands where only the documented order puts it, and `Model * Model⁻¹` is the
identity in both directions.

### Rotation values are not canonicalized

370° stays 370° in domain state and −90° stays −90°. Reduction modulo 360 happens
only inside the derived trigonometry, so what the user typed is what comes back
out and re-applying the same numbers reliably reports `Unchanged`. The self-tests
assert both halves: the stored value is untouched, and 370° and 10° produce the
same derived matrix.

### Validation

A transform value is refused only when it is not finite, or when it could not
survive into the derived `float` matrix. Zero and negative are **ordinary** for
all six: a coordinate is a place and an angle is a direction, neither is a size.
There is no arbitrary product limit.

`applyBoxTransform` (`applyTransformValues` / `applyConstructionTransform`) is the
one entry point. It fails closed: all six are validated before any is written, so
a bad rotation cannot leave a half-applied position behind. It reports `Applied`,
`Unchanged` or `Rejected` exactly as the dimension path does.

### Picking a transformed object

The ray moves, not the mesh. `transformRayToLocal` carries the world ray into
object space with the inverse model, and the unchanged local `RuntimeMesh` is
intersected there — the same vertices the GPU already holds. The hit point is
carried back to world space for reporting; the distance needs no conversion,
because a rigid transform preserves length. The transform has no reflection, so
winding is unchanged and front-face-only picking means the same thing in either
space.

This is what keeps "what is drawn" and "what is pickable" identical under a
transform, with no second collision representation to keep in sync.

## Sculpt domain

`forgeshape_sculpt.{h,cpp}` owns the second representation of the one object and
the four tools that edit it. Like the Construction domain it contains no JNI,
Android, Vulkan, renderer or UI type, and it holds no GPU resource.

### Two representations, one object

```
Construction Source                     Frozen Sculpt Mesh
  ObjectId                                the SAME ObjectId
  PrimitiveKind + that kind's parameters  a COPY of the local vertices/indices
  ConstructionTransform                   its own SculptRevision
  -> generateMesh()  [LOCAL space]  --Freeze-->  SculptMesh
```

These are separate truths and neither writes to the other:

- **Freeze copies.** `SculptMesh::freezeFrom` takes the Construction object's
  currently generated local mesh wholesale. Nothing is shared, so no sculpt edit
  can reach back into Construction data, and the Construction Source can still
  regenerate its own mesh at any moment.
- **A sculpt edit changes no parameter, no kind and no transform.** It cannot:
  the sculpt module has no mutable access to `ConstructionObject`, and the only
  mutation `SculptMesh` offers is a single vertex POSITION. No API here can add,
  remove or reorder a vertex or touch an index, so topology is preserved by
  construction rather than by discipline. **Nothing reconstructs a parameter from
  sculpt vertices** either; that direction does not exist, exactly as it does not
  exist from the Construction mesh.
- The transform is deliberately **not** duplicated. Both representations are
  local geometry under the one `ConstructionTransform`, so the object sits in
  exactly the same place in either mode and there is no second placement to keep
  in sync.
- `SculptRevision` is emphatically **not** a `MeshRevision`. `MeshStore`'s
  revisions count every published snapshot of whichever representation is active;
  a `SculptRevision` counts changes to *this* mesh and restarts at 1 on every
  Freeze. Neither is derived from the other and they cannot be compared.

### Mode ownership

`SculptSession` owns `ProductMode` — `Construction` or `Sculpt` — plus the one
`SculptMesh`, the active `SculptTool`, the brush settings and the live stroke. It
is process-scoped, like the camera, the selection, the mesh store and the
Construction object.

The mode is **native state**. The Android UI may request a change through
`freezeToSculpt` / `enterSculptMode` / `enterConstructionMode` and is then told
what the mode actually is; `EditorWorkspaceView.syncFromNative()` reads
`productMode()` back rather than assuming its request succeeded, so a refused
request (entering Sculpt with nothing frozen) cannot leave surfaces on screen
that lie about what is being edited. The active tool is read back the same way.

- **Freeze to Sculpt** snapshots the current Construction local mesh, creates the
  Frozen Sculpt Mesh with the same `ObjectId`, and enters Sculpt mode.
- **Back to Construction** keeps the sculpt mesh untouched and republishes the
  Construction Source's own generated mesh, so the original object comes back
  exactly as it was.
- **Resume Sculpt** re-enters Sculpt **without** re-freezing, so prior
  deformation returns. Freezing again is a separate button precisely because it
  discards the sculpted vertices; collapsing the two into one control would make
  which of those happens depend on hidden state.

### Stale-source policy

When the Construction Source changes while a Frozen Sculpt Mesh exists, the
sculpt mesh is **never** silently replaced or re-derived. It is marked
`sourceStale`, the Sculpt panel says so in as many words, and adopting the new
source stays an explicit user act — another Freeze, which is the only thing that
clears the flag. There is no automatic sculpt-edit transfer, and there will not
be one until a stage pays for what it would cost.

### Active representation

Exactly one place decides which representation the renderer and the picker see:
`publishActiveRepresentation` in `forgeshape_jni.cpp` publishes the Construction
mesh in Construction mode and the Frozen Sculpt Mesh in Sculpt mode, both through
the existing `MeshStore` path.

That is the whole of the sculpt render/pick integration. The renderer already
draws the store's current revision and picking already reads it, so making the
sculpt mesh active is a *publication*, not a renderer change — the renderer still
owns no geometry truth, there is no second upload mechanism, and the two
consumers cannot end up looking at different representations.

### Fixed topology: adjacency and normals

Sculpt topology never changes — `SculptMesh` can only move a vertex — so the
index buffer a Freeze copied is the index buffer for the life of that frozen
mesh. `SculptTopology` is therefore built **once, in `freezeFrom`**, and reused by
every stroke and every move; nothing rebuilds it per Move, and the self-tests
assert that its build count stays at 1 across a whole stroke.

It stores exactly two things, in flat CSR arrays: each vertex's **1-ring vertex
neighbours** and its **incident triangles**. Neighbour lists are sorted and
deduplicated, because a shared edge is seen once from each of its two triangles
and without the dedup every interior neighbour would be weighted double in a
mean. Out-of-range indices are skipped rather than trusted and no vertex is ever
its own neighbour. It is deliberately **not** a half-edge structure: half-edges
are the right shape for *changing* topology, and nothing here can change
topology.

`computeVertexNormals` produces **area-weighted** normals from the CURRENT
positions: each triangle contributes its unnormalized `(v1-v0) x (v2-v0)` — the
same canonical outward normal picking uses, whose length is twice the triangle's
area — to all three corners, and each vertex normal is then normalized. A
degenerate triangle contributes a zero vector rather than a NaN, and a vertex
with no usable accumulated direction gets the **zero** vector rather than an
invented axis, so a normal-based brush simply does not move it. Every value
written is finite.

**When normals are recomputed:** the cache is marked dirty by every accepted
`setVertexPosition` and recomputed on the next read. That is the whole rule — at
most once per batch of position writes, in practice once per brush move that
changed something, and never per frame and never when nothing has moved.

### The brush kernel

`SculptStroke` is the one kernel. It captures everything on DOWN and then holds
it fixed for the whole stroke: the **active tool**, the affected vertex set,
their falloff weights, their starting positions and **starting normals**, the
world-space camera plane, the world-per-pixel scale at the hit depth and the
inverse model transform.

```
DOWN   buildPickRay -> transformRayToLocal -> pickTriangleMesh (front faces only)
       miss                  -> no stroke at all, for any tool
       hit                   -> anchor, depth along the camera FORWARD axis,
                                worldPerPixel at that depth,
                                localRadius = radiusPixels * worldPerPixel,
                                capture every vertex inside it with its weight,
                                its base position and its base normal
MOVE   accumulate pointer path length, then dispatch on the captured tool
       -> advance SculptRevision, publish through MeshStore
UP     finalize        CANCEL  drop the stroke
```

Everything above the dispatch is shared. **A tool is a deformation rule and
nothing else**, selected by a closed enum and a switch — there is no brush base
class, registry, plugin surface or reflection, because four tools do not justify
a framework and a framework would have to be persisted, versioned and validated
like real authored state.

Deliberate properties of the kernel, shared by all four tools:

- The **radius is authored in screen pixels** and resolved to object space at the
  depth of the hit point, so the brush feels the same size at any zoom. It is a
  property of the gesture, not a length belonging to the object, which is why it
  is not in meters.
- Depth is measured along the camera **forward axis**, not along the ray, so the
  scale is the same everywhere on screen. The field of view comes from the
  snapshot's own projection term (`proj.m[5] = -1/tan(fovY/2)`); nothing here
  restates `kFovYRadians` or the aspect.
- The **affected set is fixed at stroke start**, so a vertex cannot wander into
  or out of the brush mid-stroke and a stroke stays one coherent deformation. It
  is found by a linear scan, like picking; there is no spatial acceleration.
- The falloff is `w = (1 - (d/r)^2)^2`: 1 at the centre, 0 at and beyond the rim,
  with zero derivative at both ends, so a stroke leaves no crease at the edge.
  One function, `sculptFalloff`, used by every tool.
- A brush that would capture **no** vertex starts no stroke, exactly as a miss
  does.
- **Radius and strength are shared by every tool.** There is no per-tool copy of
  either, so switching tools never changes how big or how strong the brush is.
  Both are **clamped**, not refused: a slider cannot produce a brush that does
  nothing or a brush without bound, and native code stays the authority.
- A stroke **holds the tool it began with**. Changing the session's tool
  mid-stroke changes what the next stroke will be, never what this one is.
- A cancelled stroke is a stroke that **stopped**, not one that is undone.
  Positions already written stay written; there is no undo, and a partial one
  invented here would be worse.

### The four tools

| | driven by | direction | accumulates |
| --- | --- | --- | --- |
| Grab | where the finger **is** | camera plane | no — `base + delta × weight` |
| Clay | pointer **path length** | each vertex's normal **at stroke start** | yes |
| Smooth | pointer **path length** | toward the 1-ring neighbour mean | yes |
| Inflate | pointer **path length** | each vertex's normal **right now** | yes |

**Grab** is position-driven: every Move recomputes `base + delta * weight` rather
than accumulating, so the result depends only on where the finger *is*, never on
how many events it took to get there.

The other three are **path-driven**, and that is the whole of the accumulation
rule. For each move, `travelFraction = pointer travel this move / brush radius`,
both in pixels, and

```
amount = strength * localRadius * kNormalBrushGain * travelFraction   (Clay, Inflate)
lambda = strength * weight * kSmoothGain * travelFraction             (Smooth)
```

Measuring path length rather than counting events is what makes them independent
of the event rate: the same finger path always deposits, relaxes or expands the
same amount whether Android delivered it in five events or fifty, a stationary
finger does nothing at all, and there is no timer and no per-event dab. `amount`
is clamped to one local radius per move, so a teleporting pointer cannot produce
an unbounded displacement. `kNormalBrushGain` (0.35), `kSmoothGain` (1.0) and
`kMaxSmoothLambda` (0.9) are chosen defaults, not derived constants; the low gain
is why a short stroke with a large brush reads as doing very little.

**Clay is deposition and Inflate is expansion**, and the difference between them
is in the formula rather than in a constant: Clay reads `baseNormal` — a
direction field captured once on Down and held fixed, exactly as the affected set
and the weights are — so it lays down a coherent slab in one consistent set of
directions. Inflate reads `mesh.vertexNormals()`, recomputed from the CURRENT
positions on every move, so on a bulge that is already forming the flank normals
have tilted outward and Inflate widens and rounds it instead of extruding along
the directions it started with. It is directly observable: a clayed vertex's
*total* displacement stays exactly parallel to the normal it started with however
many moves it took, while an inflated vertex's leaves that axis as soon as the
surface has moved. The self-tests assert both halves with thresholds three orders
of magnitude apart.

**Smooth is bounded interpolation, never extrapolation:**

```
p := p + (neighbourAverage - p) * lambda,    0 < lambda <= kMaxSmoothLambda
```

so a vertex can only move part of the way toward a point it is already surrounded
by. That is what makes repeated smoothing converge — the deviation from the mean
shrinks by `(1 - lambda)` per application and can never change sign — and it is
why no clamp on the result is needed to keep it finite. Every target is computed
from a coherent snapshot of the current positions **before** anything is written
(Jacobi, not Gauss-Seidel), so the outcome cannot depend on the order the
affected set happens to be in. Only affected vertices are written; neighbours
outside the brush are read and never modified. Inflate snapshots its normals the
same way and for the same reason.

### Sculpt-mode gesture rule and arbitration

One finger that goes **down on the Frozen Sculpt Mesh** is a brush stroke and
owns the whole gesture; one finger that goes down anywhere else navigates exactly
as it always has. Two-finger pan and pinch are untouched. Whether the finger
landed on the mesh is decided **once, on Down, and never revisited**, so a stroke
cannot turn into an orbit half way through a drag as the finger crosses the
silhouette.

What *is* deferred is whether the gesture is a stroke at all. A one-finger Down
on the mesh is ambiguous when it arrives — it is either the start of a stroke or
the first of two fingers — so the rule is **pending-then-promote**:

| event | result |
| --- | --- |
| Down, one finger, ray hits the mesh | **PENDING**. Swallowed: no stroke exists, nothing is deformed, and neither the camera nor the selection sees it. |
| Move, still one finger, travelled >= `kStrokeArmPixels` (8 px) | **PROMOTE**. The stroke begins at the **original down point**, and this same event is applied as its first move. |
| a second finger, an Up, a Cancel, any multi-pointer event | **ABANDON**. No stroke ever existed, so there is nothing to end and nothing to undo. |

`SculptSession::hitsSculptMesh` is the probe that answers "would a stroke start
here?" *without* starting one. It provably cannot move a vertex, mint a revision
or advance the stroke counter, and that is the structural half of the guarantee
that a gesture which turns out to be navigation cannot have mutated the sculpt
mesh — the self-tests assert it directly, and the log shows it as a
`STROKE_PENDING` with no `STROKE_BEGIN` after it.

Anchoring the promoted stroke at the original down point is what makes the
deferral free: the hit, the affected set and the falloff weights are exactly what
they would have been had the stroke begun on Down. Because the camera re-anchors
on any pointer-set change, handing it an abandoned gesture produces no jump; and
because the selection never saw the Down, an abandoned gesture cannot select or
clear either. While a stroke owns the gesture, neither `CameraController` nor
`SelectionController` sees the event at all, which is what makes a brush gesture
structurally unable to orbit, select or clear.

The 8 px threshold sits deliberately between touch jitter and the 24 px tap slop,
so a stroke still starts long before the gesture would stop being a tap. The
residual it accepts is stated rather than hidden: a first finger that
*deliberately drags more than 8 px* before the second one lands does commit a
stroke. That is a gesture the user drove as a stroke, and closing it completely
would require a buffered or undoable stroke, which does not exist yet.

`g_grabbing` and `g_strokePending` in `forgeshape_jni.cpp` are gesture routing
only, guarded by `g_stateMutex` alongside the camera and the selection. Like
camera anchors and tap candidacy they are dropped whenever the Surface goes away;
the mode, the active tool and the sculpted vertices are not, because they are
process-scoped.

## Runtime mesh ownership

`forgeshape_mesh.{h,cpp}` owns the CPU side of geometry. It contains no JNI,
Android or Vulkan types and holds no GPU resource.

`RuntimeMesh` is one **immutable** published revision: an object id, a revision
number, interleaved `MeshVertex` (position + colour) data and `uint32_t` indices.
The only way to build one is `createRuntimeMesh`, which validates first, so a
`RuntimeMesh` that exists has already been proven usable. Consumers hold it
through a `shared_ptr<const RuntimeMesh>`, so a pick in progress keeps reading
its own revision even while a newer one is published.

`MeshStore` is the single publication point and is process-scoped. It mints
monotonically increasing revisions, and:

- invalid data **fails closed** — the previous revision stays current and the
  reason is reported (null data, zero counts, index count not a multiple of
  three, an index >= vertex count, a non-finite position, or past the hard caps);
- a snapshot whose revision is not strictly newer than the current one can never
  replace it;
- the stable `ObjectId` belongs to the store, not to the geometry and not to any
  GPU resource, so replacing the mesh cannot change what is selected;
- consumers observe only the newest revision. Revisions published between two
  observations are **coalesced away**, deliberately: that is what bounds the GPU
  upload path.

The publication path is synchronous and republishes the whole mesh, which is
O(vertices) per sculpt move regardless of how few vertices the brush touched.
That is measured as free at these sizes and is the known cost to compare a future
partial-update or async path against.

### Index width

The runtime and render paths use **32-bit indices** (`uint32_t` /
`VK_INDEX_TYPE_UINT32`). 16-bit indices would structurally cap every mesh at
65,535 vertices, and there is no measured benefit at these sizes; the CPU
`TriangleMeshView` and the GPU index buffer agree on one width, so they cannot
drift.

## Derived render geometry and shading

`forgeshape_render_mesh.{h,cpp}` sits between the authoritative mesh and the GPU.
It is platform-neutral and holds no GPU resource. The chain is one-way:

```
Construction / Sculpt truth
  -> authoritative RuntimeMesh   (positions + indices + colour)
      -> RenderMeshData          (positions + NORMALS + colour)
          -> Vulkan buffers
```

Nothing is read back. A normal is never a dimension, a Construction parameter, a
sculpt deformation or picking topology, and **CPU picking still runs on the
source `RuntimeMesh`** — which is precisely what frees this layer to duplicate
vertices, because a render vertex has no identity anything outside the renderer
can observe.

### Render-only vertex duplication, and the two counts

A hard edge needs two different normals at one position, and a vertex carries one
normal, so a corner on a crease becomes several **render** vertices. Therefore:

- render vertex count >= source vertex count, and usually differs;
- render index count == source index count, always — a corner is remapped, never
  added, so the triangle list is the same triangles;
- every diagnostic names which it means. `FORGESHAPE_MESH_UPLOAD_OK` reports
  render counts in its historical positions and adds `src=v:i`;
  `FORGESHAPE_RENDER_MESH_BUILD` reports both explicitly.

Measured, at the default dimensions: box `8:36 -> 24:36`, cylinder
`66:384 -> 130:384`, sphere `482:2880 -> 482:2880`, cone `34:192 -> 66:192`,
capsule `514:3072 -> 514:3072`. A fully smooth closed surface has no crease to
split on, so its render mesh *is* its source topology — which is the cheapest
available proof that the grouping does not fragment a smooth surface.

### The crease policy

One threshold, `kCreaseAngleDegrees = 40`, stated once and used nowhere else.
Two triangles sharing a vertex contribute to the same smoothed normal when the
angle between their face normals is at most that; otherwise the vertex splits and
each group gets its own normal. Grouping is a per-vertex union-find over that
vertex's incident triangles, so transitivity lets a sphere pole's 32-triangle fan
become one group even though its extreme members are far apart in azimuth.

The value must clear the coarsest curved adjacency (360/32 = 11.25°) and stay
well under the sharpest edge a primitive presents (90°); 40° is near the middle
of that band, so neither bound is close. That single rule produces every
per-primitive contract — a box's hard 90° edges, a cylinder's smooth side with
flat caps and hard rims, a sphere's continuous shading and stable poles, a cone's
smooth side with a hard base rim and an on-axis apex normal, and a capsule's
seamless hemisphere-to-middle transition — with no per-primitive special case.

Normals are area-weighted (`(v1-v0) x (v2-v0)`, whose length is twice the area),
the same weighting `computeVertexNormals` uses for the sculpt cache, so a surface
does not change character between the two paths. A degenerate triangle
contributes the zero vector rather than a NaN, and a group with no usable length
keeps the zero normal — honest, and handled by a documented shader fallback.

Faceted shading is the other branch: three private vertices per triangle carrying
that triangle's flat normal. It intentionally exposes triangle structure.

### Rebuild policy

`RenderMeshCache` rebuilds when — and only when — the source `MeshRevision` or
the `SurfaceShading` changed. It does **not** rebuild for a camera move, a
rotation, a window resize, a unit switch, an inspector toggle, a mode or tool
change, or a Studio<->MatCap change, because that last one is a fragment-stage
uniform touching no geometry at all. The renderer gates on the same pair before
calling in, so a steady frame costs two integer comparisons.

Measured on `emulator-5558`: **4448 presented frames, 2 rebuilds** — across eight
camera-orbit gestures and three shading-model changes, zero rebuilds and zero
uploads. Rebuild cost is 0.03 ms (box), 0.55 ms (cylinder), 0.7-1.3 ms (sphere),
0.78 ms (capsule) and 0.56-0.73 ms for a 482-vertex sculpt mesh per accepted
move.

The cache derives its own adjacency from the index buffer and deliberately does
**not** reuse `SculptTopology`: the renderer depends on the published
`RuntimeMesh` and on nothing in the Sculpt domain, or "presentation only" would
stop being true the moment sculpt state changed shape.

### Studio Solid and MatCap

Two shading models plus a debug-only source-colour path, as a closed enum and a
switch — the same rule the sculpt tools follow. Both are evaluated in **view
space**, so the lights follow the camera. That is a product decision, not a
convenience: while modelling the user orbits constantly, and world-fixed lights
would swing a face from lit to unlit purely because the viewpoint moved, which
reads as the shape changing. Camera-relative light means a change in shading
always means a change in the *model*.

Studio Solid is computed per fragment in `shaders/surface.frag` and is tuned
matte and even, for judging planar faces and exact silhouettes. MatCap is a
single texture lookup at `uv = n.xy * 0.5 + 0.5`, tuned glossier and
higher-contrast, for reading curvature and sculpt deformation. They share one
geometry of light — same key direction, same fill, same hemispherical ambient —
so switching does not relight the object.

Both fills are placed lower-**front**, not opposite the key. A fill opposite the
key lifts exactly the planes the key leaves dark, and a box's left and right
faces end up nearly the same value; measured, that rig gave 0.44 vs 0.40 against
0.49 vs 0.29 for the current one.

There is no PBR here and none is implied: no metalness, no roughness, no
environment probe, no shadow map, no ambient occlusion and no tone-mapping stack.

### The MatCap asset

`forgeshape_matcap.{h,cpp}` **computes** the one 128x128 RGBA8 preset at device
initialization from a closed-form model written out in that file. There is no
image in the repository, nothing downloaded, and nothing derived from another
application's asset. Generating rather than shipping a file is also the only
option that respects the no-third-party-library rule — decoding a PNG would need
a decoder ForgeShape may not depend on. Texels outside the unit disc are clamped
to the rim value in the same direction so bilinear filtering at a silhouette does
not bleed, and the sampler uses `CLAMP_TO_EDGE` for the same reason.

Exactly one preset. No library, no browser, no import, no per-object material.

### Display settings ownership

`forgeshape_display.{h,cpp}` holds the shading model and the surface shading as
two process-scoped atomics. Native owns them exactly as it owns the product mode
and the active tool; the Android UI may request a change and read the value back,
but does not hold it — which is why they survive HOME/resume with no save/restore
code in the Android layer. A snapshot is pushed into the renderer per frame,
outside the state mutex, because no domain invariant depends on it and a frame
must never wait on the geometry lock to learn which shading model to draw with.

## GPU mesh upload

`Renderer` owns every mesh-related `VkBuffer`, `VkDeviceMemory`, copy and
destroy, and performs all of them on the render thread. Publishers never touch
Vulkan.

Steady-state vertex and index buffers are **DEVICE_LOCAL** with `TRANSFER_DST`
usage. They are written through one reused **HOST_VISIBLE** staging buffer that
carries the vertex block followed by the index block, copied in a single command
buffer with one memory barrier
(`TRANSFER_WRITE` → `VERTEX_ATTRIBUTE_READ | INDEX_READ`).

Capacity policy (`growCapacityBytes`, pure arithmetic, self-tested):

- sufficient existing capacity is **reused as-is**, including for a smaller
  mesh — capacity never shrinks and a same-topology update never recreates a
  buffer;
- otherwise capacity grows by 1.5x, but never to less than what is needed;
- everything is bounded by a hard cap, so the size arithmetic cannot overflow.

The "reuse a smaller mesh" rule earns its keep now that render vertex counts are
derived. A sculpt stroke that deforms a surface enough to create genuine creases
splits vertices, so the **render** count drifts move to move (measured: 482 → 492,
oscillating 491/492) even though the source count is fixed. Because capacity
never shrinks, that whole stroke ran with **every upload `reuse` and zero buffer
growth** — the fluctuation costs nothing.

### In-flight resource safety

Before a mesh buffer is overwritten or retired, the renderer waits on **its own
frame fences** — all `kMaxFramesInFlight` of them — so no submitted frame can
still be reading it. The transfer itself is submitted with a dedicated upload
fence that is waited on before the staging buffer or the upload command buffer is
reused. A retired buffer is therefore destroyed only after every frame that could
reference it has finished, and exactly one vertex buffer and one index buffer are
live at any time. Retired buffers are freed inline after that fence wait rather
than through a deferred-destruction queue, which is what makes the wait
necessary.

The mesh update path deliberately calls **neither `vkDeviceWaitIdle` nor
`vkQueueWaitIdle`**. Those remain only where they already were: process teardown,
surface detach and swapchain rebuild.

Mesh buffers are device-scoped, not surface-scoped: a Surface swap does not touch
them, so the uploaded revision survives home/resume with no re-upload.

## Picking and selection

`forgeshape_picking.{h,cpp}` converts a view-local pixel plus a `CameraSnapshot`
into a world-space ray, and intersects that ray with indexed triangles
(Möller–Trumbore, nearest positive hit). It reads the snapshot's own `proj` and
`view` matrices rather than restating FOV or aspect, so there is one camera
truth. It contains no JNI, Android, Vulkan or renderer types, decides no object
identity, and uses no GPU id buffer.

`pickScene` intersects the **current CPU mesh snapshot** from `MeshStore` — never
Vulkan buffer memory — and takes the object id from the store rather than from
the geometry. Because that snapshot is whichever representation is active,
generated from the current parameters or sculpted, picking automatically follows
an edit with no separate collision representation to keep in sync. Picking is a
linear scan; there is no spatial acceleration.

`SelectionController` (`forgeshape_selection.{h,cpp}`) owns the selected
`ObjectId` and the tap-versus-navigation decision. `ObjectId` is an opaque
`uint64_t` minted by this layer: never a pointer, list index, Vulkan/renderer
handle or display name, so it survives buffer recreation and Surface swaps.
`kNoObject == 0` means nothing is selected; the one object is `1`. This is a
selection *foundation*: exactly one selectable object, no hierarchy, no scene
graph, no multi-select.

## Tap versus orbit

Both controllers receive the same `forgeshape::TouchAction` event. Camera and
selection never consult each other.

Tap candidacy is one-way — it can only be revoked during a gesture, never
restored, and only a fresh `Down` starts a new candidate. It is revoked when:

- total displacement from the **original** down position exceeds
  `kTapSlopPixels` (24 px) — measured from the down point, not per-move, so a
  slow creeping drag still cancels;
- the gesture ever reaches two or more pointers (`PointerDown`/`PointerUp`, or
  any event carrying 2+ pointers);
- the event stream carries a pointer id other than the tracked one;
- `ACTION_CANCEL` arrives, or the Surface goes away.

Only `Up` on a still-valid candidate resolves a pick. A hit selects that object;
a miss clears the selection. A gesture that orbited, panned or pinched therefore
cannot select or clear on release.

## Renderer

`Renderer` (`forgeshape_renderer.{h,cpp}`) owns the Vulkan instance, device,
queues, surface, swapchain, depth resources, render pass, pipeline, command
buffers, synchronization and the geometry buffers. It exposes
`setCamera(const CameraSnapshot&)` and `setModelTransform(const Mat4&)` and
consumes both verbatim; it derives no camera pose, builds no rotation, owns no
Euler convention and interprets no pointer data.

The mesh vertices are the object's **local** geometry and never move. Where the
object appears comes from the model transform, where the viewer stands comes from
the camera snapshot, and the renderer only composes them as
`mvp = proj * view * model` — so a screen-space change is always attributable:
the camera moved, or the object did.

Its only selection input is `setSelectionHighlight(bool)` — pure visual state,
delivered to the fragment shader as a tint push constant. The renderer is never
told *which* object is selected. `setDisplaySettings(...)` is the same kind of
input: consumed verbatim, owned elsewhere. Once per frame it asks `MeshStore` for
the current revision and, if that revision or the surface shading differs from
what is uploaded, rebuilds the derived render mesh and uploads that — so render
and pick geometry both follow the same published revision and cannot diverge.

What the GPU holds is `RenderVertex` (position + normal + colour), not
`MeshVertex`. The authoritative format is no longer handed to Vulkan directly.

### Push constant budget

One 128-byte range covering both stages, which is the **smallest**
`maxPushConstantsSize` Vulkan guarantees, so the layout stays valid on
implementations exposing only the minimum:

```
  0  mat4 mvp
 64  vec4 normalRow0    xyz = row 0 of the view-space normal matrix, w = shading model
 80  vec4 normalRow1    xyz = row 1
 96  vec4 normalRow2    xyz = row 2
112  vec4 selectionTint rgb = tint, a = mix amount
```

A `static_assert` pins the size. The shading model rides in an otherwise-dead
`w` component rather than taking a fifth 16-byte slot the budget does not have;
the next thing needing per-draw uniform data belongs in a descriptor, not here.

Normals are transformed by the upper-left 3x3 of `view * model` applied
**directly**, not as an inverse-transpose. That is valid only because both
factors are rigid — a look-at view matrix, and a `ConstructionTransform`
documented as rotation + translation with no scale — so the product is
orthonormal and its inverse-transpose is itself. **Adding scale to the transform
would make normals silently wrong on scaled objects**; it is one of exactly two
places that shortcut is taken, the other being picking's "local distance is world
distance". The fix would be CPU-side; the shader would not change.

### The one descriptor set

The MatCap sampler is the only sampled image in ForgeShape and therefore the only
reason a descriptor set exists — everything else still travels as push constants.
One `COMBINED_IMAGE_SAMPLER` at set 0 binding 0, allocated once and never updated
again, because the image is immutable for the life of the device: no per-frame
descriptor traffic and no per-frame-in-flight copies. It is bound unconditionally
even in Studio Solid, since leaving a declared binding unbound is invalid usage
regardless of which shader branch runs. The image, its view, its sampler and the
set are device-scoped, so a Surface swap does not touch them and a HOME/resume
neither regenerates nor re-uploads the MatCap.

### Surface orientation convention

There is exactly one orientation convention, and it is this: **ForgeShape always
renders in Android window orientation.** The swapchain image is the size of the
window the user sees, and any display rotation is performed by the presentation
engine, never by this renderer.

Concretely, `Renderer::createSwapchain` requests
`preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR` whenever identity appears
in `caps.supportedTransforms`, and takes `imageExtent` from `caps.currentExtent`,
which on Android is the window's own width and height. So one coordinate space —
the Android window — runs unbroken through `SurfaceView` size, the camera
viewport and projection aspect (`CameraController::setViewport`), the swapchain
image, the Vulkan viewport and scissor, and the screen pixel that picking turns
into a ray. Nothing anywhere transposes width and height, and no matrix carries a
display rotation.

ForgeShape deliberately does **not** pre-rotate. Pre-rotation would require
`imageExtent` in the display's *pre-transform* (panel) space, which is the
transpose of the window at 90° and 270°, plus a matching clip-space rotation and
a camera aspect to match. Declaring `preTransform = caps.currentTransform` while
passing the window-space extent — which is what the renderer did until this was
fixed — is the inconsistent combination: SurfaceFlinger rotates the 2400×1080
buffer into a 1080×2400 layout and then stretches it back onto the 2400×1080
window, an anisotropic scale of (2400/1080, 1080/2400). That, and not the
projection, was the rotated-landscape defect; `mat4Perspective` and
`CameraController::setViewport` were correct throughout.

The cost of the convention is one compositor rotation on a rotated display, which
is what every non-pre-rotated Android application already pays. Its consequence
is that `vkAcquireNextImageKHR` and `vkQueuePresentKHR` report
`VK_SUBOPTIMAL_KHR` for as long as the device is rotated — the surface's
transform is genuinely not the one the swapchain declared. That is the convention
working, not a stale swapchain, so the frame loop must not rebuild on it:
`Renderer::expectSuboptimal_` records when the declared pre-transform differs
from the surface's own, and suboptimal is ignored in exactly that case. Rebuilds
still happen on `VK_ERROR_OUT_OF_DATE_KHR` and on the explicit `requestResize()`
that `surfaceChanged` raises, which is how every real size change arrives. Losing
this distinction rebuilds the swapchain on every single frame while rotated.

One non-per-frame `FORGESHAPE_SURFACE_CONFIG` line per swapchain creation records
the window size, `currentExtent`, `currentTransform`, `supportedTransforms`, the
chosen extent and the chosen `preTransform`, so the whole chain is auditable from
a log without adding instrumentation; `FORGESHAPE_CAMERA_VIEWPORT` is its
companion for the camera half.

## Threading

- Android UI thread: surface callbacks, `onTouchEvent` → JNI, both panels and
  both Applies, Construction publication, every brush stroke and its per-move
  sculpt publication, and DEBUG mesh fixture publication. `ConstructionObject` —
  its kind, every primitive's parameters and its transform — and `SculptSession`
  are both mutated from this thread only, which is why neither carries a mutex of
  its own; `MeshStore` has one and is what the render thread reads.
- Render thread (owned by `forgeshape_jni.cpp`): Vulkan work, presentation and
  every mesh buffer create/copy/destroy.
- One short-lived thread per DEBUG stress run, which publishes CPU revisions and
  exits. There is no general task or job system.

`MeshStore` has its own mutex, held only long enough to swap or copy a
`shared_ptr` — never across a GPU copy, and never together with the camera and
selection mutex in a way that could invert. A publisher never blocks a frame, and
the render thread's mesh upload happens outside `g_stateMutex` entirely.

`g_stateMutex` guards the single `CameraController`, the single
`SelectionController` and the single `ConstructionTransform` together, plus the
gesture-routing flags. The UI thread mutates them under it; a tap resolves its
pick against the camera snapshot *and* the transform and updates the selection
under one lock hold, so they can never be seen out of step. The render thread
takes the camera snapshot, the derived model matrix and the selection highlight
flag under the same lock immediately before each `drawFrame()`, so a frame can
never be drawn with the camera from one transform state and the model from
another.

## Lifecycle contract

The `CameraController`, the `SelectionController`, the `MeshStore`, the
`ConstructionObject` and the `SculptSession` are all process-scoped and outlive
every Surface: camera pose, selected `ObjectId`, the active primitive, its
authoritative parameters, its authoritative placement, the current mesh revision,
the active product mode, the active tool and every sculpted vertex survive
home/resume and swapchain recreation. There is no rehydration step, because
nothing was discarded — the GPU mesh buffers are device-scoped and are not
destroyed when the Surface goes away, so a resume re-presents the same revision
without re-uploading it. Nothing survives a process restart, because there is no
save, no load and no undo.

Gesture tracking is separate: camera anchors and tap candidacy are both reset on
`ACTION_CANCEL`, on `surfaceDestroyed` and on `surfaceCreated`, and a live brush
stroke and any pending one are cancelled with them — so a Surface swap can never
leave a stale touch anchor, a half-finished tap or a half-finished stroke behind,
and never disturbs what is selected or what has been sculpted. Neither camera
state nor selected identity is stored inside swapchain or surface resources.

`surfaceDestroyed` blocks until native code has released the `ANativeWindow`, so
the render thread can never touch a destroyed window.

## Current boundaries

What the architecture deliberately does **not** contain, because each of these
is a stage of its own and naming them is what stops one arriving by accident:

- **No brush framework.** Four tools behind one kernel; a fifth is another enum
  case, another `apply*` and another button — a visible, deliberate cost, and the
  right one until something needs brushes to be data rather than code. No remesh,
  no subdivision, no dynamic topology, no sculpt undo or stroke history, no
  symmetry, masking or layers, no stylus pressure or tilt, no brush presets, and
  exactly **one** Frozen Sculpt Mesh for the one object.
- **No mesh library.** `SculptTopology` carries adjacency for a fixed topology
  and nothing more: no edge collapse, split, flip or incremental update, because
  the only thing that can happen to a frozen mesh is that a vertex moves.
- **No primitive framework.** No primitive base class, polymorphism, registry,
  property metadata, reflection or plugin surface; no second Construction object,
  no hierarchy, no create and no delete. A further primitive costs another
  member, another `PrimitiveKind` case, another variant alternative and another
  per-kind JNI method. What must NOT happen is a registry sneaking in as "just a
  vector of objects": multi-object is a stage of its own, with selection,
  identity-minting and lifetime consequences this design has not paid for.
- **No scale and no gizmo.** `ConstructionTransform` is deliberately rigid —
  rotation and translation only — which is exactly what lets the picker use an
  exact composed inverse and keep the ray's distance in world units. Adding scale
  breaks both and is a domain change, not a matrix change.
- **No editable tessellation.** A consequence worth naming: the capsule's
  cylindrical middle is a **single band** between its two seam rings however long
  that middle is, exactly as the cylinder's side wall is. Shape, bounds and
  picking stay exact, but that middle carries no interior rings, so a small
  sculpt brush placed there has very few vertices to capture — a
  tessellation-fidelity limitation, not a correctness one.
- **No property-editor framework.** The Property Inspector is three hand-written
  bodies for one object: no property model, no binding layer, no editor registry,
  no reflection, no second inspected object. `NumericPropertyRow` and
  `UnitChipsView` are components, not a framework — they know what a labelled
  number and a unit are, and nothing about primitives.
- **No hierarchy surface.** The Editor Workspace has a Global Toolbar, one Tool
  Rail and one Property Inspector. There is no object browser, no history panel
  and no docked second inspector, because there is one object with two
  representations and nothing to browse.
- **`RuntimeMesh` is not a Construction mesh format**, and the debug paths are
  not product. `RuntimeMesh` carries positions, colours and indices and nothing
  else: no normals, no UVs, no material, no adjacency, no history.
  `forgeshape_demo_mesh` is the bootstrap cube's numbers only and
  `forgeshape_mesh_fixtures` is DEBUG test infrastructure reachable only through
  a key hook that compiles to a no-op in release; neither is a primitive, a
  Construction feature or a sculpting feature, and both are deliberately absent
  from `PRODUCT.md`. That hook is not a parallel implementation either: its
  bounded primitive driver (four fixed box states, one deliberately invalid)
  calls the same `applyPrimitive` entry point, and its ability to publish a
  fixture over the object's current revision is exactly what makes it a *test*
  path.
