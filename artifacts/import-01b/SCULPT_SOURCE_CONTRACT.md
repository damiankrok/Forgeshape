# IMPORT-01B — the Imported Mesh sculpt source contract (`ARCH-OWNER-11`)

## The one thing that was added

`buildSculptSourceMesh(const SceneObject&, ConstructionMesh*)` in
`forgeshape_scene.{h,cpp}`. It is the second ONE dispatch point beside
`publishSceneObject`, and it is the entire difference between the two
representations as far as sculpting is concerned.

| | Construction Body | Imported Mesh |
| --- | --- | --- |
| where the seed comes from | `ConstructionObject::generateMesh()` | the body's own `positions()` / `indices()` |
| what is read | parameters | the stored arrays |
| what is written | nothing | nothing |
| sidedness | the generated mesh's `renderBothSides` | true when ANY submesh is `doubleSided` |
| colour of the seed | the generator's | `kImportedMeshVertexColor` |

Everything downstream is unchanged: `SculptMesh::freezeFrom`, `SculptTopology`,
`computeVertexNormals`, `SculptStroke`, all four tools, `publishSculptMesh`, the
`SCUL` branch, the `hasEdits` guard, the stale flag. No sculpt subsystem was
added, no brush was added, and no second freeze path exists.

## Two decisions that carry the correctness

### 1. The RAW index array, never `buildDrawData`'s

`ImportedMesh::buildDrawData` emits a double-sided submesh's triangles a SECOND
time with reversed winding. That is right for drawing and ruinous for sculpting:
`computeVertexNormals` accumulates each triangle's unnormalized geometric normal
`(v1-v0) x (v2-v0)` into all three corners, and a reversed copy contributes the
exact negation of its twin. A seed built from the draw buffer would give every
vertex of a two-sided submesh a **zero** normal, and Clay and Inflate — both
normal-driven — would not move it at all.

Asserted by `IMP01B_02_the_seed_is_not_the_doubled_draw_index_buffer` (the seed's
index count is the source's, and strictly smaller than the draw buffer's) and by
`IMP01B_02_the_frozen_normals_are_real_directions` (every frozen vertex normal
has length ≥ 0.5, which a cancelled pair could not).

### 2. Per-submesh `doubleSided` collapses permissively

A frozen mesh has ONE sidedness answer by construction — `SculptMesh` carries a
single `renderBothSides`, which reaches the render mesh's backside duplication,
CPU picking's front-faces-only flag and the exporter's `doubleSided`. An imported
object legitimately has one answer per submesh.

The collapse is `true when ANY submesh is two-sided`. The permissive direction is
the safe one: it keeps an imported sheet both visible and **reachable by a brush
from behind**, where the strict one would leave half of what the user imported
untouchable. The cost is that a closed submesh in a mixed mesh also renders its
back faces, which depth hides anyway.

## What is NOT carried across

**The file's stated normals.** A Frozen Sculpt Mesh derives its normals from its
CURRENT positions, because a stroke moves them — already true of every
Construction freeze. The Imported Mesh keeps its own normals untouched, so *Back
to Imported Mesh* shows the artist's hard edges exactly as the file stated them,
and the `.forge` `IMPT` record still stores them.

**The placement.** Both representations are LOCAL geometry under the one authored
`ConstructionTransform`. The import already split the node transform — linear part
into the geometry, translation onto the body — and the freeze copies local
positions unchanged, so the same model matrix carries the imported point and the
seeded point to the same world place. Nothing is recentred and nothing is
re-baked.

Asserted by `IMP01B_03_the_first_sculpt_frame_is_the_same_world_geometry`, which
compares `model * imported[v]` with `model * sculpt[v]` for every vertex, and by
`IMP01B_03_freezing_does_not_touch_the_authored_transform`.

## Immutability of the source

`ImportedMesh` has no mutation path at all: it is built once through
`ImportedMesh::build`, validated once by `validateImportedMeshData`, and read
thereafter. There is no setter, no non-const accessor and no friend.

The self-test drives a **real Grab stroke** against an imported-derived sculpt
mesh and then compares, with `operator==` on the vectors:

- `positions()` — `IMP01B_05_the_imported_positions_are_unchanged`
- `normals()` — `IMP01B_05_the_imported_normals_are_unchanged`
- `indices()` — `IMP01B_05_the_imported_topology_is_unchanged`
- `batches()` — `IMP01B_05_the_imported_submesh_batches_are_unchanged`
- the authored transform — `IMP01B_05_and_the_authored_transform_is_unchanged`
- and that no Construction Source appeared —
  `IMP01B_10_and_no_construction_source_was_invented`

`ImportedMeshSculptTest.imp01b04and05_*` does the same thing on a device, through
the whole touch path, by comparing the `.forge` document's `IMPT` section byte
for byte before and after the stroke — which is the one place the imported arrays
are observable end to end.

**An Imported Mesh can never go stale.** `markSourceStale` answers "has the
SOURCE moved on since the freeze", and nothing can move an imported one. The flag
is never set for such a body and the warning block never appears over one. That
is a consequence of the representation, not a branch in the UI.

## State transitions

| state | what the toolbar offers |
| --- | --- |
| imported body, nothing frozen | *Start Sculpting*, context *Imported Mesh* |
| imported body, sculpt mesh retained | *Resume Sculpt*, context *Imported Mesh* |
| sculpting an imported body | *Back to Imported Mesh*, context *Sculpt* |

`Back to Imported Mesh` and `Back to Construction` are the SAME native act —
`enterConstructionMode`, which republishes the body's source through
`publishConstructionObject`'s per-representation dispatch. Only the label and the
content description differ, and the view id stays `back_to_construction` because
an id names the act the code performs.

`Reset Sculpt from Imported Mesh…` is the same act as `Reset Sculpt from Shape…`
— a re-freeze through `freezeToSculpt` — with the same `SCULPT_HAS_EDITS` guard,
the same confirmation, and a message naming the imported source instead of a
shape. Cancelling makes no native call.

## Export

**No exporter change was needed or made.** `captureGlbExportScene` already
decides with `useSculpt = (kind == ProjectKind::Sculpt) && frozen.mesh.frozen()`
— a rule about the project's MODE and the body's sculpt state, not about its
source representation. So an imported body with a Frozen Sculpt Mesh exports the
sculpted geometry in Sculpt and the imported source otherwise, which is exactly
what a Construction body with one already did.

`ImportedMeshSculptTest.imp01b14_*` verifies it end to end: an export taken
before a stroke differs from one taken after, and an export taken from the source
view differs from the sculpted one — and exporting records no history step and
touches neither `.forge` slot.
