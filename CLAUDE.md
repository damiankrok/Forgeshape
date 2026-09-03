# CLAUDE.md — durable rules for this repository

Short and durable. Not a stage diary.

## Workspace

`D:\TRAVELAPPS\ForgeShape` — standalone Android app, Java shell + native C++17
Vulkan renderer.

## Commands

```
gradlew.bat :app:assembleDebug
adb -s <serial> install -r app\build\outputs\apk\debug\app-debug.apk
adb -s <serial> shell am start -n com.forgeshape.app/.ForgeShapeActivity
adb -s <serial> logcat -s ForgeShape:V
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

A clean debug launch emits **seventeen** `*_SELFTEST_OK` tokens, then
`FORGESHAPE_NATIVE_VIEWPORT_OK`. All seventeen, in emission order:

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
```

Failures: `FORGESHAPE_NATIVE_VIEWPORT_FAIL:*` and the matching `*_SELFTEST_FAIL`.
Grep for `FAIL` alone over-matches: some passing check *names* contain "fails"
(`invalid_revision_fails_closed`). Match `_SELFTEST_FAIL` or `_FAIL:`.

Enlarge the log ring buffer (`adb -s <serial> logcat -G 64M`) before capturing
startup evidence: the default buffer drops part of the self-test output and it
looks like a suite that stopped partway. 16M was enough through Stage 020 and is
no longer — at E2E-R1A a 16M capture silently lost two of the fourteen suites,
with no `chatty` marker and no FAIL line to give it away. Confirm the size with
`adb -s <serial> logcat -g`, and treat a missing token with zero failures as a
dropped capture until a larger buffer proves otherwise.

Camera, picking, dynamic-mesh, Construction-box, sculpt, render-shading,
Construction-history, gizmo, project-format and render-recovery self-tests are
debug-only and run once from `NativeViewport.start()`. They must never run per frame. Each builds the domain
objects it needs — the scene, history and gizmo suites build their own
`ConstructionScene` — rather than reading process-scoped state, so a suite's
result never depends on what a live session left behind. The project suite also
prints `FORGESHAPE_PROJECT_GOLDEN_SHA256`, `..._IMPORTED` and
`..._IMPORTED_SCULPT` — the digests of all seven canonical `.forge` fixtures as
this build encodes them — so drift from the committed corpus is a value that can
be read rather than only an assertion that failed.

## Hard rules

- **No engine.** Godot, GDExtension, Unity, Unreal or any general-purpose engine
  that owns the viewport or render loop is prohibited in the product.
- **No third-party runtime, rendering, math or input library.** Math lives in
  `app/src/main/cpp/forgeshape_math.h`. No GLM.
- **Git is local only.** The root repository exists (initialized in Gate P0 with
  owner approval). Commits and branches are allowed. **No remote, no push, no
  GitHub repository, and no global Git config changes** — the committer identity
  lives in `.git/config` alone. Never commit generated build output.
- **The NDK is pinned to `29.0.14206865`** in `app/build.gradle`. Do not bump it,
  do not use an r30 beta, and do not broadly upgrade AGP, Gradle, the JDK or
  CMake as a side effect of anything else.
- **Read `PROJECT_STATUS.md` first, and update it yourself** before reporting a
  stage complete. It records only verified current facts and names exactly one
  next stage.
- **No scope expansion.** Implement what the stage asks for and stop. Do not
  start the next stage.
- **Preserve existing owner changes.** Modify only what the task requires; do not
  opportunistically refactor the renderer or bundle unrelated debt fixes.
- **One brush kernel, no brush framework.** A sculpt tool is a deformation rule
  inside `SculptStroke`, selected by a closed enum and a switch. Do not add a
  brush base class, registry, plugin surface or reflection: brushes are code, not
  data, until a stage pays for making them data.
- **A gesture that becomes multi-touch navigation must never mutate the sculpt
  mesh.** No vertex written, no `SculptRevision` minted, no stroke committed.
- **A body's SOURCE is never written by sculpting.** A body has a source
  representation — a Construction Source, or an Imported Mesh — and may also own
  a Frozen Sculpt Mesh. A sculpt edit may never change a primitive parameter, a
  `PrimitiveKind`, a transform, or one byte of an imported object's positions,
  normals, topology or submesh batches; and no Construction parameter may ever be
  reconstructed from sculpt vertices. Adopting a changed source into the sculpt
  mesh is always an explicit user act, never automatic. `buildSculptSourceMesh`
  is the ONE place a seed comes from, beside `publishSceneObject` for what a body
  draws; it copies an imported object's RAW indices, never `buildDrawData`'s
  reversed duplicates (they would cancel every area-weighted normal), and
  collapses per-submesh `doubleSided` into the frozen mesh's one sidedness answer,
  permissively. An Imported Mesh can never go STALE: nothing can edit one.
- **There is one Construction history, it is native, and a user act is one
  transaction.** `ConstructionHistory` owns undo/redo and the transaction
  boundary; no Java-side mirror scene, snapshot list or depth counter may become
  a second answer. A step holds bounded Construction-domain state — never mesh
  bytes, never a sculpt vertex or `SculptRevision`. A multi-mutation act opens
  ONE edit around its mutations rather than recording each; a commit that finds
  nothing different records nothing and must leave the redo stack alone. The
  `ObjectId` allocator is never rolled back: an undone creation's id, and a
  deleted body's, are restored by name and never reused. Construction undo may
  never move a sculpt vertex — the only sculpt state a restore touches is the
  existing stale-source flag. A body an act removes from
  the scene is HELD by the history rather than destroyed (`holdDetachedBody`),
  because an Imported Mesh's geometry and a Frozen Sculpt Mesh are the two things
  a step cannot rebuild; one is released only when NO step in either stack names
  it, on either side. **Seeding a session is not a user act**: what a new session
  does to reach the state the user starts from runs inside
  `beginSessionInitialization`/`end`, records nothing, and leaves an empty
  history both ways. That bracket is reachable only from the start answer —
  never from Back to Construction, a rotation or a resume — and the debug-only
  `clear()` stays a test seam, never production behaviour.
- **Sculpt has its OWN history, and the two never merge** (`ARCH-OWNER-12`).
  Project/object history never stores a sculpt vertex, a sculpt mesh snapshot or
  a `SculptRevision` — that rule did not move. What changed is the answer to
  what a stroke can cost: `SculptHistory` is a **bounded, volatile, per-body**
  Undo/Redo over completed strokes, living in `FrozenSculpt` beside the mesh it
  describes, so ownership alone makes one body's Undo incapable of reaching
  another's. **One completed stroke is exactly one entry** — never one per
  pointer event — recorded once in `SculptSession`'s stroke close, from the
  affected set the stroke captured at pointer-down; a stroke that moved nothing
  records nothing, and a cancelled stroke whose positions stand records them too,
  because the deformation the user can see must be one they can take back. An
  entry is a DELTA (sorted unique indices, before and after positions, and the
  edited flag on both sides), never normals, never a document, never a
  Construction parameter, never a transform. Both caps are enforced —
  `kMaxSculptHistoryEntries` and `kMaxSculptHistoryBytes`, with
  `kMaxSculptHistoryEntryBytes` for one stroke — evicting oldest-first and never
  the newest; a stroke too large to retain still APPLIES and says so by name
  rather than silently. **`hasEdits` is a stored fact, not `undoDepth > 0`**: a
  project loaded with edits starts with an empty history and must still report
  them. **Revisions stay monotonic**: geometry goes backwards while the counter
  goes forwards, because the renderer, the picker and the autosave fingerprint
  all notice a sculpt change by that number. **Nothing here is ever serialized**
  — no `.forge` byte, no checkpoint, no schema change — so reopening a project
  restores the geometry and starts a fresh, empty history. A Freeze or a
  destructive Reset from source clears it; Undo cannot cross that boundary. The
  two chrome controls are drawn in both modes and native code alone decides
  which history a tap means, from the product mode; the CONSTRUCTION entry
  points still refuse in Sculpt, because removing a control is not removing a
  guard.
- **A `.forge` project file is a semantic document, and loading one is
  all-or-nothing.** The format is ForgeShape's own, versioned, and portable
  between compatible installations: every field is a file-owned fixed-width
  little-endian encoding, and no struct image, enum ABI value, pointer,
  `size_t`, device path, Android type, window state or GPU handle may ever
  reach it. What is stored is what cannot be recomputed — identity, scene
  order, the active body, the id allocator's high-water mark, the active
  primitive kind, **all six** remembered parameter sets, placement, and each
  Frozen Sculpt Mesh's local float32 positions and index topology. Construction
  meshes, normals, adjacency, revisions, GPU state and vertex colours are
  derived and are regenerated, never written. Session history is not project
  truth: a successful load starts a fresh one, and a **failed load changes
  nothing at all** — decode and validate entirely into temporary state, then
  commit in one step. The codec is platform-neutral and knows no filesystem;
  where the bytes live is the Android adapter's business alone.
  An **Imported Mesh is the exception that proves the rule**: its name, local
  positions, effective normals, index topology and submesh ranges with their
  `doubleSided` ARE stored, because no rule could recreate them and the `.glb`
  is not part of the project. Its section is `IMPT`, always required so a reader
  that cannot rebuild one refuses the file instead of opening it with objects
  missing; `CONS` carries only the bodies that HAVE a Construction Source; and
  a body named by both branches, or by neither, is refused. A `SCUL` entry's body
  may have EITHER source (`IMPORT-01B`), and which one it was frozen from is not
  stored because nothing reads it back; the four valid combinations are
  `SCNE+CONS`, `SCNE+CONS+SCUL`, `SCNE+IMPT` and `SCNE+IMPT+SCUL`. That needed no
  version bump, because an older build REFUSES an `IMPT`+`SCUL` file rather than
  opening half a body. No source path, `Uri` or byte of the `.glb` may ever reach
  the document.
  `DATA_PACKAGE_SPEC.md` owns the layout, and `scripts/build-forge-corpus.ps1`
  is a second implementation of it whose bytes must stay identical.
  **GLB/glTF, OBJ and FBX are not `.forge`.** A `.glb` is written by Export and
  read by Import into objects that then live in `.forge` like anything else; it
  is never a project and is never referenced by one. OBJ and FBX are absent in
  both directions, and no inert menu entry for one may be drawn.
- **Autosave protects work; it never overwrites what the user chose to keep.**
  The manual slot and the recovery checkpoint are two different files written by
  two different acts, and only an explicit Save touches the one the user named.
  A checkpoint is the SAME canonical `.forge` document — no delta, no journal,
  no second format. It is written only when the project actually changed
  (`projectSemanticFingerprint` hashes the semantic VALUES, never the domain's
  update counters, because an undo deliberately does not advance those),
  coalesced so a gesture costs one write rather than hundreds, taken off the UI
  thread, and written temp-file + fsync + rename so a failed checkpoint can
  never destroy the previous valid one. The debounce is an implementation
  detail: it is not product semantics, and **no test may sleep for it** —
  `awaitIdle` is the barrier.
- **Nothing replaces the live project without the user saying so.** A recovery
  candidate exists only when the checkpoint decodes through the ordinary
  fail-closed decoder (`validateProject` decodes and throws the result away —
  it applies nothing), and it is offered as a question with two answers. A
  candidate that does not decode is **quarantined, not retried**: a corrupt
  recovery left in place would ask the same broken question on every launch
  forever. A successful Recover starts a fresh session history exactly as Open
  does; a failed one changes nothing at all.
- **One GLB parser, two destinations, and only one of them is a feature.** The
  reader (`GLB-IMPORT-R0`/`R1`, `ARCH-OWNER-08`/`09`) shares nothing with the
  writer, accepts a bounded STATIC subset — a node `matrix` or TRS, several
  TRIANGLES primitives per mesh including ones sharing a POSITION accessor, a
  missing NORMAL that is generated, colour and UV attributes that are validated
  and then ignored, and a material's `doubleSided` — and **fails closed by
  name** on everything else: a required extension, a sparse accessor, an
  interleaved view, an external buffer, animation, skinning, morph targets, a
  non-triangle mode, a node with children, a node stating both a `matrix` and a
  TRS, and any attribute it has not been told it may ignore. It bakes the node
  transform and produces GEOMETRY; it decides nothing about the project, and no
  coordinate conversion of any kind exists. OBJ and FBX stay absent in both
  directions.
- **`IMPORT-01A` (`ARCH-OWNER-10`) is durable import, and an Imported Mesh is a
  body's second representation.** A `SceneObject` owns a Construction Source or
  an Imported Mesh, never both and never neither, for its whole life. A
  Construction Source's geometry is DERIVED and regenerated on load; an Imported
  Mesh IS the geometry, so it is project truth and it is serialized. Nothing
  invents a primitive an imported object was never made from, and no
  Construction Body becomes one — `constructionOrNull()` and
  `activeConstructionOrNull()` are pointers so every call site has to say what
  it does about a body with none. One supported top-level mesh node is ONE
  object; a mesh's several primitives stay INSIDE it as submesh batches and
  never become rows. The node transform is SPLIT — its linear part baked into
  local geometry, its translation becoming the body's placement — so an imported
  body starts at rotation `0,0,0` and scale `1,1,1` with nothing recentred. The
  commit is ATOMIC and is ONE transaction: every object is built and validated
  off the scene, a refusal mints no `ObjectId` and records nothing, and an
  import of forty objects is one Undo. `Shape` is withdrawn for an imported body
  AND refused below JNI, because nothing may invent the primitive it was never
  made from. `Start Sculpting` is NOT: `IMPORT-01B` (`ARCH-OWNER-11`) gives an
  imported body the ordinary reversible Sculpt workflow, seeded from its own
  geometry, with `Back to Imported Mesh` and `Reset Sculpt from Imported Mesh…`
  as the only differences, and the imported arrays immutable throughout.
  Materials, textures, colours and UVs were never decoded, so nothing preserves
  them and no document may claim otherwise; `doubleSided` is the one exception
  and is carried per submesh.
- **The Imported Mesh Preview is a diagnostic, and diagnostics do not become
  features.** It still exists and is still **session-only**: no `ObjectId` from
  the scene's allocator, no entry in `ConstructionScene`, no Construction
  Source, no Frozen Sculpt Mesh, no `MeshStore` publish, never a history step,
  never a `.forge` byte, never a checkpoint, never re-exported, never selectable
  or editable, and gone with the process. It REPLACES what the renderer is
  handed for a frame and never merges with the project snapshot, because a mixed
  list is a list somebody would eventually pick, save or export from.
  `previewRenderKeyIsReserved` marks the renderer keys, which are resource keys
  and never identities. Since `IMPORT-01A` it has **no user-facing control at
  all** — the one visible GLB route is the durable import — and it is reached
  only from the verification suites. Two visible ways to open a `.glb` that did
  different things to the project is exactly the confusion that removal
  prevents.
- **A `Uri` never reaches the domain.** Scoped Storage transfer is
  ForgeShape's own project moving through the system's document UI, and
  `ProjectTransfer` is the whole boundary: it turns a `Uri` into bytes and
  bytes into a document. No `Uri`, `ContentResolver`, authority or filesystem
  path may reach JNI, the codec or the document, and none of them is ever
  stored as project truth. Opening a file makes that project live; it does
  **not** write the internal manual slot, which stays what the user saved until
  they save again.
- **GPU resources are not project truth, and a lost device says so.** The
  scene, every published mesh and every Frozen Sculpt Mesh live in CPU domain
  code a lost device cannot reach, so losing one costs the GPU's copy of
  derived data and nothing else. The policy lives in
  `forgeshape_render_recovery.h`, deliberately free of Vulkan so it can be
  self-tested without a GPU: a bounded number of device rebuilds, then an
  explicit `RestartRequired` that checkpoints the project and says a restart is
  needed rather than presenting a black viewport or drawing through a corrupt
  device. **Never provoke a real device loss on the authoritative emulator** —
  the debug injection seam is the only supported way to exercise it.
- **Diagnostics are local, bounded and carry no model.** A capped ring of
  tokens — never geometry, a vertex, a dimension, `.forge` bytes, a path or a
  `Uri` — rendered into a bounded report the user may write to a file they
  pick. ForgeShape holds no network permission and sends nothing anywhere; that
  is a structural fact, not a policy.
- **A viewport handle has one owner, one scale and no transform of its own.**
  Direct manipulation writes the active body's authoritative
  `ConstructionTransform` throughout the drag, so the renderer, the picker and
  the exact-value editors never disagree; no second solver, pivot or placement
  may exist above JNI. The pivot is the body's Construction placement origin —
  never a mesh AABB centre, a screen centroid or a camera-facing proxy. Drawing
  and hit-testing share ONE camera-derived scale, so what is seen and what can
  be grabbed cannot differ. One drag is one captured `pointerId` and one
  transaction: a second pointer or a cancel restores the pre-drag state and
  records nothing, and a gesture that starts off a handle still navigates
  exactly as before. A degenerate viewpoint holds the last good value rather
  than guessing — no NaN, no jump. The camera alone sizes the instrument: the
  BODY's scale never reaches it, and a drag's basis is frozen at the pointer
  down and is scale-free.
- **A rotation is composed as a matrix and stored as Euler degrees.** A handle
  drag may never add its angle to one Euler component — that is only correct
  when the other two are zero. Every sample composes from the IMMUTABLE start
  orientation (`Relem·R_start` for World, `R_start·Relem` for Local) and comes
  back through the one branch-continuous decomposition in
  `forgeshape_transform.h`, which is the ONLY bridge between the two forms. A
  correct rotation on a mixed orientation legitimately moves more than one Euler
  field, so a test asserts an ORIENTATION and never "one ring, one field".
- **Scale is a transform multiplier, never a dimension.** It is unitless, has no
  display unit, is strictly positive (zero is singular and negative is a Mirror
  this product does not have — both are refused, never clamped), and it is
  LOCAL-only, because a world-axis scale of a turned body is a shear no diagonal
  `S` can express. `Model = T·Rz·Ry·Rx·S`; it writes no primitive parameter and
  publishes no `MeshRevision`. Anything consuming the transform must be
  non-uniform-scale correct: normals ride `R·S⁻¹`, and picking's local ray
  direction stays un-normalized so its parameter is still world distance.
- **Shading is presentation, never truth.** Normals, the derived render mesh and
  every display setting are one-way products of a published `RuntimeMesh`.
  Nothing may read a dimension, a Construction parameter, a sculpt deformation,
  picking topology or an `ObjectId` back out of them, and no display change may
  mint a revision. Render-only vertex duplication for hard edges is expected, so
  render and source counts differ and a diagnostic must say which it means.
- **The Vulkan viewport is full-bleed; only chrome is inset.** The `SurfaceView`
  gets the whole window, and no layout decision, window inset or IME may inset,
  pad or resize it — that would change the render target and rebuild the
  swapchain for a problem about where buttons are drawn. Insets go on the chrome
  containers.
- **No surface owns the resting workspace.** The viewport is the workspace; the
  exact-value panel and every context surface are opened from a control, grow out
  of it, and are absent otherwise. Nothing may reintroduce a permanently visible
  panel anchored to a window edge — a collapsed panel is still one. A surface may
  stand on the *model*; it may never partially cover another live control, and
  moving it in front by z-order is not a fix — the control is still under it and
  still taking touches.
- **Chrome reports, it does not instruct.** The status line carries verdicts and
  standing faults only; a message that is always true is a permanent surface
  whatever its timeout says, and the rail entry, the control that opened a panel
  and the panel's own title already answer where the user is. And **critical
  navigation is never abbreviated where the row can carry it**: a transition's
  width is arithmetic on its row, not a constant, and the only approved second
  forms are `← Construction` and `← Imported Mesh` — the way OUT of Sculpt, in
  its two destinations — with the full wording kept as the content description in
  both. A third abbreviated label needs its own approval.
- **Nothing unimplemented is drawn as a tool or as a creation action.** There
  is no longer an exception: `Export` was the last reserved control and it now
  writes a real GLB. A control that looks like it works and does not is worse
  than an absent one, so a future reserved control needs its own approval.
- **Export is one-way, one file, and a different act from Save.** A `.forge`
  document is the project and ForgeShape reads it back as one; a `.glb` is a
  view of the geometry for another tool and is never a project. OBJ, FBX and
  every other interchange format stay absent, and no exporter may write a second
  file beside the `.glb` — no `.bin`, no `.gltf`, no texture, no sidecar. The
  exporter is platform-neutral (`forgeshape_gltf_export.h`): it never sees a
  `Uri`, a `ContentResolver` or a path; it re-evaluates a Construction Source,
  reads a Frozen Sculpt Mesh or reads an Imported Mesh's own CPU arrays, never a
  GPU buffer and never a decoded `.forge`; and it exports EVERY body, because a
  file quietly missing an object the user can see and move is worse than no
  file. Exporting is a READ — no revision, no history step, no `ObjectId`, no
  sculpt vertex and neither `.forge` slot may move because of it. And there is
  no conversion: ForgeShape and glTF are both right-handed, +Y-up and metric,
  so a conversion node or a global scale factor in an export is a defect.
- **A static export bakes `R` and `S` into the geometry and leaves `T` on the
  node** (`ARCH-OWNER-07`). The placement is SPLIT — `Model = T · L` with
  `L = Rz·Ry·Rx·S` — every position exported as `L·p` and every normal as
  `normalize(transpose(inverse(L))·n)`, never `L·n`, which is only the same
  answer while the scale is uniform. The node writes a `translation` and
  nothing else: no `rotation`, no `scale`, no `matrix`. The bake is about the
  body's LOCAL ORIGIN and never a bounds centre, because that origin is the
  pivot every downstream tool then inherits; each body bakes its own `L`, and
  nothing is merged. `det(L) = sx·sy·sz > 0` in this product's domain, so
  winding survives untouched — a zero or negative determinant is REFUSED
  (`SingularTransform` / `MirroredTransform`), never compensated by reversing
  triangles, which would be implementing the Mirror the domain says cannot
  exist. `.forge` still stores the nine authored values: baking is an
  interchange decision and lives only in `forgeshape_gltf_export.cpp`.
- **Every UI control has a stable semantic id, and verification uses it.** Ids
  live in `res/values/ids.xml` and name what a control *does*. No test and no
  evidence script may locate a control by screen coordinate: the workspace
  re-arranges itself per window, so a coordinate is only ever true for one run.
  An id names the act the code performs, not the label, so copy never renames one.
- **48 dp is the interactive floor for user-operated chrome, and it is the HIT
  AREA.** Glyphs stay the size they read at; the floor is reached with padding,
  never by growing a drawn box.
- **Delete removes a real project object, and it is one transaction**
  (`UI-OWNER-45`). `deleteSceneBody` in `forgeshape_body_delete.{h,cpp}` is the
  one implementation, and it is REPRESENTATION-NEUTRAL: it never asks what a body
  is, because a body leaves as a whole object — identity, representation,
  placement, published mesh and sculpt state together — and a per-representation
  delete path is how one of them would come back missing something. One Delete is
  exactly one Undo; Undo restores the SAME object and Redo removes it again. The
  deleted body is not RENDERED, PICKED, SAVED, checkpointed or EXPORTED, and no
  orphan `CONS`, `IMPT` or `SCUL` record may survive for one. Selection falls to
  the next body in scene order, or the previous when the deleted one was last;
  deleting an inactive body moves nothing. **This product has no empty project**,
  so deleting the last body is refused by name (`RefusedLastBody`) and NEVER
  answered by inventing a replacement primitive; Delete is refused while
  sculpting too, on the same terms body switching and Undo/Redo already are.
  Rename, visibility, lock, duplicate and grouping stay out.
- **A control that cannot succeed is not drawn.** Where the domain refuses an act
  in some state — creation or Delete while sculpting, Delete of the last body,
  `Shape` on an Imported Mesh — the control is absent there rather than shown and
  then refused. The domain guard stays: removing a control is not removing a
  guard.
- **A control's corner is concentric with its host's** (`inner = outer − gap`),
  or a crescent of the host shows at each end and reads as a rendering fault.
- **The domain is platform-neutral; the Android layer is an adapter.** Android is
  the first and only production platform, and no Apple target, Metal backend,
  MoltenVK dependency, Xcode project or cross-platform UI framework is
  authorized. But Construction, geometry, sculpt, picking, camera and selection
  code stays platform-neutral C++: no `View`, `Activity`, `MotionEvent`,
  `Surface`, `jobject` or other Android/JNI type may become domain truth, they
  stop at `forgeshape_jni.cpp`, input crosses the boundary as platform-neutral
  semantic samples (`forgeshape_input.h`), and the renderer's platform-surface
  coupling stays one explicit seam. Do not build a portability abstraction for a
  platform that has no target — keep the seams where they are.

## Naming and comments

- **Owner-facing documentation** uses plain descriptive names first, with any
  shorthand in parentheses after it. A reader must never need project-history
  jargon.
- **Production code** uses responsibility-based names that say what the type or
  function owns or does. No stage numbers in production names. Avoid `Manager`,
  `Handler`, `Layer`, `Thing` and `Generic` unless the full name makes the
  responsibility genuinely clear. One domain concept has one canonical term.
- **The UI vocabulary is fixed:** *Editor Workspace* (the whole editor UI),
  *Global Toolbar* (mode-independent top/global controls), *Tool Rail* (the edge
  tool selector), *Objects capsule* (the resting scene control: the active body's
  name plus creation), *Add Primitive* (the six-shape creation surface),
  *precision surface* (the on-demand exact-value panel, implemented by
  *Property Inspector*), *anchored surface* (any panel that grows out of the
  control that opened it; `AnchoredSurfaceView` owns the growth for all of them),
  *Construction Body* (an editable CAD-like object), *Imported Mesh* (a body
  whose geometry came from a file and has no parameters behind it),
  *Frozen Sculpt Mesh* (the
  polygon mesh `SculptMesh::freezeFrom` creates, from EITHER source), *history
  capsule* (the bottom trailing capsule holding Undo and Redo), *transform mode selector* (Move /
  Rotate / Scale) and *coordinate-space selector* (World / Local, where it
  applies — Scale omits it, because a world-axis scale of a turned body is a
  shear). Both are contextual **vertical groups inside the single right
  contextual surface** (`WorkspaceTrailingHostView`), under the high-level Tool
  Rail entries, contextual to Transform and absent everywhere else. They are
  members of that one host: never detached capsules, and never a separate
  horizontal selector grammar.
- **The user never reads "Freeze".** *Freeze*, *re-Freeze* and *Frozen Sculpt
  Mesh* stay in the C++, the view ids and the architecture docs, because they name
  what the operation does. Every user-facing string says **Start Sculpting**,
  **Reset Sculpt from Shape…** and **sculpt mesh** instead; **Back to
  Construction** and **Resume Sculpt** are unchanged. Two readers, two
  vocabularies, and neither may be renamed into the other. `UIR4B-15` enforces
  the user-facing half over every `R.string`. Over an Imported Mesh the same two
  acts read **Back to Imported Mesh** (short form `← Imported Mesh`, full wording
  always the content description) and **Reset Sculpt from Imported Mesh…**,
  because Construction wording over a body with no Construction Source names a
  place that does not exist. The view id stays `back_to_construction`: an id names
  the ACT the code performs, and copy never renames one.
- **Comments explain why**, plus ownership, units, lifecycle and constraints —
  never obvious syntax. Worth a comment: why the Android UI must not become
  geometry truth, why a transform-only edit publishes no `MeshRevision`, why a
  pointer gesture is consumed before JNI, why a platform-neutral input boundary
  exists.
- Do not mass-rename working code to satisfy this. Rename when already touching
  the area, or when an ambiguity is a real maintenance risk.

## Documentation ownership

One durable fact has exactly one primary owner.

| File | Owns |
| --- | --- |
| `PROJECT_STATUS.md` | current status, verified capability, environment facts, next stage |
| `ARCHITECTURE.md` | current production architecture and module ownership |
| `PRODUCT.md` | runtime-verified user-visible behaviour only |
| `README.md` | required tooling, build/run/verify instructions |
| `CLAUDE.md` | these rules |
| `DATA_PACKAGE_SPEC.md` | the `.forge` binary layout, codes, validation, determinism, fixtures |

Never document a feature as implemented unless it is runtime-verified. Unverified
paths are reported as UNVERIFIED, not as behaviour.

**Keep documentation navigable and current.** There is no raw line-count cap. A
document is too long when it is hard to navigate or carries text that is no
longer true — never merely because of its physical line count. When adding a
fact, replace the superseded or duplicated prose rather than appending a new
chapter beside it, and delete migration rationale once the invariant it explains
stands on its own. Use Git history for old stage detail; do not create a shadow
history Markdown file. Do not compact by deleting ownership or invariant detail
the code depends on: that is a regression, not a saving.

## Android / Vulkan safety

- The emulator dies if launched as a child of a tool shell. Spawn it detached
  (e.g. WMI `Win32_Process.Create`).
- When more than one Android target is connected, every `adb` command must use
  an explicit `adb -s <serial> ...`. Never rely on the default target.
- `AVD Medium_Phone_API_36.1` / `emulator-5554` is **reserved by another program**
  and must not be used, started, stopped, modified, installed to, logged,
  screenshotted or sent input by ForgeShape work, until the owner lifts this.
- `AVD ForgeShape_Stage004` / `emulator-5556` is **contended**: another program
  runs on it, steals the foreground and injects input. Do not use it for
  authoritative runtime evidence, and do not stop, wipe or reconfigure it.
- Runtime evidence is taken on a ForgeShape-owned isolated AVD (currently
  `ForgeShape_Stage006`, port varies — see below). Before any evidence-sensitive
  input or screenshot, confirm ForgeShape is the resumed activity, and
  invalidate any run contaminated by foreign input. Creating a new AVD from
  already-installed tooling is allowed; installing or updating SDK/NDK/JDK/
  system images is not.
- **Never trust a port or serial number to say which AVD is behind it.** The
  emulator assigns the first free port starting at `5554` to whichever
  instance boots first, so `ForgeShape_Stage006` can itself land on
  `emulator-5554` by pure allocation order — confirmed directly during Stage
  016-R. Always confirm with `adb -s <serial> emu avd name` before treating a
  serial as safe. Boot it only with `scripts\start-forgeshape-emulator.ps1`
  (defaults `-Avd ForgeShape_Stage006 -Port 5580`): it hard-rejects port
  `5554` before touching the OS or adb at all, checks occupancy of ONLY the
  requested port, BLOCKS with no automatic fallback port if that port is
  already taken, launches detached via WMI/CIM `Win32_Process.Create`, and
  confirms AVD identity with `emu avd name` before reporting ready. An
  unavailable port is BLOCKED, never a reason to try another one.
- **Bare, unscoped `connected*AndroidTest` is forbidden whenever more than one
  Android target could be attached.** That Gradle task enumerates every
  attached device with no default and installs/runs on all of them — this is
  exactly how Stage 016 touched the reserved `emulator-5554`. The one
  supported runner is `scripts\run-instrumented-tests.ps1 -Serial <serial>`,
  which requires an explicit serial, refuses `emulator-5554` before any device
  is contacted, checks readiness with `adb -s <serial> get-state` (never a
  bare `adb devices` enumeration), and drives `adb -s <serial>` explicitly for
  every install and instrumentation step — see `README.md`. No repo script
  under `scripts\` issues a bare, unscoped `adb` call;
  `scripts\verify-device-guards.ps1` checks this and the port/serial guards
  above mechanically (`DEV2-01`..`07`) and needs no device attached.
- Instrumented full-suite policy: runner execution with no filter is the
  supported monolithic full-suite path; `-FullSharded` is the supported
  authoritative exhaustive-sharded full-suite path. `-FullSharded` must use
  live AndroidJUnitRunner discovery, prove a deterministic exactly-once union,
  require a valid successful result from every shard, and emit
  `FULL_SHARDED_SUITE_PASS` only for the complete aggregate. `-TestClass` is
  focused/subset evidence and can never emit that marker. After any
  infrastructure crash/abort during `-FullSharded`, recover the isolated AVD
  and rerun the whole `-FullSharded` command from shard 1; class or shard-only
  reruns are supplementary and cannot repair an aggregate.
- `surfaceDestroyed` must block until native code has released the
  `ANativeWindow`. Never let the render thread touch a destroyed window.
- **One orientation convention: render in Android window orientation.** The
  swapchain requests an identity `preTransform` and takes its extent from the
  window, so window size, camera viewport, projection aspect, swapchain image and
  picking all share one coordinate space. Do not pre-rotate, do not transpose an
  extent, and do not add a display rotation to a matrix. The convention makes
  `VK_SUBOPTIMAL_KHR` the expected steady state on a rotated display, so the
  frame loop must **not** rebuild the swapchain on it — rebuild on
  `VK_ERROR_OUT_OF_DATE_KHR` and on the explicit resize request only. Ignoring
  this rebuilds the swapchain every frame while the device is rotated.
- Shaders are compiled ahead of time by the NDK `glslc` in CMake. No shader
  compiler ships at runtime.
- **A Vulkan validation layer is never bundled into the APK or committed to
  the repo.** When debugging needs it, fetch the official Khronos build
  (`github.com/KhronosGroup/Vulkan-ValidationLayers` releases) into a
  scratch/temp location, `adb push` it and enable it through Android's
  first-party per-app GPU debug layer settings
  (`adb shell settings put global enable_gpu_debug_layers 1` /
  `gpu_debug_app` / `gpu_debug_layers` / `gpu_debug_layer_app`), then delete
  those settings afterward. Never add it under `jniLibs`, never make it a
  Gradle dependency: it must stay a session-scoped, adb-only debug tool with
  zero footprint in the built product.
