# ForgeShape — Project Status

**Status Version:** 0.60.0
**Updated:** 2026-09-04
**Result:** **CAD-A3 + APP-H1 — COMPLETE (`PASS-CAD-A3-APP-H1-OWNER-RETEST-READY`).
ForgeShape opens on Home; a new CAD project begins from a plane picked in the
viewport and exists the moment its first sketch is extruded; a CAD sketch can
be supported by a planar CAD face; the CADB v2 corpus is independently
encoded.**

`CAD-A3-C1` closed every item the previous pass deferred, on top of the
CAD-A3 domain it left green:

- **Home is not a project** (`APP-H1`). The process starts with the scene
  EMPTY (`ConstructionScene(NoProjectTag)`), and `hasProject()` — at least one
  body — is the ONE answer to "is a project open"; the workspace derives Home,
  the New Project chooser, the unsaved-changes question or the editor from it
  on every refresh and remembers none of them. Behind Home nothing is drawn,
  picked, saved, checkpointed or fingerprinted, and nothing fabricates a
  default primitive or a placeholder body. The four process-scoped accessors
  answer for "no project" without reading `activeBody()`; the few JNI entry
  points that read it directly refuse by name; and `activeBody()` on an empty
  scene answers a COUNTED null object rather than dereferencing an empty list —
  `HomeFlowTest` asserts on every journey that the count never moved, and it
  never did.
- **New Project offers exactly CAD and Sculpt.** CAD is the transient
  bootstrap: the spatial world-plane chooser and the one volatile sketch
  session over the empty scene, owning no `ObjectId` and no `SceneObject`; the
  first Extrude creates the project through `commitFirstCadProject` — a
  one-body document replacing the scene through the SAME all-or-nothing
  `loadProjectDocument` path Open takes, so the new project starts with an
  EMPTY history, a refusal creates nothing, and Back to Home before it costs
  nothing. Sculpt is the seeded sphere inside the session-initialization
  bracket. Open File from Home takes the existing SAF contract: cancel and a
  refused file leave Home standing with the verdict on it.
- **Leaving a dirty project asks Save / Discard / Cancel**, by fingerprint
  against the last Save / Open / Open File / Recover; Save continues only if
  the slot was written, Discard retires the checkpoint, Cancel and System Back
  change nothing. Back is one step in every phase and the platform's own at
  Home.
- **New Sketch lands directly in the spatial chooser** (`UI-OWNER-46`); the
  by-name planes stay as `sketch_plane_by_name`, the accessibility fallback.
  A stylus hover highlights a target and never commits (verified as a
  synthesized hover through the real dispatch; hardware hover is not claimed).
- **The independent CADB v2 corpus** (`scripts/build-forge-corpus.ps1`): six
  fixtures — cap support, side support, a three-body chain, every
  representation beside a face-supported body, and two the decoder must refuse
  (a face reference naming no face, a dependency cycle), the corrupt two
  CONSTRUCTED with the bad value in place — PowerShell bytes = C++ bytes, pinned
  by `CADA3-46..51`, all sixteen v1 fixtures byte-identical. **The parity found
  a production defect**: the lineage token's FNV-1a offset basis in
  `forgeshape_cad_face.cpp` was mistyped one digit short; because a reader
  recomputes and compares that token it is a FORMAT field, so the contract was
  fixed to the algorithm the code and `DATA_PACKAGE_SPEC.md` §7c state. No
  shipped file carried a v2 token before this pass.
- **Screenshot evidence**: `CadA3VisualEvidenceTest` captures the ten required
  states through the composed display with measured facts, and
  `scripts\collect-cad-a3-evidence.ps1` builds `OWNER_CONTACT_SHEET.png` and
  `VISUAL_EVIDENCE.md` from them.

The CAD-A3 core is unchanged: semantic face topology and `TopoRef`
(`forgeshape_cad_face.{h,cpp}`, `ARCH-OWNER-13`), the derived world placement
of a face-supported body (`ConstructionScene::resolveWorldModel`), the acyclic
bounded dependency graph and the producer-delete refusal, CADB v2 with v1
byte-identical, the exact sketch camera and the adaptive grid.

Verified: **19/19 native suites, 2920 checks, zero failures** (13 new
`CADA3_BOOT_*`, 17 new project-suite checks); the standalone runner's 17
platform-neutral suites 2567/0; JVM 70/70; both ABIs debug and release; device
guards; the twenty-two-fixture corpus; the new device suites `HomeFlowTest` OK
(12), `SpatialSketchTest` OK (8), `CadA3VisualEvidenceTest` OK (1),
`SketchExtrudeTest` OK (9) and the seven classes the Home change touched; and
the authoritative aggregate — see *Current evidence summary*. All device work
on `emulator-5580` = `ForgeShape_Stage006`; `emulator-5554` never contacted.
See `artifacts/cad-a3-app-h1/INDEX.md`.

---

**Previous result — CAD-A3 — PARTIAL (`FAIL-CAD-A3-APP-H1`), closed above.**
A CAD sketch can be supported by a planar CAD face, and the dependent body
follows its producer. What that pass delivered, from an existing CAD project:
**New Sketch → Pick plane or face in 3D → tap a world plane OR a planar CAD
face → an exact-normal orthographic sketch → draw → Extrude New Body → a
durable, editable, FACE-SUPPORTED CAD body whose placement follows its
producer's Move / Rotate / edit, whose producer cannot be deleted while it
stands, and whose dependency survives a save / reopen.**

---

**Previous result — CAD-R0-A1A2 — COMPLETE. A sketch on a workplane becomes an
editable CAD Body.**

The first end-to-end CAD workflow, as one vertical slice: **New Sketch → a
principal plane → Rectangle / Circle / Line / Polyline with endpoint-then-grid
snapping and exact typed values → Finish Sketch → a closed profile is found
or the refusal is named → a typed depth and a direction → Extrude as a New
Body → an ordinary object with an Objects row and a gizmo → its sizes and depth
editable later, one Undo each → saved, reopened, recovered and exported as its
sketch and its extrusion, never as a frozen mesh.**

- **A third representation.** `BodyRepresentation::Cad` and `CadBody`
  (`forgeshape_cad_body.{h,cpp}`): a `CadBodyState` of one sketch
  (`forgeshape_sketch.{h,cpp}`) on one workplane (`forgeshape_workplane.{h,cpp}`)
  and one linear extrusion. It has no primitive to be a Construction Source with
  and no fixed geometry to be an Imported Mesh with, so it is its own, exclusive
  for the body's life, and every one of the twenty-odd `constructionOrNull()` /
  `importedOrNull()` call sites had to say what it does about it.
- **Truth and product.** The sketch entities, their per-sketch ids, the chosen
  profile's anchor id, the depth and the direction are truth; the closed
  profiles, the polygon, the triangles and the mesh are regenerated through
  `generateCadMesh`, the one path. `CadBody::applyState` validates and
  regenerates the WHOLE requested state and writes nothing unless all of it
  passes — no half-regenerated body, and an edit that would leave no closed
  profile is refused by name.
- **The profile engine fails closed by name.** Open, forked, crossing,
  zero-area, duplicate-edge and nested loops are refused, never repaired;
  crossings are tested before area so a bow tie says "it crosses itself"
  rather than "zero area". A rectangle is parametric (centre, width, height);
  a circle is centre and radius, tessellated at the round primitives' 32
  segments with exact cardinal points; a chain of lines closes a loop anchored
  by its smallest id. Bounded ear clipping; watertight extrusion with canonical
  outward winding on all three planes, proved by signed volume for the concave
  case.
- **The sketch session is volatile.** `SketchSession` owns one pointer, snaps
  to endpoints before the 0.25 m grid with EXACT coordinates, places a rectangle
  corner to corner, a circle centre to radius, a line end to end and a polyline
  tap by tap, and commits as ONE `ScopedConstructionEdit` around ONE
  `addCadBody`. Nothing before that commit reaches the scene, the history, the
  fingerprint or a `.forge` byte; `cancel` costs the project nothing. While a
  sketch is open one finger draws and never orbits, two fingers still pan and
  pinch, and creation, deletion, body switching, freeze and Construction
  Undo/Redo are refused below JNI and withdrawn above it.
- **The view is borrowed, not stored.** The camera frames the plane
  orthographically along its normal and the user's own pose comes back on
  commit and on cancel. The sketch is drawn as a world-space line list through
  the gizmo's own pipeline, re-uploaded only when its revision changes.
- **`.forge` gained `CADB`**, a required section on `IMPT`'s terms behind header
  bit3: the authored state and never a vertex, held to `validateCadBodyState`
  (extraction included) so a file whose sketch closes no profile is refused;
  exclusive with `CONS` and `IMPT`; `SCUL` over it refused. No version bump;
  the twelve older fixtures are byte-for-byte unchanged and four new ones are
  pinned by both encoders at identical digests.
- **Chrome.** New Sketch and a plane chooser inside the creation palette; the
  five sketch tools on the Tool Rail; *Finish Sketch* and then *Extrude* as the
  one toolbar transition per sketch state, with Cancel Sketch and Back to
  Sketch under the rail; *Sketch values* for the selected entity, the profile
  choice, the depth and the direction; a CAD Body's *Shape* panel for its sizes
  and depth; *Start Sculpting* absent for a CAD Body.

**Deliberately NOT this stage:** CAD → Sculpt (refused by name; three
decisions it needs are recorded in `ARCHITECTURE.md`), holes, booleans,
fillets, chamfers, shells, revolves, sweeps, lofts, patterns, mirrors, offsets,
trims, constraints, arcs, splines, face-based planes, rotated rectangles, and
numeric editing of a polygon profile's points.

Verified: **18/18 native suites, 2845 checks, zero failures** (122 new `CADR0_*`,
11 new project checks); JVM 70/70; both ABIs debug and release; device guards
`DEV2-01..07` / `DEV3-01..06`; the sixteen-fixture corpus verified against the
independent PowerShell encoder; the new focused device suite `SketchExtrudeTest`
**OK (9 tests, `E2E-CADR0-01..16`)**; and the authoritative exhaustive-sharded
aggregate — see *Current evidence summary* and `artifacts/cad-r0-a1a2/`. All
device work on `emulator-5580`, confirmed `ForgeShape_Stage006`;
`emulator-5554` never contacted.

---

**Previous result — SCULPT-UNDO-R0 — COMPLETE. A sculpt stroke can be taken
back, and sculpting has its own history.**

One bounded capability, under `ARCH-OWNER-12`, and nothing else.

**Sculpt Undo and Redo.** The two chrome controls the user already knows are now
drawn while sculpting, and there they step **strokes**. One press takes back the
last stroke however many times the finger moved during it; Redo puts it back.
Both work for a Construction-derived and an Imported-Mesh-derived sculpt, through
the same one implementation.

The owner's boundary — *project/object history never stores a sculpt vertex* —
did not move. What arrived beside it is a second history:

- **`SculptHistory`** in `forgeshape_sculpt_history.{h,cpp}` — a bounded,
  volatile, **per-body** Undo/Redo, living inside `FrozenSculpt` beside the mesh
  it describes. Ownership is the whole mechanism: no key, no registry, no
  "current sculpt stack", so body A's Undo is *structurally incapable* of
  reaching body B, and `sculptSession()`'s existing per-call rebind makes a body
  switch switch history for free.
- **One completed stroke is exactly one entry**, never one per pointer event.
  `SculptStroke::begin` already captured the affected set with every member's
  base position — the four brushes need it — so the entry's BEFORE side cost
  nothing; `buildDelta` reads the AFTER side at close, keeping only vertices that
  actually moved. `SculptSession::recordActiveStroke` is the ONE caller, reached
  from both `endStroke` and `cancelStroke`, which makes the boundary structural
  rather than a convention two call sites share.
- **An entry is a DELTA** — sorted unique indices, before and after positions,
  and the edited flag on both sides. No normals (the mesh regenerates them and a
  copy could only disagree), no document, no Construction parameter, no
  `ImportedMesh`, no transform, no `ObjectId`, no GPU handle, no `.forge` byte.
  A malformed delta is refused rather than stored and applied later.
- **Both caps are enforced**: 32 entries AND 4 MiB per body, with 1 MiB for one
  stroke. Either alone is escapable. Eviction is oldest-first and never takes the
  newest entry; a stroke too large to retain still APPLIES and says so by name
  rather than silently. Measured: a whole-mesh stroke on the 482-vertex sphere
  fixture costs 13.3 KiB, so a full 32-stroke stack is ~426 KiB — about a tenth
  of the budget.
- **`hasEdits` stopped deriving from the revision**, and had to. A revision must
  stay monotonic — the renderer, picking and the autosave fingerprint all notice
  a sculpt change by it, including the change an Undo makes — while "has edits"
  must be able to go back. It is now an explicit flag; a project loaded with
  edits starts with an EMPTY history and still reports them, which no undo depth
  could answer. `.forge` already stored it as a boolean, so the format is
  untouched.
- **Nothing is serialized.** No `.forge` byte, no checkpoint, no new section, no
  version bump — reopening a project restores the geometry and starts a fresh,
  empty history. All twelve corpus fixtures and all seven canonical digests are
  unchanged, verified against the independent PowerShell encoder.
- **Two boundaries Undo cannot cross**: a Freeze or destructive *Reset from
  source* clears both stacks, and a deleted body's history leaves with the body.
  Undoing the Delete restores the SAME object, so the stacks return with it —
  because `holdDetachedBody` holds the object, not because anything was
  serialized.
- **Native owns the dispatch.** `constructionUndo`/`Redo` are ALWAYS the
  Construction history and are still refused in Sculpt — the guard did not move.
  `sculptUndo`/`Redo` are always the active body's. The chrome calls a third
  pair, `historyUndo`/`historyRedo`, which choose on the product mode below JNI,
  because the layer that holds no state must not make that choice.

Sculpt still has no history panel, no named steps and no keyboard shortcut, and
brushes are still code rather than data: nothing here became a framework.

Verified: 17/17 native suites, **2712 checks, zero failures** (84 new `SCUNDO_*`,
green on the first run); JVM 70/70; both supported ABIs debug and release; device
guards `DEV2-01..07` and `DEV3-01..06`; the `.forge` corpus verified against the
independent PowerShell encoder with **all twelve fixtures and all seven digests
unchanged**; the new focused device suite `SculptUndoTest` **OK (10 tests)**;
eight focused instrumented classes re-run; and the authoritative
exhaustive-sharded aggregate — **`FULL_SHARDED_SUITE_PASS`, 34 classes / 482
tests / 5 shards, missing = duplicates = unexpected = 0**. All device work on
`emulator-5580`, confirmed `ForgeShape_Stage006`; `emulator-5554` was never
contacted. See `artifacts/sculpt-undo-r0/`.

One decision worth the owner's eye: **a cancelled stroke that moved geometry is
recorded**, so it can be taken back. The product's standing rule is that a
cancelled stroke keeps the positions it already wrote, and excluding those from
the history would make them the one deformation in the product the user cannot
reverse. A cancel that moved nothing records nothing, and a *partial* entry is
impossible by construction. See `artifacts/sculpt-undo-r0/INDEX.md`.

---

**Previous result — IMPORT-01B — COMPLETE. An imported mesh can be sculpted, and
the Objects list can delete a real project object.** Two bounded capabilities,
under `ARCH-OWNER-11` and `UI-OWNER-45`.

**Imported Mesh Sculpt.** *Start Sculpting* is offered for a body whose geometry
came from a `.glb`, and the workflow behind it is the one that already existed —
the same four brushes, the same stroke kernel, the same adjacency, the same
`SCUL` branch, the same destructive-reset guard. What was added is one function:

- **`buildSculptSourceMesh`** in `forgeshape_scene.{h,cpp}` — the second ONE
  dispatch point beside `publishSceneObject`. A Construction Body regenerates
  from its parameters; an Imported Mesh hands over the arrays it owns. Either
  source is only READ.
- **It copies the RAW indices, never `buildDrawData`'s.** A double-sided
  submesh's reversed duplicate contributes the exact negation of its twin to
  every area-weighted vertex normal, so seeding from the draw buffer would freeze
  a two-sided submesh with zero normals and no normal-based brush could move it.
  Per-submesh `doubleSided` therefore collapses into the frozen mesh's one
  `renderBothSides`, permissively, so an imported sheet stays reachable from
  behind.
- **The imported source is immutable and stays so.** Positions, normals, indices
  and submesh batches are bit-identical after a real stroke; the placement is not
  baked a second time; nothing invents a Construction Source; and an Imported
  Mesh can never go STALE, because nothing can edit one.
- **`.forge` generalized `SCUL`**, with no version bump and no new section: an
  entry's body may have `CONS` or `IMPT`. The four valid combinations are
  `SCNE+CONS`, `SCNE+CONS+SCUL`, `SCNE+IMPT`, `SCNE+IMPT+SCUL`. Compatibility is
  fail-closed — an older build REFUSES an `IMPT`+`SCUL` file rather than opening
  half a body — and all ten pre-existing fixtures' digests are unchanged.
- **The wording is representation-aware and nothing else is.** *Back to Imported
  Mesh* (`← Imported Mesh` where the row cannot carry it, full wording always the
  content description), *Reset Sculpt from Imported Mesh…*, and an *Imported
  Mesh* editing context. The view id stays `back_to_construction`, because an id
  names the act.
- **Export needed no change.** It already asks the project's MODE, so a body with
  a Frozen Sculpt Mesh exports it in Sculpt and its source otherwise — the same
  effective-state rule existing sculpt bodies follow, now verified for an
  imported one.

**Delete.** `deleteSceneBody` in `forgeshape_body_delete.{h,cpp}`, its own small
module for the reason `forgeshape_import_commit` is one.

- **Representation-neutral, and that is the design.** Nothing in it asks what a
  body is: a Construction Body, an Imported Mesh, and either carrying retained
  sculpt work are removed by the same three lines, because a body leaves as a
  whole object.
- **The body is HELD, not destroyed.** `ConstructionHistory::holdDetachedBody`
  keeps it while any step still names it, because an imported object's geometry
  and a Frozen Sculpt Mesh are the two things a step cannot rebuild.
  `pruneDetachedBodies` now asks both stacks and both sides of every step.
- **One Delete is one Undo.** Undo restores the SAME object in its own place with
  its own published revision; Redo removes it again; no orphan `CONS`, `IMPT` or
  `SCUL` record survives; the deleted body does not render, pick, save,
  checkpoint or export.
- **Selection is deterministic**: the next row, or the previous when the deleted
  one was last, and unchanged when the deleted body was not active.
- **The last body is refused by name** (`RefusedLastBody`), because this product
  has no empty project — and no replacement primitive is ever invented. Delete is
  refused while sculpting too, on the same terms body switching is, and the
  control is withdrawn in both cases. (Undo and Redo were withdrawn in Sculpt on
  those terms as well until `SCULPT-UNDO-R0`; there they now mean the Sculpt
  history, while the CONSTRUCTION entry points are still refused.)
- **The renderer stopped leaking.** `releaseBodiesAbsentFromScene` frees the GPU
  copy for a body the scene no longer names. Delete made that a real unbounded
  growth — add/delete in a loop mints a fresh `ObjectId` every cycle, and each
  resident entry holds two `VkDeviceMemory` allocations against a device limit
  commonly around 4096. Undo costs one re-upload, through the path a body already
  takes the first time it is drawn. Its in-flight wait is BOUNDED (100 ms) and
  the release is retried next frame on a timeout: the first authoritative
  aggregate caught an unbounded wait here hanging the render thread, and with it
  `surfaceDestroyed` and the Activity teardown. See
  `artifacts/import-01b/DELETE_CONTRACT.md`.

**The Objects row is now a pair**: the label selects, the control beside it
deletes, and neither is reachable by the other. Delete is deliberately
unconfirmed — it is one Undo away, and the product's one dialog guards the one
act that genuinely cannot be undone.

Verified at the time by 17/17 native suites (2628 checks), JVM 70/70, both ABIs
debug and release, the device guards, the `.forge` corpus, nine focused
instrumented classes and its own `FULL_SHARDED_SUITE_PASS` (33 classes / 472
tests). Every one of those gates was re-run for `SCULPT-UNDO-R0`, at the higher
counts recorded above; `adb kill-server` has never been used, because another
program owns a reserved session on the same host daemon.

`E2E-IMP01B-12` is **`OWNER_REAL_FILE_01B_RETEST_PENDING`**: the owner's own
`1 lowpoly.glb` is not in this environment and no user folder was searched for
it.

---

**Previous result — ARCH-HEALTH-01 — PASS-ARCH-HEALTH-REVIEW-WITH-DEBT. The
architecture is sound; one data race and one double tessellation were corrected,
and the remaining debt is named with a timing.**

An evidence-only review of the whole tree at `4c296f5` (the IMPORT-01A
commit), with corrections limited to what a test or a log line can prove. No
MVP expansion, no new stage, no `.forge` change, no UI change. What it found
and what it did, precisely:

- **`applyPrimitive` wrote Construction state with no lock** while the autosave
  worker reads exactly that state under `g_stateMutex` for the fingerprint and
  the checkpoint, so a checkpoint could encode half of one primitive and half
  of another. It now takes the lock before the edit scope, as
  `applyBoxTransform`, `runHistoryStep` and `loadProject` already did. The
  domain never takes that mutex, so nothing below can re-enter it.
- **Every Construction publish tessellated twice**: once for the log line's
  vertex and index counts, once for the real publish. The counts now come from
  the store; `FORGESHAPE_CONSTRUCTION_PUBLISHED:1:8:36` is unchanged.
- **Six stale comments and five stale `ARCHITECTURE.md` statements** — a
  removed accessor, a pre-persistence Serialization row, a history header that
  claimed the active body is compared, a false "no lock across generation
  anywhere" claim — were corrected to what the code does.
- **No null dereference, no unconditional Construction access, no JNI status
  mismatch, no test that sleeps or taps a coordinate, no representation that
  persistence skips** — each hotspot the review named was checked and has a
  verdict in `artifacts/architecture-health-review/HOTSPOTS.md`.
- **Debt that was NOT changed**, because changing it crosses the review's
  line, is recorded below under Technical Debt: renderer GPU resources are
  never pruned for a body Undo removed (RESOLVED at IMPORT-01B, which made it
  unbounded); export and the roundtrip diagnostic duplicate the
  per-representation geometry extraction; the `.forge` decoder does not bound
  `nextObjectId` below the preview key range.

Verified on the corrected tree: build, JVM 70/70, 17/17 self-tests with zero
failures, and five focused instrumented classes (autosave/recovery 14, history
20, imported-mesh durability 12, controls 23, sculpt retention 2), all on
`emulator-5580` confirmed as `ForgeShape_Stage006`. The scorecard is 4.3/5
with no area below 3; none of the three 3s blocks `IMPORT-01B`.

---

**Previous result — IMPORT-01A — COMPLETE. A `.glb` now becomes objects the user
keeps.**

Under `ARCH-OWNER-10` the reader `GLB-IMPORT-R0`/`R1` built gained a second
destination: real project bodies. *Import GLB…* is no longer a preview — it
creates objects with rows in the Objects list, the ordinary gizmo, one Undo step
for the whole import, and geometry the `.forge` document carries so the project
reopens without the source file.

What that cost, precisely:

- **A body now has two representations.** `SceneObject` owns a Construction
  Source or an Imported Mesh, never both, named by `BodyRepresentation`. A
  Construction Source's geometry is DERIVED and regenerated on load; an Imported
  Mesh IS the geometry, so it is project truth. `constructionOrNull()` and
  `activeConstructionOrNull()` are pointers, which made every call site say what
  it does about a body with none.
- **The placement moved up to the body.** It lived inside `ConstructionObject`
  while every body was a Construction Body; a body has one because it is a body.
  That is what lets the gizmo, the picker, the exact values, the history and
  `SCNE` treat an imported object exactly as they treat every other one.
- **One node, one object.** A mesh's several TRIANGLES primitives stay INSIDE it
  as submesh batches and never become rows; each keeps its own `doubleSided`.
- **The node transform is SPLIT** — linear part baked into local geometry,
  translation becoming the placement — so an imported body starts at rotation
  `0,0,0`, scale `1,1,1`, with its own origin exactly where the file's was.
  Nothing is recentred.
- **The commit is atomic and is ONE transaction.** Every object is built and
  validated off the scene; a refusal mints no `ObjectId`, records nothing and
  moves no fingerprint, and an import of forty objects is one Undo.
- **`.forge` gained `IMPT`**, always required so a reader that cannot rebuild an
  Imported Mesh refuses the file rather than opening it with objects missing;
  `CONS` became a subsequence carrying only bodies that have a Construction
  Source. A project with no imported body is **byte-identical** to what v1
  always wrote — the seven legacy fixtures' digests are unchanged.
- **`Shape` and `Start Sculpting` are withdrawn for an imported body** and
  refused below JNI as well. Imported Mesh sculpt is `IMPORT-01B`. (Superseded:
  `IMPORT-01B` shipped, so `Start Sculpting` is now OFFERED for an imported body.
  `Shape` is still withdrawn and still refused.)
- **Export writes imported bodies too**, from their own CPU arrays, baking `L`
  exactly as for every other body: a `.glb` quietly missing an object the user
  can see and move is the one thing that exporter refuses to do anywhere else.

**There is now exactly one user-facing GLB route.** The session-only diagnostic
preview still exists below JNI and the R0/R1 suites still drive it, but it has no
control on the Project surface: two visible ways to open a `.glb` that did
different things to the project is the confusion that removal prevents.

**The owner's own `1 lowpoly.glb` was NOT imported here** — the binary is
external to this coding environment and no attempt was made to find it.
`E2E-IMP01A-12` is `OWNER_REAL_FILE_01A_RETEST_PENDING`, which is not a
technical failure.

---

**Previous result — GLB-IMPORT-R1 — COMPLETE. The preview now opens ordinary
static external GLB files, and it is still not import.**

Under `ARCH-OWNER-09` the diagnostic reader widened from "exactly what
ForgeShape's exporter emits" to the class of **static** `.glb` another sculpting
tool writes, so the owner can put their own low-poly character in the ForgeShape
viewport and look at it. What that cost, precisely:

- a node **`matrix`** or an ordinary **TRS**, composed `T·R·S`, **baked** into
  the preview's positions so every draw item carries an identity model matrix;
- **several TRIANGLES primitives per mesh**, kept as separate draw batches so
  per-primitive `doubleSided` survives, and **sharing one POSITION accessor**
  decodes once rather than once per primitive;
- a **missing NORMAL**, generated area-weighted from the baked positions, with
  `CannotGenerateNormals` rather than a NaN when a vertex has no direction;
- **COLOR_0 / COLOR_1 / TEXCOORD_0 / TEXCOORD_1**, structurally validated and
  then deliberately not decoded — the preview draws one flat neutral and never
  claims an appearance it did not read;
- a material's **`doubleSided`**, which reaches preview culling and nothing
  else. No base colour, no roughness, no texture is read anywhere;
- **`extras`** — `extras.nomad` included — ignored at every level.

Normals ride `transpose(inverse(L))`, not `L`. A negative determinant corrects
triangle winding **for the preview only**; a zero determinant or a non-affine
node matrix is `SingularNodeTransform`. There is still **no coordinate
conversion of any kind** — glTF and ForgeShape are both right-handed, +Y-up and
metric.

Everything outside the subset still fails closed by name, and there are five new
names for it: `NodeTransformConflict`, `SingularNodeTransform`,
`CannotGenerateNormals`, `NonIndexedPrimitive` and `UnknownAttribute`.
`artifacts/glb-import-r1/SUPPORTED_SUBSET.md` is the full table.

**Nothing about what the preview IS changed.** No `ObjectId`, no Construction
Source, no Frozen Sculpt Mesh, no `MeshStore` publish, no history step, no
`.forge` byte, no checkpoint, not selectable, not editable, not re-exportable,
gone with the process — and most of the R1 suite exists to hold exactly that
line while the parser gets more permissive. The Project surface's third group is
now **Preview a GLB file**, whose action is **Import GLB…**; the wording names
the act while every word around it says preview, and it stays a different act
from Open File…, which opens a ForgeShape project.

**The owner's own `1 lowpoly.glb` was NOT opened here** — the binary is external
to this coding environment and no attempt was made to find it. What was
exercised is a deterministic synthetic fixture with the same structural feature
set (`artifacts/glb-import-r1/OWNER_SAMPLE_TARGET.md` maps every feature of the
sample to the case that covers it). Opening the real file is the owner's manual
step: `E2E-GLBIR1-10` is `OWNER_SAMPLE_RUNTIME_TEST_PENDING`.

---

**Previous result — GLB-IMPORT-R0 — COMPLETE, and it answered the owner's
question.**

The owner saw a discrepancy between the ForgeShape scene and Blender that the
corrected node scale of 1/1/1 did not explain. Three things could produce it —
a wrong exporter, a wrong reader, or an external tool presenting the same
geometry differently — and only the first would be ForgeShape's defect. So under
`ARCH-OWNER-08` the product gained the one thing that separates them: a GLB
reader that shares no line with the writer, a session-only preview that draws
what it read in the same viewport, and a machine comparison against domain
truth.

> ### `ROUNDTRIP_EQUIVALENT`, with a maximum world-position delta of **exactly 0.0 metres**.
>
> Every vertex of every body, in world space, is **bit-identical** between the
> ForgeShape scene and the same scene decoded out of the `.glb`. Vertex counts,
> triangle counts, index order and per-body world bounds all match exactly; the
> largest world-space normal disagreement is `1.2e-06` degrees, which is float32
> noise. It holds for the six-body Construction project, for the Sculpt project
> — where the sculpted body is compared as `source=sculpt` — and for the
> **committed sentinel bytes** in `artifacts/e2er1c/` against the projects they
> were exported from.
>
> **So the remaining discrepancy is not a ForgeShape exporter or parser defect.**
> This stage cannot say what it is instead, and it makes no claim about Blender:
> no external tool was run.

The preview is a diagnostic and is session-only: no `ObjectId` from the scene's
allocator, no Construction Source, no sculpt representation, no history step, no
`.forge` byte, no checkpoint, not selectable, not editable, not re-exportable,
and gone with the process. It replaces what the renderer draws for a frame and
never merges with the project snapshot. While it is shown the gizmo is
withdrawn, a tap selects nothing and every editing control is absent, because
over an imported mesh each of them would point at a body the user cannot see.

R0's parser supported exactly the subset the exporter emits and failed closed by
name on everything else. `artifacts/glb-import-r0/SUBSET.md` records that
narrower boundary; R1 above is the current one.

**Neither round is production import.** Durable import — materials, hierarchy,
and what an imported object even *is* in a Construction/Sculpt product — is
`IMPORT-01` and stays post-MVP. OBJ and FBX remain absent in both directions.

---

**Previous result — E2E-R1C-C1 — COMPLETE.** The static interchange contract changed
after the owner reviewed the first exported files in Blender. `ARCH-OWNER-07`
supersedes Stage 023's node-transform policy: a static `.glb` now **bakes each
body's rotation and scale into its vertices** and leaves only the translation on
the node, so the object arrives in another tool already the shape the user made
rather than a shape plus instructions for producing it. Node rotation is
identity and node scale is 1/1/1 for every exported body — in fact both are
absent, along with any node matrix.

The placement is split as `Model = T · L` with `L = Rz·Ry·Rx·S`. Positions are
`L · p`; normals are `normalize(transpose(inverse(L)) · n)`, which is the
existing `normalMatrix()` and is checked numerically against the inverse
transpose rather than assumed. Nothing is recentred — `L` is applied about the
body's local origin, which is the pivot `T` then positions — nothing is merged,
each body bakes its own `L`, and no axis conversion appears: metres, +Y up and
right-handedness are exactly as they were. `det(L) = sx·sy·sz > 0` in this
product's domain, so winding survives untouched; a zero or negative determinant
is refused as `SingularTransform` or `MirroredTransform` rather than
compensated by reversing triangles, which would be implementing the Mirror the
domain says cannot exist.

Construction and Sculpt representation selection is unchanged, `.forge` still
stores the nine authored values, and the Export control did not move. The
sentinels were regenerated: their **pre-bake digests are recorded as history and
are not presented as current** — see *Current evidence summary*.

---

**Previous result — E2E-R1C / Stage 023 — COMPLETE.** A model made in ForgeShape can
now leave it. **Export** — the last reserved control in the product, and now an
ordinary working one — writes the whole scene as a single glTF 2.0 binary
(`.glb`) wherever the user picks through Scoped Storage: every body's triangles,
its crease-policy normals and its placement, in metres, +Y up.

**There is no coordinate conversion, and that is a finding rather than a
convenience.** Before any exporter code was written, the up axis, handedness,
axis ownership, body-local convention, transform composition order and winding
were each established from named repository truth; ForgeShape's world convention
and glTF 2.0's turned out to be the same convention, so 1.0 ForgeShape metre is
1.0 glTF metre and a conversion node, a transposed matrix or a global scale
factor in an export would each be a defect. (The node-transform half of this
result was superseded by ARCH-OWNER-07 above; the coordinate conventions it
established were not, and are unchanged.) `artifacts/e2er1c/COORDINATE_AUTHORITY.md`
records the proof and the case that would catch each of those.

Representation is honoured per body and never guessed: in a Sculpt project a
body with a Frozen Sculpt Mesh exports that mesh, and a body without one exports
re-evaluated Construction geometry. There is no fallback from Sculpt to the
Construction Source, because that would silently ship the shape the user
abandoned. Exporting is a **read** — no revision, no history step, no `ObjectId`,
no sculpt vertex, and neither `.forge` slot is written.

The evidence is pinned at both ends. The `.forge` documents the device cases
load are the permanent golden-corpus fixtures, digest-matched to the second,
independent PowerShell encoder; every exported file is read back by
`GlbDocument`, a test-only glTF 2.0 reader written from the specification that
shares no line with the exporter and re-derives every chunk boundary, accessor
offset and declared bound rather than trusting the writer. The same project
exports byte-identically across runs.

**No external program opened these files.** Godot, Blender and `gltf-validator`
were not run — none is installed here and installing one is not authorized — and
no such result is claimed anywhere. `artifacts/e2er1c/GATE_E2E_GODOT_CHECK.md`
prepares that check for the owner and records no verdict.

This is the early vertical slice, not the Stage 033 exporter: no UVs, no
textures, no chosen materials, no hierarchy, no merge or unit options, no
compression, no second file beside the `.glb`, and **no importer** in any
format. No third-party interchange library is used.

The UI delta is one control that already existed becoming real. Its label stayed
"Export": "Export GLB…" was tried, and `UIR4B-16` caught that the extra width
pushed **Back to Construction** into its abbreviated form, so the format moved to
the content description and the `.glb` filename instead. The accepted
UI-LAYOUT-R2 right host does not move.

---

**Previous result — E2E-R1B / Stage 022 — COMPLETE.** Persistence is now safe for real
work. Semantic edits are checkpointed automatically to a **separate** recovery
file — never over the project the user named — and work that was never saved at
all survives the process dying and is offered back by one bounded question with
two answers. Nothing replaces the live project before the user chooses, and a
checkpoint that cannot be decoded is quarantined once rather than asked about
forever.

A project can be moved on and off the device through Android Scoped Storage:
**Save Copy…** writes the same canonical `.forge` document wherever the user
says, and **Open File…** reads one back through the same fail-closed decoder.
Opening a file makes that project live and deliberately does **not** write the
internal manual slot. No `Uri`, authority, filename or path reaches the codec or
the document — a project opened from a distinctively named file re-encodes to
the original bytes exactly.

GPU resources were never project truth and now say so: a lost device is
classified, the device and everything on it is rebuilt from CPU state, and the
viewport comes back. The **rebuild branch is the one implemented**, verified on
device through a debug injection seam; the bounded fail-closed
`RestartRequired` branch exists behind it and checkpoints the project before
reporting that a restart is needed.

Diagnostics are a bounded local ring of tokens — no geometry, no `.forge` bytes,
no path, no `Uri` — rendered into a report the user may write to a file they
pick. **ForgeShape holds no network permission at all**, so "nothing is sent
anywhere" is a structural fact rather than a policy.

The UI delta is three rows on the existing Project surface and one recovery
question. The accepted UI-LAYOUT-R2 right host does not move.

---

**Previous result — E2E-R1A / Stage 021 — COMPLETE.** ForgeShape work survived
the process dying for the first time: the portable, versioned `.forge` v1
document, one app-private slot, and a fail-closed load that costs a refused file
nothing. `DATA_PACKAGE_SPEC.md` owns the format and
`scripts/build-forge-corpus.ps1` is the second, independent implementation of it
whose bytes match the native encoder's. Both facts still stand and are described
where they are owned; the stage narrative is in Git history.

---

**Previous result — UI-LAYOUT-R2 — COMPLETE.** The coordinator reviewed the
corrected build's representative pairs and gave a **visual PASS on 2026-08-30**,
closing the stage. The measured half had already passed; this is the judgement
half, and it is now recorded rather than outstanding.

The findings behind that verdict: the rail reads as a narrow edge-hugging strip;
Exact/Details sits inboard, left of the fixed rail, in the short-landscape and
expanded states; and no rail translation, overlap, detached control grammar or
unnecessary viewport loss was identified.

Correction round 1 closed the one measured defect the R2 closeout found. The
right context host keeps the trailing window edge in **every** window and every
state: its trailing inset is 8 dp at rest and stays 8 dp with Exact, the IME and
Sculpt Details open, where it previously translated inward by about 320 dp in a
short landscape window and about 360 dp on a tablet. **This corrected edge-host
arrangement is the accepted current workspace baseline** — a narrow right edge
rail at a fixed trailing placement through every Exact/Details state, with a
side-placed Exact/Details seated inboard of it. The obsolete 132-152 dp R0 host
geometry is superseded and must not be restored.

`WorkspaceTrailingHostView` remains the one external right contextual surface,
owning its vertical child composition, fixed top/right/width geometry, internal
scrolling and downward-only contextual height. Transform mode, coordinate space
and Exact/Details access are internal members, not detached capsules. Bottom
Exact/Details sheets hide conflicting Objects and history chrome for their full
entry/open/exit lifetime and restore it at the same bounds. IME/insets remain
root-owned and never select another right-control grammar.

**Measured against the OWNER-accepted `UI-SPEC-R0 Revision 1`**, transmitted to
this run through the coordinator's immutable OWNER AUTHORITY BLOCK (owner
acceptance 2026-08-29; coordinator-side provenance SHA-256
`9d032cadc35497a1fb73b185e8889be6031d00b5b0c95fadbcd2642f2a063151`). All **66**
cell × state host rows conform inside the ≤ 4 dp tolerance, intra-cell drift is
0 dp in all six cells, persistent resting chrome returns at Δ = 0 dp, intrinsic
hit targets clear 48 × 48 dp and the Vulkan surface stays full-window under the
IME. Evidence: `artifacts/uilayoutr2-correction/`; the superseded pre-correction
measurement stays at `artifacts/uilayoutr2-spec-closeout/`.

The verdict is the coordinator's, recorded from the handoff. The evidence
packages themselves deliberately record no verdict, and no owner statement
separately approving the final screenshots is claimed.

**Correction round 1 was consumed and round 2 was never used**; with the stage
closed, it is retired rather than pending. A **UI moratorium is active**: the
next product work is not another UI pass.

**UI-LAYOUT-R2 verification range:** baseline
`54c3dfa09c7b0fd9351f6c8967b199c3380e3c1a`; R2 implementation/test HEAD
`8d9ebbf0f73e278ed7383ae89c054afa2bcce9f3` (product layout
`27dc28d27cd44c75a552a64bf121ad64937c2b08`); measured closeout
`40e8b10a0b30da27ba1e60b22ab6f8667135d452`; **correction round 1 product/test
HEAD `6d05814ddb89ef4e06d2e78d62af78d8b2262af1`**; correction evidence/docs
`40d1f4490ef874344f0b101da2e290053028f675`; owner-review bundle
`71bab57e4e12a91c5f1c7b8cec6a5af9b92569b7`. The documentation/evidence commits
contain no product or test behavior change.

Before it, Stage 020R3 closed the one user-reachable
correctness gap Scale left behind: **a sculpt brush now measures in the
world/display metric**, so a round brush stays round on a body with a
non-uniform Scale. Stage 020R2 before it turned the axis-only gizmo into the
first **complete Construction transform workflow**: Move, Rotate and Scale, in
World or Local axes, with plane handles and a uniform handle.

The brush radius is authored in screen pixels and resolved to world meters, and
the distance every brush compares against it is now
`brushWorldDistance(model, d) = |R·S·d| = |S·d|` rather than the local `|d|` —
which is only the same thing at `S = (1,1,1)`. One shared metric and one shared
normal conversion (`brushLocalStepAlongNormal`, which carries a world deposition
through `R·S⁻¹` and back) serve all four brushes; there are not four
corrections. Nothing is baked: the Frozen Sculpt Mesh stays a byte-exact copy of
the Construction local mesh, the nine transform values are untouched by Start
Sculpting, Back to Construction and Resume Sculpt, and no Construction history
step is recorded by any of it. `ARCHITECTURE.md` owns the invariant.

| mode | handles | World | Local |
| --- | --- | --- | --- |
| Move | axis X/Y/Z, plane XY/XZ/YZ | yes | yes |
| Rotate | ring X/Y/Z | yes | yes |
| Scale | axis X/Y/Z, plane XY/XZ/YZ, uniform | no — a world-axis scale of a turned body is a shear | yes |

The authoritative `ConstructionTransform` now carries **nine** values: Position
in meters, Rotation in degrees and a unitless, strictly positive **Scale**, with
`Model = T · Rz · Ry · Rx · S`. Scale is a multiplier on a derived matrix and
never a primitive parameter, so it publishes no mesh revision and uploads
nothing, exactly as a move does. Zero and negative are refused: there is no
Mirror.

Stage 020R2 also **corrected the Stage 020 world-rotation defect**. A ring drag
no longer adds its angle to one Euler component — only correct when the other
two are zero. Every sample composes from the immutable start orientation
(`Relem·R_start` for World, `R_start·Relem` for Local) and decomposes back
through one **branch-continuous** helper, so a mixed orientation turns about the
axis the ring actually draws, a drag stays continuous across ±180°, past 360°
and past 720°, and the components a drag does not move read back exactly
unchanged.

One native `GizmoSession` still owns hit-testing, the three solvers, the frozen
drag basis and the transaction around one drag; it owns no transform of its own,
and Java owns no pivot, no solver, no basis and no captured pointer. **One drag
is exactly one Undo step** however many samples it carried; a tap records
nothing, and a cancel — an `ACTION_CANCEL` or a second finger — restores all
nine values exactly. Exact Transform is now Position + Rotation + **Scale**, one
atomic Apply. Renderer and picking are scale-correct: normals ride a real
inverse-transpose (`R·S⁻¹`) and a local ray parameter is still world distance.
**Surface Snap and Grid Snap remain absent** — the quantization and placement
seam exists and is the identity.
UI-LAYOUT-R2 is complete on the UI-ARCH-R1 boundary: automated verification
passed and the coordinator gave the visual PASS on 2026-08-30.
**Current Phase:** Phase 1 — Native Viewport
**Workspace:** `D:\TRAVELAPPS\ForgeShape`
**Current technical implementation baseline:** UI-LAYOUT-R2 (unified right context and
non-moving bottom-surface anchors) on UI-ARCH-R1 (bounded trailing-cluster
ownership refactor) on UI-LAYOUT-R1 (primary-surface policy,
static shell, precision surface, semantic contrast and gizmo legibility
correction) on top of
Stage 020R3 (world/display-metric sculpt
brush under a non-uniform Scale) and Stage 020R2 (full Construction
transform gizmo — Move/Rotate/Scale, World/Local, plane and uniform handles),
Stage 020 (Construction Move/Rotate gizmo) and Stage 019 (Construction transaction and
Undo/Redo), with DOC-R2 (current-truth documentation reconciliation) on top of
it, UI-R4C (final visual composition cleanup), UI-R4B (workspace
visual and motion
correction), UI-R4A (mobile workspace interaction), UI-R3R2 (approved
dark palettes + visual composition correction), UI-R3
(visual quality polish), DOC-R1 (documentation compaction), UI-R2 (workspace
composition redesign), INPUT-R1 (pointer semantics foundation), UI-R1C2 (world
grid + adaptive workspace), UI-R1C1 (motion + selection feedback), UI-R1B2 (theme
system), UI-R1B1 (visual foundation + start flow), Stage 017 (multi-object scene),
the Pre-017 Correctness Repair, Gate P1 (physical ARM64 closure), Stage 016-R2,
Stage 016 (Plane), Stage 015D (camera projection), Stage 015C-R (front-face
culling), Stage 015C (shading), Platform Fix P2, Stage 015B, Stage 014, the NDK
r29 migration (Gate P0) and the Owner Decision Baseline. Per-stage narrative
lives in Git history; only what still constrains the code is kept here.
`SCULPT-UNDO-R0` (`ARCH-OWNER-12` — a dedicated bounded volatile per-body Sculpt
stroke history, with Undo and Redo on the existing controls) sits on top of
`IMPORT-01B`/`UI-OWNER-45`.
**Next Stage:** **return this report to the ForgeShape coordinator for the
owner's real-device Sculpt Undo/Redo retest.** See *Next Stage*.

## Current state

A standalone Android application (`com.forgeshape.app`) that owns its own Vulkan
viewport: a Java shell owning no domain truth, a plain `SurfaceView`, a JNI
boundary carrying whole sections and semantic pointer samples, and a
platform-neutral C++17 domain beneath a native renderer that owns no geometry
truth. No Compose, no AndroidX, no third-party runtime library, no engine.

A scene holds several bodies, each with a nine-value placement — double-meter
Position, double-degree Rotation and a unitless positive Scale — and one of two
representations. A **Construction Body** is an exact primitive (Box, Cylinder,
Sphere, Cone, Capsule, Plane) with authoritative double-meter dimensions, and can
be frozen to a Frozen Sculpt Mesh and deformed with four brush tools. An
**Imported Mesh** (`IMPORT-01A`) is polygon geometry read from a `.glb`: it has
no parameters, so it has no *Shape* — but since `IMPORT-01B` it can be sculpted,
seeded from its own geometry, and it is moved, rotated, scaled, saved, undone and
reopened exactly like anything else. A body owns one SOURCE representation for
its whole life, nothing converts between them, and either kind may additionally
own a Frozen Sculpt Mesh. Any body but the last can be DELETED from the Objects
list (`UI-OWNER-45`), as exactly one Undo. Sculpt strokes have their own Undo and
Redo (`ARCH-OWNER-12`): a bounded, per-body, runtime-only history over completed
strokes, stepped by the same two chrome controls that step the project history
outside Sculpt, and never written to a `.forge` document.
Three approved dark appearances, two shading models, two projections, a world
reference grid, and an Editor Workspace that re-composes itself per window.

A project can be SAVED, reopened, autosaved and transferred. `.forge` v1 is
ForgeShape's own portable, versioned semantic document — a feature graph plus
placement, never a mesh snapshot. It is written to one app-private manual slot
by an explicit Save, to a SEPARATE recovery checkpoint by autosave, and to any
location the user picks through Scoped Storage; all three are the same canonical
bytes. Loading is all-or-nothing in every direction: decode and validate
entirely into temporary state, then replace the live project in one step, so a
refused file costs nothing. `DATA_PACKAGE_SPEC.md` owns the format.

The model can also be EXPORTED, one way, as a single glTF 2.0 binary (`.glb`)
written wherever the user picks through Scoped Storage. It carries every body's
triangles, crease-policy normals and placement, in metres, +Y up, with no
coordinate conversion of any kind: ForgeShape's world convention and glTF's are
the same convention. Each body's rotation and scale are BAKED into its vertices
and only its translation stays on the node (`ARCH-OWNER-07`), so the object
arrives already the shape it was made as, standing where it stood, on the pivot
it was rotated about. A body with a Frozen Sculpt Mesh exports that mesh, an
Imported Mesh exports the geometry it owns, and every other body exports
re-evaluated Construction geometry — no body is ever skipped. The exporter is
platform-neutral, uses no third-party interchange library, and is a READ: it
mints no revision, opens no transaction and touches neither `.forge` slot.

A `.glb` can also be IMPORTED (`IMPORT-01A`), the other way and just as
one-directionally: each supported top-level mesh node becomes one durable object
with a row in the Objects list, the geometry ends up in the `.forge` document,
and the source file is never consulted again. The whole import is one Undo, a
refusal changes nothing at all, and no material, texture, UV, colour, animation
or rig crosses in either direction. There is no OBJ and no FBX.
`ARCHITECTURE.md` owns the ownership map and every invariant;
`PRODUCT.md` owns the user-visible description; `README.md` owns build/run/verify.

**Blockers: none.** Every native, JVM, instrumented and guard suite is green,
including the authoritative exhaustive-sharded instrumented aggregate. The
measured anchor comparison against the OWNER-accepted `UI-SPEC-R0 Revision 1`
still passes 66/66 and the accepted right host did not move for E2E-R1A. The expected side of every anchor comparison came
from the accepted revision as transmitted in the correction prompt's authority
block, never from `EditorControlStyles`, `dimens.xml`, runtime bounds,
screenshots or the reference-only `docs/ui/wireframes/*.svg`. A **UI moratorium
is active**: no further UI pass is queued, and the shipped workspace arrangement
stands as accepted. Known costs and accepted debt are in *Technical Debt* — the
one carried visual item is **Sculpt Radius/Strength, deferred P2**, which was not
an R2 blocker; environment hazards are in *Known Issues*.

## Current interaction model (through UI-LAYOUT-R2)

Runtime-verified on `ForgeShape_Stage006` / `emulator-5580` in compact portrait,
short landscape and expanded/tablet windows —
compact portrait, compact landscape (short height) and an overridden
1600 × 2560 @ 240 dpi expanded — in all three appearances.

**The right context is one bounded surface with one composition owner.**
`WorkspaceTrailingHostView` owns the right-cluster children, their vertical order,
internal spacing/scrolling, Display suppression and fixed top/right/width placement.
The root owns native reads, commands, mode transitions, primary-surface
exclusivity and workspace insets. The host receives a derived presentation
snapshot and reports semantic user intent through callbacks; it holds no second
copy of product, transform, tool or precision state.

**One primary task surface owns context at a time.** (UI-LAYOUT-R1.) Objects,
Add Primitive, precision/details and Display are named explicitly as the four
primary surfaces; opening any one dismisses the other three through their normal
close paths, so focus, IME ownership and invoking-control state cannot drift.
The rule is deliberately not implemented as “close every anchored popover,” so
a future lightweight collision-free popover is not silently promoted into a
primary editor.

**Bottom and trailing safe zones are deterministic.** A bottom-sheet precision or
Sculpt-details surface owns the lower region for its full entry, open and exit
lifetime. The conflicting Objects/history row is hidden rather than translated,
then restored at its exact resting bounds after the sheet is absent. Display owns
the upper trailing region while open, so the unified host withdraws and returns
to the same external frame. Neither policy animates unrelated chrome.

**Sculpt follows the same static grammar.** Radius/Strength and the Tool Rail
share stable top anchors and aligned edge margins; changing track height no
longer recentres the brush panel. The direct sliders retain 48 dp touch widths,
and Sculpt details versus Objects follows the same primary-surface replacement
rule as Construction.

**The right host keeps one external frame and expands downward only.** Shape and
Transform are the high-level entries. Move/Rotate/Scale, World/Local where
applicable, and the Exact trigger are vertical internal members of that same
surface. Changing context preserves top/right/width; only height/bottom may
change. Short windows and IME use the host's single internal vertical scroll —
there is no horizontal, detached or duplicated selector grammar, and every
visible interactive target remains at least 48 dp.

**System Back closes what the user opened, before it leaves.** (UI-LAYOUT-R1.) With
any of the four dismissible surfaces open — Add Primitive, the Objects popover,
the exact values, the display settings — Back closes the topmost through the
workspace's own close path, playing the exit the surface already had, and the app
stays. The callback is registered only while there is something to dismiss, so
the platform's own exit behaviour, predictive back included, is untouched for the
press that really does leave.

**Nothing owns the bottom of the window at rest.** The Property Inspector used to
sit there permanently, collapsed to a full-width strip, and a collapsed strip is
still a structural claim on the edge of a viewport-first tool made by a surface
nobody asked for. It is now open with its whole body or absent entirely. The
resting workspace is the model, a transparent toolbar at the top, the Objects
capsule low on the leading edge, and the tool cluster on the trailing edge.

**Exact values did not become less reachable — they stopped owning the layout.**
The final entry inside the unified right host opens the precision surface for
whatever high-level entry the host is holding, names what it will open before it
is pressed, and is drawn active while it is up. What the user last decided is
remembered per mode and starts closed; no window size can open it by itself,
which is the rule the previous shell had and which meant a rotation could put a
surface on screen that had never been asked for.

**And the panel's commit no longer scrolls away.** (UI-LAYOUT-R1.) Apply is pinned
below the scrolling body rather than being its last row — it was three swipes
under an unmarked fold in compact portrait — and the body draws a fading bottom
edge while there is more below it. The unit chips moved inside the group they
convert, under the Position fields and headed *Position unit*, so nothing about a
unit is drawn near the Scale row, which is unitless by a hard product rule. A
long signed value stays whole: the field steps its type down before it drops
anything, and only if it still does not fit, and only while it is NOT being
edited, is it shortened from the END with an ellipsis, so the sign and the
leading digits are what survive. A tap on a populated field selects it, so the
first keystroke replaces rather than appends — `0` typed into produced
`0-98765.4321098` before. What is stored and submitted is the complete value in
every one of those states.

**Objects became a capsule that says which body is current.** It carries the
active body's name, sits in the same place in both modes, and the Global
Toolbar's Objects icon is gone with it — an icon can offer to open a list but
cannot answer the question the list exists for. It is withdrawn exactly when the
window gives the scene a permanent column, which is now also a function of the
mode: **Sculpt never gets a column**, because body switching and creation are both
refused while sculpting and the column's width displaced Radius and Strength onto
the model.

**Creation is a choice, not an append — and it is offered only where it can
succeed.** Both `+` controls open **Add Primitive**, which offers exactly six
tiles — Box, Cylinder, Sphere, Cone, Capsule, Plane — each an outlined silhouette
with its name. Choosing one calls `sceneAddBody()` and then that primitive's own
`applyConstruction*()` with parameters read back from the new body's own native
state; there is no Java-side generator and no second table of defaults. There is
no *Add from file* and no placeholder. **Neither `+` is drawn in Sculpt**:
`sceneAddBody()` refuses there, and UI-R4A drew the control anyway, so a user
could tap it, be shown six shapes, choose one and only then be refused. The
native guard is unchanged; what was removed is a path that had to fail.

**Every surface grows out of the control that opened it, and there is one
implementation of that growth.** `AnchoredSurfaceView` owns it for all four —
the scene list, the palette, the precision surface and the Display popover —
against one set of `ChromeMotion` constants: 190 ms in, 150 ms out, one ease-out
curve, uniform scale from 0.96, cancel-first, and reduced motion landing
instantly. It also fixes the defect the copies shared: the pivot is expressed in
the surface's own size, so the **first** open of each in a process grew from the
top-left of a zero-sized box; an open with no size is now staged and the growth
starts once the surface has been measured.

**The Tool Rail keeps saying which tool is held.** A rebuild — which the adaptive
pass triggers on any window crossing the short-height threshold — used to leave
no entry drawn active while the tool itself was unchanged below JNI. The rail
re-applies the last key it was given; it still decides nothing.

**The status line has a lifecycle, and it never instructs.** A verdict holds 5 s,
a rejection 10 s, a newer message cancels the older one's pending clear, and the
line then returns to what stands — which is nothing, drawn as no capsule at all.
The one standing message is the stale-source warning, which is a state rather
than a verdict. The ambient lines UI-R4A wrote on every refresh are gone, and
live brush values are no longer mirrored there. UI-R4C removed the last two
messages that were not verdicts at all: one written on entering Construction and
one on every Shape/Transform switch, both captioning the workflow. They put a
sentence across the top of the viewport in every resting Construction screenshot
the review saw, and what they said is answered by the held rail entry, the
toggle that names what it opens, the surface's own title and the Objects capsule.
There is no first-run help mechanism and UI-R4C deliberately did not build one.

**The mode transition is one control, and it is never abbreviated where the row
can carry it.** Its width is arithmetic on the row inside `GlobalToolbarView`'s
own measure pass — what is left after the utility group, an inline status floor
and the context label — rather than the single dp constant that truncated *Back
to Construction* on a phone, in short landscape AND on a tablet, two of them with
50 dp of unused row beside it. Only that label has a second form, `←
Construction`, taken where the row genuinely cannot carry the sentence; the
content description is the full wording either way. And where a `COMPACT` window
withdraws the context label the editing group holds one member, so the group
stops drawing itself and the member takes the capsule's own corner and depth — a
26 dp pill painted around a 22 dp button is a halo, and it made the transition
the heaviest object in a resting workspace whose subject is the model. The hit
area did not change: 8 dp of host went, not 8 dp of control.

**A context surface stands on the model, never on another control.** Add
Primitive is wider than the distance from the Objects capsule's `+` to the
trailing window edge, and the old clamp slid it under the tool cluster, leaving a
crescent of the precision toggle showing from behind it. The anchor now bounds it
at the cluster's own leading edge less the standard anchor gap, so the surface
moves and the live control keeps its place. The motion, the pivot and the z-order
are unchanged — a z-order fix would have left the control under the panel and
still taking touches.

**The precision surface ends on a row, not through one.** `PrecisionScrollView`
rounds the visible body down to the last whole row inside the cap. It caps
nothing, changes no content and no scroll range, and does nothing when everything
fits.

**48 dp is the interactive floor**, raised from 44 dp, and it is the hit area
rather than the glyph. The 8 dp the toolbar's two icon controls gained still
comes out of the mode-transition button's width budget rather than out of an
icon control — an icon control past the window edge is unreachable, a button has
width to give.

**The Tool Rail carries only tools that work.** `ToolRailView` has no notion of a
reserved entry at all; Construction's two entries are *Shape* and *Transform*.
Transform holds both front ends to one placement — the direct handles in the
viewport and the exact numeric surface, titled *Exact Transform — Body #N*. The
one approved-but-unimplemented control left in the product is the global
`Export`, drawn recessed.

**A standing fault stays visible without a resident panel.** The stale-source
warning is still in the Sculpt context surface beside the action that resolves
it, and is also the standing status message in Sculpt, re-asserting itself after
any transient that covered it.

**No user-facing string says *Freeze* or *frozen*.** The primary Construction →
Sculpt action reads **Start Sculpting**; the guarded destructive act reads **Reset
Sculpt from Shape…**; the mesh is a **sculpt mesh**. *Back to Construction* and
*Resume Sculpt* are unchanged in wording and in behaviour, the confirmation is
unchanged, and the reversible semantics are unchanged. The domain, this document,
`ARCHITECTURE.md` and the view ids keep `Freeze` / `Frozen Sculpt Mesh`, because
that names what `SculptMesh::freezeFrom` does. `UIR4B-15` scans every `R.string`
and fails on any user-facing occurrence.

**One visual vocabulary in every window.** The Objects column, the docked Tool
Rail and the docked precision surface were opaque, flat and squared against a
window edge; all three are now inset, rounded on every corner and raised, exactly
as on a phone. Docking decides position, never material. A selected control is
fill-led with no accent hairline, and its corner is **concentric** with its
host's — `inner = outer − padding` — so no crescent of capsule or rail shows at
the ends of a group.

**Nothing below JNI moved.** Opening and closing the scene list, the palette and
the precision surface, switching rail context, and all three appearance switches
produced **zero** `MESH_UPLOAD_OK` and zero publications. A HOME/resume and a
rotation round trip produced zero as well. Verified by logcat. A real sculpt
stroke produced 29, which is the stroke working.

**Start Sculpting → stroke → Back → Resume loses nothing.** Verified at runtime
and guarded by `EditorWorkspaceSculptRetentionTest` (cited as `UIR4B-20`): after a
real Grab stroke, *Back to Construction* shows the Construction Source unchanged
(correct — sculpting never writes it), and *Resume Sculpt* returns the same
revision, vertex count, index count, stroke history and ObjectId. The UI-R4B copy
change touched no id, no native call and none of those assertions. The one-way
project bridge is **not** implemented and this path is still the only thing
keeping that work.

## Current visual state (UI-R3R2 palettes, UI-R4B/UI-R4C composition)

**The appearance set is three owner-approved DARK palettes**, and there is no
light one. Warm Graphite (`#302E2B` ground, the default), Neutral Charcoal
(`#26282A`) and Light Charcoal (`#3C3F41`) are chosen from a named list in the
Display popover's Appearance group. Twelve values of each are approved and
reproduced exactly; everything else is a derived neighbour. `ViewportBackground`
names the three grounds, and `gridLineColor` authors a palette per ground because
the alpha that is a whisper over `#26282A` is invisible over `#3C3F41`. Every
surface UI-R4A added maps through the same semantic roles; no colour is written
in Java anywhere in them.

**The Global Toolbar is not a bar.** A transparent container holding two floating
capsules with the model visible between and behind them, and the status line in a
small capsule sized to its own text. The container deliberately does not consume
touches; each capsule does, and `chromeRects()` reports the capsules rather than
the container so the viewport-floor measurement describes what is painted.

**Three material tiers, states fill-led.** Floating capsules are the only
translucent tier; context panels and the precision surface are opaque. Resting
outlines are gone from every control except the two that earn one — a numeric
field and the stale-source warning — and a reserved control is recessed rather
than boxed. The selected state carries **no accent hairline at all** since
UI-R4B: the fill and the brightened label are the signal, and the accent stays
spent on a primary commit and a focused field. `fsAccentBorder` is consequently
unreferenced and deliberately still declared, because those values are approved.
`fsTextOnPrimary` is an ink rather than white, because `#4C8FD6` carries white at
only 3.4:1.

**No chrome column spans the window, and none of them is a slab.** The compact
precision surface is an inset floating sheet with the viewport visible around it;
the Objects column and both side placements wrap their own content, hang from the
top, and — since UI-R4B — wear the same inset, all-round, raised material they
wear on a phone. `sideDockWidthDp` is 30 % capped at 340 dp because at 28 %/320
the docked panel clipped "Cylinder" to "Cyl".

**A control's corner is concentric with its host's — and a group of one has no
host.** `radius_control_inset` (22 dp) inside a 26 dp capsule with 4 dp of
padding, `radius_rail_entry` (20 dp) inside the rail's own 26 dp capsule with
6 dp. Two corners that do not share a centre leave a crescent of the host
showing, which reads as a rendering fault. UI-R4C added the other half of the
same rule: a capsule is a relation between controls, so where the editing group
holds one visible member it stops painting and the member takes
`radius_capsule` and the floating elevation itself (`bg_pill_primary` /
`bg_pill_tonal`, the same approved fills as the member forms they replace). No
palette value moved.

**The short-height rail keeps its icons.** The compact entry shrinks the glyph to
18 dp instead of dropping it; icon and caption both measure inside the 48 dp
entry.

**What the approved palettes cost — and the two anchors UI-LAYOUT-R1 was told to
move.** The twelve anchors were not the UI layer's to move, so the theme suite
asserted what they delivered rather than a number they would have to be
redesigned to reach: primary text and every typed value at WCAG AA (4.5:1),
secondary captions at 3.0:1 (measured 3.6–4.7), verdicts at 2.4:1 against the
precision surface. That 2.4:1 was the tightest number in the product — Light
Charcoal's error red on its own precision surface — and it was below AA, carried
deliberately as debt.

**UI-LAYOUT-R1 was instructed to correct it, and did, for the two roles that carry
meaning rather than decoration.** `*_text_secondary` (field captions, section
headings, slider labels, the active body's name) and `*_text_error` (every
refusal the product reports) were lightened until each clears **4.5:1 on all six
grounds it is drawn on** — the viewport ground, the chrome surface, the floating
material, the precision surface, a control fill and a field well. The worst case
in the product is now 4.60:1, up from 2.17:1. Each hue is kept, so the
appearances still read as themselves, and the ten other anchors are untouched.
`UILR1-11` measures all 36 combinations on the device and
`EditorWorkspaceThemeTest` holds the caption role to the body-text target it was
excused from. **The remaining 2.4:1 floor applies only to the two verdict colours
UI-LAYOUT-R1 was not asked to move** — success and measure — and stays recorded here as
the owner's to overrule.

**Stylus / S Pen on real hardware remains UNVERIFIED.** Tool type, pressure and
tilt are carried end to end and verified synthetically (`MotionEvent.obtain` with
a tool type and an `AXIS_TILT` value, the same path an S Pen drives); closing it
needs a person physically moving a pen. Nothing consumes those fields, so no
behaviour depends on the gap.


## Durable constraints from closed stages

Only facts that still constrain the code. Everything else is in Git history, and
every architectural invariant is owned by `ARCHITECTURE.md`.

**The ARM64 barycentric tolerance must not be removed or tightened.**
`intersectRayTriangle` widens both containment bounds by
`kBarycentricEpsilon = 1e-6f`. A ray landing on an edge two triangles *share* has
a barycentric coordinate that is mathematically exactly 0, and its sign is decided
purely by rounding; if it rounds negative for one triangle it does for its
neighbour too, so both reject a ray that geometrically hits. The rounding is
ABI-dependent — no `-ffp-contract=off` is set, so Clang contracts dot/cross
products into fused multiply-adds on arm64-v8a where baseline x86-64 cannot. An
on-device probe measured the failing ray at `u = -9e-9`. Product-visible: the
centre of a Plane *is* the shared diagonal of its two triangles, so tapping the
middle of a Plane selected nothing. Pinning the FP model instead would only
re-hide the same knife-edge geometry.

**Self-tests and instrumented tests must not depend on live process-scoped
state.** Every native case publishes its own fixture, and the scene suite builds
its own `ConstructionScene`. The scene is process-scoped, so bodies accumulate
across an instrumented run and the product may be left in Sculpt by an earlier
case: no instrumented test may assume a body count, which body sits at the
origin, or which mode is current. `UI-OWNER-45`'s Delete does not change that —
a suite that uses it to isolate a body still resolves ids from the live scene
rather than assuming them.

**The device verifier is mechanical and needs no device.**
`scripts\verify-device-guards.ps1` runs `DEV2-01`..`07` and `DEV3-01`..`06`: it
recognises a bare `adb` in any form, scans `.ps1`/`.cmd`/`.bat`/`.sh` and the
Gradle files, detects an executable `connected*AndroidTest` fan-out, proves an
argument array actually carries `-s` rather than trusting its variable name, and
reports which surfaces it scanned so a check that covered nothing cannot pass.

**Heavy-mesh ladder, measured on a physical Galaxy S25 Ultra (Gate P1).**
Single-run figures from one device; existence evidence that the mandatory tiers
work on physical ARM64, not a supported capacity limit.

| tier | vertices | triangles | gen ms | publish ms | render build ms | pick | PSS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ~10k | 10086 | 19200 | 2.763 | 1.695 | 19.577 | tri 15592 | 219 MB |
| ~50k | 49686 | 97200 | 17.776 | 7.880 | 72.204 | tri 78824 | 231 MB |
| ~100k | 99846 | 196608 | 32.405 | 15.426 | 104.387 | tri 159210 | 248 MB |

Sculpt at ~100k: freeze 110.126 ms building 592896 adjacency entries, two real
Grab strokes, every post-stroke upload `reuse`, PSS 264 MB. No crash, OOM, ANR or
thermal symptom at any tier. The last stable mandatory tier is ~100k; 250k/500k
were not run. Also closed under Gate P1: `arm64-v8a` packaged beside `x86_64`,
both `.so`s at 0x4000 ELF `LOAD` alignment with `zipalign -P 16 -c` OK, and a
debug-only Vulkan validation session with **zero ForgeShape-caused messages** of
any severity (the layer was adb-pushed through Android's own per-app GPU debug
layer settings, never bundled, never committed, and removed afterwards).

## Owner Decision Baseline

Active owner decisions are identified by `UI-OWNER-*`, `ARCH-OWNER-*`,
`INPUT-OWNER-*` and `DOC-OWNER-*`. Recorded 2026-08-20.

**Implemented:** UI-OWNER-01 (the shell), UI-OWNER-02 (compact / medium /
expanded), UI-OWNER-03 (Export as a global action — drawn reserved when the
decision was recorded, and working since Stage 023 in the same place),
ARCH-OWNER-07 (the baked static interchange transform, E2E-R1C-C1),
ARCH-OWNER-08 (the diagnostic imported mesh preview, GLB-IMPORT-R0),
ARCH-OWNER-09 (the widened external static GLB preview, GLB-IMPORT-R1),
UI-OWNER-05 (the destructive re-Freeze guard) and UI-OWNER-06 (stylus-friendly,
no pressure). **Still a decision only, with no behaviour and no drawn control:**
UI-OWNER-04 — Sketch and Extrude have no implementation whatsoever and no entry
in the Tool Rail.

Bare `D1`–`D6` decision numbers are retired and non-authoritative. No stage gate,
acceptance table or preflight may cite a bare `D` number.

| ID | Decision | Approved value |
| --- | --- | --- |
| UI-OWNER-01 | Shell family | **Forge Shell** — a modernized viewport-first shell: direct edge controls and a Tool Rail for Sculpt, the same shell language plus a contextual exact-value Property Inspector for Construction. One shell; mode and tool decide content, never structure. |
| UI-OWNER-02 | Device scope | **phone + tablet** — Stage 015B implements adaptive compact / medium / expanded behaviour now. A tablet may dock more surfaces but must not become desktop-CAD clutter. |
| UI-OWNER-03 | Export placement | **global action** — Construction, Sculpt and UV remain editing contexts; Export is not one of them and later opens its own output surface. |
| UI-OWNER-04 | Sketch + Extrude | **approved as an iterative CAD workflow**, not implemented and not part of Stage 015B geometry. See below. |
| UI-OWNER-05 | Confirmation before a destructive re-Freeze | **yes**, and **only** when existing Frozen Sculpt Mesh edits would actually be replaced. A normal Resume Sculpt has no confirmation — it destroys nothing, and guarding it would train the user to dismiss the guard that matters. |
| UI-OWNER-06 | Stylus pressure in Stage 015B | **no** — deferred to a dedicated Sculpt stage. 015B must still be stylus-friendly and preserve a clean path for pressure, tilt and hover. |
| ARCH-OWNER-01 | Future Apple portability | **required architectural constraint**. See below. |
| ARCH-OWNER-08 | Diagnostic imported mesh preview | **implement a session-only Imported Mesh Preview**, approved 2026-08-31, to separate an exporter defect from an external-tool presentation difference. Parse actual GLB bytes independently of the writer; the preview is not `.forge` project truth; no ObjectId / Construction Source / Sculpt / history semantics; no manual-save, autosave or recovery inclusion; no imported-mesh editing; no Blender-specific axis conversion. Broad/durable import remains post-MVP (`IMPORT-01`). Implemented in GLB-IMPORT-R0. |
| ARCH-OWNER-09 | External static GLB preview compatibility | **widen the session-only Imported Mesh Preview to ordinary static external GLB files**, approved 2026-08-31. Static GLB 2.0 with an embedded BIN; a node `matrix` or ordinary TRS; several TRIANGLES primitives; a missing NORMAL may be generated; COLOR/TEXCOORD attributes may be validated and ignored; material `doubleSided` affects preview culling; `extras` ignored; no Blender-specific axis conversion; the preview stays separate from project truth in every respect R0 established. Production import remains post-MVP (`IMPORT-01`), and OBJ and FBX stay absent. Implemented in GLB-IMPORT-R1. |
| ARCH-OWNER-07 | Static interchange transform | **bake rotation and scale into the exported geometry; keep translation and pivot at the node.** Approved 2026-08-31, after the owner reviewed the first exported files in Blender. Exported node rotation is identity/omitted and node scale is 1/1/1 omitted; geometry is not recentred; metres 1:1, +Y-up, right-handed and zero axis conversion are unchanged; Construction/Sculpt representation selection is unchanged; `.forge` authored transforms are unchanged. Supersedes the Stage 023 node-matrix policy for static GLB. Implemented in E2E-R1C-C1. |
| INPUT-OWNER-01 | Stylus-first interaction | **required product/architecture constraint**. See below. |
| DOC-OWNER-01 | Clear naming and code comments | **required documentation/maintainability rule**, recorded durably in `CLAUDE.md`. |

**UI-OWNER-04 in detail.** A Construction Body may begin either from an exact
primitive **or** from a 2D sketch: standard sketch planes (later on planar
Construction faces), multiple sketches on different planes, Line/Polyline,
Rectangle, Circle, select/delete, grid and snap, exact numeric entry, closed-
profile detection and validation, and repeated Sketch → Extrude workflows. The
first Extrude vertical slice supports **New Body**; Extrude **Add** and **Cut**
are required integration **after** boolean infrastructure exists. The result stays
Construction source and history until the user explicitly starts sculpting. **This
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
| C++ / STL | C++17, `c++_static`; one `.so` per filtered ABI — `lib/x86_64/` and `lib/arm64-v8a/libforgeshape_native.so` |
| Shaders | `glslc` at `<ndk>/shader-tools/windows-x86_64/glslc.exe`, AOT from CMake |
| System images | only `system-images;android-36.1;google_apis_playstore;x86_64` and the 16 KB `…;google_apis_playstore_ps16k;x86_64` used by `ForgeShape_16K` |

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
| Add Primitive creates a body that IS the chosen primitive and selects it; deterministic insertion order across edits and selection | VERIFIED |
| Resting workspace has no surface spanning the bottom edge, in Construction and in Sculpt, in every window | VERIFIED |
| Every context surface is anchored to, and grows from, the control that opened it | VERIFIED |
| Exact Shape and Exact Transform reachable in one contextual action; same validation, units, active-body routing and native read-back as before | VERIFIED |
| Legacy Freeze → stroke → Back → Resume returns the identical frozen mesh; the Construction Source is untouched | VERIFIED |
| Primitive and transform edits reach only the active body; A↔B round-trips the exact spec and placement | VERIFIED |
| Per-body mesh publication: each body its own revision chain; A's edit cannot replace B's mesh | VERIFIED |
| Renderer draws every body with its own transform and its own GPU buffers | VERIFIED |
| **CAD Body** (`CAD-R0-A1A2`): New Sketch from the creation palette, a plane chooser (XY / XZ / YZ), an orthographic plane-aligned view that gives the user's pose back afterwards | VERIFIED |
| Sketch tools Select / Line / Polyline / Rectangle / Circle through real MotionEvents; one finger draws and never orbits, two fingers pan and pinch; endpoint-then-grid snapping to EXACT coordinates; typed values never snapped | VERIFIED |
| Finish Sketch finds closed profiles (rectangle, circle, closed polyline, loop of lines) and refuses open, forked, crossing, zero-area and nested profiles by name, staying editable | VERIFIED |
| Extrude as a New Body: a typed depth, a direction, one CAD Body as one Undo, watertight and canonically wound on all three planes; Undo removes the whole body and Redo restores the same one | VERIFIED |
| A CAD Body's rectangle sizes, circle radius and depth editable later, each one history step, regenerated atomically, placement untouched; the ordinary Objects row, gizmo and export | VERIFIED |
| `CADB` `.forge` section: Save / Open / Save Copy / autosave / recovery / fingerprint carry the sketch and the extrusion; a reopened CAD Body is still editable truth; corrupt CAD records refused fail-closed; the twelve older fixtures unchanged | VERIFIED |
| A cancelled sketch changes nothing: bytes, fingerprint and history identical | VERIFIED |
| Import, Sculpt and Sculpt Undo/Redo unaffected beside a CAD Body; CAD → Sculpt deliberately refused by name | VERIFIED |
| A project is saved to one app-private `.forge` slot and reopened after the process is killed | VERIFIED |
| A reopened project keeps every body's ObjectId, scene order, active body, active kind, ALL SIX remembered parameter sets, and its exact placement including 370 degrees and a non-uniform scale | VERIFIED |
| A reopened project regenerates every Construction mesh: no mesh, revision or GPU data is read from the file, and the file is exactly the size the semantic arithmetic predicts | VERIFIED |
| A sculpted body reopens with its vertices bit-identical, its topology, its `renderBothSides`, its stale-source state and its edited state; normals and adjacency are rebuilt | VERIFIED |
| A project saved while sculpting reopens sculpting; the Construction Source companion survives, and Back/Resume still work over it | VERIFIED |
| A damaged, truncated, unsupported-major or non-project file is refused explicitly and leaves the scene, every sculpt mesh, the mode and the session history untouched | VERIFIED |
| A successful load starts a fresh Construction history; the next edit and undo act on the loaded scene | VERIFIED |
| The id allocator is pushed forward past a loaded project, so a later creation cannot collide with a loaded body | VERIFIED |
| The `.forge` encoder is byte-deterministic and matches an independent second implementation of the same specification | VERIFIED |
| Work is checkpointed automatically to a SEPARATE recovery file, in the same canonical `.forge` document, without touching the manual slot | VERIFIED |
| Repeated edits coalesce: twenty dirty generations cost one write, and an unchanged, refused or identical edit costs none | VERIFIED |
| A failed checkpoint leaves the previous valid one byte-identical | VERIFIED |
| Work never explicitly saved survives process death and is offered back by a recovery question on a cold launch | VERIFIED |
| Nothing replaces the live project before the user chooses; Discard leaves an explicitly saved project byte-identical | VERIFIED |
| A corrupt or unsupported-major checkpoint is quarantined once, changes nothing, and is never offered again | VERIFIED |
| A project can be written to, and read from, any location through Android Scoped Storage, in the same `.forge` format | VERIFIED |
| Opening a file makes that project live without writing the internal manual slot | VERIFIED |
| No `Uri`, authority, filename or path reaches the project document — a project re-encodes to the original bytes exactly | VERIFIED |
| A lost GPU device rebuilds the device from CPU truth, with every project value and the encoded document bit-identical across it | VERIFIED |
| Diagnostics are a bounded local ring carrying no geometry, no `.forge` bytes and no path, shareable only through a document the user picks | VERIFIED |
| ForgeShape holds no network permission and cannot make a request | VERIFIED |
| Editing A rebuilds and uploads nothing for B | VERIFIED |
| Only the selected body is highlighted | VERIFIED |
| Becoming selected gives a short acknowledgement pulse that decays to a much lower resting tint | VERIFIED |
| A tap on an already-selected body does not re-pulse; only a change in selection truth does | VERIFIED |
| Selection feedback mints no revision, rebuilds no render mesh and uploads nothing, in either appearance | VERIFIED |
| Two bodies' pulse states are independent; deselecting one does not disturb the other | VERIFIED |
| The selected body stays obvious and the model's form stays readable in every appearance | VERIFIED |
| A world reference grid on the XZ plane at y = 0, switchable from the Display popover's View group | VERIFIED |
| The grid is ON by default and its choice survives rotation, Activity recreation and HOME/resume | VERIFIED |
| Grid lines at 1 m, a 5 m major rhythm, a 20 m extent that fades radially rather than ending at a border | VERIFIED |
| The X and Z axes are told apart by a faint warm/cool lean, not by saturated primaries | VERIFIED |
| The grid is readable over every one of the three grounds, and never competes with the model for the eye | VERIFIED |
| The grid is correct in Perspective and in Orthographic, and changes neither projection nor camera pose | VERIFIED |
| A Construction Plane at world y = 0 shows no z-fighting; two consecutive static frames are byte-identical | VERIFIED |
| The grid never enters the scene, a snapshot, picking, Freeze, Sculpt or a revision | VERIFIED |
| Toggling the grid rebuilds no render mesh, uploads nothing and mints no revision | VERIFIED |
| The grid's vertices are uploaded exactly once per Vulkan device and survive rotation and resume | VERIFIED |
| Expanded windows give Objects a persistent column beside a docked Property Inspector | VERIFIED |
| Compact and medium keep the current viewport-first model; Objects stays in the shape editor | VERIFIED |
| One Objects section, re-parented — no second Java list and no second selection truth | VERIFIED |
| Row tap, viewport pick and creation stay in sync from whichever surface Objects is on, and the capsule names the same body | VERIFIED |
| ~20 bodies stay listed, scrollable in the column's own container, and selectable | VERIFIED |
| The SurfaceView is the whole window in every layout mode; docking never resizes the render target | VERIFIED |
| Reduced motion goes straight to the resting tint and runs no pulse at all | VERIFIED |
| Chrome hide/restore and every context surface are short, interruptible and always settle at a legitimate resting state | VERIFIED |
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
| Start Sculpting / Back to Construction / Resume Sculpt without re-freezing | VERIFIED |
| Stale-source policy: Construction change never touches the sculpt mesh | VERIFIED |
| Four sculpt tools (Grab, Clay, Smooth, Inflate) on one shared kernel | VERIFIED |
| Shared Radius and Strength, clamped, unchanged by tool switching | VERIFIED |
| Clay ≠ Inflate, measured (start-normal vs current-normal) | VERIFIED |
| Pending-then-promote: multi-touch navigation cannot mutate the sculpt mesh | VERIFIED |
| Sculpt topology fixed; buffers reused, never reallocated during a stroke | VERIFIED |
| Construction Source bit-identical after sculpting with all four tools | VERIFIED |
| Lifecycle: shape, placement, identity, unit, mode, tool, sculpt, camera and selection survive home/resume with no re-upload | VERIFIED |
| Three explicit approved appearances — Warm Graphite, Neutral Charcoal, Light Charcoal — chosen from a named list in the Display popover's Appearance group | VERIFIED |
| Warm Graphite is the product default and what a fresh process wears; a process kill returns to it | VERIFIED |
| Each appearance changes the VIEWPORT ground as well as the chrome (`#302E2B` / `#26282A` / `#3C3F41`), and every ground in the set is dark | VERIFIED |
| A theme switch publishes no mesh, mints no revision and causes zero GPU upload or render-mesh rebuild | VERIFIED |
| Scene, active ObjectId, primitive spec, placement, product mode and Frozen Sculpt Mesh survive the switch bit-identically | VERIFIED |
| The appearance survives rotation and HOME/resume; the start chooser does not reappear because of it | VERIFIED |
| The UI session — display unit, whether the precision surface was asked for, Tool Rail entry — survives the recreation that applies a theme | VERIFIED |
| Icons, pressed feedback, active-not-by-colour-alone and 48 dp hit areas all hold in all three dark appearances | VERIFIED |
| Property Inspector values, labels and verdicts meet WCAG AA contrast in every one of the three appearances; its surfaces stay opaque | VERIFIED |
| Start chooser: New Project offers exactly Construction/CAD and Sculpt, over the live viewport | VERIFIED |
| The start question is asked once per process; rotation, HOME/resume and Activity recreation do not re-ask; a process kill does | VERIFIED |
| Choosing Construction creates no body and changes no active body — the default Body is already there | VERIFIED |
| Choosing Sculpt lands on a sphere-derived Frozen Sculpt Mesh (482 : 2880) through the existing apply + Freeze path, with `freezes=1` | VERIFIED |
| After a direct Sculpt start, Back shows the exact sphere Source and Resume returns the same frozen mesh without re-freezing | VERIFIED |
| Every icon is a local vector drawable; no chrome control is a Unicode glyph | VERIFIED |
| Chips, rail entries, Objects rows and icon buttons show immediate pressed feedback | VERIFIED |
| Active state is fill-led with a brightened label, never colour alone and never an accent hairline or border as the active signal | VERIFIED |
| Icon-only controls measure at or above the 48 dp hit-area floor in a compact window | VERIFIED |
| A small drift on a Tool Rail entry selects that tool; a real scroll selects nothing | VERIFIED |
| Three-level corner radius and depth in every window; docking decides position, never material — the Objects column, the Tool Rail and the precision surface are inset, rounded on every corner and raised alike | VERIFIED |
| Editor Workspace: Global Toolbar, Tool Rail, Property Inspector, direct brush controls | VERIFIED |
| Adaptive layout: compact portrait, phone landscape, expanded/tablet, decided by window dp | VERIFIED |
| Landscape occlusion: 0 % unoccluded viewport → **60.1 %**, status line on screen | VERIFIED |
| Inspector collapse and chrome hide restore viewport area (57.5 % → 82.6 % → 100 %) | VERIFIED |
| Edge-to-edge with WindowInsets on chrome only; IME never resizes the Vulkan surface | VERIFIED |
| The trailing tool cluster is top-anchored, and a contextual control appearing or disappearing moves no persistent one — measured at zero pixels across Transform selection, all three transform modes and the precision surface opening | VERIFIED (UI-LAYOUT-R1) |
| No trailing control drops below 48 dp, and none leaves the tree, in compact portrait, short landscape, at `font_scale 1.3` or with the keyboard up; the Tool Rail absorbs the deficit and scrolls | VERIFIED (UI-LAYOUT-R1) |
| Construction and Sculpt use one `WorkspaceTrailingHostView` surface with invariant top/right/width; context is vertical inside it and may grow only downward | VERIFIED (UI-LAYOUT-R2) |
| Exact Shape/Transform and Sculpt Details bottom sheets hide conflicting Objects/history chrome for entry/open/exit and restore its exact resting bounds; they never push it upward | VERIFIED (UI-LAYOUT-R2) |
| IME keeps the right host vertical and at the same external frame; animated insets are root-owned and no horizontal/detached selector grammar exists | VERIFIED (UI-LAYOUT-R2) |
| The right host's **absolute** placement matches the owner-accepted layout within ≤ 4 dp | VERIFIED (UI-LAYOUT-R2 correction 1) — 66/66 cell × state rows against `UI-SPEC-R0 Revision 1`, 0 dp intra-cell drift |
| A side-placed Exact/Details panel never translates the right host: the host is the trailing child of the row they share, so its own margin is the only term in its trailing inset | VERIFIED (UI-LAYOUT-R2 correction 1) |
| System Back dismisses the topmost open context surface — Add Primitive, the Objects popover, the exact values, the display settings — playing that surface's own exit, and leaves the app only when none is open | VERIFIED (UI-LAYOUT-R1) |
| A long signed exact value keeps its sign and leading digits: the type steps down to fit, and a shortened display is shortened at the END, never the start. The complete value is what is stored, parsed and applied | VERIFIED (UI-LAYOUT-R1) |
| A tap on a populated numeric field selects it, so the first keystroke replaces; no concatenation of the old value and the new can occur | VERIFIED (UI-LAYOUT-R1) |
| Apply is pinned below the precision body in every placement and is on screen with no scrolling, keyboard up or down; the body fades its bottom edge while there is more | VERIFIED (UI-LAYOUT-R1) |
| The mm/cm/m chips sit inside the Position group and are headed *Position unit*; nothing about a unit is drawn near the unitless Scale row | VERIFIED (UI-LAYOUT-R1) |
| The secondary and error text roles clear 4.5:1 against all six grounds they are drawn on, in all three appearances, with primary above secondary above disabled | VERIFIED (UI-LAYOUT-R1) |
| The gizmo draws four kinds of handle as four kinds of mark — bundled axis, closed arrowhead or cube, crossed plane square, neutral pivot cross, doubled uniform cube — with hit radii, handle sets, grab points, solvers and space rules unchanged | VERIFIED (UI-LAYOUT-R1) |
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
| The scene list is one view with one owner and two hosts; no window shows it twice and no Java copy of ObjectId or selection exists | VERIFIED (UI-R2) |
| A compact window reaches the scene in one tap and gives the viewport back in one more; closed, the panel costs the model nothing | VERIFIED (UI-R2) |
| Opening the scene panel, selecting a body and collapsing the inspector publish no mesh and mint no revision | VERIFIED (UI-R2) |
| Tool type, pressure and tilt cross MotionEvent → SurfaceView → JNI → native intact, and stay with the right pointer in a multi-pointer event | VERIFIED (INPUT-R1) |
| Carrying stylus data changes no brush result: same stroke, opposite pressure and tilt, bit-identical vertices for all four tools | VERIFIED (INPUT-R1) |
| One native-owned Construction history: Java holds no mirror scene, no snapshot list and no depth counter | VERIFIED (Stage 019) |
| Exact Shape Apply is one atomic undo step whatever it changed; a rejected or identical Apply is none | VERIFIED (Stage 019) |
| Exact Position+Rotation+Scale Apply is one atomic step; all nine values move together, 370° survives un-canonicalized, and a refused scale leaves the position untouched and records nothing | VERIFIED (Stage 019, extended Stage 020R2) |
| Add Primitive is ONE creation transaction: undo removes the body with no default-Box remnant | VERIFIED (Stage 019) |
| Redo of a creation restores the same ObjectId, primitive, parameters, placement and scene position, and re-selects it | VERIFIED (Stage 019) |
| Interleaved edits across bodies undo and redo in chronological order, each touching only its own body; ordered ObjectIds preserved | VERIFIED (Stage 019) |
| A real new edit clears the redo; a rejection, a no-op and an empty transaction do not | VERIFIED (Stage 019) |
| begin → many updates → commit is exactly one step; cancel restores the pre-state and records none | VERIFIED (Stage 019) |
| History is bounded (64 steps) in memory, evicting oldest-first deterministically; no disk history | VERIFIED (Stage 019) |
| Selection, editing context, surfaces, display unit, appearance, Grid, shading, chrome-hide and rotation write no history | VERIFIED (Stage 019) |
| History survives rotation and HOME/resume; the rebuilt chrome reads its enabled state from native, never from memory | VERIFIED (Stage 019) |
| A sculpt stroke writes no Construction history, and a Construction undo leaves the sculpt revision, counts, strokes and identity alone — only the existing stale-source flag moves | VERIFIED (Stage 019) |
| Construction Undo/Redo are withdrawn in Sculpt, and the native guard refuses them there regardless | VERIFIED (Stage 019) |
| Transaction wrapping added no geometry work: one Apply is still one publication, a commit is none, a placement drag publishes nothing | VERIFIED (Stage 019) |
| **A new session's Construction history is empty before the first user act, whichever way the start question is answered** — the direct-Sculpt path's sphere seed runs inside a production initialization boundary and is not an Undo step | VERIFIED (Stage 020) |
| **Move handles for axis X/Y/Z and plane XY/XZ/YZ, in World or Local; an axis drag moves along that basis direction alone and a plane drag never leaves its plane** | VERIFIED (Stage 020R2) |
| **Rotate rings for X/Y/Z in World or Local, composed as matrices from the immutable start orientation (`Relem·R_start` / `R_start·Relem`) — correct on a mixed orientation, where a per-Euler-component add is not** | VERIFIED (Stage 020R2) |
| **Euler decomposition is branch-continuous: no ±360 jump, a drag keeps counting past 360° and past 720°, a singular pitch stays finite and orientation-correct, and an untouched component reads back exactly unchanged** | VERIFIED (Stage 020R2) |
| **Scale handles for axis X/Y/Z, plane XY/XZ/YZ and uniform, Local only; one screen-space formula, factor 1 at zero drag, monotone, never at or through zero, ratios preserved by uniform** | VERIFIED (Stage 020R2) |
| **Scale is a unitless multiplier on a derived matrix: `Model = T·Rz·Ry·Rx·S`, no primitive parameter touched, no revision published; zero and negative refused, so there is no Mirror** | VERIFIED (Stage 020R2) |
| **Renderer and picking are non-uniform-scale correct: normals ride `R·S⁻¹`, and a local ray parameter is still world distance because the local direction is never renormalized** | VERIFIED (Stage 020R2) |
| **The gizmo is sized from the camera alone — a stretched body does not stretch its own instrument, and the drag basis is scale-free** | VERIFIED (Stage 020R2) |
| **A sculpt brush measures in the world/display metric: the affected set is the world ball `\|S·d\| < R`, so a round px brush stays round on a body with a non-uniform Scale, including one that is also rotated** | VERIFIED (Stage 020R3) |
| **All four brushes share ONE metric and ONE normal conversion; Clay and Inflate deposit a world amount along the DISPLAYED normal (`R·S⁻¹`), Grab's camera-plane delta was already scale-correct, and Smooth's neighbour-mean interpolation is affine and needs none** | VERIFIED (Stage 020R3) |
| **The screen-pixel radius contract survives Scale: the same nominal px radius resolves to the same world radius before and after a body is scaled — Scale sizes the BODY, never the instrument** | VERIFIED (Stage 020R3) |
| **No Scale is baked or reset by sculpting: Start Sculpting freezes the local mesh byte-exactly and leaves all nine transform values bit-identical; Back to Construction and Resume Sculpt preserve the sculpt revision, the mesh, the ObjectId and the non-uniform Scale** | VERIFIED (Stage 020R3) |
| **Entering, leaving and resuming Sculpt, and a real sculpt stroke on a scaled body, leave both Construction history stacks exactly where they were and no edit open** | VERIFIED (Stage 020R3) |
| **An unscaled body sculpts exactly as it did: at `S = (1,1,1)` the world metric IS the local one and the shared normal conversion reduces to the captured normal times the amount** | VERIFIED (Stage 020R3) |
| **The space selector is World/Local for Move and Rotate and ABSENT in Scale; leaving Scale restores the remembered space; mode and space changes cost no step and no revision** | VERIFIED (Stage 020R2) |
| Handles pivot on the body's Construction placement origin — never a mesh AABB centre, a screen centroid or a camera-facing proxy | VERIFIED (Stage 020) |
| A touch at the pivot names the uniform handle in Scale and NO handle in Move and Rotate, where every shaft and ring converges there | VERIFIED (Stage 020R2) |
| Drawing and hit-testing share one camera-derived scale: the gizmo stays between 30 dp and 160 dp of shaft length from 3 m to 30 m of camera distance, and every target — shaft, arc, plane square and uniform cube — is 48 dp-class | VERIFIED (Stage 020, extended Stage 020R2) |
| One pointer drag is exactly one history step whatever the sample count; a tap records none; a cancel or a second finger restores the pre-drag placement exactly and records none | VERIFIED (Stage 020) |
| A drag on a handle suppresses the camera for that pointer; a drag anywhere else in the viewport orbits, pans and picks exactly as before | VERIFIED (Stage 020) |
| A stylus grabs and drags a handle through the same solver, tracked by its own pointer id | VERIFIED (Stage 020) |
| A drag can only move the body it started on, whatever the selection does underneath it | VERIFIED (Stage 020) |
| Gizmo and Exact Transform are two front ends to one native placement, in both directions | VERIFIED (Stage 020) |
| A transform-only drag — Move, Rotate OR Scale — its commit, its undo and its redo publish no mesh revision, upload nothing and regenerate no primitive; a shape change still does | VERIFIED (Stage 020, extended Stage 020R2) |
| No custom pivot, no centre free move, no arcball, no Mirror, no parent/explicit coordinate space, and no snapping of any kind — the quantization and placement seam is the identity | VERIFIED (Stage 020R2) |
| Sculpt draws no handles and neither selector, and the native guard refuses a gizmo there regardless | VERIFIED (Stage 020) |
| **The model exports as one glTF 2.0 binary (`.glb`) to a destination the user picks through SAF, with the ordinary Export control** | VERIFIED (Stage 023) |
| **The exported bytes are a conformant single-file GLB — 12-byte header, 4-byte-aligned JSON and BIN chunks, in-range accessors, correct POSITION `min`/`max`, unit normals, no external `uri`, no second file** — read back by `GlbDocument`, an independent in-repo glTF 2.0 reader that shares no code with the writer | VERIFIED (Stage 023) |
| **Zero coordinate conversion: 1.0 ForgeShape metre is 1.0 glTF metre, +Y is up, and a 1×2×4 m box exports with exactly those extents on X/Y/Z — no conversion node, no global scale factor** | VERIFIED (Stage 023) |
| **A static export bakes each body's rotation and scale into its vertices and leaves only the translation on the node: node rotation and node scale are identity — in fact absent — and no node matrix is written at all** | VERIFIED (E2E-R1C-C1) |
| **Every baked position is `L · p` for that body's own `L = Rz·Ry·Rx·S`, compared vertex by vertex against the value recomputed outside the exporter** | VERIFIED (E2E-R1C-C1) |
| **Baking is about the body's LOCAL ORIGIN: nothing is recentred on a bounds centre, and the pivot the node positions is the pivot the body is rotated about — proved on a cone, because a box, plane or sphere is centrally symmetric and would hide a recentre** | VERIFIED (E2E-R1C-C1) |
| **Baked normals ride `transpose(inverse(L))`, never `L`: unit length, still perpendicular to their own faces under a non-uniform scale, and demonstrably different from what `L` would have produced** | VERIFIED (E2E-R1C-C1) |
| **POSITION `min`/`max` are recomputed from the baked vertices**, and an independent reader re-derives them from the data rather than trusting the accessor | VERIFIED (E2E-R1C-C1) |
| **`det(L) > 0` preserves winding through the bake; a singular or mirrored linear transform is REFUSED (`SingularTransform` / `MirroredTransform`) rather than compensated, and the transform domain still refuses a zero or negative scale at the source** | VERIFIED (E2E-R1C-C1) |
| **Each body bakes its own `L` independently and nothing is merged**: two identical 1 m cubes with different placements export as two nodes with two different geometries | VERIFIED (E2E-R1C-C1) |
| **The bake changes no other export property**: metres, +Y up, right-handedness, per-body representation, determinism and one-file-no-sidecar are all unchanged | VERIFIED (E2E-R1C-C1) |
| **Every scene body is exported exactly once, in scene order, named `Body_<ObjectId>`, with triangles wound counter-clockwise from outside** | VERIFIED (Stage 023) |
| **Per-body representation is honoured and never guessed: in a Sculpt project a body with a Frozen Sculpt Mesh exports that mesh and its never-sculpted companion exports its Construction shape — there is no fallback from Sculpt to the Construction Source** | VERIFIED (Stage 023) |
| **Export is a READ**: no revision minted, no history step, no `ObjectId` moved, no sculpt vertex touched, no observable native state changed, and neither the manual `.forge` slot nor the recovery checkpoint written | VERIFIED (Stage 023) |
| **The whole six-body and Sculpt golden-corpus fixtures export correctly on device**, and the same project exports byte-identically across runs | VERIFIED (Stage 023) |
| **A cancelled export writes nothing, changes nothing, and does not leave staged bytes behind for the next answer** | VERIFIED (Stage 023) |
| **A `.glb` can be read back and drawn in the viewport as a session-only diagnostic preview**, picked through SAF, by a parser that shares no line with the exporter | VERIFIED (GLB-IMPORT-R0/R1) |
| **The exported file and the ForgeShape scene describe the SAME world geometry**: maximum world-position delta **exactly 0.0 m**, maximum world-normal delta `1.2e-06°`, identical vertex counts, triangle counts, index order and per-body world bounds — for the six-body Construction project, the Sculpt project, and the committed sentinel bytes against the projects they came from | VERIFIED (GLB-IMPORT-R0) |
| **The comparison can fail**: moving a body after export, or comparing a project against another project's file, is reported as `ROUNDTRIP_MISMATCH` naming the body, the vertex and the distance | VERIFIED (GLB-IMPORT-R0) |
| **The expected side comes from DOMAIN truth** — the primitive generator or the Frozen Sculpt Mesh, through the product's render-mesh derivation, placed by `modelMatrix()` — never from the exporter's captured arrays | VERIFIED (GLB-IMPORT-R0) |
| **Unsupported GLB features fail closed by name**: node children, a node stating both a `matrix` and a TRS, a singular or non-affine node transform, external buffer `uri`, required extension (Draco and meshopt included), animation, skinning, sparse accessor, morph targets, non-triangle mode, a non-indexed primitive, an unknown attribute, interleaved buffer view, missing POSITION, an attribute disagreeing with POSITION on count, wrong asset version, truncation, a file that lies about its own length, and a `.forge` file offered as a GLB | VERIFIED (GLB-IMPORT-R1) |
| **An ordinary STATIC external `.glb` previews**: a node `matrix` or TRS, baked column-major with normals through the inverse transpose and winding corrected on a negative determinant; seven TRIANGLES primitives over one shared POSITION accessor decoded once, as seven draw batches; a missing NORMAL generated area-weighted and finite; COLOR_0/COLOR_1/TEXCOORD_0 validated and not decoded; `doubleSided` reaching preview culling only; `extras` ignored — proved against a deterministic synthetic fixture of 1978 vertices / 3780 triangles / 7 primitives | VERIFIED (GLB-IMPORT-R1) |
| **The world bounds of the baked external geometry match an independent third reader's** computation from the same bytes | VERIFIED (GLB-IMPORT-R1) |
| **Navigation keeps working over a preview**: a real one-finger orbit through the platform-neutral input seam turns the camera and the preview stays what the viewport draws | VERIFIED (GLB-IMPORT-R1) |
| **Import GLB… and Open File… are distinct acts**: different intents, a `.glb` never reaches the `.forge` decoder and a `.forge` never reaches the GLB parser, and a cancel is a no-op in both | VERIFIED (GLB-IMPORT-R1) |
| **A supported `.glb` becomes DURABLE objects**: one object per supported top-level mesh node, appended in file order with ordinary `ObjectId`s, one row each in the Objects list named from the file, the first one selected; a mesh's several primitives stay one object with exact submesh batches | VERIFIED (IMPORT-01A) |
| **An import is exactly one Undo**: however many objects arrive, one Undo removes all of them and one Redo restores identical ids, names, geometry, batches and placement; the id allocator is never rolled back | VERIFIED (IMPORT-01A) |
| **The node transform is SPLIT**: the linear part baked into local geometry, the translation kept as the body's placement, rotation `0,0,0` and scale `1,1,1` to begin with, and nothing recentred | VERIFIED (IMPORT-01A) |
| **An import is atomic**: one object that fails validation refuses the whole file, and a refusal — cancel, unreadable, unsupported or inconsistent — leaves the scene, the history, the project bytes and the autosave fingerprint untouched and writes no checkpoint | VERIFIED (IMPORT-01A) |
| **An imported body takes the ordinary transform path**: Move, Rotate and Scale reach the body's own `ConstructionTransform`, cost one history step, publish no revision and move no vertex; Undo and Redo return the exact placements | VERIFIED (IMPORT-01A) |
| **`Shape` is absent for an imported body** and refused below JNI as well, and comes straight back when a Construction Body is selected again. *Start Sculpting* was withdrawn with it at IMPORT-01A and is OFFERED since IMPORT-01B | VERIFIED (IMPORT-01A, amended IMPORT-01B) |
| **`.forge` carries an Imported Mesh**: imported-only, Construction + Imported and Construction + Sculpt + Imported all encode, decode bit-for-bit, load and re-encode identically; the geometry survives Save/Open, Save Copy/Open File, autosave and recovery with the source `.glb` gone; and the seven legacy fixtures' digests are unchanged | VERIFIED (IMPORT-01A) |
| **The independent PowerShell encoder and the C++ codec agree** on all twelve corpus fixtures, and the seven packaged into the test APK LOAD on device with the representations the specification states | VERIFIED (IMPORT-01A, extended IMPORT-01B) |
| **An Imported Mesh can be SCULPTED**: *Start Sculpting* is offered for one, the seed is its own local geometry through `buildSculptSourceMesh`, the first pre-stroke frame is the same world geometry in the same place, and no Construction Source is invented | VERIFIED (IMPORT-01B) |
| **The imported source is immutable through a real stroke**: positions, normals, indices, submesh batches and the authored transform are bit-identical afterwards, both in the domain and on device through the `.forge` `IMPT` bytes; an Imported Mesh can never go stale | VERIFIED (IMPORT-01B) |
| **The seed uses the RAW indices and collapses sidedness permissively**: never `buildDrawData`'s reversed duplicates, which would cancel every area-weighted normal, so every frozen vertex normal is a real direction | VERIFIED (IMPORT-01B) |
| **Back to Imported Mesh and Resume Sculpt are reversible both ways**: leaving republishes the imported geometry and keeps the sculpt mesh; resuming returns the same revision, counts and edits | VERIFIED (IMPORT-01B) |
| **The destructive reset is representation-aware and still guarded**: *Reset Sculpt from Imported Mesh…*, the same `SCULPT_HAS_EDITS` question, the same confirmation, and cancelling makes no native call | VERIFIED (IMPORT-01B) |
| **`.forge` carries `IMPT` + `SCUL`**: an imported body with a sculpt mesh and a four-combination mixed project both encode, decode bit-for-bit, load, re-encode identically and survive Save, Save Copy, autosave, recovery and reload; the ten pre-existing fixtures' digests are unchanged and no version was bumped | VERIFIED (IMPORT-01B) |
| **Export follows the effective current geometry** for an imported body exactly as for a Construction one: the sculpted mesh in Sculpt, the imported source otherwise, and exporting records no history step and moves neither `.forge` slot | VERIFIED (IMPORT-01B) |
| **Delete removes a real `SceneObject`**: of either representation, with or without retained sculpt work, as exactly one history transaction; Undo restores the SAME object with its geometry, sculpt mesh, placement and published revision, and Redo removes it again | VERIFIED (UI-OWNER-45) |
| **A deleted body leaves no ghost**: it does not render, cannot be picked, and is absent from `.forge`, the autosave checkpoint and the exported GLB, with no orphan `CONS`, `IMPT` or `SCUL` record; Undo restores it to all of them | VERIFIED (UI-OWNER-45) |
| **Delete's selection is deterministic**: unchanged when the deleted body was not active, the next row when it was, the previous row when it was last | VERIFIED (UI-OWNER-45) |
| **The last body is refused by name** (`RefusedLastBody`) and Delete is refused while sculpting; both controls are withdrawn there, both guards stay below JNI, and no replacement primitive is ever invented | VERIFIED (UI-OWNER-45) |
| **The renderer releases a body that left the snapshot**, so repeated add/delete cannot grow `Renderer::bodies_` without bound; an Undo re-uploads through the path a body already takes when first drawn | VERIFIED (UI-OWNER-45) |
| **The owner's own `1 lowpoly.glb` opens and imports** | **UNVERIFIED** — `OWNER_SAMPLE_RUNTIME_TEST_PENDING` / `OWNER_REAL_FILE_01A_RETEST_PENDING`. The binary is external to this coding environment; every structural feature the coordinator listed for it is covered by the synthetic fixture, but the file itself was never in reach. The owner's manual step |
| **The preview is not project truth**: no scene `ObjectId`, no body added, no active body changed, no revision published, no history step, no `.forge` byte, no autosave fingerprint move, no checkpoint written; a refused load leaves an existing preview whole; Clear releases everything and restores the workspace | VERIFIED (GLB-IMPORT-R0) |
| **Preview mode draws no control that cannot succeed**: the trailing host, Undo/Redo, the Objects capsule, the Property Inspector and the mode transitions are all withdrawn, a tap selects nothing and the gizmo is absent — and switching back restores the accepted workspace exactly | VERIFIED (GLB-IMPORT-R0) |
| **The exported `.glb` opens in a third-party program (Godot, Blender or another importer)** | **UNVERIFIED** — no external importer was run here; not installed and not authorized. GLB-IMPORT-R0 rules out a ForgeShape exporter/parser defect but makes no claim about any external tool |
| 16 KB page size *and* ARM64 in one target | **UNVERIFIED** — see Known Issues |
| Stylus / S Pen tool type and pressure on real hardware | **UNVERIFIED** — verified synthetically end to end in INPUT-R1; closing it needs a person physically moving an S Pen |

## Self-test suite

Nineteen debug-only native suites run once from `NativeViewport.start()` —
never per frame — and total **2889 checks, zero failures**:

| suite token | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 174 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 121 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 125 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 494 |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 329 |
| `FORGESHAPE_SCENE_SELFTEST_OK` | 79 |
| `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK` | 147 |
| `FORGESHAPE_GIZMO_SELFTEST_OK` | 145 |
| `FORGESHAPE_PROJECT_SELFTEST_OK` | 230 |
| `FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK` | 24 |
| `FORGESHAPE_GLTF_EXPORT_SELFTEST_OK` | 93 |
| `FORGESHAPE_GLTF_IMPORT_SELFTEST_OK` | 184 |
| `FORGESHAPE_CAD_SELFTEST_OK` | 122 |
| `FORGESHAPE_CAD_A3_SELFTEST_OK` | 44 |

The CAD suite (`forgeshape_cad_selftest.cpp`, `CADR0-*`) builds its own sketches,
scenes, histories and camera, drives the sketch session through real
`TouchPointer` samples, and prints `FORGESHAPE_CAD_PERFORMANCE` — its bounded
extraction, triangulation and regeneration timings for a rectangle, a 32-gon
circle and 32- and 128-edge polylines — on every launch. The project suite
prints a fourth digest line, `FORGESHAPE_PROJECT_GOLDEN_SHA256_CAD`.

followed by `FORGESHAPE_PROJECT_GOLDEN_SHA256`, `FORGESHAPE_MESH_UPLOAD_OK`,
`FORGESHAPE_GRID_UPLOAD_OK`, `FORGESHAPE_GIZMO_UPLOAD_OK` and
`FORGESHAPE_NATIVE_VIEWPORT_OK`.

**The project suite owns the `.forge` format** (`FSR1A-01..15`) and, like the
scene, history and gizmo suites, builds its own `ConstructionScene`,
`ConstructionHistory` and `SculptSession` per case. Its 132 checks cover the
28-byte header and 24-byte section header field by field with every multibyte
value read back little-endian, the CRC-32/ISO-HDLC check value, a deterministic
writer (`encode` twice and `encode -> decode -> encode` both byte-identical), a
six-body roundtrip carrying all six primitive kinds, all six remembered parameter
sets per body, 370 degrees uncanonicalized and a non-uniform scale, an id
allocator that cannot mint a collision after a load, Construction meshes
regenerated rather than read (the file size is exactly the semantic arithmetic,
with no room for a vertex), sculpt positions surviving bit for bit with their
topology, `renderBothSides`, `sourceStale` and `hasEdits`, the legacy mixed state
keeping both branches, and the whole refusal matrix — bad CRC, truncation, an
impossible or overflowing count, a newer major, a newer required section version,
an unknown optional section skipped, an unknown required section refused, a
duplicate singleton, reserved bits, and every semantic value the domain's own
validators refuse — each proved to leave a live fixture project, its mode and its
history untouched. `FORGESHAPE_PROJECT_GOLDEN_SHA256` prints the digests of the
two canonical fixtures as this build encodes them. The suite also covers
E2E-R1B's `FSR1B-01`, `-02` and `-13`: the semantic fingerprint autosave asks
before it writes anything moves for a shape edit, a placement edit, a creation,
a selection and a mode change, stays put for a REFUSED edit and for an
identical re-apply — and, the case that makes counters the wrong answer,
changes on an UNDO, which `restoreState` deliberately does not count.

**The render-recovery suite owns what a lost GPU device means** (`FSR1B-16`) and
is free of Vulkan on purpose, so a device-loss contract can be verified without
destabilising the authoritative emulator's GPU — which the repository forbids.
Its 24 checks cover the classification of every result the frame loop can see
(including that an EXPECTED `SUBOPTIMAL` is not a fault, which the
identity-preTransform convention makes the steady state on a rotated display),
the bounded rebuild attempts, a failed rebuild spending the remaining attempts
at once, and the terminal state being genuinely terminal — nothing restarts a
stopped renderer. What the policy DOES on a real device is proved separately on
device by the debug injection seam.

**The gizmo suite owns direct manipulation's math and its transaction**, and —
like the scene and history suites — it builds its own `ConstructionScene`,
`ConstructionHistory` and `GizmoSession` per case. Its 145 checks cover the three
closed enums and their refusal of an unknown index, the eight handle codes round
tripping with the three axis codes unchanged from Stage 020, plane handles
naming their two basis axes and borrowing the perpendicular hue, the
quantization and placement seams being the identity, the World basis being the
world axes and the Local basis being the body's rotation columns *and staying
scale-free*, `projectWorldToScreen` and `buildPickRay` being exact inverses, the
screen-constant scale doubling at twice the distance in Perspective and being
depth-independent in Orthographic, the drawn handle length staying within 5% of
its reference-unit constant near and far, every hit target meeting the 48-unit
floor and the plane square clearing the axis corridor, the ray/axis
closest-point solver over perpendicular, negative and skew rays, the plane
fallback firing near-parallel and the exactly-parallel case being unresolvable
rather than guessed, non-finite and degenerate-axis guards, ray/plane
intersection including the edge-on refusal, the signed angle's sign and its
indifference to the out-of-plane component, the unwrap across a near-full turn,
accumulation past 360°, per-handle hit-testing in all three modes, the pivot
being the uniform handle in Scale and no handle in Move, Scale forcing Local and
restoring the remembered space on the way out, every world Move axis and plane
moving only what it may, a Local axis drag moving *along the body's own axis*
and a Local plane drag staying in the body's own plane on a mixed orientation,
World rotation matching pre-multiplication and Local matching
post-multiplication against matrices rather than Euler fields, the two spaces
producing different but individually correct orientations from one start, a ring
drag passing two full turns without canonicalising, a drag through the pitch
singularity staying finite and orientation-correct, axis/plane/uniform scale
with the ratios preserved and never reaching zero, a stretched body leaving its
own instrument alone, one drag being exactly one step whatever the sample count,
a tap recording nothing, cancel restoring all nine values exactly, a
non-captured pointer moving nothing, a drag never retargeting when the selection
changes underneath it, mode and space both refused mid-drag, edge-on and
near-camera-parallel drags staying finite, multi-object chronology, and the
session-initialization boundary leaving an empty history without undoing the
seed.

UI-LAYOUT-R1 added ten checks there, and they are about what the instrument LOOKS
like rather than what it does: the buffer holding exactly the vertices it
declares and each mode drawing exactly its own range, the pivot mark being
neutral, part of no handle and inside the disc that grabs nothing in Move and in
Rotate, Scale putting a HANDLE on that point instead of a mark, the uniform cube
being a double outline and larger than a shaft cube, a plane handle being a
crossed square rather than a bare outline, the held colour being one no axis
owns, and the unheld weight being lower than the held one. Every one is
arithmetic over the generated vertex buffer; none looks at a pixel.

**The Construction-transform suite owns the nine authoritative values and the
one Euler bridge.** Its 121 checks add, on top of the existing rigid-transform
cases: the default scale of one, an ordinary scale applying and re-applying as
Unchanged, zero and negative refused as `NotPositive` while a non-finite scale is
refused as `NotFinite`, a bad scale leaving the position untouched, scale acting
in object space before the rotation, the scaled inverse undoing the scaled model,
a local ray parameter still being world distance under scale, the normal matrix
using the INVERSE scale and keeping a carried normal perpendicular to a carried
tangent, an unscaled normal matrix being exactly the rotation, every
decomposition rebuilding its own matrix, a decomposition preferring the triple it
was given, a stepped turn climbing past 720° and falling past −360° without
wrapping, the branch nearest the previous answer winning while still naming the
same orientation, an untouched component coming back exactly, a singular
orientation staying finite and orientation-correct, and the refusal paths writing
nothing.

**The Construction-history suite owns the transaction boundary and every
undo/redo invariant**, and — like the scene suite — it builds its own
`ConstructionScene` and its own `ConstructionHistory` for every case rather than
touching the process-scoped pair, which is what makes its result independent of
what a live session left behind. Its 114 checks cover the empty history, one
Apply as one reversible step, rejection and no-op suppression, the six transform
values moving atomically and 370° surviving un-canonicalized, many updates in one
transaction committing as one step, cancel, creation as one atomic transaction
with no default-Box remnant, redo restoring the same `ObjectId` and scene
position, the id allocator never rolling back, multi-object interleaving in
chronological order, redo invalidation and the three things that do *not*
invalidate it, selection not being an edit, bounded capacity with oldest-first
eviction, Sculpt separation both ways, a restored body keeping its Frozen Sculpt
Mesh, and the publication counts a restore actually incurs.

**Suite ownership is by module, and it decides where a check lives.** The
render-shading suite owns PRESENTATION — selection feedback (`r1c1_*`) and the
world grid (`r1c2_*`) live there rather than in picking or selection, which own
*which* object is selected and what is in the scene. It also covers the crease
policy, all six primitives' Smooth contracts, the capsule equality case, Faceted,
NaN/Inf and fail-closed behaviour, determinism, the render-data rebuild policy,
the generated MatCap, the display settings, the proof that building render data
leaves the authoritative `RuntimeMesh` bit-identical, and the Plane's two-sided
render duplication and its inertness to display switching. Nothing anywhere is
driven by a clock or a GPU, so a whole 220 ms pulse costs microseconds and no
check can be flaky; none asserts a rendered pixel.

Three cross-cutting families are split across suites by that same rule. The
projection family `CAMPROJ-01`..`14`: camera `01`..`08` and `12`..`14`, picking
`09`/`10`, sculpt `11`. The direction family `NOR-01`..`10`: source winding,
outward render normals, Faceted orientation, duplication-preserves-winding, the
model→view normal transform and display-mode inertness. The Plane family: `PLN-
01`..`08` in the primitive suite, `PLN-09`/`10`/`20` in render shading, `PLN-11`..
`16` in picking, `PLN-17`..`19` in sculpt. Each family includes a member that
would fail if the behaviour collapsed to a constant —
`nor_outwardness_fails_on_global_normal_flip` asserts the measurement inverts
under a global `normal *= -1`.

**The picking suite also owns the POINTER BOUNDARY**, because it already owns
`TouchPointer` and the selection rules the same events drive: 46 checks over the
tool-type wire codes and their Unknown fallback, the struct defaults, pressure and
tilt range/wrap/non-finite behaviour, per-index packing, the 6-pointer bound, and
the proof that a stylus resolves the same tap and orbits the same distance as a
finger. The Android half of the tool-type mapping is deliberately not here: it is
JVM and instrumented, where `MotionEvent` exists. The sculpt suite's 14
pressure-independence checks are its teeth, four tools → three assertions each.

**The sculpt suite also owns the BRUSH METRIC** (`s020r3_*`, 62 checks). The
helper alone under identity, uniform and non-uniform Scale and under a
translation; the brush rim proved to be a world SPHERE without any vertex
sampling — 128 world directions each reaching the rim at the same world distance
— at `(3,1,1)` and `(0.5,2.5,1.5)`; the real affected set of a real stroke being
exactly the world ball with weights equal to `sculptFalloff` of the WORLD
distance; the same under a mixed rotation, plus the metric's indifference to
rotation over 64 offsets; the screen-pixel radius resolving to the same world
radius before and after a body is scaled (Orthographic, where the pixel scale is
depth-independent, so "the same hit depth" is exact); all four brushes' affected
sets and weights on one stretched, turned body, parameterized because the path is
shared; Grab's world displacement being exactly the camera-plane pointer delta
times the weight; Clay's and Inflate's world step having the amount as its
LENGTH and lying along the DISPLAYED normal rather than the raw local one; Start
Sculpting leaving all nine transform values bit-identical and the frozen vertices
byte-identical to the source; Back to Construction and Resume Sculpt preserving
the revision, the mesh, the ObjectId and the non-uniform Scale; both history
stacks and the open-edit flag unmoved by the transitions and by a real stroke;
and every unscaled result reproducing the plain local rule exactly. Four checks
are deliberate **negative controls** — they assert that the old local-metric
answer genuinely differs — so the suite cannot pass by measuring nothing.

Three non-per-frame diagnostics exist. `FORGESHAPE_SURFACE_CONFIG` (one per
swapchain creation) and `FORGESHAPE_CAMERA_VIEWPORT` (one per `surfaceChanged`)
audit the orientation chain; `FORGESHAPE_RENDER_MESH_BUILD` (one per accepted
geometry or surface-shading change) reports source vs render counts, the rebuild
count and the frame counter. `FORGESHAPE_GRID_UPLOAD_OK` is one per Vulkan
device. `README.md` documents how to read them.

## Android UI suites

| suite | scope | tests |
| --- | --- | --- |
| `WorkspaceLayoutModeTest` (JVM) | breakpoints, placement, chrome sizing and the Objects dock decision; no dead trailing-rail docking state remains | 15 |
| `EditorUiStateTest` (JVM) | what the UI may remember and what it refuses, including that the precision surface starts closed in both modes and no window size opens it | 10 |
| `LengthUnitTest` (JVM) | exact mm/cm/m round-tripping and parse refusal | 5 |
| `AppThemeTest` (JVM) | the default, the three approved appearances, the JNI index contract, and that choosing one moves nothing else | 11 |
| `ChromeMotionTest` (JVM) | the fade durations, the anchored-growth durations and curve shape (`UIR4B-08`), the uniform start scale, and that reduced motion returns 0 rather than a short duration (`UIR4B-10`) | 8 |
| `DisplaySettingsContractTest` (JVM) | the shading/surface index contract across JNI | 4 |
| `EditorWorkspaceControlsTest` | control sets, fields, validation, freeze wording, tools, the six-kind round trip | 23 |
| `EditorWorkspaceLayoutTest` | measured viewport floor, landscape, expanded, collapse, the UI-R1C2 adaptive pass | 10 |
| `EditorWorkspaceGestureTest` | chrome gesture ownership, IME | 6 |
| `EditorWorkspaceLifecycleTest` | HOME/resume rebuilt from native truth | 3 |
| `EditorWorkspaceDisplayTest` | display/projection ids, presentation-only, resume, refused index, the View/Grid group | 17 |
| `EditorWorkspaceObjectsTest` | rows by ObjectId, viewport pick sync, the docked surface, 20-body scalability | 10 |
| `HomeFlowTest` | `E2E-APPH1-01..12` (`CAD-A3-C1`): cold-launch Home with no project, no body, no history, no document and exactly two actions; New Project → CAD/Sculpt and Cancel; New CAD through a real spatial plane tap-tap to the first durable body with an empty history; New Sculpt as a real sculpt project with Sculpt Undo green; Open File from Home restoring a producer + face-supported dependent; cancel and a corrupt/missing file leaving Home standing; the unsaved-changes guard's Cancel, Discard and Save paths; deterministic Back in every phase; Home and the bootstrap surviving a recreation — and, on every journey, that native never read an active body while no project was open | 12 |
| `CadA3VisualEvidenceTest` | `E2E-CADA3-VIS`: the ten-frame Home → New CAD → face sketch → reopen journey captured through the composed display with measured facts per frame, for `OWNER_CONTACT_SHEET.png` / `VISUAL_EVIDENCE.md` | 1 |
| `SpatialSketchTest` | `E2E-CADA3`: New Sketch landing directly in the spatial support chooser; the by-name plane fallback; every world plane by a real aiming tap and a committing tap; a synthesized stylus hover that highlights and never commits, then a finger commit; a planar cap and a side face each supporting a sketch; a cylindrical side never doing so; a face-supported dependent surviving a producer depth edit, refusing the producer's delete and surviving a save/reopen; the adaptive grid following two real pinches with a typed value kept exact | 8 |
| `EditorWorkspaceFoundationTest` | icons, pressed feedback, touch floor in Construction **and in Sculpt** (`UIR3-01`) including the precision toggle and the capsule`+`, rail tap-vs-scroll, viewport floor, popover | 8 |
| `EditorWorkspaceThemeTest` | the three-palette control, the switch, state preservation across the recreation, the material tiers, selection-vs-commit, contrast | 19 |
| `EditorWorkspaceMotionTest` | popover preserved, inspector interruptibility, chrome hide/restore, viewport stability, reduced motion, gesture priority | 10 |
| `PointerSemanticsTest` (JVM) | the Android tool-type mapping and its Unknown fallback | 6 |
| `EditorWorkspacePointerTest` | synthetic stylus transport, per-pointer association, and that tap / navigation / sculpt arbitration are unchanged | 13 |
| `EditorWorkspaceCompositionTest` | the UI-R2 role split: viewport dominance, the scene panel, one list with one owner, inspector-names-its-body, and that composition rebuilds no geometry | 10 |
| `SketchExtrudeTest` | `E2E-CADR0-01..16`: New Sketch and the plane chooser; a real dragged rectangle, circle and tapped polyline; Finish Sketch; a typed depth and Extrude; the Objects row and the CAD context; Undo/Redo of the creation; later rectangle and depth edits as one step each; save/reopen as editable truth; an open polyline refused by name and Cancel leaving the project byte-identical; a gizmo drag surviving a depth edit; Imported Mesh Sculpt Undo/Redo beside a CAD Body | 9 |
| `EditorWorkspaceMobileTest` | the UI-R4A structural claim: nothing owns the bottom edge in either mode (`UIR4A-01`, `-11`), the Objects capsule (`-02`), Add Primitive anchored to its `+` and offering exactly the six real primitives (`-03`, `-04`), Sphere and Plane routed through native truth (`-05`, `-06`), closing leaves one Objects control (`-07`), exact Shape and Transform one action away (`-08`, `-09`), the rail's icons and touch floor (`-10`), context surfaces rebuild no geometry (`-14`), the new surfaces leak no gesture (`-15`), one vocabulary in every window (`-17`) | 15 |
| `EditorWorkspaceSculptRetentionTest` | `UIR4A-12` / `UIR4B-20`: Start Sculpting → real stroke → Back → Resume returns the same revision, counts, stroke history and ObjectId, with the Construction Source untouched; and that a stale source is readable from the resting workspace | 2 |
| `EditorWorkspaceCorrectionTest` | the UI-R4B corrections: no Sculpt creation path and the scene still reachable (`UIR4B-01`), Construction's six-primitive path intact (`-02`), the rail's active state across a rebuild in both modes and cleared on a mode change (`-03`), the status lifecycle, the longer hold for a rejection, the empty resting line and the standing fault (`-04`), brush values beside their sliders and nowhere else and publishing nothing (`-05`), no mid-row cut in Shape or Transform with scrolling and fields intact (`-06`, `-07`), one shared anchored contract (`-08`), a correct first-open pivot for all four surfaces (`-09`), instant reduced motion and interruptibility (`-10`), zero geometry from chrome and motion (`-11`), inset floating surfaces on an expanded window (`-12`), Radius/Strength off the model in expanded Sculpt (`-13`), concentric active geometry and a fill-led selection with no stroke (`-14`), no user-facing *Freeze* over every `R.string` (`-15`), Start Sculpting with Back/Resume/confirmation intact (`-16`), the 48 dp floor in both modes and the toolbar still fitting (`-17`), three appearances and no geometry (`-18`), grid and selection feedback unchanged (`-19`) | 36 |
| `EditorWorkspaceChromeCompositionTest` | the UI-R4C composition cleanup: back navigation drawn in full with no ellipsis and no clipping in the window under test (`UIR4C-01`, `-03`) and in short landscape (`-02`), Add Primitive intersecting neither the tool cluster nor the precision control in either window (`-04`), the six primitives and their native routing after the reflow (`-05`), a single-member editing group drawn as one control and a two-member one still a capsule (`-06`), Resume Sculpt on the same geometry returning the same revision and mesh (`-07`), the 48 dp floor after the reduction (`-08`), no instructional capsule at rest (`-09`) or with the exact values open, with the transient path intact (`-10`), three appearances and no new colour in the lone-control forms (`-11`), zero native change across the whole sequence (`-12`) | 12 |
| `EditorWorkspaceArchitectureTest` | UI-ARCH-R1 ownership and parity (`UIAR1-01..12`): host-owned semantic children; no right-cluster leaf fields/accessors in the root; native readback for Move/Rotate/Scale and World/Local; Exact callback parity; Display suppression/restoration; compact, IME and expanded contracts; Sculpt rail parity; no duplicate state authority; stable no-op geometry/visibility snapshot | 12 |
| `EditorWorkspaceHistoryTest` | Stage 019, from the product side of JNI: an empty history and both controls disabled (`S019-01`, `-27`), one Apply as one reversible step down to the ObjectId (`-02`..`-04`), rejection and no-op writing nothing and publishing nothing (`-05`, `-06`), six placement values as one atomic step with 370° intact (`-07`, `-08`), Add Primitive as one creation transaction with no default-Box remnant and a redo restoring the same id, order and parameters (`-09`, `-10`), interleaved multi-object undo/redo in chronological order (`-11`), redo invalidation and the three things that do not invalidate it (`-12`, `-13`), no history from selection, context, surfaces, unit, grid, shading or chrome-hide (`-14`), begin/many-updates/commit as one step and cancel as none (`-15`, `-16`), bounded capacity with a clean exhaustion (`-17`), rotation and HOME/resume retention with the rebuilt chrome reading native state (`-18`, `-19`), the Sculpt seam writing nothing and withdrawing the pair while the native guard stands (`-20`, `-24`), a real Grab stroke writing no Construction history and a Construction undo leaving the sculpt mesh's revision, counts, strokes and identity alone except for the existing stale-source flag (`-21`, `-22`), enabled state always native state including across a rotation (`-23`), the 48 dp floor and no intersection with any other chrome surface (`-25`), and one Apply still exactly one publication with a commit adding none (`-26`) | 20 |

| `EditorWorkspaceGizmoTest` | Stage 020 and Stage 020R2, from the product side of JNI: both start answers leaving an empty history and the direct-Sculpt path being source-classified as production (`S020-PRE-01`..`-05`), the Shape context and Sculpt drawing no handles and no mode selector while the native guard still refuses (`S020-01`, `-03`), Transform with a body drawing both and entering on Move (`-02`), body switching retargeting the pivot without a step (`-04`), the mode selector recording nothing and publishing nothing (`-05`), each Move axis changing only its own coordinate through a real `MotionEvent` on the real handle (`-06`..`-09`), a near-camera-parallel axis staying finite (`-10`), a tap costing nothing and a 48-sample drag costing exactly one step (`-11`, `-12`), cancel restoring exactly (`-13`), each ring turning only its own component and leaving Position (`-14`..`-17`), a 300-sample ring drag with no sample jumping a quarter turn and the total passing 360° un-canonicalised (`-18`..`-20`), one ring drag as one step and its cancel (`-21`, `-22`), undo/redo bracketing a Move and a Rotate exactly (`-23`, `-24`), the exact-value FIELDS reading the gizmo's result and a typed Apply moving the pivot (`-25`, `-26`), redo invalidation (`-27`), two bodies undoing chronologically and independently (`-28`), a captured handle not orbiting while a drag off the handles still does (`-29`, `-30`), cancel and a second pointer leaving no open transaction and no partial transform (`-31`, `-32`), a stylus driving the same solver under its own pointer id (`-33`), chrome consuming its own touches (`-34`), the 48 dp hit corridor measured perpendicular to the shaft (`-35`), the drawn size staying in band from 3 m to 30 m (`-36`), the selector and the precision toggle both at 48 dp and collision-free in portrait and landscape (`-37`), a whole drag/commit/undo/redo publishing no revision with a positive control that does (`-38`..`-40`), and the gizmo surviving display switching (`-41`). Stage 020R2 adds: scale round-tripping through the history (`S020R2-01`), one Apply being atomic across all nine with an impossible scale refused and a no-op recording nothing (`-02`), every Move plane moving in its plane and never off it (`-03`), Local space moving along the body own axis where World moves only X (`-04`), World and Local rotation being different answers to the same gesture on a mixed body (`-05`..`-07`), axis, plane and uniform scale with the ratios preserved (`-09`..`-11`), one scale drag as one step with a tap costing nothing and a second pointer restoring all nine (`-12`, `-13`), the handles and the exact-value fields being one truth in both directions including the scale (`-14`), a non-uniform scale reaching PICKING and not the instrument (`-15`), every handle classified at its own pixel with the pivot naming the uniform handle in Scale and none in Move, and the held handle reported while held (`-16`), Scale being Local-only with the space selector absent there and the remembered space restored on the way out (`-18`), both selectors at 48 dp and collision-free in portrait and landscape (`-18` layout), and a long scale drag with its undo and redo publishing no revision against a positive control that does (`-20`) | 46 |

| `EditorWorkspaceLegibilityTest` | UI-LAYOUT-R1 retained, with the old reserved-row assertion strengthened to the R2 bottom-zone rule: a bottom sheet hides conflicting chrome instead of relocating it; Display restoration, touch floors, Exact legibility and Sculpt top grammar remain covered | 20 |
| `EditorWorkspaceProjectActionsTest` | `E2ER1A-01`, `-04`, `-05`, `-06`: a real Save from the product control writing a non-empty, re-loadable `.forge` to the app-private slot without moving the accepted R2 right host; the project control and both rows at the 48 dp hit floor; a damaged, a truncated, an unsupported-major and a not-a-project file each refused with its own message and with EVERY native value, the active body, the scene size and the session history bit-identical afterwards; and a successful Open clearing both history stacks, with the next edit recording exactly one step and its undo returning the LOADED value | 7 |
| `GlbImportExternalR1Test` | `GLBIR1-16..21`, `E2E-GLBIR1-01..09`: the widened external-GLB parse, driven into the DIAGNOSTIC preview through the native seams (it has no user-facing control since `IMPORT-01A`). The deterministic Nomad-like fixture parses as one mesh of 1978 vertices, 3780 triangles and **seven** draw batches — the shared POSITION accessor decoded once rather than seven times (`-19`, `E2E-01`); it carries no NORMAL and still draws finite bounded geometry (`E2E-02`); its baked world bounds match an **independent third reader's** computation from the same bytes, so the node matrix was applied column-major and not transposed, dropped or mirrored (`E2E-03`); every batch of its double-sided material renders both sides (`E2E-04`); a real one-finger orbit through the input seam turns the camera while the preview is shown and the preview survives it (`E2E-05`); the preview adds no body, changes no active body, publishes no revision, records no history step, moves no `.forge` byte and no autosave fingerprint, and Source→Imported→Source→Clear re-encodes the project identically and leaves it editable (`-17`, `E2E-06`, `E2E-09`); exporting while a preview is shown exports the PROJECT (`-17`); a required compression extension and a `.forge` offered as a GLB each refuse with the right stable token and bounded category and leave nothing behind (`E2E-07`); a refusal never replaces an existing preview (`-17`); Import GLB and Open File are different intents, a `.glb` opened as a project changes nothing, and a cancel is a no-op (`-16`, `E2E-08`); and the one import control keeps its wording and the 48 dp floor (`-20`). Two further cases leave the fixture fingerprint and screenshot that `scripts/run-glb-import-r1-evidence.ps1` pulls | 14 |
| `ProjectAutosaveRecoveryTest` | `FSR1B-01..09`: autosave writes the canonical `.forge` document to its OWN slot and never the manual one; twenty dirty generations cost exactly one write and the newest state wins; an unchanged project, a refused edit and an identical re-apply each cost none; a failed write leaves the previous valid checkpoint byte-identical with no pending file beside it; a validated candidate is offered on a cold launch, re-offered while unanswered and never again once answered; Recover restores exact Construction values including 370 degrees and a non-uniform scale, and a fresh history; Recover restores the sculpt mesh, its counts, its stale-source state and its `hasEdits` safety state; Discard removes the candidate and leaves an explicitly saved project byte-identical; a corrupt and an unsupported-major candidate are each quarantined, change nothing, and never come back; leaving the foreground checkpoints the latest state and a recreation duplicates no body | 14 |
| `ImportedMeshDurableTest` | `IMP01A-14/15/19/21..25`, `E2E-IMP01A-01..12`: durable GLB import on a device. A `.glb` picked through the real picker-result seam becomes six objects appended after the bodies that were there, all imported, all with rows found by tag, the first one selected, in exactly one Undo step (`E2E-01`, `E2E-05`); a row reads the file's name rather than `Body #id` and selecting it makes that body active (`-23`, `E2E-02`); `Shape` and a sculpt transition are both drawn for a Construction Body; since `IMPORT-01B` an imported body keeps the sculpt transition and loses only `Shape`, which is still refused below JNI with no project byte moving and comes back when a Construction Body is reselected (`-14`, `E2E-10`); a nine-value transform Apply on an imported body moves it, undoes to the placement the file gave it and redoes exactly, with no vertex touched (`E2E-03`, `E2E-04`); undoing the IMPORT removes every object it created and the project is what it was but for the id allocator, which never rolls back (`E2E-03`); a saved project reopens every body with its representation after the scene has been genuinely replaced, and re-encodes byte-identically without the source `.glb` (`E2E-06`); Save Copy and Open File carry an imported body and keep no trace of the path (`-22`, `E2E-08`); an import is checkpointed and the checkpoint decodes and restores through the ordinary load (`-21`, `E2E-07`); a cancel, a `.forge` offered as a GLB and a missing file each change nothing, move no fingerprint, record no step and write no checkpoint, while a success dirties the project and earns one (`-24`, `E2E-09`); the trailing host, the Tool Rail, the precision trigger, Undo/Redo and the 48 dp floor are unchanged and no OBJ or FBX route appears (`-25`, `E2E-11`); and the seven committed corpus fixtures written by the INDEPENDENT PowerShell encoder — the two `IMPORT-01B` ones included — decode, load with the representations the specification states, and re-encode identically (`-19`) | 12 |
| `ImportedMeshSculptTest` | `IMP01B-01..14`, `E2E-IMP01B-01..07`, `-12`: sculpting an Imported Mesh on a device. *Start Sculpting* is drawn for an imported body with the product's one wording, the toolbar names the *Imported Mesh* context, and the freeze lands on that body's own `ObjectId` (`-01`, `E2E-01`); the first pre-stroke frame carries the imported vertex count, reports no edits, and the authored placement is untouched by the freeze (`-02`, `-03`, `E2E-02`); a real Grab stroke through the whole touch path mints a new `SculptRevision` and turns `SCULPT_HAS_EDITS` on while the `.forge` `IMPT` section stays byte-identical (`-04`, `-05`, `E2E-03`); *Back to Imported Mesh* is drawn, announced by its full wording, republishes the imported vertex count and leaves the source byte-identical, and *Resume Sculpt* returns the same revision, counts and edits (`-07`, `-08`, `E2E-04`, `E2E-05`); Undo/Redo are withdrawn in Sculpt, refused below JNI, and a Construction undo taken after leaving moves no sculpted vertex (`-06`); the destructive reset reads *Reset Sculpt from Imported Mesh…*, carries no Construction wording, shows no stale-source block, still asks first, still names the consequence, and cancelling keeps the work (`-09`); Save, the autosave checkpoint, a saved copy and a fresh load all carry `IMPT`+`SCUL`, the reopened project re-encodes to the same bytes and offers the way back in (`-11`, `-13`, `E2E-06`, `E2E-07`); export follows the sculpted geometry in Sculpt and the imported source outside it, records no history step and moves neither slot (`-14`); and no Construction Source is invented, the brush set is still exactly four by the domain's own answer, and no OBJ, FBX or second import route appeared (`-10`, `-24`). One case records `OWNER_REAL_FILE_01B_RETEST_PENDING` as a checked fact rather than a comment (`E2E-12`) | 9 |
| `ObjectsDeleteTest` | `IMP01B-15..24`, `E2E-IMP01B-08..11`: Delete on a device. The row's Delete is a SIBLING of the label with its own semantic id, its ObjectId tag, a content description naming the body, and 48 dp in both dimensions with the label still reachable beside it (`-23`); pressing it removes the body, records exactly one history step, leaves the selection on a body that still exists and takes the row with it (`-15`, `E2E-08`); Undo restores the project byte-for-byte and the row with it, Redo removes both again (`-18`, `-19`, `E2E-09`); an imported body carrying retained sculpt work deletes and returns whole, with no orphan `IMPT` and no orphan `SCUL` surviving (`-16`, `-17`); deleting the ACTIVE body selects the next row and the chrome follows, leaving no stale *Resume Sculpt* from the deleted body, and deleting the last row falls back to the one before it while deleting an inactive body moves nothing (`-20`, `E2E-10`); the only body's row draws no Delete and the domain refuses it as `DELETE_REFUSED_LAST_BODY` with no project byte and no history step moving, and no replacement body is invented (`-21`); Delete is withdrawn on every row while sculpting, refused as `DELETE_REFUSED_IN_SCULPT`, and back the moment Sculpt is left (`-21`); a deleted body cannot be picked and is absent from the saved document, the checkpoint and the exported GLB, with Undo restoring it to all of them and Redo omitting it again (`-22`, `E2E-11`); and the list gained Delete and nothing else — no rename, duplicate, hide, lock or group (`-24`) | 8 |
| `ProjectTransferTest` | `FSR1B-10..13`, `FSR1B-18`: Save Copy writes canonical bytes the decoder accepts and TRUNCATES a longer existing document rather than overwriting its front; Open File applies a valid document and starts a fresh history; a damaged one, an unreadable one and a cancel each change nothing below JNI; a cancelled copy cannot be written by a later pick; neither direction touches the internal manual slot; a project opened from a distinctively named file re-encodes to the ORIGINAL bytes exactly, and carries no filename, scheme, authority or path; the two native project entry points take bytes and structurally cannot take a `Uri`; and the project surface offers no GLB, glTF, OBJ, FBX or import/export entry | 13 |
| `DiagnosticsAndRendererLossTest` | `FSR1B-14..17`, `E2ER1B-07/08`: a report is written locally, is bounded, names the build and the device and never a project dimension, `.forge` magic or vertex data, and stays bounded after a flood; ForgeShape requests no INTERNET permission and the platform agrees it holds none, so nothing can be sent anywhere; sharing is a document-creation intent that writes where the user chose; an injected device loss leaves every project value and the encoded document bit-identical and either rebuilds the device or reports restart-required with the work checkpointed; the project stays editable and publishing after a rebuild; and all six project controls plus both recovery answers clear 48 x 48 dp without moving the accepted R2 right host | 9 |
| `DiagnosticLogTest` (JVM) | `FSR1B-14` core: the ring never grows past its bound, drops the oldest and says how many, caps its rendered output, truncates a detail far below anything worth hiding, and cannot be made to forge a second record from one record's contents | 11 |
| `ProjectProcessDeathTest` | `E2ER1A-02`, `-03`, `E2ER1B-01`, `-02`, across a real process death driven by `scripts/run-project-persistence-e2e.ps1`: three bodies of different kinds with non-default placements and a remembered box saved, the app killed, and the project reopened with every ObjectId, scene order, active body, active kind, remembered parameters and exact placement — 370 degrees and a non-uniform scale included — intact, geometry republished, and a post-load creation unable to collide; and a real Grab stroke saved from Sculpt reopening in Sculpt on the same body with the same vertex and index counts, its stale-source and edited state, its Construction Source companion still correct, and Back/Resume still coherent over it Stages 5-8 add the harder E2E-R1B claim: nothing was saved at all, autosave alone protected the work, and on a genuinely cold launch — no seam, no cleared flag — the recovery question offers it back, restoring the exact Construction values and the sculpted mesh with its `hasEdits` state. | 8 |
| `EditorWorkspaceUnifiedRightHostTest` | `UILR2-01..18`: invariant host frame; downward-only Transform expansion; mode/space/Exact/Shape/Sculpt parity; same-host descendants; bottom hide/restore through slowed entry/exit; IME vertical grammar and touch floor; signed numeric and unitless Scale regression; compact portrait, short landscape and expanded/tablet; Stage020R2/R3 semantics | 18 |
| `EditorWorkspaceRightHostPlacementTest` | `UILR2C-01..12`: the correction round-1 contract that the right host keeps the trailing window edge. Compact and short-landscape hold one external host frame through all eleven R2 states with the trailing inset unchanged (`-01`, `-03`); the compact anchors and the host width take no font scale, so the 1.3 cells resolve to the 1.0 answer (`-02`, `-04`); the expanded window docks the panel inboard of the host and its decision arithmetic carries no text input (`-05`, `-06`); Exact, Exact+IME and Sculpt Details never translate the host (`-07`) because a side-placed panel is always seated BEFORE it in the row they share (`-08`); compact hide/restore and short-window persistent chrome return at Δ = 0 dp (`-09`, `-10`); intrinsic hit boxes clear 48 dp with the clipped visible intersection reported separately (`-11`); and the IME leaves the Vulkan surface full-window while only the host height may move (`-12`) | 12 |

| `GlbExportTest` | `FSR1C-13..16`, `FSR1C-C1-01..14`, `E2ER1C-01..06`, `E2ER1C-C1-01..06`: every assertion made through `GlbDocument`, a test-only glTF 2.0 reader written from the specification that shares no code with the exporter. A well-formed single-file GLB with no external `uri` and a generator string, and a truncated one rejected — so the reader can fail (`FSR1C-13`); metres unscaled with the extent of a 1×2×4 m box exactly 1/2/4 on X/**Y**/Z, no node matrix anywhere and exactly one translation per body (`-14`); every body exported once in scene order as `Body_<ObjectId>`, unit normals and outward winding (`-16`). The baked contract: the node carrying translation only with rotation, scale and matrix all absent, asserted on the parsed node and again on the raw JSON (`C1-01..03`); a 1×2×4 m box arriving 2×6×2 m under a 2/3/0.5 scale and its extents swapping axes under a quarter turn (`C1-04`); no recentre, proved on a CONE because a symmetric primitive would hide one (`C1-05`); normals compared against `transpose(inverse(L))` and against `L`, computed in the test from two exports of the same sphere, plus per-face perpendicularity on a box (`C1-06`); `min`/`max` recomputed from the baked vertices (`C1-07`); two bodies baking independently (`C1-09`); byte-identical repeats (`C1-11`); and metres and +Y unchanged by the bake (`C1-12`). On device: the six-body corpus fixture exporting as six valid distinct nodes (`E2ER1C-01`) keeping its authored translations on the nodes while its scales reach the geometry (`-02`); the Sculpt fixture exporting the sculpted body's tetrahedron — identified by rotation-invariant properties, since the bake turns it — and its never-sculpted companion's Construction box (`-03`); the production SAF handler writing the exact bytes to the chosen destination and the create Intent asking for `model/gltf-binary` and a `.glb` name (`-04`); a cancel writing nothing, changing nothing and leaving no staged bytes (`-05`); and an export changing no observable native state, no revision, no history depth, no `ObjectId` and neither `.forge` slot (`-06`). Two further cases leave the sentinel `.glb` files `scripts/run-glb-export-evidence.ps1` pulls | 24 |

| `GlbImportPreviewTest` | `GLBIR0-01..20`, `E2E-GLBIR0-01..08`: the diagnostic imported mesh preview and the roundtrip it exists to measure. Both committed sentinels parse (`-01`, `-02`); the container fails closed on empty bytes, a wrong magic, truncation and a `.forge` file offered as a GLB, leaving no preview behind (`-03`); a refusal never replaces an existing preview (`-06`, `-07`); the preview adds no body, changes no active body, publishes no revision and records no history step (`-08`); Source↔Imported↔Source moves no observable native state and re-encodes the project to identical bytes (`-11`); a manual Save writes exactly the bytes it would have without the preview, and the autosave fingerprint does not move for load, show or clear, with no checkpoint written (`-09`, `-10`); Clear releases everything and the workspace comes back (`-12`); the six-body Construction project and the Sculpt project both roundtrip `ROUNDTRIP_EQUIVALENT` with the deltas reported, the sculpted body compared as `source=sculpt` and its companion as `source=construction` (`-13`, `-14`, `-15`, `-16`); a project compared against another project's file does NOT read as equivalent, so the diagnostic can fail (`-13`); the committed sentinel still matches its own source project (`-13`); the open Intent asks for a file and a cancel or an unreadable selection is a bounded no-op (`-17`); the ONE import control has a semantic id and meets the 48 dp floor, and the preview has no control of its own since `IMPORT-01A` (`-18`); preview mode leaves no editing control on screen and switching back restores the workspace (`-19`); and the project surface still offers no OBJ, FBX, material, texture or animation and frames the GLB row as Import GLB, with nothing on the surface still calling it a preview (`-20`). Four further cases leave the three comparison reports and the Source/Imported screenshot pair that `scripts/run-glb-import-evidence.ps1` pulls | 23 |

**542 tests** — 70 JVM, counted from `:app:testDebugUnitTest`'s own result XML,
and 472 instrumented, counted from the sharded runner's live AndroidJUnitRunner
discovery over 33 classes rather than from this table. Both numbers come from a
run, because a hand-maintained total drifts and this one had. No Java test
asserts a rendered pixel;
every control is reached by its stable semantic id and no assertion uses a screen
coordinate. The foundation and theme suites deliberately assert no colour
literal, radius or shadow — those are judged by eye and by runtime evidence, and
pinning them would break on every deliberate restyle, which is exactly what UI-R3
did. What the theme suite asserts instead is *relational*, plus WCAG contrast
ratios computed in the test; that is why a whole palette and every resting
background could change with no test edit beyond the one UI-R3 added.

`EditorWorkspaceCorrectionTest` asserts corner **radii**, and that is the same
rule rather than an exception to it: what it pins is `inner = outer − padding`,
a relation that survives any deliberate change to either number. A literal
"10 dp" would break on the next restyle; "concentric" breaks only when the defect
returns.

**The suite is run in TWO windows** — the default compact phone window and an
overridden 1600 x 2560 @ 240 dpi expanded window. The adaptive cases read the
window they are actually in and assert the contract that belongs to it, so an
overridden run is a genuine expanded-layout run rather than a simulation, and it
is the only thing that exercises the docked Objects and docked rail branches.

**Both windows are mandatory, and UI-R4B is why.** Two cases passed compact and
failed expanded, and neither was a product defect: `UIR4B-17` asserted the
Objects capsule's size unconditionally, and the capsule is correctly `GONE` when
the window gives Objects a column, so it measured 0 x 0; and
`r1c234` still asserted "a docked rail claims no elevation", which is exactly the
claim this stage removed. A compact-only run would have shipped both.

**`EditorWorkspaceMotionTest` asserts a RESTING state and never a frame of an
animation.** A case that sampled a transition part-way through would be a test of
the device's frame timing. It writes `animator_duration_scale` through the
instrumentation's own shell — the product holds no `WRITE_SECURE_SETTINGS` and
must never ask for one — and restores it in **both** `@Before` and `@After` so a
case that dies part-way cannot leave animation off for every suite that follows.
`EditorWorkspaceDisplayTest` restores the display defaults for the same reason.

**An appearance switch recreates the Activity**, so
`EditorWorkspaceThemeTest.switchTo` drives the real row and polls until a
workspace reports the new appearance and has been laid out; setting the field
directly would prove a field changed and nothing about whether the workspace
survives being rebuilt. Every case leaves the process in Warm Graphite.

**The start question is asked once per process, so every case that is not ABOUT
it answers it first.** `resetToBaselineConstruction` dismisses it;
`EditorWorkspaceObjectsTest` — which deliberately does not use that baseline —
dismisses it in its own `@Before`. Cases that *are* about it call
`showStartChooserAsFirstLaunch()`.

**A view that has just been made visible reports a size of 0 until a traversal has
run.** Measuring a control in the same block that opened its container is a
recurring mistake; settle the layout in between. It is also the shape of the
first-open pivot defect UI-R4B fixed in the product itself, which is why
`UIR4B-09` is worth having.

**`WorkspaceTestSupport`'s open/close helpers must be called on the UI thread.**
They drive real controls with `performClick()`, which plays a sound effect
through the view root, so calling one from the instrumentation thread throws
`CalledFromWrongThreadException` — a defect in the case that looks exactly like a
defect in the product. Wrap them in `doOnWorkspace`.

**A `-FullSharded` run interrupted mid-shard can leave the emulator unstable.**
Seen at E2E-R1A: a run stopped part-way through shard 2 to pick up a code
change, and the next `-FullSharded` attempt aborted inside shard 1 with
`INSTRUMENTATION_ABORTED: System has crashed.` — an infrastructure abort, not a
test failure. `CLAUDE.md` already names the remedy and it is the one that
worked: shut the AVD down, reboot it with
`scripts/start-forgeshape-emulator.ps1`, and rerun the WHOLE command from shard
1. A shard-only rerun cannot repair an aggregate. Prefer letting a run finish
over killing it.

**An emulator session degrades under hours of instrumentation, and the symptom
looks exactly like a lifecycle defect.** Seen at UI-R4B after roughly two hours
of continuous runs on one boot: `EditorWorkspaceStartFlowTest` began failing with
`Activity never becomes requested state "[DESTROYED]" / "[CREATED]" (last
lifecycle transition = "PAUSED")`, individual cases took minutes instead of
seconds, and `MonitoringInstr` reported "Unstopped activity count: 2" while a
single binder transaction to `IActivityManager` took 3.4 s. ForgeShape's own
surface log ruled the product out — `surfaceCreated` / `ANativeWindow released`
completed in tens of milliseconds throughout, so the blocking `surfaceDestroyed`
contract was not involved — and **the same APK ran the same eight cases 8/8 in
16 seconds on a freshly booted emulator.** The signature is not `ui11`'s, so it
was diagnosed rather than assumed: if it appears, reboot the emulator with
`scripts\start-forgeshape-emulator.ps1` and re-run before reading it as a
regression.

`EditorWorkspaceGestureTest.ui11_theImeLeavesTheFieldAndTheCommitPathUsableAndTheSurfaceUntouched`
still asks Android for the real soft keyboard first. Some emulator boots decline
that `SHOW_IMPLICIT` request even with `show_ime_with_hard_keyboard=1`; in that
case the harness now dispatches a deterministic 40% IME inset through the
workspace's real `WindowInsets` listener and first asserts that the app consumed
it. The case therefore tests product behaviour instead of failing on an emulator
precondition. Runtime evidence separately shows the real keyboard.

## Current evidence summary

Latest run (**CAD-A3-C1**), on the isolated `ForgeShape_Stage006` /
`emulator-5580` AVD, on the final runtime/test tree:
[`artifacts/cad-a3-app-h1/`](artifacts/cad-a3-app-h1/) — `INDEX.md`,
`HOME_FLOW.md`, `BOOTSTRAP_SESSION.md`, `CORPUS.md`, the CAD-A3 contracts,
`TEST_RESULTS.md`, `DEVICE_E2E.md`, `VISUAL_EVIDENCE.md` with
`OWNER_CONTACT_SHEET.png` and `captures/`, the raw startup log (19/19
`_SELFTEST_OK`, 2920 checks, zero failures, seventeen digests,
`FORGESHAPE_STARTUP_NO_PROJECT`), the focused logs with the three first-fail
transcripts, and the aggregate.

- **Instrumented (authoritative):** `scripts\run-instrumented-tests.ps1 -Serial
  emulator-5580 -FullSharded` discovered **37 classes / 504 tests** — the new
  `HomeFlowTest` (12), `CadA3VisualEvidenceTest` (1) and the widened
  `SpatialSketchTest` (8) beside the retired `EditorWorkspaceStartFlowTest`
  raise the inventory from 491 — assigned them exhaustively to five shards
  (**100 + 104 + 99 + 101 + 100**), missing = duplicates = unexpected =
  execution_missing = 0, aborted_shards = 0, and emitted
  **`FULL_SHARDED_SUITE_PASS`** on the final runtime/test tree
  (`artifacts/cad-a3-app-h1/FULL_SHARDED.txt`). Five aggregates preceded it,
  each kept as `FULL_SHARDED_RUN1..5_FAIL.txt` and each rerun from shard 1:
  a baseline-reset defect that inherited a CAD body as the box, an
  order-dependent selection assertion, a keyboard-sheet timing wait, one
  guest state that refused its IME until a real reboot, and the palette count
  that had to learn the by-name fallback. None was the product.
- **Focused:** `HomeFlowTest` OK (12), `SpatialSketchTest` OK (8),
  `SketchExtrudeTest` OK (9), `CadA3VisualEvidenceTest` OK (1), and the seven
  classes the Home change touched OK. Three rounds before the passing one were
  the new suites' own assertions (a dirty-project helper re-applying an
  identical box, an absolute history depth, a pinch direction, the grid's
  pre-sample fallback, a rectangle value slot) plus one emulator
  `system_server` crash that the guest recovered from on its own.
- **JVM:** 70/70. **Native:** 19 suites, 2920 checks. **Runner:** 17 suites,
  2567 checks. **Builds:** debug and release, both ABIs. **Guards:**
  `DEV2-01..07`, `DEV3-01..06`. **Corpus:** twenty-two fixtures verified,
  PowerShell = C++ on all six v2 fixtures after the FNV basis fix.

Previous run (**CAD-R0-A1A2**), on the isolated `ForgeShape_Stage006` /
`emulator-5580` AVD, on the final tree:
[`artifacts/cad-r0-a1a2/`](artifacts/cad-r0-a1a2/) — `INDEX.md`, the domain,
workplane, entity, profile, triangulation, extrude, regeneration and data
contracts, the corpus table, the `CADR0-01..40` results, the `E2E-CADR0-01..16`
table, the performance record, the raw startup log (18/18 `_SELFTEST_OK`, 2845
checks, zero failures, eleven digests) and the raw focused and aggregate logs.

- **Instrumented (authoritative):** `scripts\run-instrumented-tests.ps1 -Serial
  emulator-5580 -FullSharded` discovered **35 classes / 491 tests** — the new
  `SketchExtrudeTest` raises the inventory from 482 — assigned them exhaustively
  to five shards (**100 + 98 + 99 + 97 + 97**), missing = duplicates =
  unexpected = 0, and emitted **`FULL_SHARDED_SUITE_PASS`**. A first aggregate
  failed one pre-existing palette test (it counted creation actions one level
  deep, ignoring visibility, against the palette's new two-section structure);
  the test was corrected to count visible actions recursively and expect New
  Sketch as the seventh, and the aggregate was rerun once from shard 1.
- **Focused:** `SketchExtrudeTest` OK (9 tests). **JVM:** 70/70. **Native:**
  18 suites, 2845 checks. **Builds:** debug and release, both ABIs. **Guards:**
  `DEV2-01..07`, `DEV3-01..06`. **Corpus:** sixteen fixtures verified.

Previous run (**IMPORT-01B**), on the isolated `ForgeShape_Stage006` /
`emulator-5580` AVD, on the final tree:
[`artifacts/import-01b/`](artifacts/import-01b/) — `INDEX.md`, the sculpt-source
and delete contracts, the `.forge` data-contract decision, the corpus table, the
test results, the device E2E table, the raw startup log (17/17 `_SELFTEST_OK`,
2628 checks, zero failures) and twelve raw instrumented logs.

- **Instrumented (authoritative):** `scripts\run-instrumented-tests.ps1 -Serial
  emulator-5580 -FullSharded` discovered **33 classes / 472 tests** — the two new
  `IMPORT-01B` classes raise the inventory from 455 — assigned them exhaustively
  to five shards (**93 + 96 + 92 + 95 + 96**) and passed **472/472**.
  `missing=0`, `duplicates=0`, `unexpected=0`, `execution_missing=0`,
  `failed_shards=0`, `aborted_shards=0`; marker `FULL_SHARDED_SUITE_PASS`
  (`artifacts/import-01b/FULL_SHARDED.txt`).
- **The aggregate earned its cost twice.** It caught a real product defect — an
  unbounded GPU fence wait in the new renderer release path, which hung the
  render thread and with it `surfaceDestroyed` and the Activity teardown — and a
  real test defect that assumed process-scoped state. Both were corrected, each
  proved against the exact condition that exposed it, and the whole command
  rerun from shard 1 on the corrected tree.
- **Four device failures, none of them the product.** One `system_server` death
  (no ForgeShape tombstone, no crash-buffer entry) and three installer wedges.
  The root cause of the wedge was that the sanctioned launcher passes no
  `-no-snapshot-load`, so restarting the AVD RESUMED a snapshot carrying the
  broken system — `system_server` kept the same pid across the restart. A real
  guest reboot (`adb -s emulator-5580 reboot`) cleared it. `adb kill-server` was
  never used and `emulator-5554` was never contacted.

Latest run (**ARCH-HEALTH-01**), on the isolated `ForgeShape_Stage006` /
`emulator-5580` AVD, on the corrected tree:
[`artifacts/architecture-health-review/`](artifacts/architecture-health-review/)
— `INDEX.md`, the module map, the comment audit, the 15-area scorecard, the
hotspot verdicts, the safe-fix list, the four change-surface probes, the raw
startup log (17/17 `_SELFTEST_OK`, zero failures) and the raw output of five
focused instrumented classes. Focused classes are subset evidence; no
`FULL_SHARDED_SUITE_PASS` is claimed for this run.

Latest acceptance run (**GLB-IMPORT-R1**), on the isolated `ForgeShape_Stage006`
/ `emulator-5580` AVD. Evidence:
[`artifacts/glb-import-r1/`](artifacts/glb-import-r1/) — `INDEX.md`, the widened
supported-subset and refusal table, the owner-sample compatibility target, the
Nomad-like fixture's structure and digest, a screenshot of it imported, the
device case table and the full-suite aggregate.

- **Native self-tests:** seventeen suites, **2528 checks, zero failures**, of
  which the GLB import/durable-import suite is 184 and the `.forge` suite 184.
- **The external subset:** the deterministic Nomad-like fixture imports as one
  mesh, 1978 vertices, 3780 triangles and seven draw batches, with baked world
  bounds matching an independent third reader's computation from the same bytes.
- **The owner's `1 lowpoly.glb`:** `OWNER_SAMPLE_RUNTIME_TEST_PENDING` — the
  binary is external to this coding environment.

The R0 package stays at [`artifacts/glb-import-r0/`](artifacts/glb-import-r0/)
and is unchanged: `ROUNDTRIP_EQUIVALENT` for the Construction project, the
Sculpt project and the committed sentinels, with `max_position_delta_m=0` and
`max_normal_delta_deg=1.20741827e-06`. Those roundtrip cases still pass under
R1.

### Previous acceptance run (E2E-R1C-C1)

Evidence:
[`artifacts/e2er1c/`](artifacts/e2er1c/) — `INDEX.md`, the coordinate-authority
proof, `BAKED_TRANSFORM_C1.md`, the device case table, the full-suite aggregate,
the startup capture, both sentinel `.glb` files and the prepared owner check.
The E2E-R1B package stays at [`artifacts/e2er1b/`](artifacts/e2er1b/) and the
E2E-R1A package at [`artifacts/e2er1a/`](artifacts/e2er1a/); both are historical.

- **Native self-tests:** sixteen suites, **2292 checks, zero failures**, then
  `FORGESHAPE_NATIVE_VIEWPORT_OK` (`artifacts/e2er1c/selftest_startup.txt`).
- **Instrumented:** `FULL_SHARDED_SUITE_PASS` — 28 classes, 406 tests
  discovered live, executed exactly once, `missing=0 duplicates=0 unexpected=0
  execution_missing=0 failed_shards=0 aborted_shards=0`
  (`artifacts/e2er1c/FULL_SHARDED.txt`).
- **Exported sentinels, CURRENT (baked, `ARCH-OWNER-07`):**
  `construction_sentinel.glb` 59884 bytes
  `8ac4fb6abb509361b911c9b4bbc2dd8385910668808b40fbd9faeab244f2c3ff`;
  `sculpt_sentinel.glb` 2588 bytes
  `ae82a0720d9f503f77d0237edf40ab5c1152bae6938df08166e1ea9b31014f2c`.
- **Superseded, PRE_ARCH_OWNER_07 — history, not current:** the pre-bake pair
  was 60312 bytes `266355a5…` and 2684 bytes `16805a05…`, in Git at
  `cdc184b999bae716809f632c11fe771ebbb98c2b`. **The files in the bundle no
  longer have those digests.**

### Previous acceptance run (E2E-R1B / Stage 022)

Evidence: [`artifacts/e2er1b/`](artifacts/e2er1b/).

- **Native self-tests:** fifteen suites, **2199 checks, zero failures**, then
  `FORGESHAPE_NATIVE_VIEWPORT_OK` (`artifacts/e2er1b/native-launch.txt`). The
  project suite is **132** and the new render-recovery suite **24**. The golden
  `.forge` digests are **unchanged** from E2E-R1A, which is the R1A-compatibility
  gate: the format did not move.
- **Build/JVM:** debug and release APKs built for both ABIs; **70/70 JVM** tests
  passed, zero failures, zero errors (11 new, all `DiagnosticLogTest`).
- **Focused `FSR1B`:** `ProjectAutosaveRecoveryTest` (14),
  `ProjectTransferTest` (13) and `DiagnosticsAndRendererLossTest` (9) run
  together, **36/36** (`artifacts/e2er1b/focused-fsr1b.txt`).
- **Process death:** `scripts/run-project-persistence-e2e.ps1` passed all
  **eight** stages with a confirmed absent process before each verifying half;
  `PROJECT_PERSISTENCE_E2E=PASS` (`artifacts/e2er1b/process-death-e2e.txt`).
  Stages 5-8 are the E2E-R1B claim: nothing was saved.
- **Regression:** the E2E-R1A project-actions suite and the UI-LAYOUT-R2
  right-host, placement, correction and legibility suites re-run together,
  **94/94** (`artifacts/e2er1b/regression-focused.txt`).
- **Instrumented (authoritative):**
  `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded
  -ShardCount 5` discovered 27 classes / **382** tests — the four new classes
  raise the inventory from 342 — assigned them exhaustively to five shards
  (**76 + 79 + 78 + 75 + 74**) and passed **382/382**. `missing=0`,
  `duplicates=0`, `unexpected=0`, `execution_missing=0`, `failed_shards=0`,
  `aborted_shards=0`; marker `FULL_SHARDED_SUITE_PASS`
  (`artifacts/e2er1b/full-sharded.txt`). An earlier attempt failed in shard 5 on
  an order-dependent assertion in the new autosave suite — it compared the scene
  against a literal body count, which is only true when the twenty-body
  scalability case has not run first — and the whole command was rerun from
  shard 1 after the fix, as `CLAUDE.md` requires.
- **GPU device loss:** the REBUILD branch, verified on device through the debug
  injection seam — `DEVICE_LOST:acquire` to `DEVICE_REBUILT` to a second
  `NATIVE_VIEWPORT_OK` in about 130 ms, with every project value and the encoded
  document bit-identical across it
  (`artifacts/e2er1b/render-device-loss.txt`).
- **Device guards:** `DEV2-01..07` and `DEV3-01..06` all PASS
  (`artifacts/e2er1b/device-guards.txt`).

The E2E-R1A run below is retained for the measurements this stage did not
repeat.

- **Native self-tests:** fourteen suites, **2163 checks, zero failures**, then
  `FORGESHAPE_NATIVE_VIEWPORT_OK` (`artifacts/e2er1a/native-launch.txt`). The
  new project suite contributes **120** (`FSR1A-01..15`) and prints
  `FORGESHAPE_PROJECT_GOLDEN_SHA256`, whose two digests equal the committed
  corpus's exactly.
- **Golden corpus:** seven v1 fixtures under `testdata/forge/v1/`, written and
  re-verified by `scripts/build-forge-corpus.ps1` — a **second, independent**
  implementation of the specification. Digests in
  `artifacts/e2er1a/corpus-digests.txt` and `DATA_PACKAGE_SPEC.md`.
- **Build/JVM:** debug APK built; **59/59 JVM** tests passed, zero failures,
  zero errors.
- **Both ABIs, debug and release:** `arm64-v8a` and `x86_64` build in both
  configurations and both ship in the release APK; every `LOAD` segment is
  16 KB-aligned (`p_align 0x4000`) in all four binaries
  (`artifacts/e2er1a/abi-and-alignment.txt`).
- **Focused persistence:** `EditorWorkspaceProjectActionsTest` (`E2ER1A-01`,
  `-04`, `-05`, `-06`) passed **7/7**
  (`artifacts/e2er1a/focused-project-actions.txt`).
- **Process death:** `scripts\run-project-persistence-e2e.ps1 -Serial
  emulator-5580` passed all four stages with a confirmed absent process before
  each verifying half; `PROJECT_PERSISTENCE_E2E=PASS`
  (`artifacts/e2er1a/process-death-e2e.txt`).
- **Instrumented (authoritative):**
  `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -FullSharded
  -ShardCount 5` discovered 24 classes / **342** tests — the two new persistence
  classes and one new `UIR4B-09` case raise the inventory from 330 — assigned
  them exhaustively to five shards (**69 + 68 + 68 + 69 + 68**) and passed
  **342/342**. `missing=0`, `duplicates=0`, `unexpected=0`, `execution_missing=0`,
  `failed_shards=0`, `aborted_shards=0`; marker `FULL_SHARDED_SUITE_PASS`
  (`artifacts/e2er1a/full-sharded.txt`). An earlier attempt aborted inside shard
  1 with `INSTRUMENTATION_ABORTED: System has crashed.` after a previous run had
  been killed mid-shard; the AVD was rebooted with
  `scripts\start-forgeshape-emulator.ps1` and the whole command rerun from shard
  1, as `CLAUDE.md` requires.
- **R2 right-host regression:** `EditorWorkspaceUnifiedRightHostTest`,
  `EditorWorkspaceRightHostPlacementTest` and `EditorWorkspaceCorrectionTest`
  re-run together, **67/67**
  (`artifacts/e2er1a/right-host-regression.txt`). `UIR4B-08`'s surface count
  moved from four to five and `UIR4B-09` gained the project surface — the
  tripwire fired on the new surface exactly as intended, and it caught a real
  defect: the surface grew from its leading edge instead of the trailing corner
  its control sits at.
- **Six-cell anchor matrix:** unchanged from UI-LAYOUT-R2 and not re-measured;
  the accepted right host did not move. 66 host rows measured at exact `wm size` /
  `wm density` / `font_scale` triples across C10/C13/L10/L13/E10/E13. All 66
  conform to `UI-SPEC-R0 Revision 1` inside ≤ 4 dp; intra-cell drift 0 dp;
  resting chrome Δ = 0 dp; 1299 control rows with **zero** intrinsic hit-area
  violations; the Vulkan surface full-window under the IME in all six.
- **Window profiles:** compact portrait, 2400 × 1080 @ 420 dpi short landscape,
  and 1600 × 2560 @ 240 dpi expanded/tablet all passed their R2 contracts and
  are represented in runtime evidence.
- **Device guards:** `DEV2-01..07` and `DEV3-01..06` all PASS. No command touched
  reserved `emulator-5554`; every runtime command used explicit
  `-s emulator-5580` after confirming the AVD name.
- **Runtime evidence:** [`artifacts/e2er1a/`](artifacts/e2er1a/) is the current
  package: the native launch log, the corpus digests, the ABI/alignment record,
  the focused persistence run, the process-death transcript, the right-host
  regression and the full-sharded aggregate. The UI-LAYOUT-R2 packages remain
  where they were made and are historical.
  [`artifacts/uilayoutr2-correction/`](artifacts/uilayoutr2-correction/) holds
  the R2 measurement: 36 raw screenshots beside 36 mechanical overlays for six
  visual-critical states in six cells, the four CSVs, the harness, the derived
  `analysis.txt` and an owner review `INDEX.md`. The superseded pre-correction
  measurement stays at
  [`artifacts/uilayoutr2-spec-closeout/`](artifacts/uilayoutr2-spec-closeout/),
  and [`artifacts/uilayoutr2/`](artifacts/uilayoutr2/) holds the original R2
  walkthrough. The six representative pairs the coordinator reviewed are packaged
  in [`artifacts/uilayoutr2-owner-review/`](artifacts/uilayoutr2-owner-review/);
  that bundle records no verdict of its own, and the visual PASS of 2026-08-30
  is the coordinator's, recorded here.
- **Physical ARM64 (Gate P1):** closed on a Galaxy S25 Ultra —
  `primaryCpuAbi=arm64-v8a`, `PAGE_SIZE` 4096, the mandatory ~10k/~50k/~100k
  ladder and Sculpt at 100k measured on real hardware. Stylus stays UNVERIFIED.
- **Runtime walkthrough (Stage 020R2),** on `ForgeShape_Stage006` /
  `emulator-5580`. Every chrome control was resolved from a live `uiautomator
  dump` by its stable semantic id, and **every handle pixel was read back from
  native code** through the debug driver (keyevent `35`) immediately before the
  gesture that used it. Cold start: thirteen `*_SELFTEST_OK`, zero failures.
  Holding Transform drew **seven** new ids — `transform_mode_group`,
  `_move`/`_rotate`/`_scale`, `transform_space_group`, `_world`/`_local` — and
  `GIZMO_HANDLES:move/world` listed the plane handles beside the axes.
  * A **World XY plane** drag logged `DRAG_BEGIN:move axis=xy` then
    `DRAG_COMMIT:recorded updates=30 undo=1` and left
    `pos=(0.517990,0.520885,0.000000)` — thirty samples, one step, and **Z
    exactly 0.000000**: the plane constraint is arithmetic, not a correction.
  * A **World Y ring** drag left `rot=(0.000000,19.570949,0.000000)` — X and Z
    **exactly zero**, which is `kEulerStickyDegrees` doing its job in the shipped
    product rather than in a test.
  * Switching to **Local** moved the reported ring pixels (x=553.6,1033.3 →
    600.5,1044.5) and a Local X ring drag gave
    `rot=(117.584251,19.570949,0.000000)` — the Y component **bit-identical** to
    what it was, which is exactly `R_start · Relem(X, Δ)`.
  * A **Local X move** on that mixed body gave `pos=(-0.046678,0.513135,
    0.198009)`: X and Z both moved and Y stayed bit-identical, because that
    body's local X is `(0.942, 0, -0.335)`. A world X move cannot do that.
  * In **Scale** the space selector was **ABSENT** from the live dump. An axis,
    a plane and a uniform drag were three steps (`undo=5,6,7`) and left
    `scale=(2.134748,2.211988,2.211988)` — the YZ plane's two components
    **bit-identical** to each other — with position and rotation untouched.
  * Opening the exact values showed `Pos X -0.04667829`, `Rot X 117.5842512`,
    `Rot Y 19.5709491`, **`Rot Z 0`** and `Scale 2.134747617 / 2.211987980 /
    2.211987980` under a `Scale (multiplier)` heading with no unit — the gizmo's
    own result, in the fields.
  * A second body was added and dragged; only it moved, and **only the active
    body carried a gizmo**. In Sculpt both selectors were **ABSENT** and the
    driver reported `GIZMO_HANDLES:absent`. That pre-R2 capture used a horizontal
    short-window selector reflow; UI-LAYOUT-R2 supersedes it with one vertical,
    internally scrollable right host in every profile.
  * The two-finger cancel is still the one case `adb shell input` cannot produce
    — it cannot inject a genuine concurrent second pointer — and is covered
    instead by `S020-32` and `S020R2-13`, which dispatch real two-pointer
    `MotionEvent`s and pass in all three windows.
- **Runtime walkthrough (Stage 020),** on `ForgeShape_Stage006` /
  `emulator-5580`. Every chrome control was resolved from a live `uiautomator
  dump` by its stable semantic id, and **every handle pixel was read back from
  native code** through the debug driver (keyevent `35`) immediately before the
  gesture that used it — no coordinate was written down or reused between steps.
  Cold start: thirteen `*_SELFTEST_OK`, zero failures,
  `FORGESHAPE_SESSION_INIT_END undo=0 redo=0` and **both history controls drawn
  disabled**. Holding Transform logged `FORGESHAPE_GIZMO_ACTIVE:1` and drew the
  handles with the Move/Rotate selector beside the rail. A finger drag on the X
  shaft logged `DRAG_BEGIN:move axis=x objectId=1` then
  `DRAG_COMMIT:recorded updates=42 solve=resolved undo=1` — **forty-two samples,
  one step** — with **no `CAMERA_STATE`, no `CAMERA_ORBIT_OK` and no `PICK_`
  line**, so the camera never saw the gesture and no tap was resolved. A drag
  that started off the handles logged `CAMERA_ORBIT_OK` and a new
  `CAMERA_STATE`, moved no body, and the pivot followed the camera on screen.
  Opening the exact values after two drags showed **Pos X 1.039234161, Pos Y
  0.45292168…, Z and all three rotations 0** — the gizmo's own result, in the
  fields. HOME pressed mid-drag logged `GIZMO_DRAG_CANCEL updates=1 undo=7`: the
  system took the gesture, the placement went back, and the depth did not move.
  A 36-sample Move drag plus its commit, Undo and Redo logged **0
  `CONSTRUCTION_PUBLISHED` and 0 `MESH_UPLOAD_OK`**, with
  `republished=0 placements=1` on both history steps; the counter is not blind —
  the very next act, one driver-applied box state, logged both immediately. A
  second body was created, dragged on its own axis, and only it moved. Rotation
  to landscape and HOME/resume both retargeted the handles to the new window and
  left Undo enabled. In Sculpt the selector, both mode controls and the handles
  were all **ABSENT**, and the driver reported `GIZMO_HANDLES:absent`. The
  two-finger cancel is the one case `adb shell input` cannot produce — it cannot
  inject a genuine concurrent second pointer — and is covered instead by
  `S020-32`, which dispatches a real two-pointer `MotionEvent` and passes in all
  three windows.
- **Runtime walkthrough (Stage 019),** on `ForgeShape_Stage006` /
  `emulator-5580`, every control resolved from a live `uiautomator dump` by its
  stable semantic id and no coordinate reused between steps. Cold start: the
  then-twelve
  `*_SELFTEST_OK`, zero failures, **both controls drawn disabled**. An exact
  Shape Apply (box width retyped to 3.75 m) logged one
  `CONSTRUCTION_PRIMITIVE` and one `CONSTRUCTION_PUBLISHED`; Undo then logged
  `republished=1 placements=0 restored=0 removed=0 undo=0 redo=1 bodies=1` —
  **exactly one publication** — and Redo the same. Add Primitive → Sphere logged
  `SCENE_BODY_ADDED:2`, `CONSTRUCTION_PRIMITIVE:ui kind=sphere` and **one**
  `CONSTRUCTION_HISTORY_COMMIT:recorded undo=2`; its Undo logged
  `republished=0 removed=1 bodies=1` — the body gone in one step with **zero
  publications, so no default Box was ever put on screen** — and its Redo
  `restored=1 bodies=2` with **zero republication**, because a body held by the
  history keeps its own revision. Undo, then a different real creation, left
  Redo disabled. Rotation to landscape and HOME/resume both kept the stacks and
  the drawn state, and Undo still worked afterwards. In Sculpt both controls
  were **ABSENT**. A real Grab stroke (sculptRev 27→35, then 34 in the
  stale-source pass) wrote **no** Construction step; a Box→Cylinder Apply on the
  sculpted body logged `SCULPT_SOURCE_STALE:ui sculptRev=34`, its Undo logged
  `republished=1`, and Resume Sculpt returned
  `sculptRev=34 v=482 i=2880 objectId=2 strokes=1 freezes=1 stale=1` — identical
  revision, counts, stroke history and identity, with only the pre-existing
  stale flag standing. Measured hit areas: 126 × 126 px at 420 dpi (compact and
  short landscape) and 72 × 72 px at 240 dpi (expanded) — **48 dp in all three**.
  No crash and no ObjectId aliasing throughout.
- **Runtime walkthrough (UI-R4C):** cold start and start chooser into
  Construction; the resting workspace with **no instructional capsule** and
  **Start Sculpting drawn as one pill**; Add Primitive opened and confirmed clear
  of the tool cluster; exact Shape opened and dismissed with **no global
  instruction added**; Start Sculpting into the Sculpt workspace with **the whole
  of *Back to Construction* readable**; back, then Resume Sculpt on the same
  cleaned geometry; short landscape and the expanded window with the same back
  navigation readable in both; Neutral Charcoal and Light Charcoal. No crash, no
  new clipping and the viewport usable throughout. Every control was driven by
  resolving its stable semantic id from a live `uiautomator dump`; no coordinate
  was reused between steps. Earlier walkthroughs are in Git history.
- **Zero geometry from pure UI, measured rather than asserted.** With the log
  cleared, a sequence of Add Primitive open/close, the scene list open/close, both
  rail contexts, the precision surface open/close, Grid off/on, two appearance
  switches and a rotation round trip logged **0 `MESH_UPLOAD_OK` and 0
  `*_PUBLISHED`**. The counter is not blind: the very next act, one driver-applied
  box change, logged exactly 1 of each.
- **Screenshot review sets:** `artifacts/uir4a/` (UIR4A-S01..S12 plus three
  walkthrough frames), `artifacts/uir4b/` (UIR4B-S01..S12), `artifacts/uir4c/`
  (UIR4C-S01..S10), `artifacts/stage019/` (S019-S01..S06 — the resting
  workspace with both controls disabled, Undo live after an edit, Redo live
  after an undo, short landscape, expanded, and Sculpt with the pair absent),
  `artifacts/stage020/` (S020-S01..S10, the axis-only gizmo) and
  `artifacts/stage020r2/` (S01..S10 — Move with a plane handle, Move in Local,
  Rotate in World and in Local, the three kinds of Scale, Exact Transform showing
  the scale, short landscape, expanded, multi-object, and Sculpt with no gizmo),
  all **committed**. `/artifacts/` is
  deliberately not ignored because the cited runtime evidence is versioned
  repository content; each review's set sits under its own folder so the reviews
  can be compared rather than one overwriting the next.

**One caveat about capturing self-test evidence.** On both the emulator and the
physical phone the logcat ring buffer intermittently drops whole suites from the
*middle* of a startup capture, which reads exactly like a suite that never ran.
The reliable signal is that the suites which do appear always report their full
expected check counts and `_SELFTEST_FAIL` is always absent; read a partial
capture as "no failures observed", and re-run until one capture is complete before
quoting a total.

## Display-control research, and what was rejected

**Adopted:** the popover grows from the control that opened it rather than sliding
in from a screen edge; selection feedback happens **in place**, so the surface
does not move, re-animate or close on a choice — the one that matters, because
comparing two options means switching repeatedly; non-modal, staying open across
several changes; grouped labelled chip rows rather than one long list, as layout
and not as styling.

**Rejected:** live preview thumbnails with a check badge per option — a rendered
thumbnail per shading mode needs a second offscreen render target, for a choice
whose result is already filling the screen behind the panel; and a bottom sheet
with drag detents — a sheet covers the model, which is the thing being judged, and
ForgeShape already has a Property Inspector for sheet-shaped content.

Nothing decorative was taken. There is no continuous or looping model animation,
no motion on the viewport itself, and no animation anywhere on the path of a
pointer sample. Both animations are interruptible, and a zero system animator
duration scale skips them outright rather than shortening them.

## Known Issues / Blockers

- **An instrumented run leaves a project in the app's slot.** The process-death
  suite has to: its two halves communicate through the saved file, which is the
  whole point. `EditorWorkspaceProjectActionsTest` deletes the slot after every
  case so its injected damage cannot be inherited, but `ProjectProcessDeathTest`
  deliberately leaves a real project behind. Evidence taken after an instrumented
  run therefore starts with *Open Saved Project* enabled and a project already
  saved. Clear it with
  `adb -s <serial> shell run-as com.forgeshape.app rm -f files/project.forge`,
  or reinstall the app.
- **A crashed emulator session can leave the quickboot snapshot corrupt, and
  every later launch then hangs before `adbd` starts.** Seen on
  `ForgeShape_Stage006` after the INPUT-R1 session: QEMU runs and
  `adb -s <serial> emu avd name` answers, so the AVD looks alive, but the serial
  never leaves `offline` and `start-forgeshape-emulator.ps1` reports BLOCKED at
  its 180 s deadline. The tell is `snapshots\default_boot\ram.img.dirty` plus a
  `ram.bin` of a few hundred bytes against a multi-GB `ram.img`. Deleting
  `snapshots\default_boot` fixes it — the emulator cold-boots and rebuilds the
  snapshot on its next clean exit. `userdata-qemu.img.qcow2`, `config.ini` and
  the AVD definition are untouched, so nothing authored is lost.
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
  capture method fully prevents it.** It caps the buffer at 5 MiB (`logcat -G` is
  silently reduced whatever is asked for) and whole suites vanish from the *middle* of a capture,
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
  `adb -s <serial> logcat -G 64M` first, and confirm it took with
  `adb -s <serial> logcat -g`. 16M was enough through Stage 020 and no longer is:
  at E2E-R1A a 16M capture on the EMULATOR silently lost two of the fourteen
  suites, with no `chatty` marker and no FAIL line to give it away.
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

**CAD-R0-A1A2, recorded as bounded debt with timing LATER:**

- **CAD → Sculpt is refused by name, not offered.** `buildSculptSourceMesh`
  returns false for a CAD Body, `freezeToSculpt` answers
  `SCULPT_REFUSED_CAD_BODY`, *Start Sculpting* is absent for one, and the codec
  refuses `SCUL` over `CADB`. Enabling it needs three decisions: the wording of
  the way back out of Sculpt over a CAD Body (a third abbreviated label needs
  its own approval), the stale-source rule over a CAD edit, and the
  `CADB`+`SCUL` file combination with its own fixture. Each is small; none is
  this stage's.
- **A polygon profile's points are not numerically editable.** A closed
  polyline or a loop of lines extrudes and its depth is editable; its vertices
  are shown by count only. Rectangle and circle sizes are the R0 editable
  dimensions, as specified.
- **The sketch grid is fixed at 0.25 m / 1 m and 8 m half extent.** An
  adaptive multi-decade grid is a CAD grid system with its own contract.
- **The top view sits at the pitch clamp (≈ 87°), not exactly vertical.** The
  ray–plane mapping is exact regardless; only the presentation is
  near-orthographic. Lifting the clamp for the sketch view alone would put a
  degenerate look-at into the camera for one mode.
- **The sketch session is process-scoped and volatile.** A rotation keeps it
  (native state survives the Activity), process death loses it by contract, and
  a project Open or Recover cancels it. Edit-session recovery is not a feature.
- **`CADB` v1 is exactly one sketch and one extrusion.** A second feature kind
  takes a new section version; nothing pretends a feature tree exists.

**RESOLVED at IMPORT-01B: the renderer now prunes a body that left the
snapshot.** ARCH-HEALTH-01 recorded this as bounded debt with timing LATER, and
`UI-OWNER-45` made it unbounded: add and delete in a loop mints a fresh
`ObjectId` every cycle, so `Renderer::bodies_` would grow one entry per cycle
forever, each holding two `VkDeviceMemory` allocations against a device limit
commonly around 4096. `releaseBodiesAbsentFromScene`, at the top of `syncScene`,
frees the GPU copy for any body the current scene no longer names, after waiting
on every in-flight frame and only when there is something to free. An Undo costs
one re-upload, through the path a body already takes the first time it is drawn.

**Export and the roundtrip diagnostic extract a body's geometry twice.**
(ARCH-HEALTH-01.) `forgeshape_gltf_export.cpp` and `forgeshape_glb_roundtrip.cpp`
each re-evaluate a Construction Source or read an Imported Mesh's arrays with
their own `if/else` over the representation. Two sites is bounded; a third body
representation would be the moment to give them one shared "interchange
geometry of a body" function, and turning the Java `sceneActiveBodyIsImported()`
boolean into a representation query would come with it. Timing:
BEFORE_NEXT_MAJOR_FEATURE.

**The `.forge` decoder does not bound `nextObjectId` below the preview key
range.** (ARCH-HEALTH-01.) `validateProjectDocument` requires
`nextObjectId > highest` and refuses duplicates, but a hand-crafted file can
state a value at or above `kFirstPreviewRenderKey` (2^60), after which the
diagnostic preview and a real body could share a renderer key. Reachable only
from a crafted file with the preview driven from a test; no user path exists.
It is a `.forge` validation rule, so it is reported for the coordinator rather
than changed. Timing: LATER.


**The preview's renderer keys occupy the `ObjectId` type.** (GLB-IMPORT-R0.)
`ARCH-OWNER-08` says the preview has no ForgeShape `ObjectId`, and it has none
in every sense that matters: nothing is minted by the scene's allocator, the
preview is in no body list, is not selectable, is not persisted and is not an
identity anywhere. But the renderer caches GPU buffers keyed by
`SceneDrawItem::objectId`, so preview meshes need *some* distinct key, and they
take one from a reserved range at 2^60 that the allocator cannot reach —
`previewRenderKeyIsReserved` names the boundary and a self-test asserts the two
ranges cannot meet. A cleaner shape would give the renderer its own key type, or
its own small preview collection, and both are real Vulkan work this diagnostic
did not need. Recorded rather than hidden: the values are `ObjectId`-typed, and
that is the one place the letter of the rule and its spirit differ.

**The preview's normals are parsed but not shaded with.** (GLB-IMPORT-R0/R1.)
The importer decodes NORMAL — or generates one when the file states none — and
the roundtrip compares it, but the drawn preview uses one flat neutral colour
and the renderer's own derived normals, because `MeshVertex` carries position
and colour and adding a normal to it would change the upload path for every body
to serve a diagnostic. The comparison is unaffected: it reads the parsed normals
directly, not the drawn ones. The practical consequence is that R1's generated
normals are validated numerically and are not what the viewer sees shaded.

**A preview batch re-uploads its mesh's whole vertex array.** (GLB-IMPORT-R1.)
R1 keeps each TRIANGLES primitive as its own draw batch so per-primitive
`doubleSided` survives, and each batch is published as its own `RuntimeMesh`
over the same baked vertices — so a seven-primitive file uploads seven copies of
its vertex block. The COUNTS reported stay the file's own, because the importer
decodes a shared POSITION accessor once. Sharing one GPU vertex buffer between
draws needs a renderer that separates buffers from draw items, which is real
Vulkan work a diagnostic did not need at these sizes (a 2k-vertex character is
under a megabyte either way).

**The reserved-control styling is now dead code.** (E2E-R1C.)
`EditorControlStyles.setChipReserved` and `res/drawable/bg_capsule_reserved.xml`
had exactly one caller, the reserved Export chip, and it is gone. Both were left
in place rather than deleted, because deleting them would have changed the
product tree after the authoritative sharded aggregate was taken and bought a
second full run for two unreferenced declarations. They are unreferenced,
compile to nothing that runs, and are the first thing to remove in the next
stage that touches `EditorControlStyles` — or the thing to reuse, if a control
is ever approved as reserved again. The `fsTextDisabled` palette role is NOT
part of this: it is still read by `res/color/control_content_tint.xml` and is
still asserted by the theme suite.

**Export cannot say WHY it refused.** (E2E-R1C.) `NativeViewport.exportGlb()`
returns null for every refusal — an empty scene, a mesh the writer will not
vouch for, a model past the 512 MB ceiling — so the status line says the one
thing true of all of them and the specific reason goes to the log as
`FORGESHAPE_GLB_EXPORT_FAIL:<why>`. `GlbExportStatus` already carries the
distinction natively; surfacing it needs a second JNI out-parameter and three
more strings, which is a cost this slice did not pay for. Every one of those
causes is currently unreachable from the product's own state except the size
ceiling.

**One project slot, and one recovery copy.** (E2E-R1A shape, narrowed by
E2E-R1B.) There is exactly one app-private manual `.forge` file and Save
replaces it, so there is still no way to keep two projects, no naming, no recent
list, and no way back to a project a later Save overwrote. Autosave protects
UNSAVED work — it keeps one recovery copy and no version history — so the
remaining exposure is narrower than it was: closing ForgeShape without saving no
longer loses the session, but overwriting a saved project still cannot be
undone. Save As, naming and a project library remain `APP-H1`'s.

**A reopened sculpt mesh restarts its `SculptRevision`.** (E2E-R1A, accepted.)
Revision NUMBERS are derived state and are deliberately not file truth, so a
loaded mesh begins at the revision a freeze starts at. The one thing that
depended on the number — `hasEdits()`, which the destructive *Reset Sculpt from
Shape* guard asks — is carried across as an explicit boolean and restored, so the
guard still warns. What is genuinely lost is the numeric value itself, which no
product behaviour reads.

**A reopened sculpt mesh loses its vertex colours.** (E2E-R1A, accepted.)
`MeshVertex` interleaves a position and a colour, and the colour feeds only the
debug-only source-colour shading mode — Studio Solid and MatCap both ignore it.
Storing three floats per vertex for a debug view would grow every sculpt file by
a third, so a loaded sculpt vertex is given one neutral value. The debug shading
mode therefore renders a reopened sculpt mesh flat grey; nothing a release user
can reach is affected.

**The corpus generator is a second implementation and must be kept in step.**
(E2E-R1A, accepted.) `scripts/build-forge-corpus.ps1` deliberately re-implements
the `.forge` v1 encoder from `DATA_PACKAGE_SPEC.md`, which is what makes the
portability claim evidence rather than assertion — and it is also a second place
a format change has to land. `FSR1A-12` fails loudly when the two part company,
and `FORGESHAPE_PROJECT_GOLDEN_SHA256` prints the new digest beside the failure,
so the cost is a visible one rather than silent drift.

**Autosave protects the current work, not a history of it.** (E2E-R1B, by
design.) One recovery copy, replaced as the project changes. There is no version
history, no snapshot browser, no way back to an earlier point in the session,
and no protection at all against a deliberate Save over a project the user
wanted to keep. Undo covers a session; this covers a crash; neither covers "I
saved over the wrong thing".

**The recovery question is asked once per process, and a real crash is what
re-asks it.** (E2E-R1B, accepted.) An unanswered question survives an Activity
recreation and is re-presented, because suppressing it would strand the user
with a candidate they can never answer; an ANSWERED one is never asked again in
that process. The flag is deliberately losable and lives nowhere on disk.

**Two device rebuilds, then a restart.** (E2E-R1B, judgement.)
`kMaxDeviceRebuildAttempts` is 2 and the number is a judgement rather than a
measurement: one is too few because a device can be lost once for a reason that
has already passed, and many is worse than two because a device that will not
come back does not come back on the fifth try either, while each attempt costs a
full teardown and rebuild of every pipeline and buffer.

**A real GPU device loss has never been observed, only injected.** (E2E-R1B,
accepted and unavoidable here.) The recovery path is exercised through the debug
injection seam, which enters it at exactly the point a real `VK_ERROR_DEVICE_LOST`
would and runs every line after it. What is NOT covered is driver behaviour
after a genuine loss — whether the same physical device can be re-created, and
how a particular driver reports the failure. Provoking one would mean
destabilising the authoritative emulator's GPU, which the repository forbids.

**The crash report's throwable branch has no test of its own.** (E2E-R1B, new
debt.) The uncaught-exception handler is installed once per process and chains
to whatever handler was already there, and the previous-process-exit path is
covered by real evidence — `PREVIOUS_EXIT reason=16` appears in the committed
diagnostic sample. What is NOT covered by a case is the branch of
`renderReport` that formats a throwable's class, message and bounded stack
trace, because forcing a genuine uncaught exception would kill the
instrumentation process along with the app. The report's no-throwable branch,
its bounds and its redaction are all covered.

**SAF is proved at the bytes, not through the picker's own UI.** (E2E-R1B,
accepted.) The production result handlers are driven end to end against a real
`ContentResolver`, and the Intents that would be sent are asserted field by
field — but nobody taps through the system document picker, which is another
app's surface and differs per device. "SAF works" would be a bigger claim than
the evidence makes.

**Sculpt Radius/Strength remains visually heavy.** (UI-LAYOUT-R2, deferred P2.)
The direct-access sliders retain their existing geometry and >=48 dp targets.
A safe local reduction that improved parity with the unified right host without
starting a second Sculpt layout system was not clear, so this static stage did
not redesign them.

**IME verification accepts both real and deterministic platform input.** The
gesture suite asks Android for the real keyboard first; if unavailable it
dispatches a deterministic IME inset through the same listener. When real IME
animation is present, it waits for chrome to reach the root's target inset rather
than assuming a fixed animation duration. Runtime evidence separately shows the
real keyboard (`artifacts/uilayoutr2/10-exact-transform-ime.png`).

**Selection composition.** Selection is composed as a lerp toward a flat colour
rather than as a per-channel gain on the shaded colour
(`shaded * mix(vec3(1.0), tint, a)`), which would preserve face-to-face luminance
ratios exactly. At the resting 0.20 the flattening is about a fifth of what the
old permanent 0.55 cost, so the gain formulation is now an optional refinement
rather than a fix. Selection is still a whole-object **tint** rather than an
outline; the outline is the expensive half, needs either a second geometry pass
or a screen-space edge filter, and remains an owner decision. The readability
enhancement (outline or cavity) was **explicitly deferred**: both candidates start
the post-processing framework the shading stage was told not to build, and the
vertex-based alternative would expose triangle structure in Smooth mode.

**Documentation currency, not documentation size.** DOC-R2 removed the raw
line-count cap from `CLAUDE.md`: a document is too long when it is hard to
navigate or carries text that is no longer true, never merely because of its
physical line count. DOC-R2 reconciled the live docs against the Stage 020R2
product, retired the migration rationale whose invariants now stand on their own,
and deleted `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md` — a Stage 015A-R proposal
whose own header said nothing in it was implemented. The remaining cost is
ongoing: each stage must retire what it supersedes in place rather than appending
beside it, and never quote a document size from memory.

**A verdict colour is below WCAG AA in one palette.** Light Charcoal's error red
on its own precision surface measures 2.4:1. Both halves are owner-approved and
fixed, so the theme suite asserts 2.4:1 rather than a number the palette would
have to be redesigned to reach. Every verdict stays distinguishable from body
text, and no typed value is affected — those are held to 4.5:1 in all three
appearances. Raising it needs an owner decision about the approved value.

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
| `app/src/main/java/.../EditorWorkspaceView.java` | Workspace orchestration: native reads and commands, mode transitions, primary-surface exclusivity, inspector/bottom/toolbar/viewport composition, system insets, chrome visibility, System Back and `syncFromNative()`. It owns one trailing-host field, not the host's leaf views |
| `app/src/main/java/.../WorkspaceTrailingHostView.java` | The unified right-context composition and presentation owner: one floating surface, one internal vertical `BoundedScrollView`, Tool Rail, vertical transform/space selectors, precision/details trigger, fixed top/right/width placement, downward-only height and Display suppression. It owns no product or native state |
| `app/src/main/java/.../WorkspaceLayoutMode.java` | Window-dp breakpoints, where the precision surface appears when open, and chrome sizing, as arithmetic. It has no opinion about whether that surface is open. No Android type |
| `app/src/main/java/.../EditorUiState.java` | The closed list of UI-owned state: display unit, draft kind, rail selection, whether the precision surface was asked for (per mode, false to begin with), chrome-hidden |
| `app/src/main/java/.../GlobalToolbarView.java` | Editing context, the three mutually exclusive mode transitions, Export (a working control since Stage 023, no longer reserved), the project control, the Display control, chrome hide, and the one status line — including its lifecycle: transient versus standing, the two holds, and cancel-first. Owns no scene control — that is the Objects capsule's |
| `app/src/main/java/.../DisplaySettingsPopoverView.java` | The compact display popover: Shading (Studio / MatCap / Debug), Surface (Smooth / Faceted) and Projection (Perspective / Orthographic), with short interruptible open/close motion that honours the system animator scale. Owns no state |
| `app/src/main/java/.../ToolRailView.java` | The edge tool selector for either mode. Every entry works — there is no reserved-entry support left. Selects; decides nothing |
| `app/src/main/java/.../BrushEdgeControlsView.java`, `VerticalSliderView.java` | Direct Radius and Strength, and the custom vertical control behind them. Own no brush value |
| `app/src/main/java/.../PropertyInspectorView.java`, `PrecisionScrollView.java`, `BoundedScrollView.java` | The on-demand precision surface: open or absent, never collapsed, with a measured height cap — a scroll container that ends the visible body on a whole row rather than through one and fades its bottom edge while there is more, and a PINNED footer holding the body commit so Apply cannot scroll away. Owns no value |
| `app/src/main/java/.../EditorWorkspaceView.java` (history capsule) | Undo and Redo: two icon controls in one capsule at the trailing end of the bottom row, opposite the Objects capsule. Withdrawn in Sculpt, enabled straight from native `canUndo`/`canRedo`, and holding no history of its own |
| `app/src/main/java/.../ObjectsCapsuleView.java` | The resting scene control: the active body's name, and — in Construction only — the `+`. Holds no scene state; both its controls only report which was pressed |
| `app/src/main/java/.../AddPrimitivePaletteView.java` | The one creation surface: six primitive tiles and New Sketch with its plane chooser, shared by both `+` controls. Builds no geometry and defaults no dimension |
| `app/src/main/java/.../SketchEditorView.java` | The sketch's precision surface: the selected entity's exact values with Apply and Delete entity while editing; the profile choice, the depth, the direction and the pinned Extrude once finished. Owns field text only |
| `app/src/main/java/.../CadFeatureEditorView.java` | A CAD Body's *Shape* panel: the plane, the profile's sizes, the depth and the direction, one Apply that is one history step. Owns field text only |
| `app/src/main/java/.../CadStatusMessages.java` | The one place a `CAD_*` refusal becomes a status-line sentence |
| `app/src/main/cpp/forgeshape_workplane.{h,cpp}` | The three principal workplanes and the one right-handed `(u, v)` ↔ body-local mapping. No camera, no pixel |
| `app/src/main/cpp/forgeshape_sketch.{h,cpp}` | Sketch entities and per-sketch ids, validation, closed-profile extraction (chaining, loop rules, nesting), bounded ear clipping, and the one `CadStatus` vocabulary |
| `app/src/main/cpp/forgeshape_cad_body.{h,cpp}` | `CadBodyState` (one sketch, one linear extrusion), `generateCadMesh` (the one regeneration path), `CadBody::applyState` (atomic) and the typed rectangle / circle / extrude edits |
| `app/src/main/cpp/forgeshape_sketch_session.{h,cpp}`, `forgeshape_sketch_overlay.h` | The volatile sketch edit session: tools, the one owned pointer, snapping, placement, selection, finish, profile choice, depth, the one-transaction commit, and the world-space overlay the renderer draws |
| `app/src/main/cpp/forgeshape_cad_selftest.{h,cpp}` | The `CADR0-*` suite and its performance report |
| `app/src/main/java/.../ConstructionShapeEditorView.java` | Primitive chooser, that primitive's exact fields, unit chips, Apply Shape. Owns field text and a DRAFT kind only |
| `app/src/main/java/.../ConstructionPlacementEditorView.java` | Position/rotation fields, unit chips, Apply Transform. Owns field text only |
| `app/src/main/java/.../SculptContextView.java` | Sculpt-mesh summary, stale-source warning, and the guarded reset (*Reset Sculpt from Shape…*) |
| `app/src/main/java/.../InspectorHost.java`, `NumericPropertyRow.java`, `UnitChipsView.java`, `EditorControlStyles.java` | The four small shared pieces: what a body may ask of the workspace, one labelled exact field (which keeps the COMPLETE value and draws a presentation of it — see UI-LAYOUT-R1), the mm/cm/m selector, and the one place controls get their look |
| `app/src/main/java/.../LengthUnit.java` | Exact `BigDecimal` mm/cm/m ↔ meter conversion, parsing and formatting |
| `app/src/main/java/.../StartChooserView.java` | The New Project question: two ways to begin, over the live viewport. Owns no state, makes no native call |
| `app/src/main/res/values/*` | `ids.xml` (the stable semantic id contract), `dimens.xml` (radius/type/depth scales), `colors.xml` (role names, dark values), `strings.xml`, `themes.xml` (edge-to-edge) |
| `app/src/main/java/.../AppTheme.java` | The three approved appearances: the Android style each applies, and the viewport ground each hands to native code |
| `app/src/main/java/.../ChromeMotion.java` | The rules every chrome transition follows: the fade durations, the anchored-growth durations, the one ease-out curve, the uniform start scale, the reduced-motion question, cancel-first, and one alpha helper. Not a framework and must not become one |
| `app/src/main/java/.../AnchoredSurfaceView.java` | The one implementation of "a surface grows out of the control that opened it", shared by all four: pivot, staging an open that has no size yet, cancel-first, reduced motion, and the open/closed state the invoking control reads |
| `app/src/main/res/values/attrs.xml`, `themes.xml` | The semantic roles, and the one place each is given a value per theme. Adding a theme touches these two files and nothing else |
| `app/src/main/res/drawable/*` | 21 icon vector drawables on one 24 dp grid, plus the `bg_*` background state lists every control's look comes from, all written in `?attr/fs*` — including the `bg_capsule_*` set, whose only difference from the ordinary controls is a corner concentric with the capsule they sit in |
| `app/src/main/res/color/*` | `control_content_tint.xml` — the one state list an icon and its label both read, so they cannot disagree |
| `app/src/test/java/...` | JVM suites: layout arithmetic, UI-owned state, unit conversion |
| `app/src/androidTest/java/...` | Instrumented Editor Workspace suites plus `WorkspaceTestSupport` (native snapshots, drag consumption, exact chrome-union viewport measurement) |
| `app/src/main/java/.../NativeViewport.java` | JNI declarations, library load, `APPLY_*` / `SCULPT_*` status codes, `MODE_*`, `TOOL_*`, `POINTER_SAMPLE_*` slots and the DEBUG-only `debugLastPointerEvent` observation seam |
| `app/src/main/cpp/forgeshape_jni.cpp` | JNI boundary, render thread, `ANativeWindow`, MotionEvent→`TouchAction`, pointer sanitization and unpacking, camera + selection locking, stroke arbitration, `publishActiveRepresentation` |
| `app/src/main/cpp/forgeshape_input.{h,cpp}` | Platform-neutral pointer event data: `TouchAction`, `PointerToolType`, `TouchPointer` (id, position, tool type, pressure, tilt), and THE range/wrap/non-finite sanitizers those fields are defined by |
| `app/src/main/cpp/forgeshape_camera.{h,cpp}` | Camera pose, the `ProjectionMode` enum and the orthographic world span, both projections, the framing-preserving switch, gesture state machine |
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all six primitives + transform), the six `Construction*` generators, shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation including `validateCapsuleMeters`, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
| `app/src/main/cpp/forgeshape_history.{h,cpp}` | `ConstructionHistory`: what a Construction edit IS (begin/commit/cancel over a `ConstructionScene` handed in by reference), the bounded before/after Construction-domain snapshot a step holds, the capacity constant, the restore that does the least geometry work it can, and the parked bodies a redo needs. Owns no mesh byte and no sculpt state |
| `app/src/main/cpp/forgeshape_transform.{h,cpp}` | `ConstructionTransform`: authoritative double-meter position and double-degree rotation, THE axis/Euler convention, validation, atomic apply, derived model and inverse-model matrices |
| `app/src/main/cpp/forgeshape_sculpt.{h,cpp}` | `ProductMode`, `SculptTool`, `SculptSession` (mode + tool + brush + live stroke + the `hitsSculptMesh` probe), `SculptMesh`, `SculptTopology`, `computeVertexNormals`, `SculptStroke` (the one kernel plus one `apply*` per tool), sculpt publication |
| `app/src/main/cpp/forgeshape_picking.{h,cpp}` | Screen→world ray for **both** projections (perspective: one origin, fanning directions; orthographic: one direction, per-pixel origin), `transformRayToLocal`, ray/triangle, nearest hit, winding check |
| `app/src/main/cpp/forgeshape_selection.{h,cpp}` | `ObjectId`, `SelectionController`, tap-vs-navigation, `pickScene` |
| `app/src/main/cpp/forgeshape_project_bytes.{h,cpp}` | Explicit little-endian readers/writers and CRC-32/ISO-HDLC. Knows integers, IEEE-754 scalars, bounds and a checksum, and nothing about what they mean |
| `app/src/main/cpp/forgeshape_project_document.{h,cpp}` | The `.forge` v1 document and codec: the DTOs, the encoder, the bounded decoder, the version dispatch seam, and every validation and compatibility rule. `DATA_PACKAGE_SPEC.md` owns the same layout as documentation |
| `app/src/main/cpp/forgeshape_project_state.{h,cpp}` | The bridge: running project -> document, and a validated document -> running project in one all-or-nothing commit that stages every body before touching anything live |
| `app/src/main/java/.../ProjectSlot.java` | The Android storage adapter: one app-private `.forge` slot, written temp-file + fsync + rename so a crash never leaves a partial project, and read with a bound. Owns no byte of meaning |
| `app/src/main/java/.../ProjectActionsPopoverView.java` | The five project actions in two groups — Save Project and Open Saved Project for the app's own slot, then Save Copy…, Open File… and Share Diagnostics… through the system document UI. Owns no state; Open is drawn inert and says so when there is nothing saved |
| `app/src/main/java/.../ProjectCheckpoint.java` | The RECOVERY file: a separate app-private slot autosave writes atomically, and the quarantine an undecodable one is moved to. Never the manual slot |
| `app/src/main/java/.../AutosaveController.java` | WHEN a checkpoint happens: the debounce, the coalescing, the worker thread and the `awaitIdle` barrier tests wait on instead of sleeping. Owns no bytes and no format |
| `app/src/main/java/.../ProjectTransfer.java` | The Scoped Storage boundary: `Uri` to bytes and back, plus the four Intents — three for `.forge` and diagnostics, one for the `.glb` export. Nothing below it ever sees a `Uri`, a resolver or a path |
| `app/src/main/cpp/forgeshape_gltf_export.{h,cpp}` | The glTF 2.0 / GLB export: the capture that picks each body's representation, the `Model = T · L` split and the bake of `L` into vertices and normals, the determinant guard, the container framing, the JSON, the accessor layout and the refusal of anything non-finite or out of range. Platform-neutral, no third-party interchange library, and it writes nothing back into the domain |
| `app/src/main/cpp/forgeshape_gltf_import.{h,cpp}` | GLB-IMPORT-R0/R1: reading a `.glb`, for the durable import and the diagnostic preview alike. Shares no code with the writer and re-derives every offset, length, stride and bound from the file; supports a bounded STATIC subset — a node matrix or TRS which it BAKES, several TRIANGLES primitives as draw batches, a generated NORMAL, validated-and-ignored colour/UV, `doubleSided` — and fails closed BY NAME on everything else. It produces geometry and decides nothing about the project |
| `app/src/main/cpp/forgeshape_imported_mesh.{h,cpp}` | IMPORT-01A: what an Imported Mesh may be. ONE validator and ONE name rule, called by the importer and the `.forge` codec alike, so a file cannot carry geometry or a name the importer would have refused. It resolves per-submesh `doubleSided` into draw geometry and holds no material, no revision and no source path |
| `app/src/main/cpp/forgeshape_import_commit.{h,cpp}` | IMPORT-01A: turning a parsed file into durable project objects — how many objects, what they are called, where the node transform goes. Atomic, and ONE `ScopedConstructionEdit`, so an import is one Undo and a refusal costs no `ObjectId` |
| `app/src/main/cpp/forgeshape_body_delete.{h,cpp}` | UI-OWNER-45: removing one body from the project. Representation-neutral by construction — it never asks what a body IS — one transaction, the removed body handed to the history rather than destroyed, the deterministic replacement selection, and the named last-body refusal |
| `app/src/main/cpp/forgeshape_glb_import_fixture.{h,cpp}` | GLB-IMPORT-R1: the deterministic Nomad-like external-GLB compatibility fixture. Every coordinate an integer over a power of two, so its bytes are identical on every platform. A debug test seam; no product path calls it, and it is not any owner asset |
| `app/src/main/cpp/forgeshape_json.{h,cpp}` | A bounded read-only JSON parser that knows nothing about glTF. Its own number grammar, because `strtod` accepts `nan` and `inf` and a non-finite value reaching geometry is what the reader exists to prevent |
| `app/src/main/cpp/forgeshape_import_preview.{h,cpp}` | What an imported preview IS and every boundary it may not cross: session-only, no scene `ObjectId`, no Construction Source, no sculpt representation, no `MeshStore`, no history, no `.forge`, no checkpoint, not selectable, not re-exportable, gone with the process. Since IMPORT-01A it has no user-facing control and is reached only by the verification suites |
| `app/src/main/java/.../BodyLabels.java` | What a body is CALLED wherever the user reads it: an Imported Mesh's stored name, or `Body #id` for a Construction Body. One answer for four surfaces |
| `app/src/main/cpp/forgeshape_glb_roundtrip.{h,cpp}` | Whether the file agrees with the scene. Compares the independently parsed file against DOMAIN truth re-derived from the generator and `modelMatrix()`, never against the exporter's captured arrays. Reads only, and reports a number rather than a boolean |
| `app/src/androidTest/java/.../GlbDocument.java` | TEST ONLY: a second, independent glTF 2.0 reader written from the specification, sharing no code with the exporter. It is what makes the export evidence evidence |
| `app/src/main/java/.../RecoveryPromptView.java` | The one question asked when unsaved work is found: Recover or Discard, over the live viewport, answered once. Owns no state and makes no native call |
| `app/src/main/java/.../DiagnosticLog.java`, `Diagnostics.java` | The bounded local ring and its redaction (free of Android types, JVM-tested), and the Android half that names the build, chains the uncaught handler and renders a report |
| `app/src/main/cpp/forgeshape_render_recovery.{h,cpp}` | What a lost GPU device MEANS: the classification, the bounded rebuild policy and the terminal state. Free of Vulkan on purpose, so it is self-testable without a GPU |
| `testdata/forge/v1/*` | The permanent v1 golden corpus: two canonical fixtures and five deliberately broken ones. Digests are recorded in `DATA_PACKAGE_SPEC.md` |
| `scripts/build-forge-corpus.ps1` | A SECOND, independent implementation of the v1 encoder, written from the spec. Writes and verifies the corpus, needs no device |
| `scripts/run-project-persistence-e2e.ps1` | The process-death driver for E2ER1A-02/03: save, `am force-stop`, confirm by PID that no process remains, then open and verify in a fresh one |
| `app/src/main/cpp/forgeshape_object_id.h` | `ObjectId` type and reserved values, shared by the mesh and selection layers |
| `app/src/main/cpp/forgeshape_mesh.{h,cpp}` | `RuntimeMesh` (immutable revision), `MeshStore`, validation, capacity policy, upload diagnostics including source-vs-render counts |
| `app/src/main/cpp/forgeshape_render_mesh.{h,cpp}` | Derived render geometry: `RenderVertex` (position + normal + colour), `SurfaceShading`, THE crease policy (`kCreaseAngleDegrees`), per-vertex crease grouping with render-only duplication, and `RenderMeshCache`'s rebuild gate. Presentation only |
| `app/src/main/cpp/forgeshape_matcap.{h,cpp}` | The one ForgeShape-owned MatCap, computed at device init from the closed-form model in that file. No asset, no decoder, one preset |
| `app/src/main/cpp/forgeshape_display.{h,cpp}` | `ShadingModel`, `ViewportBackground`, grid visibility, the reduced-motion bool, the process-scoped `DisplaySettingsStore`, and the UI index mapping. Presentation state, never truth |
| `app/src/main/cpp/forgeshape_gizmo.{h,cpp}` | `GizmoSession`: which handle a pointer landed on, the captured pointer, the frozen World or Local drag basis, the Move/Rotate/Scale solvers with their degeneracy fallbacks, and the transaction around ONE drag (over a `ConstructionScene` and `ConstructionHistory` handed in by reference). Also the closed mode/space/handle enums, the canonical reference-unit geometry for all three modes, the screen-constant scale, the axis/highlight/neutral palette, and the placement and quantization seam. Owns no transform of its own |
| `app/src/main/cpp/forgeshape_grid.{h,cpp}` | The world reference grid's CONTRACT: the XZ plane at y = 0, the 1 m / 5 m / 20 m spacing and extent, `GridLineTier`, one pure vertex generator and the per-appearance palette. No ObjectId, no revision, not in the scene, not pickable |
| `app/src/main/cpp/forgeshape_selection_pulse.{h,cpp}` | How a SELECTED body is drawn, never which one is: the peak, the resting alpha, the decay, and one pure function over an explicit frame delta. Holds no ObjectId and reads no clock |
| `app/src/main/cpp/forgeshape_renderer.{h,cpp}` | Vulkan renderer, frame loop, camera snapshot + model transform + selection highlight consumer |
| `app/src/main/cpp/forgeshape_math.h` | Minimal self-owned vec3/mat4. No GLM |
| `app/src/main/cpp/forgeshape_demo_mesh.{h,cpp}` | Baseline cube numbers; source data for the baseline debug fixture only |
| `app/src/main/cpp/forgeshape_mesh_fixtures.{h,cpp}` | DEBUG test fixtures (baseline / same-topology / larger / stress step) |
| `app/src/main/cpp/forgeshape_*_selftest.{h,cpp}` | The thirteen debug-only deterministic suites: camera, picking, mesh, construction (box), transform, primitive, sphere, cone/capsule, sculpt brush kernel, render shading, scene, Construction history, gizmo |
| `app/src/main/cpp/shaders/surface.{vert,frag}` | GLSL source for the surface pipeline: view-space normals, Studio Solid, the MatCap lookup and the debug colour path. AOT compiled to SPIR-V by `glslc` in CMake |
| `app/src/main/cpp/shaders/grid.{vert,frag}` | GLSL source for the grid pipeline: world→clip with no model matrix, the tier→colour choice, the depth nudge that settles the coplanar Plane, and the PER-FRAGMENT radial fade |
| `app/src/main/cpp/CMakeLists.txt` | Native build + glslc shader step |
| `artifacts/` | Runtime evidence screenshots from accepted stages, plus `stage015c_shading_comparison.md`, the Stage 015C comparison sheet |
| `docs/ui/wireframes/*.svg` | Stage 015A-R proposal sketches, kept as reference only. They are **not** the shipped shell and WF-3 draws a Sketch/Extrude flow that does not exist |
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

**Exactly one next step: return the CAD-A3-C1 report to the ForgeShape
coordinator for the OWNER's real-device Home + CAD workflow retest.** No
product stage may begin here: booleans, fillets, chamfers, a constraint solver,
custom construction planes, curved-face and imported/sculpted-surface sketches,
an independently movable dependent and the face-first contextual shortcut are
**not started**; CAD → Sculpt is **not started** (refused by name and recorded
as the three decisions it needs); `BRIDGE-R1` remains **future**; Stage 018A
still owns rename, visibility, lock, duplicate and grouping — Delete is the
ONLY object command that exists — and Stage 033's full exporter, OBJ and FBX
are **not started**. None may be begun without the coordinator opening it.
GATE-E2E remains the owner's and is not opened here.

CAD-A3 + APP-H1 is closed on the technical side. `Home → New Project → CAD →
tap a plane in the viewport → sketch → Extrude = the first body and the
project → New Sketch → tap a face → Extrude = a face-supported dependent → Save
→ Home → Open File` works through the real chrome and real MotionEvents, with
New Sculpt, the unsaved-changes guard and a one-step Back beside it; the CADB
v2 corpus is independently encoded and its lineage token is a stated format
field. What the owner's retest is for is what no emulator settles: whether
choosing a plane, a face and a depth with a real finger or stylus — and a real
stylus hover, which only hardware can show — reads as the product it should
be.

CAD-R0-A1A2 is closed on the technical side. `New Sketch → plane → Rectangle /
Circle / Line / Polyline → Finish Sketch → depth → Extrude` works through the
real chrome and real MotionEvents; a CAD Body is a third representation whose
truth is its sketch and its extrusion; its sizes and depth are editable later
as one Undo each with the placement untouched; the `CADB` branch survives Save,
Open, Save Copy, autosave, recovery and export; a cancelled sketch changes
nothing; the profile engine refuses by name; and the twelve older corpus
fixtures are byte-for-byte unchanged beside four new ones pinned by two
encoders. What the owner's retest is for is what no emulator settles: whether
drawing a profile with a real finger or stylus on real hardware, snapping to a
corner, typing a depth and taking the whole thing back reads as the product it
should be. The two standing `OWNER_REAL_FILE_*_RETEST_PENDING` items are
unchanged by this stage.

SCULPT-UNDO-R0 is closed on the technical side. `Start/Resume Sculpt → stroke A →
stroke B → Undo B → Undo A → Redo A → Redo B` works through the real controls for
both a Construction-derived and an Imported-Mesh-derived sculpt; one completed
stroke is one entry however many events it took; the project history is untouched
and still refuses its own entry points in Sculpt; source meshes, placements and
`ObjectId`s are immutable throughout; both memory caps are enforced with
deterministic oldest-first eviction; and nothing reaches a `.forge` byte, so all
seven corpus digests are unchanged.

What the owner's retest is for is the part no emulator can settle: whether taking
a stroke back on real hardware, with a real finger and a real file, feels like
the product it should be. The named gaps it may also close are the two standing
`OWNER_REAL_FILE_*_RETEST_PENDING` items, which this stage did not change.

IMPORT-01B is closed on the technical side. An imported body enters the ordinary
reversible Sculpt workflow, a real stroke moves sculpt truth while the imported
arrays stay bit-identical, Back to Imported Mesh and Resume Sculpt both work,
`IMPT`+`SCUL` survives Save, autosave, recovery and reload, and export follows
the same effective-state rule every sculpted body already followed. The Objects
list deletes a real `SceneObject` — of either representation, with or without
retained sculpt work — as exactly one Undo, with a deterministic replacement
selection and a named refusal for the last body.

The two things it cannot claim are the owner's own binaries. `E2E-IMP01B-12` is
`OWNER_REAL_FILE_01B_RETEST_PENDING`: the owner's `1 lowpoly.glb` is not in this
environment, no user folder was searched for it, and closing it is a tap on
**Import GLB…** followed by **Start Sculpting**. `E2E-IMP01A-12` remains
`OWNER_REAL_FILE_01A_RETEST_PENDING` on the same terms.

GLB-IMPORT-R1 is closed. The preview reads the class of static external file the
owner's character belongs to, proved against the same synthetic fixture. It is
now a diagnostic with no user-facing control, and the R0/R1 suites are what
drive it.

GLB-IMPORT-R0 is closed. Its finding is the useful one: the file and the scene
describe the same world geometry to **exactly 0.0 m**, so the discrepancy the
owner is chasing is not a ForgeShape exporter or parser defect. What it cannot
say is what the discrepancy IS instead, because no external tool was run.

E2E-R1C is closed, with `ARCH-OWNER-07` applied. Export ships as an early
vertical slice: one `.glb`, whole scene, metres, +Y up, zero conversion, each
body's rotation and scale baked into its geometry with only its translation on
the node, per-body representation honoured, and a read that changes nothing. The
native suites (17, 2528 checks), the JVM suite, `GlbExportTest`,
`GlbImportPreviewTest`, the E2E-R1A/R1B regression suites and the authoritative
exhaustive-sharded instrumented aggregate are all green, and both supported ABIs
build debug and release.

**The one thing GATE-E2E still needs is the external-importer check**, which was
deliberately not performed: no Godot, Blender or `gltf-validator` run is claimed
anywhere. `artifacts/e2er1c/GATE_E2E_GODOT_CHECK.md` holds the two **corrected**
sentinel `.glb` files, the new "rotation 0,0,0 and scale 1,1,1 on every object"
check that `ARCH-OWNER-07` adds, what to look for in each file, and an empty
verdict for the owner. The pre-bake files it first offered are recorded there as
superseded history with their own digests, so neither pair can be mistaken for
the other.

A **UI moratorium remains active**, with one owner-directed exception:
CAD-R0-A1A2's brief named the sketch surfaces it needed, and they were added
inside the accepted structure rather than beside it — New Sketch and the plane
chooser as a second group INSIDE the creation palette, the five sketch tools as
entries on the SAME Tool Rail, Cancel Sketch and Back to Sketch as a vertical
group inside the SAME trailing host (on the transform selector's terms),
*Finish Sketch* and *Extrude* as the one toolbar transition per sketch state
with no abbreviated form, and two more bodies for the SAME precision surface.
No region was added and the resting workspace is unchanged when no sketch is
open. The corrected edge-host arrangement is the
accepted baseline; E2E-R1C changed one existing control from reserved to working,
and GLB-IMPORT-R0 added one group of three rows to the existing Project surface
and moved nothing else. GLB-IMPORT-R1 changed only the WORDING of those three
rows. IMPORT-01A **removed** two of them — the preview's Show and Clear — leaving
the group as *Import a mesh* with the one action *Import GLB…*, and withdraws the
Tool Rail's *Shape* entry and the toolbar's *Start Sculpting* while an Imported
Mesh is selected. Nothing was added anywhere and the accepted R2 right host does
not move in any of them. The one carried visual item is Sculpt Radius/Strength,
which stays deferred P2.

`CAD-R0-DATA` remains a COORDINATOR-owned decision/research gate that Claude Code
does not execute.

The evidence stands where it was made: `artifacts/uilayoutr2-correction/` holds
the 66-row matrix, 36 raw screenshots, 36 mechanical overlays and the derived
analysis, and `artifacts/uilayoutr2-owner-review/` holds the six representative
pairs the coordinator reviewed. Both are historical and are not to be rewritten.

The one feature the repo still records as a candidate is **Selection Outline** —
the expensive half of selection feedback, needing either a second geometry pass
or a screen-space edge filter. It is a *candidate awaiting owner decision*, not
an approved stage: today's whole-object tint is the shipped behaviour, and the
readability enhancement was deferred because both candidates start the
post-processing framework the shading stage was told not to build.

**Still out** and unchanged: snap-to-grid for the transform handles (the sketch
grid and its snapping arrived with CAD-R0-A1A2 and are a different contract from
the world reference grid, which is still not a snap target); a View Cube, camera
focus or named views; blur or glass of any kind; a post-processing framework; an
automatic system theme; hierarchy, and every object command but Delete — rename,
visibility, lock, duplicate, grouping, reorder and multi-select; every CAD
feature beyond the R0 sketch-and-extrude — holes, booleans, fillets, chamfers,
shells, revolves, sweeps, lofts, patterns, mirrors, offsets, trims, constraints,
arcs, splines, face-based planes, rotated rectangles, editing a polygon
profile's points, and CAD → Sculpt; Mirror, Subdivide and Remesh; materials,
textures, UVs, animation and rigging in either direction; the one-way
Construction-to-Sculpt project derivation (`BRIDGE-R1`); and pressure-driven
sculpting.

Persistence is no longer on that list, in the shape E2E-R1A and E2E-R1B shipped:
one app-private manual slot, one recovery checkpoint, and transfer of that same
document through Scoped Storage. Save As, project naming, a recent list,
thumbnails, a project browser, a version history, cloud and accounts are all
still out.

Export is no longer on it either, in the shape E2E-R1C shipped: one `.glb`, one
direction, one default material. GLB import is no longer on it in the shape
IMPORT-01A shipped: the bounded STATIC subset, into non-parametric objects. UVs,
textures, chosen materials, hierarchy, merge and unit options, compression, OBJ
and FBX are all still out in both directions.
