# MODELING-FOUNDATIONS-R1 — DESIGN

Decisions taken before implementation, from `BEFORE.md`. ForgeShape's
architecture stays authoritative: every new truth is a bounded, typed value; every
mesh is derived; every edit is one Construction-history transaction; every
refusal is named.

Terminology is fixed: **Parametric History**, **Freeform/SubD** (product word:
*Freeform*), **Surface**. Nothing in code, UI or documentation calls the Freeform
work a T-Spline: it is Catmull-Clark subdivision over a quad control cage, with
no T-junctions and no local refinement.

---

## A. Parametric History R1

### A1. Timeline architecture — derived, never stored

`forgeshape_cad_timeline.{h,cpp}` adds one pure function:

```
CadTimeline buildCadTimeline(const CadBodyState& state,
                             const CadRegenerationReport& report,
                             uint32_t editingFeatureId);
```

It reads the existing durable chain (`cadFeatureAt`) and the existing first-failure
report. Nothing new is serialized: the feature chain IS the history, so a second
stored timeline would be a second truth that could disagree with it.

Row order (construction order, deterministic):

1. For each feature in chain order: the SKETCH it extrudes, the first time that
   sketch is consumed, then the FEATURE.
2. Retained sketches no feature consumes, ascending by id, at the end (state
   `Unused`).

A row carries: kind (`Sketch` / `Feature`), its stable semantic id
(`CadSketchId` for a sketch, `featureId` for a feature — never an index), the
feature kind (Extrude / Revolve) and operation (New Body / Add / Cut), the
numeric summary (entities, dimensions, extent mode and both distances, side,
angle, revolve sense, axis edge ref, region / hole counts, support feature), and
a state:

| State | Meaning |
| --- | --- |
| `Ok` | regenerated |
| `Editing` | the feature (or its sketch) the staged edit changes |
| `Failed` | the FIRST feature regeneration refused, with its `CadStatus` |
| `NotRegenerated` | a feature after the failing one (never evaluated) |
| `Unused` | a retained sketch no feature consumes |

A concise summary string (`cadTimelineRowSummary`) exists for logs and tests;
Android formats its own text from the numeric slots so units follow
`LengthUnit`.

### A2. Editing an earlier row

No second editor is built. A row opens the editor that already exists:

* **Sketch row** → `sketchBeginEditFeature(body, firstConsumer, startReady=false)`
  (the sketch, staged, in its aligned view).
* **Extrude row** → `sketchBeginEditFeature(body, featureId, startReady=true)`
  (depth, side, extent and — on a face sketch — operation, on the canvas HUD and
  the precision surface).
* **Revolve row** → the same, opening the revolve ring: angle (exact degrees),
  direction (Flip) and axis (Change Axis → tap a straight edge). Numeric truth
  stays in native; Java holds none.

### A3. Downstream regeneration and the staged failure

The staged candidate is regenerated through `regenerateCadBody` (unchanged):
base, then features in order, stopping at the FIRST failing feature, whose id and
`CadStatus` the report already carries. The project is untouched until
`commitEdit` (one `ScopedConstructionEdit`), which refuses an invalid candidate.

New: `cadStagedTimeline` JNI reads `buildCadTimeline(candidateState(),
evaluation)` while an edit session is open over that body, so the History surface
shows the STAGED chain: the edited row, the failing row (`Failed`, with its
reason) and every row after it (`NotRegenerated`).

When a staged edit breaks a LATER feature (`failedFeatureId > editingFeatureId`),
a compact **regeneration issue card** appears over the editor: the failing
feature named by kind, ordinal and reason, and two actions:

* **Fix** — collapses the card; the staged edit stays open so the value can be
  corrected (the card returns while the candidate is still invalid).
* **Cancel** — `sketchCancel`: the staged copy is dropped; the body is exactly
  what it was (byte- and fingerprint-equal, PAR-11).

Never: delete a later feature, retarget a reference, skip a failing feature, or
convert anything to a mesh.

### A4. History UX

A **History** control (`feature_history`) joins the history capsule beside
Undo/Redo, drawn only when the active body is a CAD or Surface body and no sketch,
Sculpt or Freeform edit is open. It opens `FeatureHistoryView`, an anchored,
scrollable surface (rows 48 dp, five visible, scrolls beyond) — the Sculpt
History navigator's shape, not a desktop timeline bar. A tap on a row edits it;
the current failing row is marked by SHAPE (a ✕ glyph), by text and by colour as
a second carrier. No reorder, no suppression (both would need new durable
semantics). Tablet: the same surface.

### A5. Fillet / Draft

Not implemented. Honest Fillet or Draft needs B-Rep edge/face topology the
derived triangle solid does not carry (the kernel returns triangles with face
TAGS, not edges with curvature). Deferred as kernel work; no triangle-id or
post-tessellation substitute.

### A6. Tests

`PAR-01..15` run inside the CAD-feature suite (no new token), in
`forgeshape_cad_timeline_selftest.cpp`. JVM: `FeatureHistoryPresentationTest`.
Device: `CadParametricHistoryOwnerTest`.

---

## B. Freeform/SubD R1

### B1. Representation

`BodyRepresentation::Freeform = 4`. Its truth is the CONTROL CAGE
(`forgeshape_freeform.{h,cpp}`); the smooth mesh is derived on every publish and
never stored, never compared, never serialized. Not an Imported Mesh, not a CAD
body, not a Frozen Sculpt Mesh.

### B2. Durable state

```
enum class FreeformVertexId : uint32_t {};   // strong ids: no implicit int
enum class FreeformEdgeId   : uint32_t {};
enum class FreeformFaceId   : uint32_t {};

struct FreeformCage {
    vector<FreeformVertex> vertices;  // {id, DVec3 position}, ascending id
    vector<FreeformEdge>   edges;     // {id, v0 < v1, crease in [0, 1]}, ascending id
    vector<FreeformFace>   faces;     // {id, 4 vertex ids, canonical rotation}, ascending id
    uint32_t nextVertexId, nextEdgeId, nextFaceId;   // high-water marks
    uint8_t  subdivisionLevel;        // 0..4
    uint8_t  symmetry;                // bit0 X (plane x=0), bit1 Y, bit2 Z
};
```

Identity is the id, never a vector index; a face's loop names vertex ids and the
edge between two loop neighbours is found by its id record. A body holds its cage
as `shared_ptr<const FreeformCage>` and replaces it on every edit, so a history
step SHARES unchanged cages instead of copying them (bounded memory with 64
steps).

Caps: 4096 vertices, 8192 edges, 4096 faces; derived quads at the stored level
≤ 131072 (a 512-face cage at level 4).

### B3. Topology rules (`validateFreeformCage`)

Quads only; every face's four vertices distinct; canonical rotation (smallest id
first) and outward winding; every face side has exactly one edge record and every
edge record bounds 1 (boundary) or 2 faces traversing it in OPPOSITE directions
(manifold, orientable); no duplicate edge or face; no isolated vertex; the faces
around a vertex form ONE fan (no bow-tie); finite coordinates within ±1e5 m; ids
non-zero, ascending, below the marks; level 0..4; symmetry exact (B7). Every
refusal is a `FreeformStatus` by name.

### B4. Creation

Deterministic, exactly symmetric about all three principal planes:

* **Box** — 8 vertices, 12 edges, 6 quads, side 1 m.
* **Plane** — 4×4 quads (25 vertices) in XZ, 2 m square, open boundary.
* **Cylinder** — 8 quads around, one height segment, radius 0.5 m, height 1 m;
  END TREATMENT: each end is closed by 4 quads meeting at a centre vertex
  (spokes to every second ring vertex), so the cage is closed, all-quad, with
  valence-3 ring vertices as its only extraordinary points. 18 vertices, 32
  edges, 16 faces (V − E + F = 2).

Ring coordinates are built from first-quadrant values and mirrored by sign so the
cage is bit-exactly symmetric.

### B5. Catmull-Clark (`forgeshape_freeform_subdivision.{h,cpp}`)

binary64 throughout; float only when publishing. Per level: face point = face
average; edge point = smooth `(v0+v1+f0+f1)/4` blended toward the midpoint by the
edge's crease weight `c` (a boundary edge is always the midpoint); vertex point =
the interior smooth rule `(Q + 2R + (n−3)V)/n`, blended toward the crease rule
`(6V + a + b)/8` (exactly two creased edges, weight = their mean) or the corner
rule `V` (three or more, weight = their mean); boundary vertices use
`(6V + a + b)/8`, and a boundary vertex on one face is a fixed corner. Child edges
inherit the parent crease weight at every level (a continuous weight held, not a
decaying semi-sharpness). Iteration order is the ascending-id dense order, so the
result is a pure function of the cage. Each derived triangle carries the CONTROL
face id it came from (picking attribution). `freeformMeshDigest` hashes positions
and indices (FNV-1a 64) for determinism checks.

### B6. Tools (pure functions `cage → cage | status`)

* **Transform** — move / rotate / scale an affine about a pivot applied to the
  selected vertices (an edge or face selection resolves to its vertices).
* **Push/Pull** — selected faces move along their averaged normal by an exact
  distance.
* **Extrude Face** — the selected face region gets new side quads along its
  boundary; boundary vertices are duplicated at `old + d·n`; interior vertices
  move; the region's faces KEEP their ids, every new vertex/edge/face takes a new
  id in a canonical order; a pinched region is refused.
* **Insert Edge Loop** — from a selected edge, walk the quad ring through
  opposite edges in both directions to a closure or the boundary; split every
  ring edge at the typed ratio (0..1, default 0.5) carried consistently along the
  ring; a ring that re-enters a face is refused (`EdgeLoopSelfCrossing`). Global
  loop insertion: no T-junction is ever created.
* **Crease** — continuous per-edge weight in [0, 1] (Sharp = 1, Smooth = 0).
* **Delete Face** — removes the faces, then edges and vertices left unused; the
  result must still validate (open boundary allowed, bow-tie / empty refused).
* **Subdivision level** — 0..4.

### B7. Symmetry

Durable flags X/Y/Z (planes through the body origin). Rule: with symmetry on, the
cage is EXACTLY symmetric — every vertex has a partner whose position is the
bit-exact reflection, a vertex on a plane has that coordinate exactly 0, and the
reflected face set is the face set. Enabling symmetry on a cage that is not
symmetric is refused (`CageNotSymmetric`). Every tool runs on the selection
united with its mirror images, then the orbit of every vertex under the symmetry
group is re-derived and each member is SET from its orbit's smallest-id
representative by exact reflection (on-plane coordinates zeroed) — so no duplicate
centre vertex is created and no ULP drift accumulates. A loop insertion on a ring
that maps to itself requires ratio 0.5 (`SymmetryRequiresMidpoint`).

### B8. Session, picking, gizmo, overlay

`FreeformEditSession` (volatile): edit mode on/off, element mode
(Vertex/Edge/Face), selection set of ids, multi-select, transform mode. Picking is
SEMANTIC on the cage: vertices and edges by screen distance to their projected
control positions (24 dp), faces by the pick ray against the cage quads; never a
derived triangle index. The transform gizmo is drawn from a `GizmoSnapshot` at the
selection pivot (zero renderer change); its pure solvers drive the drag, one
captured pointer, basis frozen at Down, a second pointer cancels, one drag = one
Construction step. The cage is an overlay producer (edges in `Entities`, vertices
as small crosses, selection in the highlight colour), a fourth branch beside
support chooser / dimensions / sketch.

### B9. History

`BodyConstructionState` gains the shared cage pointer; capture copies the
pointer, compare is pointer-or-deep equality, restore swaps it back. One gesture
or one Apply = one `ScopedConstructionEdit` = one Undo. `SculptHistory` is not
involved.

### B10. Sculpt

Not this stage. `buildSculptSourceMesh` refuses a Freeform body; Start Sculpting
is absent for one and the native freeze refuses by name
(`FreeformNotSculptable`). The cage stays the only truth.

---

## C. Surface R1

### C1. Representation and state

`BodyRepresentation::Surface = 5`. Truth is an ORDERED surface feature list over
retained sketches (`forgeshape_surface.{h,cpp}`):

```
SurfaceBodyState {
    vector<SurfaceSketchRecord> sketches;  // {id, Workplane, offset (m), CadSketch}
    vector<SurfaceFeature>      features;  // {SurfaceFeatureId, kind, typed payload}
    uint32_t nextSketchId, nextFeatureId;
}
```

Strong ids: `SurfaceFeatureId`, `SurfacePatchId` (feature id + local patch
ordinal, derived deterministically), `SurfaceEdgeId` (patch + boundary edge
ordinal). Tessellation, normals, adjacency and render triangles are derived;
patch and edge ids are DERIVED identities a tap resolves to, never stored
geometry.

### C2. Feature kinds

| Kind | Durable inputs | Result |
| --- | --- | --- |
| `PlanarPatch` | sketch id, region selection (`ProfileRegionRef`s) | one planar patch per merged component, holes preserved, no thickness |
| `ExtrudedSurface` | sketch id, curve entity ids, distance, side | one ruled patch per curve (open chains valid), no caps |
| `RevolvedSurface` | sketch id, curve entity ids, axis `CadSketchEdgeRef`, angle (deg), sense | one swept patch per curve, open profile valid, no caps; on-axis points collapse to an apex |
| `LoftSurface` | two sections (sketch id + curve ids), `reverseB`, `startOffsetB` | one ruled patch; open↔open or closed↔closed; mismatch refused |
| `TrimSurface` | target feature id, trim sketch id (coplanar), loop region, keep inside/outside | the target planar patch clipped through the planar arrangement; trim edges become semantic boundary edges |
| `Stitch` | patch ids (or all), tolerance 1e-6 m | adjacency between coincident boundary edges |
| `Thicken` | source feature id, signed thickness | a closed solid validated by the production kernel (`cadKernelValidateSolid`) |

Section/curve chains are walked deterministically from the smallest entity id;
a fork is refused. Loft samples both sections by arc length to one count
(≤ 256), correspondence fixed by the durable `reverseB` / `startOffsetB`.

Trim R1: planar targets only; the trim sketch must lie on the target's plane and
offset; computed exactly through the existing planar arrangement over the union
of both sketches. A non-planar target is refused (`TrimUnsupportedTarget`) — never
emulated by deleting triangles.

Stitch: an edge pair stitches when both endpoints and every sample coincide
within 1e-6 m; pairs inside 1 mm but outside tolerance refuse `StitchGapTooLarge`;
an edge matching two others refuses `StitchNonManifold`; endpoints that meet with
diverging interiors refuse `StitchIncompatibleBoundary`; none at all refuses
`StitchNoCompatibleEdges`. Nothing is averaged.

Thicken R1: a planar patch (and a trimmed one) → the prism of its region; an
extruded ruled surface whose curves are straight (Line, Polyline, Rectangle) →
the extrusion of the mitred thin-wall profile, a Circle → the annulus, a single
Arc → the annular sector. Revolved, lofted, stitched and spline-based shells
refuse `ThickenUnsupportedForSurfaceType`. Every result must pass
`cadKernelValidateSolid`. A thickened feature's patches are replaced by its solid.

### C3. Regeneration and editing

`regenerateSurfaceBody(state, &mesh, &report)` evaluates features in order and
stops at the FIRST failing one (`report.failedFeatureId`, `SurfaceStatus`).
`SurfaceEditSession` stages a copy of the body state; a parameter edit
regenerates the whole staged chain (latest-only evaluation); Apply is ONE
`ScopedConstructionEdit`, Cancel writes nothing. The History surface lists the
surface features (rows by kind) and opens this editor.

### C4. Render and pick

Open patches publish `renderBothSides = true`; a fully thickened body publishes
single-sided. Each derived triangle carries its patch id; a tap resolves
triangle → patch through that table. In Surface edit context the open boundary
edges are drawn subtly and the selected patch's boundary in the highlight colour
(overlay producer).

### C5. Creation flow

`Surface` (New Project, and Add Body) opens a sketch on XY in SURFACE purpose:
Finish Sketch offers **Patch / Extrude / Revolve** instead of the extrusion; the
first commit creates the body (a project's first commit replaces the scene
through the same all-or-nothing path `commitFirstCadProject` takes). With a
Surface body active the Surface surface offers Create (Patch, Extrude, Revolve,
Loft — each opening a sketch; Loft's second section on a parallel plane at a
typed offset) and Modify (Trim — a sketch on the target's plane; Stitch;
Thicken).

### C6. Sculpt and CAD

Not this stage: a Surface body is not sculptable (`SurfaceNotSculptable`), is no
sketch support, and takes part in no CAD boolean.

---

## D. Format

* **No top-level version change.** `FORGESH1` major 1 / minor 0 stays. Two new
  REQUIRED sections, each version 1, each announced by a new header flag:
  * `FRFM` — bit4 `0x10` `HasFreeform`
  * `SURF` — bit5 `0x20` `HasSurface`
* An older build refuses such a file (unknown header bit → `BadHeader`; unknown
  required section → `UnknownRequiredSection`) rather than opening it with bodies
  missing.
* A body is named by exactly one of `CONS` / `IMPT` / `CADB` / `FRFM` / `SURF`.
* The conditional writer emits `FRFM` / `SURF` only when such a body exists, so
  every project of the older representations encodes byte-identically; `CADB`
  and its v1..v8 layouts are untouched.
* Bounds are validated before any allocation, on the existing count pattern.
* Fixtures: six new — `freeform_box_v1`, `freeform_crease_symmetry_v1`,
  `freeform_bad_topology_v1` (refused), `surface_patch_extrude_v1`,
  `surface_loft_trim_stitch_v1`, `surface_bad_ref_v1` (refused) — built by the
  independent PowerShell encoder and pinned by digest; the 66 existing fixtures
  stay byte- and verdict-identical (72 in all).
* Layout documented in `DATA_PACKAGE_SPEC.md` §7j (`FRFM`) and §7k (`SURF`).

Fingerprint: per representation (cage values / surface state values), mixed only
for the new representations, so every older project keeps its fingerprint.

## E. Every representation switch

History (capture, compare, fabricate, restore), Duplicate (copies the truth),
Mirror (refused by name), Delete/Rename/Hide/Lock (neutral, unchanged), publish,
sculpt seed (refused), export (derived mesh; open surfaces double-sided), codec
bridge, fingerprint and the JNI representation code all learn `Freeform` and
`Surface`. A representation no branch knows fails closed — never rebuilt as a
Construction body.

## F. Test strategy

| Phase | Host | JVM | Device |
| --- | --- | --- | --- |
| A | `PAR-01..15` in the CAD-feature suite | `FeatureHistoryPresentationTest` | `CadParametricHistoryOwnerTest` |
| B | `FF-01..22` in a new `FREEFORM` suite | `FreeformPresentationTest` | `FreeformOwnerTest` |
| C | `SURF-01..24` in a new `SURFACE` suite | `SurfacePresentationTest` | `SurfaceOwnerTest` |
| Format | `FMT-*` in each suite (round trip, bounds, legacy bytes) | — | — |

Startup tokens become **twenty-five**: `FORGESHAPE_FREEFORM_SELFTEST_OK` and
`FORGESHAPE_SURFACE_SELFTEST_OK` are appended after the CAD-feature token
(`scripts/ci-device-smoke.sh`, `CLAUDE.md`, `README.md`, the host runner).
Performance lines: `FORGESHAPE_FREEFORM_PERFORMANCE`,
`FORGESHAPE_SURFACE_PERFORMANCE`; parametric timings join the CAD-feature line.

Gate order (binding, lightweight): focused host + JVM + the phase's device class
per phase; then, once, the full host aggregate, the JVM suite, the
debug/release/androidTest builds, the release guard, corpus parity, a focused
multi-domain device union, and one CI FAST whose APK is the OWNER handoff.
