# ForgeShape — Project Status

**Status Version:** 0.15.1  
**Updated:** 2026-08-19  
**Last Stage:** Stage 015A — UI/UX Audit + Architecture Decision Pack  
**Result:** COMPLETE — DECISION REQUIRED  
**Current Phase:** Phase 1 — Native Viewport  
**Workspace:** `D:\TRAVELAPPS\ForgeShape`  
**Next Stage:** Owner UI Architecture Decision for Stage 015B

Stage 015A was an audit and design stage. **No product behaviour changed and no
product source file was touched** — the diff is one new document, four audit
screenshots and this file. The previous stage, Gate P0, was likewise
toolchain-only (NDK r27 → r29, one line in `app/build.gradle`).

The decision pack is at
[`docs/ui/UX_ARCHITECTURE_DECISION_PACK.md`](docs/ui/UX_ARCHITECTURE_DECISION_PACK.md).
Six owner decisions are listed in its §12 and are summarised below; Stage 015B
cannot start until D1, D2 and D3 are answered.

## Product Direction

ForgeShape is an author-owned offline 3D modeling/sculpting application.

Production architecture must NOT use Godot, GDExtension, Unity, Unreal, or any
other general-purpose engine that owns the viewport or render loop.

## Verified Environment Facts

- JDK 21.0.9 (Android Studio JBR, `JAVA_HOME`).
- Android SDK at `C:\Users\damia\AppData\Local\Android\Sdk`.
- SDK platforms installed: android-33/34/35/36/36.1.
- Build tools installed: 27.0.0, 33.0.3, 35.0.0, 35.0.1, 36.1.0.
- NDK installed: 25.2.9519653, 27.2.12479018, **29.0.14206865** (installed in
  Gate P0 with owner approval, via `sdkmanager --install "ndk;29.0.14206865"`).
- **The project is pinned to NDK `29.0.14206865`** (`app/build.gradle`
  `ndkVersion`). r30 beta is prohibited. The historical baseline was r27.2.12479018.
- SDK CMake installed: 3.22.1, 4.1.2.
- Gradle distributions cached: 8.7, 8.13, 8.14.3. AGP 8.13.2 cached.
- `glslc` present at `<ndk>/shader-tools/windows-x86_64/glslc.exe`.
- Only one Android system image is installed:
  `system-images;android-36.1;google_apis_playstore;x86_64`.
- ForgeShape root Git **is initialized** as of Gate P0 (owner-approved, local
  only). There is no remote, nothing has been pushed, and no global Git config
  was written — the committer identity lives in `.git/config` alone. Before the
  gate there was no `.git` anywhere in the workspace and no `~/.gitconfig`.
  - baseline commit `5d386f03e7b33de47ea717a4a1d629231b073b56` —
    `baseline: accepted Stage 014`, 171 files.
  - `.gitignore` excludes Gradle/CMake/NDK output, `.cxx`, `.gradle`, IDE state,
    `local.properties`, APK/AAB output and tool-local scratch. It deliberately
    does **not** ignore `artifacts/`, because this file and `PRODUCT.md` cite
    those screenshots by filename as stage evidence.
- No Godot/GDExtension files remain in the workspace.

### Android runtime targets

**Reserved — unavailable to ForgeShape:** AVD `Medium_Phone_API_36.1` /
`emulator-5554` is used by another program. ForgeShape must not use, start,
stop, wipe, reconfigure, install to, send input to, log, or screenshot it until
the owner removes this constraint. It was not attached, was not running, and was
never addressed during Stage 004 or Stage 005.

**Contended — not used for authoritative evidence:** AVD `ForgeShape_Stage004` /
`emulator-5556`. Confirmed during Stage 005 that it is *not* exclusively
ForgeShape's: another program runs `com.damian.wlochyikafalonia.claude.debug` on
it, repeatedly brings it to the foreground, and injects taps that reach the
ForgeShape window. Two Stage 005 lifecycle runs were invalidated by a foreign tap.
Stage 006 did not use, start, stop, wipe or reconfigure it, and produced no
runtime evidence on it.

**Runtime target (created in Stage 006, isolated and ForgeShape-owned):**

| Property | Value |
| --- | --- |
| AVD name | `ForgeShape_Stage006` (own AVD definition, own data dir at `%USERPROFILE%\.android\avd\ForgeShape_Stage006.avd`) |
| adb serial / port | `emulator-5558` / port 5558 |
| Device profile | pixel_6, 1080x2400, density 420, multi-touch screen, GPU host |
| ABI | `x86_64` |
| API level | 36 (Android 16), system image `google_apis_playstore` (already installed; nothing was downloaded) |
| Vulkan capability | `android.hardware.vulkan.version=4206592` (0x402000 → 1.3), `android.hardware.vulkan.level=1`, compute supported |
| Vulkan at runtime | loader instance 1.4.0; physical device "Goldfish GFXStream (AMD Radeon RX 9070 XT)", device API 1.3.0; swapchain format 37 (`R8G8B8A8_UNORM`), 4 images, FIFO |

Launched detached with WMI `Win32_Process.Create`. No foreign foreground change
or foreign input was observed on it during Stage 006; ForgeShape was confirmed as
`topResumedActivity` before evidence-sensitive actions.

`adb root` is unavailable on Play Store system images, so `/dev/input/*`
`sendevent` injection is not possible; multi-touch is injected with the
on-device `uinput` tool instead. The `uinput` tool consumes a stream of
concatenated JSON objects (no enclosing array, no commas between them); an array
is rejected with `Error reading in object, ignoring.`

## Current Architecture

See `ARCHITECTURE.md` for the authoritative ownership map.

- Standalone Android application (`com.forgeshape.app`), Java shell, no Compose,
  no AndroidX, no third-party runtime library.
- ForgeShape-owned viewport: plain `android.view.SurfaceView` +
  `SurfaceHolder.Callback`; forwards surface lifecycle and raw pointer state only.
- `ConstructionPanelView` (Stage 008, extended in Stage 009): a plain
  framework-view overlay above the viewport in a `FrameLayout`. Owns field text,
  the selected mm/cm/m display unit, a DRAFT primitive kind and validation
  messages — and no parameter, no authoritative kind and no transform. `LengthUnit` owns exact `BigDecimal` unit conversion and
  formatting. Neither exists below the JNI boundary.
- JNI bridge: 5 lifecycle methods + 1 compact touch-event method + 6 Construction
  shape methods (read the active kind and every primitive's parameters; submit a
  box, a cylinder, a sphere, a cone or a capsule through **one method per
  primitive**, each taking exactly that primitive's parameters) + 2 Construction
  transform methods (read all six, submit all six). There is no generic
  `kind + a + b + c` shape method.
- Native `ConstructionTransform` owns the object's authoritative placement:
  position X/Y/Z as `double` **meters** and rotation X/Y/Z as `double`
  **degrees**, plus the derived model and inverse-model matrices. No JNI/Android/
  Vulkan/renderer/UI types, no mesh and no GPU resource — it cannot publish a
  mesh revision because it cannot reach `MeshStore`. Since Stage 010 it is held by
  `ConstructionObject` rather than being a singleton of its own.
- Native `CameraController` owns camera pose, projection and gesture state; it
  contains no JNI/Android/Vulkan types.
- Native `SelectionController` owns the selected `ObjectId` and the
  tap-versus-navigation decision. Neither the renderer nor the camera owns
  selected identity.
- Native picking module owns screen→world ray construction and CPU
  ray/triangle intersection; no JNI/Android/Vulkan/renderer types, no GPU id
  buffer.
- Native `ConstructionObject` is the single active Construction object: a stable
  `ObjectId`, a `PrimitiveKind` (`Box`, `Cylinder`, `Sphere`, `Cone` or
  `Capsule`), a `ConstructionBox`, a `ConstructionCylinder`, a
  `ConstructionSphere`, a `ConstructionCone` and a `ConstructionCapsule` each
  holding its own exact parameters, and a `ConstructionTransform`. There is
  exactly one; there is no registry, list, hierarchy or scene graph, and no
  create or delete. It is the startup geometry source.
- Native `ConstructionBox` owns exact width/height/depth as `double` **meters**
  and deterministic local mesh generation from them.
- Native `ConstructionCylinder` owns exact diameter/height as `double` **meters**
  and deterministic local mesh generation from them, at a fixed 32 radial
  segments.
- Native `ConstructionSphere` owns an exact diameter as `double` **meters** and
  deterministic local mesh generation from it, at a fixed 32 meridians × 16
  stacks.
- Native `ConstructionCone` owns an exact bottom diameter and height as `double`
  **meters** and deterministic local mesh generation from them. The apex radius
  is zero by definition and is not a parameter.
- Native `ConstructionCapsule` owns an exact diameter and **total** height as
  `double` **meters** and deterministic local mesh generation from them; the
  radius and the cylindrical middle are derived and never stored. No
  JNI/Android/Vulkan/renderer/UI types in any of the five.
- Native `kPrimitiveRadialSegments` (32) and `kPrimitiveLatitudeStacks` (16) are
  the single owner of round-primitive tessellation. Every curved generator names
  them rather than writing down a count of its own.
- Native `PrimitiveSpec` is the typed shape request: its payload is a
  `std::variant` of the five parameter structs, so a request contains one
  primitive's parameters and no other's, and its `kind()` is derived from that
  payload rather than stored beside it.
- Native `MeshStore` owns the current CPU mesh: immutable `RuntimeMesh`
  revisions, monotonic revision numbers, validation and the stable `ObjectId`.
  No JNI/Android/Vulkan types, no GPU resource.
- Native `forgeshape_demo_mesh` holds the baseline cube's numbers only; it is no
  longer read by the renderer or the picker and is no longer on any startup path.
- Native `forgeshape_mesh_fixtures` holds DEBUG test fixtures. Not product
  geometry, not primitives, not Construction.
- Native C++17 renderer owns `ANativeWindow`, Vulkan and its own render thread,
  consumes a `CameraSnapshot`, a derived model transform and a boolean selection
  highlight, and owns every mesh buffer create/copy/destroy. It builds no
  rotation and owns no Euler convention.
- Native `SculptSession` owns the product mode (`Construction` or `Sculpt`), the
  one `SculptMesh`, the active `SculptTool`, the brush settings and the live
  stroke. It contains no JNI/Android/Vulkan/renderer/UI type and holds no GPU
  resource. Mode and tool are native state; the Android UI may only *request* a
  change and is then told what is actually active.
- Native `SculptMesh` is the Frozen Sculpt Mesh: the same `ObjectId` as the
  Construction object, a **copy** of its local vertex/index data taken at the
  moment of Freeze, its fixed-topology `SculptTopology`, its current-position
  vertex normals and its own `SculptRevision`. Its only mutation is a single
  vertex *position* — there is no API that can add, remove or reorder a vertex or
  touch an index, so topology is preserved by construction.
- Native `SculptTopology` owns the 1-ring vertex neighbours and incident
  triangles of the frozen mesh, in flat CSR arrays. Built **once per Freeze** and
  never per move, because nothing can change an index. Not a half-edge structure:
  that is the right shape for changing topology, and nothing here can.
- Native `SculptStroke` is the one brush kernel shared by all four tools: the
  raycast, the affected-vertex set and its falloff weights, the radius resolution
  and the camera basis are implemented exactly once; a tool contributes a
  deformation rule and nothing else. It restates no FOV or aspect — it reads the
  `CameraSnapshot`'s own matrices.
- `ConstructionPanelView` is joined by `SculptPanelView`, a compact Sculpt Mode
  strip (a four-tool selector, Radius, Strength, Back to Construction). Exactly
  one panel is visible and which one is read back from
  `NativeViewport.productMode()`; which tool is highlighted is read back from
  `NativeViewport.sculptState`.
- Beyond this the Geometry/Sculpt Core does not exist: four tools and one closed
  enum, no brush registry, base class or plugin surface, no remesh, no sculpt
  undo, no symmetry, no masking, no layers, no pressure, and one Frozen Sculpt
  Mesh for the one object.
- No Godot, no GDExtension, no engine, no OpenGL ES fallback.

### Canonical winding and culling (established in Stage 005)

Triangle vertices are ordered **counter-clockwise viewed from outside the
surface**, in right-handed world space; the outward normal is
`(v1-v0) x (v2-v0)`, and a triangle is front-facing to a ray when
`dot(N, rayDir) < 0`. The Y-flipped Vulkan projection mirrors this into
framebuffer space, so the pipeline uses `VK_CULL_MODE_BACK_BIT` with
`VK_FRONT_FACE_CLOCKWISE`. CPU picking accepts front faces only under the same
rule, so what is pickable is exactly what is drawn. The existing cube indices
already obeyed this convention and were not changed; only the pipeline state was.

### Construction unit and data contract (established in Stage 007)

The internal Construction length unit is the **meter**, carried as `double` and
named `...Meters` at every boundary. `RuntimeMesh` positions are **derived
`float`** data. Authoritative dimensions are read only in the direction
parameters → mesh → store → picking/renderer → GPU; nothing infers a dimension
from vertices, a bounding box or GPU data. Since Stage 009 that mesh is the box's
**local** geometry, and placement is a separate truth applied on top of it.

A dimension is refused only when it is non-finite, non-positive, or when its
derived `float` half-extent would be zero or non-finite. There is no arbitrary
product size limit.

### Presentation unit boundary (established in Stage 008)

mm/cm/m exist **only** above the JNI boundary. Every value crossing into native
code is already in meters, and no native type, function or log line names a
display unit. Conversion is an exact `BigDecimal` decimal point shift
(`10^3` mm, `10^2` cm, `10^0` m), never a floating-point multiply, so unit
switching is lossless and the same box entered in a different unit produces the
identical `double`.

### Transform convention (established in Stage 009)

Right-handed world space, **+Y up**, column-vector math (`p' = M * p`),
column-major storage. A positive angle rotates by the **right-hand rule**.
Rotations compose in **local X → Y → Z** order:

```
Model    = T * Rz * Ry * Rx
Model^-1 = Rx(-x) * Ry(-y) * Rz(-z) * T(-p)
```

so `Rx` acts on the object first and the translation last — a rotated object at
(2, 0, 0) is still centred at (2, 0, 0). The renderer consumes `modelMatrix()`
and the picker consumes `inverseModelMatrix()`; neither builds a rotation itself,
so there is exactly one convention in the product. The inverse is composed from
the authoritative values rather than inverted numerically, which is exact for a
rigid transform.

Position is `double` meters, rotation is `double` degrees. Rotation values are
**not canonicalized**: 370° stays 370° in domain state, and reduction modulo 360
happens only inside the derived trigonometry. A transform value is refused only
when it is non-finite or could not survive into the derived `float` matrix; zero
and negative are ordinary for all six.

### Active object and primitive contract (established in Stage 010)

`ConstructionObject` owns identity, the active `PrimitiveKind`, **both**
primitives' parameters and the placement. Exactly one exists. Both primitives'
parameters are retained across a kind change, so Box → Cylinder → Box does not
forget the box's dimensions; only `kind` decides which set is authoritative right
now. `constructionTransform()` returns `constructionObject().transform()`, so
placement is not a second singleton and cannot drift from the object.

`applyPrimitive(object, store, spec)` is the single entry point: validate the
requested kind's parameters, update kind and parameters atomically, publish
exactly one revision if and only if something changed. A **kind change is always
a change**, even when the target primitive already held those parameters. The
transform is never read, written or reset by it, in any outcome.

Cylinder contract: centred on the local origin, axis along **local +Y**,
authoritative **diameter** and height as `double` meters (radius is derived, never
stored), both finite and > 0. Tessellation is fixed at **32 radial segments** and
is not user-editable — it is a detail of how an exact shape is drawn, not part of
what it is. Topology is `2N + 2` = 66 vertices and `12N` = 384 indices: two side
triangles per segment plus one cap-fan triangle per segment per cap. The four
cardinal directions use written-down sines and cosines rather than trigonometry,
so the generated X/Z bounds are **exactly** ±diameter/2 and the Y bounds exactly
±height/2. Winding is the canonical convention, so sides and both caps survive
back-face culling.

Between two ring vertices the surface is a flat facet, so a picked point on the
side lies between `radius * cos(pi/32)` and `radius` from the axis. That is the
tessellation showing through; the parameters stay exact.

### Typed primitive boundary (established in Stage 011)

A shape request is typed by kind from the UI down to the domain, and there is no
longer any place where a number's meaning depends on a separate kind field.

- `PrimitiveSpec`'s payload is
  `std::variant<BoxDimensionsMeters, CylinderDimensionsMeters,
  SphereDimensionsMeters>`. A spec built for a cylinder physically does not
  contain box or sphere values, so there is no inactive parameter group riding
  along and nothing to read by mistake. `kind()` is derived from the payload's
  alternative index, and a `static_assert` pins that order to `PrimitiveKind`'s.
- Access is typed: `box()`, `cylinder()` and `sphere()` each return a pointer
  that is null unless the spec really is that primitive.
- `ConstructionObject::spec()` returns only the **active** primitive's
  parameters. Remembered inactive parameters are still available, but only by
  asking for that primitive by name.
- The JNI shape boundary is **one method per primitive** —
  `applyConstructionBox(w, h, d)`, `applyConstructionCylinder(dia, h)`,
  `applyConstructionSphere(dia)`. The Stage 010 generic
  `applyConstructionPrimitive(kind, a, b, c)` is **removed**. Each method's
  parameter list is that primitive's parameter list, so a caller cannot put a
  height where a depth belongs and a sphere request has no spare number to get
  wrong.
- Below the boundary all three build the matching typed spec and go into the same
  `applyPrimitive`, so the update-then-publish rule still has one implementation.
- The shape read (`constructionPrimitive(double[7])`) still reports every
  primitive's parameters so the panel can populate an inactive draft, but each
  slot has one fixed meaning whatever the active kind is.

### Sphere contract (established in Stage 011)

Centred on the local origin. The authoritative parameter is the **diameter** as
`double` meters — one parameter is the whole of it — finite and > 0; the radius
is derived and never stored. The transform is separate and preserved.

Tessellation is fixed at **32 meridians × 16 latitude stacks** and is not
exposed. 32 is divisible by four so the cardinal meridians land on real vertices;
16 is even so exactly one ring falls **on** the equator. Topology is
`15 × 32 + 2` = 482 vertices and `2N(S-1)` = 960 triangles = 2880 indices: two
triangles per quad in each of the 14 full bands plus one fan triangle per
meridian at each pole.

Each pole is one vertex fanned to its adjacent ring, not a collapsed quad row, so
there are **no zero-area pole triangles**. The equator and the four cardinal
directions are written down rather than computed and the two exact factors are
multiplied in double before the single float conversion, so the local X, Y and Z
bounds are **exactly** ±diameter/2. Winding is the canonical convention; closure
is asserted directly — every directed edge appears once with its reverse present,
and `V - E + F = 2`.

The drawn surface is inscribed in the exact one, so a picked point lies between
`radius * cos(pi/32)^2` (0.9904 × radius) and `radius` from the centre. That is
the tessellation showing through; the parameter stays exact, and no diameter is
ever reconstructed from mesh floats.

### Cone contract (established in Stage 014)

Centred on the local origin, axis along **local +Y**, base at `y = -height/2`,
apex at `y = +height/2`, base **closed**. The authoritative parameters are the
**bottom diameter** and the **height**, both `double` meters, finite and > 0. The
apex radius is **zero by definition** and is not a parameter: there is no top
diameter and no frustum, because a truncated cone is a different shape rather
than a cone with an extra number.

Tessellation is the shared `kPrimitiveRadialSegments` = 32 and is not exposed.
Topology is `N + 2` = **34 vertices** and `6N` = **192 indices**: one side
triangle and one base-fan triangle per segment. Each base-ring vertex is
referenced exactly four times, which accounts for the whole index buffer.

The apex is **one vertex fanned to the base ring**, not a collapsed top ring, so
there are no zero-area triangles at the tip. Concretely the side triangle is the
cylinder's `(b0, t1, b1)` with the top ring collapsed to the apex; the partner
triangle that would have been degenerate does not exist. The four cardinal
directions are written down rather than computed, so the X and Z bounds are
**exactly** ±bottomDiameter/2 and the Y bounds exactly ±height/2 — asserted as
exact equality, not a tolerance, across five shapes including 40 m × 0.01 m and
0.01 m × 40 m. Winding is the canonical convention and closure is asserted
directly (`V - E + F = 2`, every directed edge once with its reverse present),
which is what would fail if the base were open.

### Capsule contract (established in Stage 014)

Centred on the local origin, axis along **local +Y**. The authoritative
parameters are the **diameter** and the **total** height — ends included, which
is what a caliper measures — both `double` meters, finite and > 0, with
`totalHeight >= diameter`. The radius (`diameter/2`) and the cylindrical middle
(`totalHeight - diameter`) are **derived and never stored**, for the same reason
a radius never is.

The middle is centred on the origin, so its ends sit at ±(totalHeight −
diameter)/2, and each hemispherical end caps that end at the same radius. Bounds
are **exactly** ±diameter/2 in X and Z and ±totalHeight/2 in Y. Each end is
exactly half of the sphere's latitude bands — `kCapsuleHemisphereBands` is
literally `kPrimitiveLatitudeStacks / 2` — so a capsule end *is* a sphere end.

Topology with a middle is `R * N + 2` = **514 vertices** and `6NR` = **3072
indices** for R = 16 rings. The cylindrical middle is **not a special case in the
index loop**: it is simply the band between ring H−1 and ring H, generated by the
same code as every other band.

**The equality case.** `totalHeight == diameter` is valid and describes a capsule
with no middle, which is a sphere. It is generated as one: the two seam rings are
**the same ring**, R is one smaller, that band does not exist, and the result is
exactly the sphere's **482 : 2880**. That is what keeps it free of a duplicated
zero-length ring and of zero-area triangles, and it is pinned by `static_assert`
rather than left to coincidence. Topology is therefore a deterministic *function*
of the parameters rather than a constant — the two forms are the two sides of one
exact double comparison, with nothing in between (a capsule at 2.0 × 2.0000001 m
has the full 514 : 3072 and no degenerate triangle).

The seam bands use a written-down `cos = 0, sin = 1`, so a seam ring lands
exactly on ±middle/2 at exactly the full radius; the poles are written down from
the authoritative **total** height rather than accumulated as `middle/2 + radius`,
so the Y bounds are exact rather than a rounding away.

### The capsule relation and float resolvability (established in Stage 014)

The capsule is the first primitive whose two parameters are **related** rather
than independent, so `DimensionValidation` gains one reason —
**`RelationInvalid`** — surfaced at the boundary as `APPLY_REJECTED_RELATION`.
`validateCapsuleMeters` owns both halves of the rule:

- `totalHeight >= diameter`, because the two hemispherical ends alone are already
  `diameter` tall. This is a distinct reason on purpose: "0.5 m is not a length"
  and "0.5 m is too short to be this capsule's total height" are different
  problems and deserve different messages.
- **float resolvability** — the generated positions must not merely be finite,
  they must be far enough apart to describe the shape. A 1e30 m capsule 1e-6 m
  across is finite in every coordinate and yet its whole hemisphere rounds to a
  single float. The check compares the pole against the first ring below it (the
  smallest latitude step anywhere on a hemisphere, so resolving it resolves all
  of them) and refuses as `NotRepresentable`.

The UI restates neither half: it refuses only what it can name from the text
alone, and the relation comes back to it as an ordinary rejection.

### Shading and normals (Stage 014 note)

`RuntimeMesh` carries positions, colours and indices and **no normals**, and the
renderer does no lighting — so the stage's shading-seam question does not arise
in the product as it stands. Cone base-versus-side hardness and capsule
body-to-hemisphere smoothness are not expressible today, and inventing a normal
channel to express them would be a material/shading stage, not this one. What the
generators do own is the *colour* sweep, which is derived presentation data: both
new primitives follow the existing convention (sweep with the angle, brighten
toward the top) and no dimension is ever inferred from it.

### Construction ↔ Sculpt representation contract (established in Stage 012)

The one object now has **two** representations, and they are separate truths that
never write to each other.

| | Construction Source | Frozen Sculpt Mesh |
| --- | --- | --- |
| owner | `ConstructionObject` | `SculptMesh` (inside `SculptSession`) |
| identity | `ObjectId` 1 | the **same** `ObjectId` 1 |
| content | `PrimitiveKind` + that kind's exact `double`-meter parameters | a **copy** of the generated local vertex/index data |
| placement | `ConstructionTransform` | none of its own — the same transform applies |
| revision | `MeshRevision` via `MeshStore` | its own `SculptRevision`, restarting at 1 on every Freeze |

- **Freeze copies.** Nothing is shared, so a sculpt edit cannot reach back into
  Construction data, and the Construction Source can regenerate its own mesh at
  any moment.
- **A sculpt edit changes no parameter, no kind and no transform.** It cannot:
  the sculpt module has no mutable access to `ConstructionObject`.
- **No Construction parameter is ever reconstructed from sculpt vertices.** That
  direction does not exist, exactly as it does not exist from the Construction
  mesh.
- The transform is deliberately **not** duplicated: both representations are
  local geometry under the one `ConstructionTransform`, so there is no second
  placement to keep in sync.
- `SculptRevision` is **not** a `MeshRevision`. The store's revisions count every
  published snapshot of whichever representation is active and never restart; a
  `SculptRevision` counts changes to *this* mesh and restarts at 1 on each
  Freeze. Neither is derived from the other.
- Vulkan buffers stay derived copies: `publishActiveRepresentation` publishes the
  Construction mesh in Construction mode and the sculpt mesh in Sculpt mode,
  through the existing `MeshStore` path. The renderer owns no sculpt truth and
  did not change.

**Stale-source policy.** A Construction change while a Frozen Sculpt Mesh exists
marks it `sourceStale` and does nothing else. The sculpt mesh is never silently
replaced or re-derived, the Sculpt panel says in as many words that the shape
changed and the sculpt was kept, and adopting the new source stays an explicit
user act — another Freeze, the only thing that clears the flag. There is no
automatic sculpt-edit transfer.

### Grab brush contract (established in Stage 012)

Radius is authored in **screen pixels** (24-600, default 120) and resolved to
object space at the depth of the stroke's hit point, so the brush feels the same
size at any zoom; it is a property of the gesture, not a length belonging to the
object. Strength (0.05-1.0, default 0.6) scales the displacement. Both are
**clamped**, never refused.

Depth is measured along the camera **forward axis**, not along the ray, and the
field of view comes from the snapshot's own projection term
(`proj.m[5] = -1/tan(fovY/2)`); nothing restates `kFovYRadians` or the aspect.

Everything is captured on DOWN and held fixed for the stroke: the affected vertex
set, the falloff weights, the base positions, the world camera plane, the
world-per-pixel scale and the inverse model. Each MOVE recomputes
`base + delta * weight` rather than accumulating, so the result depends only on
where the finger *is*, never on how many events it took to get there. Falloff is
`w = (1 - (d/r)^2)^2` — 1 at the centre, 0 at and beyond the rim, zero derivative
at both ends, so a stroke leaves no crease at the edge.

A miss starts **no** stroke, and neither does a brush that would capture no
vertex. A cancelled stroke is one that *stopped*, not one that is undone: there
is no undo in this stage and a partial one invented here would be worse than
none.

**Sculpt-mode gesture rule.** One finger down **on the Frozen Sculpt Mesh** is a
Grab stroke and owns the whole gesture; one finger down anywhere else navigates
as before, and two fingers pan and zoom as before. The rule is decided once, on
Down, and never revisited, so a stroke cannot turn into an orbit half way through
a drag. While a stroke owns the gesture neither `CameraController` nor
`SelectionController` sees the event at all. A second finger arriving mid-stroke
ends the stroke cleanly and hands the gesture to the camera, which re-anchors on
any pointer-set change and so produces no jump.

### Brush kernel and the four tools (established in Stage 013)

There is **one** stroke kernel. `SculptStroke` owns the whole of what a stroke
is — the raycast against the Frozen Sculpt Mesh, the hit anchor, the depth along
the camera forward axis, the screen-pixel → object-space radius resolution, the
affected vertex set with its falloff weights, base positions and base normals,
the camera basis, the inverse model, and the begin/move/end/cancel lifecycle. A
**tool is a deformation rule and nothing else**, selected by a closed enum and a
switch. There is deliberately no brush base class, registry, plugin surface or
reflection: four tools do not justify a framework, and a framework would have to
be persisted, versioned and validated like authored state.

| | driven by | direction | accumulates |
| --- | --- | --- | --- |
| Grab | where the finger **is** | camera plane | no — `base + delta × weight` |
| Clay | pointer **path length** | each vertex's normal **at stroke start** | yes |
| Smooth | pointer **path length** | toward the 1-ring neighbour mean | yes |
| Inflate | pointer **path length** | each vertex's normal **right now** | yes |

Grab keeps the Stage 012 contract exactly: it is position-driven, so the result
depends only on where the finger is, never on how many events it took to get
there.

The other three are **path-driven**, which is the whole of the accumulation rule.
For each move, `travelFraction = pointer travel this move / brush radius`, both
in pixels, and

```
amount = strength * localRadius * 0.35 * travelFraction     (Clay, Inflate)
lambda = strength * weight * 1.0  * travelFraction          (Smooth, capped 0.9)
```

so a slow stroke and a fast stroke over the same path do the same thing, a
stationary finger does nothing at all, and there is no timer, no per-event dab
and no dependence on how many `MotionEvent`s Android delivered. `amount` is
clamped to one local radius per move, so a teleporting pointer cannot produce an
unbounded displacement. Smooth's lambda cap is what makes repeated smoothing
convergent: a vertex can only move part of the way toward a mean it is already
surrounded by, so the deviation shrinks by `(1 - lambda)` per application and can
never change sign or overshoot.

**Clay is not Inflate, and the difference is in the formula, not a constant.**
Clay reads `baseNormal`, captured once at stroke start; Inflate reads
`mesh.vertexNormals()`, recomputed from the geometry it is deforming. The
consequence is directly measurable: a clayed vertex's *total* displacement stays
exactly parallel to the normal it started with however many moves it took, while
an inflated vertex's leaves that axis as soon as the surface has moved. The
self-tests assert both halves with thresholds three orders of magnitude apart
(clay off-axis < 1e-4, inflate off-axis > 1e-2), and the two produce visibly
different shapes at runtime.

Radius and Strength are **shared by every tool** — there is no per-tool copy of
either, so switching tools never changes how big or how strong the brush is.
Both still clamp rather than refuse.

### Navigation-versus-brush arbitration (established in Stage 013)

A one-finger Down on the Frozen Sculpt Mesh is ambiguous when it arrives: it is
either the start of a stroke or the first of the two fingers of a pan/pinch.
Stage 012 resolved it optimistically — the stroke began on Down — so a two-finger
gesture whose first finger landed on the mesh committed a stroke that ended a
moment later having moved nothing.

The rule is now **pending-then-promote**, decided entirely in native code:

| event | result |
| --- | --- |
| Down, one finger, ray hits the mesh | **PENDING**. The event is swallowed; no stroke exists, nothing is deformed, and neither the camera nor the selection sees it. |
| Move, still one finger, travelled ≥ 8 px | **PROMOTE**. The stroke begins at the **original down point**, then this same event is applied as its first move. |
| a second finger, an Up, a Cancel, any multi-pointer event | **ABANDON**. No stroke ever existed, so there is nothing to end and nothing to undo. |

The probe that decides "hits the mesh" is `SculptSession::hitsSculptMesh`, which
answers the question *without* starting a stroke and provably cannot move a
vertex, mint a revision or advance the stroke counter — that is the structural
half of the guarantee, and the self-tests assert it directly.

Because the stroke anchors at the original down point, the deferral costs
nothing: the hit, the affected set and the falloff weights are exactly what
Stage 012 would have captured. Because the camera re-anchors on any pointer-set
change, handing it an abandoned gesture produces no jump. Because the selection
never saw the Down, an abandoned gesture cannot select or clear either.

The 8 px arming threshold sits deliberately between touch jitter and the 24 px
tap slop, so a stroke still starts long before the gesture would stop being a
tap. The residual it accepts is stated plainly: a first finger that *deliberately
drags more than 8 px* before the second one lands does commit a stroke. That is
a gesture the user drove as a stroke, not a two-finger navigation, and closing it
completely would need a buffered/undoable stroke, which this stage does not have.

Both `g_grabbing` and `g_strokePending` are gesture routing only, guarded by
`g_stateMutex`, and dropped whenever the Surface goes away.

### Sculpt topology and normals (established in Stage 013)

Topology is fixed for the life of a frozen mesh, so the adjacency is built once,
in `SculptMesh::freezeFrom`, and reused by every stroke and every move.
`SculptTopology` stores 1-ring vertex neighbours and incident triangles in flat
CSR arrays: neighbour lists are sorted and deduplicated (a shared edge is seen
once from each of its two triangles, and without the dedup every interior
neighbour would be weighted double in a mean), out-of-range indices are skipped
rather than trusted, and no vertex is ever its own neighbour.

Vertex normals are **area-weighted**: each triangle contributes its unnormalized
`(v1-v0) x (v2-v0)` to all three corners, which weights a large triangle above a
sliver with no extra arithmetic, and each vertex normal is then normalized. A
degenerate triangle contributes a zero vector rather than a NaN, and a vertex
with no usable accumulated direction gets the **zero** vector rather than an
invented axis — so a normal-based brush simply does not move it. Every value
written is finite.

**Recomputation rule:** the cache is marked dirty by every accepted
`setVertexPosition` and recomputed on the next read. Normals are therefore
recomputed at most once per batch of position writes — in practice once per brush
move that changed anything — and never per frame and never when nothing moved.

### Dimensions vs placement (established in Stage 009)

`ConstructionBox` owns **what** the box is and generates its **local** mesh;
`ConstructionTransform` owns **where** it sits. A transform-only edit changes one
derived 4×4 matrix and nothing else: no vertex is rewritten, no `MeshRevision` is
published and no GPU upload happens. Picking moves the ray into local space
instead of moving the mesh, so what is drawn and what is pickable stay identical
with no second collision representation.

### The one Construction apply entry point (established in Stage 008)

`applyPrimitive(object, store, spec)` is the single place where a shape change
becomes a mesh revision: it validates, updates and publishes, and returns the
status, rejection reason, resulting spec, published counts and revision. The
update-then-publish orchestration that Stage 007 left in `forgeshape_jni.cpp`
lives here. JNI adds logging and a status code only; the DEBUG primitive driver
calls the same function. (Stage 008 introduced this as
`applyBoxDimensionsMeters`; Stage 010 generalised it to a whole primitive.)

### Object identity

`ObjectId` is an opaque `uint64_t` minted by the selection layer. It is never a
pointer, list index, Vulkan/renderer handle or display name. `kNoObject == 0`
means nothing is selected; the bootstrap demo object is `1`. Since Stage 006 it
lives on the `MeshStore`, not on the geometry, so replacing the mesh entirely
does not change what is selected — proven at runtime across three fixtures.

### Index width (established in Stage 006)

The runtime and render paths use 32-bit indices (`uint32_t` /
`VK_INDEX_TYPE_UINT32`). The previous 16-bit indices would have capped every
future mesh at 65,535 vertices; the CPU `TriangleMeshView` and the GPU index
buffer now share one width and cannot drift.

## Implemented Current Capabilities

Exact Cone and Capsule (new in Stage 014, proven at runtime on `emulator-5558`,
AVD `ForgeShape_Stage006`):

- **The Shape selector now offers five primitives** — Box, Cylinder, Sphere,
  Cone, Capsule — all five fully on screen and tappable at 1080 px width
  (`[37,64]`…`[1043,148]`). It moved to its own full-width weighted row, because
  five buttons beside a caption would have clipped the last one off the screen.
- **Selecting Cone or Capsule changes nothing at all.** Three selector taps
  produced **zero** ForgeShape log lines — no primitive apply, no revision, no
  upload — and the status line said "Cone selected — press Apply Shape to change
  the object." (CC-05)
- **Cone happy path.** Cone 1.6 m × 2.4 m applied as exactly one revision:
  `FORGESHAPE_CONSTRUCTION_PUBLISHED:8:34:192 kind=cone` and one
  `MESH_UPLOAD_OK:8:34:192`. The render is a coherent solid cone, apex up, with
  no culling artefacts (`stage014_cone.png`). (CC-06)
- **Cone picking follows the exact taper, measured at the pixel.** Two side hits
  came back at `(-0.0496,-0.2568,0.4807)` and `(0.2561,0.0019,0.3040)`; the exact
  radius at height y is (1.2 − y)/3, giving 0.4856 and 0.3993 against measured
  radial distances of 0.4832 and 0.3995 — both within the facet inradius
  `cos(pi/32)` = 0.99518. Off the silhouette and far below both **missed**.
- **The cone's base is closed, shown two ways.** Orbited underneath (pitch
  −1.52) it renders as a **filled disc**, not a hollow shell
  (`stage014_cone_base.png`), and two taps on it hit at exactly
  `y = -1.2000` — precisely −height/2 — on two different triangles, with a miss
  just past the rim.
- **Capsule happy path.** Capsule 1.2 m × 3.0 m total applied as exactly one
  revision: `PUBLISHED:9:514:3072 kind=capsule` and one upload. The render shows
  parallel straight sides with smoothly rounded ends and no visible seam
  (`stage014_capsule.png`). (CC-07)
- **The capsule's middle really is a cylinder and its ends really are
  hemispheres.** Two taps 1.8 m apart in y both landed at radial distance
  **0.597** from the axis (0.6 × cos(pi/32) exactly); a tap on the lower end
  landed at `(0.3071,-1.2575,0.3647)`, which is **0.5959** from that end's own
  centre (0, −0.9, 0) against an exact 0.6. Beside the silhouette and past the
  total height both missed.
- **The capsule relation is real and is reported in words.** Total Height 0.5 m
  against Diameter 1.2 m logged
  `PRIMITIVE_REJECTED:ui:relation_invalid requested=(capsule dia=1.200000m
  totalH=0.500000m) retained kind=capsule capsule dia=1.200000m totalH=3.000000m
  rev=9` and showed "Rejected: Total Height cannot be less than Diameter — the
  two rounded ends alone are that tall. Object unchanged." (CC-08)
- **No-ops cost nothing, for both new primitives.** Re-applying the identical
  capsule and the identical cone each logged `PRIMITIVE_UNCHANGED` at the
  unchanged revision and published nothing. Invalid cone values (diameter 0,
  height −2) were refused by the panel with the field named, producing **zero**
  native calls. (CC-08)
- **Transform-only edits publish nothing, for both.** Cone at pos (1.5,0,0)
  rot (0,0,90) and capsule at pos (0.4,−0.3,0) rot (0,0,45) each logged one
  `CONSTRUCTION_TRANSFORM` line and **0** publish/upload lines, revision held.
  (CC-09)
- **Transformed picking is exact.** On the cone laid along world X, a tap
  returned `(0.9050,-0.1098,0.1689)`: 0.605 m along the axis from the moved apex
  at x = 0.3, where the exact radius is 0.2017 — against a measured
  perpendicular distance of **0.2015**. Where the cone used to be, and screen
  centre, both **missed**. (CC-09)
- **The five-kind round trip preserves everything.** Box → Cylinder → Sphere →
  Cone → Capsule → Box and back: `objectId=1` throughout, the transform never
  counted an update, and the cone's remembered 1.6 / 2.4 came back into the
  fields untouched after a capsule detour. (CC-10)
- **The stale-source policy holds across the new primitives.** A capsule frozen
  (`SCULPT_FROZEN:514:3072 sculptRev=1 objectId=1`, `adjacency=3072` = 2E for
  E = 514 + 1024 − 2) and sculpted to `sculptRev=42`, then the Construction
  Source changed to a **cone**: `SCULPT_SOURCE_STALE:ui sculptRev=42`, the sculpt
  mesh untouched. Resume Sculpt returned `sculptRev=42 v=514 i=3072 stale=1
  freezes=1` — the sculpted capsule, not the cone — and the viewport was
  **pixel-identical**: 1,836,000 pixels compared, **0** differing, with the panel
  saying the shape changed and the sculpt was kept. (CC-11)
- **A transformed non-default capsule survives home/resume.** Viewport
  **byte-identical** below the panel — 1,296,000 pixels, **0** differing — with
  no re-publish and no re-upload, the same PID, and the panel re-reading
  1.2 / 3 / (0.4, −0.3, 0) / (0, 0, 45) back out of native state. (CC-12)
- **The three older primitives are untouched.** Applied through the real UI after
  all of the above: box `8:36`, cylinder `66:384`, sphere `482:2880`, each one
  revision, `objectId=1`. (REG-01)
- 163 debug-only cone/capsule self-checks run once at startup and pass, alongside
  the seven previous suites — **992 checks, all green**, no crash and one PID for
  the whole session.

Sculpt brush kernel and four tools (new in Stage 013, proven at runtime on
`emulator-5558`, AVD `ForgeShape_Stage006`, from a Frozen Sculpt Sphere of
2.0 m):

- **The Sculpt panel offers four tools** — Grab, Clay, Smooth, Inflate — as a row
  of buttons above the shared Radius and Strength sliders. The highlighted button
  is the tool native code reports, not the one last tapped. Every selection was
  honoured: `FORGESHAPE_SCULPT_TOOL:clay requested=1 known=1`, and the same for
  `smooth`, `inflate` and `grab`.
- **Multi-touch navigation cannot mutate the sculpt mesh.** A real two-finger
  gesture injected through `uinput`, with the **first finger landing on the
  mesh**, logged `SCULPT_STROKE_PENDING sculptRev=1` then
  `SCULPT_STROKE_ABANDONED:navigation sculptRev=1` and went on to
  `CAMERA_ZOOM_OK` (distance 8.2 → 1.2104). No `STROKE_BEGIN`, no `STROKE_MOVE`,
  `sculptRev` unchanged at 1, `strokes=0`, `storeRev` unchanged at 9. A second
  such gesture (pinch back out, distance → 4.9084) reproduced it exactly.
- **Grab is unchanged and still numerically correct.** A 340 px drag captured
  4 vertices at `radiusLocal=0.2261`, produced 53 `STROKE_MOVE:grab` lines
  (`sculptRev` 1 → 54) and a visible spike. Final `local=(0.2963,0,-0.2496)`,
  magnitude 0.3874; independently 340 px × (0.2261/120) world/px × 0.6 strength =
  0.3844, and X = −Z, which is the camera-right vector at yaw 0.7. Both the
  camera-plane derivation and the world→local conversion are proven from the
  running app.
- **Clay visibly builds volume.** Three 500 px strokes at 151 px radius,
  strength 1.00, on the upper-left of the sphere produced a large protruding
  wedge that breaks the silhouette (`stage013_clay.png`), leaving the rest of the
  object untouched.
- **Smooth visibly removes a spike.** Two 113 px strokes at 312 px radius over
  the Grab spike (102 `STROKE_MOVE:smooth` lines, `sculptRev` 54 → 156) collapsed
  it from a broad arrowhead protrusion to a thin sliver
  (`stage013_smooth_before.png` → `stage013_smooth_after.png`).
- **Inflate visibly expands and is materially different from Clay.** The same
  brush and an equivalent stroke pattern on the lower-right produced a broad
  outward expansion of the surface, distinctly rounder and flatter along the
  surface than Clay's angular extrusion (`stage013_inflate.png`).
- **Radius has a large effect on both non-Grab tools.** Same object, same stroke:
  Clay at 312 px captured **51** vertices at `radiusLocal=0.6031`, at 151 px
  **11** at 0.2956; Inflate at 312 px captured **29** at 0.6032, at 151 px **8**
  at 0.3005.
- **Strength scales both non-Grab tools, measured under control.** Four runs,
  each from a **freshly frozen** sphere with an identical 500 px swipe at 151 px
  radius, summing the logged per-move centre displacement:

  | tool | strength 0.060 | strength 1.000 | ratio |
  | --- | --- | --- | --- |
  | Clay | 0.01769 | 0.31279 | 17.7 |
  | Inflate | 0.01825 | 0.31509 | 17.3 |

  against a requested ratio of 16.7; the remainder is one extra sampled move in
  the stronger runs. All four captured 14 vertices at `radiusLocal=0.3015`, so
  only the strength differed.
- **Tool switching preserves everything.** Grab → Clay → Smooth → Inflate → Grab
  in one session: every stroke used the requested tool, no `SCULPT_MODE` line was
  emitted (no mode reset), `freezes` stayed at 5 (no automatic re-freeze) and
  `sculptRev` advanced monotonically to 282 across 25 strokes.
- **The Construction Source is untouched by all four brushes.** After the
  multi-brush session the parameters were bit-identical
  (`kind=sphere sphere dia=2.000000m objectId=1 updates=1 rejects=0`) and so was
  the placement (`pos=(0,0,0)m rot=(0,0,0)deg identity=1 updates=0`). The
  Construction view rendered **before** and **after** the session is a
  byte-identical 209,258-byte PNG.
- **Construction ↔ Sculpt preserves both representations.** After Back to
  Construction and Resume Sculpt, the sculpted viewport was **byte-identical**:
  7,689,600 bytes (1,922,400 pixels) below the panel, **0** differing. The panel
  region does differ, because `refreshFromNative` rewrites the status line — that
  is the panel reading native truth, not the model changing.
- **Picking follows the deformation, measured at the pixel.** Probing outward
  from the sphere centre along the sculpted upper-left direction, the sculpt mesh
  was hit at 380, 410, 440 and **470** px and missed at 500 px; along the
  opposite direction it was hit at 380 and 410 px and **missed at 440** px. The
  source sphere's silhouette is ~430 px, so the pickable surface reaches ~40 px
  past it exactly where material was deposited and stops at it elsewhere.
- **A multi-brush sculpt survives home/resume.** With Inflate active, the
  viewport came back **byte-identical** below the panel (7,689,600 bytes, 0
  differing), `mode=sculpt tool=inflate sculptRev=282 freezes=5` — no re-freeze,
  no tool reset — the Construction parameters still bit-identical, and no crash
  (same PID throughout the session, empty crash buffer).
- **Topology never changes and buffers are never reallocated.** Every published
  sculpt revision was `482:2880`. Across 1,176 published revisions and 1,170
  uploads: `reuse=1168`, `grows=4`, `sgrows=2`, and `vcap=11568 icap=11520
  scap=23088` never moved after the sphere's initial allocation. No stall,
  hitch or failed upload was observed at any point (`failed=0`).
- **Adjacency and normals are cheap and correct.** The frozen sphere reports
  `adjacency=2880` neighbour entries, which is exactly `2E` for
  `E = V + F - 2 = 482 + 960 - 2 = 1440` — Euler's formula, from the running app.
  `normalRecomputes=514` across 282 sculpt revisions and 25 strokes, i.e. under
  two per revision, and the adjacency `buildCount` stays at 1 across a whole
  stroke.
- **A brush gesture still cannot select.** Zero `SELECTION_CHANGED` lines were
  produced by any stroke.
- 232 debug-only brush-kernel self-checks run once at startup and pass, alongside
  the seven previous suites (597 checks), all green.

Sculpt vertical slice — Freeze plus Grab (new in Stage 012, proven at runtime on
`emulator-5558`):

- **Freeze to Sculpt** copies the Construction object's current local mesh into a
  Frozen Sculpt Mesh and enters Sculpt Mode. From a non-default source (sphere
  2.0 m at `pos=(0.4,-0.3,0)m rot=(0,0,45)deg`) it produced
  `FORGESHAPE_SCULPT_FROZEN:482:2880 sculptRev=1 objectId=1` — exactly the
  source's counts, exactly the source's identity.
- **Freeze changes nothing visible.** The viewport is **pixel-identical** before
  and after: 313,200 sampled pixels below both panels, **0** differing.
- Sculpt Mode replaces the Construction panel with a compact strip — Grab, a
  Radius slider, a Strength slider and Back to Construction — so the shape and
  transform editors are not merely disabled but absent.
- **Grab starts only on a mesh hit.** The same pixel that reported
  `FORGESHAPE_PICK_MISS` before a stroke reported
  `FORGESHAPE_SCULPT_GRAB_BEGIN:10` after one, and a pixel past the deformation
  still missed. That is the pickable surface following the deformation, measured
  at one pixel.
- A real Android drag (798,1308) → (1000,1308) captured 17 vertices at
  `radiusLocal=0.4229` and produced 42 `SCULPT_GRAB_MOVE` lines, taking
  `sculptRev` 1 → 43 and `storeRev` 9 → 51, with a **visible** spike pulled out
  of the sphere.
- **The brush math is confirmed numerically at runtime, not just asserted.** The
  final displacement was `local=(0.2313,-0.2313,-0.2755)`, magnitude 0.4277.
  Independently: 202 px × 0.0035242 world/px × 0.6 strength = 0.4271. And X = −Y
  exactly, which is `Rz(-45°)` applied to a world vector in the XZ plane — the
  camera-right vector at the current pose. Both the camera-plane derivation and
  the world→local conversion are therefore proven from the running app.
- **Radius has a large, controllable effect.** Same drag, same object, three
  brushes: 64.3 px → `radiusLocal=0.2267`, **5** vertices, a small nub;
  120 px → 0.4229, **17** vertices, a narrow spike; 409.9 px → 1.4447, **256** of
  482 vertices, the whole side stretched into an egg.
- **Strength scales the displacement linearly.** Same radius (409.9 px) and the
  same drag: strength 0.050 → |local| 0.0355; strength 1.000 → 0.7039. Ratio
  19.8 against a requested 20.0, the remainder being where the swipe's last
  sample landed. The affected set (256) was identical at both.
- **Topology never changes.** Every published sculpt revision was `482:2880` and
  every upload was `reuse` — no buffer growth at any point during any stroke.
- **The Construction Source is untouched by sculpting.** After a visible
  deformation the parameters were bit-identical
  (`kind=sphere sphere dia=2.000000m updates=1`) and so was the placement
  (`pos=(0.400000,-0.300000,0.000000)m rot=(0.000000,0.000000,45.000000)deg
  updates=1`). `objectId` stayed 1 throughout.
- **Back to Construction shows the original, exactly.** The Construction view
  after a visible sculpt is **pixel-identical** to the same view taken before the
  Freeze: 313,200 sampled pixels, **0** differing.
- **Resume Sculpt restores the prior edits without re-freezing.** `freezes` stayed
  at 1 and `sculptRev` at 43, and the restored sculpt view was
  **pixel-identical** to the one taken right after the stroke: 491,400 sampled
  pixels, **0** differing.
- **Navigation is intact in Sculpt Mode.** A one-finger drag from empty viewport
  orbited (`CAMERA_ORBIT_OK`, yaw 0.7 → −1.14) with no stroke; a two-finger
  gesture panned and zoomed (`CAMERA_PAN_OK`, `CAMERA_ZOOM_OK`, distance
  8.2 → 4.07) with no stroke. Neither picked, cleared or sculpted.
- A two-finger gesture whose **first** finger lands on the mesh begins a stroke
  that the second finger immediately ends having moved nothing (`sculptRev`
  unchanged at 49), after which the gesture zooms normally. This is the one
  visible seam in the gesture rule and it is harmless.
- A **tap** on the mesh is a stroke that moved nothing: `SCULPT_GRAB_BEGIN`
  followed by `SCULPT_GRAB_END` at the same `sculptRev`, no `PICK_*` line and no
  `SELECTION_CHANGED`. A brush gesture cannot select or clear.
- **The stale-source policy is real and visible.** Changing the sphere to 1.2 m
  in Construction Mode logged `FORGESHAPE_SCULPT_SOURCE_STALE:ui sculptRev=49`,
  left the sculpt mesh untouched, and on returning to Sculpt showed the unchanged
  2.0 m sculpted egg with an amber panel line saying the shape changed after the
  Freeze and the sculpt was kept.
- **A sculpted mesh survives home/resume.** The viewport came back
  **byte-identical** below the panel (518,400 sampled pixels, 0 differing) with
  `mode=sculpt sculptRev=49`, uploads unchanged at 239 — **no re-upload** — the
  Construction parameters still bit-identical, and picking still hitting the
  sculpted geometry.
- 126 debug-only Freeze/Grab self-checks run once at startup and pass.

Sphere primitive, typed shape boundary and shape UI (new in Stage 011, proven at
runtime on `emulator-5558`):

- The panel's **Shape** selector now offers Box / Cylinder / Sphere. Sphere shows
  a single Diameter field; only the selected primitive's row is on screen, and
  the panel's height is unchanged (bottom edge still ≈ y 985 of 2400), so no
  scroll or collapse was needed.
- Moving the selector to Sphere is a **draft**: `kind=box`, `currentRev=7`,
  `uploads=1`, `objectId=1` and the identity transform were all untouched, and
  **zero** native calls were made.
- Apply Shape with 150 cm produced exactly
  `kind=sphere sphere dia=1.500000m`, exactly one new revision (7 → 8) with
  topology `482:2880`, through the existing dynamic-mesh upload path
  (`MESH_UPLOAD_OK:8:482:2880:grow`), `objectId=1`, transform still identity.
- The sphere renders coherently — a clean round silhouette with correct depth and
  culling and no gaps at the poles or the seam.
- Sphere picking is exact. Two taps in materially different regions hit at
  0.74530 and 0.74719 from the centre, both inside the facet band
  [0.7428, 0.75] for a 0.75 m radius; a tap beside the silhouette reported
  `PICK_MISS`.
- A transform on the sphere published nothing: after
  `pos=(0.5,-0.6,0)m rot=(30,45,90)deg`, `currentRev` stayed 8, `uploads` stayed
  2 and every capacity was unchanged. Picking followed it — the moved sphere was
  hit at 0.74501 from its new centre. **Rotation is proven from native state**
  (`rot=(30,45,90) identity=0`), not from the silhouette: a sphere has no visible
  rotation, and the report does not claim one.
- A sphere diameter change (150 → 60 cm) published exactly one revision (8 → 9),
  `reuse` with unchanged capacity, transform untouched, and picking followed the
  new radius: the tap that had hit the 1.5 m sphere reported `PICK_MISS`, and a
  tap through the centre hit at 0.29773 — inside [0.29712, 0.3].
- Re-applying the identical diameter reported `Unchanged` with no revision and no
  upload. Blank, zero, negative and malformed (`1.2.3`) diameters were each
  refused with a named message, leaving kind, parameters, transform, revision and
  upload count bit-identical and making **zero** native calls.
- Sphere → Cylinder (80/200 cm) → Box (150/80/40 cm) each cost exactly one
  revision (9 → 10 → 11), `reuse` both times, `objectId=1` throughout, and the
  transform `pos=(0.5,-0.6,0)m rot=(30,45,90)deg` preserved across both. Box
  picking was exact again on the rotated placement (a hit on the local +Z face
  0.2050 from the centre, half the 0.4 m depth).
- All three primitives' parameters are remembered independently and never mix:
  with the box active, the drafts still read sphere 60 cm and cylinder 80/200 cm.
- A non-default transformed sphere (90 cm) survived home/resume with kind,
  parameters, transform, revision and upload count all preserved and no
  re-upload; the panel refreshed the selector and every field from native truth,
  and picking still hit at 0.44772 from the centre (radius 0.45).

Cylinder primitive and shape UI (new in Stage 010, proven at runtime on
`emulator-5558`):

- The panel gained a **Shape** selector (Box / Cylinder). The dimension button is
  now **Apply Shape** and submits the selected kind together with its parameters.
  Box shows Width/Height/Depth; Cylinder shows Diameter/Height; only the selected
  primitive's fields are on screen.
- Moving the selector is a **draft**: with Box active, selecting Cylinder left
  `kind=box`, `currentRev=7`, `uploads=1`, the transform and the `ObjectId` all
  untouched, and made **zero** native calls.
- Apply Shape with 120 / 240 cm produced exactly
  `kind=cylinder cylinder dia=1.200000m h=2.400000m`, exactly one new revision
  (7 → 8) with topology `66:384`, through the existing dynamic-mesh upload path
  (`MESH_UPLOAD_OK:8:66:384:grow`), `objectId=1`, transform still identity.
- The cylinder renders coherently: curved silhouette, correct depth, and both the
  side wall and the caps present — verified from above, from below (bottom cap)
  and lying on its side (end cap).
- Cylinder picking is exact. A side tap hit 0.59713 from the axis, which is the
  32-segment facet radius `0.6 * cos(pi/32) = 0.59711`, not the nominal 0.6. Cap
  taps hit exactly `y = -1.2000` = −height/2. A tap beside the silhouette
  reported `PICK_MISS`.
- A transform on the cylinder published nothing: after `pos=(0,-0.6,0)m
  rot=(0,0,90)deg`, `currentRev` stayed 8, `uploads` stayed 2 and capacities were
  unchanged. Picking followed it — the lying cylinder was hit at 0.59709 from its
  new axis.
- A cylinder parameter change (60 / 300 cm → 0.6 × 3.0 m) published exactly one
  revision (8 → 9), `reuse` with unchanged capacity, transform untouched, and
  picking followed the new radius: the tap that had hit the 0.6 m tube now
  reported `PICK_MISS`, and taps 100 px lower hit at 0.2998–0.29856 from the
  axis — inside `[0.3 * cos(pi/32), 0.3]`.
- Re-applying identical cylinder parameters reported `Unchanged` with no revision
  and no upload. Zero diameter, negative height and malformed text were each
  refused with a named message, leaving kind, parameters, transform, revision and
  upload count bit-identical and making no native call.
- Cylinder → Box restored an explicit 150 / 80 / 40 cm box in exactly one
  revision (9 → 10), `reuse`, `objectId=1`, and the transform
  `pos=(0,-0.6,0)m rot=(0,0,90)deg` preserved. Box picking was exact again:
  `y = 0.1500` = −0.6 + 0.75, the rotated half-width.
- A non-default transformed cylinder (0.9 × 1.8 m) survived home/resume with kind,
  parameters, transform, revision and upload count all preserved and no
  re-upload; the panel refreshed the Shape selector and every field from native
  truth.

Transform core and UI (new in Stage 009, proven at runtime on `emulator-5558`):

- The panel gained a Position X/Y/Z section (in the shared display unit) and a
  Rotation X/Y/Z section (degrees only), with a separate **Apply Transform**.
  The existing dimension Apply is unchanged in behaviour; Stage 010 renamed it
  again, to **Apply Shape**.
- Startup placement is position 0/0/0 m, rotation 0/0/0°, and the fields read it
  from native truth (`identity=1`).
- A position Apply of 0.75 / 0.25 / 0 m produced exactly
  `pos=(0.750000,0.250000,0.000000)m`, moved the box visibly, and moved what can
  be tapped with it: a tap on the top face of the 1.0 m tall box hit exactly
  y = 0.7500 (0.25 + 0.5).
- A rotation Apply of 15 / 30 / 10° produced exactly
  `rot=(15.000000,30.000000,10.000000)deg` and a hit at
  (0.2735, 0.3953, 0.2095) — a point on no axis-aligned face plane of the
  unrotated box, so the picker is reading rotated geometry.
- Combined placement is renderer/picker coherent. With the 2.0 m wide box rotated
  90° about +Z at y = −1.25 m, three separate taps all hit exactly y = −0.2500 on
  triangles 6/7 — the box's local **+X** face, which is what a +90° Z rotation
  turns into the world top face. An unrotated box there would have topped out at
  −0.75.
- **A transform-only edit costs nothing on the GPU.** Across five applied
  transforms the mesh revision stayed 7, `published` stayed 7, `uploads` stayed 1
  and `vcap/icap/scap` and `grows/sgrows` never moved.
- Re-applying an identical transform reports `Unchanged` and changes no state.
- Blank, malformed and one-bad-value-out-of-six input each show a message naming
  the field; all four transform values and all three dimensions are retained, and
  the request never reaches native code.
- Size and placement are independent in both directions. Applying
  1.5 × 3.0 × 0.4 m under a non-zero transform published exactly one new revision
  (7 → 8, one upload, `reuse`, capacities unchanged) and left the transform at
  `pos=(0,-1.25,0)m rot=(0,0,90)deg updates=5`. Picking then found the resized box
  at the same placement: top face at exactly y = −0.5000 (−1.25 + 0.75) and the
  +Z face at exactly z = 0.2000 (half the new 0.4 m depth).
- Placement, dimensions, `ObjectId`, mesh revision and the selected display unit
  all survive home/resume with no re-upload, and every field is rewritten from
  native truth on resume.

Dimension editing UI (new in Stage 008, proven at runtime on `emulator-5558`):

- A compact top overlay shows Width, Height and Depth, one shared mm/cm/m unit
  selector and an Apply button, built from plain framework views. No Compose, no
  AndroidX, no third-party UI dependency, no toolbar/drawer/design system.
- The displayed numbers come from native truth: at startup the fields read
  `2 / 1 / 0.5` in m, matching the authoritative `w=2.000000m h=1.000000m
  d=0.500000m`. No dimension is stored or defaulted in Java.
- Switching m → cm → mm → m converts correctly (`2/1/0.5` → `200/100/50` →
  `2000/1000/500` → `2/1/0.5`) and touches native state **not at all**: no
  parameter change, no mesh revision, no upload, no native call.
- Apply submits all three dimensions in one call. 125 / 250 / 75 cm produced
  exactly `w=1.250000m h=2.500000m d=0.750000m` and one new revision.
- A valid Apply reuses GPU capacity (same topology): `MESH_UPLOAD_OK:8:8:36:reuse`
  with `vcap/icap/scap` and `grows/sgrows` unchanged.
- Picking follows the applied dimensions: a tap on the top face of the 2.5 m tall
  box hit exactly y = 1.2500, and on the 0.42 m box exactly y = 0.2100.
- `.` and `,` are both accepted as the decimal separator. `333,3` cm applied as
  exactly 3.333 m; 42 cm → 0.42 m; 112.5 cm → 1.125 m.
- Re-applying an equivalent state is `Unchanged` with no revision and no upload —
  judged on the meter value, not the text: 125 × 250 × 75 cm and the same box
  written as 1.25 × 2.5 × 0.75 m both reported `Unchanged` at `rev=8`.
- Blank, negative, zero and malformed (`1.2.3`) input each show a message naming
  the field, focus it, and change nothing. All four were refused in the UI before
  reaching native code, so native dimensions, revision, upload count and reject
  count were all bit-identical before and after.
- Panel touches never drive the camera and never pick; the viewport outside the
  panel orbits and picks normally. Touching the viewport also takes focus and the
  soft keyboard away from a field being edited.
- Dimensions, `ObjectId`, revision and the selected display unit all survive
  home/resume with no re-upload, and the fields are rewritten from authoritative
  truth on resume — a stale invalid `0` in the Height field became `42` cm.

Display-unit persistence contract: the selected unit is **preserved** for the
life of the app process, including across home/resume. It returns to `m` only
when the process restarts. This is deliberate and tested.

Construction box (new in Stage 007, proven at runtime on `emulator-5558`):

- The app's startup geometry is generated by `ConstructionBox` from its
  authoritative `double` meter parameters, not by a debug fixture. The default is
  a deliberately non-cubic 2.0 × 1.0 × 0.5 m box centred at the world origin.
- Generated X/Y/Z extents match the requested dimensions to within one float
  rounding of the half-extent; the centre is exactly the origin (min and max are
  exact negatives on every axis).
- The mesh is 8 corners and 36 `uint32_t` indices in the canonical Stage 005
  winding, and passes `RuntimeMesh` validation.
- Dimensions can be changed at runtime with no APK rebuild, reinstall or
  relaunch; each valid change publishes a new same-topology mesh revision.
- Because topology is constant, a dimension change **reuses** Stage 006 GPU
  capacity: `reuse`, `grows` and `sgrows` unchanged, capacities unchanged.
- An identical-dimensions request is a no-op: no parameter change, no revision,
  no upload.
- An invalid dimension **fails closed**: all three values are validated before
  anything is written, the previous valid parameters stand, the current mesh
  revision is untouched, and nothing partial is published.
- The stable `ObjectId` (1) is independent of the parameters and of the mesh, so
  it survives every dimension change.
- Picking reads the generated box: a tap on the top face of the 0.42 m tall
  state-C box hit exactly y = 0.2100.
- Authoritative dimensions, `ObjectId` and the current mesh revision all survive
  home/resume with no re-upload.

The dynamic-mesh block below was proven at runtime on `emulator-5558` during
Stage 006. The block after it was established on `emulator-5556` in Stages
003-005; Stage 006 re-verified the parts it exercised on `emulator-5558`
(startup and present, orbit, pan, pinch, tap-to-select, tap-to-clear, drag does
not select, home/resume persistence, culling correctness) and did not re-measure
the rest.

Dynamic mesh (new in Stage 006):

- Mesh geometry can be replaced at runtime, repeatedly, with no APK rebuild,
  reinstall or relaunch.
- `MeshStore` publishes immutable revisions with monotonically increasing
  numbers. Invalid data fails closed and the previous revision stays current.
- A revision that is not strictly newer can never replace the current one.
- Vertex and index buffers are `DEVICE_LOCAL` with `TRANSFER_DST`, written
  through one reused `HOST_VISIBLE` staging buffer and a fenced copy.
- A same-topology update reuses existing capacity and recreates no buffer
  (Fixture B: revision 7, 8 vertices / 36 indices, `reuse`, grows stayed at 2,
  staging grows stayed at 1).
- A larger replacement grows capacity safely (Fixture C: 294 vertices /
  1296 indices, `grow`, vertex capacity 192 → 7056 B, index 144 → 5184 B,
  staging 336 → 12240 B).
- A later smaller revision does not shrink or reallocate anything.
- 60 revisions published in a bounded run gave 59 uploads and 1 coalesced
  revision, with **zero** buffer growths and **zero** staging growths, live GPU
  mesh buffers constant at 2, no failures and no crash. The paced run took
  1024 ms; that is a pacing artefact, not a performance measurement.
- The mesh update path calls neither `vkDeviceWaitIdle` nor `vkQueueWaitIdle`;
  it waits only on the renderer's own frame fences and its upload fence.
- Picking reads the current CPU mesh snapshot: a tap on Fixture B hit
  y = 0.4950, exactly its flattened top face, and a tap on Fixture C hit
  triangle 358 of the 432 that only that fixture has.
- The selected `ObjectId` stayed `1` across all three fixtures and the stress
  run, and the highlight stayed attached to it.
- The current mesh revision and its GPU buffers survive home/resume with no
  re-upload (revision 7 before and after; post-resume screenshot byte-identical).

Established in Stages 003-005 (see the attribution note above):

- Everything verified in Stage 003 (Vulkan init chain, swapchain, depth,
  pipeline, indexed cube, 2-frame-in-flight, surface recreation) still works.
- Camera-driven viewport: the cube is stationary in world space and all on-screen
  motion comes from the camera. (Stage 003's automatic Y-spin was removed.)
- One-finger drag orbits around the target; drag right decreases yaw, drag down
  increases pitch; sensitivity 0.005 rad/px; pitch clamped to ±1.52 rad; yaw wraps.
- Two-finger centroid translation pans the target in the camera plane, scaled by
  camera distance and viewport height (1 finger px = 1 world px at the target plane).
- Pinch changes camera distance multiplicatively, `exp(-Δspan × 0.0035)`, clamped
  to `[0.35, 400]`. Pan and pinch are interpreted from the same gesture.
- Pointer identity is tracked by pointer id; the active set is sorted by id, so
  MotionEvent index reordering is inert. Any change to the pointer set re-anchors
  with zero delta, so 1↔2 transitions never jump.
- `ACTION_CANCEL`, last-pointer-up, and Surface destroy/create all clear gesture
  tracking without disturbing the camera pose.
- Camera pose persists across home/resume and swapchain recreation for the life
  of the process (verified byte-identical screenshot).
- Back-face culling is on and the cube renders correctly from multiple camera
  poses; no face disappears.
- A short one-finger tap on the cube selects it (stable `ObjectId` 1) and tints
  it orange; a tap on empty background clears the selection and restores the
  exact unselected image.
- One-finger drag past ~24 px orbits and never picks or clears. Two-finger
  pan/pinch never picks or clears.
- Picking follows the camera: after a material orbit, tapping the cube at its
  new screen position selects the same `ObjectId`.
- Selected identity and highlight persist across home/resume and Surface
  recreation for the life of the process.
- 38 debug-only camera self-checks, 94 debug-only picking/selection self-checks,
  91 debug-only dynamic-mesh self-checks, 100 debug-only Construction-box
  self-checks, 94 debug-only Construction-transform self-checks, 75 debug-only
  Construction-primitive self-checks, 105 debug-only typed-boundary/sphere
  self-checks and 126 debug-only Freeze/Grab self-checks run once at startup; all
  pass. None runs per frame.

Not implemented: scale, transform gizmos, snapping, sculpt brushes other than
Grab, a brush framework or kernel, remesh, sculpt undo, symmetry, masking,
sculpt layers, stylus pressure, a second sculptable object,
the rest of Construction Mode, further primitives (cone, capsule,
plane), an editable tessellation, a
primitive framework, object creation UI, multiple scene objects,
hierarchy/outliner,
multi-select, lasso/box select, acting on the selection,
booleans, remesh, subdivision as a product feature, grid, modeling/CAD, UV,
save/load, undo, export, materials, lighting, scene graph, ECS, two-finger
twist, inertia, camera presets, orthographic camera, GPU picking, spatial
acceleration for picking.

The debug mesh fixtures are **not** product functionality: they are test
infrastructure behind a debug-only key hook that compiles to a no-op in release,
and `PRODUCT.md` deliberately does not document them.

## Build / Run / Test Commands

```
gradlew.bat :app:assembleDebug
adb -s emulator-5558 install -r app\build\outputs\apk\debug\app-debug.apk
adb -s emulator-5558 shell am start -n com.forgeshape.app/.ForgeShapeActivity
adb -s emulator-5558 logcat -s ForgeShape:V
adb -s emulator-5558 shell input tap 540 1150                    (tap select / clear)
adb -s emulator-5558 shell input swipe 300 1200 800 1300 600     (one-finger orbit)
adb -s emulator-5558 shell uinput - < <gesture>.json             (two-finger pan/pinch)
adb -s emulator-5558 exec-out screencap -p > artifacts\<name>.png
adb -s emulator-5558 shell input keyevent 8|9|10|11|12           (DEBUG mesh fixtures A/B/C/stress/diag)
adb -s emulator-5558 shell input keyevent 13|14|15|16            (DEBUG box states A/B/C/invalid)
adb -s emulator-5558 logcat -G 16M                               (see note below)
```

Driving the Construction panel (view 1080x2400 @ density 420):

```
adb -s emulator-5558 shell input tap 174|326|544 64              (shape Box | Cylinder | Sphere)
adb -s emulator-5558 shell input tap 78|238|383 316              (unit mm | cm | m)
adb -s emulator-5558 shell input tap 197|539|881 193             (focus Width | Height | Depth)
adb -s emulator-5558 shell input tap 287|800 193                 (cylinder: focus Diameter | Height)
adb -s emulator-5558 shell input tap 540 193                     (sphere: focus Diameter)
adb -s emulator-5558 shell input tap 197|539|881 530             (focus Pos X | Pos Y | Pos Z)
adb -s emulator-5558 shell input tap 197|539|881 720             (focus Rot X | Rot Y | Rot Z)
adb -s emulator-5558 shell input keyevent 123                    (move to end of field)
adb -s emulator-5558 shell input keyevent 67                     (backspace; repeat to clear)
adb -s emulator-5558 shell input text "333,3"                    ('.' and ',' accepted; '-' allowed)
adb -s emulator-5558 shell input tap 896 315                     (Apply Shape)
adb -s emulator-5558 shell input tap 862 842                     (Apply Transform)
adb -s emulator-5558 shell input tap 214 989                     (Freeze to Sculpt)
adb -s emulator-5558 shell input tap 584 989                     (Resume Sculpt; only once frozen)
adb -s emulator-5558 shell input tap 200 1400                    (viewport: return focus)
```

Driving the Sculpt panel:

```
adb -s emulator-5558 shell input tap 837 84                      (Back to Construction)
adb -s emulator-5558 shell input tap <x> 229                     (Radius slider, x 37..1043)
adb -s emulator-5558 shell input tap <x> 330                     (Strength slider)
adb -s emulator-5558 shell input swipe 798 1308 1000 1308 800    (a Grab stroke on the mesh)
```

Tapping a `SeekBar` sets it, so one `input tap` drives a slider; the value native
code actually kept is logged as `FORGESHAPE_SCULPT_BRUSH` and is the authority,
not the tap position.

The Construction panel gained a mode row in Stage 012, so its bottom edge moved
from y 985 to **y 1132** and the safe viewport-tap band while the keyboard is up
is now roughly **y 1170-1500**. The Sculpt panel gained a tool row in Stage 013,
so its bottom edge moved from about y 470 to about **y 610**, and its sliders
from y 229 / 330 to **y 372 / 473**.

`FORGESHAPE_SCULPT_STROKE_PENDING` (Stage 012's `SCULPT_GRAB_BEGIN`) appears only
when the ray hits the current Frozen Sculpt Mesh, so it doubles as the picking
probe for the sculpt representation: a pixel that logs `FORGESHAPE_PICK_MISS`
before a stroke and `STROKE_PENDING` after it is direct evidence that the
pickable surface followed the deformation. Sweeping a line of taps outward from
the object's centre measures the pickable radius per direction.

A field keeps input focus after typing and swallows the debug number keys. Tap
the viewport to hand focus back before sending `keyevent 12`, but stay in the
band **below the panel and above the soft keyboard** — roughly y 1020-1400. The
panel did not grow in Stage 011: only one primitive's field row is ever on
screen, so its bottom edge is still at about y 985. A tap inside the keyboard
area never reaches the SurfaceView and the diagnostics dump silently does not
run, which looks exactly like a dump that produced nothing. This was hit once
during Stage 011 (a tap at y 1900) and re-run in the safe band.

Enlarge the logcat ring buffer before capturing startup evidence. The seven
self-test suites emit ~600 lines in a few milliseconds and the default buffer
silently drops the tail, which looks exactly like a self-test that stopped
partway. Nothing is wrong with the app; the log just ages out.

Capture screenshots through a POSIX shell (`>` in PowerShell re-encodes the
stream and corrupts the PNG). Push files with an absolute Windows source path
from PowerShell; Git Bash rewrites `/data/local/tmp` into a Windows path.

Use `adb -s <serial>` for every command; more than one Android target may be
attached.

Toolchain pinned by the project: compileSdk 36, buildTools 36.1.0,
NDK 27.2.12479018, CMake 3.22.1, Gradle 8.14.3 (wrapper), AGP 8.13.2,
minSdk 26, targetSdk 36, ABI filter `x86_64` only.

Shaders: GLSL in `app/src/main/cpp/shaders/`, compiled ahead of time by the
NDK `glslc` in CMake using `-mfmt=c`; the emitted C initializer lists are
`#include`d into the renderer, so no SPIR-V asset is loaded at runtime.

## Stage 015A — UI/UX Audit + Architecture Decision Pack

Audit and design only. Deliverable:
`docs/ui/UX_ARCHITECTURE_DECISION_PACK.md`. Nothing in that document is
implemented, and `PRODUCT.md` was deliberately not updated — it describes only
runtime-verified behaviour, and a proposal is not behaviour.

### Measured audit facts (`emulator-5558`, `uiautomator` hierarchy dumps)

| | portrait 411×914 dp | landscape 914×411 dp | compact 360×640 dp |
| --- | --- | --- | --- |
| `SurfaceView` | `[0,0][1080,2400]` | `[0,0][2400,1080]` | `[0,0][720,1280]` |
| Construction panel | `[0,0][1080,1175]` | `[0,0][2400,1080]` | `[0,0][720,~880]` |
| viewport unoccluded | 51 % | **0 %** | ~31 % |
| status line | visible | **clipped off-screen** | visible |

- **Landscape is a hard failure, not a degradation.** The panel measures to the
  full window, the model is entirely occluded, and the status line — the only
  channel for validation messages and the stale-source warning — lays out below
  the window bottom with no `ScrollView` anywhere in the Android layer to reach
  it. Evidence:
  `artifacts/stage015a_audit_landscape_panel_covers_screen.png`.
- **No configuration handling exists.** `configChanges` absorbs
  `orientation|screenSize|screenLayout|density` and there is no
  `onConfigurationChanged`. Grep across the Android layer:
  `onConfigurationChanged` 0, `getResources().getConfiguration` 0,
  `ORIENTATION_LANDSCAPE` 0, `screenWidthDp` 0, `ScrollView` 0.
- **No window-inset handling exists.** `WindowInsets` 0,
  `setOnApplyWindowInsetsListener` 0. Survives only because the deprecated
  fullscreen theme hides the system bars; `targetSdk` 36 makes edge-to-edge the
  platform default, so this is latent, not hypothetical.
- **No stable identifiers.** `setId` appears twice, both `RadioGroup` internals,
  which is why every runtime verification since Stage 007 has been driven by
  screen coordinates and why `README.md` carries a pixel table. There are zero
  automated tests of the Android layer. `setContentDescription` *is* populated on
  7 control families and is a usable bridge.
- Android layer is 2223 lines of Java; `ConstructionPanelView` alone is **1041**.
- **The portrait IME case works** and is a deliberate strength to preserve:
  top-anchored panel + `adjustPan` keeps fields and model both visible and never
  resizes the window (which would rebuild the swapchain per keystroke session).
  Evidence: `artifacts/stage015a_audit_ime_portrait.png`.

### Mobbin Pro usage

Four searches across 3D/AR scene editors, canvas/drawing tools, photo editors and
destructive-action confirmations (~20 screens examined). Adopted *structurally*,
never visually: segmented one-parameter-at-a-time editing (Depop *Adjust*),
persistent mode row + transient contextual selection actions (Depop, IKEA room
planner), inspector as a bottom sheet over a live canvas (Photoroom), thin
mode-independent top strip for undo/redo (Photoroom, Genie), and
consequence-stating verb-labelled destructive confirmations (Alta, Posh).
Rejected: thumbnail carousels, full-height modal editors, floating draggable
palettes, drawer-as-primary-navigation. **Nomad Sculpt was not studied for
imitation and nothing is reproduced from it or from any Mobbin screen.**

### Recommendation

- **Shell:** Option **B — Rail + Contextual Inspector**, the only direction that
  solves landscape by spending *width* rather than rationing height, and the only
  one with real reserved homes for hierarchy, gizmos and multi-object. Strongest
  alternative is **Option A — Docked Inspector**, markedly cheaper and lower risk.
  **The recommendation flips to A if ForgeShape is phone-only** — which is why the
  device question is the load-bearing decision.
- **Toolkit:** **structured Views**, not Compose. Decisive evidence: the project
  has **zero runtime dependencies** — no `dependencies { }` block at all,
  `android.useAndroidX=false`, 0 Kotlin files. Compose would add Kotlin, AndroidX,
  a compiler plugin and 30+ artifacts, and would layer its pointer pipeline over
  the most carefully proven behaviour in the product (raw `MotionEvent` → JNI and
  the Sculpt gesture arbitration). Espresso's test artifacts are
  `androidTest`-only and ship nothing in the product APK.
- **Native arbitration is preserved unchanged** — `g_strokePending`, the 8 px
  arming threshold and pending-then-promote are not touched by any option.

### Owner decisions required before Stage 015B

| | Decision | Recommendation |
| --- | --- | --- |
| D1 | Shell direction: A / B / C | **B** (A is the strong cheaper alternative) |
| D2 | Primary device: phone / tablet / both | **load-bearing** — phone-only flips D1 to A |
| D3 | Views or Compose | **Views** |
| D4 | Export: a mode or a top-strip action | either; affects reserved homes |
| D5 | Confirmation before re-Freeze discards sculpt work | **yes** |
| D6 | Stylus pressure in 015B | **no** — read `getToolType` only |

D1, D2 and D3 block Stage 015B. D4–D6 can be answered with it.

## Tests / Verification (Gate P0, target `emulator-5558`)

Runtime evidence is from AVD `ForgeShape_Stage006` / `emulator-5558`, confirmed
by `adb emu avd name`, with ForgeShape confirmed as `topResumedActivity` before
every evidence-sensitive action. Every `adb` command named the serial
explicitly. The reserved (`Medium_Phone_API_36.1`) and contended
(`ForgeShape_Stage004`) AVDs were never addressed, started, stopped or
reconfigured. The crash buffer was empty for the whole gate.

### Toolchain, before and after

| | before | after |
| --- | --- | --- |
| NDK | 27.2.12479018 | **29.0.14206865** |
| AGP | 8.13.2 | unchanged |
| Gradle wrapper | 8.14.3 | unchanged |
| SDK CMake | 3.22.1 | unchanged |
| compile / target / min SDK | 36 / 36 / 26 | unchanged |
| build tools | 36.1.0 | unchanged |
| ABI filter | `x86_64` only | unchanged |
| C++ standard / STL | C++17 / `c++_static` | unchanged |
| native linker options | none declared | unchanged |
| packaged `.so` | one: `lib/x86_64/libforgeshape_native.so` | unchanged |

`compileDebugJavaWithJavac`, `buildCMakeDebug[x86_64]` and `packageDebug` all
succeeded on the first attempt under r29. **No migration fix was required** —
not a source change, not a CMake change, not a Gradle change, not a flag. The
whole migration is one line.

### Regression under r29

One clean launch of the r29 build ran all nine debug self-test suites, in order,
with **992 checks and zero failures**:

| suite | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 38 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 94 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 94 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 75 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 232 |

followed by `FORGESHAPE_MESH_UPLOAD_OK` and `FORGESHAPE_NATIVE_VIEWPORT_OK`.
Zero `*_SELFTEST_FAIL`, zero `*_VIEWPORT_FAIL`, zero `_FAIL:` tokens for the
whole gate; crash buffer empty.

### Integration smoke under r29

- **Construction Apply.** Cone 1.6 m × 2.4 m →
  `CONSTRUCTION_PUBLISHED:8:34:192 kind=cone` and exactly one
  `MESH_UPLOAD_OK:8:34:192` — the Stage 014 cone topology unchanged. Selecting
  Cone in the shape selector beforehand produced **zero** native log lines.
- **Transform-only edit publishes nothing.** Pos (1,0,0) m, Rot (0,0,30)° →
  one `CONSTRUCTION_TRANSFORM` line, **0** publishes and **0** uploads,
  revision held at 8 (`gatep0_cone_transformed_r29.png`).
- **Picking on the transformed cone is exact.** A tap returned
  `PICK_HIT:1:6 at=(1.6413,-0.2165,0.3484)`. Carried into local space by
  `Rz(-30)` about (1,0,0) that is y = −0.5082, where the exact cone radius
  (1.2 − y)/3 = 0.56938, against a measured radial distance of 0.56683 —
  ratio 0.99552, inside the facet band `cos(pi/32)` = 0.99518. A tap off the
  silhouette gave `PICK_MISS` and cleared the selection.
- **Freeze to Sculpt.** Sphere ⌀2 m → `PUBLISHED:8:482:2880 kind=sphere`, then
  `SCULPT_FROZEN:482:2880 sculptRev=1 objectId=1` — same `ObjectId`
  (`gatep0_sphere_frozen_before_stroke_r29.png`).
- **One real brush stroke.** A single-finger Grab drag injected through the
  on-device `uinput` tool: `STROKE_PENDING` → `STROKE_BEGIN:grab:16
  radiusLocal=0.4162` → 24 `STROKE_MOVE` lines with `sculptRev` advancing
  monotonically 2…25 → `STROKE_END sculptRev=25`. Every published revision was
  `482:2880` and every upload said `reuse` — topology fixed and no buffer
  reallocated, exactly as the fixed-topology contract requires. The Y component
  of every displacement was exactly `0.0000` for a horizontal drag, and the X/Z
  components grew monotonically: Grab is position-driven, as documented.
- **Construction ↔ Sculpt preservation.** Back to Construction republished
  `kind=sphere sphere dia=2.000000m` — the authoritative parameter is
  bit-identical after 25 sculpt revisions — while `SCULPT_STATE` still reported
  `frozen=1 sculptRev=25 strokes=1 stale=0`
  (`gatep0_construction_source_preserved_r29.png`). Resume Sculpt re-entered at
  `sculptRev=25` **without** re-freezing.
- **`normalRecomputes=1` for 24 moves**, so the dirty-flagged normal cache is
  still recomputing per batch rather than per event or per frame.
- **HOME / resume.** The process survived (same pid), the Vulkan viewport was
  rebuilt (`FORGESHAPE_NATIVE_VIEWPORT_OK` after resume), and the sculpted mesh
  came back in Sculpt mode. The before-HOME and after-resume screenshots are
  **byte-identical** (md5 `86e3c42e8bb4365e82b6bde4c24d91d9`), which is a
  stronger statement than "looks the same"
  (`gatep0_sculpt_after_resume_r29.png`).

### 16 KB page-size compatibility

| evidence | result |
| --- | --- |
| ForgeShape-owned source/config audit for `PAGE_SIZE`, `getpagesize`, `sysconf`, `mmap`, page-alignment assumptions and custom linker page-size options | **none found**. The only literal `4096` in the tree is a Vulkan buffer-capacity figure in `forgeshape_mesh_selftest.cpp`, which is a byte count and not a page size. No `-Wl,-z,max-page-size`, no `jniLibs` or `useLegacyPackaging` override. |
| packaged `.so` inventory | exactly **one**, ForgeShape-owned: `lib/x86_64/libforgeshape_native.so`. There is no third-party native library in the APK at all. |
| ELF LOAD alignment, r27 (baseline) | `0x1000` on all three LOAD segments — **4 KB, not 16 KB compatible** |
| ELF LOAD alignment, r29 | `0x4000` on all three LOAD segments — **16 KB compatible** |
| `zipalign -c -P 16 -v 4 app-debug.apk` | **Verification successful**; the `.so` is stored uncompressed at offset 32768 |
| normal (4 KB) runtime smoke | **PASS** — the whole integration smoke above ran on `emulator-5558`, which reports `getconf PAGE_SIZE` = 4096 |
| 16 KB runtime smoke | **UNVERIFIED** — see below |

The r27 → r29 alignment change is the substantive result of the gate: the
project genuinely was not 16 KB compatible at the ELF level before it, and is
now, with no source change.

**Why 16 KB runtime evidence is UNVERIFIED.** The only Android runtime available
to ForgeShape is `emulator-5558`, and it reports a 4096-byte page. The only
system image installed on this machine is
`system-images;android-36.1;google_apis_playstore;x86_64`, which is a 4 KB
image. Proving the 16 KB runtime needs
**`system-images;android-36.1;google_apis_playstore_ps16k;x86_64`** (available
in `sdkmanager`, not installed), plus an AVD created from it. The gate forbids
silently installing a system image, so this was not done and no PASS was
fabricated. Static, ELF and APK-package compatibility stand on their own
evidence above.

### Verification matrix

| ID | Result | Evidence |
| --- | --- | --- |
| GIT-01 | PASS | no `.git` anywhere in the workspace before the gate; initialized only after inventory and a secret scan |
| GIT-02 | PASS | 171 files staged: 66 source/doc/wrapper + 105 cited artifacts. Ignored: `build/`, `app/build/`, `app/.cxx/`, `.gradle/`, `.claude/`, `local.properties` |
| GIT-03 | PASS | `5d386f03e7b33de47ea717a4a1d629231b073b56` — `baseline: accepted Stage 014` |
| NDK-01 | PASS | before/after table above |
| NDK-02 | PASS | `ndkVersion '29.0.14206865'`; `app/.cxx` CMake cache resolves to that toolchain |
| NDK-03 | PASS | `:app:clean :app:assembleDebug` BUILD SUCCESSFUL, 41 tasks, zero migration fixes |
| NDK-04 | PASS | 9 suites, 992 checks, 0 failures, 0 FAIL tokens |
| NDK-05 | PASS | integration smoke above, on `emulator-5558` |
| P16-01 | PASS | source/config audit above |
| P16-02 | PASS | one `.so`, `0x4000` LOAD alignment on all segments |
| P16-03 | PASS | `zipalign -c -P 16 -v 4` → Verification successful |
| P16-04 | PASS | normal-runtime smoke, empty crash buffer |
| P16-05 | **UNVERIFIED** | no safe 16 KB runtime exists; exact image named above |
| GIT-04 | PASS | migration committed separately; working tree clean afterwards |
| WF-01 | PASS | audit performed; see Technical Debt |

## Tests / Verification (Stage 014, target `emulator-5558`)

All CC-* and REG-01/02 evidence below is from one installed/running app
(**pid 10547**) on AVD `ForgeShape_Stage006` / `emulator-5558`, confirmed by
`adb emu avd name`, with ForgeShape confirmed as `topResumedActivity` before
every evidence-sensitive action. No other target was attached, and the reserved
(`Medium_Phone_API_36.1`) and contended (`ForgeShape_Stage004`) AVDs were never
addressed, started, stopped or reconfigured. Every product edit was made through
the real UI with `input tap` / `input text` / `input swipe`. The crash buffer was
empty and the PID was unchanged across that whole run, home/resume included.

The emulator then shut down on its own between the evidence run and the final
build check. It was restarted detached with WMI `Win32_Process.Create` (the
documented method), re-confirmed as `ForgeShape_Stage006` / `emulator-5558`, and
REG-03 was re-run cold on the rebuilt APK under **pid 12952**: all eight suites
green, empty crash buffer, ForgeShape resumed. No CC-* evidence was gathered
after the restart, and none was re-attributed across it.

Topology, bounds, winding, degeneracy and transaction semantics are proven in the
deterministic native suite; the runtime cases below are integration smoke tests,
not a re-proof of the invariants by tapping.

| ID | Result | Evidence |
| --- | --- | --- |
| CC-01 | PASS | `PrimitiveSpec` payload is a 5-alternative variant; a cone spec returns non-null only from `cone()`, a capsule only from `capsule()`, and a cone (2.0, 3.0) versus a capsule (2.0, 3.0) — the same two numbers — stay separate. `kind()` still equals `payload().index()`. Applying a cone leaves box/cylinder/sphere/capsule payloads at their remembered values. JNI is still one method per primitive: `applyConstructionCone`, `applyConstructionCapsule` added, no generic `kind + a + b + c` reintroduced. |
| CC-02 | PASS | Applied/Unchanged/Rejected asserted for both. 13 rejection cases — zero, negative, NaN, Inf, 1e40 for each primitive, capsule `totalHeight < diameter` (exact and 1.9999999), and the unresolvable 1e-6 × 1e30 capsule — each preserving active kind, **every** remembered payload, `ObjectId`, transform (pos Y −1.25, rot Z 90) and the store revision/published count. A refused cone or capsule does not switch the kind away from a cylinder. |
| CC-03 | PASS | 34 : 192 deterministic across 0.05 m…40 m; all positions finite; indices in range and a multiple of 3; ring vertices used exactly 4×, apex and base centre exactly N× each, total = 192; exact bounds ±d/2 in X/Z and ±h/2 in Y on five shapes; canonical winding; no zero-area or repeated-vertex triangle; closed and consistently oriented with `V − E + F = 2` (the check that fails on an open base). |
| CC-04 | PASS | 514 : 3072 deterministic; finite; valid indices; pole fans N×, rings beside poles 5×, interior rings 6×; exact bounds ±d/2 in X/Z and ±totalHeight/2 in Y on six shapes including the equality case; canonical winding; no degenerate triangle; closed with `V − E + F = 2`; every vertex exactly one radius from its own hemisphere centre. Equality case (2.0, 2.0) is **482 : 2880**, no two vertex positions coincide anywhere in the mesh, and 2.0 × 2.0000001 has the full 514 : 3072 with no degenerate triangle. |
| CC-05 | PASS | Three selector taps (Cone, Capsule, Cylinder) produced **zero** ForgeShape log lines; status line confirmed the draft changed. |
| CC-06 | PASS | `PUBLISHED:8:34:192 kind=cone` + one `MESH_UPLOAD_OK` for cone 1.6 × 2.4. Coherent render (`stage014_cone.png`). Side hits at radial 0.4832 / 0.3995 vs exact 0.4856 / 0.3993; base hits at exactly `y = -1.2000` on two triangles; misses off the silhouette, far below, and past the base rim. |
| CC-07 | PASS | `PUBLISHED:9:514:3072 kind=capsule` + one upload for capsule 1.2 × 3.0. Coherent render (`stage014_capsule.png`). Two middle hits both at radial **0.597** 1.8 m apart in y; hemisphere hit 0.5959 from that end's own centre vs exact 0.6; misses beside the silhouette and above the total height. |
| CC-08 | PASS | Capsule re-apply → `PRIMITIVE_UNCHANGED … rev=9`, nothing published. Capsule totalH 0.5 < dia 1.2 → `relation_invalid`, prior state retained, message names both fields. Cone re-apply → `PRIMITIVE_UNCHANGED … rev=10`. Cone diameter 0 and height −2 → refused by the panel, field named, **0** native calls. |
| CC-09 | PASS | Cone and capsule transform edits each logged one `CONSTRUCTION_TRANSFORM` and **0** publish/upload lines. Transformed cone hit `(0.9050,−0.1098,0.1689)` = 0.2015 from the axis vs exact 0.2017; old location and screen centre both missed. |
| CC-10 | PASS | Box → Cylinder → Sphere → Cone → Capsule → Box: `objectId=1` throughout, transform update count unchanged, all five typed payloads remembered exactly, and returning to the box a second time reports `Unchanged`. Confirmed in the real UI: the cone's 1.6 / 2.4 reappeared in the fields after a capsule detour. |
| CC-11 | PASS | Capsule frozen at `514:3072 sculptRev=1 objectId=1`, sculpted to `sculptRev=42`, Construction changed to a cone → `SCULPT_SOURCE_STALE:ui sculptRev=42`. Resume Sculpt → `sculptRev=42 v=514 i=3072 stale=1 freezes=1`, viewport **0 of 1,836,000 pixels differing**, panel says the shape changed and the sculpt was kept. |
| CC-12 | PASS | HOME/resume with capsule 1.2 × 3.0 at pos (0.4, −0.3, 0) rot (0, 0, 45): **0 of 1,296,000** viewport pixels differing, no re-publish, no re-upload, same PID, panel re-read from native. |
| REG-01 | PASS | Real UI after all of the above: box `8:36`, cylinder `66:384`, sphere `482:2880`, one revision each, `objectId=1`. Native suites `CONSTRUCTION_BOX` (100), `CONSTRUCTION_PRIMITIVE` (75) and `CONSTRUCTION_SPHERE` (105) all green; the shared tessellation owner is asserted not to have moved any older count. |
| REG-02 | PASS | `SCULPT_BRUSH_KERNEL_SELFTEST_OK (232 checks)`. A real Grab stroke on the frozen capsule: `STROKE_BEGIN:grab:257 radiusLocal=1.5623`, 41 `STROKE_MOVE` lines, `sculptRev` 1 → 42, then Freeze/Back/Resume verified pixel-identical (CC-11). |
| REG-03 | PASS | `gradlew.bat :app:assembleDebug` BUILD SUCCESSFUL; install/launch clean; `FORGESHAPE_NATIVE_VIEWPORT_OK`; `CAMERA_SELFTEST_OK (38)`, `PICKING_SELFTEST_OK (94)`, `DYNAMIC_MESH_SELFTEST_OK (91)`, `CONSTRUCTION_TRANSFORM_SELFTEST_OK (94)` — **992 checks across 8 suites, 0 failures**, empty crash buffer. |

## Tests / Verification (Stage 013, target `emulator-5558`)

All evidence below is from one installed/running app (pid 6697) on AVD
`ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name`, with
ForgeShape confirmed as `topResumedActivity` before every evidence-sensitive
action. No other target was attached. Every edit was made through the real UI
with `input tap` / `input text` / `input swipe` and the on-device `uinput` tool;
the debug key hook was used only to dump diagnostics.

Pixel comparisons here are **exact and complete**, not sampled: raw
`screencap` framebuffers compared byte for byte over rows 620-2399 (the whole
viewport below the Sculpt panel), 7,689,600 bytes each.

| Check | Result |
| --- | --- |
| Build, install, launch | PASS — `BUILD SUCCESSFUL`, `FORGESHAPE_NATIVE_VIEWPORT_OK` |
| All eight self-test suites | PASS — camera 38, picking 94, dynamic mesh 91, Construction box 100, transform 94, primitive 75, sphere 105, brush kernel **232**; 829 checks, 0 failures |
| Freeze to Sculpt | PASS — `SCULPT_FROZEN:482:2880 sculptRev=1 objectId=1 from kind=sphere sphere dia=2.000000m` |
| Adjacency built at Freeze | PASS — `adjacency=2880` = 2E for E = 482 + 960 − 2 = 1440 |
| Real two-finger gesture, first finger on the mesh | PASS — `STROKE_PENDING` → `STROKE_ABANDONED:navigation`, `CAMERA_ZOOM_OK`, distance 8.2 → 1.2104 |
| … sculpt revision after it | PASS — `sculptRev=1`, `strokes=0`, `storeRev=9`, all unchanged |
| … no committed stroke | PASS — no `STROKE_BEGIN`, no `STROKE_MOVE` |
| Reproduced on a second two-finger gesture | PASS — same two lines, distance 1.21 → 4.9084 |
| Real Grab | PASS — `STROKE_BEGIN:grab:4 radiusLocal=0.2261`, 53 moves, `sculptRev` 1 → 54, visible spike |
| … Grab math | PASS — `local` magnitude 0.3874 vs 340 px × 0.0018842 × 0.6 = 0.3844, X = −Z at yaw 0.7 |
| Real Clay | PASS — visible protruding wedge breaking the silhouette (`stage013_clay.png`) |
| Real Smooth on the Grab spike | PASS — 102 moves, `sculptRev` 54 → 156, spike collapsed to a sliver |
| Real Inflate | PASS — visible outward expansion, distinct in shape from Clay (`stage013_inflate.png`) |
| Radius effect, Clay | PASS — 312 px → 51 vertices / 0.6031; 151 px → 11 / 0.2956 |
| Radius effect, Inflate | PASS — 312 px → 29 vertices / 0.6032; 151 px → 8 / 0.3005 |
| Strength effect, Clay | PASS — Σ\|local\| 0.01769 → 0.31279 (×17.7) for strength 0.060 → 1.000 |
| Strength effect, Inflate | PASS — Σ\|local\| 0.01825 → 0.31509 (×17.3), same brush, same 500 px swipe |
| Tool switching Grab → Clay → Smooth → Inflate → Grab | PASS — every stroke used the requested tool; no `SCULPT_MODE`, `freezes` stayed 5 |
| Construction Source after all four brushes | PASS — `kind=sphere sphere dia=2.000000m updates=1 rejects=0`, `pos=(0,0,0)m rot=(0,0,0)deg identity=1 updates=0` |
| Construction view before vs after the session | PASS — byte-identical 209,258-byte PNG |
| Sculpt → Construction → Sculpt | PASS — viewport byte-identical, 7,689,600 bytes, **0** differing |
| Picking follows the sculpt, sculpted direction | PASS — hit at 380/410/440/**470** px, miss at 500 px |
| Picking, opposite direction | PASS — hit at 380/410 px, **miss at 440** px (source silhouette ≈ 430 px) |
| HOME/resume with Inflate active and an edited mesh | PASS — viewport byte-identical, 0 differing; `mode=sculpt tool=inflate sculptRev=282 freezes=5` |
| … no crash | PASS — same pid 6697 throughout, crash buffer empty |
| Brush gestures never select | PASS — 0 `SELECTION_CHANGED` lines from any stroke |
| Topology across every stroke | PASS — every published revision `482:2880` |
| GPU buffers across 1,170 uploads | PASS — `reuse=1168 grows=4 sgrows=2 failed=0`, `vcap/icap/scap` unchanged |
| Normal recomputes | PASS — 514 across 282 sculpt revisions and 25 strokes |

## Tests / Verification (Stage 012, target `emulator-5558`)

All evidence below is from one installed/running app (pid 5403) on AVD
`ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name`. No
rebuild, reinstall or relaunch occurred between the states. Every edit was made
through the real UI with `input tap` / `input text` / `input swipe` and the
on-device `uinput` tool; the debug key hook was used only to dump diagnostics.

Pixel comparisons sample every second pixel in both axes with exact ARGB
equality.

| Check | Result |
| --- | --- |
| Runtime target `ForgeShape_Stage006` / `emulator-5558` | PASS (`adb emu avd name`) |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| Reserved `emulator-5554` used or modified | No (not attached, never addressed) |
| Contended `emulator-5556` used for evidence | No (not attached) |
| Root Git initialized | No — still uninitialized |
| **A.** `gradlew.bat :app:assembleDebug`, `install -r`, `am start` | PASS (pid 5403) |
| **A.** `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| **A.** `*_SELFTEST_FAIL` / `*_CASE_FAIL` / `VIEWPORT_FAIL` / `PUBLISH_FAIL` for the whole session | **0** |
| **A.** Crash buffer | Empty (0 bytes) |
| **A.** All seven previous suites | PASS (38/38, 94/94, 91/91, 100/100, 94/94, 75/75, 105/105) |
| **A.** `FORGESHAPE_SCULPT_GRAB_SELFTEST_OK` | PASS (126/126) |
| **A.** Construction shape and transform UI still work | PASS (used throughout B, D, F) |
| **B.** Non-default source before Freeze | PASS — `kind=sphere sphere dia=2.000000m`, `pos=(0.4,-0.3,0)m rot=(0,0,45)deg identity=0` |
| **B.** Freeze through the real UI | PASS — `FORGESHAPE_SCULPT_FROZEN:482:2880 sculptRev=1 objectId=1 from kind=sphere sphere dia=2.000000m` |
| **B.** Mode is Sculpt afterwards | PASS — `SCULPT_STATE:freeze mode=sculpt frozen=1 freezes=1 stale=0` |
| **B.** SculptMesh counts equal the source at Freeze | PASS — 482:2880, the sphere's exact topology |
| **B.** Same `ObjectId` | PASS — 1 on both sides |
| **B.** Construction parameters and transform unchanged by the Freeze | PASS — `updates=1` on both, values bit-identical |
| **B.** Before/after-Freeze geometry equivalent | PASS — **313,200 sampled pixels, 0 differing** |
| **C.** Target pixel (905,1308) before any stroke | `FORGESHAPE_PICK_MISS`, no `GRAB_BEGIN` |
| **C.** Real Android drag (798,1308)→(1000,1308), 800 ms | PASS — `SCULPT_GRAB_BEGIN:17 radiusLocal=0.4229`, 42 `GRAB_MOVE`, `GRAB_END` |
| **C.** `SculptRevision` increases | PASS — 1 → 43; store revision 9 → 51 |
| **C.** Rendered geometry visibly changes | PASS (`stage012_c_after_grab.png` — a spike pulled from the right of the sphere) |
| **C.** Picking follows the deformation | PASS — the same pixel (905,1308) now reports `SCULPT_GRAB_BEGIN:10`; a pixel past the spike (960,1308) still missed |
| **C.** `ObjectId` stable | PASS — 1 before, during and after |
| **C.** Construction parameters bit-identical after the stroke | PASS — `sphere dia=2.000000m updates=1`, `pos=(0.400000,-0.300000,0.000000)m rot=(0.000000,0.000000,45.000000)deg updates=1` |
| **C.** Topology unchanged | PASS — every published revision `482:2880`, every upload `reuse` |
| **C.** Displacement matches the brush math independently | PASS — logged 0.4277; 202 px × 0.0035242 × 0.6 = 0.4271, and X = −Y exactly as `Rz(-45°)` requires |
| **C.** Any pick or selection change during the stroke | **0** |
| **D.** Radius 64.3 px | 5 vertices, `radiusLocal=0.2267`, a small nub |
| **D.** Radius 120 px | 17 vertices, `radiusLocal=0.4229`, a narrow spike |
| **D.** Radius 409.9 px | 256 of 482 vertices, `radiusLocal=1.4447`, the whole side stretched (`stage012_d2_radius_large.png`) |
| **D.** Strength 0.050 vs 1.000, same radius and drag | \|local\| 0.0355 vs 0.7039 — ratio 19.8 against a requested 20.0; affected set 256 in both |
| **D.** Both controls driven through the real UI | PASS (`input tap` on the SeekBars; native echoed `radiusPx`/`strength`) |
| **E.** One finger from empty viewport in Sculpt Mode | PASS — `CAMERA_ORBIT_OK`, yaw 0.7 → −1.14, no stroke |
| **E.** Two fingers from empty viewport (uinput) | PASS — `CAMERA_PAN_OK`, `CAMERA_ZOOM_OK`, distance 8.2 → 4.07, target moved, no stroke |
| **E.** Two fingers whose FIRST finger lands on the mesh | Stroke begins and the second finger ends it with `sculptRev` unchanged (49); the gesture then zooms 4.07 → 2.02 normally |
| **E.** Brush gesture selects or clears | Never — a tap on the mesh logged `GRAB_BEGIN`/`GRAB_END` and no `PICK_*` or `SELECTION_CHANGED` |
| **F.** Back to Construction shows the original source | PASS — **313,200 sampled pixels, 0 differing** against the pre-Freeze view |
| **F.** Construction parameters after switching back | PASS — bit-identical, `updates=1` |
| **F.** Picking in Construction Mode at (905,1308) | `PICK_MISS` — the original surface does not reach where the sculpted one does |
| **F.** Resume Sculpt restores prior edits without re-freezing | PASS — `freezes=1` unchanged, `sculptRev=43` unchanged, **491,400 sampled pixels, 0 differing** against the post-stroke view |
| **G.** HOME then resume with an edited SculptMesh | PASS — no crash, crash buffer empty, same pid |
| **G.** Mode and sculpt edits persist | PASS — `mode=sculpt sculptRev=49`, viewport **518,400 sampled pixels, 0 differing** |
| **G.** No re-upload on resume | PASS — `uploads=239` before and after |
| **G.** No unintended reset from Construction | PASS — Construction parameters still `dia=2.000000m updates=1` |
| **G.** Picking follows the sculpted geometry after resume | PASS — `SCULPT_GRAB_BEGIN:470` |
| **H.** Stale-source policy | PASS — changing to 1.2 m logged `SCULPT_SOURCE_STALE:ui sculptRev=49`, left the sculpt mesh untouched, and Sculpt Mode showed the unchanged sculpt with an amber warning (`stage012_h_stale_source.png`) |

Performance, on the 482-vertex / 2880-index sculpt mesh through the existing
**synchronous** upload path:

| Measure | Value |
| --- | --- |
| `GRAB_MOVE` events in one 800 ms stroke | 42 |
| GPU uploads during that stroke | 34 (8 revisions coalesced by the render thread) |
| Upload kind | `reuse` every time — topology never changes |
| Buffer growths during any stroke | **0** (`grows=4`, `sgrows=2` for the whole session, all from earlier topology changes) |
| Live mesh buffers | 2, constant |
| Failed uploads | 0 |
| Observable frame stall | None — strokes tracked the finger smoothly and every screenshot was captured without a visible hitch |

The synchronous upload path was **not** redesigned and did not need to be at this
mesh size. See Technical Debt for where that stops being true.

**Not covered at runtime:** `ACTION_CANCEL` was not injected during a live stroke.
Cancel is covered by the self-tests (stroke cleared, captured set released, later
`update` a no-op, revision unmoved) and by the `surfaceDestroyed` path, which
cancels any live stroke and was exercised in **G**.

## Tests / Verification (Stage 011, target `emulator-5558`)

All evidence below is from one installed/running app (pid 15179) on AVD
`ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name`. No
rebuild, reinstall or relaunch occurred between the states. Every edit was made
through the real UI with `input tap` / `input text`, never through the debug key
driver.

| Check | Result |
| --- | --- |
| Runtime target `ForgeShape_Stage006` / `emulator-5558` | PASS (`adb emu avd name`) |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| Reserved `emulator-5554` used or modified | No (not attached, never addressed) |
| Contended `emulator-5556` used for evidence | No (not attached) |
| Root Git initialized | No — still uninitialized |
| **A.** `gradlew.bat :app:assembleDebug`, `install -r`, `am start` | PASS (pid 15179; crash buffer empty) |
| **A.** `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| **A.** Any `FAIL` token for the whole session | 0 |
| **A.** Camera / picking / dynamic-mesh / Construction-box / transform / primitive suites | PASS (38/38, 94/94, 91/91, 100/100, 94/94, 75/75) |
| **A.** `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | PASS (105/105) |
| **A.** Box UI, cylinder UI and transform UI still work | PASS (exercised in E and H) |
| **B.** Selector Box → Sphere without Apply Shape | PASS — `kind=box` unchanged, `currentRev=7 published=7 uploads=1` unchanged, transform identity, `objectId=1`; **0** native calls |
| **B.** Sphere draft showed the remembered default | PASS — Diameter 1 (`stage011_selector_draft.png`) |
| **C.** Sphere Apply, 150 cm | PASS — `CONSTRUCTION_PRIMITIVE:ui kind=sphere sphere dia=1.500000m updates=1` |
| **C.** Exactly one mesh revision | PASS — `CONSTRUCTION_PUBLISHED:8:482:2880 ... reason=ui`, rev 7 → 8, `published` 7 → 8 |
| **C.** Expected topology counts | PASS — 482 vertices / 2880 indices = 15·32+2 and 6N(S−1) at N = 32, S = 16 |
| **C.** Existing GPU update path used | PASS — `MESH_UPLOAD_OK:8:482:2880:grow vcap=11568 icap=11520 scap=23088` |
| **C.** `ObjectId` and transform preserved | PASS — `objectId=1`, `identity=1 updates=0` |
| **C.** Sphere renders coherently, culling/depth correct | PASS (`stage011_sphere.png` — round silhouette, no gaps at poles or seam) |
| **D.** Sphere front-surface hit | PASS — `PICK_HIT:1:297 at=(0.4378,0.3059,0.5198)`, 0.74530 from the centre, inside the facet band [0.7428, 0.75] |
| **D.** Materially different sphere region | PASS — `PICK_HIT:1:179 at=(-0.1782,0.5441,0.4801)`, 0.74719, a different triangle and the opposite sign of x |
| **D.** Miss outside the silhouette | PASS — `FORGESHAPE_PICK_MISS` |
| **E.** Transform on the sphere | PASS — `pos=(0.500000,-0.600000,0.000000)m rot=(30.000000,45.000000,90.000000)deg updates=1 rev=8` |
| **E.** Mesh revision / upload after a transform-only edit | Unchanged — `currentRev=8 published=8 uploads=2`, `vcap/icap/scap`, `grows=4 sgrows=2` all unmoved |
| **E.** Visible translation | PASS (`stage011_sphere_transformed.png`) |
| **E.** Rotation proven | PASS — from native state `rot=(30,45,90)deg identity=0`, not from the silhouette; a sphere shows no rotation and none is claimed |
| **E.** Transformed picking | PASS — `at=(0.8838,-0.2069,0.5032)`, 0.74501 from the moved centre (0.5,−0.6,0) |
| **F.** Sphere diameter update, 60 cm | PASS — `dia=0.600000m updates=2`, `PUBLISHED:9:482:2880`, `MESH_UPLOAD_OK:9:482:2880:reuse` — exactly one revision, capacity reused |
| **F.** Transform and `ObjectId` after the update | PASS — `pos=(0.5,-0.6,0) rot=(30,45,90) updates=1`, `objectId=1` |
| **F.** Picking follows the new radius | PASS — the tap that hit the 1.5 m sphere now `PICK_MISS`; a tap through the centre hit at 0.29773, inside [0.29712, 0.3] |
| **G.** Re-apply identical sphere | PASS — `PRIMITIVE_UNCHANGED:ui ... rev=9`, no revision, no upload |
| **G.** Blank / zero / negative / malformed (`1.2.3`) diameter | PASS — each refused with a named message (`stage011_invalid_blank.png`, `stage011_invalid_malformed.png`) |
| **G.** Any native change from the four refusals | None — `kind=sphere dia=0.6 updates=2 rejects=0`, `currentRev=9 published=9 uploads=3`, transform unchanged; **0** native calls |
| **H.** Sphere → Cylinder, 80 / 200 cm | PASS — `kind=cylinder dia=0.800000m h=2.000000m updates=3`, `PUBLISHED:10:66:384`, `MESH_UPLOAD_OK:10:66:384:reuse` |
| **H.** Cylinder → Box, 150 / 80 / 40 cm | PASS — `kind=box w=1.500000m h=0.800000m d=0.400000m updates=4`, `PUBLISHED:11:8:36`, `MESH_UPLOAD_OK:11:8:36:reuse` |
| **H.** Exactly one revision per kind change | PASS — 9 → 10 → 11, `published` tracking each |
| **H.** Transform preserved and `ObjectId` still 1 | PASS — `pos=(0.5,-0.6,0)m rot=(30,45,90)deg updates=1` across both, `objectId=1` |
| **H.** Remembered typed parameters correct | PASS — with the box active, the drafts read sphere 60 cm and cylinder 80 / 200 cm (`stage011_remembered_sphere.png`, `..._cylinder.png`) |
| **H.** Box picking / rendering correct again | PASS — `PICK_HIT:1:1 at=(0.6093,-0.5125,0.1498)` on the local +Z face, 0.2050 from the centre = half the 0.4 m depth (`stage011_back_to_box.png`) |
| **I.** Home → resume with a non-default transformed sphere (90 cm) | PASS (same pid 15179, `surfaceDestroyed acked=1`, `NATIVE_VIEWPORT_OK`, crash buffer empty) |
| **I.** Kind / parameters / transform after resume | PASS — `kind=sphere sphere dia=0.900000m updates=5`, `pos=(0.5,-0.6,0)m rot=(30,45,90)deg updates=1` |
| **I.** Mesh coherent, no re-upload | PASS — `currentRev=12 uploadedRev=12 uploads=6` before and after |
| **I.** UI refreshed from native truth | PASS — Sphere selected, 90 cm, position 50 / −60 / 0 cm, rotation 30 / 45 / 90, unit `cm` preserved (`stage011_after_resume.png`) |
| **I.** Picking after resume | PASS — 0.44772 from the centre, inside [0.44568, 0.45] |
| **J.** ScrollView added | No — the panel did not grow (bottom edge ≈ y 985), so none was needed |
| **J.** Drags across the selector row, field row and status row | No camera movement and no picks — 0 `CAMERA_STATE`, 0 `PICK`, 0 apply lines |
| **J.** Orbit outside the panel still works | PASS — `CAMERA_ORBIT_OK`, `CAMERA_STATE yaw=-1.7915 pitch=1.2475` |
| **J.** Selection / picking after orbit | PASS — `PICK_HIT:1:229`, 0.44695 from the centre, `SELECTION_STATE:1` (`stage011_after_orbit.png`) |
| Generic `kind + a + b + c` JNI shape method still present | No — removed; three per-kind methods replace it |
| New third-party runtime, UI, math or test dependency added | No |
| Mobbin or any other plugin used | No |

Every shape edit was a real `MotionEvent` / IME text path through
`ConstructionPanelView` → `NativeViewport` → `applyConstructionBox` /
`applyConstructionCylinder` / `applyConstructionSphere`. Publication is a CPU act
on the UI thread; every Vulkan buffer create, copy and destroy happened on the
render thread.

## Tests / Verification (Stage 010, target `emulator-5558`)

All evidence below is from one installed/running app (pid 12004) on AVD
`ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name`. No
rebuild, reinstall or relaunch occurred between the states. Every edit was made
through the real UI with `input tap` / `input text`, never through the debug key
driver.

| Check | Result |
| --- | --- |
| Runtime target `ForgeShape_Stage006` / `emulator-5558` | PASS (`adb emu avd name`) |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| Reserved `emulator-5554` used or modified | No (not attached, never addressed) |
| Contended `emulator-5556` used for evidence | No (not attached) |
| Root Git initialized | No — still uninitialized |
| **A.** `gradlew.bat :app:assembleDebug`, `install -r`, `am start` | PASS (pid 12004; crash buffer empty) |
| **A.** `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| **A.** Any `FAIL` token for the whole session | 0 |
| **A.** Camera / picking / dynamic-mesh / Construction-box / transform suites | PASS (38/38, 94/94, 91/91, 100/100, 94/94) |
| **A.** `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | PASS (75/75) |
| **A.** Box UI and transform UI still work | PASS (exercised throughout; see H) |
| **B.** Selector Box → Cylinder without Apply Shape | PASS — `kind=box` unchanged, `currentRev=7 published=7 uploads=1` unchanged, transform identity, `objectId=1`; **0** native apply calls |
| **B.** Cylinder draft showed remembered defaults | PASS — Diameter 1, Height 2 (`stage010_selector_draft.png`) |
| **C.** Cylinder Apply, 120 / 240 cm | PASS — `CONSTRUCTION_PRIMITIVE:ui kind=cylinder cylinder dia=1.200000m h=2.400000m updates=1` |
| **C.** Exactly one mesh revision | PASS — `CONSTRUCTION_PUBLISHED:8:66:384 ... reason=ui`, rev 7 → 8, `published` 7 → 8 |
| **C.** Expected topology counts | PASS — 66 vertices / 384 indices = 2N+2 / 12N at N = 32 |
| **C.** Existing GPU update path used | PASS — `MESH_UPLOAD_OK:8:66:384:grow vcap=1584 icap=1536 scap=3120` |
| **C.** `ObjectId` and transform preserved | PASS — `objectId=1`, `identity=1 updates=0` |
| **C.** Cylinder renders coherently, culling/depth correct | PASS (`stage010_cylinder.png`, `..._from_below.png` bottom cap, `..._transformed.png` end cap) |
| **D.** Cylinder side hit | PASS — `at=(0.3847,-0.0971,0.4567)`, 0.59713 from the axis = the 32-segment facet radius `0.6*cos(pi/32)` = 0.59711 |
| **D.** Cylinder cap hit | PASS — three taps all `y=-1.2000` on triangle 83, exactly −height/2, all inside the radius |
| **D.** Miss outside the silhouette | PASS — `FORGESHAPE_PICK_MISS` |
| **E.** Transform on the cylinder | PASS — `pos=(0.000000,-0.600000,0.000000)m rot=(0.000000,0.000000,90.000000)deg updates=1 rev=8` |
| **E.** Mesh revision / upload after a transform-only edit | Unchanged — `currentRev=8 published=8 uploads=2`, capacities unchanged |
| **E.** Visible transform | PASS (`stage010_cylinder_transformed.png`, lying on its side) |
| **E.** Transformed picking | PASS — hit 0.59709 from the moved axis, on the facet radius |
| **F.** Cylinder parameter update, 60 / 300 cm | PASS — `dia=0.600000m h=3.000000m updates=2`, `PUBLISHED:9:66:384`, `MESH_UPLOAD_OK:9:66:384:reuse` — exactly one revision, capacity reused |
| **F.** Transform and `ObjectId` after the update | PASS — `pos=(0,-0.6,0) rot=(0,0,90) updates=1`, `objectId=1` |
| **F.** Picking follows the new size | PASS — the tap that hit the 0.6 m tube now `PICK_MISS`; taps 100 px lower hit at 0.2998 / 0.29856 from the axis, inside `[0.3*cos(pi/32), 0.3]` |
| **G.** Re-apply identical cylinder | PASS — `PRIMITIVE_UNCHANGED:ui ... rev=9`, no revision, no upload |
| **G.** Zero diameter | PASS — `Diameter must be greater than 0 cm.` |
| **G.** Negative height | PASS — reported and refused |
| **G.** Malformed diameter (`1.2.3`) | PASS — `Diameter is not a number: "1.2.3".` |
| **G.** Any native change from the three refusals | None — `kind=cylinder dia=0.6 h=3.0 updates=2 rejects=0`, `currentRev=9 uploads=3`, transform unchanged; **0** native calls |
| **H.** Cylinder → Box with explicit 150 / 80 / 40 cm | PASS — `kind=box box w=1.500000m h=0.800000m d=0.400000m updates=3` |
| **H.** Exactly one revision | PASS — `PUBLISHED:10:8:36 reason=ui`, `MESH_UPLOAD_OK:10:8:36:reuse`, rev 9 → 10 |
| **H.** Transform preserved and `ObjectId` still 1 | PASS — `pos=(0,-0.6,0)m rot=(0,0,90)deg updates=1`, `objectId=1` |
| **H.** Box picking / rendering correct again | PASS — three taps at exactly `y=0.1500` on triangles 6/7 (−0.6 + 0.75, the rotated half-width) |
| **I.** Home → resume with a non-default transformed cylinder (0.9 × 1.8 m) | PASS (same pid 12004, `surfaceDestroyed acked=1`, `NATIVE_VIEWPORT_OK`, crash buffer empty) |
| **I.** Kind / parameters / transform after resume | PASS — `kind=cylinder dia=0.900000m h=1.800000m updates=4`, `pos=(0,-0.6,0)m rot=(0,0,90)deg updates=1` |
| **I.** Mesh coherent, no re-upload | PASS — `currentRev=11 uploadedRev=11 uploads=5` before and after |
| **I.** UI refreshed from native truth | PASS — Cylinder selected, 90 / 180 cm, position 0 / −125→−60 cm, rotation 0/0/90 (`stage010_after_resume.png`) |
| **I.** Picking after resume | PASS — hit 0.44978 from the axis, inside `[0.45*cos(pi/32), 0.45]` |
| **J.** Drags across the selector row, field row and status row | No camera movement and no picks — 0 new `CAMERA_STATE`, 0 new `PICK` lines |
| **J.** Orbit outside the panel still works | PASS — `CAMERA_STATE yaw=-2.0454 pitch=1.2716` |
| **J.** Selection / picking after orbit | PASS — `PICK_HIT:1:124`, 0.44884 from the axis, `SELECTION_STATE:1` |
| New third-party runtime, UI, math or test dependency added | No |
| Mobbin or any other plugin used | No |

Every shape edit was a real `MotionEvent` / IME text path through
`ConstructionPanelView` → `NativeViewport` → `applyConstructionPrimitive`.
Publication is a CPU act on the UI thread; every Vulkan buffer create, copy and
destroy happened on the render thread.

## Tests / Verification (Stage 009, target `emulator-5558`)

All evidence below is from one installed/running app (pid 8846) on AVD
`ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name`. No
rebuild, reinstall or relaunch occurred between the states. Every edit was made
through the real UI with `input tap` / `input text`, never through the debug key
driver.

| Check | Result |
| --- | --- |
| Runtime target `ForgeShape_Stage006` / `emulator-5558` | PASS (`adb emu avd name` → `ForgeShape_Stage006`) |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| Reserved `emulator-5554` used or modified | No (not attached, never addressed) |
| Contended `emulator-5556` used for evidence | No (not attached) |
| Root Git initialized | No — still uninitialized |
| **A.** `gradlew.bat :app:assembleDebug` | PASS (BUILD SUCCESSFUL) |
| **A.** `install -r`, `am start`, no crash | PASS (pid 8846; crash buffer empty) |
| **A.** `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| **A.** Any `FAIL` token for the whole session | 0 |
| **A.** `FORGESHAPE_CAMERA_SELFTEST_OK` | PASS (38/38) |
| **A.** `FORGESHAPE_PICKING_SELFTEST_OK` | PASS (94/94) |
| **A.** `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | PASS (91/91) |
| **A.** `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | PASS (100/100) |
| **A.** `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | PASS (94/94) |
| **B.** Startup placement in UI and native | PASS — fields 0/0/0 and 0/0/0; `pos=(0,0,0)m rot=(0,0,0)deg identity=1 updates=0` (`stage009_initial.png`) |
| **C.** Position converts m → cm → mm → m | PASS — 1.5 / −0.25 → 150 / −25 → 1500 / −250 → 1.5 / −0.25; section label tracks the unit |
| **C.** Rotation converted by a unit change | No — stays 0/0/0 in every unit, by design |
| **C.** Unit switching changed native dimensions or transform | No — `w=2 h=1 d=0.5`, `pos=(0,0,0) rot=(0,0,0) updates=0` before and after |
| **C.** Unit switching changed revision / upload count | No — `currentRev=7 published=7 uploads=1` before and after |
| **C.** Native apply calls made while switching units | 0 |
| **D.** Position Apply 0.75 / 0.25 / 0 m | PASS — `CONSTRUCTION_TRANSFORM:ui pos=(0.750000,0.250000,0.000000)m rot=(0,0,0)deg updates=1 rev=7` |
| **D.** Box visibly moved | PASS (`stage009_position.png`) |
| **D.** Picking hits the moved box | PASS — `PICK_HIT:1:8 at=(0.4726,0.7500,0.2286)`; y = 0.7500 is exactly 0.25 + half the 1.0 m height |
| **D.** `ObjectId` stable | PASS (`objectId` 1 throughout) |
| **E.** Rotation Apply 15 / 30 / 10° | PASS — `rot=(15.000000,30.000000,10.000000)deg updates=2 rev=7` |
| **E.** Box visibly rotated | PASS (`stage009_rotation.png`) |
| **E.** Picking hits the rotated box | PASS — `PICK_HIT:1:1 at=(0.2735,0.3953,0.2095)`, on no axis-aligned face plane of the unrotated box |
| **E.** Dimensions changed by a rotation | No — `w=2 h=1 d=0.5 updates=0` |
| **F.** Combined transform (pos 0/−1.25/0 m, rot 0/0/90°) | PASS — `pos=(0.000000,-1.250000,0.000000)m rot=(0.000000,0.000000,90.000000)deg updates=5` |
| **F.** Renderer and picker agree | PASS — three taps at y 1240/1270/1300 all hit exactly `y=-0.2500` on triangles 6/7, the local **+X** face that +90° about Z turns into the world top. An unrotated box there tops out at −0.75 |
| **F.** Rotated world X extent | PASS — every hit `|x| < 0.5`, the rotated half-extent from the 1 m height, not the 1.0 m from the 2 m width |
| **G.** Re-apply identical transform | PASS — `CONSTRUCTION_TRANSFORM_UNCHANGED:ui ... rev=7`, `updates` stayed 5 |
| **G.** Unchanged caused revision / upload churn | No — `currentRev=7 published=7 uploads=1` before and after |
| **10/11.** Transform-only edits vs the mesh | PASS — across five applied transforms: `currentRev` 7, `published` 7, `uploads` 1, `vcap=192 icap=144 scap=336 grows=2 sgrows=1`, all unmoved |
| **H.** Blank Pos X | PASS — `Pos X is empty — enter a number.` (`stage009_invalid_blank.png`) |
| **H.** Malformed Pos X (`1.2.3`) | PASS — `Pos X is not a number: "1.2.3".` |
| **H.** Malformed Rot Y (`-`), one of six | PASS — `Rot Y is not a number: "-".` (`stage009_invalid_rotation.png`) |
| **H.** Any partial or native change from the three refusals | None — `pos=(0,-1.25,0) rot=(0,0,90) updates=5 rejects=0`, `w=2 h=1 d=0.5`, `currentRev=7 uploads=1`, all identical before and after |
| **H.** Native calls made by the three refusals | 0 (refused in the UI) |
| **I.** Dimension Apply under a non-zero transform | PASS — `BOX_DIMENSIONS:ui w=1.5 h=3 d=0.4`, `PUBLISHED:8:8:36 reason=ui`, `MESH_UPLOAD_OK:8:8:36:reuse`, exactly one revision and one upload |
| **I.** Transform preserved across the resize | PASS — `pos=(0,-1.25,0)m rot=(0,0,90)deg updates=5`, unchanged |
| **I.** Resized box renders/picks at the same placement | PASS — top face now exactly `y=-0.5000` (−1.25 + 0.75, triangle 7) and the +Z face exactly `z=0.2000` (half the new 0.4 m depth) |
| **J.** Drag across the panel moved the camera | No — 0 new `CAMERA_STATE` lines across three panel drags (status, position and rotation rows) |
| **J.** Drag across the panel selected or cleared | No — 0 new `PICK` lines |
| **J.** Orbit on the viewport still works | PASS — `CAMERA_ORBIT_OK`, yaw → −2.0546, pitch → 1.2512 (`stage009_after_orbit.png`) |
| **J.** Picking after orbit | PASS — `PICK_HIT:1:6 at=(-0.2389,-0.5000,-0.1255)`, still the transformed top face |
| **K.** Home → resume with non-default size and transform | PASS (same pid 8846, `surfaceDestroyed acked=1`, `NATIVE_VIEWPORT_OK`, crash buffer empty) |
| **K.** Dimensions and transform after resume | PASS — `w=1.5 h=3 d=0.4 updates=1`, `pos=(0,-1.25,0)m rot=(0,0,90)deg updates=5`, unchanged |
| **K.** Mesh coherent, no re-upload | PASS — `currentRev=8 uploadedRev=8 uploads=2` before and after |
| **K.** UI refreshed from native truth | PASS — 150 / 300 / 40 cm, position 0 / −125 / 0 cm, rotation 0 / 0 / 90 (unconverted), unit `cm` preserved (`stage009_after_resume.png`) |
| **K.** Picking after resume | PASS — `PICK_HIT:1:6 at=(-0.2389,-0.5000,-0.1255)`, identical to the pre-home hit |
| Stage 008 Height display-text anomaly reproduced | No — not observed at any point in this session |
| New third-party runtime, UI, math or test dependency added | No |
| Mobbin or any other plugin used | No |

Every placement edit was a real `MotionEvent` / IME text path through
`ConstructionPanelView` → `NativeViewport` → `applyConstructionTransform`.
Placement is a CPU act on the UI thread; every Vulkan buffer create, copy and
destroy happened on the render thread.

## Tests / Verification (Stage 008, target `emulator-5558`)

All evidence below is from one installed/running app (pid 6321) on AVD
`ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name`. No
rebuild, reinstall or relaunch occurred between the states. Every edit was made
through the real UI with `input tap` / `input text`, never through the debug key
driver.

| Check | Result |
| --- | --- |
| Runtime target `ForgeShape_Stage006` / `emulator-5558` | PASS (`adb emu avd name` → `ForgeShape_Stage006`) |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| Reserved `emulator-5554` used or modified | No (not attached, never addressed) |
| Contended `emulator-5556` used for evidence | No (not attached) |
| Root Git initialized | No — still uninitialized |
| `gradlew.bat :app:assembleDebug` | PASS (BUILD SUCCESSFUL) |
| `adb -s emulator-5558 install -r` | PASS (Success) |
| `am start`, no Java/native crash | PASS (pid 6321; crash buffer empty) |
| `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| Any `FAIL` token in the ForgeShape log for the whole session | 0 |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | PASS (38/38) |
| `FORGESHAPE_PICKING_SELFTEST_OK` | PASS (94/94) |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | PASS (91/91) |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | PASS (100/100; was 83 — 17 new checks cover the apply entry point) |
| **B.** Fields at startup match native truth | PASS — panel reads `2 / 1 / 0.5` m against `w=2.000000m h=1.000000m d=0.500000m` (`stage008_initial_m.png`) |
| **C.** m → cm converts | PASS — `200 / 100 / 50` (`stage008_unit_cm.png`) |
| **C.** cm → mm converts | PASS — `2000 / 1000 / 500` (`stage008_unit_mm.png`) |
| **C.** mm → m returns the original digits | PASS — `2 / 1 / 0.5` (`stage008_unit_back_m.png`) |
| **C.** Unit switching changed native dimensions | No — `w=2.000000m h=1.000000m d=0.500000m updates=0` before and after |
| **C.** Unit switching changed the mesh revision | No — `currentRev=7 published=7` before and after |
| **C.** Unit switching changed the upload count | No — `uploads=1` before and after |
| **C.** Native apply calls made while switching units | 0 (no `..._DIMENSIONS:ui` / `_UNCHANGED:ui` / `_REJECTED:ui` line) |
| **D.** Valid Apply in cm (125 / 250 / 75) | PASS — `CONSTRUCTION_BOX_DIMENSIONS:ui w=1.250000m h=2.500000m d=0.750000m updates=1` |
| **D.** Exactly one new revision | PASS — `CONSTRUCTION_BOX_PUBLISHED:8:8:36 ... reason=ui`, rev 7 → 8, `published` 7 → 8 |
| **D.** Same-topology GPU reuse | PASS — `MESH_UPLOAD_OK:8:8:36:reuse vcap=192 icap=144 scap=336 grows=2 sgrows=1` (unchanged) |
| **D.** Box visibly changed | PASS (`stage008_applied_cm.png`, tall narrow box; status reads `Applied — 125 x 250 x 75 cm`) |
| **D.** Picking uses the updated geometry | PASS — `PICK_HIT:1:8 at=(0.0159,1.2500,0.0133)`; y = 1.2500 is exactly height/2 of the 2.5 m box |
| **D.** `ObjectId` stable | PASS — `objectId=1` in every line; `SELECTION_CHANGED:1` |
| **E.** Comma decimal through the real UI | PASS — `333,3` typed into Width (`stage008_decimal_typed.png`) applied as `w=3.333000m` |
| **E.** Non-trivial decimal state | PASS — `CONSTRUCTION_BOX_DIMENSIONS:ui w=3.333000m h=0.420000m d=1.125000m updates=2`, `PUBLISHED:9:8:36` |
| **E.** Decimal state uploaded, capacity reused | PASS — `MESH_UPLOAD_OK:9:8:36:reuse` with unchanged capacities |
| **E.** Picking on the decimal state | PASS — `PICK_HIT:1:8 at=(0.2036,0.2100,0.1237)`; y = 0.2100 is exactly height/2 of the 0.42 m box |
| **E.** Comma normalized in the field after Apply | PASS — `333.3` (`stage008_decimal_applied.png`) |
| **F.** Re-apply identical values (cm) | PASS — `CONSTRUCTION_BOX_UNCHANGED:ui w=1.250000m h=2.500000m d=0.750000m rev=8` |
| **F.** Same state re-entered in a different unit (m) | PASS — also `UNCHANGED ... rev=8`; the unit a value was typed in does not reach the domain |
| **F.** Unchanged published a revision or uploaded | No — `currentRev=8 published=8 uploads=2` before and after both attempts |
| **G.** Blank field | PASS — `Width is empty — enter a number.` (`stage008_invalid_blank.png`) |
| **G.** Negative value | PASS — `Width must be greater than 0 cm.` (`stage008_invalid_negative.png`) |
| **G.** Zero value | PASS — `Height must be greater than 0 cm.` (`stage008_invalid_zero.png`) |
| **G.** Malformed text (`1.2.3`) | PASS — `Width is not a number: "1.2.3".` (`stage008_invalid_malformed.png`) |
| **G.** Any partial or native change from the four refusals | None — `w=3.333000m h=0.420000m d=1.125000m updates=2 rejects=0` and `currentRev=9 published=9 uploads=3` bit-identical before and after |
| **G.** Native apply calls made by the four invalid attempts | 0 — all four were refused in the UI; no `..._REJECTED:ui` line exists |
| **G.** App healthy after the refusals | PASS (no crash, frames continued, later Apply still works) |
| **H.** Drag across the panel moves the camera | No — 0 new `CAMERA_STATE` lines across two panel drags (status row and field row) |
| **H.** Drag across the panel selects or clears | No — 0 new `PICK` lines |
| **H.** Orbit on the viewport still works | PASS — `CAMERA_ORBIT_OK`, yaw 0.7 → −2.0638, pitch 0.5 → 1.2537 |
| **H.** Orbit selected or cleared | No |
| **H.** Picking after orbit | PASS — `PICK_HIT:1:9 at=(-0.4111,0.2100,-0.2208)`, `SELECTION_STATE:1` |
| **H.** Viewport touch takes focus/keyboard back from a field | PASS (debug key hook reachable again after a viewport tap) |
| **I.** Home → resume with a non-default box and cm selected | PASS (same pid 6321, `surfaceDestroyed acked=1`, `NATIVE_VIEWPORT_OK`) |
| **I.** Native dimensions after resume | PASS — `w=3.333000m h=0.420000m d=1.125000m updates=2 rejects=0`, unchanged |
| **I.** Mesh revision preserved, no re-upload | PASS — `currentRev=9 uploadedRev=9 uploads=3` before and after |
| **I.** Fields refreshed to authoritative truth | PASS — a stale invalid `0` in Height became `42`; stale error message cleared (`stage008_before_home.png` → `stage008_after_resume.png`) |
| **I.** Selected display unit after resume | PASS — still `cm` (documented contract: preserved for the process lifetime) |
| **I.** Picking after resume | PASS — `PICK_HIT:1:9 at=(-0.0605,0.2100,-0.0816)`, still the 0.42 m box |
| New third-party runtime or UI dependency added | No |
| Mobbin or any other plugin used | No |

Every dimension edit was a real `MotionEvent` / IME text path through
`ConstructionPanelView` → `NativeViewport` → `applyBoxDimensionsMeters`. Publication
is a CPU act on the UI thread; every Vulkan buffer create, copy and destroy
happened on the render thread.

## Tests / Verification (Stage 007, target `emulator-5558`)

All evidence below is from one installed/running app (pid 8215). No rebuild,
reinstall or relaunch occurred between the dimension states.

| Check | Result |
| --- | --- |
| Runtime target `ForgeShape_Stage006` / `emulator-5558`, confirmed by `adb emu avd name` | PASS |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| `gradlew.bat :app:assembleDebug` | PASS (BUILD SUCCESSFUL) |
| `adb -s emulator-5558 install -r` | PASS (Success) |
| `am start`, no Java/native crash | PASS (pid 8215; crash buffer empty) |
| `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| Any `FAIL` token in the ForgeShape log | 0 |
| Fatal Vulkan / JNI / surface errors | 0 |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | PASS (38/38) |
| `FORGESHAPE_PICKING_SELFTEST_OK` | PASS (94/94) |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | PASS (91/91) |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | PASS (83/83) |
| Startup geometry generated by `ConstructionBox`, not Fixture A | PASS — `CONSTRUCTION_BOX_PUBLISHED:1:8:36 w=2.000000m h=1.000000m d=0.500000m objectId=1 reason=startup`, and again `:7:8:36 reason=startup_after_selftests`. No `MESH_REVISION_PUBLISHED ... fixture=A_baseline` on any startup path |
| Baseline screenshot shows a non-cubic 2 × 1 × 0.5 box | PASS (`stage007_box_state_a.png`, 3 coherent faces, wide/half-height/thin) |
| State B applied at runtime (1.25 × 2.5 × 0.75 m) | PASS — `CONSTRUCTION_BOX_DIMENSIONS:state_B w=1.250000m h=2.500000m d=0.750000m updates=1`, `CONSTRUCTION_BOX_PUBLISHED:8:8:36` |
| State B uploaded, counts unchanged, GPU capacity reused | PASS — `MESH_UPLOAD_OK:8:8:36:reuse vcap=192 icap=144 scap=336 grows=2 sgrows=1` |
| State B visibly changed shape | PASS (`stage007_box_state_b.png`, tall narrow box) |
| Identical dimensions re-applied → no-op | PASS — `CONSTRUCTION_BOX_UNCHANGED:state_B ... rev=8`; no new revision, `published` stayed 8 |
| State C applied at runtime (3.333 × 0.42 × 1.125 m) | PASS — `CONSTRUCTION_BOX_PUBLISHED:9:8:36 w=3.333000m h=0.420000m d=1.125000m` |
| State C uploaded, counts unchanged, GPU capacity reused | PASS — `MESH_UPLOAD_OK:9:8:36:reuse vcap=192 icap=144 scap=336 grows=2 sgrows=1` |
| State C visibly changed shape | PASS (`stage007_box_state_c.png`, long flat slab) |
| Mesh stayed 8 vertices / 36 indices across A→B→C | PASS (`v=8 i=36` in every diagnostic) |
| GPU vertex/index/staging capacity grew because of a dimension change | No — 192/144/336 and `grows=2 sgrows=1` unchanged across B and C |
| Stable `ObjectId` across all dimension states | PASS (`objectId=1` in every line) |
| Pick on state C uses the current generated extents | PASS — `PICK_HIT:1:9 at=(-0.0148,0.2100,-0.0176)`; y = 0.2100 is exactly height/2 for the 0.42 m box, triangle 9 is the +Y face |
| Selection after the pick | PASS — `SELECTION_CHANGED:1`, same `ObjectId` |
| Invalid dimension update rejected | PASS — `CONSTRUCTION_BOX_REJECTED:invalid_negative_width:not_positive requested=(-1.000000,1.000000,0.500000) retained w=3.333000m h=0.420000m d=1.125000m rev=9 rejects=1` |
| Invalid update left authoritative parameters unchanged | PASS (`BOX_STATE:on_request w=3.333000m h=0.420000m d=1.125000m updates=2 rejects=1`) |
| Invalid update left the mesh revision unchanged | PASS (`currentRev=9`, `published=9`, `uploads=3` before and after) |
| App healthy after the rejection | PASS (no crash, frames continued) |
| Home → resume with non-default state C active | PASS (same pid 8215, `surfaceDestroyed acked=1`, `NATIVE_VIEWPORT_OK`) |
| Construction parameters authoritative after resume | PASS (`w=3.333000m h=0.420000m d=1.125000m updates=2`) |
| Mesh revision preserved across home/resume, no re-upload | PASS (`currentRev=9 uploadedRev=9 uploads=3` before and after) |
| Picking/selection coherent after resume | PASS — post-resume `PICK_HIT:1:9 at=(-0.0148,0.2100,-0.0176)`, `SELECTION_STATE:1`, identical to the pre-home hit |
| Post-resume screenshot shows state C, still highlighted | PASS (`stage007_box_after_resume.png`) |
| Camera orbit regression after dimension changes | PASS — `CAMERA_ORBIT_OK`, yaw → −1.8083, pitch → 1.0017, no pick |
| Debug fixture path still functional (dynamic-mesh regression) | PASS — Fixture C `MESH_REVISION_PUBLISHED:10:294:1296`, `MESH_UPLOAD_OK:10:294:1296:grow` |
| Box republished over a larger fixture without capacity shrink | PASS — `CONSTRUCTION_BOX_PUBLISHED:11:8:36 reason=state_A`, `MESH_UPLOAD_OK:11:8:36:reuse vcap=7056 icap=5184 scap=12240` |
| New third-party runtime dependency added | No |
| Reserved `emulator-5554` used or modified | No (attached, never addressed) |
| Contended `emulator-5556` used for evidence | No (not attached) |

Dimension updates and publication are CPU acts on the UI thread; every Vulkan
buffer create, copy and destroy happened on the render thread. The pick was a
real `MotionEvent` via `input tap`; the orbit was a real `input swipe`.

## Tests / Verification (Stage 006, target `emulator-5558`)

| Check | Result |
| --- | --- |
| Isolated AVD `ForgeShape_Stage006` created from installed tooling, launched detached on port 5558 | PASS |
| ForgeShape confirmed `topResumedActivity` before evidence-sensitive actions | PASS |
| Foreign foreground change or foreign input observed | None |
| `gradlew.bat :app:assembleDebug` | PASS (BUILD SUCCESSFUL) |
| `adb -s emulator-5558 install -r` | PASS (Success) |
| `am start`, no Java/native crash | PASS (pid 7281, later 7619; crash buffer empty) |
| `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| `FORGESHAPE_NATIVE_VIEWPORT_FAIL` / `MESH_UPLOAD_FAIL` occurrences | 0 |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | PASS (38/38) |
| `FORGESHAPE_PICKING_SELFTEST_OK` | PASS (94/94) |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | PASS (91/91) |
| Fixture A renders correctly, culling and depth correct | PASS (`stage006_mesh_baseline.png`, 3 coherent faces) |
| Tap selects Fixture A | PASS — `FORGESHAPE_PICK_HIT:1:1 dist=6.7721 at=(0.7581,0.8284,0.9000)`, `SELECTION_CHANGED:1` |
| Fixture B published at runtime, no rebuild/reinstall/relaunch | PASS — `MESH_REVISION_PUBLISHED:7:8:36` |
| Fixture B uploaded, same counts, capacity reused | PASS — `MESH_UPLOAD_OK:7:8:36:reuse vcap=192 icap=144 scap=336 grows=2 sgrows=1` |
| Fixture B did not recreate GPU buffers | PASS (`grows` unchanged at 2, `sgrows` unchanged at 1) |
| Fixture B visibly changed geometry | PASS (`stage006_mesh_deformed.png`, wide flattened sheared slab) |
| Tap on updated Fixture B geometry | PASS — `PICK_HIT:1:8 at=(0.4782,0.4950,0.7819)`; y = 0.4950 is exactly the flattened top face |
| Fixture C published at runtime, no rebuild/reinstall | PASS — `MESH_REVISION_PUBLISHED:8:294:1296` |
| Fixture C forced a safe capacity growth | PASS — `MESH_UPLOAD_OK:8:294:1296:grow vcap=7056 icap=5184 scap=12240 grows=4 sgrows=2` |
| Fixture C visibly distinct, no mesh corruption | PASS (`stage006_mesh_larger.png`, spherified box) |
| Tap on Fixture C uses updated CPU triangles | PASS — `PICK_HIT:1:358`; triangle 358 exists only in the 432-triangle fixture |
| Stable `ObjectId` 1 across all three fixtures | PASS |
| Selection highlight coherent after each update | PASS (tint visible in both post-update screenshots) |
| 60-revision stress run | PASS — `FORGESHAPE_MESH_STRESS_OK:60:59:1 finalRev=68 uploadedRev=68 grows=0 sgrows=0 live=2 ms=1024` |
| Stress: capacity did not grow once sufficient | PASS (`grows=0`, `sgrows=0`; vcap/icap/scap unchanged at 7056/5184/12240) |
| Stress: smaller revisions did not shrink capacity | PASS (8-vertex revisions kept the Fixture C capacities) |
| Stress: live GPU mesh buffers bounded | PASS (`live=2` throughout) |
| Stress: crash / fatal Vulkan / JNI / surface error | None; `failed=0` |
| Final uploaded revision deterministic and current | PASS (`currentRev=68`, `uploadedRev=68`) |
| Mesh update path calls `vkDeviceWaitIdle` / `vkQueueWaitIdle` | No (only process teardown, surface detach, swapchain rebuild — all pre-existing) |
| Orbit regression after mesh updates | PASS — `CAMERA_ORBIT_OK`, yaw 0.7→−1.7521, pitch 0.5→0.9904, no pick |
| Pan + pinch regression after mesh updates | PASS — `CAMERA_PAN_OK`, `CAMERA_ZOOM_OK`, distance 8.2→2.1891, target→(−0.2677,−0.1925,−0.1681), no pick |
| Real tap after camera movement picks `ObjectId` 1 | PASS — `PICK_HIT:1:8 at=(−0.3697,0.5732,−0.0678)`, matching the live stress deformation |
| Home → resume with Fixture C active and selection on | PASS (same pid, `surfaceDestroyed acked=1`, `NATIVE_VIEWPORT_OK`) |
| Mesh revision preserved across home/resume, no re-upload | PASS (`currentRev=7`, `uploadedRev=7` before and after) |
| Post-resume screenshot byte-identical to pre-home | PASS (md5 `f563a27feeb77273d2d690f2bf70ea0b` both) |
| Selection and highlight coherent after resume | PASS — post-resume tap `PICK_HIT:1:296`, `SELECTION_STATE:1` |
| Reserved `emulator-5554` used or modified | No |
| Contended `emulator-5556` used for evidence | No |

Fixture publication is a CPU act on the UI thread; every Vulkan buffer create,
copy and destroy happened on the render thread. Taps were real `MotionEvent`s
via `input tap`; two-finger pan/pinch was delivered through a `uinput` virtual
touchscreen as real `MotionEvent`s, not by calling controllers directly.

## Tests / Verification (Stage 005, target `emulator-5556`)

| Check | Result |
| --- | --- |
| `gradlew.bat :app:assembleDebug` | PASS (BUILD SUCCESSFUL, native + shader steps ran) |
| `adb -s emulator-5556 install -r` | PASS (Success) |
| `am start`, no Java/native crash | PASS (pid 7973, later 8428; crash buffer empty) |
| `FORGESHAPE_NATIVE_VIEWPORT_OK` after present | PASS |
| `FORGESHAPE_NATIVE_VIEWPORT_FAIL` occurrences | 0 |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | PASS (38/38 checks) |
| `FORGESHAPE_PICKING_SELFTEST_OK` | PASS (94/94 checks) |
| Back-face culling enabled and pipeline reports it | PASS (`cull BACK, frontFace CLOCKWISE`) |
| Cube visually correct with culling, default pose | PASS (`stage005_selection_unselected.png`, 3 coherent faces) |
| Cube visually correct with culling, orbited pose | PASS (`stage005_selection_after_camera.png`, no face missing) |
| Real tap on cube selects (`input tap 540 1150`) | PASS — `FORGESHAPE_PICK_HIT:1:1 dist=6.7721 at=(0.7581,0.8284,0.9000)`, `FORGESHAPE_SELECTION_CHANGED:1` |
| Tap did not move the camera | PASS (yaw/pitch/distance/target unchanged at 0.7 / 0.5 / 8.2 / origin) |
| Selected highlight clearly visible | PASS (`stage005_selection_selected.png`, whole cube tinted orange) |
| Real tap on empty background clears | PASS — `FORGESHAPE_PICK_MISS`, `FORGESHAPE_SELECTION_CHANGED:0` |
| Cleared image returns to unselected state | PASS (`stage005_selection_cleared.png` byte-identical to the unselected baseline) |
| Real one-finger orbit does not select or clear | PASS — yaw 0.7→−1.8177, pitch 0.5→1.2553; no `PICK` line, no `SELECTION_CHANGED`, `FORGESHAPE_SELECTION_STATE:1` |
| Real two-finger pan + pinch does not select or clear | PASS — `FORGESHAPE_CAMERA_PAN_OK`, `FORGESHAPE_CAMERA_ZOOM_OK`, distance 8.2→1.5283, target→(1.6002,0.5386,0.4034); no `PICK` line, `FORGESHAPE_SELECTION_STATE:1` |
| Pick after material camera change selects same id | PASS — after orbit to yaw −1.8177 / pitch 1.2553, tap at (400,1150) gave `FORGESHAPE_PICK_HIT:1:9 at=(0.0149,0.9000,−0.5041)` (+Y face), same `ObjectId` 1 |
| Home → resume, no crash, frame presents again | PASS (same pid 8428, `surfaceDestroyed acked=1`, `FORGESHAPE_NATIVE_VIEWPORT_OK`) |
| Selection + highlight preserved across home/resume | PASS (post-resume screenshot byte-identical to the pre-home selected baseline; no foreign input in the window) |
| Selected id still `1` after resume | PASS (post-resume orbit reported `FORGESHAPE_SELECTION_STATE:1`, no pick) |
| Reserved `emulator-5554` used or modified | No (not attached; never addressed) |

Multi-touch was delivered as real `MotionEvent`s through
`ForgeShapeSurfaceView.onTouchEvent` → JNI → `CameraController` /
`SelectionController`, using a `uinput` virtual touchscreen — not by calling
controller methods directly. Taps were real `MotionEvent`s via `input tap`.

Two earlier lifecycle attempts were discarded because a tap injected by the other
program on this emulator cleared the selection mid-test; the accepted run was
audited for foreign input and had none.

## Known Issues / Blockers

- **16 KB page-size runtime behaviour is UNVERIFIED.** Static, ELF and APK
  evidence all pass, but no 16 KB Android runtime is available to ForgeShape.
  Closing this needs
  `system-images;android-36.1;google_apis_playstore_ps16k;x86_64` and an AVD
  created from it. Not a defect — an unmeasured dimension.
- **Test-harness note, not a product defect:** `adb shell input swipe` and
  `adb shell input motionevent` do reach the viewport, but a Grab stroke driven
  by them on a **sparse** frozen mesh logs `STROKE_PENDING` →
  `STROKE_ABANDONED:navigation` and never promotes. The cause is the documented
  rule that a brush capturing **no vertex** starts no stroke: a frozen box has 8
  vertices, all at its corners, so a 120 px brush at the centre of a face
  captures none. Verified identically under **both** r27 and r29, so it is not
  an r29 regression, and it is not the arbitration misfiring. Strokes for
  evidence should be driven on a dense primitive (sphere 482 v, capsule 514 v)
  or with a larger radius. The README's Stage 013 recipe
  (`input swipe 540 1194 880 1194 900`) reproduces the abandon on the default
  box and should be read with this in mind.
- `AVD Medium_Phone_API_36.1` / `emulator-5554` is reserved by another program
  and unavailable to ForgeShape until the owner removes the constraint.
- `emulator-5556` is shared in practice: another program runs
  `com.damian.wlochyikafalonia.claude.debug` on it, steals the foreground and
  injects taps that reach ForgeShape. This does not affect the product, but it
  makes runtime verification flaky and can silently invalidate a test.
- An `adb shell input tap` aimed at the viewport while the soft keyboard is up
  must be above the keyboard (y ≈ 1000 on this device, not y ≈ 1900) or it never
  reaches the SurfaceView, focus stays in the text field, and a following
  `keyevent 12` types a digit instead of dumping diagnostics — which looks
  exactly like a diagnostics dump that did not run. Test-harness issue only.
- During one Stage 008 run the Height field's displayed text changed from `0` to
  `65` at an unidentified point between two screenshots. It was **display text
  only** — the authoritative dimensions, revision, upload count and reject count
  were verified bit-identical at the checkpoint immediately after — and the
  resume refresh corrected it. It could not be reproduced: replaying the panel
  drags, the viewport orbit, and the focused-field-plus-`keyevent` sequence each
  left the field text untouched. Most likely scripted-input interference from the
  test harness rather than app behaviour, but recorded as unexplained.
- **RESOLVED in Stage 013.** A two-finger gesture whose first finger landed on the
  Frozen Sculpt Mesh used to begin a Grab stroke that the second finger then
  ended. The pending-then-promote arbitration means no stroke is created at all
  for such a gesture; confirmed at runtime twice
  (`STROKE_PENDING` → `STROKE_ABANDONED:navigation`, `sculptRev` and `strokes`
  unchanged).
- The arbitration's remaining, deliberate seam: a first finger that **drags more
  than 8 px** before the second one lands does commit a stroke. That is a gesture
  the user drove as a stroke rather than as a two-finger navigation, and closing
  it completely would need a buffered or undoable stroke, which does not exist
  yet. Not observed in any of the injected two-finger runs.
- The Construction panel grew a mode row, so its bottom edge moved from y 985 to
  **y 1132**. Any Stage ≤ 011 script that taps the viewport at y ≈ 1020-1130 now
  hits the panel instead. Test-harness issue only.
- The Sculpt panel grew a tool row in Stage 013, so its bottom edge moved from
  y ≈ 470 to **y ≈ 610**, and the Radius and Strength sliders moved from y 229 /
  330 to **y 372 / 473**. Any Stage 012 script driving those sliders needs the new
  coordinates. Test-harness issue only.
- The path-driven tools deposit in proportion to pointer travel measured in brush
  radii, so a **short stroke with a large brush** does very little: three 70 px
  strokes at a 312 px radius produced a deformation of about 0.09 on a 1.0 m
  radius sphere, which is real but not visible in a silhouette. Longer strokes or
  a tighter brush are what the tool is for; this is documented behaviour, not a
  defect, but it is the one thing that reads as "the brush did nothing" on a first
  try.
- The default logcat ring buffer silently drops part of the startup self-test
  output (eight suites, ~720 lines in a few milliseconds). It looks exactly like
  a self-test that stopped partway. Run `adb -s <serial> logcat -G 16M` before
  capturing startup evidence.
- The Android emulator dies if launched as a child of a tool shell; it must be
  spawned detached (e.g. WMI `Win32_Process.Create`) to survive.
- One redundant swapchain rebuild occurs at startup and again on each resume,
  because `surfaceChanged` arrives immediately after `surfaceCreated`. Cosmetic.
- Android input resampling can emit a one-finger MOVE between `ACTION_DOWN` and
  `ACTION_POINTER_DOWN`, so starting a two-finger gesture may orbit by a few
  pixels first. This is correct behaviour for the contract, not a defect.
- On `emulator-5558`, `screencap` output acquired a uniform whole-framebuffer
  black-level lift partway through the Stage 006 session (background
  (14,18,27) → (43,47,55), with a matching lift on the geometry). It is a
  system/compositing effect, not a ForgeShape rendering change: the swapchain
  format stayed 37 (`R8G8B8A8_UNORM`) in every session, the rendered image is
  structurally identical, and home/resume within one display state is
  byte-identical. It only affects how screenshots from different points in a
  session compare; the Stage 006 evidence images were all captured in one state.

## Technical Debt

New in Stage 014:

- **The capsule's cylindrical middle is a single band, however long it is.**
  Between its two seam rings there are no interior rings at all — exactly as the
  cylinder's side wall has had none since Stage 010. The shape, its bounds, its
  winding and its picking are all exact regardless, but a small sculpt brush
  placed on a long capsule's middle has very few vertices to capture: measured at
  runtime, a 120 px brush there captured **8** vertices at `radiusLocal=0.4569`
  on a 514-vertex mesh, and an earlier attempt captured none at all. This is a
  tessellation-fidelity limitation, not a correctness one. Closing it means
  subdividing the middle by length, which changes the capsule's vertex count into
  a function of its proportions and belongs to its own stage.
- **`FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation` names two different causes.**
  It is logged both when the gesture really did become navigation and when a
  promoted stroke could not start because no vertex fell inside the brush. Those
  are different events and the log cannot tell them apart, which cost real time
  during this stage's evidence run. Pre-existing since Stage 013; the fix is a
  second token, not a behaviour change.
- **Capsule topology is a function of the parameters, not a constant.** The
  equality case publishes 482 : 2880 and everything else 514 : 3072. This is
  deliberate and is what keeps the degenerate case free of zero-area triangles,
  but it does mean a capsule edit can change the vertex count and so trigger a
  buffer growth where the other primitives never do. The existing capacity policy
  handles it (observed: `grow` then `reuse`), and nothing measured worse.
- **The float-resolvability check is coupled to the tessellation.**
  `validateCapsuleMeters` computes the smallest latitude step from
  `kCapsuleHemisphereBands` to decide representability. That is honest — it is
  exactly the question "can this shape be built at this fidelity" — but it means
  changing the shared stack count silently changes which capsules are accepted.
  No other primitive's validation depends on its tessellation.

New in Stage 012:

- **The sculpt upload path is synchronous and republishes the whole mesh per
  move.** A stroke on 482 vertices costs 34 uploads in 800 ms with zero buffer
  growth and no observable stall, so it was not redesigned — the stage said not
  to pre-emptively. But it is O(vertices) per move regardless of how few vertices
  the brush touched, and at a mesh two or three orders of magnitude larger it
  will stop being free. The honest measurement above is the baseline to compare a
  future partial-update or async path against; the decision is deferred, not
  made.
- **RESOLVED in Stage 013.** `GrabStroke` was one brush, not a kernel. It is now
  `SculptStroke`, which owns the hit, the affected set, the falloff, the radius
  resolution and the lifecycle for all four tools; each tool contributes only a
  deformation rule. Grab migrated onto it with its Stage 012 behaviour intact.
- The path-driven gain constants (`kNormalBrushGain = 0.35`, `kSmoothGain = 1.0`,
  `kMaxSmoothLambda = 0.9`) are chosen, not derived. They are honest defaults that
  behave well over the range that was exercised, but no stage has tuned them
  against a real modelling session, and the low gain is why a short stroke with a
  large brush reads as doing nothing. If brush feel is ever tuned, these are the
  three numbers, and they should move together with a runtime comparison rather
  than one at a time.
- The per-move `SculptStroke::update` still walks the whole affected set and the
  whole mesh's normals for Inflate. At 482 vertices and 960 triangles that is
  free (measured: 1,170 uploads, no stall), but the normal recompute is
  O(triangles) per move regardless of how few vertices the brush touched — the
  same shape of cost as the O(vertices) publication below it, and it should be
  addressed by the same future stage rather than separately.
- Freeze regenerates the Construction mesh to copy it, rather than copying the
  store's current revision. That is correct (the store may be holding a debug
  fixture) and cheap, but it is a second generation of geometry that already
  exists.
- The affected set is found by a **linear scan** over every vertex at stroke
  start, like picking. Fine at 482 vertices; it is the same missing spatial
  acceleration that picking already lacks, and both will want the same answer.
- Sculpt state is process-scoped only. There is no save, no load and no undo, so
  a stroke is unrecoverable the moment it lands — and Freeze silently discards
  the previous sculpt. The panel says so, but the honest fix is undo, which is a
  stage of its own.
- `SculptPanelView` duplicates `ConstructionPanelView`'s styling helpers (`dp`,
  the background drawables, the button factory). Two panels is not yet a reason
  for a shared base; a third would be.
- The Sculpt panel's status line is written on refresh and does not update during
  a stroke, so it deliberately reports no revision. A live readout would need a
  native→Java notification that does not exist.

New in Stage 011:

- Adding a primitive now costs **four** parallel edits: a member on
  `ConstructionObject`, a `PrimitiveKind` case, a variant alternative, and a JNI
  method plus its Java declaration. That is deliberate — the cost is visible
  rather than hidden behind a registry — but at a fourth primitive it should be a
  decision, not a habit.
- `ConstructionObject::setPrimitive` still dispatches with an if-else chain over
  the typed accessors rather than a `std::visit`, because the object's per-kind
  members are not uniform. It is correct and self-tested, but it is the one place
  a new primitive can be forgotten without a compile error.
- Sphere tessellation is a compile-time constant, like the cylinder's. A very
  large sphere will show its 32×16 facets, and nothing adapts.
- The sphere's parameter row is a single full-width field, which looks unlike the
  two- and three-column rows above it. Cosmetic; not addressed to avoid
  redesigning the panel.
- The panel is still a fixed `WRAP_CONTENT` block with no scroll and no collapse,
  verified at 1080x2400 portrait only. It did not grow in Stage 011 (only one
  primitive row is ever visible), so no `ScrollView` was added — but that is
  luck, not design, and the next section added will need one.

Carried over from Stage 010 (both resolved in Stage 011):

- ~~`PrimitiveSpec` carries both parameter groups and the caller must respect
  `kind`~~ — resolved: the payload is a `std::variant` and `kind()` is derived
  from it.
- ~~The JNI shape apply passes three positional doubles whose meaning depends on
  `kind`~~ — resolved: removed, replaced by one method per primitive.

New in Stage 010:

- `ConstructionObject` holds one member per primitive and dispatches on
  `PrimitiveKind` with a `switch`. A further primitive means another member
  and another case. That is deliberate — no base class, no vtable, no registry —
  but it is the point at which to reconsider, not to keep extending silently.
- Every primitive's parameters are always resident even though only one is
  active. Trivial at three primitives and six doubles; it is a real decision to
  revisit if the primitive set grows.
- Cylinder tessellation is a compile-time constant. Correct for the product
  today, but a very large cylinder will show its 32 facets, and nothing adapts.
- The two `MeshStore`-ObjectId names (`kConstructionBoxObjectId` /
  `kDemoCubeObjectId`) are now doubly misnamed: the object is not always a box
  and never was a demo cube. Renaming was left out of this stage to avoid
  churning unrelated code.

New in Stage 009:

- The panel is now ~830 px tall on a 2400 px screen and is still a fixed
  `WRAP_CONTENT` block with no scroll and no collapse. It was verified at
  1080x2400 portrait only; a shorter viewport or landscape would crowd it. A
  third section will need a `ScrollView` or collapsible sections.
- The panel's own top edge can hide part of the object. During Stage 009 a box
  lifted to y = +0.5 m had its top face behind the panel and could not be tapped;
  the test was re-run with the box lowered. There is no focus-on-selection, no
  camera framing helper and no viewport inset.
- `ConstructionTransform` and `ConstructionBox` are two singletons with no owning
  "Construction object" that holds both. Fine for one box; a second object needs
  that container before it needs a registry.
- `modelMatrix()` / `inverseModelMatrix()` recompute six trig calls per frame and
  per pick. Correct and negligible at this scale, but it is recomputation of a
  value that only changes on Apply.
- The transform is rigid by design (no scale). The exact composed inverse and the
  "local distance is world distance" shortcut in picking both depend on that;
  adding scale invalidates them and is a domain change, not a matrix change.
- A large translation makes the float round-trip residual grow linearly with
  `|p|` (about `|p| * 2^-23`). The self-test bound scales with position magnitude
  for exactly this reason. At kilometre scale the derived `float` matrix, not the
  `double` domain, is the precision limit.

New in Stage 008:

- `ConstructionPanelView` builds its view tree in code and hard-codes its colours
  and spacing, because the project ships no resource layouts or theme. Fine for
  one panel; a second panel should not copy it.
- The panel is anchored to the top at a fixed `WRAP_CONTENT` height with no
  collapse, no scroll and no landscape-specific layout. It was verified at
  1080x2400 / density 420 only.
- Focus handling is minimal: a field keeps focus after a rejected Apply (correct
  for editing) and therefore keeps swallowing hardware/`adb` number keys until
  the viewport is touched. Real users are unaffected; it is a test-harness sharp
  edge, and it is documented in the commands section.
- The panel refreshes from native truth on resume, discarding any half-typed
  edit. Deliberate for this stage — there is no edit-in-progress model to
  preserve — but a future undo/edit-session feature will need one.
- `LengthUnit.format` strips trailing zeros, so 2.0 m displays as `2`. Exact and
  unambiguous, but a future engineering-notation or significant-figures policy
  will replace it.

Resolved in Stage 008: the Stage 007 debt item "publication is still driven from
`forgeshape_jni.cpp`". Update-then-publish now lives in
`applyBoxDimensionsMeters` inside the Construction layer, and both the UI path
and the DEBUG driver call it.

New in Stage 007:

- `ConstructionBox` is a single process-scoped object with no registry, so a
  second Construction object cannot exist yet. Deliberate for this stage, and the
  same limit `MeshStore` already has.
- A dimension change regenerates and republishes all 8 vertices. Correct and
  trivially cheap at this size; a future parameter-driven mesh with many vertices
  will want incremental regeneration.
- `kConstructionBoxObjectId` and `kDemoCubeObjectId` are the same value under two
  names. Keeping both avoided churning the existing selection/mesh code in this
  stage; the demo-cube name should retire when the bootstrap data does.
- ~~Publication is still driven from `forgeshape_jni.cpp`.~~ Resolved in Stage
  008: see `applyBoxDimensionsMeters` above.

Resolved in Stage 006: the HOST_VISIBLE geometry buffers from Stage 003 are gone.
Vertex and index buffers are now DEVICE_LOCAL with a bounded, reused
HOST_VISIBLE staging path, tracked capacities and fenced in-flight safety.
Runtime evidence also moved off the contended `emulator-5556` onto an isolated
ForgeShape-owned target.

Resolved in Stage 005: back-face culling is enabled with a documented canonical
winding convention shared by the rasterizer and CPU picking.

Carried from Stage 003, untouched: static viewport and scissor; no
`oldSwapchain` handling; no validation layers; single global viewport.

Carried from Stage 005, untouched: picking is a linear scan with no spatial
acceleration (fine for these fixtures, will not scale); the selection highlight
is a whole-object tint rather than an outline; there is still no checked-in
`uinput` harness file, so the multi-touch gesture is regenerated per stage.

New in Stage 006:

- A mesh upload waits on all frame fences and then on its own upload fence, so
  it is synchronous with respect to the render thread. Correct and bounded, but
  a future high-frequency sculpt path will want an asynchronous transfer.
- The `MeshStore` publishes for exactly one object; there is no multi-mesh
  registry.
- `MeshStore::publish` validates the data twice (once before taking the lock,
  once inside `createRuntimeMesh`). Negligible at these sizes, wasteful at
  sculpt sizes.
- Retired buffers are freed inline after the fence wait rather than through a
  deferred-destruction queue, which is what makes the synchronous wait necessary.

## Current Files / Modules

| Path | Ownership |
| --- | --- |
| `settings.gradle`, `build.gradle`, `gradle.properties`, `local.properties` | Gradle project config |
| `gradlew(.bat)`, `gradle/wrapper/*` | Gradle 8.14.3 wrapper |
| `app/build.gradle` | Android app module config, SDK/NDK/CMake/ABI pinning |
| `app/src/main/AndroidManifest.xml` | App/activity declaration, Vulkan feature requirement |
| `app/src/main/java/.../ForgeShapeActivity.java` | Android lifecycle, `FrameLayout` root composition, resume refresh |
| `app/src/main/java/.../ForgeShapeSurfaceView.java` | Viewport surface, forwards lifecycle + raw pointer state, takes focus back from an editor |
| `app/src/main/java/.../ConstructionPanelView.java` | Shape and placement field text, display unit, DRAFT primitive kind, validation messages, Apply Shape and Apply Transform. Owns no parameter, no authoritative kind and no transform |
| `app/src/main/java/.../SculptPanelView.java` | Sculpt Mode strip: the Grab/Clay/Smooth/Inflate selector, the shared Radius and Strength sliders, Back to Construction. Owns no vertex, no brush value, no tool and no mode — it reads all four back from native state |
| `app/src/main/java/.../LengthUnit.java` | Exact `BigDecimal` mm/cm/m ↔ meter conversion, parsing and formatting |
| `app/src/main/java/.../NativeViewport.java` | JNI declarations, library load, `APPLY_*` and `SCULPT_*` status codes, `MODE_*`, `TOOL_*` |
| `app/src/main/cpp/forgeshape_jni.cpp` | JNI boundary, render thread, ANativeWindow, MotionEvent→TouchAction, camera + selection locking |
| `app/src/main/cpp/forgeshape_input.h` | Shared platform-neutral touch event data (`TouchAction`, `TouchPointer`) |
| `app/src/main/cpp/forgeshape_camera.{h,cpp}` | Camera pose, projection, gesture state machine |
| `app/src/main/cpp/forgeshape_camera_selftest.{h,cpp}` | Debug-only deterministic camera checks |
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all five primitives + transform), `ConstructionBox`, `ConstructionCylinder`, `ConstructionSphere`, `ConstructionCone`, `ConstructionCapsule`, the shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation (including `validateCapsuleMeters`, the capsule relation), deterministic local mesh generation, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
| `app/src/main/cpp/forgeshape_primitive_selftest.{h,cpp}` | Debug-only deterministic active-object, cylinder topology/bounds/winding and transformed-pick checks |
| `app/src/main/cpp/forgeshape_sphere_selftest.{h,cpp}` | Debug-only deterministic typed-boundary, sphere topology/closure/bounds/winding/degeneracy and transformed-pick checks |
| `app/src/main/cpp/forgeshape_cone_capsule_selftest.{h,cpp}` | Debug-only deterministic cone and capsule checks: typed payloads, apply semantics, the capsule relation and float resolvability, topology/closure/bounds/winding/degeneracy, the equality case, picking and transformed picking, and the five-kind round trip (163 checks) |
| `app/src/main/cpp/forgeshape_construction_selftest.{h,cpp}` | Debug-only deterministic Construction-box checks |
| `app/src/main/cpp/forgeshape_sculpt.{h,cpp}` | `ProductMode`, `SculptTool`, `SculptSession` (mode + tool + brush + live stroke + the `hitsSculptMesh` arbitration probe), `SculptMesh` (the Frozen Sculpt Mesh, its `SculptRevision`, its topology and its normal cache), `SculptTopology` (1-ring adjacency + incident triangles, built once per Freeze), `computeVertexNormals`, `SculptStroke` (the one kernel: hit, affected set, falloff, radius resolution, lifecycle, plus one `apply*` per tool), sculpt publication into `MeshStore` |
| `app/src/main/cpp/forgeshape_sculpt_selftest.{h,cpp}` | Debug-only deterministic Freeze, storage-independence, mode-switching, stale-source, falloff, radius, strength, transform-conversion and stroke-lifecycle checks, plus the tool set, the adjacency, the normals, the shared kernel across all four tools, the arbitration probe, and Clay / Smooth / Inflate including the Clay-is-not-Inflate measurement (232 checks) |
| `app/src/main/cpp/forgeshape_transform.{h,cpp}` | `ConstructionTransform`: authoritative double-meter position and double-degree rotation, THE axis/Euler convention, validation, atomic apply, derived model and inverse-model matrices |
| `app/src/main/cpp/forgeshape_transform_selftest.{h,cpp}` | Debug-only deterministic transform, convention, inverse and transformed-pick checks |
| `app/src/main/cpp/forgeshape_picking.{h,cpp}` | Screen→world ray, ray/triangle, nearest hit, winding check |
| `app/src/main/cpp/forgeshape_selection.{h,cpp}` | `ObjectId`, `SelectionController`, tap-vs-navigation, `pickScene` |
| `app/src/main/cpp/forgeshape_object_id.h` | `ObjectId` type and reserved values, shared by the mesh and selection layers |
| `app/src/main/cpp/forgeshape_mesh.{h,cpp}` | `RuntimeMesh` (immutable revision), `MeshStore`, validation, capacity policy, upload diagnostics |
| `app/src/main/cpp/forgeshape_mesh_fixtures.{h,cpp}` | DEBUG test fixtures (baseline / same-topology / larger / stress step) |
| `app/src/main/cpp/forgeshape_mesh_selftest.{h,cpp}` | Debug-only deterministic runtime-mesh checks |
| `app/src/main/cpp/forgeshape_demo_mesh.{h,cpp}` | Baseline cube numbers; source data for the baseline fixture only |
| `app/src/main/cpp/forgeshape_picking_selftest.{h,cpp}` | Debug-only deterministic picking/selection/winding checks |
| `app/src/main/cpp/forgeshape_renderer.{h,cpp}` | Vulkan renderer, frame loop, camera snapshot + selection highlight consumer |
| `app/src/main/cpp/forgeshape_math.h` | Minimal self-owned vec3/mat4 |
| `app/src/main/cpp/shaders/cube.{vert,frag}` | GLSL source, AOT compiled to SPIR-V |
| `app/src/main/cpp/CMakeLists.txt` | Native build + glslc shader step |
| `artifacts/stage003_native_viewport.png` | Stage 003 visual evidence |
| `artifacts/stage004_camera_{initial,orbit,pan,zoom}.png` | Stage 004 visual evidence |
| `artifacts/stage005_selection_{unselected,selected,cleared,after_camera,after_resume}.png` | Stage 005 visual evidence |
| `artifacts/stage006_mesh_{baseline,deformed,larger,after_stress}.png` | Stage 006 visual evidence |
| `artifacts/stage007_box_{state_a,state_b,state_c,after_resume}.png` | Stage 007 visual evidence |
| `artifacts/stage008_*.png` | Stage 008 visual evidence (15 images: initial, unit switching, valid apply, decimal state, unchanged, four invalid cases, orbit, home/resume) |
| `artifacts/stage009_*.png` | Stage 009 visual evidence (15 images: initial, unit switching, position, rotation, combined, unchanged, three invalid cases, resize under transform, orbit, home/resume) |
| `artifacts/stage010_*.png` | Stage 010 visual evidence (14 images: initial box, selector draft, cylinder, cylinder from below, transformed, resized, back to box, unchanged, three invalid cases, orbit, home/resume) |
| `artifacts/stage011_*.png` | Stage 011 visual evidence (9 images: initial box, sphere selector draft, applied sphere, transformed sphere, two invalid cases, back to box, remembered sphere/cylinder drafts, home/resume, after orbit) |
| `artifacts/stage012_*.png` | Stage 012 visual evidence (13 images, all from the one authoritative process: before/after Freeze, after Grab, two radius states, two strength states, orbit in Sculpt, the Construction source after sculpting, the resumed sculpt, before/after home, stale source) |
| `README.md` | Required tool versions, build/run/verify instructions |
| `ARCHITECTURE.md` | Current production architecture and ownership map |
| `PRODUCT.md` | Runtime-verified user-visible behaviour |
| `CLAUDE.md` | Durable repository rules |

## Next Recommended Stage

**Owner UI Architecture Decision for Stage 015B**

Stage 015A ends deliberately before implementation. Two of its six questions are
not engineering preferences that a stage can settle on evidence — they are
product decisions. **D2 (primary device)** changes the answer to **D1 (shell
direction)**: Option B earns its rail and three-pane layout on a tablet and does
not earn them on a 411 dp phone, where Option A is cheaper and lower risk for the
same landscape fix. Choosing B on the owner's behalf would be choosing a device
story on the owner's behalf.

**D3 (Views or Compose)** is recommended firmly — Views — because the evidence is
one-sided: the project has zero runtime dependencies today, and Compose would add
a second language, AndroidX and 30+ artifacts while placing the most carefully
proven behaviour in the product under a pointer pipeline it does not control. It
is listed as a decision only because it commits the project's dependency posture
for years, which is the owner's call to ratify.

What the audit contributes regardless of the answers: **landscape is currently
broken, not merely cramped**, and the Android layer remains the only part of the
product with no automated test of any kind. Nine native suites and 992 checks run
themselves; every UI assertion in every stage so far has been driven by hand
through screen coordinates. Tests T8 (viewport ≥ 55 % unoccluded at four window
sizes) and T9 (a drag inside a panel produces no camera change and no
`SculptRevision`) are the two that would have caught what this audit found by
hand, and they should land with the shell rather than after it.

### Why Plane and the coverage cleanup remain the geometry candidates

Stage 014 answered the question Stage 013 posed about the Construction side: is
the per-kind repetition honest, or a registry trying to be born? Two more
primitives cost a member, a `PrimitiveKind` case, a variant alternative, a
per-kind JNI method, a panel row and a generator each — and every one of those
was a real decision rather than boilerplate. The typed payload earned itself
outright here: a cone's `(diameter, height)` and a capsule's
`(diameter, totalHeight)` are the same two numbers in the same order and mean
different things, and nothing in the product can read one as the other.

The two primitives tested different things, as intended. The cone exercised the
apex singularity — one vertex, one fan, no second ring — and its base is the
first surface whose closure had to be proven rather than assumed. The capsule was
the first *composition*, and it forced the one genuinely new idea in the stage:
its two parameters are **related**, so `DimensionValidation` grew a reason and
validation grew an owner. Its degenerate case is the part worth remembering — a
capsule as tall as it is wide is a sphere, and generating it as one, with the two
seam rings collapsed into a single shared ring, is what keeps the equality case
free of duplicate rings and zero-area triangles instead of hiding them.

Two things this stage surfaced belong to the next one rather than to a patch.
Tessellation now has a single owner, which is the right shape, but it also made
visible that the capsule's middle — like the cylinder's side wall — carries no
interior rings however long it is, so sculpt fidelity there is coarse in a way
the sphere's is not. And capsule topology is now a function of its parameters
rather than a constant, which is correct and is also the first time a shape edit
can change a vertex count.

Plane is the right next primitive precisely because it breaks the pattern the
other five share: it is the first primitive that is **not a closed solid**. Every
invariant this stage leaned on — closure, `V − E + F = 2`, every directed edge
matched by its reverse, a ray from inside missing under front-face-only picking —
is either false or meaningless for a single-sided surface. That is worth finding
out deliberately, with one primitive, rather than discovering it later underneath
something else. It is also the moment to do the coverage cleanup: five primitives
in, the per-kind surface is wide enough to be worth reading once as a whole and
deciding, on evidence rather than anticipation, which parts are honest repetition
and which have become a pattern the code should state directly.
