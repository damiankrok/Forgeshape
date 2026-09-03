# ForgeShape `.forge` project format — v1

**Owner of this document:** the exact binary layout of a ForgeShape project
file, its section payload field order, its enum numeric codes, its validation
and compatibility rules, its deterministic-write rules, and the fixture
inventory. Nothing else owns any of those.

Implementation: `app/src/main/cpp/forgeshape_project_bytes.{h,cpp}` (little-endian
primitives and CRC-32), `forgeshape_project_document.{h,cpp}` (the document and
the codec), `forgeshape_project_state.{h,cpp}` (live scene ↔ document).
`forgeshape_imported_mesh.{h,cpp}` owns what an Imported Mesh may be, and the
codec calls its validator rather than restating it.
Independent second implementation: `scripts/build-forge-corpus.ps1`.

---

## 1. What a project is

A `.forge` file is a **declarative semantic document** — a feature graph plus
placement. It is not a render-mesh snapshot, not a UI event log, and not the
Undo/Redo stack.

Today the graph is degenerate: each **Construction Body** carries exactly one
real feature, a **PrimitiveSource**, and the body's placement lives beside it.
Future CAD features extend the graph; the format already carries a per-body
feature list so that they can arrive without the file changing shape.

Since `CAD-R0-A1A2` a body may instead be a **CAD Body**: one sketch on a
principal workplane and one linear extrusion of one of its closed profiles.
That is its own geometry source, carried in its own required section (`CADB`,
§7b) on `IMPT`'s terms, because a CAD Body has no primitive to be a
Construction Source with and no fixed geometry to be an Imported Mesh with. Its
mesh is regenerated on load, exactly as a primitive's is.

### 1.1 Semantic project truth — serialized

| Truth | Why it cannot be recomputed |
| --- | --- |
| Body identity (`ObjectId`), scene order, active body | Identity is minted, not derived |
| Which representation a body's geometry comes from | A body owns a Construction Source, an Imported Mesh or a CAD Body's sketch-and-extrusion, exactly one of the three |
| A CAD Body's workplane, every sketch entity with its per-sketch id, the sketch's id allocator, the chosen profile's anchor entity id, the extrusion depth and its direction | The authored truth of the body; the mesh is a product of it |
| The id allocator's high-water mark | Stops a reopened project minting a collision |
| Active primitive kind | The user chose it |
| **All six** remembered primitive parameter sets, per body | A Box → Sphere → Box round trip must return the box the user typed |
| Each Imported Mesh's display name, local float32 positions, effective normals, index topology and submesh ranges with their `doubleSided` | An imported object IS its geometry: no rule could recreate it, and the `.glb` it came from is not part of the project |
| Placement: position (metres), rotation (degrees), scale (unitless) | The user placed it |
| Each Frozen Sculpt Mesh's local float32 vertex **positions** | After a stroke they cannot be recreated from the Construction Source |
| Each Frozen Sculpt Mesh's index topology | Same |
| `renderBothSides` | A geometric fact about that frozen representation, not about the current source |
| `sourceStale` | Whether the source moved on after the freeze |
| `hasEdits` (a **boolean**, never a revision or an undo depth) | What the destructive *Reset Sculpt from Shape* guard asks; losing it would let a reset discard the whole file silently. Since `ARCH-OWNER-12` it is a stored fact on the mesh rather than a predicate over the revision, because an Undo moves geometry backwards while the revision only ever goes forwards — and a project restored with edits starts with an empty sculpt history, so an undo depth could not answer it either |

### 1.2 Derived runtime/render truth — **not** serialized

* Every Construction `RuntimeMesh` — regenerated from the parameters on load.
* Every CAD Body's closed profiles, their polygons, their triangulation and the
  extruded mesh — all regenerated from the `CADB` record by `generateCadMesh`
  on load. No polygon point and no vertex of an extrusion is ever stored.
* The sketch edit session: a sketch in progress, its selection, its drag and
  its held tool are volatile until the one commit that makes a CAD Body, and a
  `.forge` file never carries a half-drawn sketch.
* Vertex normals, sculpt adjacency (`SculptTopology`), render-mesh vertex
  duplication for hard edges.
* `SculptRevision`, `MeshRevision`, update counters, rejection counters.
* GPU buffers, upload diagnostics, renderer caches.
* The source `.glb` an Imported Mesh came from: its path, its `Uri`, its bytes,
  its node index, and every material, colour and UV attribute the reader
  validated and then never decoded. Where geometry came from is not project
  truth, and an appearance that was never read cannot be preserved.
* Vertex **colours**. They feed only the debug-only source-colour shading mode —
  Studio Solid and MatCap both ignore them — so they are presentation, not
  truth. A loaded sculpt vertex is given one neutral value
  (`kLoadedSculptVertexColor`, 0.5 in each channel).
* The Construction Undo/Redo stack. It is session history: a successful load
  starts a fresh session, and a failed load leaves the existing history
  untouched.
* **The Sculpt Undo/Redo stacks** (`SculptHistory`, `ARCH-OWNER-12`). Runtime
  only, and for the same reason: the stored positions are where the user's mesh
  got to, and the strokes that reached it are a property of the editing session,
  not of the document. A successful load starts an empty sculpt history over the
  restored geometry; a failed load changes nothing at all. This added **no**
  field, section, flag or version bump — the layout below is byte-identical to
  what it was before Sculpt Undo existed, which the committed corpus digests
  prove.
* Camera pose, open panels, display unit, appearance, shading model, held tool,
  brush radius and strength. All session or presentation state.

---

## 2. File header — 28 bytes

All multibyte fields are explicit little-endian. Offsets are from byte 0.

| Offset | Field | Encoding | v1 value |
| ---: | --- | ---: | --- |
| 0 | magic | 8 bytes ASCII | `FORGESH1` |
| 8 | major | `u16` | `1` |
| 10 | minor | `u16` | `0` |
| 12 | headerBytes | `u16` | `28` |
| 14 | projectKind | `u8` | `1` = Construction, `2` = Sculpt |
| 15 | headerFlags | `u8` | bit0 `hasCONS`, bit1 `hasSCUL`, bit2 `hasIMPT`, bit3 `hasCADB`; all other bits zero |
| 16 | sectionCount | `u32` | exact number of sections that follow |
| 20 | fileBytes | `u64` | exact total file size |

The magic's trailing `1` is a **generation marker, not the version**. The
version is the `major`/`minor` pair.

`headerFlags` must agree with the sections actually present, or the file is
refused: a reader that trusted the flags without parsing would otherwise be told
something false.

`hasIMPT` is also the **compatibility gate**. Every reader before `IMPORT-01A`
refuses an unknown header-flag bit outright (`BadHeader`), so a build that could
not reconstruct an Imported Mesh cannot open a file carrying one at all — rather
than opening it with those objects silently missing. The `IMPT` section's
required bit says the same thing a second time, for a reader that got past the
header. The `major`/`minor` pair is unchanged at `1`/`0`: §12 already allows a
new required section within a major, and bumping the minor would have rewritten
every legacy fixture's bytes for no added protection.

`hasCADB` (`CAD-R0-A1A2`) is the same gate a second time, for the same reason:
a build that cannot regenerate a sketch cannot open a file that needs one, and
refuses it on the header flag first and the `CADB` section's required bit
second. Files without a CAD Body are byte-for-byte what they were.

---

## 3. Section header — 24 bytes, before every payload

| Offset | Field | Encoding | Rule |
| ---: | --- | ---: | --- |
| 0 | tag | 4 bytes ASCII | FourCC |
| 4 | sectionVersion | `u16` | this section's schema version |
| 6 | flags | `u16` | bit0 = required; every other bit must be zero in v1 |
| 8 | payloadBytes | `u64` | exact payload length |
| 16 | crc32 | `u32` | CRC-32/ISO-HDLC of the payload bytes only |
| 20 | reserved | `u32` | must be zero |

**CRC-32/ISO-HDLC**: `poly = 0x04C11DB7`, reflected implementation `0xEDB88320`,
`init = 0xFFFFFFFF`, `refin = refout = true`, `xorout = 0xFFFFFFFF`. Check value:
CRC of the ASCII string `123456789` is `0xCBF43926`. It covers the **payload
alone**, which is what lets a reader validate a section it does not understand
and then skip it safely.

**Canonical writer order:** `SCNE`, then `CONS` when present, then `SCUL` when
present, then `IMPT` when present, then `CADB` when present.

---

## 4. Numeric codes owned by the file

These are **file-owned** and deliberately not `static_cast` of any C++ enum: the
enumerator order is an internal decision, and a file must not move underneath a
project if it ever changes.

| Primitive | code |
| --- | ---: |
| Box | 1 |
| Cylinder | 2 |
| Sphere | 3 |
| Cone | 4 |
| Capsule | 5 |
| Plane | 6 |

| Feature kind | code |
| --- | ---: |
| PrimitiveSource | 1 |

| ProjectKind | code |
| --- | ---: |
| Construction | 1 |
| Sculpt | 2 |

The `CADB` codes (`CAD-R0-A1A2`), file-owned on the same terms:

| Workplane | code | | Extrude direction | code | | Sketch entity kind | code |
| --- | ---: | --- | --- | ---: | --- | --- | ---: |
| XY (U = +X, V = +Y, N = +Z) | 1 | | Along the normal | 1 | | Line | 1 |
| XZ (U = +X, V = −Z, N = +Y) | 2 | | Against the normal | 2 | | Polyline | 2 |
| YZ (U = −Z, V = +Y, N = +X) | 3 | | | | | Rectangle | 3 |
| | | | | | | Circle | 4 |

Every workplane frame is right-handed (`U × V = N`), which is what lets a
profile that is counter-clockwise in `(u, v)` extrude with canonical outward
winding on any of the three.

---

## 5. `SCNE` v1 — the project-neutral scene record

Required for **both** project kinds. Section version 1, required bit set.

```
u32  bodyCount                 1 .. 4096
u64  nextObjectId              must be > every objectId below
u64  activeObjectId            must name one of the bodies below
repeat bodyCount times, in SCENE ORDER:
  u64  objectId                != 0, unique within the file
  f64  positionX, positionY, positionZ      metres
  f64  rotationX, rotationY, rotationZ      degrees, NOT canonicalized
  f64  scaleX,    scaleY,    scaleZ         unitless, strictly positive
```

Per body: 8 + 9 × 8 = **80 bytes**. Payload = `4 + 8 + 8 + bodyCount × 80`.

Rotation is stored **exactly as given**: `370°` stays `370°` and comes back as
`370°`. Reduction modulo 360 happens only inside derived trigonometry.

Scale is a transform multiplier, never a dimension: strictly above
`kMinScaleFactor` (1e-6). Zero is singular and negative is a Mirror the product
does not have; both are refused, never clamped.

---

## 6. `CONS` v1 — the Construction feature graph

Required when `projectKind = Construction`; the optional retained companion when
`projectKind = Sculpt`. Section version 1. Keyed by `ObjectId`, and it carries
**one entry per SCNE body that has a Construction Source**, as a subsequence of
`SCNE` in strictly ascending scene order.

That rule read as "one entry per SCNE body" until `IMPORT-01A`, and it looked the
same because every body had one. An Imported Mesh has none, and no entry may be
fabricated for it: a default Box standing in for geometry the file actually
carried would be inventing project data, and a later edit would reshape a body
from parameters nobody authored. A file whose bodies are all Construction Bodies
is byte-identical to what v1 always wrote.

```
u32  bodyCount
repeat bodyCount times, in SCENE ORDER:
  u64  objectId                must equal the SCNE body at the same index
  u8   activePrimitiveCode     1 .. 6, from the table above
  f64  box.width, box.height, box.depth
  f64  cylinder.diameter, cylinder.height
  f64  sphere.diameter
  f64  cone.bottomDiameter, cone.height
  f64  capsule.diameter, capsule.totalHeight
  f64  plane.width, plane.depth
  u32  featureCount
  repeat featureCount times, ascending LocalFeatureId:
    u32  localFeatureId
    u8   featureKindCode
```

Per body with the v1 single feature: 8 + 1 + 96 + 4 + 5 = **114 bytes**.
Payload = `4 + bodyCount × 114`.

All twelve parameters are written for every body — the five **inactive**
primitive parameter sets are user data, not spare state.

The v1 feature graph is exactly one `PrimitiveSource` per body, identified by the
pair `(ObjectId, LocalFeatureId = 1)`. A file with more, fewer or a different
kind describes a CAD project this version cannot evaluate and is refused rather
than guessed at.

No Construction `RuntimeMesh` appears anywhere. The section's length is fixed by
the body count, so there is physically no room in it for a vertex.

---

## 7. `SCUL` v1 — the Frozen Sculpt Meshes

Required when `projectKind = Sculpt`; optional retained data when
`projectKind = Construction`. Section version 1. Carries an entry only for bodies
that **have** a Frozen Sculpt Mesh, in scene order.

```
u32  entryCount                1 .. 4096
repeat entryCount times, in ascending SCENE ORDER:
  u64  objectId                must be a SCNE body; entries strictly ascending
  u8   flags                   bit0 renderBothSides
                               bit1 sourceStale
                               bit2 hasEdits
                               all other bits must be zero
  u32  vertexCount             1 .. 4,000,000
  u32  indexCount              1 .. 24,000,000, a multiple of 3
  f32  positions[vertexCount * 3]     LOCAL space, IEEE-754 bit patterns
  u32  indices[indexCount]            every index < vertexCount
```

Per entry: 8 + 1 + 4 + 4 + 12 × vertexCount + 4 × indexCount bytes.

Positions are the **bit patterns**, never a decimal rendering: a sculpted vertex
must come back exactly, and a text form would quietly round it. Normals,
adjacency and the revision are absent and are rebuilt from exactly this data by
`SculptMesh::freezeFrom` on load — the same path a fresh freeze takes, so a
loaded sculpt mesh is exactly as validated as a newly frozen one.

When `projectKind = Sculpt`, the **active body must have an entry here**. A
Sculpt project reopens showing the active body's sculpt mesh, so a file whose
mode and content disagreed would force the load to invent an answer.

A Sculpt file's `SCNE + SCUL` data is self-contained at the document level. While
the reversible Construction↔Sculpt runtime exists, such a file also carries the
source companion; neither branch overwrites the other, and only the header's
`projectKind` decides which representation the project reopens in.

**A `SCUL` entry's body may have EITHER source** -- `CONS` or `IMPT` -- since
`IMPORT-01B`. Which one it was frozen from is deliberately not stored, because
nothing reads it back: a Frozen Sculpt Mesh is its own positions and its own
topology whatever produced it. `SCUL` is not a third geometry SOURCE and does
not join the exactly-one-of rule; it is a second representation OF a body that
already has one.

---

## 7a. `IMPT` v1 — the Imported Meshes

Present when any body is an Imported Mesh, and then **required in both project
kinds** — unlike `CONS` and `SCUL`, whose required bit follows `projectKind`.
Those two are branches of data a reader can legitimately skip because the other
branch still describes the same bodies. An Imported Mesh has no other branch: it
is the only copy of its own geometry, and a reader that skipped it would open the
project with objects missing.

Section version 1. One entry per body whose representation is an Imported Mesh,
as a subsequence of `SCNE` in strictly ascending scene order.

```
u32  entryCount                1 .. 4096
repeat entryCount times, in ascending SCENE ORDER:
  u64  objectId                must be a SCNE body; entries strictly ascending
  u16  nameBytes               1 .. 96
  u8   name[nameBytes]         UTF-8, no terminator, no padding
  u32  vertexCount             1 .. 4,000,000
  u32  indexCount              1 .. 24,000,000, a multiple of 3
  u32  batchCount              1 .. 4096
  f32  positions[vertexCount * 3]    LOCAL space, IEEE-754 bit patterns
  f32  normals[vertexCount * 3]      unit directions, IEEE-754 bit patterns
  u32  indices[indexCount]           every index < vertexCount
  repeat batchCount times, in order:
    u32  firstIndex
    u32  indexCount            a multiple of 3, non-zero
    u8   flags                 bit0 doubleSided; all other bits must be zero
```

Per entry: 8 + 2 + `nameBytes` + 12 + 24 × vertexCount + 4 × indexCount + 9 ×
batchCount bytes. The name is variable-length in the middle of the record on
purpose: every field is read byte-wise little-endian, so alignment costs nothing,
and a fixed-width padded name would make one name several byte sequences.

**Positions are LOCAL.** The source node's transform is SPLIT at import: its
linear part — rotation, non-uniform scale, shear, none of which ForgeShape's
nine authored placement values can express — is baked into these positions, and
its translation becomes the body's `SCNE` placement. So an imported body's stored
rotation is `0,0,0` and its stored scale is `1,1,1` until the user moves it.
Nothing is recentred: the origin is the source node's own, which is the pivot
every downstream tool inherits.

**Normals are stored, not regenerated.** The file may have STATED them, and
re-deriving smooth normals from the triangles on load would silently replace an
artist's hard edges with this build's guess.

**Batches must tile the index array exactly, in order** — no gap, no overlap. A
gap would be triangles nothing draws and an overlap triangles drawn twice; either
means the record does not describe the geometry beside it. `doubleSided` is the
one material fact carried, because it decides which triangles are VISIBLE rather
than how they look; base colour, roughness, metallic, textures, COLOR_0 and
TEXCOORD were validated by the reader and never decoded, so there is nothing here
to preserve and no document may claim otherwise.

The name is held to the DOMAIN's own rule (`sanitizeImportedMeshName`): non-empty,
at most 96 bytes, well-formed UTF-8, no control characters, no leading or
trailing space, and exactly what that function would produce for itself. A file
cannot carry a name the importer could not have made.

Nothing about the source `.glb` appears — no path, no `Uri`, no bytes, no node
index. Where the geometry came from is not project truth.

A body must be named by **exactly one** of `CONS`, `IMPT` and `CADB`; a body in
two of them is two answers to what the object IS and is refused. A `SCUL` entry for an `IMPT`
body was refused too until `IMPORT-01B` and is **valid now**: an imported object
can be sculpted, seeded from its own geometry, and the sculpt mesh it gains is a
second representation of the same body rather than a second answer to what the
body is.

That rule change needed **no version bump**, because it can only ever be
encountered fail-closed: a build from before `IMPORT-01B` refuses an
`IMPT`+`SCUL` file outright (`UnresolvedReference`) rather than opening it with
half a body. An older reader cannot misunderstand such a file, only decline it --
the same shape `kHeaderFlagHasImported` already gives a pre-`IMPORT-01A` reader.
The four valid combinations are `SCNE+CONS`, `SCNE+CONS+SCUL`, `SCNE+IMPT` and
`SCNE+IMPT+SCUL`.

---

## 7b. `CADB` v1 — the CAD Bodies

Present when any body is a CAD Body (`CAD-R0-A1A2`), and then **required in
both project kinds**, on `IMPT`'s terms: a CAD Body has no other branch
describing it, and a reader that skipped this section would open the project
with objects missing. Announced by header bit3 `hasCADB`.

Section version 1. One entry per body whose representation is a CAD Body, as a
subsequence of `SCNE` in strictly ascending scene order. v1 is exactly **one
sketch and one linear extrusion** per body — a second feature kind takes a new
section version rather than a discriminator inside this one.

```
u32  bodyCount                 1 .. 4096
repeat bodyCount times, in ascending SCENE ORDER:
  u64  objectId                must be a SCNE body; entries strictly ascending
  u8   workplaneCode           1 .. 3, from the table in §4
  u32  nextEntityId            != 0, must be > every entity id below
  u32  profileEntityId         must be the anchor of one closed profile
  u8   directionCode           1 along the normal, 2 against it
  f64  depth                   metres, strictly positive, usable as a float
  u32  entityCount             1 .. 256
  repeat entityCount times, in the sketch's own order:
    u32  entityId              != 0, unique within the body, < nextEntityId
    u8   kindCode              1 .. 4, from the table in §4
    Line:       f64 startU, startV, endU, endV
    Polyline:   u8 flags (bit0 closed; all other bits zero)
                u32 vertexCount (1 .. 256; 2 .. 256 to validate, 3 .. when closed)
                f64 u, v  × vertexCount
    Rectangle:  f64 centreU, centreV, width, height
    Circle:     f64 centreU, centreV, radius
```

Per body: 8 + 1 + 4 + 4 + 1 + 8 + 4 = **30 bytes** plus its entities. A line is
37 bytes, a rectangle 37, a circle 29, and a polyline 10 + 16 × vertexCount.

Coordinates are **metres on the plane**, in the body's LOCAL space: `(u, v)`
maps to local 3D through the plane's fixed frame (§4), with the sketch origin at
the body's local origin. Nothing is recentred. A CAD Body created by extruding a
sketch starts at the identity placement, and the placement in `SCNE` is the
body's exactly as it is for a primitive — a gizmo moves it, and an edit to its
sketch or its depth leaves the placement alone.

**What is deliberately absent:** the profile's polygon, the triangulation, the
extruded vertices and indices, and which OTHER profiles the sketch happened to
close. All of it is regenerated by `generateCadMesh` on load, and a file that
carried a vertex beside the sketch would be carrying a product of the truth
beside the truth. A circle's tessellation is fixed at `kSketchCircleSegments`
(32, the same count every round primitive uses) and is not a parameter.

The record is held to the **domain's own rule**, `validateCadBodyState`, and to
nothing restated: every entity's own geometry (finite, bounded, non-degenerate),
the depth as a Construction length, the direction, AND that the sketch actually
closes the profile `profileEntityId` names — extracted by the same
`extractClosedProfiles` the product runs. A file whose sketch closes no profile,
or whose chosen profile is open, crossing, zero-area, forked or nested, is
refused rather than opened as an object with nothing to draw.

A `SCUL` entry over a `CADB` body is **refused** (`UnresolvedReference`):
`CAD-R0-A1A2` leaves CAD → Sculpt out, so such a file describes something this
build cannot evaluate. The same fail-closed shape the pre-`IMPORT-01B` reader
gave an `IMPT`+`SCUL` file. The valid combinations are therefore the four of
§7a plus `SCNE+CADB` and `SCNE+CONS+CADB` (and either with `IMPT` beside them).

## 7c. `CADB` v2 — a sketch supported by a CAD face (`CAD-A3`)

A CAD sketch may be supported not by a world plane but by a planar FACE of
another CAD body. That support is authored truth and no rule could recreate it,
so it is stored — as a **section version 2** of `CADB`, written **only** when at
least one CAD body is face-supported. A world-only CAD project still writes v1
and is byte-identical to what `CAD-R0-A1A2` wrote; the sixteen corpus fixtures
prove it. Section versions evolve independently of the file's major/minor, so
this needs no header bump; an older build refuses a required `CADB` at an
unknown version rather than opening half a body.

The v2 record is the v1 record with a support block inserted after the
workplane code:

```
u64  objectId
u8   workplaneCode             XY (1) for a face support: the canonical basis
u8   supportKind               0 world plane, 1 face          ← v2 only
  if supportKind == 1 (face):                                 ← v2 only
    u64  producerObjectId      a CADB body in this document, not this one
    u32  producerFeatureId     the producer's CAD feature (v1: always 1)
    u8   faceKind              1 CapPlane, 2 CapFar, 3 Side
    u32  faceEdgeEntityId      the profile-edge entity for a Side; 0 for a cap
    u32  faceEdgeLocalIndex    which of that entity's edges; 0 for a cap
    u64  lineageToken          the producer's topology signature at authoring
  ... then the v1 tail: nextEntityId, profileEntityId, directionCode, depth,
      entityCount and the entities, exactly as §7b.
```

The token is SEMANTIC feature lineage — never a render-triangle index, of which
no byte reaches the file. On load, after every body is decoded, the WHOLE
dependency graph is validated before anything is applied: each face-supported
body's producer must be another CAD body in the document; the producer's current
topology signature (`cadTopologySignature`) must equal the stored
`lineageToken`; the named face must resolve (`resolveCadFace`) and be eligible; a
body may not support itself; and the graph must be acyclic (a chain longer than
the body count is refused). A face-supported body's placement is **derived** and
is NOT stored — its `SCNE` placement is the unused identity, and its world model
is recomputed from the producer on every load and frame.

## 8. Validation and compatibility

Decoding happens entirely into temporary document structures. **No live project
state is touched until a complete document has passed every check below**, and a
refusal changes nothing at all — not the scene, not a Frozen Sculpt Mesh, not the
active mode or body, not the session history.

### Refused

| Cause | Status |
| --- | --- |
| Magic is not `FORGESH1` | `NotForgeFile` |
| Newer, incompatible `major` | `UnsupportedMajor` |
| Same major, a **required** section whose version is not understood | `UnsupportedSectionVersion` |
| `headerBytes` ≠ 28, unknown `projectKind`, reserved header-flag bit set, header flags that no section backs, `sectionCount` = 0 or larger than the file can hold, sections that do not exactly fill the file | `BadHeader` |
| File ends inside a header or payload; `fileBytes` ≠ the actual size; a section length past the end | `Truncated` |
| Reserved section-flag bit set, non-zero section `reserved` word | `BadSectionHeader` |
| Payload CRC mismatch | `ChecksumMismatch` |
| Unknown section with the required bit set | `UnknownRequiredSection` |
| A second `SCNE`, `CONS`, `SCUL`, `IMPT` or `CADB` — judged on the **tag**, before the version is, so a duplicate at a version the reader cannot read is still a duplicate | `DuplicateSection` |
| `SCNE` absent; `SCUL` absent for a Sculpt project | `MissingRequiredSection` |
| A payload's own structure does not add up; a sculpt, batch or polyline flags byte with a reserved bit; an imported record whose normals do not match its positions one for one, or whose batches do not tile its indices | `BadPayload` |
| A count no project can have, or one whose byte size would overflow — refused **before any allocation**; a `CADB` body with no entities or more than 256, a polyline with no vertices or more than 256 | `ImpossibleCount` |
| A value the live model refuses: a non-positive or non-finite dimension, a broken capsule relation, a non-finite position or rotation, a zero or negative scale, a duplicate or reserved `ObjectId`, an allocator that could mint a collision, an index out of range, an unknown primitive, feature, workplane, direction or entity-kind code, an imported normal that is not a unit direction, an imported name the domain's own sanitizer would not have produced, and any `CADB` record `validateCadBodyState` refuses — a bad coordinate, size, depth or entity id, or a sketch that does not close the profile the extrusion names | `InvalidSemanticValue` |
| An active body no section carries, a `CONS`/`SCUL`/`IMPT`/`CADB` body `SCNE` does not carry, a Sculpt project whose active body has no sculpt mesh, a body claimed by two of `CONS`, `IMPT` and `CADB`, a `SCUL` entry over a `CADB` body | `UnresolvedReference` |
| A load attempted while a Construction edit is open (not a property of the file) | `RefusedEditInProgress` |

Semantic values are checked by calling the **domain's own** validators —
`validateDimensionMeters`, `validateCapsuleMeters`, `validateTransformValue`,
`validateScaleValue`, `validateImportedMeshData`, `validateCadBodyState` — never
by restating the rules, so a file can never carry a value the editor would have
refused.

### Accepted

* An unknown section with the required bit **clear** is skipped after its
  validated length and its verified CRC.
* A **known** section at a version this reader does not understand is skipped on
  the same terms when its required bit is clear. So a Sculpt project whose
  retained `CONS` companion is a version newer than this build still opens, and
  still opens sculpting the right body — which is the whole point of the
  companion being optional.
* A newer `minor` of the same `major` is accepted, provided every required
  section version is understood.

The header's `hasCONS` / `hasSCUL` / `hasIMPT` / `hasCADB` flags are checked
against the section **tags the file carried**, not against the sections this
reader managed to decode. Otherwise skipping an optional section it could not
read would make the header look like a lie.

### One thing the FORMAT allows and this RUNTIME does not

A document that leaves a body with **no geometry branch at all** is a legal
`.forge` file: a Sculpt project's `CONS` is optional, and a reader that could not
understand its version is right to skip it after its validated length. This build
still refuses to LOAD one (`MissingRequiredSection`), because it cannot build a
body it has no geometry for and inventing a default Box for one whose real shape
the file described would be fabricating project data. The refusal happens in
`forgeshape_project_state.cpp`, where the reason is "this build cannot evaluate
that project", rather than in the codec, where the file itself is not at fault.

The rule is stated **once**, as `runtimeCanEvaluateProject`, because two callers
ask it: the load path, and the recovery-candidate check — which must never offer
a candidate this build could not load. A second copy of it is precisely what went
stale when a body gained a second way to have geometry.

### Deterministic writer

The same semantic document always produces byte-identical output: canonical
section order, bodies in scene order, features by ascending `LocalFeatureId`,
imported records in scene order, and every field a fixed-width little-endian
encoding. `encode → decode → encode` is byte-identical.

No compression and no encryption in v1.

---

## 9. Portability

A `.forge` file written by one supported ForgeShape installation is structurally
readable by another. What makes that true:

* Every field is a **file-owned fixed-width little-endian** encoding. Nothing is
  a C++ struct image, an enum's ABI value, a pointer, a `size_t`, a padding byte
  or a host-endian blob. The `CONS` body record is 114 bytes — a size no aligned
  C++ layout of an `ObjectId`, an enum and twelve doubles can produce.
* Nothing device-local appears: no filesystem path, no Android object id or
  type, no window or layout state, no GPU handle, no renderer cache.
* The codec and document layer compile and are exercised with no Android, JNI or
  Vulkan dependency. Where the bytes live is the Android adapter's problem
  (`ProjectSlot.java`), and the adapter never touches their meaning.
* Both supported ABIs (`arm64-v8a`, `x86_64`) build the same codec and produce
  the same bytes for the same document.

`.forge` is **not** an interchange format. It is ForgeShape's own project
document, and it is the only one ForgeShape reads back as a project. GLB is a
separate, one-way pipeline in each direction: Export writes a `.glb` view of the
geometry, and Import reads one into durable objects that then live in `.forge`
like everything else — the `.glb` itself is never a project and is never
referenced by one. OBJ and FBX are absent in both directions.

---

## 10. Migration honesty

**There has never been a production `.forge` format before v1.**

`decodeProject` is the version dispatch seam and it has exactly one branch. There
is deliberately **no v0**, and no v0→v1 migration is claimed or tested. The first
real schema bump must add a branch there, **keep the v1 fixtures below**, and
bring a real compatibility/migration test with it.

`IMPORT-01A` was not such a bump. It added a required section and a header-flag
bit within v1, and the direction of compatibility it establishes is one-way and
explicit: **a build from before it refuses a file carrying `IMPT`**, on the
header flag first and the section's required bit second, rather than opening the
project with the imported objects silently missing. In the other direction
nothing changed at all — every file written before it still decodes exactly as
it did, and the seven pre-`IMPORT-01A` fixtures' digests are unchanged, which is
what `IMP01A-19` asserts.

`IMPORT-01B` was not such a bump either, and it added no section and no flag at
all. It only widened which bodies a `SCUL` entry may sit over, from `CONS`-only
to either source. The compatibility direction is the same one-way, explicit
shape: a build from before it **refuses** an `IMPT`+`SCUL` file
(`UnresolvedReference`) rather than opening half a body, because that pairing was
previously a stated refusal rather than an unread field. In the other direction
nothing changed: all ten pre-`IMPORT-01B` fixtures' digests are unchanged, which
is what `FSR1A-12` and `IMP01A-19` assert against the two new ones `IMP01B-11`
and `IMP01B-12` add.

---

## 10a. One format, two files

Since E2E-R1B there are two app-private `.forge` files, and they hold **exactly
the same canonical v1 document** described above:

| File | Written by | Read by |
| --- | --- | --- |
| `project.forge` | an explicit Save, and nothing else | Open Saved Project |
| `recovery.forge` | autosave, and nothing else | the recovery question on a cold launch |

There is deliberately **no second format**: no delta, no journal, no append log
and no private encoding. A checkpoint is a project file that happens to have
been written automatically, which is why the ordinary fail-closed decoder reads
it and the ordinary load path applies it. A checkpoint that fails to decode is
moved to `recovery.forge.quarantine` and never offered again.

A copy written through the system document UI is the same bytes once more. The
format carries no trace of where a file came from — no path, no `Uri`, no
authority — which is what `FSR1B-13` asserts by re-encoding a project opened
from a distinctively named file and requiring the original bytes back exactly.

That applies to an imported body's SOURCE too, and it is the stronger case:
after an import, the `.glb` is not referenced by the document, not needed to open
it, and not recorded anywhere. `IMP01A-22` asserts it by carrying an imported
body through Save Copy and Open File and requiring the original bytes back.

## 11. Golden corpus

`testdata/forge/v1/`, written by `scripts/build-forge-corpus.ps1`.

That script is a **second, independent implementation** of the v1 encoder,
written from this document. The C++ codec's own self-tests can prove it decodes
what it encodes; only a separate implementation can show that this specification
is what the encoder actually implements. The two agree exactly when the digests
below match — which is verified on device by `FSR1A-12`, and printed on every
debug launch as `FORGESHAPE_PROJECT_GOLDEN_SHA256`.

| Fixture | Bytes | SHA-256 | What it is |
| --- | ---: | --- | --- |
| `construction_multibody_v1.forge` | 1264 | `8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf` | Six bodies, one per primitive kind, all six remembered parameter sets different per body, 370° and non-uniform scale |
| `sculpt_mixed_v1.forge` | 629 | `112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784` | Two bodies, both with a Construction Source, the second also carrying an edited Frozen Sculpt Mesh; reopens in Sculpt |
| `corrupt_crc_v1.forge` | 1264 | `df353c419215864d8de3aca6e2d43733c29740e2a1be4daeeb188af7c9728b96` | One bit flipped inside the `SCNE` payload; only the checksum can catch it |
| `truncated_v1.forge` | 1224 | `af80dae6c3bc9f8854a326d46090aa54f93ef59c193a22893c335135e6b04204` | The canonical Construction file minus its last 40 bytes |
| `unsupported_major_v1.forge` | 1264 | `27df45ad6bff0577514df65aa8773024203197c6323cb9c625003a67a7b6d77a` | `major = 2` |
| `unknown_optional_v1.forge` | 1296 | `85286d94592b783644d2dc00744538325f6d91557de32235a6038595d56a57ca` | An `XTRA` section with a valid length and CRC, required bit **clear** — must be skipped |
| `unknown_required_v1.forge` | 1296 | `c71553ef0d4f0be28a8972418b981b47ee0c19ae715058719ad7f0a3cd0db5cd` | The same section with the required bit **set** — must be refused |
| `imported_only_v1.forge` | 348 | `0f42be318da9faf4aa780e152b8550171085a267882d5e6e69cc9ef29a1539a8` | One Imported Mesh and **no `CONS` at all**: the fixture that proves an imported object needs no Construction Source standing in for it |
| `construction_imported_v1.forge` | 570 | `539e10e7a9e388ab1ca72867b78c1e461d3bbe0ef5876d87fa54bc6bd6ae7a51` | A Construction Body beside an imported one — a **sparse** `CONS` next to an `IMPT` |
| `mixed_imported_v1.forge` | 905 | `3fdc82a099da69b93552d7c84c56002ed8ae24ba7086ddbc6a69a9bbf671f1fb` | All three branches at once, representations interleaved rather than grouped, reopening in Sculpt on the sculpted body |
| `imported_sculpt_v1.forge` | 489 | `b82430cf6dbb82fddf075722d7ae335460f687d2a06cde09db43817323729f76` | An Imported Mesh carrying a Frozen Sculpt Mesh, with **no `CONS` at all** — the `IMPORT-01B` fixture that pins the generalized `SCUL` rule; reopens in Sculpt |
| `mixed_imported_sculpt_v1.forge` | 1266 | `ab709ecea27ec29f21b6fbef126e8cdc15dc5c733d9b751bd1c8832907f27a2b` | All **four** valid source/sculpt combinations in one document, `SCUL` entries over bodies of both source kinds; reopens in Sculpt on the sculpted imported body |

The five imported fixtures share one Imported Mesh: four vertices, two submeshes
with **different** `doubleSided` answers, named `head_low`, placed at
`(1.5, -0.25, 4.0)` with the identity rotation and scale. The two `IMPORT-01B`
fixtures share one sculpt mesh on an imported body: a four-vertex tetrahedron,
edited, and deliberately NOT the geometry it was frozen from -- a fixture where
the two matched could not tell a decoder that confused them apart. Every number
is an exact binary fraction, so the two implementations agree byte for byte or
not at all.

The seven legacy fixtures are **unchanged**, and so are the three `IMPORT-01A`
ones: the imported branch costs a project that has none exactly nothing, and
generalizing `SCUL` changed no byte of any file that already existed.

Regenerate and re-verify with:

```
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-forge-corpus.ps1 -VerifyOnly
```

Sample projects are **not** packaged into the APK. That is later roadmap work.

---

## 12. Future extension rules

* A new **optional** section may be added at any time within the same major.
  Older readers skip it after its validated length.
* A new **required** section, or a bump of an existing required section's
  version, is readable only by a reader that understands it; older readers refuse
  the file explicitly rather than loading part of it. `IMPT` is the worked
  example: it is required, it is announced by a new header-flag bit an older
  reader refuses outright, and it appears only in files that need it — so every
  file that existed before it is still byte-for-byte what it was.
* A new **representation** — a third way for a body to have geometry — takes its
  own section on `IMPT`'s terms and joins the exactly-one-of rule, rather than
  widening an existing section with a discriminator. Every body must be named by
  exactly one geometry section, and a reader that does not understand one of them
  must refuse the file rather than open it with objects missing.
* A new primitive takes the next unused **file** code and one more parameter
  block appended to the `CONS` body record, behind a `CONS` section-version bump.
* A new feature kind takes the next unused feature-kind code and appears in a
  body's feature list. `LocalFeatureId` is numbered **per body**, so a feature
  graph can grow without renumbering anything that already exists.
* Persistent topology references, when they arrive, must be expressed as feature
  lineage — never as a render-triangle index, which is derived and unstable.
* The `major` is bumped only for a change an older reader must not attempt.
