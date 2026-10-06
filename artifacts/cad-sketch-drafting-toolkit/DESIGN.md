# CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1 — DESIGN

Written before implementation, from `BEFORE.md`. Technical drafting INSIDE
Sketch; no constraint solver, no drawing sheets.

## 1. Entity role storage

- `enum class SketchEntityRole : uint8_t { Regular, Construction }` in
  `forgeshape_sketch.h`, stored on `SketchEntity` (`role_`, default
  `Regular`). Domain truth: `sameSketchEntity`, history snapshots, the codec
  and the fingerprint all see it. No Java or renderer copy.
- Applies to all six kinds. Every legacy record (v1..v7) decodes `Regular`.
- **Material topology ignores Construction, in exactly two places**:
  `extractClosedProfiles` (loops, chains) and the arrangement's
  `collectSourceEdges` (PlanarFace input). Everything derived from them —
  regions, faces, tokens, lineage, fill cells, preview, Extrude/Revolve — is
  therefore construction-free with no further predicate.
- Construction stays: selectable, snappable (all snap kinds), dimensionable,
  mirrorable, offsettable (its kind permitting), a valid Revolve axis
  (`resolveRevolveAxis` reads every entity) and a valid Mirror axis.
- Drafting intersection queries (snap, Trim, Extend) need ALL curves, so the
  arrangement gains ONE switch: `deriveSketchArrangement(sketch,
  ArrangementCurves::AllCurves)` — the SAME contact/fragment machinery with the
  role filter off. Default (`MaterialOnly`) is what every existing caller uses.
- Toggle: `setSketchEntitiesRole(sketch, ids, role)`; the session's one act
  `toggleSelectionConstruction()` — if ANY selected entity is Regular the act
  is **Make Construction** (all become Construction), otherwise **Make
  Regular**. One deterministic label for a mixed selection
  ("Make Construction"). Ids and coordinates are untouched; the derived
  profiles are re-extracted at the next Finish/preview as always.
- Visual: Construction entities are DASHED (CPU-generated dash segments, fixed
  reference-unit rhythm) in their own overlay range
  `SketchOverlayStyle::Construction` with a lower alpha than `Entities`;
  selected Construction stays emphasised (highlight tag) and dashed. Dash +
  weight carry the meaning, never colour alone.

## 2. Dimension record model

```
using SketchDimensionId = uint32_t;            // per sketch, non-zero
enum class SketchDimensionKind : uint8_t {
    LineLength, LineAngle, LineHorizontal, LineVertical,
    RectangleWidth, RectangleHeight, CircleRadius, CircleDiameter,
    EdgeLength, ArcRadius, ArcSweep, EdgeAngle };
enum class SketchDimensionMode : uint8_t { Driving, Reference };
struct SketchDimension {
    SketchDimensionId id; SketchDimensionKind kind; SketchDimensionMode mode;
    CadSketchEdgeRef first;   // entity + semantic edge-local index
    CadSketchEdgeRef second;  // EdgeAngle only; {0,0} otherwise
};
CadSketch { ...; std::vector<SketchDimension> dimensions;  // ascending id
            SketchDimensionId nextDimensionId = 1; }
```

- `CadSketchEdgeRef` moves from `forgeshape_cad_body.h` into
  `forgeshape_sketch.h` (same struct, same identity family as the Revolve
  axis). Never a tessellation, sample, renderer index or screen point.
- **No stored number.** The displayed value is DERIVED from authored geometry
  (`sketchDimensionValue`). A Driving edit mutates geometry through ONE path
  (`applySketchDimensionValue`) and stores nothing else.
- Ids: `nextDimensionId` high-water persisted in v8; committed forward edits
  never lower it (the sketch's own rule, checked by `CadBody::applyState`
  beside the body marks); Undo/Redo restore it with the snapshot; a cancelled
  session burns nothing (the session mints into its staged copy).
- Bound: `kMaxSketchDimensions = 512` per sketch.

### Driving matrix (R1)

| kind | target | edit keeps | value domain |
| --- | --- | --- | --- |
| LineLength | Line | P0 and direction | finite, `validateDimensionMeters`, ≤ 1e5 |
| LineAngle | Line | P0 and length | degrees in (-180, 180], refused outside |
| RectangleWidth | Rectangle | centre, height | length rule |
| RectangleHeight | Rectangle | centre, width | length rule |
| CircleRadius | Circle | centre | length rule |
| CircleDiameter | Circle | centre (same radius DOF) | length rule, r = d/2 |

### Reference matrix (R1, read-only)

| kind | target | value |
| --- | --- | --- |
| LineLength / LineAngle | Line | as above (Reference allowed) |
| LineHorizontal | Line | `|Δu|` |
| LineVertical | Line | `|Δv|` |
| RectangleWidth/Height, CircleRadius/Diameter | as above | as above |
| EdgeLength | Polyline segment i, Rectangle edge k | segment length |
| ArcRadius | Arc | circumradius (`arcGeometry`) |
| ArcSweep | Arc | `|sweep|` in degrees |
| EdgeAngle | two distinct straight edges | unsigned angle between the edges' directions, `atan2(|cross|, dot)`, in [0°, 180°] |

No Spline dimension of any kind (no fake exact length).

### Conflict rule (no solver)

- DOF owners: LineLength → (entity, length); LineAngle → (entity, angle);
  RectangleWidth → (entity, width); RectangleHeight → (entity, height);
  CircleRadius and CircleDiameter → (entity, radius).
- A second Driving dimension on an owned DOF → `SketchDimensionConflict`. An
  exact duplicate (same kind and refs, any mode) → `SketchDimensionConflict`.
- Reference dimensions never own a DOF.
- Deleting a dimension removes the record only; geometry is untouched.

## 3. Dimension dependency rules (fail closed, no remap)

| act | rule | refusal |
| --- | --- | --- |
| Delete entities | dimensions whose refs ALL name deleted entities are deleted in the same act and counted (`Deleted N dimensions with the entity.`); a dimension naming a deleted AND a surviving entity refuses the delete | `SketchDimensionDependency` |
| Trim | target referenced by ANY dimension | `SketchDimensionDependency` ("Remove the dimension before trimming this entity.") |
| Extend | a Driving LineLength on the line | `SketchDimensionLocked`; Reference dimensions update |
| Offset / Mirror | new geometry gets no dimension records; originals keep theirs | — |
| Construction toggle | dimensions stay valid | — |

Validation (`validateCadSketch`) refuses any dimension whose ref no longer
resolves, so no dimension can survive pointing at a missing entity.

## 4. Format / version decision — `CADB` v8

v7 has no byte for a role or a dimension, so **v8 is required** (authorized).
v8 is the v7 layout (v6 sketch table + a kind byte after every feature id)
with two additions, both inside each sketch record:

1. every entity: `u32 id, u8 kind, u8 role (1 Regular, 2 Construction), payload`;
2. after the entity list: `u32 nextDimensionId, u32 dimensionCount`, then per
   dimension (ascending id, 22 bytes): `u32 id, u8 kind (1..12), u8 mode
   (1 Driving, 2 Reference), u32 first.entity, u32 first.edge, u32
   second.entity, u32 second.edge`.

Conditional writer: a sketch carries drafting truth when any entity is
Construction, any dimension exists, or `nextDimensionId != 1`.
`cadBodyStateLegacyRepresentable` is false for such a state (so it takes the
table layout) and `cadDocumentNeedsV8` selects section version 8. Every other
project writes exactly what it wrote before (v1..v7), byte for byte, with its
fingerprint unchanged (a `DRF8` fingerprint block is mixed only for drafting
states).

Decoder (structure) + `validateCadSketch` (relations) refuse: unknown role
code, unknown dimension kind, invalid mode, zero/duplicate id, non-ascending
order, `nextDimensionId` not above every id, unresolved entity/edge refs,
second ref on a non-angle kind, Driving on a Reference-only kind, conflicting
Driving DOFs, count over the bound — counts are proven against the remaining
bytes before any allocation. An older build refuses v8 as a required section
at an unknown version.

Fixtures (5): `cad_construction_v8`, `cad_dimension_driving_v8`,
`cad_dimension_reference_v8`, and two the decoder must refuse:
`cad_bad_dimension_ref_v8`, `cad_dimension_conflict_v8`. Built by the
PowerShell encoder from the spec; corpus 61 → 66, the 61 unchanged.

## 5. Tool support matrix

| tool | Line | Polyline | Rectangle | Circle | Arc | Spline |
| --- | --- | --- | --- | --- | --- | --- |
| Construction role | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Trim target | ✓ (delete / shorten / split) | segment ✓ (shorten / split, order kept) | edge ✓ → 4 Lines then trimmed (`Rectangle converted to lines by Trim.`) | ✓ → Arc (or deleted with ≤ 1 cut) | ✓ → 0..2 Arcs | `TrimSplineUnsupported` (cutter ✓) |
| Extend | ✓ either end | open, end segment only | `ExtendUnsupported` | `ExtendUnsupported` | ✓ along its circle | `ExtendUnsupported` (target ✓) |
| Offset | parallel Line | mitered (open/closed), miter limit 4, self-intersection refused | same-centre Rectangle ±2d | concentric, r>0 | concentric, same angles, r>0 | `OffsetSplineUnsupported` |
| Mirror | ✓ | ✓ | axis-parallel → Rectangle, else 4 Lines | ✓ | 3 points reflected | points reflected |
| Mirror axis | ✓ | segment ✓ | edge ✓ | ✗ | ✗ | ✗ |

- Trim uses the all-curves arrangement: the fragment of the tapped edge under
  the finger is exactly what is removed (no second intersector). An
  arrangement that cannot derive (overlap, cap) refuses by its own name.
- Ids on Trim: the canonical FIRST surviving piece keeps the id when the kind
  is unchanged (Line→Line, Polyline→Polyline, Arc→Arc); extra pieces, and every
  piece of a kind change (Rectangle→Lines, Circle→Arc), get fresh ids from the
  sketch allocator. Pieces inherit the role.
- Extend: the target's exact continuation (a ray probe for a line/segment, the
  rest of its circle for an arc) is intersected with each other entity through
  the same arrangement machinery; the nearest forward contact wins; none →
  `ExtendNoTarget`; a collinear overlap with the probe → `ExtendAmbiguous`;
  probe bounded to the sketch range.
- Offset sign: positive = left of the source direction (line/polyline),
  outward (circle/arc/rectangle). One-time creation; no stored link; role
  inherited; no dimension copied.
- Mirror: reflect authored points across the infinite axis line in binary64;
  the axis entity itself is never mirrored; role preserved; no dimension
  copied.

## 6. Snap priority

`forgeshape_sketch_snap.{h,cpp}`, a pure function over the sketch (Construction
included) and the raw point:

1. Endpoint — line ends, polyline vertices, rectangle corners, arc start/mid/end,
   spline authored points
2. Intersection — the all-curves arrangement's crossing / T-junction nodes
   (ambiguous overlap → no intersection candidates at all)
3. Midpoint — line, polyline segment, rectangle edge (exact average)
4. Center — circle centre, arc centre (`arcGeometry`), rectangle centre
5. Origin — exactly (0, 0)
6. Horizontal / Vertical guide — a raw point whose v (u) lies within tolerance
   of a reference point's (endpoints, midpoints, centres, origin, and the
   gesture's own start) snaps THAT coordinate to the reference's exact value;
   both may apply at once; a free coordinate falls to the grid
7. Grid

Tolerance: 24 dp in world units at pointer-down. The first non-empty priority
level wins; within it the smallest distance; then the candidate's semantic
order (entity id, edge/point index; arrangement node order). The arrangement
is cached per sketch content, so a pointer move costs a bounded scan of
precomputed points, never a re-derivation. Guides are drawn dashed while
active and create NO persistent relation. Markers: endpoint square,
intersection X, midpoint triangle, centre circle, origin double square,
guide = dashed line, grid = small plus.

## 7. Mobile UI placement

- Tool Rail unchanged (seven drawing tools).
- A **Modify** capsule under the orientation navigator (upper trailing),
  Editing only. It opens the **sketch actions palette** (an anchored surface,
  never a bottom toolbar), contextual — a control that cannot succeed is not
  drawn:
  - nothing selected: Dimension, Trim, Extend, Offset, Select multiple,
    Dimensions (Selected / All / Off)
  - one entity: Dimension (if its kind has one), Make Construction / Make
    Regular, Offset (if supported), Mirror, Delete, Select multiple,
    Dimensions
  - several: Make Construction / Make Regular, Mirror, Delete, Select multiple,
    Dimensions
- An active modify mode shows a compact **mode capsule** in the same place:
  its name, its context (Offset: exact distance field; Mirror: Choose mirror
  axis; Dimension: the applicable kinds as chips plus Driving/Reference), and
  Confirm / Done / Cancel. While a mode is active a viewport tap means that
  mode only; choosing a drawing tool ends the mode.
- Dimension labels: one chrome label per visible dimension at its projected
  native anchor (≥ 48 dp hit proxy; the container is not clickable so a
  missed tap reaches the viewport); `R`, `Ø`, `°`; Reference in parentheses
  and muted; Driving tappable → compact field + Apply + Delete; Reference tap
  → Delete only. Collisions hide the lower-priority label (selected entity
  first, then Driving, then id), never teleport it.
- Exact input: plain numbers mandatory; a small safe BigDecimal expression
  parser (`+ - * /`, parentheses) is used by the drafting fields only.

## 8. Test strategy

- Host (inside the CAD_FEATURE suite, no new startup token): DR-CON-01..06,
  DR-DIM-01..17, DR-SNAP-01..10, DR-TRIM-01..10, DR-EXT-01..06,
  DR-OFF-01..10, DR-MIR-01..10, DR-FMT-01..11, performance tokens.
- JVM: label formatting (R/Ø/°/parentheses), visibility filter, collision
  layout, ≥ 48 dp proxy, palette contents matrix, expression parser,
  construction/marker presentation constants.
- Device: `CadSketchDraftingOwnerTest` (DEV-DR-01..12, real window touches)
  in one focused `CI DEVICE` union with the eleven existing classes.
- Gates: host aggregate, JVM, debug/release/androidTest builds, release guard,
  corpus parity 66/66, `git diff --check`, CI FAST, CI DEVICE.
