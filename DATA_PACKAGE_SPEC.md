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

## 5. `SCNE` — the project-neutral scene record

Required for **both** project kinds, required bit set. Two readable versions:
**v1**, as every build before Stage 018A wrote, and **v2**, which adds per-body
visibility, lock and name.

```
u32  bodyCount                 1 .. 4096
u64  nextObjectId              must be > every objectId below
u64  activeObjectId            must name one of the bodies below
repeat bodyCount times, in SCENE ORDER:
  u64  objectId                != 0, unique within the file
  f64  positionX, positionY, positionZ      metres
  f64  rotationX, rotationY, rotationZ      degrees, NOT canonicalized
  f64  scaleX,    scaleY,    scaleZ         unitless, strictly positive
  --- v2 only, from here ---
  u8   flags                   bit0 hidden, bit1 locked; all others RESERVED
  u16  nameBytes               0 .. 96
  u8   name[nameBytes]         UTF-8, no terminator
```

Per body: v1 is 8 + 9 × 8 = **80 bytes**; v2 adds 3 + `nameBytes`.
Payload = `4 + 8 + 8 + Σ per-body`.

### 5a. When v2 is written, and what an older reader does

v2 is written **only when at least one body needs it** — one that is hidden,
locked, or carries a name `SCNE` owns. A project of visible, unlocked, unnamed
bodies stays at **v1 and byte-identical**, which is what keeps every fixture
written before Stage 018A unchanged. This is exactly the rule `CADB` v2 and v3
follow, for exactly the same reason.

`SCNE` is a **required** section, so a build that does not know v2 refuses the
whole file (`UnsupportedSectionVersion`) rather than opening a project with
every body visible and unlocked when the user hid or locked some. Fail-closed is
the right side to err on: silently ignoring a lock is worse than declining.

A **v1 file loads as the default a body has always had** — visible, unlocked,
and named only where its own section already named it. That is the live model's
own member initializers, not a migration step, and nothing is rewritten.

### 5b. The name has ONE owner per representation

`IMPT` has carried an imported object's name since `IMPORT-01A` — it came from
the file the geometry came from — and Rename writes that same field rather than
a second one. So an imported body's `SCNE` name is required to be **empty**, and
a file that states both is refused (`InvalidSemanticValue`): two answers to what
one body is called is a choice the next writer would have to make silently. A
Construction Body and a CAD Body had nowhere to store a name, so `SCNE` v2 is
where theirs lives.

A non-empty `SCNE` name is held to the domain's own storability rule — the same
`sanitizeImportedMeshName` idempotence check `IMPT`'s name is held to — so a
file cannot carry a name Rename could not have produced.

### 5c. Reserved flag bits are refused, never masked

Only bits 0 and 1 are defined. A set reserved bit is refused as `BadPayload`
rather than masked off, because a future flag this build cannot honour must not
be silently dropped — that would open a project in a state its writer did not
mean. `object_state_bad_flags_v2.forge` is the fixture that pins this.

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
body's producer must be another CAD body in the document
(`UnresolvedReference` otherwise); the producer's current topology signature
must equal the stored `lineageToken` and the named face must resolve and be
eligible (`InvalidSemanticValue` otherwise — a circle's cylindrical side is
never eligible); a body may not support itself; and the graph must be acyclic
(a chain that revisits a body, or is longer than the body count, is
`UnresolvedReference`). A face-supported body's placement is **derived** and
is NOT stored — its `SCNE` placement is the unused identity, and its world model
is recomputed from the producer on every load and frame.

### The lineage token is a format field

`lineageToken` is the producer's **topology signature**, and because a reader
compares it against a value it computes itself, the rule is part of this
format and is restated here so a second implementation can produce it:

```
faces  = [CapPlane, CapFar, Side(edge 0), Side(edge 1), …, Side(edge n-1)]
           for the extruded profile's n polygon edges, in profile order
code   = (kind << 56) | (edgeEntityId << 16) | (edgeLocalIndex & 0xFFFF)
           kind: CapPlane 0, CapFar 1, Side 2   (the DOMAIN enumeration,
           not the file's 1/2/3); edgeEntityId and edgeLocalIndex are 0 for a cap
h      = 0xCBF29CE484222325                     (FNV-1a 64-bit offset basis)
mix(v) = for each of v's 8 bytes, least significant first:
           h = (h XOR byte) × 0x100000001B3  (mod 2^64)
mix(profileEntityId); mix(faces.count)
for each face: mix(code); mix(eligible ? 1 : 0)
if h == 0 then h = 1
```

Eligibility is 1 for both caps and for every planar side, and 0 for a circle's
cylindrical sides (all `kSketchCircleSegments` of them). For a one-rectangle
profile the profile polygon runs counter-clockwise from the lower-left corner,
so its four sides carry `edgeEntityId = ` the rectangle's entity id and
`edgeLocalIndex = 0..3` in that order; a rectangle producer therefore hashes six
faces, a circle producer thirty-four. Sizes, depth and direction take no part,
which is what lets a supported parent edit keep a dependent attached.
`scripts/build-forge-corpus.ps1` reimplements exactly this in
`Get-CadTopologySignature`, from this text and not from the C++.

## 7d. `CADB` v3 — the curve entities (`SKETCH-UX-R1`)

A sketch may carry an **Arc** and a **Spline** beside its lines, polylines,
rectangles and circles. Both are authored truth that no rule could recreate from
anything else in the file, so both are stored — as a **section version 3** of
`CADB`, written **only** when at least one CAD body's sketch actually carries
one. A project drawn entirely from the four v1 entity kinds still writes v1 (or
v2, if it is face-supported) and is **byte-identical** to what the earlier stage
wrote; the twenty-two fixtures that predate this stage prove it. An older build
refuses a required `CADB` at an unknown version rather than opening a body with
a curve silently missing — or, far worse, replaced by the straight edge between
its two ends.

v3 is v2's record with two entity kinds added to the table. **v3 always writes
the v2 support block**, whether or not any body is face-supported, because a
version is a superset of the one below it and a reader that has to guess which
optional blocks a version carries is not reading a format.

```
kindCode 5 — Arc:     f64 startU, startV, midU, midV, endU, endV
kindCode 6 — Spline:  u32 pointCount (2 .. 32)
                      f64 u, v  × pointCount
```

An Arc is 4 + 1 + 48 = **53 bytes**; a Spline is 4 + 1 + 4 + 16 × pointCount.

**An Arc is three points on the curve** — where it starts, a point it passes
through, and where it ends — and not a centre, a radius and two angles. Three
points have no ambiguity to resolve: exactly one circle passes through three
non-collinear points, and exactly one of its two arcs contains the middle point.
A centre/angle form would have to store a sweep direction and a major/minor flag
beside the endpoints and keep all of them consistent with each other. The
centre, the radius and the swept angle are **derived** (`arcGeometry`) and no
byte of them reaches the file.

**A Spline is its authored points**, which the curve interpolates: a
Catmull-Rom through the points, converted span by span to a cubic Bezier, so the
curve passes exactly through every stored point and moving one moves the curve
there. Control handles, knots and the tessellation are all derived.

Neither carries a tessellated point. Both are turned into a polyline by
`tessellateSketchCurve`, deterministically and from the authored values alone —
an arc at the density a full circle gets (`kSketchCircleSegments` over 2π,
clamped to 4..64 segments), a spline at 8 segments per authored span — with the
two ENDS written as the authored points themselves rather than re-evaluated, so
a chain closes on the numbers the snap produced. Nothing about the tessellation
depends on a camera, a zoom or a window, because the extruded solid must not.

The same domain rule decides the record's validity, and nothing is restated: an
arc whose three points are collinear or coincident is refused (`InvalidArc`,
surfacing as `InvalidSemanticValue`), as is a spline with fewer than two points,
two coincident consecutive points, or two ends that meet — a chainable entity
whose ends coincide is a loop the single chain walker cannot read, and it is
refused rather than half-supported. A `pointCount` outside 2..32 is
`ImpossibleCount`, refused before a byte is allocated for it.

**A curve kind inside a section that declared itself v1 or v2 is refused**
(`InvalidSemanticValue`). The version says what the payload may contain, and a
payload contradicting its own version is malformed rather than newer.

For the topology signature of §7c, a curve's derived polyline edges are **not
eligible** side faces — each is a facet approximating a curved surface, the same
answer a circle's cylindrical side already gets — while a straight line's side
in the same profile stays eligible. Eligibility is decided per polygon edge for
exactly this reason: one profile may mix both.

## 7e. `CADB` v4 — the extrusion's extent (`CAD-EXT-R1`)

An extrusion reaches a stated distance on each side of its sketch plane. The
durable truth is **two non-negative distances** — one along `+N`, one along
`-N` — of which at least one is positive, plus the **mode** naming which
combinations the controls author. That is stored as a **section version 4** of
`CADB`, written **only** when at least one CAD body's extent is not One Side.

A One Side extrusion **is** a direction and a positive depth, which every
version since v1 has carried, so a project whose every extrusion is One Side
still writes v1, v2 or v3 and is **byte-identical** to what the earlier stage
wrote; the twenty-eight `CADB` fixtures that predate this stage prove it. An
older build refuses a required `CADB` at an unknown version rather than opening
a body with half its extent silently missing — which for a Symmetric body would
be a solid of the wrong size and in the wrong place.

v4 is v3's record with two fields. **v4 always writes the v2 support block and
understands the v3 entity kinds**, because a version is a superset of the one
below it.

```
u64  objectId
u8   workplaneCode
u8   supportKind                (+ the TopoRef for a face support)   ← v2
u32  nextEntityId
u32  profileEntityId
u8   extentCode                 1 One Side, 2 Symmetric, 3 Two Sides ← v4 only
u8   directionCode              1 along the normal, 2 against it
f64  depth                      the PRIMARY distance, metres
f64  secondDistance             the -N distance, metres              ← v4 only
u32  entityCount  … then the entities, exactly as §7b and §7d.
```

The **extent code comes BEFORE the direction it qualifies**: in every mode but
One Side the direction carries no information, and a reader that met it first
would have to read backwards to learn that. The **second distance is written
always at v4**, exactly `0.0` outside Two Sides, so a v4 body record is one
fixed size and a decoder can refuse a non-zero value where the mode has no
second side rather than quietly ignoring it.

What `depth` means, per mode — and it is a **distance and never a signed
offset**, because a side and a length are two different facts:

| extentCode | `+N` distance | `-N` distance | `directionCode` | `secondDistance` |
| --- | --- | --- | --- | --- |
| 1 One Side | `depth` if along, else 0 | `depth` if against, else 0 | 1 or 2 | exactly `0.0` |
| 2 Symmetric | `depth` (per side) | `depth` (per side) | exactly 1 | exactly `0.0` |
| 3 Two Sides | `depth` (A) | `secondDistance` (B) | exactly 1 | B |

A Symmetric body stores the distance **per side** and never a total thickness,
so the file does not depend on a presentation preference the UI might later
change its mind about.

**One solid has exactly one encoding**, and the decoder enforces it: a
`directionCode` of 2 under Symmetric or Two Sides, and a non-zero
`secondDistance` outside Two Sides, are **refused** (`InvalidSemanticValue`) —
never masked and never repaired, on the reserved-flag-bit rule of §5c applied to
a pair of fields. An `extentCode` outside 1..3 is refused the same way. The
distances are then held to the **domain's own rule**, `validateCadBodyState`,
and to nothing restated: each side finite, non-negative and within the sketch
bound; a side may be zero **only** in Two Sides, where the other side carries
the extent; and the total span `A + B` is always a usable Construction length.
A file whose extrusion reaches nowhere at all is refused rather than opened as a
body with no volume.

The mesh spans exactly `-B .. +A` along the plane normal, which reduces to
`0 .. +depth` and `-depth .. 0` for the two One Side cases — the offsets
`generateCadMesh` always produced. **`CapPlane` is the cap the extrusion grows
FROM and `CapFar` the one it grows TO.** For One Side that is still literally
the cap lying ON the sketch plane; for Symmetric and Two Sides neither cap is on
the plane and the start cap is the `-N` one. The face TOKENS are unchanged by
any of this, so the §7c lineage signature is unchanged too, and a
face-supported dependent stays attached across an extent edit — while its
derived world placement follows the cap that moved, which is correct.

## 7f. `CADB` v5 — regions with holes and the retained feature chain (`CAD-VERTICAL-SLICE-R1`)

Two authored facts arrive together, and no rule could recreate either, so both
are stored — as a **section version 5** of `CADB`, written **only** when at least
one CAD body needs it:

* **a region selection that is not the R0 profile.** A sketch's closed loops
  enclose REGIONS: every loop's interior minus the interiors of the loops it
  directly and cleanly contains (its holes). A rectangle around a circle
  therefore offers two regions — the disk, and the rectangle minus the disk —
  and the extrusion stores WHICH it chose, by semantic identity: the anchor of
  each chosen region's OUTER loop and the anchors of that region's HOLES. A
  selection of exactly one region without holes is the R0 profile, carried by
  `profileEntityId` since v1, and needs nothing new.
* **a retained feature chain.** After its first (New Body) extrusion a CAD body
  may carry up to fifteen LATER features, each a sketch standing on a planar
  face of an EARLIER feature of the SAME body, extruded, and applied as an
  **Add** (union) or a **Cut** (difference). A later feature never creates a
  body; New Body is the act that does, and it is a separate `SCNE` body.

A project whose every CAD body selects one region without holes and carries no
later feature still writes v1, v2, v3 or v4 exactly as before and is
**byte-identical** to what the earlier stages wrote; the thirty-six fixtures
that predate this stage prove it. An older build refuses a required `CADB` at an
unknown version rather than opening a body with its holes silently filled or its
Add and Cut features silently missing.

v5 is v4's record with a **tail** after the first feature's entities. **v5
always writes the v2 support block and the v4 extent fields and understands the
v3 entity kinds**, because a version is a superset of the one below it.

```
u64  objectId
u8   workplaneCode
u8   supportKind  (+ the TopoRef for a face support)     ← v2
u32  nextEntityId
u32  profileEntityId           the FIRST chosen region's outer-loop anchor
u8   extentCode                                           ← v4
u8   directionCode
f64  depth
f64  secondDistance                                       ← v4
u32  entityCount … then the entities, exactly as §7b and §7d
— the v5 tail —
REGIONS   the first feature's region selection (below)
u32  laterFeatureCount         0 .. 15
repeat laterFeatureCount times, in chain (application) order:
  u32  featureId               strictly ascending, every one > 1
  u8   operationCode           2 Add, 3 Cut   (1 New Body is refused here)
  u32  supportFeatureId        an EARLIER feature of this body: 1, or a
                               smaller later featureId
  u8   faceKind                1 CapPlane, 2 CapFar, 3 Side
  u32  faceEdgeEntityId        0 for a cap
  u32  faceEdgeLocalIndex      0 for a cap
  u64  lineageToken            the supporting feature's signature (below)
  u32  nextEntityId
  u32  profileEntityId
  u8   extentCode
  u8   directionCode
  f64  depth
  f64  secondDistance
  u32  entityCount … then the entities, exactly as §7b and §7d
  REGIONS                       this feature's region selection
```

and `REGIONS` is

```
u32  holeCount                 holes of the FIRST chosen region, 0 .. 64
u32  holeAnchorId × holeCount  strictly ascending
u32  additionalRegionCount     further chosen regions, 0 .. 15
repeat additionalRegionCount times:
  u32  outerAnchorId           strictly ascending, every one > profileEntityId
  u32  holeCount               0 .. 64
  u32  holeAnchorId × holeCount   strictly ascending
```

A later feature's sketch is authored on its own canonical local XY — it has no
workplane code and no support block of its own — and is placed in the body by
its support face's frame alone: `(u, v)` on the sketch at offset `w` along the
normal is `origin + U·u + V·v + N·w` of the frame the named face of the named
earlier feature has in the body's local space (the same frame a face-supported
body of §7c is placed by, but composed inside ONE body rather than across two).
A world placement is never involved, so a later feature moves with its body and
with the face it stands on.

The codes are file-owned:

| CAD feature operation | code |
| --- | ---: |
| New Body (the first feature only; never stored — the first feature IS the body) | 1 |
| Add (union) | 2 |
| Cut (difference) | 3 |

Per later feature the fixed record is 4 + 1 + 4 + 1 + 4 + 4 + 8 + 4 + 4 + 1 +
1 + 8 + 8 + 4 = **56 bytes**, plus its entities and its `REGIONS` block of
8 + 4 × (holes) + Σ(8 + 4 × holes) bytes. A v5 body with no later feature and a
one-region selection costs 12 bytes more than its v4 record.

### The canonical form, and what is refused

One solid has exactly one encoding, and the decoder enforces it: anchors and
feature ids strictly ascending as stated, `operationCode` 2 or 3,
`supportFeatureId` an earlier feature of the same body, every count within its
bound — `ImpossibleCount` for a count out of range, **before** anything is
allocated for it; `InvalidSemanticValue` for an unknown operation or face code,
for ids out of order, for a support that names no earlier feature, for a stored
hole set that is not EXACTLY the one the sketch derives for that outer loop, for
two chosen regions whose loops touch or cross or one of which stands inside the
other's material without being its own direct hole, and for anything the
domain's own `validateCadBodyState` refuses. A body carrying later features is
then **regenerated** through the boolean kernel as part of the check — an Add
whose tool does not touch the body (`AddDisjoint`), an Add that adds nothing, a
Cut that removes nothing (`CutNoIntersection`) or everything (`CutRemovesBody`),
a support face an earlier Cut carved away (`SupportFaceLost`) — so a file is
refused rather than opened as a body the editor could never have produced.
Nothing about the kernel's result is stored: no vertex, no triangle, no tag.

### Regions, stated so a second implementation derives the same ones

The loops are exactly what `extractClosedProfiles` reads (§7b, §7d), and loops no
longer refuse each other for nesting. Loop B is CLEANLY INSIDE loop A when no
edge of one touches or crosses an edge of the other and B's first vertex lies
strictly inside A by the even-odd rule. B's PARENT is the smallest-area loop B is
cleanly inside (ties by the smaller anchor). The region of loop L is L minus its
children. A loop that touches or crosses another is never anyone's hole — each
such loop stays its own region, exactly as every earlier version read it — and a
region whose holes touch or cross each other cannot be selected. There is no
planar arrangement: loops never split each other.

**A selection means the UNION of the regions it lists** (`CAD-FOUNDATION-C1`).
The stored list is unchanged — the chosen atomic regions, each with the holes
it was chosen with — and a region may now be listed beside its own direct hole
(a ring and its disk). What is extruded is derived by one parity rule over the
parent tree: loop L bounds the union exactly when L's region is chosen and its
parent's region is not (or L has no parent) — L is then a component's OUTER
loop — or when L's region is not chosen and its parent's region is, walking down
from such an outer loop through chosen regions — L is then one of that
component's HOLES. Components are taken in ascending outer anchor, holes in
ascending anchor. A list that chooses no region beside its own hole (every list
an earlier build could write) unions to exactly its own regions, in the same
order, with the same holes, so no stored byte, token or fixture changes. No
section version changed either: an earlier build reading a list with a region
beside its own hole refuses it by name (`InvalidSemanticValue` through
`OverlappingRegions`), which is fail-closed.

### The lineage token, generalized

`lineageToken` is §7c's signature of the SUPPORTING feature's own face topology,
with the face list generalized to the union components of the selection (for a
list with no region beside its own hole these are exactly the chosen regions):

```
faces = [CapPlane, CapFar]
        then for each union component in ascending outer-anchor order:
          one Side per edge of its OUTER loop, in the loop's polygon order
          then for each hole in ascending anchor order:
            one Side per edge of the HOLE loop, in that loop's polygon order
mix(profileEntityId); mix(faces.count)
for each face: mix(code); mix(eligible ? 1 : 0)
```

with `code` and `mix` exactly as §7c. A loop's polygon order is the
counter-clockwise order the extraction produces: a rectangle from its
`(-w/2, -h/2)` corner (edges 0..3), a circle from `+U` (32 curved edges), and
a polyline or chain in its stored order when that order is counter-clockwise;
when it is clockwise the vertex sequence is reversed and edge `j` of the result
is stored edge `(n − 2 − j) mod n` of the `n` edges. Eligibility is 0 for a
curved side (a circle's, an arc's or a spline's) and 0 for EVERY face of a Cut
feature — a Cut leaves its faces behind as the inside of a pocket, facing the
other way, and R1 does not let a sketch stand there — and 1 otherwise. For a
first feature that selects one region without holes this is §7c exactly, so no
stored token of any earlier fixture changes.

A `TopoRef` of §7c may now name a LATER feature of its producer
(`producerFeatureId` > 1): the dependent then stands on that feature's face, at
that feature's signature, and — because a Cut can carve a face away entirely —
the face must still carry material in the producer's regenerated body.

## 7g. `CADB` v6 — the retained sketch table and the selection kind (`CAD-V6-S1`)

ONE version for two facts that both change what a feature's INPUT is, so there
is one migration and not two:

* **sketch identity.** A body carries a TABLE of retained sketches, each with a
  stable, body-local, non-zero `sketchId`, and every feature — the first one
  included — names the sketch it extrudes BY ID instead of carrying one inline.
  Two features may therefore extrude ONE sketch, and a sketch may be retained
  with no feature extruding it. Where a sketch stands belongs to the sketch,
  because a sketch two features share stands in one place.
* **the selection kind.** Every feature states explicitly WHAT it selects:
  `LoopRegions` (the §7f `REGIONS` block, unchanged) or `PlanarFaces` — atomic
  faces of the sketch's planar arrangement, named by canonical `PlanarFaceRef`
  (`forgeshape_sketch_arrangement.h`, `CAD-PLANAR-FACE-PF-S1`).

Plus the two id high-water marks, `nextSketchId` and `nextFeatureId`, so an id
a deleted feature or sketch wore is never minted again (the feature-id reuse
the CAD architecture audit flagged). A file states the marks of the state it
was written from and nothing else: they describe the forward history branch
that reached that state (`ARCHITECTURE.md`, id lifetime), and a reopened
project continues from them. No history, redo branch or runtime floor is
stored.

### When v6 is written, and what an older reader does

v6 is written **only** when a body says something v1..v5 cannot: a sketch two
features name, a sketch no feature names, sketch ids other than the ones a
legacy read synthesizes (below), a high-water mark other than the one it
derives, or a `PlanarFaces` selection. Every other project writes v1..v5 exactly
as before and is **byte-identical**; the forty-four fixtures that predate this
stage prove it. An older build refuses v6 as a required section at an unknown
version rather than opening a body with a shared sketch silently duplicated or a
face selection silently read as a loop selection.

### Layout

v6 is its own body record — the sketch table REPLACES the inline sketches, so it
is not a tail on v5. It understands every entity kind (§7b, §7d), carries the
§7c `TopoRef` and the §7e extent, and holds every feature (the first one
included) in one list.

```
u32  bodyCount                   1 .. 4096
repeat bodyCount times, in ascending SCENE ORDER:
  u64  objectId
  u32  nextSketchId              > every sketchId below
  u32  nextFeatureId             > every featureId below
  u32  sketchCount               1 .. 16
  repeat sketchCount times, strictly ascending sketchId:
    u32  sketchId                != 0
    u8   placementCode           1 workplane, 2 face of another CAD body,
                                 3 face of an earlier feature of this body
      1:  u8   workplaneCode     §4
      2:  u64  producerObjectId  the §7c TopoRef; the sketch is on local XY
          u32  producerFeatureId
          u8   faceKind          1 CapPlane, 2 CapFar, 3 Side
          u32  faceEdgeEntityId
          u32  faceEdgeLocalIndex
          u64  lineageToken
      3:  u32  supportFeatureId  the §7f support; the sketch is on local XY
          u8   faceKind
          u32  faceEdgeEntityId
          u32  faceEdgeLocalIndex
          u64  lineageToken
    u32  nextEntityId
    u32  entityCount … then the entities, exactly as §7b and §7d (codes 1..6)
  u32  featureCount              1 .. 16
  repeat featureCount times, in chain (application) order:
    u32  featureId               the first exactly 1; strictly ascending
    u8   operationCode           the first 1 New Body; every later one 2 Add or 3 Cut
    u32  sketchId                a sketch in this body's table
    u8   extentCode              §7e
    u8   directionCode
    f64  depth
    f64  secondDistance
    u8   selectionKind           1 LoopRegions, 2 PlanarFaces
      1:  u32  profileEntityId, then REGIONS (§7f)
      2:  FACES
```

and

```
FACES     u32 faceCount 1 .. 16
          repeat faceCount: CYCLE outer, u32 holeCount 0 .. 64, CYCLE × holeCount
CYCLE     u32 fragmentCount 1 .. 1024, then FRAGMENT × fragmentCount
FRAGMENT  u32 sourceEntityId          != 0
          u32 sourceEdgeLocalIndex    rectangle side 0..3, polyline segment k, else 0
          CUT start
          CUT end
          u8  reversed                0 or 1; every other bit reserved
CUT       u8  cutKind                 1 SourceStart, 2 Intersection, 3 SourceEnd
          cutKind 2 only:
          u32 partnerEntityId         != 0
          u32 partnerEdgeLocalIndex
          u32 ordinal                 this contact's place among this edge's
                                      contacts with that partner edge, in this
                                      edge's own parameter order
```

A body record is 20 bytes, then per sketch 14 bytes (placement 1), 42 (placement
2) or 34 (placement 3) plus its entities, then 4 bytes and per feature 28 bytes
plus its selection: 4 + `REGIONS` for loop regions, and for faces 4 + per face
4 + its cycles, per cycle 4 + per fragment 9 + its two cuts (1 byte for a source
end, 13 for an intersection). Nothing in a face selection is a coordinate, a
floating-point value, a vector index, a tessellation index or a triangle.

The fixed caps, and why: sketches `kMaxCadSketches` = the feature cap (16);
faces per selection `kMaxPlanarFaceSelection` = the region cap (16); holes per
face `kMaxPlanarFaceHoles` = `kMaxRegionHoles` (64); fragments per cycle
`kMaxPlanarFaceCycleFragments` = `kMaxProfileVertices` (1024), because every
fragment becomes at least one polygon vertex when a face is extruded, so a longer
cycle could never be regenerated.

### Reading v1..v5 into the table (the migration)

The file keeps its version; nothing is rewritten because it was read. A v1..v5
body becomes, in memory: the first feature's inline sketch as sketch **1** (its
placement the v1 workplane or the v2 `TopoRef`), each later feature's inline
sketch as sketch **k + 1** in chain order (placement 3, its v5 support), every
selection `LoopRegions`, `nextSketchId` = the sketch count + 1 and
`nextFeatureId` = the last feature id + 1 (2 for a single feature). Feature ids
and every support are kept as stored. Two inline sketches that happen to be
byte-identical stay TWO sketches: the old format could not say they were one.
Writing that state back produces the same v1..v5 bytes, because it is exactly
the shape v6 is not required for.

### The canonical form, and what is refused

Structure first, by the codec, before any allocation: a count out of range is
`ImpossibleCount`; an unknown placement, face, operation, extent, direction,
selection or cut code, a first feature whose id is not 1 or whose operation is
not New Body, and a later New Body are `InvalidSemanticValue`; a `reversed`
byte other than 0 or 1 is `BadPayload` (a reserved bit, never masked).

Everything else is the domain's `validateCadBodyState`, surfaced as
`InvalidSemanticValue` and named in the domain (`CadStatus`):

| Refused | `CadStatus` |
| --- | --- |
| a `sketchId` of 0, or sketches not strictly ascending | `SketchIdInvalid` |
| two sketches with one id | `DuplicateSketchId` |
| a feature naming an id the table lacks | `SketchNotFound` |
| `nextSketchId` or `nextFeatureId` not above every id | `HighWaterInvalid` |
| more than 16 sketches | `TooManySketches` |
| the first feature on a placement-3 sketch; a second placement-1/2 sketch; a later feature on a placement-1/2 sketch other than the first feature's | `SketchSupportInvalid` |
| a placement-3 sketch whose support names no EARLIER feature (for a sketch no feature extrudes: no feature of the body), a stale lineage, a missing or ineligible face | `FeatureSupportInvalid` (as §7f) |
| a selection carrying the other kind's payload | `InvalidSelectionKind` |
| an empty or over-long cycle, too many holes, a start cut of kind 3 or an end cut of kind 1, a source-end cut carrying a partner, an intersection naming partner 0, `sourceEntityId` 0 | `PlanarFaceRefMalformed` |
| a cycle not rotated to its smallest fragment, a fragment twice in a cycle, holes or faces not strictly ascending | `PlanarFaceRefNotCanonical` |
| one face twice in a selection | `DuplicatePlanarFace` |
| a face the sketch's arrangement does not derive | `PlanarFaceUnresolved` |
| a face selection over a sketch holding a Spline | `PlanarFaceUnsupportedCurve` |
| over a sketch whose curves share a stretch | `PlanarFaceAmbiguousOverlap` |
| over a sketch past the arrangement's caps | `PlanarFaceCapExceeded` |
| an arrangement cycle below the area floor | `PlanarFaceDegenerate` |
| a sketch placed on a face of a `PlanarFaces` feature | `PlanarFaceRegenerationUnavailable` |

The ORDER of fragments, holes and faces is the canonical order the arrangement
produces, stated as a total order on the tuples: a cut by (kind code − 1,
partner entity, partner edge, ordinal); a fragment by (source entity, source
edge, start cut, end cut, reversed); a cycle lexicographically by its fragments
and then by length; a face by its outer cycle, then its holes in order, then the
hole count. A cycle is rotated to start at its unique smallest fragment; the
outer cycle runs counter-clockwise and every hole clockwise. A reader checks the
rotation and the orders and refuses — it never re-sorts — and leaves the
orientation to exact resolution, because only the arrangement knows it.

**Resolution is exact.** A `PlanarFaceRef` resolves only if it EQUALS, tuple for
tuple, one face the named sketch's arrangement derives (`resolvePlanarFaceRef`).
There is no nearest-face search, no seed point and no fallback: an edit that
adds or removes an intersection changes the face and the stored ref no longer
resolves. The arrangement itself — analytic intersections of lines, polyline
segments, rectangle sides, circles and arcs in binary64 sketch coordinates,
T-junctions and endpoint coincidences within `kSketchCoincidenceMeters`, cuts
named by partner and ordinal, the half-edge walk and the bounded faces — is the
one `CAD-PLANAR-FACE-PF-S1` defines (`artifacts/cad-planar-face-pf-s1/SUMMARY.md`);
a Spline is never intersected, so a Spline anywhere in the sketch refuses a face
selection over it.

### One thing this FORMAT allows and this RUNTIME does not (yet)

A body with a `PlanarFaces` selection is a valid document: it decodes, every
face resolves, and it round-trips byte for byte. This build does not yet
regenerate a solid from planar faces (`PlanarFaceRegenerationUnavailable`), so it
does not run the kernel check of §7f on such a body, and the runtime REFUSES TO
LOAD a project containing one (`runtimeCanEvaluateProject` false, reported as
`MissingRequiredSection`) — the recovery check never offers one either. That is
the §8 rule for a body this build cannot evaluate, applied once more; it is lifted
when regeneration from faces lands. A `LoopRegions` v6 body — a shared or a
retained sketch — loads and regenerates normally.

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
| A payload's own structure does not add up; a sculpt, batch, polyline or SCNE v2 body flags byte with a reserved bit; an imported record whose normals do not match its positions one for one, or whose batches do not tile its indices | `BadPayload` |
| A count no project can have, or one whose byte size would overflow — refused **before any allocation**; a `CADB` body with no entities or more than 256, a polyline with no vertices or more than 256 | `ImpossibleCount` |
| A value the live model refuses: a non-positive or non-finite dimension, a broken capsule relation, a non-finite position or rotation, a zero or negative scale, a duplicate or reserved `ObjectId`, an allocator that could mint a collision, an index out of range, an unknown primitive, feature, workplane, direction or entity-kind code, an imported normal that is not a unit direction, an imported name the domain's own sanitizer would not have produced, and any `CADB` record `validateCadBodyState` refuses — a bad coordinate, size, depth or entity id, a sketch that does not close the profile the extrusion names, and every `CADB` v6 refusal §7g names | `InvalidSemanticValue` |
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

`CADB` v6 (`CAD-V6-S1`) is the first section version whose reader MIGRATES in
memory: a v1..v5 body's inline sketches become a sketch table (§7g), one sketch
per feature, ids 1..n, high-water marks derived. The migration is real and
tested — `CADV6_P02..P04` read a v5 chain into the table, prove two
byte-identical legacy sketches stay two, and prove the migrated state writes
the same v5 bytes back — and every file written before it decodes to exactly
the body it always meant.

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
| `cad_rectangle_v1.forge` | 247 | `e2fd79c4070d1168ae883e064200438244c09a41bcf1682f9a0596b549d1b26b` | One CAD Body — a rectangle on XZ extruded along +Y — at a 370° / non-uniform-scale placement, with **no `CONS` at all** |
| `cad_circle_v1.forge` | 239 | `886b1538a1a20113316b7badcc7c6aaca6b17ccb54d3dcfed042ff41866e4559` | One CAD Body: a circle on YZ extruded AGAINST its normal |
| `mixed_cad_v1.forge` | 780 | `94f014cdc01fe8beaa14301ef2a99c0805a7e13afe1bca0126b29026882c93db` | A Construction Body beside two CAD Bodies — a closed polyline with an unrelated open line, and a chained-line loop — a **sparse** `CONS` next to a `CADB` carrying every entity kind |
| `cad_bad_plane_v1.forge` | 247 | `60476603b6c1b1e1cf6b785e863c953ee6aa63573c1a2453bd7db0935909aba9` | The rectangle fixture with workplane code 9 and the `CADB` CRC recomputed; only the semantic check can refuse it |
| `cad_face_sketch_cap_v2.forge` | 425 | `4e4bdacc20954b063ef38d81a0c2c1c4bb4faff97255aea1e11d161bec560a86` | **`CADB` v2.** A translated rectangle producer and a dependent rectangle supported by its far cap (`CapFar`); the dependent's `SCNE` placement is the unused identity |
| `cad_face_sketch_side_v2.forge` | 425 | `9afa0ae2036cc1c03ee2e49f42fe45c19e79a8f050c9c87a2202205e10ed5a3d` | A dependent supported by the producer's **side** face (edge entity 1, local index 1), extruded against the face normal |
| `cad_face_chain_v2.forge` | 594 | `24ad47b5b5d600a47860ccafc3a644fa22e3d0bde994c43d969567cb299580b7` | A → B → C: a world body, a dependent on its far cap, and a circle dependent on that one's far cap — a chain a reader must resolve hop by hop |
| `mixed_cad_face_v2.forge` | 923 | `4b20f3c8ea05876850043dff28591f19855efa4c0566bebd43df11f1c0ff1529` | A Construction Box, an Imported Mesh, a CAD producer and a face-supported dependent: `SCNE`, `CONS`, `IMPT` and a v2 `CADB` side by side |
| `cad_bad_face_ref_v2.forge` | 425 | `2f728f27393ff19b9dfa327be8b6598f26eb595dfe9a8c399a0db5e8cd50f564` | The side fixture naming side **7** of a rectangle that has sides 0..3; producer present, lineage right, CRC right — refused `InvalidSemanticValue` by face resolution alone |
| `cad_dependency_cycle_v2.forge` | 594 | `88072d355efa54f95e6d68c10d15c81a9cfebc0e2ed1f231255ebe5a42b117f0` | The chain with B on C's far cap and C on B's, B's lineage set to C's own signature — refused `UnresolvedReference` by the cycle alone |
| `cad_arc_profile_v3.forge` | 301 | `580a47dcfa305ddec34c2ed7831ba88b7a05659008e7944285ca2a70fc02b8cd` | **`CADB` v3.** One body whose profile is a semicircular Arc closed by a Line — the smallest v3 file there is, and the one that pins the three-point arc encoding |
| `cad_spline_profile_v3.forge` | 321 | `e3ff4f7be2529a30f040bd3d699b46df9792473b88755494144c15697b19261a` | One body whose profile is a four-point Spline closed by a Line; the authored points and nothing derived |
| `cad_mixed_curve_profile_v3.forge` | 892 | `3e2fa16f05e353f1745a36e165aedadbb5d5378db0c039167293aececad7078d` | An Arc body, a Spline body and a Rectangle body in one v3 `CADB`, with a **sparse** `CONS` beside them |
| `cad_face_curve_v3.forge` | 478 | `0a8218f0aa86cfb7cdcf7781c72864e76e9065e2f7a788d5b2cf4e6fc1250ad0` | A curve profile supported by a producer's far cap: v3 carrying a v2 support block, which is what makes a version a superset rather than a variant |
| `cad_bad_arc_v3.forge` | 301 | `628d74fdbf3cf9082ef4869f9b948acef81c6a14517703b707896602e7c4b418` | The arc fixture with its three points made **collinear**; no circle passes through them, and only the semantic check can refuse it |
| `cad_bad_spline_v3.forge` | 321 | `f5437366d894032e97b2e49d3027726fa2bc739bda747f05f3b1eaf8c9ed714d` | The spline fixture whose two **ends coincide** — a loop the one chain walker cannot read; every length, count and CRC is correct |
| `cad_symmetric_v4.forge` | 257 | `6674e7225933ee2195292b6e43a091d3e327b7189ebd35fa1519191ecf92ab5a` | **`CADB` v4.** One rectangle body reaching 0.75 m each side of its XY sketch plane — the smallest v4 file there is |
| `cad_two_sides_v4.forge` | 249 | `8c49e09cca8133b81ef9bbfab75549ed111dc08b4057777ed077191cf25cab47` | One circle body on XZ with two **unequal** distances: 1.25 m along the normal (A) and 0.5 m against it (B) |
| `cad_face_extent_v4.forge` | 629 | `73739a226a30f8d015fe19663cd06062eeab7dab541f90795282ab5660510673` | A One Side producer with a Symmetric dependent on its far cap and a Two Sides dependent on one of its sides: v4 carrying a v2 support block |
| `mixed_cad_extent_v4.forge` | 785 | `32ee99fccc5be71ed15003dc5b6856f7ed9c57b764c1032394b2c0b2f814f076` | A One Side body beside a Symmetric and a Two Sides one, on the three world planes, with a **sparse** `CONS` beside them — the One Side pair carried unchanged inside v4 |
| `cad_bad_extent_v4.forge` | 257 | `15f1cecae31287643467d4b127e92918aca9677c5a1016b1def41a30d493b995` | The symmetric fixture with `extentCode` **9**; every length, count and CRC is correct — refused `InvalidSemanticValue` by the extent code alone |
| `cad_bad_two_sides_v4.forge` | 249 | `aeb1b6784c546dde05a8e3227227aa19f2b396bee5d0f40981ad1e115414ea39` | A Two Sides body whose **both** distances are zero — an extrusion that reaches nowhere, refused `InvalidSemanticValue` rather than clamped |
| `object_state_v2.forge` | 698 | `3c9bcd6305bcdc26ac2f6ad5db72d0f8a163acb26236a4e48f2afe12905ba608` | **`SCNE` v2.** Three Construction Bodies carrying between them every piece of per-body state v2 adds — the first named `housing` and **locked**, the second **hidden**, the third plain so the file also pins an unmarked body at v2 |
| `object_state_bad_flags_v2.forge` | 494 | `b2cc2bf2eac579361111565cd9de24e535387928e1cbe916cc4aec1f118fd159` | The same shape with a **reserved** flag bit (`0x04`) on the first body; every length, count and CRC is correct, so only the reserved-bit rule can refuse it |
| `cad_region_hole_v5.forge` | 302 | `c6d269425cfa02d749e00b6fe922edf1d6896e6d488a1bad52efbfb59ece91f5` | **`CADB` v5.** A 4 × 3 m rectangle (entity 1) around a 0.8 m-radius circle (entity 2) on XY, extruded One Side 1 m as the region **between** them — hole list `[2]`, no later feature; the smallest v5 fixture |
| `cad_feature_add_v5.forge` | 370 | `2cb25af694e26a1d186374ec7e691d500201068eeb0606e08dc486c2d637f264` | A 2 × 2 m block 1 m deep carrying ONE later **Add**: a 0.8 m square on feature 1's far cap, 0.5 m along the face normal, at the six-face rectangle signature `0x958F78AF1C70BAA1` |
| `cad_feature_cut_v5.forge` | 362 | `9f4efdb626fea351790a0063308b422954aa49f1162e199027d5d04b43b66cbc` | The same block carrying ONE later **Cut**: a 0.3 m-radius circle on the far cap, 0.5 m **against** the face normal |
| `cad_feature_chain_v5.forge` | 496 | `a4a65681d08c093a9179d5c6be22a56fe2d7b2347670d2231229c2ef0230f10f` | The holed base of `cad_region_hole_v5` carrying an Add (a 0.6 m square at `(1.4, 0)`, **Symmetric** 0.25 m) and then a Cut (a 0.3 m-radius circle at `(-1.4, 0)`, 0.5 m against the normal), both on feature 1's far cap at the 38-face holed signature `0x937BA1514FAF7381` — a chain a reader must apply in order |
| `cad_bad_operation_v5.forge` | 370 | `b04b776b29189ffe7262b5ba95f9d106e5a5f37fa6f6c8a8566a5b31de002a23` | The Add fixture with `operationCode` **9** — refused `InvalidSemanticValue` by the operation code alone |
| `cad_bad_feature_ref_v5.forge` | 370 | `79436a25e7177fa818b7ac4dac1476254658c9e09f1c3a49b0f042f560b738cd` | The Add fixture whose support names feature **7**, which the body does not have — refused `InvalidSemanticValue` |
| `cad_bad_feature_order_v5.forge` | 370 | `7d50931ddf3f7a734454beaed51576225aae0cc1d990b94ba5bc3ab8d7c4e951` | The Add fixture whose later feature id is **1**, which is not above the first feature's — refused `InvalidSemanticValue` |
| `cad_bad_region_v5.forge` | 302 | `17d27c5c46b9b9f7d606d278d524aff6cfbc48647504497825c8438f2493f7a0` | The hole fixture storing the hole list `[7]` instead of the `[2]` the sketch derives — refused `InvalidSemanticValue` |
| `cad_sketch_shared_v6.forge` | 384 | `9dedb935d190c8831e80c31798a21fc6ebb7f7325564e4e7d30c3f0fd80a8342` | **`CADB` v6.** ONE retained sketch — a 4 × 3 m rectangle (entity 1) around a 1 × 1 m square (entity 2) on XY — extruded by TWO features: the base selects the ring AND the square (their union, the whole block), One Side 1 m; feature 2 is an **Add of the square alone**, 0.5 m against the normal, naming the same `sketchId` 1. Loads and regenerates (volume 12.5 m³) |
| `cad_face_lens_v6.forge` | 394 | `928f42163c3983f97c79cf408b5f00a5a4b990cc20e4001a2c8073bf43458e12` | The PF-S1-01 sketch (a 4 × 3 m rectangle and a 0.5 m circle centred on its right side) with the **lens inside the rectangle** selected as a planar face: `1.1[X(2.0#0)>X(2.0#1)] 2.0[X(1.1#0)>X(1.1#1)]` |
| `cad_face_protrusion_v6.forge` | 474 | `f5602527a1bf228746016334ad030d02904c60d5c6fafe775a8d66ccdf087528` | The rectangle and three lines closing a 1 × 1 m square against its right side through two T-junctions; the **protrusion** face `~1.1[X(2.0#0)>X(4.0#0)] 2.0[S>E] 3.0[S>E] 4.0[S>E]` |
| `cad_face_two_circles_v6.forge` | 386 | `41675dd1d5edbf40886c5cb73ecfab00801453ba5ba77d84f2d900123315c081` | Two 1 m circles at u = ±0.4 and the **lens** between them: `1.0[X(2.0#1)>X(2.0#0)] 2.0[X(1.0#0)>X(1.0#1)]` |
| `cad_mixed_selection_v6.forge` | 497 | `4406c9f85a89481ace78a14591134dcfa8373a4b30b43908cf2fec608e3d27a5` | A **LoopRegions** base (the 2 × 2 m block, 1 m) and a **PlanarFaces** Add on its far cap: a second retained sketch (placement 3, feature 1 CapFar, lineage `0x958F78AF1C70BAA1`) of two 0.5 m circles at u = ±0.2, selecting their lens, 0.25 m |
| `cad_bad_sketch_ref_v6.forge` | 384 | `cde2cab0a6230a6bb5f479874c4c6db0c36321ea3a17cbe77823651d32df66e7` | The shared fixture whose Add names sketch **7**, which the table does not carry — refused `InvalidSemanticValue` (`SketchNotFound`) |
| `cad_duplicate_sketch_id_v6.forge` | 497 | `ead5488fc5cccdbe89f8d29d3e9c79cc0467de8447f7cf9930179b9bb77e6a1d` | The mixed fixture whose second sketch wears id **1** as well — refused `InvalidSemanticValue` (`DuplicateSketchId`) |
| `cad_bad_selection_kind_v6.forge` | 394 | `cf3b0c64e066b6b809a5744e4b9d32d46864ae7d690b89c4bc5abd6785517e27` | The lens fixture with `selectionKind` **9** — refused `InvalidSemanticValue` by the code alone |
| `cad_noncanonical_face_v6.forge` | 394 | `2aed2f15bb5437286906a3352af39d3d8dd29c40b876a18d65d14b0aac8ca2a3` | The lens fixture with its outer cycle **rotated** to start at the circle fragment — the same boundary, not the canonical encoding; refused `InvalidSemanticValue` (`PlanarFaceRefNotCanonical`), never re-rotated |
| `cad_unresolved_face_v6.forge` | 394 | `9602fc4a281e4d6b7bf79fedf766d76ad2e2e2d142e9917d5b44635c65d05b4b` | The lens fixture whose circle fragment ends at crossing ordinal **2** of a side the circle crosses twice — well formed, canonical, and derived by no arrangement; refused `InvalidSemanticValue` (`PlanarFaceUnresolved`), no nearest face |
| `cad_spline_face_v6.forge` | 451 | `d85f98db59968bbe6c47f1842f59bb2f93f167f61deba67f5f846e98651cd800` | The lens fixture with a three-point **Spline** (entity 3) added to its sketch — refused `InvalidSemanticValue` (`PlanarFaceUnsupportedCurve`) |
| `cad_overlap_face_v6.forge` | 376 | `313652c95c14d3ebfd5e451447b8b46d0890fbed35b7c680911fb94426e118f9` | The rectangle with a line lying **along** its bottom side, selecting the rectangle's whole boundary — refused `InvalidSemanticValue` (`PlanarFaceAmbiguousOverlap`) |

The eighteen corrupt fixtures written since `CADB` v2 — two each for `CADB` v2,
v3 and v4, four for `CADB` v5, seven for `CADB` v6 and one for `SCNE` v2 — are
**constructed** by the PowerShell builder with the bad value in place, never
generated and then mutated; the C++ self-test reaches the same bytes by its own
route (patching the valid parent's one field and its CRC, or — for six of the
seven v6 ones — writing the bad STATE through `encodeProjectV1Unchecked`), and
the digests agreeing is what proves the two routes describe one file. The
envelope fixtures and `cad_bad_plane_v1` predate the rule and are still derived
from their canonical parent.

Every fixture written before Stage 018A is **byte-for-byte unchanged** by the
`SCNE` v2 bump, because none of them hides, locks or names a body and v2 is
written only when one does — the same promise `CADB` v2 and v3 already keep.

The five imported fixtures share one Imported Mesh: four vertices, two submeshes
with **different** `doubleSided` answers, named `head_low`, placed at
`(1.5, -0.25, 4.0)` with the identity rotation and scale. The two `IMPORT-01B`
fixtures share one sculpt mesh on an imported body: a four-vertex tetrahedron,
edited, and deliberately NOT the geometry it was frozen from -- a fixture where
the two matched could not tell a decoder that confused them apart. Every number
is an exact binary fraction, so the two implementations agree byte for byte or
not at all.

No section version has moved an older fixture: `CADB` v2, v3, v4, v5 and v6 are
each written only when a body needs what they add — a face support, a curve, an
extent that is not One Side, a hole or a later feature, a shared or retained
sketch or a face selection — so a world-only CAD
project still writes `CADB` v1, a curveless one v1 or v2, a One Side one v1..v3
and a one-region single-feature one v1..v4. Each of these features costs a
project that does not use it exactly nothing, exactly as the imported branch
and the generalized `SCUL` cost the files before them nothing.

**Fifty-six fixtures in all.** The thirty-six that predate `CADB` v5 are
verified by `FSR1A-12`, `IMP01A-19`, `IMP01B-11/12`, `CADR0-33..36`,
`CADA3-46..51`, `CADUXR1-38`, `CADEXT-10` and `OBJ018A-15/16`, and printed on
every debug launch as `FORGESHAPE_PROJECT_GOLDEN_SHA256`, `…_IMPORTED`,
`…_IMPORTED_SCULPT`, `…_CAD` and `…_CAD_V2`. The eight `CADB` v5 fixtures are
asserted by `CADVS_IO_22` and the twelve `CADB` v6 fixtures by `CADV6_P11`:
the production encoder reaches every one of those digests from a state built in
C++ — the five valid ones through the ordinary writer, deriving each planar face
from its sketch's arrangement rather than copying it; the refusals through the
same writer without its validation (`encodeProjectV1Unchecked`), and the one bad
code by patching the valid parent's byte. CI FAST's corpus parity step holds all
fifty-six byte-identical between the builder and the repository. The twelve v6
fixtures are single-body Construction projects with the `SCNE` record of the
v4/v5 ones, and every one of the forty-four before them is byte-for-byte
unchanged: none needs what v6 adds.

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
