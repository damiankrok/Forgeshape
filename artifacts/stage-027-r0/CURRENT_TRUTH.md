# STAGE027-R0 — Current truth map

Audit only. Nothing in this file is a proposal; every claim below names the file
and line that carries it, read at `6808fae17b1ad06530cdcd38d8962b1298f112a8`.

Where a statement is derived from source reading alone and has **not** been run
on a device in this audit, it says so. This audit built nothing and ran nothing.

---

## 1. Object visibility and lock — who owns what

### 1.1 The durable facts

| Fact | Owner | Kind |
| --- | --- | --- |
| `visible_` | `SceneObject`, `forgeshape_scene.h:248` | durable project truth |
| `locked_` | `SceneObject`, `forgeshape_scene.h:249` | durable project truth |
| accessors | `visible()`/`setVisible()` `:190-191`, `locked()`/`setLocked()` `:205-206` | — |
| defaults | `true` / `false`, `:248-249` — initializers, not a migration | — |

Both are **representation-neutral**: nothing in `forgeshape_body_commands.cpp`
asks what generates a body's geometry in order to decide whether it can be
hidden or locked (`forgeshape_body_commands.h:60-152`).

### 1.2 The one enforcement point for Hidden

`ConstructionScene::snapshot()`, `forgeshape_scene.cpp:261-306`:

```cpp
for (const auto& body : bodies_) {
    if (!body->visible()) {
        continue;          // scene.cpp:264-277
    }
    ...
}
```

`snapshot()` is the **single list** the renderer and CPU picking both consume,
so "not drawn" and "not picked" are one fact and not two predicates. Evidence
that both really do read it:

- renderer: `forgeshape_jni.cpp:1652` — `renderer.setScene(constructionScene().snapshot())`;
- picking: `forgeshape_selection.cpp:14-18` — `pickScene(...)` forwards
  `constructionScene().snapshot()` into `pickSceneSnapshot(...)`;
- selection outline: the mask pass rasterises the same snapshot, so a hidden
  body leaves no stale silhouette (`forgeshape_scene.cpp:270-273`).

A hidden body **keeps** its `ObjectId`, its Objects row, its selection, its
published `MeshRevision`, its `.forge` record and its export.

### 1.3 The command path

`setSceneBodyVisible(id, visible, scene, history)` —
`forgeshape_body_commands.cpp:123-138`:

- resolves the target through the shared `resolveTarget` (unknown body /
  edit-in-progress refusals);
- wraps exactly one `ScopedConstructionEdit` → **one Undo**;
- **does not move the selection** when the active body is hidden
  (`:133-135`, and the rationale at `forgeshape_body_commands.h:135-141`);
- publishes nothing, mints no `MeshRevision`, uploads nothing.

JNI: `Java_..._sceneSetBodyVisible`, `forgeshape_jni.cpp:3071-3100`.
Read-back: `Java_..._sceneBodyVisible`, `:3058-3068` (an unknown body answers
*visible*).

### 1.4 The mode guard — the load-bearing fact for Stage027

```cpp
// forgeshape_jni.cpp:3017-3019
bool objectCommandsBlockedByMode() {
    return forgeshape::sculptSession().inSculptMode() || forgeshape::sketchSession().active();
}
```

`sceneSetBodyVisible` consults it at `:3078-3083` and returns
`kObjCmdRefusedInSculpt`, logging
`FORGESHAPE_SCENE_VISIBILITY_REFUSED:in_sculpt_mode:<id>`.

**So the durable Hide command cannot be executed at all while Sculpt is
active.** The UI agrees: `EditorWorkspaceView.java:2551` calls
`objectsSection.showObjectCommandsAvailable(!sculpting && !sketching)`, which
withdraws the whole row overflow (Rename, Show/Hide, Lock/Unlock, Duplicate,
Mirror) and closes any strip standing open (`ObjectsSectionView.java:325-336`).
`+` and Delete are withdrawn on the same terms
(`EditorWorkspaceView.java:2533-2545`).

The Objects **list itself stays** in Sculpt — deliberately, so the scene is
still legible (`ObjectsSectionView.java:289-292`).

This is the gap the historical phrase *"Isolate/hide … Sculpt workflow"* names:
**visibility is delivered, and it is delivered everywhere except where Stage027
wants it.**

### 1.5 Persistence

`SCNE` **version 2**, `DATA_PACKAGE_SPEC.md:192-249`:

```
u8 flags        bit0 hidden, bit1 locked; all others RESERVED
```

- v2 is written **only** when some body is hidden, locked or carries an
  `SCNE`-owned name (`:213-218`); otherwise the file stays v1 and byte-identical.
- `SCNE` is required, so an older build **refuses** a v2 file rather than
  opening it with a lock silently dropped (`:221-224`).
- A reserved flag bit is refused, never masked (`:249`,
  fixture `object_state_bad_flags_v2.forge`).

Visibility therefore **moves the project fingerprint** and **reaches `.forge`
bytes**. Any transient Stage027 state must not.

### 1.6 Lock

`locked` stays **visible and pickable**; what it removes is being *moved*
(`forgeshape_scene.h:193-206`). Two named guards: the gizmo is not activated
(`FORGESHAPE_GIZMO_REFUSED:body_locked`) and a transform write is rejected
(`APPLY_REJECTED_LOCKED`). Lock does not change Delete. Lock is irrelevant to
Isolate except as the pattern for "a state that refuses an act by name".

### 1.7 Who reads visibility above JNI

Exactly two call sites, both in the Objects row:

```
ObjectsSectionView.java:525  final boolean visible = NativeViewport.sceneBodyVisible(objectId);
ObjectsSectionView.java:735  final boolean wantVisible = !NativeViewport.sceneBodyVisible(objectId);
```

Nothing else in the Java layer reads it. **Consequence:** Start Sculpting /
Resume Sculpt are **not** gated on the body being visible — see §4.2.

---

## 2. Sculpt — what exists today

### 2.1 Mode and target

- `SculptSession` is **one** session for the whole product
  (`forgeshape_scene.cpp:497-518`). It owns the mode, the tool, Radius and
  Strength and any stroke in progress.
- The per-body half — the Frozen Sculpt Mesh, its stale flag and its
  `SculptHistory` — lives in `SceneObject::frozenSculpt()`
  (`forgeshape_scene.h:224-225`) and is **borrowed**.
- `sculptSession()` **re-binds to the active body's `FrozenSculpt` on every
  access** (`forgeshape_scene.cpp:516`). This is deliberate and is the fact
  that makes §4.1 below a real risk rather than a theoretical one.
- Entry: `enterSculpt()` `forgeshape_sculpt.cpp:1462-1469` — refuses unless
  something is frozen; **does not consult `visible()`**.
- Exit: `enterConstruction()` `:1448-1460` — keeps the mesh, the tool and the
  Sculpt history untouched; Resume comes back to all of them.
- Freeze: `Java_..._freezeToSculpt` `forgeshape_jni.cpp:2589-2640` — refuses a
  CAD Body (`CadBodyNotSculptable`) and a body with no geometry source;
  **does not consult `visible()`**.

### 2.2 What is drawn while sculpting

In Sculpt the active body publishes its **sculpt** mesh into its own
`MeshStore` (`publishActiveRepresentation`, `forgeshape_jni.cpp:1060-1071`) and
every **other** visible body publishes whatever it already had. The renderer is
handed the ordinary `constructionScene().snapshot()` — there is no sculpt
branch at `forgeshape_jni.cpp:1649-1652`.

**So Sculpt draws the whole scene.** `PRODUCT.md:1287-1288` states this as a
deliberate product fact: *"The other bodies stay visible while you sculpt, so
you can see what you are working against."*

### 2.3 The stroke hit test does NOT consult the scene

`SculptSession::hitsSculptMesh` (`forgeshape_sculpt.cpp:1471-1492`) casts the
ray against **the active body's own triangle view alone**. It never calls
`pickScene`, so an occluding body is not consulted.

**Consequence, source-backed:** a body standing between the camera and the
sculpt target **hides the target from the eye but does not block the brush**.
The user sculpts geometry they cannot see. That is the concrete, mechanical
user value an Isolate would deliver, and it is not an aesthetic preference.

### 2.4 User-visible Sculpt controls (inventory)

| Control | Where | Notes |
| --- | --- | --- |
| Start Sculpting / Resume Sculpt | Global Toolbar | one button, two labels |
| Back to Construction / ← Imported Mesh | Global Toolbar | the two approved short forms |
| Seven tools (Grab, Clay, Smooth, Flatten, Inflate, Crease, Mask) | Tool Rail | `SculptContextView.TOOL_HINTS:39-42` |
| Radius, Strength | `BrushEdgeControlsView` | always on screen, shared across bodies |
| Sculpt mesh summary, mask summary, stale-source warning, Clear Mask, Reset Sculpt from Shape… | `SculptContextView` | the Property Inspector body |
| Undo / Redo | history capsule | routed to `SculptHistory` by mode (`forgeshape_jni.cpp:5474-5512`) |
| History navigator | history capsule, Sculpt only | `SculptHistoryNavigatorView` |
| Objects list (read + select attempt) | Objects capsule / column | list present; **all commands withdrawn** |
| Display popover: Shading, Surface, Projection, Grid, Selection Outline | Global Toolbar | session-only, native-owned |

**Absent in Sculpt:** `+`, Delete, the row overflow (Rename/Show/Hide/Lock/
Duplicate/Mirror), the transform gizmo, Dimensions, Relative Scale, the shape
and placement editors.

### 2.5 Sculpt history and mask — untouched by Stage027

`SculptHistory` is bounded, volatile, per body, never serialized
(`forgeshape_sculpt_history.h`; `CLAUDE.md` Sculpt-history rule). The Mask is a
runtime-local per-vertex weight, never project truth. Stage027 is *non-history*
by its own phrase and must not add a second stack or touch either.

---

## 3. Renderer — what is actually available

### 3.1 The one scene seam, and the precedent for replacing it

`forgeshape_jni.cpp:1642-1652`:

```cpp
if (forgeshape::importedMeshPreview().visible()) {
    renderer.setScene(forgeshape::importedMeshPreview().snapshot());
} else {
    renderer.setScene(forgeshape::constructionScene().snapshot());
}
```

There is **exactly one** `setScene` call, and it already carries a precedent for
a **session-only scene REPLACEMENT** that changes no project truth: the Imported
Mesh Preview (`forgeshape_import_preview.h:1-40`). That is the closest existing
shape to a transient Isolate.

`pickSceneSnapshot(camera, x, y, w, h, snapshot)` already takes the snapshot as
an **argument** (`forgeshape_selection.h:77`), so a filtered list can feed the
renderer and the picker from one place and keep "not drawn = not picked" as a
single fact rather than a new second predicate.

### 3.2 Pipelines that exist

| Pipeline | Topology | Shader |
| --- | --- | --- |
| surface | `TRIANGLE_LIST` (`renderer.cpp:2427`) | `surface.vert/frag` |
| grid | `LINE_LIST` (`:2549`) | `grid.vert/frag` |
| gizmo / sketch overlay | `LINE_LIST` (`:2695`) | `gizmo.vert/frag` |
| outline mask | `TRIANGLE_LIST` (`:3067`) | `outline_mask.*` |
| outline composite | `TRIANGLE_LIST` (`:3155`) | `outline.*` |

### 3.3 The two hard renderer facts for "Mesh Preview"

**(a) No polygon-mode wireframe is available.** `VkDeviceCreateInfo` at
`forgeshape_renderer.cpp:601-608` sets **no `pEnabledFeatures` at all**. Every
pipeline states `polygonMode = VK_POLYGON_MODE_FILL` and `lineWidth = 1.0f`, and
the grid pipeline's own comment says so outright (`:2567-2577`):
`VK_POLYGON_MODE_LINE` would require the `fillModeNonSolid` device feature,
*"which ForgeShape does not request"*. A wireframe by polygon mode is therefore
**a device-creation change plus a feature query plus a no-feature fallback**, on
a feature that is optional on Android GPUs.

**(b) The existing line path is capped at 65 536 vertices.**
`kMaxSketchOverlayVertices = 65536` (`forgeshape_sketch_overlay.h:93`) = 32 768
line segments. A frozen sphere (482 verts / 960 tris,
`forgeshape_construction.h:129-134`) needs ~1 440 edges = 2 880 overlay vertices
and fits comfortably. An **Imported Mesh** may carry up to
`kMaxMeshVertices = 4 000 000` (`forgeshape_mesh.h:65`) and does not fit by
three orders of magnitude. The existing overlay path **cannot** carry a general
mesh wireframe.

### 3.4 Shading models that exist

`ShadingModel` = `StudioSolid`, `MatCap`, `DebugSourceColor`
(`forgeshape_display.h`). `DebugSourceColor` is marked **DEBUG ONLY**.
`SurfaceShading` = Smooth / Flat. There is **no** wireframe, no topology-edge,
no x-ray and no density visualisation anywhere in the product.

### 3.5 The pattern a transient toggle would follow

`DisplaySettingsStore` (`forgeshape_display.h`) is process-scoped, native-owned,
atomic, session-only, survives rotation and resume, reaches no `.forge` byte and
is explicitly **not** an `AppPreferences` field. `Grid` and `Selection Outline`
are its two booleans. Any transient Stage027 flag has an exact existing home and
an exact existing precedent here.

---

## 4. Contradictions and gaps found while reading

### 4.1 `FINDING-A` — the Sculpt target is not actually fixed against a viewport tap

**SOURCE-CONFIRMED, RUNTIME-UNVERIFIED in this audit.**

`PRODUCT.md:1286-1290` and `forgeshape_jni.cpp:2800-2806` both state that the
Sculpt target is fixed for the duration of Sculpt Mode. `sceneSelectBody`
enforces it (`:2813-2827`, returns `kSculptFailedFreeze`, logs
`FORGESHAPE_SCENE_SELECT_REFUSED:in_sculpt_mode`) — that is the **Objects row**
path.

The **viewport tap** path does not go through it. In Sculpt, a Down that
*misses* the sculpt mesh is deliberately **not** swallowed
(`forgeshape_jni.cpp:7118-7120`: *"A miss starts no stroke and is deliberately
NOT swallowed: the gesture falls through and orbits"*). `grabHandled` stays
false, so control reaches `forgeshape_jni.cpp:7250-7284`, where the only
suppression is the imported preview (`:7267-7269`), and then:

```cpp
hit = forgeshape::pickScene(g_camera.snapshot(), tapX, tapY, viewWidth, viewHeight);
selectionChanged = g_selection.applyPick(hit);
if (hit.hit) {
    forgeshape::constructionScene().setActiveBody(hit.objectId);   // :7283
}
```

`setActiveBody` is called **directly on the scene**, bypassing the JNI guard.
Because `sculptSession()` re-binds to the active body on every access
(`forgeshape_scene.cpp:516`), the session would then be pointing at a different
body's `FrozenSculpt` while `mode_` is still `Sculpt` — and `bindTarget`
cancels any stroke in flight (`forgeshape_sculpt.h:844-850`).

Why it matters to Stage027: Isolate removes other bodies from the snapshot, so
an isolated session would *incidentally* make this unreachable. **That must not
be mistaken for a fix.** The guard is its own work and belongs beside, not
inside, Isolate.

### 4.2 `FINDING-B` — Sculpt is reachable on a hidden body

**SOURCE-CONFIRMED, RUNTIME-UNVERIFIED in this audit.**

Neither `freezeToSculpt` (`forgeshape_jni.cpp:2589-2640`) nor `enterSculptMode`
(`:2696-2716`) consults `body.visible()`, and the Java layer reads visibility
only in the Objects row (§1.7). A user can hide the active body in Construction
and then press Start Sculpting: the mode is entered, strokes land, the
`SculptRevision` advances, `SculptHistory` records entries and the fingerprint
moves — while `snapshot()` draws nothing, because the body is hidden.

Stage027 must state what this means. It is the single place where the existing
durable visibility truth and the Sculpt workflow already collide.

### 4.3 `FINDING-C` — `PRODUCT.md` is stale about the selection outline

`PRODUCT.md:1946-1948` and `:1955-1956` still say *"Selection is still a tint
over the whole body, not an outline"* and *"no outline around the selected
object"*. `SEL-OUT-R1` is delivered (`CLAUDE.md` selection-outline rule;
`forgeshape_selection_outline.h`; `outline.frag`; `kSelectionRestingAlpha = 0`).

**Not corrected in this audit.** It does not prevent truthful reporting about
Stage027, and §8 of the task authorises only the smallest correction that a
blocking contradiction would force. Reported to the coordinator instead.

---

## 5. Fact classification

| Fact | Classification |
| --- | --- |
| `SceneObject::visible_` / `locked_` | **domain truth**, in `.forge` (`SCNE` v2) |
| `ConstructionScene::activeBodyId_` | domain truth, in `.forge` |
| `FrozenSculpt` mesh positions + topology | domain truth, in `.forge` (`SCUL`) |
| `SculptMesh::hasEdits_` | domain truth, in `.forge` |
| `MeshVertex::mask` | **runtime-local**, never serialized |
| `SculptHistory` | **runtime-local, volatile, per body**, never serialized |
| `SculptSession` mode / tool / radius / strength | **session-only**, native-owned |
| `DisplaySettingsStore` (Grid, Selection Outline, Shading, Surface) | **session-only**, native-owned, process-scoped |
| `ImportedMeshPreview` | **session-only diagnostic**, replaces the frame's scene |
| `AppPreferences` | **application state**, `SharedPreferences`, never project truth |
| normals, adjacency, render mesh, GPU buffers | **derived**, regenerated |
| Java view state (expanded row, open strip) | **UI draft state**, rebuilt from native on refresh |
