# ForgeShape — Architecture

Current production architecture only. No roadmap, no history.

ForgeShape is a standalone Android application that owns its own viewport and
renderer. No game engine (Godot/GDExtension, Unity, Unreal) and no third-party
runtime, rendering, math or input library participates in the running product.

## Layer map

```
ForgeShapeActivity            Android lifecycle, hosts the FrameLayout root,
        |                     shows the panel for the mode NATIVE code is in
        |
        +-- ConstructionPanelView field text + display unit + validation msgs
        |        |                (presentation/input ONLY - owns no parameter,
        |        |                 no kind and no transform)
        |        |  LengthUnit    exact BigDecimal mm/cm/m <-> meters
        |        |
        +-- SculptPanelView    tool selector + brush sliders + the way back
        |        |             (presentation/input ONLY - owns no vertex, no
        |        |              brush value, no tool and no mode)
        |        |
ForgeShapeSurfaceView         Surface lifecycle + raw pointer forwarding
        |                     (no camera state, no matrices, no Vulkan)
        |
NativeViewport (JNI decls)
        |  JNI
forgeshape_jni.cpp            render thread, ANativeWindow ownership,
        |                     MotionEvent -> TouchAction translation,
        |                     Camera + Selection ownership + locking
        |
        +----------------> CameraController      (camera + gesture truth)
        |                        |
        |                   CameraSnapshot (view, proj, eye, target, pose)
        |                        |
        +----------------> SelectionController   (tap decision + selected id)
        |                        |  on a valid tap
        |                        v
        |                   pickScene() -> Picking (ray build, ray/triangle)
        |                        |              ^
        |                        |              | current mesh snapshot
        |                        v              |
        |                   ObjectId or none    |
        |                        |              |
        |                  bool "draw highlight"|
        |                        |              |
        |                        |     ConstructionObject      SculptSession
        |                        |              |   ObjectId       ProductMode
        |                        |              |                  SculptTool
        |                        |              |                  SculptStroke
        |                        |              |   PrimitiveKind    Construction
        |                        |              |   ConstructionBox    | Sculpt
        |                        |              |   ConstructionCylinder    |
        |                        |              |   ConstructionSphere      |
        |                        |              |   ConstructionCone        |
        |                        |              |   ConstructionCapsule     |
        |                        |              |   ConstructionTransform   |
        |                        |              |  generateMesh()      SculptMesh
        |                        |              |   [LOCAL space]      [LOCAL,
        |                        |              |          |            copied
        |                        |              |          Freeze  -->  at Freeze,
        |                        |              |                       own rev]
        |                        |              |                           |
        |                        |              +-- the ACTIVE one is published --+
        |                        |              v
        |                        |         MeshStore  (immutable revisions,
        |                        |              |      monotonic, latest-wins)
        |                        |              |
        |                        |         modelMatrix() / inverseModelMatrix()
        |                        v              v
        +----------------> Renderer              (Vulkan, frame loop, upload)
                                 |
                            ANativeWindow -> VkSurfaceKHR -> swapchain
```

## Ownership

| Concern | Owner | Explicitly NOT an owner |
| --- | --- | --- |
| Activity lifecycle, view tree composition | `ForgeShapeActivity` | — |
| Shape/transform field text, display unit, the *draft* primitive kind, input validation messages | `ConstructionPanelView` | it owns no parameter, no kind, no transform, no mesh and no publish decision |
| Brush slider positions, which tool button looks active, and the sculpt status text | `SculptPanelView` | it owns no vertex, no brush value, no tool and no mode; it reads all four back from native state |
| Which panel is on screen | `ForgeShapeActivity.syncMode()` | it decides nothing — it *reads* `NativeViewport.productMode()` |
| The active product mode, the one Frozen Sculpt Mesh, the active tool, the brush and the live stroke | `SculptSession` (`forgeshape_sculpt.{h,cpp}`) | Java owns none of these; the renderer owns no sculpt truth |
| Frozen local vertex/index data and its `SculptRevision` | `SculptMesh` | it is NOT Construction truth and no parameter is ever read back out of it |
| 1-ring adjacency and incident triangles of the frozen mesh | `SculptTopology` | built once per Freeze, never per move; it is not a half-edge mesh and cannot change topology |
| Area-weighted vertex normals from current positions | `computeVertexNormals` + `SculptMesh`'s dirty-flagged cache | derived data, not truth; no brush computes its own |
| The stroke kernel: hit, affected set, falloff, radius resolution, lifecycle | `SculptStroke` | it restates no FOV or aspect — it reads the `CameraSnapshot`'s own matrices |
| What each tool does to the vertices it captured | one `SculptStroke::apply*` per `SculptTool` | there is no brush base class, registry or plugin surface |
| Whether a one-finger Down becomes a stroke or a navigation | `g_strokePending` in `forgeshape_jni.cpp`, using `SculptSession::hitsSculptMesh` | Java decides none of it; the probe cannot mutate the mesh |
| mm/cm/m ↔ meter conversion and number formatting | `LengthUnit` | the domain never sees a display unit |
| Identity, which primitive is active, every primitive's parameters, and placement | `ConstructionObject` (`forgeshape_construction.{h,cpp}`) | it is ONE object, not a registry, a list, a scene graph or a hierarchy |
| Exact box W/H/D and its local mesh | `ConstructionBox` | — |
| Exact cylinder diameter/height and its local mesh | `ConstructionCylinder` | radial segment count is fixed, not a parameter |
| Exact sphere diameter and its local mesh | `ConstructionSphere` | meridian and stack counts are fixed, not parameters |
| Exact cone bottom diameter/height and its local mesh | `ConstructionCone` | the apex radius is zero by definition, not a parameter; there is no top diameter and no frustum |
| Exact capsule diameter/**total** height and its local mesh | `ConstructionCapsule` | the cylindrical middle is derived and never stored, exactly as a radius is |
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
| Local-space mesh generation from exact parameters | `ConstructionBox` / `ConstructionCylinder` / `ConstructionSphere` / `ConstructionCone` / `ConstructionCapsule` | no JNI/Android/Vulkan/renderer/UI types; the mesh, the GPU and the picker own no parameter |
| Current CPU mesh, revisions, validation | `MeshStore` / `RuntimeMesh` (`forgeshape_mesh.{h,cpp}`) | no JNI/Android/Vulkan types; owns no GPU resource |
| Baseline cube numbers | `forgeshape_demo_mesh.{h,cpp}` | not read by the renderer or the picker any more |
| DEBUG mesh fixtures | `forgeshape_mesh_fixtures.{h,cpp}` | test infrastructure, not product geometry |
| Vulkan mesh buffers, staging, upload | `Renderer` | it owns no CPU mesh and mutates none |
| Vulkan, presentation, geometry upload | `Renderer` | it never interprets input and owns no identity |
| Vector/matrix math | `forgeshape_math.h` | no GLM or other third-party math |

## Android layer

`ForgeShapeSurfaceView` is a plain `android.view.SurfaceView` (no Compose, no
AndroidX). Its whole contribution to navigation is `onTouchEvent`, which copies
the masked action, the id of any lifting pointer, and each pointer's stable id
and view-local x/y into preallocated arrays, then makes one JNI call.

`GestureDetector` / `ScaleGestureDetector` are deliberately unused: they would
move camera semantics into the Android layer.

### Viewport / panel composition

The Activity's content view is a `FrameLayout` holding the `SurfaceView` and,
added after it so they draw and receive touches first, `ConstructionPanelView`
and `SculptPanelView`, both anchored to the top. All are plain framework views
built in code — no Compose, no AndroidX, no resource layouts, no design system,
no toolbar or drawer.

Exactly one panel is visible, and which one is decided by native state:
`syncMode()` reads `NativeViewport.productMode()` and shows the matching panel.
In Sculpt Mode the shape and transform editors are therefore not merely disabled
but **absent**, so nothing on screen can edit the Construction Source while the
Frozen Sculpt Mesh is the thing being worked on.

Both panels swallow every touch inside their bounds that none of their own
controls takes (`onTouchEvent` returns `true`), so an unclaimed touch on a panel
can never fall through and orbit the camera — or, in Sculpt Mode, deform the
model while reaching for a slider. Touches outside its bounds never reach
it and navigate normally. In the other direction, `ACTION_DOWN` on the viewport
pulls focus and the soft keyboard away from any field being edited, so navigation
never happens "through" a focused editor.

The panel is at the **top** deliberately: the soft keyboard rises from the
bottom, so with `windowSoftInputMode="adjustPan"` neither the fields nor the box
is covered and the window is never resized — which would otherwise rebuild the
swapchain on every keystroke session.

### Properties UI ownership boundary

`ConstructionPanelView` holds exactly three pieces of state: the text in its
fields, the selected display unit, and a **draft** primitive kind. It holds no
parameter, no authoritative kind and no transform. Everything it shows is read
from native code (`NativeViewport.constructionPrimitive` and
`NativeViewport.boxTransform`) at construction and again on every resume, and the
only way it can change anything is to submit a whole section at once and accept
the verdict.

The shape selector is the draft: it swaps which parameter fields are on
screen and nothing else. The object's kind changes only when Apply Shape reads
the drafted primitive's own fields and calls **that primitive's own native
method**, so there is no window in which the object is a cylinder carrying box
dimensions and no point at which a value travels in a slot whose meaning depends
on a separate kind. `refreshFromNative` resets the draft to the object's real
kind, so the selector can never be left claiming a shape the object is not.

Exactly one parameter row is on screen at a time, and it is always the drafted
kind's, so no field on screen can be read as another primitive's parameter.
Every primitive's fields are kept populated and converted, including the hidden
ones, so an inactive draft does not silently change meaning while it is off
screen.

Shape and placement have separate Apply buttons — *Apply Shape* and *Apply
Transform* — because they are separate truths with different consequences: one
republishes the mesh, the other cannot. One button doing both would hide that.

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
printed with trailing zeros stripped and no grouping separator.

Parsing accepts `.` or `,` as the decimal separator. The fields use a numeric IME
via `setRawInputType` combined with a `DigitsKeyListener` accepting
`0123456789.,-`, so a comma and a minus sign are typeable. The minus matters
twice over: a negative coordinate or angle is an ordinary value that must be
enterable, and a negative *dimension* must be enterable so it can be visibly
refused rather than being unreachable.

## JNI boundary

Five lifecycle methods, one input method and six Construction methods on
`NativeViewport`:

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
applyConstructionCylinder(double diameterMeters,           // submit a cylinder
                          double heightMeters)
applyConstructionSphere(double diameterMeters)             // submit a sphere
applyConstructionCone(double bottomDiameterMeters,         // submit a cone
                      double heightMeters)
applyConstructionCapsule(double diameterMeters,            // submit a capsule
                         double totalHeightMeters)

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
where a depth belongs, and has no second or third number to get wrong when asking
for a sphere.

Every length crosses in meters and every angle in degrees, so no display unit and
no radian ever reaches native code.

The eight sculpt methods follow the same shape: Java may **request** a mode or a
tool and is then told what is actually active, and it reads the brush back rather
than assuming its slider mapping was honoured. There is no method that sets a
vertex, no method that starts a stroke and no method that carries geometry across
the boundary in either direction — a stroke is driven entirely by the existing
`touchEvent` path, and the geometry it produces reaches the GPU through
`MeshStore` exactly as every other revision does. In particular there is no
method for the arbitration: whether a Down becomes a stroke is decided below the
boundary, where the mesh actually is.

These six Construction methods are the whole contract. Reading fills a
caller-allocated array; there is no other direction — nothing is measured from
the mesh, from GPU data or from a model matrix. Writing carries a whole section
in one call and returns a status code (`APPLY_APPLIED`, `APPLY_UNCHANGED`, or one
of the `APPLY_REJECTED_*` reasons). There is no `setWidth`, no `setRotationX`, no
`setKind`, no separate `publish`, and no way for Java to observe or produce a
half-applied state.

Below the boundary all five shape methods build the matching typed
`PrimitiveSpec` and go straight into `applyConstructionPrimitive`, so the
per-kind split is a boundary shape only: the update-then-publish rule still has
exactly one implementation.

The JNI layer adds logging and that status code and nothing else: it does not
decide validity, does not decide whether to publish, and never touches
`MeshStore` or the transform. The DEBUG primitive driver calls the same function,
so there is exactly one implementation of the rule in the process.

The transform path reuses the same `APPLY_*` vocabulary.
`APPLY_REJECTED_NOT_POSITIVE` simply cannot occur there, because zero and
negative are ordinary coordinates and angles.

One JNI call carries one complete `MotionEvent`, never one call per pointer.
`forgeshape_jni.cpp` translates Android's masked action constants into
`forgeshape::TouchAction`; unrecognised actions (hover, scroll, button) are
dropped rather than forwarded.

## Camera ownership

`CameraController` (`forgeshape_camera.{h,cpp}`) is the single source of camera
truth and contains no JNI, Android or Vulkan types. It owns:

- `target` (orbit centre), `yaw`, `pitch`, `distance`;
- FOV, near and far planes;
- viewport width/height, and therefore projection aspect;
- the gesture state machine (mode, tracked pointer ids, anchors).

It produces a `CameraSnapshot` — view matrix, projection matrix, eye, target and
pose scalars — which is the only thing the renderer ever sees.

Gesture rule: every touch event recomputes the set of pointers that are still
down, sorted by pointer id. If that set differs from the tracked one, the
controller re-anchors and applies **no** delta; deltas are applied only on Move
events whose pointer set is unchanged. This is what makes 1↔2 pointer
transitions and MotionEvent index reordering jump-free.

## Canonical winding and culling

A triangle's vertices are ordered **counter-clockwise when the triangle is
viewed from outside the surface**, in right-handed world space. Equivalently the
geometric normal `N = (v1 - v0) x (v2 - v0)` points away from the solid, and a
triangle is front-facing to a ray when `dot(N, rayDirection) < 0`.

The projection in `forgeshape_math.h` flips Y for Vulkan clip space, which
mirrors screen-space winding, so the same convention appears **clockwise in
framebuffer coordinates**. The pipeline therefore uses

```
cullMode  = VK_CULL_MODE_BACK_BIT
frontFace = VK_FRONT_FACE_CLOCKWISE
```

CPU picking accepts front faces only, under the same rule, so what can be picked
is exactly what the rasterizer draws. The self-tests assert the convention on all
12 demo cube triangles, and assert that a ray fired from inside the cube misses
under front-face-only picking; that is what keeps the data, the rasterizer and
the picker from drifting apart.

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

There is exactly one of these. It is emphatically **not** a registry: there is no
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
typed: `box()`, `cylinder()` and `sphere()` each return a pointer that is null
unless the spec really is that primitive, so reading the wrong one is a null
check away rather than silent nonsense. Specs are built only through
`forBox` / `forCylinder` / `forSphere`, so a request always says what it is by
construction.

`ConstructionObject::spec()` therefore returns only the **active** primitive's
parameters. The remembered parameters of the inactive ones are still reachable —
through `box()`, `cylinder()` and `sphere()` — but only by asking for that
primitive by name, which is exactly what the panel's draft display does and
nothing else needs.

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

The cylinder's authoritative parameter is its **diameter**, because that is what
a drawing and a caliper give you. `radiusMeters()` exists but is derived on
demand and never stored.

mm/cm/m exist **only** above the JNI boundary, in `LengthUnit` and
`ConstructionPanelView`. Every value crossing into native code is already in
meters, and no native type, function or log line names a display unit. The unit a
dimension was typed in is therefore unrecoverable from domain state — which is
correct: it is not part of what the box is.

### Box generation

The box is centred on the local origin and axis-aligned: its 8 corners are
exactly `(±width/2, ±height/2, ±depth/2)` with no offset term, so the centre is
the origin by construction rather than by arithmetic that could drift. Its 36
`uint32_t` indices are a fixed table in the canonical ForgeShape winding
(counter-clockwise seen from outside), the same convention the pipeline and CPU
picking already share. Generation is a pure deterministic function of the
parameters, so identical parameters always produce identical vertices.

Vertex colour is **derived presentation data**, not Construction truth: it exists
because `RuntimeMesh` carries a colour channel and a per-corner gradient makes
orientation readable. No dimension is ever recovered from it.

Because the topology is constant, a box dimension change publishes a same-count
revision (8 vertices / 36 indices), which the Stage 006 capacity policy reuses
without recreating a buffer.

### Cylinder generation

The cylinder is centred on the local origin with its axis along **local +Y**, so
its extents are exactly `±height/2` in Y and `±diameter/2` in X and Z.

Tessellation is **fixed at 32 radial segments** and is deliberately not a
parameter. It is a rendering detail of an exact shape, not a property of it: a
user who could set it would be authoring the approximation rather than the
cylinder. 32 is divisible by four, which matters — see below.

Vertex layout, shared by the side wall and both caps:

```
[0, N)      bottom ring, y = -height/2
[N, 2N)     top ring,    y = +height/2
2N          bottom cap centre
2N + 1      top cap centre
```

so the counts are `2N + 2` vertices (66) and `12N` indices (384): two triangles
per segment for the side wall, one per segment for each cap fan. Every ring
vertex is referenced exactly five times — three by the side wall, two by a cap
fan — and each centre exactly `N` times, which accounts for the whole index
buffer and is asserted by the self-tests.

The four cardinal directions are **written down rather than computed**.
`cos(pi/2)` in floating point is about 6.1e-17, not 0, and that error would make
the generated X/Z bounds not quite the radius. Because N is divisible by four,
those four directions always land on real ring vertices, so the bounds come out
exactly `±diameter/2`. The self-tests assert exact equality, not a tolerance.

Winding follows the same canonical convention as the box — counter-clockwise seen
from outside — with side normals pointing radially outward, the top cap's at +Y
and the bottom cap's at −Y. That is what makes the sides and *both* caps survive
back-face culling, and the self-tests check it with the same
`meshObeysCanonicalWinding` predicate the box uses, plus a ray fired from inside
that must miss.

Between two ring vertices the surface is a flat facet, so a picked point on the
side lies between `radius * cos(pi/N)` and `radius` from the axis. That is the
tessellation showing through, not an error: the *parameters* remain exact.

### Sphere generation

The sphere is centred on the local origin, so its extents are exactly
`±diameter/2` on all three axes. The authoritative parameter is the **diameter**,
for the same reason as the cylinder's; `radiusMeters()` is derived on demand and
never stored.

Tessellation is fixed at **32 meridians and 16 latitude stacks**, and is not a
parameter, for the same reason the cylinder's is not. 32 is divisible by four, so
the four cardinal meridians land on real vertices; 16 is even, so exactly one
ring falls **on** the equator rather than straddling it.

Vertex layout:

```
ring r in [0, 15), meridian j in [0, 32):
    index r * 32 + j      theta = pi * (r + 1) / 16, counted from the north pole
15 * 32                   north pole, y = +radius
15 * 32 + 1               south pole, y = -radius
```

so the counts are `15 * 32 + 2` = 482 vertices and `2N(S-1)` = 960 triangles =
2880 indices: two triangles per quad in each of the 14 full bands, plus one fan
triangle per meridian at each pole.

Each pole is **one vertex fanned to its adjacent ring**, not a collapsed row of a
quad grid. The collapsed form produces 32 zero-area triangles at each pole —
invisible, but real, and noise in every winding, area and validation check. The
self-tests assert that no triangle is degenerate and that no triangle repeats a
vertex.

The equator and the four cardinal directions are **written down rather than
computed**, exactly as the cylinder's cardinal directions are, and the two exact
factors are multiplied in double before the single conversion to float. That is
what makes the X and Z bounds exactly `±radius` instead of one rounding short,
and the poles make the Y bounds exactly `±radius` too. The self-tests assert
exact equality on all six bounds, on the four equator samples and on both poles.

Winding follows the same canonical convention as the box and the cylinder: the
band ordering is the cylinder's side-wall ordering with "lower ring" and "upper
ring" in the same roles, and the two pole fans are its two cap fans. Closure is
asserted directly — every directed edge appears exactly once and its reverse
exists, and `V - E + F = 2` — which fails on a hole, on a duplicated triangle and
on a triangle wound the wrong way round.

As with the cylinder, the drawn surface is inscribed in the exact one, so a
picked point lies between `radius * cos(pi/32)^2` and `radius` from the centre.
The *parameter* stays exact, and no diameter is ever reconstructed from mesh
floats.

### Shared tessellation

`kPrimitiveRadialSegments` (32) and `kPrimitiveLatitudeStacks` (16) are the one
owner of how finely every round primitive is divided. The cylinder, the sphere,
the cone and the capsule all name those constants rather than writing down a
count of their own, so they cannot drift apart and a capsule's hemispherical end
is provably the same tessellation as a sphere's — `kCapsuleHemisphereBands` is
literally `kPrimitiveLatitudeStacks / 2`.

Both are fixed and not exposed, for the reason each generator already gave
individually: they are a rendering detail of an exact shape, not a property of
it, and exposing them would put the approximation into authored state where it
would have to be persisted, versioned and validated like a real parameter.

32 is divisible by four so the four cardinal directions land on real vertices;
16 is even so a hemisphere is exactly half of it and one ring falls on the
equator plane rather than straddling it. Every exactness claim below depends on
those two facts.

### Cone generation

The cone is centred on the local origin with its axis along **local +Y**: the
base is at `y = -height/2`, the apex at `y = +height/2`, so the extents are
exactly `±height/2` in Y and `±bottomDiameter/2` in X and Z. The base is
**closed**.

The authoritative parameters are the **bottom diameter** and the height. The apex
radius is **zero by definition** and is not a parameter: there is deliberately no
top diameter and no frustum, because a truncated cone is a different shape rather
than a cone with an extra number.

```
[0, N)      base ring, y = -height/2
N           base centre
N + 1       apex,      y = +height/2
```

so the counts are `N + 2` = 34 vertices and `6N` = 192 indices: one side triangle
and one base-fan triangle per segment. Each ring vertex is referenced exactly
four times — twice by the two side triangles it borders and twice by the two
base-fan triangles — which accounts for the whole index buffer.

The apex is **one vertex fanned to the base ring**, exactly as a sphere's pole
is, not a collapsed top ring. The collapsed form produces N zero-area triangles
at the tip: invisible, but real, and noise in every winding, area and validation
check. Concretely the side triangle is the cylinder's `(b0, t1, b1)` with the top
ring collapsed to the apex; its `(b0, t0, t1)` partner is the one that would have
been degenerate, so it simply does not exist here.

The four cardinal directions are written down rather than computed, exactly as
the cylinder's are, so the X and Z bounds are exactly `±bottomDiameter/2`. The
base fan is the cylinder's bottom cap unchanged, with its outward normal at −Y;
the side normals tilt outward and upward. Closure is asserted directly — every
directed edge appears once with its reverse present, and `V - E + F = 2` — which
is what would fail if the base were left open.

### Capsule generation

The capsule is centred on the local origin with its axis along **local +Y**. Its
authoritative parameters are the **diameter** and the **total** height — ends
included, which is what a caliper measures. Both the radius (`diameter/2`) and
the cylindrical middle (`totalHeight - diameter`) are **derived and never
stored**, for the same reason a radius never is: they are not what the user
measured.

The middle is centred on the origin too, so its ends sit at
`y = ±(totalHeight - diameter)/2`, and each hemispherical end caps that end at
the same radius. The extents are therefore exactly `±diameter/2` in X and Z and
`±totalHeight/2` in Y.

```
ring r in [0, R), meridian j in [0, N):   index r * N + j
R * N                                     north pole, y = +totalHeight/2
R * N + 1                                 south pole, y = -totalHeight/2
```

Rings run from the north pole downward. The first `kCapsuleHemisphereBands` are
the top hemisphere, the last of those being the **top seam ring**; the rest are
the bottom hemisphere, the first of those being the **bottom seam ring**.

The cylindrical middle is therefore **not a special case in the index loop at
all**: it is simply the band between ring `H-1` and ring `H`, generated by the
same code as every other band. `R` rings plus two poles is `R - 1` quad bands
plus two pole fans, so the counts are `R * N + 2` vertices and `6 * N * R`
indices — 514 and 3072 for a capsule with a middle.

**The equality case.** `totalHeight == diameter` is valid and describes a capsule
with no middle, which is a sphere. It is generated as one: the two seam rings are
**the same ring**, `R` is one smaller, that band does not exist, and the result
is exactly the sphere's 482 : 2880 topology. That is what keeps the equality case
free of a duplicated zero-length ring and of zero-area triangles, and it is
asserted at compile time rather than left as a coincidence — a `static_assert`
pins `kCapsuleSphericalVertexCount` and `kCapsuleSphericalIndexCount` to one
fewer ring and one fewer band. Topology is therefore a deterministic function of
the parameters, not a constant: the two forms are the two sides of one exact
double comparison with nothing in between.

The seam bands use a written-down `cos = 0, sin = 1` for the same reason the
sphere's equator does, so a seam ring lands exactly on `±middle/2` at exactly the
full radius; the poles are written down from the authoritative **total** height
rather than accumulated as `middle/2 + radius`, so the Y bounds are exact rather
than a rounding away. Winding, band ordering and pole fans are the sphere's,
unchanged.

### The capsule relation, and float resolvability

The capsule is the first primitive whose two parameters are **related** rather
than independent, so it is the first with validation beyond the per-length rule.
`validateCapsuleMeters` owns both halves:

- `totalHeight >= diameter`, because the two hemispherical ends alone are already
  `diameter` tall. Violating it reports `DimensionValidation::RelationInvalid`,
  a reason distinct from `NotPositive` because "0.5 m is not a length" and "0.5 m
  is too short to be this capsule's total height" are different problems and
  deserve different messages. It surfaces at the JNI boundary as
  `APPLY_REJECTED_RELATION`.
- **float resolvability**: the generated positions must not merely be finite,
  they must be far enough apart to describe the shape. A 1e30 m capsule 1e-6 m
  across is finite in every coordinate and yet its whole hemisphere rounds to a
  single float, which would be a mesh of zero-area triangles. The check compares
  the pole against the first ring below it — the smallest latitude step anywhere
  on a hemisphere, so resolving it resolves all of them — and refuses the request
  as `NotRepresentable` rather than publishing geometry no parameter set could
  usefully produce.

The UI restates neither half. It refuses only what it can name from the text
alone (blank, non-numeric, non-positive), and the relation is reported back to it
as an ordinary rejection, which is why the capsule's message is the only one
that names two fields at once.

### Update and invalid-update behaviour

`ConstructionObject::setPrimitive` reports `Applied`, `Unchanged` or `Rejected`:

- **Rejected** — a parameter that is not finite, not positive, or whose derived
  `float` half-extent would be zero or non-finite. This **fails closed**: every
  value the request carries is validated before anything at all is written, so a
  bad depth cannot leave a half-applied width behind *and a bad diameter cannot
  leave the kind switched*. The previous kind, parameters, transform and mesh revision all
  stand. The only limits are physical validity and float representability — there
  is no arbitrary product size policy.
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
the rule in the process. `publishConstructionObject` remains separately callable for
the startup publish, which changes no parameter.

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
reads the other. Since Stage 010 both are held by the same
`ConstructionObject`, so they cannot become inconsistent about which object they
describe.

```
active primitive parameters (double meters) -> LOCAL geometry -> MeshStore -> GPU
ConstructionTransform P (double meters)     -> modelMatrix()  -> Renderer
                      R (double degrees)    -> inverseModel() -> picking
```

The published `RuntimeMesh` is the object's geometry in **object space**. The
transform never touches it. That is the whole point: moving or rotating the
object changes one derived 4×4 matrix and nothing else — no vertex is rewritten,
no `MeshRevision` is published and no GPU upload happens. `ConstructionTransform`
could not publish one if it wanted to; it has no access to `MeshStore`.

The independence runs both ways: a shape change republishes the mesh and leaves
the placement exactly as it was, including across any switch among the five kinds.

### Unit contract

Position is `double` **meters**, rotation is `double` **degrees**, named
`...Meters` and `...Degrees` at every boundary. Degrees are authoritative because
degrees are what the product exposes; radians exist only inside the derived
trigonometry. mm/cm/m applies to position exactly as it applies to a dimension,
and never to rotation.

### Axis and Euler convention

Right-handed world space, **+Y up**, column-vector math (`p' = M * p`), storage
column-major. A positive angle rotates by the **right-hand rule**.

Rotations compose in **local X → Y → Z** order, which for column vectors is
written right to left:

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

The self-tests assert the convention directly: `Rx(+90)` takes +Y to +Z, `Ry(+90)`
takes +Z to +X, `Rz(+90)` takes +X to +Y, the X-then-Y composition lands where
only the documented order puts it, and `Model * Model⁻¹` is the identity in both
directions.

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

### Debug fixture boundary

`forgeshape_mesh_fixtures` remains DEBUG test infrastructure and is no longer on
any startup path. Product geometry at startup comes from `ConstructionObject`.
The debug key hook can still publish a fixture over the object's revision — that
is exactly what makes it a *test* path, and it compiles to a no-op in release.

The debug hook also carries a bounded primitive driver (four fixed box states,
including one deliberately invalid one). It is not the product edit path — that
is `ConstructionPanelView` — but it is not a parallel implementation either: it
calls the same `applyPrimitive` entry point, and it compiles to a no-op in
release.

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
- **A sculpt edit changes no parameter.** It cannot: the sculpt module has no
  mutable access to `ConstructionObject`, and the only mutation `SculptMesh`
  offers is a single vertex POSITION. There is no API here that can add, remove
  or reorder a vertex or touch an index, so topology is preserved by
  construction rather than by discipline.
- **Nothing reconstructs a parameter from sculpt vertices.** That direction does
  not exist here, exactly as it does not exist from the Construction mesh.
- The transform is deliberately **not** duplicated. Both representations are
  local geometry under the one `ConstructionTransform`, so the object sits in
  exactly the same place in either mode and there is no second placement to keep
  in sync.

`SculptRevision` is emphatically not a `MeshRevision`. `MeshStore`'s revisions
are minted by the publication path and count every published snapshot of
whichever representation is active; a `SculptRevision` counts changes to *this*
mesh and restarts at 1 on every Freeze. Neither is derived from the other and
they cannot be compared.

### Mode ownership

`SculptSession` owns `ProductMode` — `Construction` or `Sculpt` — plus the one
`SculptMesh`, the brush settings and the live stroke. It is process-scoped, like
the camera, the selection, the mesh store and the Construction object.

The mode is **native state**. The Android UI may request a change through
`freezeToSculpt` / `enterSculptMode` / `enterConstructionMode` and is then told
what the mode actually is; `ForgeShapeActivity.syncMode()` reads
`productMode()` back rather than assuming its request succeeded, so a refused
request (entering Sculpt with nothing frozen) cannot leave a panel on screen that
lies about what is being edited.

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
every stroke and every move; nothing rebuilds it per MOVE, and the self-tests
assert that its build count stays at 1 across a whole stroke.

It stores exactly the two things this stage needs, in flat CSR arrays: each
vertex's **1-ring vertex neighbours** and its **incident triangles**. Neighbour
lists are sorted and deduplicated, because a shared edge is seen once from each
of its two triangles and without the dedup every interior neighbour would be
weighted double in a mean. Out-of-range indices are skipped rather than trusted
and no vertex is ever its own neighbour, so a neighbour list can never contain
something a caller would dereference out of bounds.

It is deliberately **not** a half-edge structure. Half-edges are the right shape
for *changing* topology, and nothing here can change topology.

`computeVertexNormals` produces **area-weighted** normals from the CURRENT
positions: each triangle contributes its unnormalized `(v1-v0) x (v2-v0)` — the
same canonical outward normal picking uses, whose length is twice the triangle's
area — to all three corners, and each vertex normal is then normalized. A
degenerate triangle contributes a zero vector rather than a NaN, and a vertex
with no usable accumulated direction gets the **zero** vector rather than an
invented axis, so a normal-based brush simply does not move it. Every value
written is finite.

**When normals are recomputed:** the cache is marked dirty by every accepted
`setVertexPosition` and recomputed on the next read. That is the whole rule. So
they are recomputed at most once per batch of position writes — in practice once
per brush move that changed something — and never per frame, never per vertex and
never when nothing has moved.

### The brush kernel

`SculptStroke` is the one kernel. It captures everything on DOWN and then holds
it fixed for the whole stroke: the **active tool**, the affected vertex set, their
falloff weights, their starting positions and **starting normals**, the
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
  or out of the brush mid-stroke and a stroke stays one coherent deformation.
- The falloff is `w = (1 - (d/r)^2)^2`: 1 at the centre, 0 at and beyond the rim,
  with zero derivative at both ends, so a stroke leaves no crease at the edge.
  One function, `sculptFalloff`, used by every tool.
- **Radius and strength are shared by every tool.** There is no per-tool copy of
  either, so switching tools never changes how big or how strong the brush is.
  Both are **clamped**, not refused: a slider cannot produce a brush that does
  nothing or a brush without bound, and native code stays the authority.
- A stroke **holds the tool it began with**. Changing the session's tool
  mid-stroke changes what the next stroke will be, never what this one is.
- A cancelled stroke is a stroke that **stopped**, not one that is undone.
  Positions already written stay written; there is no undo in this stage and a
  partial one invented here would be worse.

### The four tools

| | driven by | direction | accumulates |
| --- | --- | --- | --- |
| Grab | where the finger **is** | camera plane | no — `base + delta × weight` |
| Clay | pointer **path length** | each vertex's normal **at stroke start** | yes |
| Smooth | pointer **path length** | toward the 1-ring neighbour mean | yes |
| Inflate | pointer **path length** | each vertex's normal **right now** | yes |

**Grab** is position-driven and unchanged from Stage 012: every Move recomputes
`base + delta * weight` rather than accumulating, so the result depends only on
where the finger *is*, never on how many events it took to get there.

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
an unbounded displacement.

**Clay is deposition.** Every affected vertex moves along the direction it faced
when the finger landed — a direction field captured once and held fixed for the
stroke, exactly as the affected set and the weights are. The stroke lays down a
coherent slab in one consistent set of directions, so repeated passes make it
taller without changing what it is.

**Inflate is expansion.** It re-reads the vertex normals from the CURRENT
positions on every move, so each vertex expands along the surface as the surface
changes. On a bulge that is already forming the flank normals have tilted
outward, so Inflate widens and rounds the bulge instead of extruding it along the
directions it started with.

That is the whole difference, and it is a difference in the formula rather than
in a constant: Clay reads `baseNormal`, Inflate reads `mesh.vertexNormals()`. It
is directly observable — a clayed vertex's *total* displacement stays exactly
parallel to the normal it started with however many moves it took, while an
inflated vertex's leaves that axis as soon as the surface has moved — and the
self-tests assert both halves with thresholds three orders of magnitude apart.

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
outside the brush are read and never modified.

Inflate snapshots its normals the same way and for the same reason.

### Sculpt-mode gesture rule and arbitration

One finger that goes **down on the Frozen Sculpt Mesh** is a brush stroke and owns
the whole gesture; one finger that goes down anywhere else navigates exactly as
it always has. Two-finger pan and pinch are untouched. Whether the finger landed
on the mesh is decided **once, on Down, and never revisited**, so a stroke cannot
turn into an orbit half way through a drag as the finger crosses the silhouette.

What *is* deferred is whether the gesture is a stroke at all. A one-finger Down on
the mesh is ambiguous when it arrives — it is either the start of a stroke or the
first of two fingers — so the rule is **pending-then-promote**:

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
clear either.

The 8 px threshold sits deliberately between touch jitter and the 24 px tap slop,
so a stroke still starts long before the gesture would stop being a tap. The
residual it accepts is stated rather than hidden: a first finger that
*deliberately drags more than 8 px* before the second one lands does commit a
stroke. That is a gesture the user drove as a stroke, and closing it completely
would require a buffered or undoable stroke, which does not exist yet.

While a stroke owns the gesture, neither `CameraController` nor
`SelectionController` sees the event at all, which is what makes a brush gesture
structurally unable to orbit, select or clear.

`g_grabbing` and `g_strokePending` in `forgeshape_jni.cpp` are gesture routing
only, guarded by `g_stateMutex` alongside the camera and the selection. Like
camera anchors and tap candidacy they are dropped whenever the Surface goes away;
the mode, the active tool and the sculpted vertices are not, because they are
process-scoped.

## Runtime mesh ownership

`forgeshape_mesh.{h,cpp}` owns the CPU side of geometry. It contains no JNI,
Android or Vulkan types and holds no GPU resource.

`RuntimeMesh` is one **immutable** published revision: an object id, a revision
number, interleaved `MeshVertex` (position + colour) data and `uint32_t`
indices. The only way to build one is `createRuntimeMesh`, which validates
first, so a `RuntimeMesh` that exists has already been proven usable. Consumers
hold it through a `shared_ptr<const RuntimeMesh>`, so a pick in progress keeps
reading its own revision even while a newer one is published.

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

### Index width

The runtime and render paths use **32-bit indices**
(`uint32_t` / `VK_INDEX_TYPE_UINT32`) as of Stage 006. The bootstrap cube's
16-bit indices would have structurally capped every future mesh at 65,535
vertices, and there is no measured benefit to 16-bit at these sizes; the CPU
`TriangleMeshView` and the GPU index buffer now agree on one width, so they
cannot drift.

## GPU mesh upload

`Renderer` owns every mesh-related `VkBuffer`, `VkDeviceMemory`, copy and
destroy, and performs all of them on the render thread. Publishers never touch
Vulkan.

Steady-state vertex and index buffers are **DEVICE_LOCAL** with
`TRANSFER_DST` usage. They are written through one reused **HOST_VISIBLE**
staging buffer that carries the vertex block followed by the index block, copied
in a single command buffer with one memory barrier
(`TRANSFER_WRITE` → `VERTEX_ATTRIBUTE_READ | INDEX_READ`).

Capacity policy (`growCapacityBytes`, pure arithmetic, self-tested):

- sufficient existing capacity is **reused as-is**, including for a smaller
  mesh — capacity never shrinks and a same-topology update never recreates a
  buffer;
- otherwise capacity grows by 1.5x, but never to less than what is needed;
- everything is bounded by a hard cap, so the size arithmetic cannot overflow.

### In-flight resource safety

Before a mesh buffer is overwritten or retired, the renderer waits on **its own
frame fences** — all `kMaxFramesInFlight` of them — so no submitted frame can
still be reading it. The transfer itself is submitted with a dedicated upload
fence that is waited on before the staging buffer or the upload command buffer
is reused. A retired buffer is therefore destroyed only after every frame that
could reference it has finished, and exactly one vertex buffer and one index
buffer are live at any time.

The mesh update path deliberately calls **neither `vkDeviceWaitIdle` nor
`vkQueueWaitIdle`**. Those remain only where they already were: process
teardown, surface detach and swapchain rebuild.

Mesh buffers are device-scoped, not surface-scoped: a Surface swap does not
touch them, so the uploaded revision survives home/resume with no re-upload.

## Picking and selection

`forgeshape_picking.{h,cpp}` converts a view-local pixel plus a `CameraSnapshot`
into a world-space ray, and intersects that ray with indexed triangles
(Möller–Trumbore, nearest positive hit). It reads the snapshot's own `proj` and
`view` matrices rather than restating FOV or aspect, so there is one camera
truth. It contains no JNI, Android, Vulkan or renderer types, decides no object
identity, and uses no GPU id buffer.

`pickScene` intersects the **current CPU mesh snapshot** from `MeshStore` —
never Vulkan buffer memory — and takes the object id from the store rather than
from the geometry. Picking is a linear scan; there is no spatial acceleration.

Because that snapshot is the box generated from the current parameters, picking
automatically follows a dimension change with no separate collision
representation to keep in sync: a tap on the top face of a 0.42 m tall box
reports a hit at y = 0.2100.

`SelectionController` (`forgeshape_selection.{h,cpp}`) owns the selected
`ObjectId` and the tap-versus-navigation decision. `ObjectId` is an opaque
`uint64_t` minted by this layer: never a pointer, list index, Vulkan/renderer
handle or display name, so it survives buffer recreation and Surface swaps.
`kNoObject == 0` means nothing is selected; the bootstrap cube is `1`.

Stage 005 is a selection *foundation*: exactly one selectable object, no
hierarchy, no scene graph, no multi-select.

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
buffers, synchronization and the geometry buffers. It exposes `setCamera(const
CameraSnapshot&)` and `setModelTransform(const Mat4&)` and consumes both
verbatim; it derives no camera pose, builds no rotation, owns no Euler
convention and interprets no pointer data.

The mesh vertices are the box's **local** geometry and never move. Where the box
appears comes from the model transform, where the viewer stands comes from the
camera snapshot, and the renderer only composes them:

```
mvp = proj * view * model
```

So a screen-space change is always attributable: the camera moved, or the object
did.

Its only selection input is `setSelectionHighlight(bool)` — pure visual state,
delivered to the fragment shader as a tint push constant. The renderer is never
told *which* object is selected. Once per frame it asks `MeshStore` for the
current revision and uploads it only if it differs from the uploaded one, so
render and pick geometry both follow the same published revision and cannot
diverge.

## Threading

- Android UI thread: surface callbacks, `onTouchEvent` → JNI, the Construction
  panel and both its Applies, Construction publication, every brush stroke and
  its per-move sculpt publication, and DEBUG mesh fixture publication.
  `ConstructionObject` — its kind, every primitive's parameters and its
  transform — and `SculptSession` are both mutated from this thread only, which
  is why neither carries a mutex of its own; `MeshStore` has one and is what the
  render thread reads.
- Render thread (owned by `forgeshape_jni.cpp`): Vulkan work, presentation and
  every mesh buffer create/copy/destroy.
- One short-lived thread per DEBUG stress run, which publishes CPU revisions and
  exits. There is no general task or job system.

`MeshStore` has its own mutex, held only long enough to swap or copy a
`shared_ptr` — never across a GPU copy, and never together with the camera and
selection mutex in a way that could invert. A publisher never blocks a frame,
and the render thread's mesh upload happens outside `g_stateMutex` entirely.

`g_stateMutex` guards the single `CameraController`, the single
`SelectionController` and the single `ConstructionTransform` together. The UI
thread mutates all three under it; a tap resolves its pick against the camera
snapshot *and* the transform and updates the selection under one lock hold, so
they can never be seen out of step. The render thread takes the camera snapshot,
the derived model matrix and the selection highlight flag under the same lock
immediately before each `drawFrame()`, so a frame can never be drawn with the
camera from one transform state and the model from another.

## Lifecycle contract

The `CameraController`, the `SelectionController`, the `MeshStore`, the
`ConstructionObject` and the `SculptSession` are all process-scoped and outlive
every Surface: camera pose, selected `ObjectId`, the active primitive, its
authoritative parameters, its authoritative placement, the current mesh
revision, the active product mode and every sculpted vertex survive home/resume
and swapchain recreation. There is
no rehydration step, because nothing was discarded — the GPU mesh buffers are
device-scoped and are not destroyed when the Surface goes away, so a resume
re-presents the same revision without re-uploading it. Gesture tracking is separate — camera anchors and tap
candidacy are both reset on `ACTION_CANCEL`, on `surfaceDestroyed` and on
`surfaceCreated`, and a live brush stroke and any pending one are cancelled with them — so a Surface
swap can never leave a stale touch anchor, a half-finished tap or a
half-finished stroke behind, and never disturbs what is selected or what has
been sculpted. Neither camera
state nor selected identity is stored inside swapchain or surface resources.

## Boundary to future work

The Sculpt domain is a **vertical slice**, not a sculpt core. It sits beside the
renderer, not inside it — `Renderer` consumes geometry and a `CameraSnapshot` and
owns neither — and it is a *producer* of `MeshStore` revisions exactly as the
five primitives already are, which is why the upload path below `MeshStore` did
not have to change to accommodate it.

What it deliberately is not: there are **four** tools behind one kernel, and no
brush framework — no brush base class, no registry, no reflection and no plugin
surface. A fifth tool is another enum case, another `apply*` and another button;
that is a visible, deliberate cost, and it is the right one until something needs
brushes to be data rather than code. There is no remesh, no subdivision, no
dynamic topology and no way to change a vertex count at all; no sculpt undo and
no stroke history; no symmetry, no masking and no layers; no stylus pressure or
tilt; no brush presets; and exactly **one** Frozen Sculpt Mesh, for the one
object. The stroke publishes through the existing synchronous upload path and
does not have an uploader of its own — measured across 25 strokes and 1,170
uploads, every one reused capacity and none stalled, so there is nothing here to
optimise on evidence.

`SculptTopology` carries adjacency for a **fixed** topology and nothing more. It
is not a mesh library and not a step toward one: it has no edge collapse, no
split, no flip and no incremental update, because the only thing that can happen
to a frozen mesh is that a vertex moves.

`ConstructionObject` is **one** object and deliberately not a primitive
framework: there is no primitive base class, no polymorphism, no registry, no
property metadata, no reflection, no plugin surface, no second Construction
object, no hierarchy, and no create or delete. A further primitive is another
member, another `PrimitiveKind` case, another variant alternative and another
per-kind JNI method — that is a deliberate, visible cost, and it is the right one
until a real reason to generalise appears. It is visible precisely because it is
spelled out once per primitive rather than hidden behind a registry that would have to be
persisted, versioned and validated. What must NOT happen is a registry sneaking in as "just a
vector of objects": multi-object is a stage of its own, with selection,
identity-minting and lifetime consequences this design has not paid for.

Shape and transform editing exist; **scale** does not, and neither does a gizmo.
`ConstructionTransform` is deliberately rigid — rotation and translation only —
which is exactly what lets the picker use an exact composed inverse and keep the
ray's distance in world units. Adding scale breaks both of those and is a domain
change, not a matrix change.

The shared radial segment and latitude stack counts are fixed and are not
exposed. Making them editable would put the approximation into the product's
authored state, where it would then have to be persisted, versioned and validated
like a real parameter.

A consequence worth naming: the capsule's cylindrical middle is a **single band**
between its two seam rings, however long that middle is — exactly as the
cylinder's side wall has been since Stage 010. The shape, its bounds and its
picking are all exact regardless, but the middle carries no interior rings, so a
small sculpt brush placed there has very few vertices to capture. That is a
tessellation-fidelity limitation, not a correctness one, and closing it means
subdividing the middle by length, which is a stage of its own.

`ConstructionPanelView` is one panel for one object, not a property-editor
framework: there is no property model, no binding layer, no editor registry and
no second inspected object. Its primitive selector holds a *draft* kind and
changes nothing until Apply Shape.

`RuntimeMesh` is the smallest representation that lets geometry change at
runtime — **not** the Geometry Core and **not** the Construction mesh format. It
carries positions, colours and indices, and nothing else: no normals, no UVs, no
material, no topology adjacency, no history.

`forgeshape_demo_mesh` is the bootstrap cube's numbers only. Since Stage 006
neither the renderer nor the picker reads it, and since Stage 007 it is not on
any startup path; it is the source data for the baseline debug fixture.

`forgeshape_mesh_fixtures` is **DEBUG TEST INFRASTRUCTURE**, not product
geometry. Its baseline / deformed / spherified-box fixtures exist to prove the
upload path, are reachable only through a debug-only key hook that compiles to a
no-op in release, and are deliberately absent from `PRODUCT.md`. They are not
primitives, not a Construction feature and not a sculpting feature.

`SelectionController` holds a single `ObjectId`. Multi-object selection,
hierarchy and a scene graph are deliberately absent, and picking must stay out of
`CameraController`.
