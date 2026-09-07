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

A clean debug launch emits **twenty-two** `*_SELFTEST_OK` tokens, then
`FORGESHAPE_NATIVE_VIEWPORT_OK`. All twenty-two, in emission order:

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
FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK
FORGESHAPE_MIRROR_SELFTEST_OK
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
Construction-history, gizmo, project-format, render-recovery, CAD and mirror
self-tests are debug-only and run once from `NativeViewport.start()`. They must
never run per frame. Each builds the domain objects it needs — the scene,
history, gizmo, CAD and mirror suites build their own `ConstructionScene` and
their own camera — rather than reading process-scoped state, so a suite's
result never depends on what a live session left behind. The project suite
also prints
`FORGESHAPE_PROJECT_GOLDEN_SHA256`, `..._IMPORTED`, `..._IMPORTED_SCULPT`,
`..._CAD` and `..._CAD_V2` — the digests of the canonical `.forge` fixtures as
this build encodes them — so drift from the committed corpus is a value that
can be read rather than only an assertion that failed. The CAD suite prints
`FORGESHAPE_CAD_PERFORMANCE`, the bounded extraction, triangulation and
regeneration timings for its four sizes, and the sketch-UX suite prints
`FORGESHAPE_SKETCH_UX_PERFORMANCE`, the arc and spline tessellation and
curve-profile timings.

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
  data, until a stage pays for making them data. There are **SEVEN** tools
  (`SCULPT-FCM-R1`): Grab, Clay, Smooth, Flatten, Inflate, Crease and Mask, in
  the rail's reading order; the enum APPENDED the last three so the four that
  crossed JNI as an index before keep theirs, and the rail's order is a separate
  decision from the enum's. **Six write a POSITION and Mask writes a WEIGHT** —
  `sculptToolMovesGeometry` is the one place that division is stated. Flatten
  and Crease measure in the Stage 020R3 world/display metric like every other
  brush: Flatten fits ONE plane at pointer-down from the footprint's weighted
  centroid and weighted normal (no camera, no zoom, no viewport in it) and then
  INTERPOLATES toward it — `d' = d·(1-λ)`, `λ ≤ kMaxFlattenLambda` — so `|d|`
  can neither grow nor change sign and repeated passes converge; Crease is one
  displacement with TWO components from the shared `amount`, inward along the
  captured normal (`kCreaseInwardFraction`) plus a tangential pinch toward the
  brush centre (`kCreasePinchFraction`), both held in one place because their
  RATIO is what makes the groove narrow. A degenerate fit, a degenerate normal
  or a degenerate tangent moves nothing rather than inventing a direction.
- **The Sculpt Mask is runtime-local editing state, never project truth**
  (`SCULPT-FCM-R1`). A per-vertex weight in `[0, 1]`, default 0, living in the
  Frozen Sculpt Mesh's own vertex records (`MeshVertex::mask`) — a DERIVED
  PRESENTATION CHANNEL on exactly the terms the colour beside it is, so no
  publication signature between `SculptMesh` and the vertex buffer had to learn
  about masking. It scales every geometry brush's displacement by `1 - w` with
  **exact ends** (`sculptMaskFactor`: 0 is the full effect, 1 is exactly zero,
  by an equality and not by arithmetic that lands there), captured at
  pointer-down with the affected set and held fixed for the stroke. The Mask
  brush is deliberately NOT held off by the mask it paints. **It is not
  geometry**: a mask write mints no `SculptRevision`, sets no `hasEdits`, moves
  no project fingerprint, earns no checkpoint and reaches no `.forge` byte — so
  **no format, section, version or corpus fixture changed** and reopening a
  project restores the geometry with an EMPTY mask, exactly as it restores an
  empty history. It survives Back to Construction and Resume Sculpt because it
  lives on the body; a Freeze or a destructive Reset clears it, on the same
  boundary the history cannot cross. It is per body by OWNERSHIP, so one body's
  mask is structurally incapable of reaching another's. **Mask paint and Clear
  Mask each land in the ONE `SculptHistory`** as one entry: a delta gained a
  second SIDE (`maskIndices`/`beforeMask`/`afterMask`, 12 bytes per touched
  vertex), never a second stack, and an entry may carry either side or both —
  `applyHistorySide` moves the revision and the edited flag only for one that
  carries geometry. The caps are exactly the numbers they were (32 entries,
  4 MiB per body, 1 MiB per entry) and a navigator jump across mask entries is
  still repeated Undo/Redo. **Clear Mask REFUSES an entry over the per-entry
  cap** (`EntryTooLarge`) rather than applying it — deliberately not the
  brush's `NotRetained` policy, because a stroke's deformation is what the user
  is doing while Clear Mask's whole value is that it can be taken back. The
  viewport feedback is one fragment-stage mix toward a cool low value, driven
  by a fourth vertex attribute (`RenderVertex::mask`, a single float clamped on
  the CPU) — no second pass, no second pipeline, no push-constant slot. **Not
  this stage:** mask Invert/Grow/Shrink/Blur, mask by topology, mask
  persistence, vertex-colour or texture painting, symmetry, pressure, and any
  topology mutation.
- **A gesture that becomes multi-touch navigation must never mutate the sculpt
  mesh.** No vertex written, no `SculptRevision` minted, no stroke committed.
- **A CAD Body's truth is its sketch and its extrusion, never its mesh**
  (`CAD-R0-A1A2`). A `SceneObject` owns exactly one of THREE representations
  for its whole life: a Construction Source, an Imported Mesh, or a CAD Body —
  one sketch on a principal workplane (XY, XZ or YZ; `forgeshape_workplane.h`
  owns the one right-handed mapping) and one linear **New Body** extrusion of
  one of its closed profiles. Everything else — the closed profiles, the
  polygon, the triangles, the extruded mesh — is DERIVED and regenerated
  through `generateCadMesh`, the ONE path, and no rule anywhere reads a sketch
  parameter back out of a vertex. `CadBody::applyState` validates and
  regenerates the WHOLE requested state and writes nothing unless all of it
  passes, so an edit that would leave no closed profile is refused by name and
  the last valid state stands: there is no half-regenerated body. The profile
  engine (`extractClosedProfiles`) **fails closed by name** — open, forked,
  crossing, zero-area, duplicate-edge and nested loops are refused, never
  repaired — and a nested profile is refused as a hole this stage does not
  fill. There is no boolean, no fillet, no chamfer, no shell, no revolve, no
  taper and no constraint solver; a rectangle is parametric (centre, width,
  height, axis-aligned in sketch space) and a circle is centre and radius, and
  both are editable later. R0 had no arc and no spline either; `SKETCH-UX-R1`
  adds both as AUTHORED-point entities — see the curve rule below — and they
  change none of the rest of this. The
  **sketch edit session is volatile**: nothing before its one-transaction
  commit is project truth, `cancel` costs the project nothing, a half-drawn
  sketch never reaches a `.forge` byte, a checkpoint or the fingerprint, and
  process death loses the sketch and nothing else. Creating a body from a
  sketch is ONE `ScopedConstructionEdit` around ONE `addCadBody` (one Undo),
  every later sketch-size or depth edit is ONE step, and a refused commit mints
  no `ObjectId`. CAD uses the Construction history, never `SculptHistory`.
  **CAD → Sculpt is deliberately NOT this stage**: `buildSculptSourceMesh`
  returns false for a CAD Body, the freeze refuses by name
  (`CadBodyNotSculptable`), the control is absent for one, and a `SCUL` entry
  over a `CADB` body is refused by the codec. While a sketch is being DRAWN the
  single-finger gesture belongs to the sketch and never orbits — an orbit would
  take the aligned view away under the finger placing a point — two fingers
  still pan and pinch, and creation, deletion, body switching, freeze and
  Construction Undo/Redo are refused below JNI and withdrawn above it. In
  `Ready`, after Finish Sketch, a single finger that misses the extrude arrow
  NAVIGATES like anywhere else in the product (`CAD-UX-S1-C1`): the drawing is
  done, there is no aligned view left to protect, and the staged extrusion is
  looked at rather than drawn on.
- **A CAD sketch may be supported by a world plane OR a planar CAD face, and a
  face's identity is SEMANTIC** (`CAD-A3`, `ARCH-OWNER-13`). An extrusion
  exposes a bounded set of planar faces — two caps and one side per profile
  edge — each with a stable `CadFaceToken` (kind plus the profile-edge entity
  it came from) and a deterministic frame in body-local space
  (`forgeshape_cad_face.h`). A face-supported sketch stores a `TopoRef`
  (producer `ObjectId`, feature id, face token, lineage signature); the render
  triangle index is NEVER persisted and there is NO nearest-face retargeting. A
  reference resolves only when the producer exists and is a CAD body, its
  topology signature still equals the stored lineage token, and the named face
  resolves and is eligible — a stale lineage, a missing producer, a bad token, a
  curved circle side or a cycle all **fail closed**. A face-supported body's
  world placement is DERIVED (`ConstructionScene::resolveWorldModel` composes
  `producerWorldModel · faceFrame` every snapshot), so moving, rotating or
  editing the producer carries the dependent with it and no follow-state is
  stored; such a body is NOT independently transformable
  (`SceneObject::isFaceSupportedCad`). Deleting a producer with dependents is
  **refused** (`RefusedHasDependents`), never cascaded. The dependency graph is
  acyclic and bounded by the body count. **CADB gains version 2** carrying the
  support: a world-only CAD project stays v1 and byte-identical, an older build
  refuses v2 as a required section at an unknown version, and load validates the
  whole dependency graph before applying anything. The sketch camera frames
  EXACTLY along the sketch frame's normal (`frameSketchView`, no pitch-clamp
  approximation, no gimbal), and the sketch grid is view-adaptive (nice
  1/2/5·10^k, never persisted, a typed value never re-snapped). **The lineage
  token is a FORMAT field**: FNV-1a 64 (basis `0xCBF29CE484222325`, prime
  `0x100000001B3`) over the profile anchor id, the face count and each face's
  token code and eligibility, exactly as `DATA_PACKAGE_SPEC.md` §7c states,
  and `scripts/build-forge-corpus.ps1` reimplements it from that text.
  **CADB gains version 3** for the curve entities (`SKETCH-UX-R1`), written
  only when a body's sketch carries an Arc or a Spline, and always carrying the
  v2 support block because a version is a superset of the one below it; a
  curveless project keeps v1 or v2 byte-identical, and a curve code inside a
  section that declared itself v1 or v2 is refused as malformed rather than
  read. **Not this stage**: CAD → Sculpt, custom construction planes,
  curved-face sketches, projected edges, a constraint solver, and every
  boolean/fillet/chamfer.
- **Home is not a project, and a project is never empty** (`APP-H1`,
  `CAD-A3-C1`). The process starts with the scene EMPTY
  (`ConstructionScene(NoProjectTag)`), and `hasProject()` — at least one body —
  is the ONE answer to "is a project open": no Java flag mirrors it, and
  `EditorWorkspaceView::refreshShellPhase` derives Home, the New Project
  chooser, the unsaved-changes question or the editor from it on every
  refresh. Behind Home nothing is drawn, picked, saved, checkpointed or
  fingerprinted, and nothing fabricates a default primitive or an invisible
  placeholder body. The four process-scoped accessors answer for "no project"
  without reading `activeBody()` (null, an unbound store, an unbound identity
  placement, an unbound sculpt target); the few JNI entry points that read it
  directly ask `hasProject()` first and refuse by name; `activeBody()` on an
  empty scene answers a counted null object rather than dereferencing an empty
  list, and `HomeFlowTest` asserts the count never moves. **Home and New
  Project are full-screen PAGES, not cards on a scrim** (`SKETCH-UX-R1`):
  `StartPageView` owns both, each is opaque and owns the whole window, New
  Project REPLACES Home rather than standing on it, insets go on the page's
  CONTENT because the page is the window's own ground, and the decorative motif
  is a static vector that is never interactive and never an editor viewport.
  The two questions asked OVER a live project — unsaved changes and recovery —
  stay `ChooserSurfaceView` modals, because there a scrim is the right shape.
  **New Project offers exactly CAD and Sculpt.** CAD is the transient
  bootstrap: ONE volatile sketch session over the empty scene, opened DIRECTLY
  on XY seen along +Z with no plane-chooser step before it, owning no
  `ObjectId` and no `SceneObject`; the first Extrude creates the project
  through `commitFirstCadProject` — a one-body document replacing the scene
  through the SAME all-or-nothing `loadProjectDocument` path Open takes, so the
  new project starts with an EMPTY history like every loaded one, a refusal
  creates nothing, and Back to Home before it costs nothing (one step, because
  the first sketch IS the bootstrap). Sculpt is the seeded sphere inside the
  session-initialization bracket; a refusal closes the project again. Leaving a
  dirty project (New Project…, Open Saved Project, Open File…) asks Save /
  Discard / Cancel by FINGERPRINT against the last Save/Open/Recover; Save
  continues only if the slot was written, Discard retires the checkpoint,
  Cancel and System Back change nothing. `closeProject` writes nothing. Back is
  one step in every phase and the platform's own at Home. **A LATER New Sketch
  still lands directly in the spatial chooser** (`UI-OWNER-46`) — there are
  real bodies with real faces to pick by then, which is what the empty world of
  a first sketch did not have; the by-name plane list stays as
  `sketch_plane_by_name`, the accessibility fallback, never removed. A stylus
  hover highlights a chooser target and never commits; hardware hover is not
  claimed on the emulator.
- **The sketch's plane and its VIEW are two different things**
  (`SKETCH-UX-R1` C). The orientation navigator in the sketch's upper trailing
  corner owns both, holds neither, and reads `sketchViewState` on every
  refresh. `viewFlipped` and `viewQuarterTurns` are PRESENTATION in the
  strongest sense the project has: never persisted, never in a history step, a
  checkpoint or the fingerprint, and incapable of moving one authored
  coordinate — `SketchSession::viewFrame` derives the camera's frame from the
  authoring frame and is ALWAYS right-handed, so no view state can mirror a
  sketch. The support plane may change only while the sketch is EMPTY and
  world-supported; afterwards it is refused by name (`SketchNotEmpty`) and the
  authored `(u, v)` are never silently reinterpreted on another plane, and a
  FACE-supported sketch refuses a world plane outright (`InvalidWorkplane`)
  rather than detaching from the TopoRef it follows. The navigator may stand on
  the drawing; it may never stand on the Tool Rail.
- **A selected straight Line carries a technical dimension, and it is an
  INTERACTION overlay** (`SKETCH-UX-R1` E). Extension lines, a dimension line
  and its end ticks are their own `SketchOverlayStyle::Dimension` range; the
  numeric label is chrome, positioned from the anchor native reports. Neither
  is geometry: neither is exported to GLB, reaches a `.forge` byte, mints a
  revision or appears in a snapshot. `applyLineLength` keeps `P0` FIXED and the
  direction UNCHANGED (`P1' = P0 + normalize(P1 - P0) * length`) — no solver,
  no neighbour moved, nothing re-snapped. Zero, negative, non-finite and
  out-of-range are refused, never clamped. If the edit opens a chain, `finish`
  refuses `OpenProfile` by name; the sketch is never repaired around the user.
- **An Arc and a Spline are AUTHORED points, and everything else about them is
  derived** (`SKETCH-UX-R1` D). An Arc is three points ON the curve (start, a
  point it passes through, end) — never a centre, a radius and a sweep, which
  would need a direction flag and a major/minor flag kept consistent with the
  endpoints. A Spline is the bounded point list its curve INTERPOLATES. The
  centre, radius, sweep, control handles and every tessellated point are
  regenerated by `tessellateSketchCurve` from the authored values and these
  constants ALONE — never from a camera, a zoom or a window, because the
  extruded solid must not depend on how the sketch was looked at. Both chain by
  coincident AUTHORED endpoints through the ONE walker that also chains lines,
  so connectivity and triangulation stay separate and a denser tessellation can
  never open or close a profile. A curve's extruded side face is a facet of a
  curved surface and is therefore NEVER eligible to support a sketch — decided
  per polygon edge, because one profile may mix curves and exact lines. No
  tangent, radius or dimensional constraint solver exists.
- **Edit Sketch is STAGED, and Finish is one transaction**
  (`SKETCH-UX-R1` F). `SketchSession::beginEdit` takes a copy of a committed
  CAD Body's authored state; the project keeps its own until `commitEdit`, so
  Cancel costs it exactly what cancelling a new sketch costs — nothing, and no
  history step. `commitEdit` validates the whole candidate, then applies it
  inside ONE `ScopedConstructionEdit`: one Finish is one Undo, Undo restores
  the entire previous sketch and Redo the edited one, and no second body is
  ever created. A dependent's world placement is DERIVED and needs no push. An
  edit that would strip a planar face another body's sketch stands on is
  REFUSED by name (`DependentFaceLost`) on exactly the terms deleting such a
  producer is: never cascaded, never retargeted, never silently broken.
- **The extrusion is controlled AT THE GEOMETRY, and the tool holds no truth of
  its own** (`CAD-UX-S1`). `forgeshape_cad_extrude_tool.{h,cpp}` adds three
  things and deliberately not a fourth: WHERE the manipulator stands
  (`CadExtrudeAnchors` — the profile's AREA centroid on the support plane, the
  frame normal, the growth axis and the tip, derived on every read from the
  frame, the chosen profile and the extrusion, with **no camera, no viewport
  and no zoom in it**), HOW BIG it is drawn and grabbed
  (`CadExtrudeControlScale`), and WHAT A DRAG MEANS (`CadExtrudeManipulator`).
  It does **not** add a second model of the extrusion: `SketchSession` still
  owns the profile, the depth and the direction and is still the only writer,
  and a drag lands through `setExtrude` — the one door a typed value already
  used, so a dragged depth passes exactly the validation a typed one does.
  Nothing here is serialized, reaches a `.forge` byte, a checkpoint, the
  fingerprint or a history step, so **no format, section, version or corpus
  fixture changed** and a project reached by dragging the arrow encodes
  byte-identically to one reached by typing the same number.
  **The camera-attached scale is a new rule BESIDE the gizmo's, never a change
  to it**: `gizmoWorldScale` holds a constant number of PIXELS at every
  distance, which is right for a placement instrument and the opposite of a
  control that belongs to the work. The cluster is a world object of reference
  size `W` whose screen size is clamped into a band —
  `scale = clamp(W / metersPerPixel / S_ref, 0.80, 1.60)` — so it shrinks as
  the camera pulls back and saturates at both ends, and that ONE `scale` sizes
  the drawn arrowhead and the Android cluster alike. The `0.80` floor is
  ARITHMETIC: the cluster's controls are authored at 60 dp and 60 × 0.80 is
  exactly the 48 dp interactive floor, which a native case asserts so the pair
  cannot drift. The hit CORRIDOR is 24 reference units and is deliberately NOT
  scaled, for the reason the gizmo's own corridors are not scaled by its visual
  size preference. **The drag is the gizmo's contract restated**: one captured
  `pointerId`, a basis FROZEN at pointer-down so the growing preview cannot
  move the control out from under the finger, a second pointer or a Cancel
  restoring the pre-drag depth and recording nothing, and a degenerate
  viewpoint holding the last good value rather than guessing. A DRAG is clamped
  at a millimetre floor where a TYPED value is refused, and that difference is
  deliberate: a typed value is a statement the user made, and a drag has no
  moment at which the user submitted zero. **Flip is a direction change and
  never a negative depth.** The arrow is one more `SketchOverlay` producer in
  the range the extrude preview already uses, so the renderer needed no change,
  no new style and no new pipeline; its SHAFT is the depth and only its head
  takes the control scale. The exact value, Flip, the `New Body` badge and the
  retained-sketch `Edit Sketch` chip are Android chrome positioned from a
  projected native anchor — the `bodyDimensionLabelPoint` pattern a third time
  — and an anchor that does not project is HIDDEN, never placed at a guess.
  **The sketch's view and the extrusion's are TWO views of one authored truth,
  and Finish Sketch is where the second begins** (`CAD-UX-S1-C1`, closing
  `OQ-CAD-UX-01`). A sketch is AUTHORED through the exact support-normal view —
  `frameSketchView` aims along the normal and `applyOrbit` returns early while
  `sketchView_` is true, both unchanged — and that normal IS the extrusion axis,
  so from it the arrow points at the eye and no axial drag can be resolved. The
  answer is a bounded VIEW transition and deliberately not a second gesture
  model: a raw screen delta would give the drag a meaning the world axis does
  not have. `cadFeatureViewPose` is the whole policy, a pure function over
  values with two paths — the user's pre-sketch 3D view when it already sees the
  axis, re-centred on the work anchor; otherwise a deterministic OBLIQUE view
  leaning `kCadFeatureViewObliqueRadians` off the support normal in the frame's
  own `(u, v)`, so no world up enters its construction and an XZ sketch's world
  `+Y` normal needs no special case. "Usable" is one stated number,
  `kCadFeatureViewMinAxisSine` (0.35, about 20.5°), measured as the SINE between
  the view direction and the axis and chosen clear of `solveAxisParameter`'s own
  0.02 limit. Because the orbit pose CLAMPS pitch, the candidate is re-derived
  from the clamped angles and re-measured, and a failure steps the azimuth a
  quarter turn — a bounded four attempts, because at most one quadrant can aim
  the tilt up the meridian. Nothing it can work from is refused as
  `Unavailable` and NO view is installed, on the manipulator's own fail-closed
  terms. The transition keeps the sketch's projection and span, so it is a TILT
  and not a reframe; it consumes nothing, so `endSketchView` still returns the
  user's pre-sketch view on cancel and on commit; and every path back to the
  authored sketch — `backToEditing` and `Edit Sketch` — reinstalls the exact
  aligned view. **None of it is truth**: no `CadBodyState`, no `.forge` byte, no
  section, no version, no fixture, no checkpoint, no fingerprint and no history
  step moves for a camera.
  **The sketch panel holds no draft direction any more**: it shows what native
  says on every refresh, so the canvas Flip and the panel chips cannot become
  two answers. **Not this stage:** `Add`, `Cut`, any boolean, `Symmetric`,
  `Two Sides / Asymmetric A+B`, a feature list, `CADB` v4 or v5, `Revolve`,
  `Intersect`, a Hole feature, a constraint solver, and multi-feature reuse of
  one sketch — the retained-sketch access delivered here is RE-EDIT and is not
  that.
- **Preferences are application state, never project truth** (`UI-PREF-R1`,
  UI-OWNER-37, UI-OWNER-32, UI-OWNER-42). `AppPreferences` is ONE versioned,
  immutable value — palette, handedness, gizmo visual scale, gizmo stroke
  weight — persisted by `AppPreferencesStore` in the app's own
  `SharedPreferences` file and read BEFORE the Activity applies its theme, so
  the first frame is already the stored palette. It enters no `.forge` byte,
  moves no fingerprint, dirties no project, records no Construction or Sculpt
  step, mints no `ObjectId` and never reaches a checkpoint;
  `SettingsPreferencesTest` serialises a project before and after changing
  every field and asserts identical bytes. Reading is forgiving by ONE rule per
  field: an unknown name is the default, a non-finite number is the default, a
  finite out-of-range number is CLAMPED to the nearer bound, a missing key is
  the exact product default, and an unknown key is ignored — no migration
  framework. **The Settings page** (`SettingsPageView`, a start page) is the
  ONE home of every persistent preference, reached from Home and from the
  Project surface's `Settings…`; the transient viewport controls (Shading,
  Surface, Projection, Grid and, since `SEL-OUT-R1`, Selection Outline) stay in
  the Display popover and must not move there. **Exactly five palettes**: the three DARK ones are value-for-value
  what was approved, the two LIGHT ones (Warm Light, Cool Light) are derived
  through the same `attrs.xml` roles and held to the same measured targets
  (`artifacts/ui-pref-r1/CONTRAST.md`), every per-ground tool colour below JNI
  asks `viewportBackgroundIsLight` rather than naming a member, and the system
  bars flip their icon appearance with the ground. **Handedness mirrors edge
  anchoring and nothing else**: Right is the default and reproduces
  UI-LAYOUT-R2 exactly; Left seats the trailing host, a side-placed precision
  surface, the brush controls and the Objects column on the left edge with the
  same 8 dp inset, width and top, opening inward (`applyHandedness`), and no
  CAD coordinate, axis, workplane, camera, gizmo arithmetic, transform,
  exported byte or gesture meaning is mirrored. **The gizmo's visual size is a
  bounded multiplier on handle PLACEMENT** (`[0.9, 1.5]`, default 1.0,
  refused-not-clamped below JNI) applied to drawing and hit test alike through
  `gizmoPlacementScale`, while every hit corridor stays in reference units and
  every drag amount — the ray solvers, the ring angle, the scale ruler measured
  on a canonical snapshot — is unchanged; the floor is 0.9 because below it an
  obliquely viewed plane handle's centre falls inside the pivot's 24-unit dead
  disc. **The stroke weight is a closed bundle recipe** (Thin, Regular, Bold;
  Regular is byte-identical to the pre-preference gizmo, pinned at 1116
  vertices) that hit testing never reads, and the renderer re-uploads the
  canonical list only when it changes. There is NO handle-style preference:
  the gizmo is a one-pixel line list by renderer contract, and a second style
  that shared its hit semantics does not exist, so the row is ABSENT rather
  than inert (`GIZMO_STYLE_DEFERRED_BY_RENDERER_CONTRACT`).
- **A body's SOURCE is never written by sculpting.** A body has a source
  representation — a Construction Source, an Imported Mesh, or a CAD Body —
  and may also own a Frozen Sculpt Mesh (not yet for a CAD Body). A sculpt edit may never change a primitive parameter, a
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
  edited flag on both sides — plus, since `SCULPT-FCM-R1`, a second SIDE for the
  mask), never normals, never a document, never a
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
- **The History navigator is a VIEW of that branch, and a jump IS repeated
  Undo** (`SCULPT-H1`). It adds no storage, no capacity, no budget and no
  second stack: `SculptHistory::cursor()` derives
  `{cursor, undoCount, redoCount, stateCount}` from the two deques on every
  read and stores none of it. A ROW is a STATE and not an entry — `u` undo and
  `r` redo entries are `u + r + 1` states, the cursor stands at `u`, and state
  0 is the OLDEST RETAINED state rather than necessarily the Freeze, because
  eviction drops from that end and what it dropped cannot be jumped to.
  `SculptSession::jumpToHistoryCursor` is **implemented as** repeated
  `undoStroke()`/`redoStroke()` rather than described that way, so there is no
  second delta path, no batched apply and no snapshot restore; it asks the same
  three refusals a single step does, answers `NothingToDo` for the row already
  stood on (applying nothing, minting no revision), and refuses an ordinal off
  the branch by name (`OutOfRange`) — **never clamped**, because landing the
  user on a state they did not tap is worse than a refusal they can see. It
  records NOTHING: no entry, no Construction step, no `.forge` byte, no
  checkpoint of its own, and a round trip encodes byte-identically. The
  **abandoned future is not dropped by the jump** — jumping backward leaves it
  walkable exactly as an Undo does, and the EXISTING rule (`record` clears the
  redo stack) drops it when the next stroke lands. `sculptHistoryState` reads
  the whole model in ONE locked call, because five values must describe one
  body's branch at one instant, and one jump publishes ONCE rather than once
  per intermediate step. Body switching needs no rebinding code: the model is
  per body below JNI. The control is Sculpt's alone and ABSENT in Construction
  — there is no Construction branch to list — and **five visible rows is a
  VIEWPORT target, not a history bound**: the list scrolls over everything
  `SculptHistory` kept, and no row is shrunk below the 48 dp floor to fit more
  in. The current row is marked by SHAPE and by its accessible label, never by
  colour alone. **Not this stage:** a real-stylus hover preview (deferred —
  `AutosaveController.performCheckpoint` reads the fingerprint on its own
  worker thread at RUN time, so previewed geometry could reach the recovery
  checkpoint, and making that safe needs an autosave-suspend concept or a
  shadow render path), thumbnails, named states, a branching tree, a
  Construction navigator, and any keyboard shortcut.
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
  the document. **A CAD Body takes `CADB`** (`CAD-R0-A1A2`), on `IMPT`'s terms:
  always required, announced by header bit3, exclusive with `CONS` and `IMPT`
  per body, carrying the authored sketch and extrusion and never a vertex, and
  held to `validateCadBodyState` — a file whose sketch closes no profile is
  refused rather than opened. `CAD-R0-A1A2` added `cad_rectangle`,
  `cad_circle`, `mixed_cad` and `cad_bad_plane`; `CAD-A3-C1` added the six
  `CADB` v2 fixtures (`cad_face_sketch_cap`, `cad_face_sketch_side`,
  `cad_face_chain`, `mixed_cad_face`, and the two the decoder must refuse,
  `cad_bad_face_ref` and `cad_dependency_cycle`); `SKETCH-UX-R1` added the six
  **`CADB` v3** fixtures (`cad_arc_profile`, `cad_spline_profile`,
  `cad_mixed_curve_profile`, `cad_face_curve`, and the two the decoder must
  refuse, `cad_bad_arc` and `cad_bad_spline`); Stage 018A added the two
  **`SCNE` v2** fixtures (`object_state`, and the one the decoder must refuse,
  `object_state_bad_flags`) — a **thirty**-fixture corpus in which every older
  fixture is byte-for-byte unchanged. The five corrupt fixtures are CONSTRUCTED
  by the PowerShell builder with the bad value in place, never generated and
  then mutated.
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
  display unit, is strictly positive (zero is singular and a negative factor is
  a reflection the transform will not carry — both are refused, never clamped;
  `MIRROR-01` mirrors through the ORIENTATION instead), and it is
  LOCAL-only, because a world-axis scale of a turned body is a shear no diagonal
  `S` can express. `Model = T·Rz·Ry·Rx·S`; it writes no primitive parameter and
  publishes no `MeshRevision`. Anything consuming the transform must be
  non-uniform-scale correct: normals ride `R·S⁻¹`, and picking's local ray
  direction stays un-normalized so its parameter is still world distance.
- **A body DIMENSION is derived, and one solver owns every resize**
  (`UI-OWNER-33B`, Stage 020M). A Construction Body's overall size on one of
  its own axes is `unscaledLocalExtent[a] * absoluteScale[a]` — the extent read
  from the Construction Source's PARAMETERS, never measured off generated
  vertices, and never a world AABB, so rotating, moving or looking at a body
  cannot change what it measures. Nothing stores a dimension: editing one
  writes the Scale the transform already had, plus a Position correction for a
  one-sided anchor, so **no `.forge` byte, section or version changes** and a
  project reached by a dimension edit encodes byte-identically to one reached
  by typing the same numbers into the placement editor. `forgeshape_
  body_dimensions.{h,cpp}` is the ONE implementation, deliberately a pure
  function over values (no scene, no history, no body, no camera, no renderer)
  precisely so Stage 020D's Directional Scale can drive the same arithmetic:
  `T_new = T_old + (R·e_a)·(S_old[a] − S_new[a])·b`, with `b` the held face's
  local coordinate and `R·e_a` column `a` of the rotation matrix, which is what
  makes a turned body correct. **Center writes NO position** — the scale
  changes and the placement does not — and the other two hold the negative or
  positive face STILL in world space. Zero, negative, non-finite, an
  out-of-range axis and a degenerate axis (a plane's zero-thickness Y) are all
  **refused by name, never clamped**, and no thickness is fabricated for one.
  **Relative Scale is a temporary multiplier, not a second scale vector**: it
  opens at `(1, 1, 1)` EVERY activation because nothing anywhere stores one,
  commits as `newAbsolute = oldAbsolute ⊙ multiplier` with the position
  untouched, and is never serialized, never in a history step, a checkpoint or
  the fingerprint. It is never called World Scale. One exact dimension edit is
  ONE `ScopedConstructionEdit` and one Relative Scale Apply is another; a
  commit that finds nothing different records nothing. Both refuse a LOCKED
  body (Stage 018A's guard, because a resize MOVES one), a HIDDEN body (the
  leaders are read off geometry that is not drawn — the visibility the user set
  is not touched by the refusal), and every representation this stage does not
  cover. Entering Dimensions WITHDRAWS the transform gizmo below JNI as well as
  above it, because the leaders and the handles are two instruments for one
  placement. The leaders are the SKETCH overlay's own line list and ranges fed
  through the same renderer path — the renderer needed no change — and the
  numeric labels are chrome anchored to the projected midpoint of the real
  dimension line. **Not this stage:** Directional Scale handles or mode (Stage
  020D, blocked by OQ-01), Sculpt dimensions (`SCULPT-DIM-01`, blocked by
  OQ-02), Imported Mesh and CAD Body dimensions, CAD FEATURE dimensions (a
  sketch length, a radius, an extrusion depth — those stay CAD authored truth),
  multi-select and group scale, hierarchy, snapping and a new unit system
  (`MIRROR-01` is its own creation act, never a dimension edit).
- **Selection is the Objects capsule plus an OUTLINE, and the outline is a
  true silhouette of the body's own rendered geometry** (`SEL-OUT-R1`,
  UI-OWNER-10 / UI-OWNER-11). There is no persistent whole-object glow:
  `kSelectionRestingAlpha` is **0**, so the acknowledgement pulse
  (`forgeshape_selection_pulse.h`, 0.55 decaying over 220 ms) is the only thing
  that ever tints a surface, and what stays afterwards is a band a few screen
  pixels wide around the selected body. The renderer draws it in TWO steps: a
  **mask pass** recorded before the frame's own pass rasterises every scene body
  through a position-only pipeline into a single-channel image with its own
  depth attachment — unselected bodies write 0 and their depth, the selected one
  writes 1 — and a **composite** full-screen triangle recorded inside the main
  pass, after the grid and before the gizmo, that paints the band where a pixel
  is OUTSIDE that coverage and something within the band's width is inside.
  Occlusion is therefore resolved by the mask pass's depth test and by nothing
  else: the hidden part of a selected body is simply not in the mask, so an
  x-ray outline is not something the composite could draw. The band is drawn
  OUTSIDE the silhouette, never inside, because an inner band on a small or thin
  body is the full-object fill this stage exists to avoid. **It is
  representation-neutral without one branch**: the mask pass binds the body's
  OWN device-local buffers — the ones its shaded draw binds — so Construction,
  Imported Mesh, Sculpt and CAD are correct for free, and a selection change
  costs one push-constant float per body and re-uploads nothing. The width is
  screen-space and bounded (`forgeshape_selection_outline.h`: a fraction of the
  viewport's short side clamped to [2, 5] px, ~3.0 px on a 1080-wide phone), so
  the camera is not an input and zoom cannot change it; the colour is authored
  per ground FAMILY through `viewportBackgroundIsLight` like every other tool
  colour, and both values are **policy, not preference** — there is no outline
  width or colour control anywhere. `selectionOutlineCoverage` is the CPU
  REFERENCE implementation of the edge rule and `shaders/outline.frag` mirrors
  its tap counts, exactly as `grid.vert` mirrors `kGridDepthNudge`. **The
  toggle is the GRID's in every respect**: `Selection Outline` sits beside
  `Grid` in the Display popover's View group, lives in the process-scoped
  `DisplaySettingsStore`, is session-only and native-owned, survives rotation
  and HOME/resume, and is NOT an `AppPreferences` field — the Settings page owns
  persistent preferences and a transient viewport overlay does not belong there.
  With it off the renderer records neither pass, so "off" costs one boolean per
  frame. Nothing here is truth: no `MeshRevision`, no rebuild, no upload, no CAD
  regeneration, no history step, no dirty flag, no `.forge` byte. The mask and
  its depth image are swapchain-scoped and their allocation count moves on an
  extent change or a device rebuild and on nothing else. **Not this stage:** an
  x-ray or hidden-object reveal, multi-select, a second selection mode, and any
  user setting for the band.
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
  triangles, which would be implementing through winding the reflection the
  transform refuses to carry — `MIRROR-01` mirrors through the orientation and
  therefore exports as an ordinary body. `.forge` still stores the nine authored
  values: baking is an interchange decision and lives only in
  `forgeshape_gltf_export.cpp`.
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
  deleting an inactive body moves nothing. **A project is never empty** (an
  empty scene is Home, reached only by leaving the project — see `APP-H1`),
  so deleting the last body is refused by name (`RefusedLastBody`) and NEVER
  answered by inventing a replacement primitive; Delete is refused while
  sculpting too, on the same terms body switching and Undo/Redo already are.
  Grouping stays out; Rename, visibility, lock and duplicate arrived in Stage
  018A, below.
- **A body's NAME, VISIBILITY and LOCK are representation-neutral project
  truth** (`UI-OWNER-40`, Stage 018A). A body has all three because it is a
  body, exactly as it has a placement because it is a body, and nothing in
  `forgeshape_body_commands.{h,cpp}` — the one implementation, beside
  `deleteSceneBody` and on its terms — asks what a body is in order to decide
  whether it can be renamed, hidden or locked. Each act is ONE
  `ScopedConstructionEdit` (one Undo), each is refused while sculpting and
  while a sketch is open, an act that changes nothing records nothing, and a
  refusal changes nothing at all. None of the three publishes a mesh, mints a
  `MeshRevision`, rebuilds a CAD mesh or moves a sculpt vertex; all three move
  the fingerprint, because all three reach `.forge` bytes.
  **Hidden is enforced in exactly ONE place** — `ConstructionScene::snapshot`,
  the single list the renderer draws and CPU picking casts against — so "not
  rendered" and "not pickable" are one fact rather than two predicates that
  could drift, and the selection outline follows for free. A hidden body keeps
  its row, its selection, its published revision, its `.forge` record and its
  export; hiding is not deleting, and hiding the ACTIVE body does not move the
  selection. **Locked stays visible and stays pickable**; what it refuses is
  being MOVED, through two named guards — the gizmo is not activated over one
  (`FORGESHAPE_GIZMO_REFUSED:body_locked`) and a transform write is rejected
  (`APPLY_REJECTED_LOCKED`, its own code because a lock is not a statement
  about a value). Lock changes Delete not at all. **Rename reuses the DOMAIN's
  existing name rule** (`sanitizeImportedMeshName` /
  `importedMeshNameIsStorable` / `kMaxImportedMeshNameBytes`) rather than a
  second policy, an empty or unsanitizable name is REFUSED rather than replaced
  by the ObjectId fallback label, and the JNI boundary reads UTF-16 units
  through `utf16ToUtf8` — never `GetStringUTFChars`, whose modified UTF-8 would
  make the sanitizer drop a legitimate emoji. **Duplicate** mints a fresh
  `ObjectId`, never reuses one, and copies the source representation's own
  truth, the placement, the visibility, the lock, a deterministic `name copy`
  suffix and the Frozen Sculpt Mesh's current geometry — but NOT the
  `SculptHistory`, which describes strokes made on the original, and not a
  renderer resource, a published revision or the Construction history. The copy
  is appended and becomes active. A **face-supported CAD Body is refused by
  name** (`RefusedFaceSupportedCad`): its placement is derived from its
  producer's face frame, so a copy would stand permanently coincident with the
  original and could never be moved off it; nothing is retargeted and no
  dependency is rewritten. A world-plane CAD Body duplicates normally, and so
  does a producer that has dependents — one Duplicate copies one body, never a
  graph. **`SCNE` gains version 2** carrying the per-body flags byte and name,
  written ONLY when a body is hidden, locked or named, so a project without any
  of them stays v1 and byte-identical and the whole existing fixture corpus is
  unchanged; `SCNE` is required, so an older build refuses v2 rather than
  opening a project with a lock silently dropped, and a v1 file loads visible
  and unlocked by the model's own defaults rather than by a migration. The NAME
  has ONE owner per representation — `IMPT`'s for an imported body, `SCNE` v2's
  for every other — and a file stating both is refused. A reserved flag bit is
  refused, never masked. The row keeps two targets (the label and Delete) plus
  ONE overflow that grows the other four out INLINE beneath it, because a
  220 dp panel cannot carry a command column and a persistent one is the
  desktop shape this product does not have. **Not this stage:** multi-select,
  hierarchy, nesting, reorder, drag and drop, a group command bar, and bulk
  rename.
- **Mirror is a proper ROTATION, never a negative scale, and it creates one new
  body** (`MIRROR-01`). Reflecting a Construction Body across one principal
  WORLD plane — `XY` reflects Z, `XZ` reflects Y, `YZ` reflects X — is a
  discrete CREATION act and not a live symmetry modifier: the source is not
  touched, nothing links the two afterwards, and no second Mirror is implied by
  the first. Scale stays strictly positive, because a negative factor inverts
  winding and every normal with it and would make the exporter's own
  `MirroredTransform` refusal a lie. The reflection is carried by the
  orientation instead: with `Qx = diag(-1, +1, +1)` and `F` the world
  reflection, `p' = F·p`, `R' = F·R·Qx`, `S' = S`, and `det(R') = +1`. The
  determinant is not the proof — the claim is the identity
  `Model_mirror(q) = F · Model_source(Qx·q)`, which holds for ANY local `q`
  because `Qx` and `S` are both diagonal, and which becomes world-geometry
  equivalence because every Construction primitive's generated vertex set is
  closed under `Qx` (all six are origin-centred and their rings carry
  `kPrimitiveRadialSegments`, 32, divisible by four). `Qx` is algebra: it is
  never persisted and never appears in a history step. The arithmetic lives in
  `forgeshape_body_mirror.{h,cpp}` as a pure function over values — no scene, no
  history, no body, no camera — and `mirrorSceneBody` beside the other object
  commands owns the eligibility, the identity and the transaction. **It is the
  one object command that is deliberately NOT representation-neutral**: an
  Imported Mesh, a CAD Body and a body carrying a Frozen Sculpt Mesh are each
  refused BY NAME, because only a Construction primitive's own geometry is
  symmetric enough for a rotation to be an exact reflection; the control is
  absent for one and the guard below JNI stays regardless. One Mirror is ONE
  `ScopedConstructionEdit` (one Undo); Undo removes only the reflection and
  restores the previous selection, and Redo restores the SAME `ObjectId`. The
  reflection gets a fresh id, the source's own parameters, the mirrored
  placement, Stage 018A's Duplicate policy for visibility and lock, and a
  `<name> Mirror` name from the ONE `derivedBodyName` collision rule Duplicate
  also uses. **No `.forge` field, section or version changes** — a mirrored body
  is an ordinary Construction body wearing an ordinary transform, so nothing
  stores which plane made it, and a mirrored project encodes byte-identically to
  one placed by hand. The plane chooser grows INLINE in place of the row command
  strip and is a CHOICE and not yet an act: System Back closes it and mutates
  nothing. **Not this stage:** Imported Mesh, CAD and Sculpt mirror, sketch
  entity mirror, Sculpt stroke X symmetry, an arbitrary or face plane, a custom
  workplane, hierarchy subtree and multi-select mirror, a live symmetry
  modifier, a linked instance, and any boolean.
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
  name plus creation), *row command strip* (the inline group of Rename,
  Show/Hide, Lock/Unlock, Duplicate and Mirror an Objects row's overflow grows
  out beneath itself, on two lines because five 48 dp targets do not fit one),
  *mirror plane chooser* (the three-chip XY/XZ/YZ surface Mirror grows in place
  of that strip), *Mirror* (creating one new body reflected across a principal
  world plane; never a live symmetry modifier and never a negative scale),
  *Add Primitive* (the six-shape creation surface),
  *precision surface* (the on-demand exact-value panel, implemented by
  *Property Inspector*), *anchored surface* (any panel that grows out of the
  control that opened it; `AnchoredSurfaceView` owns the growth for all of them),
  *Construction Body* (an editable CAD-like object), *Imported Mesh* (a body
  whose geometry came from a file and has no parameters behind it), *CAD Body*
  (a body made by extruding a sketch; its sketch and depth are editable),
  *Sketch* (the editing context between New Sketch and Extrude, on a
  *workplane* XY, XZ or YZ, with the seven sketch tools Select, Line, Polyline,
  Rectangle, Circle, Arc and Spline on the Tool Rail and *Finish Sketch* /
  *Extrude* as its two toolbar transitions),
  *extrude arrow* (the world-space arrow along the extrusion normal, its shaft
  the depth; never a "gizmo", which is the transform instrument and scales the
  opposite way), *canvas extrude cluster* (the camera-attached group anchored to
  that arrow: the exact depth, *Flip* and the *New Body* badge),
  *Flip* (reversing which side of the sketch the solid grows on; a direction and
  never a negative depth), *New Body* (what an extrusion does — the only
  operation there is, and never drawn beside an Add or a Cut that do not exist), *start page* (the full-window
  opaque Home and New Project screens, `StartPageView`), *Settings page* (the
  full-window persistent-preferences page, `SettingsPageView`, reached from
  Home and from the Project surface), *palette* (one of the five appearances:
  Warm Graphite, Neutral Charcoal, Light Charcoal, Warm Light, Cool Light),
  *handedness* (which window edge the rail zone stands on), *orientation
  navigator* (the sketch's upper-trailing plane/normal/rotation control),
  *dimension* (the technical-drawing annotation on a selected Line, with its
  editable length), *Edit Sketch* (reopening a committed CAD Body's sketch,
  staged until Finish), *Frozen Sculpt Mesh* (the
  polygon mesh `SculptMesh::freezeFrom` creates, from EITHER source),
  *sculpt brush* (one of the seven Tool Rail entries in Sculpt: Grab, Clay,
  Smooth, Flatten, Inflate, Crease, Mask; never "brush preset" and never
  "brush type", because a brush is code and not data), *Sculpt Mask* (the
  runtime-local per-vertex weight that holds the six geometry brushes off an
  area — the user reads "mask" and "protected", never "weight" or "factor"),
  *Clear Mask* (the one contextual act that empties it, as one Undo),
  *selection outline* (the persistent silhouette band around the selected body;
  never "selection highlight", which is what the tint it replaced was),
  *Dimensions* (the mode that reads and types a Construction Body's overall
  local X/Y/Z size; never a CAD feature dimension, which is a sketch
  parameter), *dimension leader* (one axis's extension lines, dimension line and
  end ticks, drawn by the renderer), *anchor* (which side of the body a resize
  holds still: negative, centre or positive), *Relative Scale* (the temporary
  multiplier on the size a body already has, opening at 1/1/1 every time; never
  "World Scale"), *Absolute Scale* (the stored, authoritative, unitless scale
  vector the transform has always had), *history
  capsule* (the bottom trailing capsule holding Undo and Redo, and in Sculpt the
  History navigator beside them), *History navigator* (the compact scrolling
  list of the active body's retained sculpt STATES, and the tap that stands the
  body on one; never a "history panel", and its rows are states rather than
  entries), *transform mode selector* (Move /
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
  authoritative exhaustive-sharded full-suite path.
- **An aggregate is resumable, and a resume never crosses a rebuild**
  (`TEST-RUNTIME-R1`). The runner fingerprints the tested tree from the
  INSTALLED BYTES — the app APK's SHA-256, the test APK's SHA-256, the
  discovered inventory with its shard assignment, the partition, the shard
  count and the device — never from Git HEAD, because a dirty candidate is
  routinely what is under test. A checkpoint is written into the run directory
  after every shard, so `-Resume` re-runs only what did not pass; it skips a
  shard ONLY when the fingerprint is identical, and refuses by name
  (`RESUME_INVALID_APP_APK_CHANGED`, `..._TEST_APK_CHANGED`,
  `..._TEST_INVENTORY_CHANGED`, `..._PARTITION_CHANGED`,
  `..._SHARD_COUNT_CHANGED`, `..._DEVICE_CHANGED`,
  `..._CHECKPOINT_SCHEMA`) otherwise. **Editing any source rebuilds the APKs
  and invalidates every earlier PASS**: rerun the failing shard focused,
  stabilise the tree, and give a final aggregate one fresh run.
  `FULL_SHARDED_SUITE_PASS` keeps every integrity condition it had and adds
  one — every contributing shard belongs to the same fingerprint — so it can
  never be assembled from two builds. `-ShardOnly N` and `-PlanOnly` are
  subset and dry-run evidence and emit no aggregate marker in either
  direction. **The runner never restarts an aggregate from shard 1 by
  itself**: on a failure it stops, classifies (`PRODUCT_TEST_FAILURE` only for
  attributable assertion evidence, `INFRASTRUCTURE_FAILURE` for aborts and
  device faults, `RUNNER_ERROR` when the evidence is ambiguous), and prints
  the exact rerun and resume commands. **TEST-OWNER-02** budgets are enforced:
  a warning at `-TargetMinutes` (90) and a stop at `-HardStopMinutes` (120),
  and at most two automatic aggregate attempts per fingerprint —
  `-OwnerOverrideAttemptLimit` is the only way past that and is logged. `-FullSharded` must use
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
