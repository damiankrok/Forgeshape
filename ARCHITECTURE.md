# ForgeShape — Architecture

Current production architecture only. No roadmap, no history.

ForgeShape is a standalone Android application that owns its own viewport and
renderer. No game engine (Godot/GDExtension, Unity, Unreal) and no third-party
runtime, rendering, math or input library participates in the running product.

## Layer map

What each box owns is in the Ownership table below; what the diagram adds is the
**direction of every arrow**, which the table cannot show.

```
ForgeShapeActivity
        |
        +-- EditorWorkspaceView   (+ EditorUiState, WorkspaceLayoutMode)
        |        +-- StartChooserView / GlobalToolbarView / ToolRailView
        |        +-- BrushEdgeControlsView / ObjectsSectionView
        |        +-- PropertyInspectorView
        |                 +-- ConstructionShapeEditorView    what the object IS
        |                 +-- ConstructionPlacementEditorView  where it SITS
        |                 +-- SculptContextView   mesh state + guarded re-Freeze
        |
ForgeShapeSurfaceView         Surface lifecycle + raw pointer forwarding
NativeViewport (JNI decls)
        |  JNI  -- Android/JNI types stop HERE --
forgeshape_jni.cpp            render thread, ANativeWindow, MotionEvent ->
        |                     TouchAction, camera/selection locking,
        |                     stroke-vs-navigation arbitration
        |
        +--> CameraController -> CameraSnapshot
        |
        +--> SelectionController -- on a valid tap --> pickScene() -> Picking
        |         |                                        ^
        |         v                                        | current snapshot
        |    ObjectId or none -> bool "draw highlight"      |
        |                                                   |
        |    ConstructionObject                 SculptSession
        |        |                                  |
        |        generateMesh() [LOCAL] --Freeze--> SculptMesh [LOCAL copy,
        |        |                                  own SculptRevision]
        |        +---- the ACTIVE one is published -+
        |        v                                  |
        |    MeshStore (immutable revisions, monotonic, latest-wins)
        |        |                                  |
        |    modelMatrix() / inverseModelMatrix()   |
        v        v                                  v
      Renderer  (Vulkan, frame loop, upload; owns no geometry truth)
        |            ^-- forgeshape_grid: world reference floor, NOT in the scene
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
| The ordered collection of Construction Bodies, ObjectId minting, and which body is active | `ConstructionScene` (`forgeshape_scene.{h,cpp}`) | a flat list, not a scene graph or a hierarchy; ids are never a collection index |
| Identity, which primitive is active, every primitive's parameters, and placement — for ONE body | `ConstructionObject` (`forgeshape_construction.{h,cpp}`) | it knows nothing of the collection holding it |
| Which of the two appearances the process wears | `EditorUiState` (static) → `AppTheme` | UI-owned, losable, never persisted; the domain sees only a viewport-background index |
| What the viewport is cleared to | `ViewportBackground` in `forgeshape_display.{h,cpp}` | native owns the colours; no Android theme or RGB crosses JNI |
| The world reference grid's plane, spacing, extent, tiers and palette | `forgeshape_grid.{h,cpp}` | it is not a `SceneObject`, has no `ObjectId` or revision, is not pickable, and is not a snap target |
| Whether the grid is drawn | `DisplaySettingsStore` | the renderer owns no presentation preference; the grid module owns no visibility |
| Whether Objects and the Tool Rail are docked for a given window | `WorkspaceLayoutMode.objectsDocked(widthDp)` / `railDocked()` | arithmetic on window dp; neither reads theme, mode or domain state |
| Which surface currently hosts the Objects list | `EditorWorkspaceView` | there is exactly ONE `ObjectsSectionView`, re-parented; no second list and no Java-side selection truth |
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
| Platform-neutral pointer event data | `forgeshape_input.h` | one shared type, not an input framework. Carries tool type, pressure and tilt as well as id and position |
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
| Input | crosses the boundary as **semantic, platform-neutral** data. `forgeshape_input.h`'s `TouchAction`/`TouchPointer` is that boundary, and it carries tool type, pressure and tilt alongside id and position -- in ForgeShape's own enum and its own units, never Android's. A pointer sample is translated out of Android's vocabulary in the Android layer rather than carried inward. Hover and generic (non-touch) motion are still outside the vocabulary and stay a consumer-driven question |
| Platform services | future file, storage and system services get narrow boundaries of their own, for the same reason input has one |
| Renderer coupling | the renderer's dependency on a platform surface stays **explicit and local**: `forgeshape_jni.cpp` owns the `ANativeWindow` and hands it over, and `Renderer` never creates or releases one. That single visible seam is what a second backend would be added beside |

The practical test is one question: *if this file had to compile on a platform
that has no Android, what would break?* For everything below `forgeshape_jni.cpp`
the answer must stay "nothing". This is deliberately **not** an abstraction
layer — no renderer interface, no platform façade, no `#ifdef` for an operating
system that has no target. What is required is only that the seams stay where
they are and that nothing new crosses them.

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
3. `overlayRoot`, holding what must survive chrome being hidden — the restore
   chip, the Display popover and the start chooser.

Inside `chromeRoot`: `GlobalToolbarView` at the top, then a weighted horizontal
row carrying — leading edge first — the Objects column (expanded windows only),
`BrushEdgeControlsView` (Sculpt only), a weighted gap where the model lives, and
the `ToolRailView` inside a `ScrollView` (trailing edge). `PropertyInspectorView`
is placed either after that row (bottom sheet) or inside it (side placement). All
are plain framework views built in code; no Compose, no AndroidX in the product,
no design system, no drawer.

### Appearance: roles, not colours

**No colour is written in Java, and no component knows which theme it is in.**
A role is declared once as a theme attribute in `attrs.xml`, given a value once
per theme in `themes.xml`, and referenced as `?attr/fs*`. Backgrounds are
`res/drawable` state lists and content colours `res/color` state lists, both
carrying attributes; the one imperative path is
`EditorControlStyles.themeColor(context, attr)`, for text roles and the two brush
sliders, which are drawn onto a Canvas rather than composed.

That buys three things at once: pressed and active states come from the platform
instead of a repaint call; instances share one parsed `ConstantState` rather than
allocating a `GradientDrawable` per control; and **two themes cost one component
tree** — one `bg_control.xml`, one `chip()`, one `control_content_tint.xml`, no
`if (light)` anywhere, so a third theme would touch two resource files and
nothing else. Two roles exist only because a light theme forced them apart:
`fsAccentFill` (what an ACTIVE control is filled with) and `fsPrimaryFill` (what a
PRIMARY COMMIT is filled with, carrying `fsTextOnPrimary`) are the same colour on
dark, but on light an active chip wants a pale tint with dark text while Apply
wants a solid accent with white, and one attribute could not be both.

Icons are local vector drawables on one 24 dp grid, drawn white and tinted from
the same state list, so an entry's glyph and its caption cannot disagree about
whether it is active, pressed or reserved. Metrics come from `dimens.xml`: corner
radius is three semantic levels — control, floating surface, sheet — and type is
five roles rather than five sizes.

**The theme itself is UI-owned, process-scoped and losable**, in the one static
field in `EditorUiState` beside the start choice. It is applied by `setTheme()`
**before** anything is inflated, and changed by recreating the Activity — the only
clean way to re-resolve themed resources for a UI built entirely in code. That is
safe because nothing that matters lives in the Activity: the scene, every body,
the active ObjectId, the mode, the Frozen Sculpt Mesh, the camera and the display
settings are process-scoped native state. It is also free, because `onDestroy`
skips `NativeViewport.stop()` while `isChangingConfigurations()`, so the render
thread, the Vulkan device and every GPU buffer survive and `start()` returns early
rather than re-running the self-tests. `EditorUiState` is handed to the incoming
workspace, so changing colour does not reset the display unit or close a panel.
Nothing is persisted: a real process kill returns to Dark.

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

Two layout contracts in the chrome are load-bearing rather than cosmetic. In the
Global Toolbar the **editing-context label is the weighted child**, so a row that
has run out of width ellipsises the label instead of squeezing the last action
below the 44 dp touch floor. And a **Tool Rail entry holds its gesture against the
enclosing `ScrollView`**: it disallows interception on Down and allows it again
once travel passes twice the platform slop, at which point the container takes the
next event and the platform's own `ACTION_CANCEL` prevents the click — otherwise a
scroll container takes the tap the moment a finger or a resting stylus tip drifts,
which made selecting a tool intermittently do nothing. Neither touches viewport
gesture arbitration: both are chrome.

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
`setOnApplyWindowInsetsListener` applies `systemBars | displayCutout` — plus the
`ime()` inset, which replaces rather than adds to the navigation bar — as padding
to the chrome containers only. `windowSoftInputMode` is `adjustResize`, but with
decor-fits off the window is **not** resized: the keyboard arrives as an inset the
chrome absorbs and the surface is untouched.

**Chrome depth, and what "docked" claims.** Surfaces that float over the model
carry a small elevation; **docked** surfaces deliberately carry none and are drawn
opaque and flush, because they sit *beside* the model rather than over it. That is
a claim about the layout and it has to be true, which is why it is answered by the
window rather than by taste. `railDocked()` and `objectsDocked(widthDp)` are the
two predicates; the Property Inspector has followed the same rule since Stage 015B.
Containers set `clipChildren(false)`, since a shadow is drawn outside its child's
bounds — drawing only, never hit-testing. The rail's surface and elevation live on
its `ScrollView` rather than on the rail, or the container would clip exactly the
shadow it wraps.

**The Objects surface is one view with two hosts.** An expanded window with room
gives the scene list a leading-edge column (`objectsDock`, a `ScrollView`); every
other window keeps it inside the Construction shape editor. The **same
`ObjectsSectionView` instance** is re-parented between them — never a second list,
because a second Java Objects view would be a second place for "which body is
active" to be remembered, and that answer lives in exactly one place, below JNI.
Because the same view moves, a viewport pick, a row tap and Add Body all end at the
same one native fact and the same one `refreshFromNative()` in every window.
Docked, it scrolls in its own container rather than nested inside the inspector's.

Whether it docks is arithmetic, **not a fourth breakpoint**: `EXPANDED` is
necessary and not sufficient, and a window qualifies only when a 180 dp Objects
column, the rail and the inspector still leave a central viewport at least 480 dp
wide — the absolute floor the expanded layout has always been held to. Deriving it
means a later change to any column width moves the answer instead of silently
violating the floor, and it is why the bottom of the expanded range gets no third
column: three permanent chrome columns on a large phone in landscape is the
desktop-CAD clutter UI-OWNER-02 rules out. The re-parent is **instant**, because
it runs inside `onMeasure` and starting an animation there is the same defect as
running the decision in `onSizeChanged`. **None of it touches the render target:**
the `SurfaceView` is the whole window in every layout mode.

### Property Inspector ownership boundary

`EditorUiState` is the closed list of what the UI may remember: display unit,
draft primitive kind, which Construction editor the rail points at, inspector
detent per mode, chrome-hidden, and whether the start question has been answered.
Every field is safe to lose — kill the process and the object is exactly what it
was. Anything that would change the model if it were wrong belongs in native code
instead.

The start flag and the theme are the **static** members, and deliberately so:
both questions are per *process*, not per Activity, and an instance field would
be destroyed by the very recreation that applies a theme. The start flag records
only *that* an answer was given, never which one — the mode is native truth, read
back on every refresh, and a copy here could disagree with it.

The primitive chooser is a **draft**: it swaps which parameter fields are on
screen and nothing else. The kind changes only when Apply Shape reads the drafted
primitive's own fields and calls **that primitive's own native method**, so there
is no window in which the object is a cylinder carrying box dimensions.
`refreshFromNative` resets the draft to the object's real kind. Exactly one
parameter row is on screen and it is always the drafted kind's; every primitive's
fields stay populated and converted, including hidden ones, so an inactive draft
cannot silently change meaning off screen.

Shape and placement live in **separate inspector bodies with separate Apply
buttons** — *Apply Shape* and *Apply Transform* — because they are separate
truths with different consequences: one republishes the mesh, the other cannot.
One button doing both would hide that. The Construction Tool Rail chooses which
body is on screen; that choice is UI layout state, makes no native call, and
deliberately does not refresh the editors, so a half-typed value in the other
section survives.

Consequently: typing changes no geometry or placement, publishes no revision and
uploads nothing; switching display units makes **no native call at all**, being a
decimal point shift on text applied to size and position and never to rotation,
which is degrees in every unit; and the UI never clamps, rounds or repairs an
invalid value — it refuses only input it can already name a problem with (blank,
non-numeric, and non-positive *for a dimension only*) so it can report a useful
message, and everything else goes to native validation, the final authority. The
positivity rule is deliberately not applied to placement, where zero and negative
are ordinary.

`LengthUnit` converts by exact `BigDecimal` point shift, never a floating-point
multiply, so switching is lossless and a value re-entered in another unit produces
the identical `double` — which is why re-applying the same box in a different unit
reports `Unchanged`. Parsing accepts `.` or `,`; fields use a numeric IME whose
key listener accepts `0123456789.,-`, and the minus matters twice over: a negative
coordinate or angle is ordinary, and a negative *dimension* must be enterable so
it can be visibly refused rather than unreachable.

### Start flow ownership

`StartChooserView` asks which representation the model begins in, over the live
viewport. It owns no state and makes no native call; it reports which option was
pressed.

**Native state already exists when it is asked.** The Activity starts native code
before building any view, so the scene, the default Body and its ObjectId are the
same in both branches — neither answer creates anything, and startup is still one
default Box at identity (`S17-01`). *Construction* therefore only stops asking and
re-reads. *Sculpt* makes exactly the two calls a user would make by hand:
`applyConstructionSphere` with the diameter **read back from native state** rather
than a constant invented in Java, then `freezeToSculpt`. Nothing about Freeze is
duplicated, so *Back to Construction* finds the exact sphere and *Resume Sculpt*
returns the same frozen mesh for the ordinary reasons. A refusal leaves the
product in Construction, unchanged, and says so.

### The destructive-act guard

Three mode transitions live in the Global Toolbar and exactly one is on screen:
*Freeze to Sculpt* while no Frozen Sculpt Mesh exists, *Resume Sculpt* once one
does, *Back to Construction* while sculpting. None is guarded, and none needs to
be: the first can discard nothing, and the second and third discard nothing.

The one irreversible act in the product is **re-Freeze**, which rebuilds the
sculpt mesh from the current Construction shape and throws away what was
sculpted into the old one. It lives in the Sculpt inspector as *Freeze again…*
and it confirms **only when `SCULPT_HAS_EDITS` says the CURRENT frozen mesh has
edits** — deliberately not the session-lifetime stroke count, which describes
meshes that no longer exist and made every later re-Freeze of an untouched mesh
raise a dialog with nothing behind it. Cancel makes no native call at all.

### Android UI verification boundary

The native self-tests own geometry and math; the runtime smoke owns the
Vulkan/input bridge; the Android suites own shell behaviour, control visibility
and the UI→native call contract. **No Java test asserts a rendered pixel**, every
control is reached by its stable semantic id from `res/values/ids.xml`, and no
assertion depends on a screen coordinate. `src/test` is JVM/JUnit only, over the
pieces deliberately free of Android types; `src/androidTest` is instrumentation
with Espresso deliberately absent — the assertions are view state, measured
geometry and touch consumption read directly from the view tree.

Two techniques carry most of the weight. "A UI action changed nothing" is asserted
by comparing a **bit-identical** snapshot of the native primitive, transform and
sculpt arrays across the action. "Chrome does not leak a gesture" is asserted by
dispatching a drag to the surface and requiring it to return `true`, plus the
native sculpt revision and stroke count being unchanged. The camera has no
read-back across JNI, so camera immobility is proven at runtime instead, by a
pixel-identical viewport region across a chrome drag.

An **adaptive** case reads the window it is actually in and asserts the contract
belonging to that window, so running the suite under an overridden window size is
a genuine expanded-layout run rather than a simulation.

## JNI boundary

`NativeViewport` is the entire boundary, in six groups. **Lifecycle**: `start`,
`surfaceCreated`, `surfaceChanged`, `surfaceDestroyed` (blocks until the
`ANativeWindow` is released), `stop`. **Input**: one `touchEvent` carrying the
masked action, the lifting pointer's id, and per pointer a stable id, view-local
pixels, a neutral tool type, a pressure and a tilt -- see *Pointer semantics*
below. Plus one DEBUG-only reader, `debugLastPointerEvent`, which exists so a
test can observe what actually crossed; it is compiled out of a release build.
**Construction**: `constructionPrimitive` / `boxTransform` to read, one
`applyConstruction*` per primitive plus `applyBoxTransform` to submit.
**Scene**: `sceneBodyCount`, `sceneBodyIds`, `sceneActiveBodyId`,
`sceneSelectBody`, `sceneAddBody`. **Sculpt**: `productMode`, `freezeToSculpt`,
`enterConstructionMode`, `enterSculptMode` (resume WITHOUT re-freezing),
`sculptState`, `setSculptBrush`, `setSculptTool`, `sculptTool`.
**Presentation**: `setShadingModel`, `setSurfaceShading`, `setProjectionMode`,
`setViewportBackground`, `setGridVisible`, `setReducedMotion` and their readers —
every one of which requests a value, refuses an index it does not recognise, and
returns what is actually in effect. `setGridVisible` takes a plain `bool` rather
than an index, because there are two answers and no third to refuse.

The methods are listed by group rather than one by one because the *shape* is the
contract and an exhaustive list only drifts. Reading and writing mirror each
other: read the whole section, or submit the whole section. The shape read reports
**every** primitive's parameters plus the active kind, so the panel can populate an
inactive draft without inventing defaults, and each slot of that array has one
fixed meaning whatever the active kind is.

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

Below the boundary all six shape methods build the matching typed
`PrimitiveSpec` and go straight into `applyConstructionPrimitive`, so the
per-kind split is a boundary shape only: the update-then-publish rule still has
exactly one implementation. The JNI layer adds logging and the status code and
nothing else — it does not decide validity, does not decide whether to publish,
and never touches `MeshStore` or the transform. The DEBUG primitive driver calls
the same function.

The transform path reuses the same `APPLY_*` vocabulary.
`APPLY_REJECTED_NOT_POSITIVE` simply cannot occur there, because zero and
negative are ordinary coordinates and angles.

### Pointer semantics

One JNI call carries one complete `MotionEvent`, never one call per pointer, and
never more than `kMaxTrackedPointers` (6) of them. `forgeshape_jni.cpp`
translates Android's masked action constants into `forgeshape::TouchAction`;
unrecognised actions (hover, scroll, button) are dropped rather than forwarded,
which is why hover has no representation below the boundary yet.

**A `TouchPointer` carries what a stylus reports, in ForgeShape's own
vocabulary.** Beyond the stable id and view-local pixels it carries:

| field | meaning | units / range | fallback |
| --- | --- | --- | --- |
| `toolType` | `PointerToolType`: Unknown, Finger, Stylus, Eraser, Mouse | closed enum, ForgeShape's own wire codes | `Unknown` -- an ordinary contact pointer, never a dropped event |
| `pressure` | normalised contact force | `[0, 1]`, 1 = the device's full force | `1.0` (full contact) when non-finite or unreported; clamped otherwise |
| `tiltRadians` | lean away from perpendicular | radians, `[0, pi/2]`; 0 = straight up | `0` when non-finite; clamped otherwise |
| `tiltOrientationRadians` | which way it leans, in the screen plane | radians, `(-pi, pi]`; 0 = screen -y | `0` when non-finite or when there is no tilt; **wrapped**, not clamped, because it is periodic |

The two-angle tilt model is the smallest one that keeps direction. Android
reports exactly these two axes, and an Apple Pencil's altitude/azimuth converts
into them with arithmetic alone (`tilt = pi/2 - altitude`), which is what keeps
the contract portable without a portability layer. It is deliberately **not** a
full stylus pose: no barrel rotation, no hover distance, no button state.

**The mapping has one home each way.** Android's `MotionEvent.TOOL_TYPE_*`
constants reach exactly as far as `PointerSemantics` in the Android layer, which
turns them into wire codes; an unrecognised code -- including one a future
Android release invents -- becomes `Unknown` rather than being guessed at.
Ranges and non-finite fallbacks are owned once, natively, in `forgeshape_input.h`
and applied in `forgeshape_jni.cpp`, so the two sides cannot disagree and no NaN
can reach a domain consumer. The stylus arrays are individually optional: a
caller with nothing to say about tilt passes null and every pointer keeps its
documented default, which is also why the pre-stylus behaviour is exactly the
default behaviour.

**Carried is not consumed.** No brush, camera or selection rule reads
`toolType`, `pressure` or tilt. A stylus and a finger tracing the same pixels
produce bit-identical geometry, an eraser switches no tool, and a mouse gets no
wheel, hover or context behaviour. That is asserted, not assumed: the sculpt
suite drives the same stroke path at both ends of the pressure range for all four
tools and compares vertices bit-exactly. Making pressure *mean* something is a
Sculpt stage's work, and it will have to change the brush kernel to do it.

## Camera ownership

`CameraController` (`forgeshape_camera.{h,cpp}`) is the single source of camera
truth and contains no JNI, Android or Vulkan types. It owns `target` (orbit
centre), `yaw`, `pitch`, `distance`; the **projection mode and the orthographic
world span**; FOV, near and far planes; viewport width/height and therefore
projection aspect; and the gesture state machine (mode, tracked pointer ids,
anchors). It produces a `CameraSnapshot` — view matrix, projection matrix, eye,
target, pose scalars, the active `ProjectionMode` and the visible half-height —
which is the only thing the renderer, picking and sculpt ever see.

Gesture rule: every touch event recomputes the set of pointers that are still
down, sorted by pointer id. If that set differs from the tracked one, the
controller re-anchors and applies **no** delta; deltas are applied only on Move
events whose pointer set is unchanged. This is what makes 1↔2 pointer transitions
and MotionEvent index reordering jump-free.

### Projection mode

`ProjectionMode` is a closed enum with two members — `Perspective` (the product
default) and `Orthographic` — and a switch, the same rule the sculpt tools and
the shading models follow. There is no camera framework and no projection
registry.

It is **camera/presentation state, never geometry truth**. Changing it mints no
`MeshRevision` and no `SculptRevision`, moves no vertex, and touches no
Construction parameter, `PrimitiveKind`, transform or `ObjectId`. It changes
which pixels a fixed piece of geometry lands on, and nothing else. Like the
camera pose and the display settings it is process-scoped, which is why it
survives HOME/resume and Surface recreation with no save/restore code in the
Android layer.

| | Perspective | Orthographic |
| --- | --- | --- |
| Matrix | `mat4Perspective(kFovYRadians, aspect, near, far)` | `mat4Orthographic(halfHeight, aspect, near, far)` |
| Field of view | 60° vertical (`kFovYRadians`), unchanged by this stage | not applicable |
| Visible scale set by | `distance` | `orthoHalfHeightMeters` |
| `proj.m[11]` | `-1` — divides by depth | `0` — **w is 1 everywhere**, a true parallel projection |
| Depth | `[0, 1]`, non-linear | `[0, 1]`, linear |
| `snapshot.eye` | the pinhole, `target + dir × distance` | the **view-plane centre**, `target + dir × kOrthoViewPlaneDistance` |
| Pinch changes | `distance`, clamped to `[0.35, 400] m` | `orthoHalfHeightMeters`, clamped to `[0.02, 250] m` |

**The orthographic scale is a world length, not a zoom factor.**
`orthoHalfHeightMeters` is half the world-space height the viewport shows, in
meters, measured at the target plane — so it can be reasoned about against an
exact Construction dimension rather than against an abstract multiplier. The
snapshot carries it in **both** modes: in Perspective it is the equivalent
framing `distance × tan(fovY / 2)`, so the field is always a live, physically
interpretable span rather than a stale leftover.

**Switching preserves the framing at the target plane**, converting between the
two descriptions rather than resetting:

```
Perspective -> Orthographic:  orthoHalfHeight = distance * tan(fovY / 2)
Orthographic -> Perspective:  distance        = orthoHalfHeight / tan(fovY / 2)
```

These are one identity read in opposite directions, so a round trip returns to
where it started (up to the distance clamps). The target, yaw and pitch are never
touched, so the frame keeps its centre and its viewing direction and the object
can neither jump nor vanish.

**Why the orthographic eye is pulled back.** A parallel projection produces the
same image from anywhere on the view axis, so the view plane's distance is free,
and `kOrthoViewPlaneDistance` (= `kFarPlane / 2`, 250 m) spends that freedom on
centring the `[near, far]` slab on the target: nothing framable is sliced by the
near plane, and every drawn surface lies in front of the pick-ray origin, so
*what is pickable stays what is drawn*. Ortho depth is linear, so a 500 m slab
costs no precision worth naming. The consequence to know: `snapshot.eye` is
**not** `target + dir × distance` in Orthographic, and a reported pick distance
is measured from that view plane rather than from the orbit eye.

**Pinch must not fake orthographic zoom with distance**, which would change
nothing on screen and read as a dead gesture. Pinch changes the visible span; the
orbit distance is left alone, because it is still the pose radius and still what
a switch back to Perspective is computed from. Pan is scaled by whichever
quantity is active, so "one pixel of finger is one pixel of world at the target
plane" holds identically in both modes.

### Screen ray, per projection

`buildPickRay` handles both, and the difference is structural rather than a
tweak to a constant:

| | origin | direction |
| --- | --- | --- |
| Perspective | one point — the eye | depends on the pixel; the rays fan out |
| Orthographic | depends on the pixel; slides across the view plane | one shared direction — the view axis |

Both are inverted out of `camera.proj` and `camera.view`; neither restates a
field of view, an orthographic span or an aspect. Picking keeping a perspective
origin under an orthographic image would agree with the picture only at the
screen centre and drift further from it toward every edge — which is why the
picking suite probes off-centre pixels and round-trips each hit back through the
same matrices to the pixel it came from.

The same split governs the sculpt brush. `worldPerPixelAtDepth` reads
`proj.m[5]` in both modes, but multiplies by the hit depth only in Perspective:
a parallel view does not open with distance, so the orthographic brush covers the
same amount of surface at every depth. `CAMPROJ-11` measures both halves — that
the orthographic radius does *not* move when the object is pushed along the view
axis, and that the perspective one does.

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

**The Plane is a bounded, explicitly named exception to both halves of this
rule**, never a change to the rule itself. It is the one primitive with no
"inside": a flat, open, single-sided sheet has no far wall a two-sided render
or pick could wrongly reach, unlike every other, closed primitive above.

**Sidedness is a property of the active published representation, and exactly
one value carries it.** The chain is:

```
ConstructionMesh::renderBothSides      (set only when generating a Plane)
  -> SculptMesh::renderBothSides()     (copied wholesale by freezeFrom)
    -> RuntimeMesh::renderBothSides()  (carried by BOTH publication paths:
                                        publishConstructionObject and
                                        publishSculptMesh)
```

and the three consumers all read the last link and nothing else:

| Consumer | Reads |
| --- | --- |
| Render (`buildRenderMesh`) | `RuntimeMesh::renderBothSides()`, duplicating its one-sided result — see *Derived render geometry and shading* |
| Selection (`pickScene`) | `meshStore().current()->renderBothSides()` |
| Sculpt hit-test (`SculptStroke::begin`, `SculptSession::hitsSculptMesh`) | the `SculptMesh`'s own `renderBothSides()` |

**No consumer may re-derive this from `PrimitiveKind`.** A Frozen Sculpt Mesh
outlives the Source it was frozen from, so the Source can be a Plane while the
frozen geometry is a closed solid, and the reverse — asking the Source is wrong
in both directions, and in neither case has the geometry changed at all.
`forgeshape_selection.cpp` deliberately does not include
`forgeshape_construction.h`, so the dependency that permits the mistake is absent
rather than merely unused. `SIDE-01`..`09` assert the chain, both stale-source
directions included. The pipeline is untouched: one global
`VK_CULL_MODE_BACK_BIT` / `VK_FRONT_FACE_COUNTER_CLOCKWISE` for every primitive
including the Plane.

**Naming this `VK_FRONT_FACE_CLOCKWISE` double-counts the Y flip** and inverts
culling. It does not blank the viewport: a closed solid keeps its silhouette but
draws its far walls, so every convex primitive reads as a concave interior — and
the renderer silently splits from the picker, which stays correct.

## Construction domain

`forgeshape_construction.{h,cpp}` owns the product geometry. It contains no JNI,
Android, Vulkan, renderer or UI type, and it holds no GPU resource.

### The scene, and the active body

`forgeshape_scene.{h,cpp}` owns the collection. `ConstructionScene` holds an
ordered vector of `SceneObject`, one per Construction Body, plus the id of the
active one. Platform-neutral C++17 like the rest of the domain: no JNI, no
Android, no Vulkan, no renderer type.

A `SceneObject` owns exactly three things, and they are precisely the things
that used to be process-global singletons:

| Owned per body | Why |
| --- | --- |
| `ConstructionObject` | the exact primitive, its parameters, and (through it) its `ConstructionTransform` |
| `MeshStore` | its own publication chain, so revisions are per body and A's edit cannot replace B's mesh |
| `FrozenSculpt` | its Frozen Sculpt Mesh and its stale-source flag |

Bodies are held by `unique_ptr`, so their addresses are stable as the
collection grows — a `SceneObject` owns a mutex through `MeshStore` and must
not move. `SceneObject` is non-copyable: duplicating one would duplicate
identity, which is the one thing an `ObjectId` exists to prevent.

**`constructionObject()`, `meshStore()` and `sculptSession()` still exist under
their original names but now mean "the ACTIVE body's".** They are defined in
`forgeshape_scene.cpp` rather than beside their own types, because their answer
is a scene question and defining them in their own translation units would make
those units depend on the scene, which depends on them. Keeping the names is
what made the multi-object migration small: every caller that means "the object
the user is editing" is still correct unchanged, and only code that means
"every body in the scene" — the renderer and scene picking — was rewritten.

**`ObjectId` allocation.** Monotonic, minted by the scene, never reused, and
never derived from a collection index, a `MeshRevision` or a GPU resource. It
survives primitive edits, transform edits, Freeze/Resume/re-Freeze and
selection. The first body keeps `kConstructionBoxObjectId`, so startup is
identical to the single-object product's. There is no delete, and therefore
deliberately no reuse policy.

**The collection is flat.** Root-level bodies in insertion order, with stable
enumeration. There is no parent, no child, no group, no reorder and no
speculative field for any of them.

### The scene snapshot

`ConstructionScene::snapshot()` is the one thing the renderer and CPU picking
consume. Per body it carries an `ObjectId`, a `RuntimeMeshPtr`, a model and an
inverse-model matrix, and a selection flag — a `shared_ptr` copy and two
matrices, and **no geometry**.

That cheapness is the point. A snapshot is taken under the existing state mutex
and then used with that mutex released, so no lock is ever held across normal
generation, a GPU upload or a triangle scan. Because each item holds a
`shared_ptr` to the exact revision it names, an older snapshot stays valid and
self-consistent even while newer revisions are published, and a body with
nothing published yet is simply absent rather than a null to guard against.

**Lock order is unchanged**: the one state mutex, then `MeshStore`'s own mutex
inside `publish`/`current`. `ConstructionScene` is deliberately *not* internally
synchronised — adding a second scene-level lock would have introduced a new
ordering to get wrong, for a collection whose callers already hold the state
mutex the singletons required.

### The one active body's object

`ConstructionObject` is the single owner of everything that makes one body what
it is:

- a stable `ObjectId`;
- a `PrimitiveKind` — `Box`, `Cylinder`, `Sphere`, `Cone`, `Capsule` or
  `Plane` — saying which primitive is **active**;
- a `ConstructionBox`, a `ConstructionCylinder`, a `ConstructionSphere`, a
  `ConstructionCone`, a `ConstructionCapsule` and a `ConstructionPlane`, each
  holding its own exact parameters;
- a `ConstructionTransform` holding its placement.

One per Construction Body, owned by that body's `SceneObject`. It is still not
a registry — it contains no container, no list, no parent and no child; the
collection is the scene's job, above it.

`constructionTransform()` returns `constructionObject().transform()` rather than
owning a singleton of its own, so identity, shape and placement are one body's
state and cannot drift apart.

**Every** primitive's parameters are retained across a kind change. Switching
Box → Cylinder → Sphere → Box does not silently forget the box's dimensions; only
`kind` decides which set is authoritative for the geometry right now. That is
also what lets the panel show an inactive primitive's remembered values without
inventing defaults of its own.

### The typed primitive payload

A primitive *request* is a `PrimitiveSpec`, and its payload is a
`std::variant<BoxDimensionsMeters, CylinderDimensionsMeters,
SphereDimensionsMeters, ConeDimensionsMeters, CapsuleDimensionsMeters,
PlaneDimensionsMeters>` — a real tagged union, not a struct carrying all six
groups at once. A spec built for a cylinder physically does not contain box or
sphere values, so there is no "inactive parameter group" riding along beside
the active one and nothing for a caller to read by mistake.

A cone's `(diameter, height)` and a capsule's `(diameter, totalHeight)` are the
same two numbers in the same order meaning different things; the payload makes
reading one as the other impossible rather than merely discouraged.

`kind()` is **derived** from the payload's alternative index rather than stored
beside it, so tag and data cannot disagree; a `static_assert` pins the variant's
alternative order to `PrimitiveKind`'s. Access is typed — `box()` … `plane()`
each return a pointer that is null unless the spec really is that primitive —
and specs are built only through the matching `forBox` … `forPlane`.
`ConstructionObject::spec()` therefore returns only the **active** primitive's
parameters; the remembered parameters of inactive ones are reachable only by
asking for that primitive by name, which is what the panel's draft display does.

**Update dispatch is typed, not stringly or if-else.** Both the "changed?" check
and the write `std::visit` the payload against a private per-kind overload set
(`parametersDiffer` / `writeParameters`) rather than an if-else chain. A
`PrimitiveSpec` alternative with no matching overload fails to **compile** — the
same guarantee `validateParameters` has — so a seventh kind added to the variant
without its overloads cannot ship.

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
| Plane | width (local X), depth (local Z) | lies in the local XZ plane, `y = 0`, front `+Y`, centred | none — a fixed 4-vertex, 2-triangle rectangle | 4 : 6 |

Every row above but the last is a **closed solid**; the Plane is the one
open, single-sided sheet, and it is called out on its own after the shared
invariants below rather than folded into them.

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

Four cases need more than the table says:

**The cone's base is closed** and its apex is a true point — apex radius is zero
by definition and is not a parameter, because a truncated cone is a different
shape rather than a cone with an extra number. The side triangle is the
cylinder's `(b0, t1, b1)` with the top ring collapsed to the apex; the
`(b0, t0, t1)` partner that would have been degenerate simply does not exist. The
base fan is the cylinder's bottom cap unchanged, and `V - E + F = 2` is what
would fail if the base were left open.

**The capsule's cylindrical middle is not a special case in the index loop.** The
middle is just the band between the two seam rings, generated by the same code as
every other band. It is centred on the origin, so its ends sit at
`±(totalHeight-diameter)/2`, and the poles are written down from the
authoritative **total** height rather than accumulated as `middle/2 + radius`.

**The capsule equality case is a sphere, and is generated as one.**
`totalHeight == diameter` describes a capsule with no middle: the two seam rings
are **the same ring**, `R` is one smaller, that band does not exist, and the
result is exactly the sphere's 482 : 2880 — pinned by a `static_assert` rather
than left to coincidence, which is what keeps the degenerate case free of a
zero-length ring and zero-area triangles. Capsule topology is therefore a
deterministic *function* of the parameters (the two sides of one exact `double`
comparison, nothing in between), which makes the capsule the one primitive whose
edit can change a vertex count and so trigger a buffer growth.

**The Plane has no tessellation constant and no relation to validate.** Source
topology is always exactly 4 vertices and 2 triangles —
`kPlaneIndices = {0, 3, 2, 2, 1, 0}` over corners `(∓w/2, 0, ∓d/2)` — CCW seen
from `+Y`, so both triangles' geometric normals are exactly `(0, 1, 0)`, not
merely outward-ish. Width and depth are validated independently by the same
`validateDimensionMeters` every other per-length field uses; there is no
capsule-style cross-field rule because the two extents do not constrain each
other. Being the one open, single-sided primitive, it is also the one whose
`ConstructionMesh` sets `renderBothSides = true` — see *Derived render geometry
and shading* and *Picking and selection* for what that authorizes. It is
additionally the one primitive that can sit exactly coplanar with the world
reference grid, which is why the grid carries a depth nudge; nothing about the
Plane changes for it.

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

Callers therefore decide nothing. The product UI path (for all six primitives)
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
placement exactly as it was, including across any switch among the six kinds.

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

`forgeshape_sculpt.{h,cpp}` owns each body's second representation and the four
tools that edit it. Like the Construction domain it contains no JNI, Android,
Vulkan, renderer or UI type, and it holds no GPU resource.

### What is per body, and what is the session

This split is load-bearing and was got wrong once, so it is stated explicitly.

| Per body — `FrozenSculpt`, owned by `SceneObject` | Session-wide — one `SculptSession` |
| --- | --- |
| the Frozen Sculpt Mesh (`SculptMesh`) | the product mode (Construction / Sculpt) |
| its stale-source flag | the held tool |
| — | brush radius and strength |
| — | the stroke in progress, and the session-lifetime stroke count |

`sculptSession()` re-points the one session at the **active body's**
`FrozenSculpt` on every access. It is a single pointer write, and rebinding
every time rather than only on a selection change removes the whole class of bug
where the session is left pointing at the body the user just navigated away from.

Making `SculptSession` itself per body — the obvious first move — is wrong twice
over. The product mode becomes ambiguous: `productMode()` would answer for
whichever body is active, so selecting a body that was itself left in Sculpt mode
would refuse every later selection. And **"Radius and Strength are shared" is a
product contract** — switching bodies must no more change the brush than
switching tools does — which a per-body copy breaks silently.

Body switching is refused while in Sculpt mode. The Sculpt target is fixed for
the duration of the mode and the user returns to Construction to change bodies,
which avoids having to decide what a body switch does to a half-finished stroke.

### Two representations, one body

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
- **A sculpt edit changes no parameter, no kind and no transform.** It cannot: the
  sculpt module has no mutable access to `ConstructionObject`, and the only
  mutation `SculptMesh` offers is a single vertex POSITION — nothing here can add,
  remove or reorder a vertex or touch an index, so topology is preserved by
  construction rather than by discipline. **Nothing reconstructs a parameter from
  sculpt vertices** either; that direction does not exist.
- The transform is deliberately **not** duplicated. Both representations are local
  geometry under the one `ConstructionTransform`, so there is no second placement
  to keep in sync.
- `SculptRevision` is emphatically **not** a `MeshRevision`. `MeshStore`'s
  revisions count every published snapshot of whichever representation is active;
  a `SculptRevision` counts changes to *this* mesh and restarts at 1 on every
  Freeze. Neither is derived from the other and they cannot be compared.

### Mode ownership

`SculptSession` owns `ProductMode` — `Construction` or `Sculpt` — plus the one
`SculptMesh`, the active `SculptTool`, the brush settings and the live stroke, and
is process-scoped like the camera, the selection and the mesh store.

The mode is **native state**. The Android UI may request a change and is then told
what the mode actually is; `syncFromNative()` reads `productMode()` back rather
than assuming its request succeeded, so a refused request (entering Sculpt with
nothing frozen) cannot leave surfaces on screen that lie about what is being
edited. The active tool is read back the same way.

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
and without the dedup every interior neighbour would be weighted double in a mean.
Out-of-range indices are skipped rather than trusted and no vertex is ever its own
neighbour. It is deliberately **not** a half-edge structure: half-edges are the
right shape for *changing* topology, and nothing here can change topology.

`computeVertexNormals` produces **area-weighted** normals from the CURRENT
positions: each triangle contributes its unnormalized `(v1-v0) x (v2-v0)` — the
same canonical outward normal picking uses, whose length is twice the area — to
all three corners, then each vertex normal is normalized. A degenerate triangle
contributes a zero vector rather than a NaN, and a vertex with no usable
accumulated direction gets the **zero** vector rather than an invented axis, so a
normal-based brush simply does not move it.

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
  snapshot's own projection term (`proj.m[5]`, which is `-1/tan(fovY/2)` in
  Perspective and `-1/orthoHalfHeight` in Orthographic); nothing here restates
  `kFovYRadians`, the orthographic span or the aspect. The depth factor applies
  in Perspective only — see *Screen ray, per projection*.
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

The other three are **path-driven**. Per move,
`travelFraction = pointer travel this move / brush radius` (both in pixels), and

```
amount = strength * localRadius * kNormalBrushGain * travelFraction   (Clay, Inflate)
lambda = strength * weight * kSmoothGain * travelFraction             (Smooth)
```

Measuring path length rather than counting events makes them independent of the
event rate: the same finger path deposits the same amount whether Android
delivered it in five events or fifty, a stationary finger does nothing, and there
is no timer and no per-event dab. `amount` is clamped to one local radius per
move, so a teleporting pointer cannot produce an unbounded displacement.
`kNormalBrushGain` (0.35), `kSmoothGain` (1.0) and `kMaxSmoothLambda` (0.9) are
chosen defaults, not derived constants; the low gain is why a short stroke with a
large brush reads as doing very little.

**Clay is deposition and Inflate is expansion**, and the difference is the
formula, not a constant. Clay reads `baseNormal`, captured once on Down and held
fixed exactly as the affected set and weights are, so it lays a coherent slab in
one consistent set of directions. Inflate reads `mesh.vertexNormals()`,
recomputed from CURRENT positions every move, so on a forming bulge the flank
normals have tilted outward and it widens and rounds instead of extruding —
directly observable, and asserted by self-tests with thresholds three orders of
magnitude apart.

**Smooth is bounded interpolation, never extrapolation** —
`p := p + (neighbourAverage - p) * lambda`, `0 < lambda <= kMaxSmoothLambda` — so
a vertex can only move part of the way toward a point it is already surrounded
by. That is what makes repeated smoothing converge (deviation shrinking by
`(1 - lambda)` per application, never changing sign) and why no clamp on the
result is needed. Targets are computed from a coherent snapshot **before**
anything is written (Jacobi, not Gauss-Seidel), so the outcome cannot depend on
the affected set's order; only affected vertices are written. Inflate snapshots
its normals the same way and for the same reason.

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
or advance the stroke counter — the structural half of the guarantee that a
gesture which turns out to be navigation cannot have mutated the sculpt mesh.

Anchoring the promoted stroke at the original down point makes the deferral free:
the hit, the affected set and the weights are exactly what they would have been
had the stroke begun on Down. The camera re-anchors on any pointer-set change so
an abandoned gesture produces no jump, and the selection never saw the Down. While
a stroke owns the gesture neither controller sees the event at all, which is what
makes a brush gesture structurally unable to orbit, select or clear.

The 8 px threshold sits between touch jitter and the 24 px tap slop, and the
residual it accepts is stated rather than hidden: a first finger that
*deliberately drags more than 8 px* before the second lands does commit a stroke.
`g_grabbing` and `g_strokePending` in `forgeshape_jni.cpp` are gesture routing
only, guarded by `g_stateMutex`, and are dropped whenever the Surface goes away;
the mode, the active tool and the sculpted vertices are not, being process-scoped.

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

The measured per-primitive counts live in `PROJECT_STATUS.md`'s shading cost
record and are not restated here. Two shapes of result matter architecturally: a
fully smooth closed surface has no crease to split on, so its render mesh *is* its
source topology — the cheapest available proof that the grouping does not fragment
a smooth surface — and the Plane's doubled count is the **other** reason a render
count can exceed a source one, not a crease split but the bounded two-sided
exception below.

### The two-sided render exception

`buildRenderMesh` takes an optional `renderBothSides` argument
(`RenderMeshCache::refresh` reads it straight off `RuntimeMesh::
renderBothSides()`, so the renderer itself never branches on `PrimitiveKind`).
When true — today, only for a Plane — the ordinary one-sided Smooth or Faceted
result is duplicated once more by `appendMirroredBackFace`: every render
vertex is repeated with its normal negated, every triangle is repeated with
reversed winding, on the duplicated vertex set. Nothing about the source
`RuntimeMesh` changes and nothing is re-validated against a different rule;
this runs entirely on already-built render data.

The reason this needs no pipeline, culling or material change is the winding
reversal itself: from the front, the duplicate is the **back**-facing triangle and
the existing `VK_CULL_MODE_BACK_BIT` culls it; from the far side the original is
back-facing and culled, leaving the duplicate, whose negated normal is correct for
a viewer there. One global pipeline keeps drawing exactly one of the two triangles
at any position, for every primitive — the exception is entirely in what data
reaches the pipeline, not in the pipeline itself.

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

Normals are area-weighted (`(v1-v0) x (v2-v0)`), the same weighting
`computeVertexNormals` uses for the sculpt cache, so a surface does not change
character between the two paths. A degenerate triangle contributes the zero
vector rather than a NaN, and a group with no usable length keeps the zero
normal — honest, and handled by a documented shader fallback. Faceted shading is
the other branch: three private vertices per triangle carrying that triangle's
flat normal, intentionally exposing triangle structure.

### Rebuild policy

`RenderMeshCache` rebuilds when — and only when — the source `MeshRevision` or
the `SurfaceShading` changed. It does **not** rebuild for a camera move, a
rotation, a window resize, a unit switch, an inspector toggle, a mode or tool
change, or a Studio<->MatCap change, because that last one is a fragment-stage
uniform touching no geometry at all. The renderer gates on the same pair before
calling in, so a steady frame costs two integer comparisons.

The measured frame-to-rebuild ratio and the per-primitive rebuild costs live in
`PROJECT_STATUS.md`'s shading cost record, which owns runtime evidence.

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

Both fills are placed lower-**front**, not opposite the key: a fill opposite the
key lifts exactly the planes the key leaves dark, so a box's left and right faces
end up nearly the same value and the form stops reading.

There is no PBR here and none is implied: no metalness, no roughness, no
environment probe, no shadow map, no ambient occlusion and no tone-mapping stack.

### The MatCap asset

`forgeshape_matcap.{h,cpp}` **computes** the one 128x128 RGBA8 preset at device
initialization from a closed-form model in that file. No image in the repository,
nothing downloaded, nothing derived from another application's asset — and it is
the only option that respects the no-third-party-library rule, since decoding a
PNG would need a decoder ForgeShape may not depend on. Texels outside the unit
disc are clamped to the rim value in the same direction, and the sampler uses
`CLAMP_TO_EDGE`, so filtering at a silhouette does not bleed.

Exactly one preset. No library, no browser, no import, no per-object material.

### Display settings ownership

`forgeshape_display.{h,cpp}` holds the shading model, the surface shading and the
**viewport background** as process-scoped atomics. Native owns them exactly as it
owns the product mode and the active tool; the Android UI may request a change and
read the value back, but does not hold it — which is why they survive HOME/resume
with no save/restore code in the Android layer. A snapshot is pushed into the
renderer per frame, outside the state mutex, because no domain invariant depends
on it and a frame must never wait on the geometry lock to learn which shading
model to draw with.

**The viewport background is the whole of what a theme means below JNI.** It is a
closed `ViewportBackground` enum — `NeutralDark`, `WarmLight` — and what crosses
JNI is its index, refused if unrecognised, exactly like a shading model. No
Android theme, no style, no Android type and no RGB authored above JNI reaches
this layer: native code owns what each appearance looks like, so the geometry
domain never learns that themes exist. `viewportBackgroundColor` is read when the
render pass records its clear value, which happens every frame anyway, so a switch
rebuilds no geometry, mints no revision, re-uploads nothing and does not touch the
swapchain, the pipeline, the descriptor set or any buffer. The two float triples
are duplicated in `colors.xml` as the Android *window* background — what covers
the moment before the surface has content — and `display_dark_background_is_0e121b`
and `display_light_background_is_e6e1d9` pin them so the pair cannot drift into a
launch flash.

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

`pickScene` takes a `SceneSnapshot` and intersects EVERY body in it — never
Vulkan buffer memory — keeping the nearest positive hit and taking the object id
from the published mesh rather than from the geometry. Each item is intersected
with ITS OWN model/inverse-model pair and ITS OWN sidedness, so what is pickable
is each body where it actually appears. Distances are directly comparable
between bodies because every Construction transform is rigid, which is what
makes "nearest wins" meaningful across the scene; ties keep the earlier body in
scene order, so the result is deterministic rather than an iteration accident.
Because each item is whichever representation that body has published,
generated from current parameters or sculpted, picking automatically follows an
edit with no separate collision representation to keep in sync. Picking is a
linear scan over bodies and over triangles; there is no spatial acceleration.

Taking the snapshot as a parameter is deliberate: it lets a self-test pick a
scene it built itself, and it lets the caller take the snapshot under the state
mutex and then scan triangles with that mutex released.

**The two-sided picking exception is one boolean, computed once per item.** The
process-scoped `pickScene` reads the sidedness of the mesh it is about to
intersect and passes its negation as `frontFacesOnly` to the explicit-transform
overload, which forwards it to `pickTriangleMesh`. That argument defaults to
`true`, so every other caller — including the self-tests, which use the explicit
overload precisely to avoid depending on process-scoped state — is unaffected.

This reads the **published mesh**, never `constructionObject().kind()`: a
Construction edit after a Freeze leaves the frozen mesh untouched and stale by
design, so the Source and the active mesh can disagree about shape entirely, and
then the kind describes geometry that is not on screen. See *Canonical winding
and culling* for the single ownership chain and `SIDE-01`..`09`.

`SelectionController` (`forgeshape_selection.{h,cpp}`) owns the selected
`ObjectId` and the tap-versus-navigation decision. `ObjectId` is an opaque
`uint64_t` minted by the scene: never a pointer, list index, Vulkan/renderer
handle or display name, so it survives buffer recreation and Surface swaps.
`kNoObject == 0` means nothing is selected. Exactly one body is selected at a
time — no multi-select, no lasso or box selection, and no hierarchy.

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
`setCamera(const CameraSnapshot&)` and `setScene(SceneSnapshot)` and consumes
both verbatim; it derives no camera pose, builds no rotation, owns no Euler
convention, interprets no pointer data and holds no geometry truth.

The mesh vertices are each body's **local** geometry and never move. Where a
body appears comes from its own model transform, where the viewer stands comes
from the camera snapshot, and the renderer only composes them as
`mvp = proj * view * model` — so a screen-space change is always attributable:
the camera moved, or that body did.

### Per-body GPU resources

`BodyRenderResources` holds everything the GPU keeps for **one** body: its
device-local vertex and index buffers and their capacities, its
`RenderMeshCache`, and which revision and surface shading those buffers
currently hold. They live in a map keyed by **stable `ObjectId`** — never by
scene index, which would silently rebind a body's buffers to a different body if
the collection were ever reordered.

Staging, the upload command buffer and the upload fence stay **shared**: they
are transient scratch used inside one upload and waited on before the next, so
one copy is both correct and the smaller footprint.

`syncScene()` runs the existing per-frame gate once per body: if that body's
published revision and the surface shading both match what it already holds, it
returns without generating a normal, allocating anything or touching a buffer.
**This is what makes body independence structural rather than a promise** — an
edit to A mints a revision in A's own `MeshStore`, so B's cached revision still
equals B's published revision and B's branch returns immediately. Camera motion,
rotation, a selection change and a Studio↔MatCap switch all land in that gate
and stop, for every body.

`recordBodyDraw` then issues one draw per body, with that body's own model
matrix, its own buffers and its own selection flag. Selection reaches the
fragment shader as a tint push constant **per draw**; there is deliberately no
renderer-wide selection bool, because a single one would tint every body at once
as soon as anything was picked. The renderer is still never told *which* object
is selected — the snapshot carries a plain bool per item and identity stays with
`SelectionController`.

`FORGESHAPE_RENDER_MESH_BUILD` and `FORGESHAPE_MESH_UPLOAD_OK` carry a `body=`
field, appended so the historical prefix keeps parsing. With several bodies an
upload line is otherwise ambiguous about which one it describes, and that
ambiguity would destroy the only direct evidence for "editing A did not rebuild
or re-upload B".

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
112  vec4 selectionTint rgb = tint, a = mix amount (see Motion and selection feedback)
```

A `static_assert` pins the size. The shading model rides in an otherwise-dead
`w` component rather than taking a fifth 16-byte slot the budget does not have;
the next thing needing per-draw uniform data belongs in a descriptor, not here.
The selection pulse added no byte to this block: `selectionTint.a` was already
here, and what changed is only what is written into it.

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
reason a descriptor set exists — everything else still travels as push constants,
and the grid pipeline declares no set at all. One `COMBINED_IMAGE_SAMPLER` at set
0 binding 0, allocated once and never updated again because the image is immutable
for the life of the device. It is bound unconditionally even in Studio Solid,
since leaving a declared binding unbound is invalid usage regardless of which
branch runs. Image, view, sampler and set are device-scoped, so a HOME/resume
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
`imageExtent` in the display's *pre-transform* (panel) space — the transpose of
the window at 90° and 270° — plus a matching clip-space rotation and camera
aspect. Declaring `preTransform = caps.currentTransform` while passing the
window-space extent is the inconsistent combination: SurfaceFlinger rotates the
buffer into the transposed layout and stretches it back, an anisotropic scale.
That, not the projection, was the rotated-landscape defect.

The cost is one compositor rotation on a rotated display, which every
non-pre-rotated Android application already pays. Its consequence is that
`vkAcquireNextImageKHR` and `vkQueuePresentKHR` report `VK_SUBOPTIMAL_KHR` for as
long as the device is rotated — the surface's transform is genuinely not the one
the swapchain declared. **That is the convention working, not a stale swapchain,
so the frame loop must not rebuild on it:** `Renderer::expectSuboptimal_` records
when the declared pre-transform differs from the surface's own, and suboptimal is
ignored in exactly that case. Rebuilds still happen on
`VK_ERROR_OUT_OF_DATE_KHR` and on the explicit `requestResize()` that
`surfaceChanged` raises. Losing this distinction rebuilds the swapchain on every
single frame while rotated.

One non-per-frame `FORGESHAPE_SURFACE_CONFIG` line per swapchain creation records
the window size, `currentExtent`, `currentTransform`, `supportedTransforms`, the
chosen extent and the chosen `preTransform`, so the whole chain is auditable from
a log without adding instrumentation; `FORGESHAPE_CAMERA_VIEWPORT` is its
companion for the camera half.

### The world reference grid

The grid is a viewport **reference**, and the boundary is absolute: it has no
`ObjectId`, never enters `ConstructionScene` or a `SceneSnapshot`, is not a
`RuntimeMesh`, is invisible to picking, takes no part in Freeze, Resume or a
sculpt stroke, is not exported and is not a snap target. A future Sketch grid —
the one that snaps, drawn on a sketch plane — is a **different contract** with
its own approval and must not be grown out of this one.

Ownership splits in the usual place. `forgeshape_grid.{h,cpp}` is
platform-neutral and owns the *contract*: the plane (`y = 0`), the spacing (1 m
minor, 5 m major), the extent (20 m half), `GridLineTier`, one pure vertex
generator and the palette per `ViewportBackground`. `DisplaySettingsStore` owns
*visibility*, beside the shading model. The renderer owns only Vulkan.

**Generated once, never regenerated.** The vertex count is a compile-time
constant, so `createGridResources()` uploads 2624 bytes at device creation and is
never called again — no input can change, no cache to invalidate, no revision to
follow. Device-scoped like the MatCap, so a Surface swap or rotation does not
re-upload it, and `FORGESHAPE_GRID_UPLOAD_OK` appears once per device. Showing or
hiding the grid decides only whether **one** already-built `vkCmdDraw` is
recorded. The tier travels in the vertex buffer and the colour does not: the four
tier colours ride in a second 128-byte push block, so switching appearance
re-uploads nothing.

It has **its own pipeline and layout with `setLayoutCount = 0`**, because it
consults no sampler — no normal, no light, no shading model — and it is the **one
blended pipeline** in the renderer, which is not an invitation to reuse it as a
general overlay path. Depth is **tested, not written**, and it draws after every
body, so the model occludes it and it contributes nothing a later draw could be
occluded by. Three constraints are non-obvious enough that each was wrong once,
and all three are documented at their sites in `forgeshape_renderer.cpp` and
`shaders/grid.{vert,frag}`: `polygonMode` must be `FILL` under `LINE_LIST`; the
destination **alpha channel must be preserved**, or the compositor punches the
viewport translucent along every line; and the radial fade must be **per
fragment**, because a line's only two vertices are its fully-faded endpoints.

**The coplanar case is settled in the vertex shader.** A Construction Plane at
world `y = 0` shares the grid's plane exactly, so the grid is pushed `1e-4` of the
depth range further from the eye and loses every tie deterministically under
`VK_COMPARE_OP_LESS`. **Nothing in the domain moves for this.** It is in the
shader rather than in `VkPipelineRasterizationStateCreateInfo` because Vulkan's
depth bias is defined for **polygon** fragments and would have been inert on a
line list — reading as the fix while doing nothing.

## Motion and selection feedback

Both are **presentation**, and the ownership rule is the same one shading
follows: nothing here is truth, nothing mints a revision, and nothing may be read
back out.

**Selection feedback is renderer-owned and per body.**
`forgeshape_selection_pulse.{h,cpp}` owns the peak alpha, the resting alpha, the
decay and one pure function over an explicit frame delta. It holds no `ObjectId`,
reads no clock and touches no `MeshStore`, so a whole selection cycle beside a
published mesh leaves that mesh bit-identical *structurally* rather than by
promise. `Renderer::advanceSelectionFeedback` runs once per frame from
`drawFrame`, deliberately **outside** `syncScene`'s revision gate — a pulse has
to keep decaying on the frames where nothing was published, which is nearly all
of them — and writes one float into the `BodyRenderResources` entry that already
keys that body's GPU buffers by stable `ObjectId`. That keying is what makes A's
pulse structurally unable to reach B's. The renderer is still never told which
object is selected: `SceneDrawItem` carries a plain bool and identity stays with
`SelectionController`.

**Motion is a shared helper, not a system.** `ChromeMotion` (Java) owns exactly
four decisions — the two durations, the reduced-motion question, cancel-first, and
one alpha helper — and nothing may be added to it that describes motion as data.
Three rules constrain every caller. **Nothing on the path of a pointer sample:** a
chrome transition never runs while a viewport gesture is in flight, because input
responsiveness outranks motion. **Nothing that changes a size:** chrome
hide/restore is alpha only, because the `SurfaceView` is full-bleed and a
transition that changed a size would rebuild the swapchain, and the Property
Inspector changes its size **once** per detent change rather than animating a
height that would `requestLayout` per frame and re-run the whole adaptive decision.
**Only a user act animates** — the instant, idempotent paths are what the measure
pass and every state refresh call, which is also why the UI-R1C2 Objects re-parent
does not animate.

**Reduced motion crosses the boundary as one bool.** The Android layer reads
`ANIMATOR_DURATION_SCALE`, decides what it means, and hands the answer to
`DisplaySettingsStore::setReducedMotion`, which carries it to the renderer in the
same per-frame snapshot as the shading model — the same seam shape as
`ViewportBackground`: native code is given the *meaning*, never the platform
value. It deliberately does not advance the store's `changeCount_`, which exists
to prove a display transition the user chose.

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

`g_stateMutex` guards the single `CameraController`, `SelectionController` and
`ConstructionTransform` together, plus the gesture-routing flags. A tap resolves
its pick against the camera snapshot *and* the transform and updates the selection
under **one** lock hold, so they can never be seen out of step; the render thread
takes the camera snapshot, the derived model matrix and the selection flag under
the same lock immediately before each `drawFrame()`. The display snapshot (shading
model, surface shading, background, grid visibility, reduced motion) is read from
plain atomics rather than under that lock, because no combination of those values
is invalid to draw.

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
  symmetry, masking or layers, no stylus pressure or tilt, no brush presets.
- **No mesh library.** `SculptTopology` carries adjacency for a fixed topology
  and nothing more: no edge collapse, split, flip or incremental update, because
  the only thing that can happen to a frozen mesh is that a vertex moves.
- **No primitive framework.** No primitive base class, polymorphism, registry,
  property metadata, reflection or plugin surface. A further primitive costs
  another member, another `PrimitiveKind` case, another variant alternative and
  another per-kind JNI method.
- **No object commands and no hierarchy.** The scene adds and selects bodies and
  does nothing else: no delete, duplicate, rename, hide, lock, group, nesting or
  reorder, no parent field, no multi-select, and therefore no ObjectId reuse
  policy — there is nothing yet that can stop existing. No command framework and
  no Undo; the first stage that needs commands to be undoable owns that decision.
  The expanded Objects column is a **view** of that same flat list and adds no
  verb to it.
- **No snapping, and no Sketch grid.** The world reference grid is a viewport
  reference only. Nothing snaps to it, no cursor is quantised, and no dimension is
  ever derived from it. A Sketch grid — drawn on a sketch plane, with snapping —
  is a different contract with its own approval and must not be grown out of
  `forgeshape_grid.h`. Nor is there a Selection Outline, a View Cube, camera
  focus, named views, blur/glass or any post-processing framework.
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
  bodies: no property model, no binding layer, no editor registry, no reflection.
  `NumericPropertyRow` and `UnitChipsView` are components, not a framework — they
  know what a labelled number and a unit are, and nothing about primitives.
- **No design system and no motion framework.** Themes are two styles over one set
  of semantic attributes, not a component library; the Objects section is a flat
  list of rows, not an object browser or a history panel. `ChromeMotion` is four
  shared decisions, not a transition system.
- **No persistence.** Nothing is written to disk: not the scene, not the camera,
  not the start choice, not the theme and not the grid. A process kill is a clean
  slate.
- **`RuntimeMesh` is not a Construction mesh format**, and the debug paths are
  not product. `RuntimeMesh` carries positions, colours and indices and nothing
  else: no normals, no UVs, no material, no adjacency, no history.
  `forgeshape_demo_mesh` is the bootstrap cube's numbers only and
  `forgeshape_mesh_fixtures` is DEBUG test infrastructure reachable only through
  a key hook that compiles to a no-op in release; neither is a primitive, a
  Construction feature or a sculpting feature, and both are deliberately absent
  from `PRODUCT.md`. That hook is not a parallel implementation either: its
  bounded primitive driver calls the same `applyPrimitive` entry point, and its
  ability to publish a fixture over the object's current revision is exactly what
  makes it a *test* path.
