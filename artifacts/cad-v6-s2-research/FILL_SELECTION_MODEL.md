# Fill selection model — selection truth separated from extrusion validity

Task `CAD-V6-S2-RESEARCH-FILL-PICK-HUD3D-R1`, read-only. Baseline `bbae765`.
All paths are under `app/src/main/cpp/` unless stated.

## 1. Proof: the current tap depends on what is already selected

### 1.1 Source

`SketchSession::togglePlanarFace` (`forgeshape_sketch_session.cpp:1635`):

- REMOVING a face (the `removed` branch) writes the reduced set **with no
  validation** (`:1670 setPlanarSelection(std::move(next))`).
- ADDING a face (`:1652 if (!removed)`) builds `next`, resolves every selected
  face index, and calls `mergePlanarFaceSelection(arrangement_, indices,
  nullptr)` (`:1665`). Any non-`Ok` status refuses the tap; the selection is
  unchanged.

`mergePlanarFaceSelection` (`forgeshape_cad_body.cpp:857`) maps
`mergePlanarFaces` (`forgeshape_sketch_arrangement.cpp`) to `CadStatus`.
`mergePlanarFaces` keeps ONE `nodeUsed` vector across EVERY output loop
(`:2200`), and returns `ArrangementStatus::PinchedSelection` (`:2210`) the
moment any union-boundary walk reaches a node any loop already passed. That
covers both a loop revisiting a node and two different loops sharing one node.
`cadStatusForArrangement` maps it to `PlanarFacesTouchAtPoint` (code 61).

So **selection validity == extrusion-union validity**, checked on every add.
The repo's own test writes the order dependence down as intended behaviour:
`forgeshape_cad_feature_selftest.cpp:2238`
`S2CORR_TAP_05_two_cells_meeting_at_a_point_are_refused_by_their_own_name`
refuses B + outside, while `:2243` `S2CORR_TAP_06` reaches the same two cells
once the bridging cell is selected first.

### 1.2 Reproduction (scratch harness outside the repo, real `SketchSession`)

Sketch: rectangle 4×3; circle A (−0.8, 0, r 0.8) and circle B (0, 0, r 0.8)
overlapping; circle C (2, 0, r 0.6) across the right side; two lines making a
cell in the upper-left corner. **Seven cells**:

| face | cell | area |
| --- | --- | --- |
| 0 | rectangle remainder, hole = A ∪ B | 7.949 |
| 1 | C inside the rectangle | 0.566 |
| 2 | C outside the rectangle | 0.566 |
| 3 | corner cell | 0.250 |
| 4 | A only | 1.224 |
| 5 | A ∩ B lens | 0.786 |
| 6 | B only | 1.224 |

Results:

- From one selected cell, adding a second is refused in **3 of 21** pairs:
  `{0}+2`, `{0}+5`, `{4}+6` → `PlanarFacesTouchAtPoint`. Each pair meets only
  at crossing points.
- Ascending or descending taps over all seven both succeed: the bridging cell
  always arrives first in either order for this sketch. Order dependence does
  not need a reversed order. It needs a bridge that is missing at the moment
  of the tap.
- **The OWNER's sequence, reproduced exactly.** Select all 7. Remove face 1
  (C inside): accepted, with no validation. The selection is now **6** cells
  and **already pinched**: 0 and 2 touch at C's two crossings. The preview
  reports `PlanarFacesTouchAtPoint`. Next, remove face 0 and tap it again:
  refused, `PlanarFacesTouchAtPoint`.

  That matches the screenshots: "Regions in the extrusion: 7" → toggle → "6" →
  tapping another visible cell → "Those areas meet only at a point…".

  Two defects compound here:
  1. adding is refused by OTHER cells;
  2. removing can create a set that adding would have refused. Every later add
     then fails, unless it happens to re-bridge.

## 2. Proof: the kernel does not need the pinch refusal

Scratch probe of the vendored kernel through `forgeshape_cad_kernel.h`. Unit
boxes A and B touch only along one vertical edge, which is a 2D point-touch
extruded:

| case | result |
| --- | --- |
| K1 `{A,B}` as ONE `CadSolid`, separate vertex sets | `Ok`, components 2, volume 2 |
| K2 `union(A, B)` | `Ok`, components 2, volume 2 |
| K3 Add: `union(T, {A,B})` (one tool) | `Ok`, components 1, volume 18 (exact) |
| K4 Add sequential: `union(union(T,A),B)` | `Ok`, components 1, volume 18 |
| K5 Cut: `difference(Big, {A,B})` (one tool) | `Ok`, components 1, volume 30 (exact) |
| K6 Cut sequential | `Ok`, components 1, volume 30; result re-validates `Ok` |
| K7 `{A,B}` SHARING the edge's vertices | `NotManifold` |

A region whose hole touches its outer loop at one point was also tested,
triangulated by `cadKernelTriangulateRegion` and extruded with
`appendPrism`'s layout (one vertex ring per loop):

- triangulation `Ok`;
- prism validates `Ok`, 1 component, volume exact (8);
- a Cut box straddling the pinch: `Ok`, volume exact (7.625).

**Conclusion.** The pinch is a problem only when vertices are SHARED across
the touch. `appendPrism` already emits one vertex ring per loop and per
component (`forgeshape_cad_feature.cpp:490`), so a union whose loops touch at a
node is a valid, closed, oriented solid. Two cases are covered:
- **separate loops**: shells or loops that touch geometrically but not
  topologically;
- **point-touching components**: one tool for Add/Cut gives the same result as
  sequential booleans.

The refusal protects nothing the kernel needs.

## 3. Selection truth

```text
FaceSelectionSet  !=  ExtrusionToolComponents
```

- **Truth.** `ExtrudeFeature::planarFaces`: a canonical, sorted, duplicate-free
  set of `PlanarFaceRef`. This is unchanged from today, including its
  persisted form.
- **Tap invariant.** A tap toggles membership of the atomic face under the
  finger. When ADDING it may refuse for three reasons only:
  - not in `Ready`;
  - the face does not resolve, which a derived face always does;
  - the set is at `kMaxPlanarFaceSelection` (16) → `TooManyRegions`.

  It never merges, never derives topology and never consults the other
  members. Removing is the same toggle in reverse.
- **Order independence.** The selection after any sequence of taps is a
  function of the multiset of tapped faces only (XOR). Pinned by the
  permutation tests in `IMPLEMENTATION_PLAN.md`.
- The infinite exterior is not a face (`deriveSketchArrangement` already
  excludes it). Nothing changes there.

## 4. Component derivation (derived, never stored)

`mergePlanarFaces` stays the ONE derivation and changes in exactly one way:
**a node shared by union loops is no longer an error.**

1. Chosen half-edges: a half-edge bounds the union when its twin is not
   chosen. This is unchanged, so a shared fragment still cancels and no wall is
   ever emitted twice.
2. The walk keeps `nextOnUnion`. From `h` it tries the own face's successor
   first and then rotates around the node to the next chosen face. That is the
   TIGHT turn: at a pinch node the walk stays on the boundary of the material
   it came from instead of crossing to the other side. So loops that meet at a
   node come out as separate simple loops that touch. The current
   `nodeUsed`/`PinchedSelection` early return (`:2200-2211`) is removed.

   Only one guard stays: an edge-repeat. A half-edge walked twice remains
   `InvalidSelection`, and is unreachable for derived faces.
3. Outer and hole classification by signed area, and hole ownership by the
   smallest outer that strictly contains a hole-edge midpoint clear of every
   outer boundary. Both unchanged. A hole that touches its own outer at a node
   still has edge midpoints clear of that outer.
4. Components: one per outer loop. Faces touching only at points therefore
   become SEPARATE components (cells 0 and 2 → two components). An outer whose
   hole touches it at a node stays ONE component with a touching hole.
5. Each component → `appendPrism`, one vertex ring per loop. Touch points are
   duplicated vertices, which keeps the solid closed and oriented: K1 and the
   pinched-ring probe.

**Verified, not only argued.** A scratch COPY of
`forgeshape_sketch_arrangement.cpp` had only the `nodeUsed` early return
replaced by a bounds check. It was linked against the unmodified rest of the
domain and run over **all 127 non-empty subsets** of the 7-cell sketch:

- every subset merges `Ok`;
- every subset regenerates through `regenerateCadBody` with volume within the
  existing 2% chord bound of the summed face areas;
- the OWNER's 6-cell state (all minus C-inside) → 2 components, mesh
  components 2, volume 12.000 (exact);
- `{0, 2}` → 2 components, the first carrying its A∪B hole;
- `{4, 6}` → 2 components.

No repo file was changed.

What decides "one component or two" is therefore **edge adjacency**. Cells
sharing a fragment fuse, because the walk cancels the fragment. Cells sharing
only a node stay apart. No union-find is needed beside the walk; the walk
already is one.

The three requested cases:

| case | derived extrusion |
| --- | --- |
| A. faces share an edge | ONE component: shared fragment cancelled, one prism, no internal wall (as today) |
| B. faces disjoint | TWO components, two prisms (as today) |
| C. faces meet at one point | TWO components touching along one vertical edge. Separate vertex rings, so the solid is closed and oriented (K1). Today this is refused. |

## 5. Operation rules

### New Body

The body is every derived component, one prism each, in ONE `CadSolid`. This
is the existing multi-component New Body. Point-touching components are two
shells that touch along an edge:
- Single-feature bodies never pass through the kernel (`regenerateCadBody`'s
  `chain.size() == 1` path).
- A later feature validates the base through `cadKernelValidateSolid`. That is
  `Ok` with 2 components (K1).
- Mesh `components` reports 2.

### Add

ONE kernel `Union` of the target with ONE tool solid carrying every component.
This keeps the "one kernel boolean per feature" invariant (K3 == K4). The
operation rules stay as they are, measured on the kernel's result:
- `AddDisjoint`: the result has more shells. This now also names the case
  where one point-touching tool component reaches the target only through
  another component's corner. It is a disconnected lump by the kernel's own
  count, which is the honest name.
- `AddNoEffect`: unchanged.

### Cut

ONE kernel `Difference` with the same multi-shell tool (K5 == K6).
`CutRemovesBody` and `CutNoIntersection` are unchanged.

### Preview

The preview IS the candidate (`CAD-VERTICAL-SLICE-R1`), unchanged:
- every selected cell is hatched;
- the candidate is evaluated over the whole set;
- an operation-level refusal is named in the status line and on the badge;
- the toolbar's Extrude is withdrawn.

**The selection is never trimmed to make the preview valid.**

## 6. Error timing

| when | may refuse with |
| --- | --- |
| Finish Sketch | `decideSketchSelectionMode` / arrangement statuses (`PlanarFaceAmbiguousOverlap`, `PlanarFaceCapExceeded`, `SelfIntersectingProfile`, …) — unchanged |
| tap (add) | `TooManyRegions` only (plus `NotSketching` / `ProfileNotFound` guards) |
| tap (remove) | never |
| preview / commit | `PlanarFaceDegenerate`, `TriangulationFailed`, `TooManyEntities` (loop > `kMaxProfileVertices`), `AddDisjoint`, `AddNoEffect`, `CutRemovesBody`, `CutNoIntersection`, `KernelFailed`, `FeatureSupportInvalid`, `SupportFaceLost` |
| load / reopen | `PlanarFaceUnresolved`, `PlanarFaceRefMalformed`, `PlanarFaceRefNotCanonical`, plus the preview set via `validateCadBodyState` |

## 7. Named statuses

- **`PlanarFacesTouchAtPoint` (61)** is no longer produced by any selection.
  The enum value is kept so the codes stay append-only (`static_assert` in
  `forgeshape_sketch.cpp:15`), and so does its Java message. Its comment
  becomes "retained, not emitted". `ArrangementStatus::PinchedSelection` is
  kept on the same terms so `cadStatusForArrangement` stays total.
- `OverlappingRegions` and `OverlappingHoles` stay the LOOP model's names.
  They are never returned for faces (FILL-06d already pins that).
- Everything else above keeps its name and meaning.

## 8. Persistence impact

- **No format, section, version or fixture byte changes.** The stored form
  (`CADB` v6 `PlanarFaces` selection) is the same sorted ref list.
- **The decoder becomes more accepting.** A v6 file whose selection pinches was
  refused by name (`validateCadBodyState` → `PlanarFacesTouchAtPoint`). It now
  opens. No committed fixture depends on that refusal:
  - `cad_overlap_face_v6` is `PlanarFaceAmbiguousOverlap`;
  - corpus 57/57 is unchanged and keeps its verdicts.

  This is the same terms on which `cad_spline_face_v6` became valid in
  `CAD-V6-S2-CORRECTION-FILL-HUD-R1` without a v7.
- **An older build** (this unmerged branch's earlier APKs) refuses such a file
  by name instead of mis-reading it. It fails closed, never corrupts.
- `DATA_PACKAGE_SPEC.md` validation table row "a face selection whose UNION
  pinches → `PlanarFacesTouchAtPoint`" is rewritten as accepted, with the
  component rule from §4.

## 9. History impact

None. A tap is still volatile session state: no history step, no fingerprint,
no checkpoint. Commit is still ONE `ScopedConstructionEdit` around ONE
`applyState`. Undo/Redo restore the stored ref set, which is now never refused
on re-validation for a pinch.

## 10. Residual risks to pin in the implementation's tests

- An ATOMIC face whose own boundary pinches would make `finish()` refuse the
  whole sketch. `finish()` builds `faceShapes_` per face through the same merge
  (`forgeshape_sketch_session.cpp:1330-1340`). After §4 that refusal
  disappears too.
  - Probes found no such face: a tangent circle, externally tangent circles,
    two squares sharing a corner, and a diagonal to a corner all derived 0
    self-pinched faces.
  - Pin it anyway.
- Hole ownership when a hole touches its outer at a node: pin the
  pinched-ring case (§2) through `mergePlanarFaces` itself.
- The hatch, the selection anchor (`planarSelectionAnchor` reads the first
  component), and the face-support chooser over point-touching components.
  Each reads the component list and needs no rule change. Pin them by test.
