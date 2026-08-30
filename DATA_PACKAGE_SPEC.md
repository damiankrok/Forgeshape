# ForgeShape `.forge` project format — v1

**Owner of this document:** the exact binary layout of a ForgeShape project
file, its section payload field order, its enum numeric codes, its validation
and compatibility rules, its deterministic-write rules, and the fixture
inventory. Nothing else owns any of those.

Implementation: `app/src/main/cpp/forgeshape_project_bytes.{h,cpp}` (little-endian
primitives and CRC-32), `forgeshape_project_document.{h,cpp}` (the document and
the codec), `forgeshape_project_state.{h,cpp}` (live scene ↔ document).
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

### 1.1 Semantic project truth — serialized

| Truth | Why it cannot be recomputed |
| --- | --- |
| Body identity (`ObjectId`), scene order, active body | Identity is minted, not derived |
| The id allocator's high-water mark | Stops a reopened project minting a collision |
| Active primitive kind | The user chose it |
| **All six** remembered primitive parameter sets, per body | A Box → Sphere → Box round trip must return the box the user typed |
| Placement: position (metres), rotation (degrees), scale (unitless) | The user placed it |
| Each Frozen Sculpt Mesh's local float32 vertex **positions** | After a stroke they cannot be recreated from the Construction Source |
| Each Frozen Sculpt Mesh's index topology | Same |
| `renderBothSides` | A geometric fact about that frozen representation, not about the current source |
| `sourceStale` | Whether the source moved on after the freeze |
| `hasEdits` (a **boolean**, never the revision it derives from) | What the destructive *Reset Sculpt from Shape* guard asks; losing it would let a reset discard the whole file silently |

### 1.2 Derived runtime/render truth — **not** serialized

* Every Construction `RuntimeMesh` — regenerated from the parameters on load.
* Vertex normals, sculpt adjacency (`SculptTopology`), render-mesh vertex
  duplication for hard edges.
* `SculptRevision`, `MeshRevision`, update counters, rejection counters.
* GPU buffers, upload diagnostics, renderer caches.
* Vertex **colours**. They feed only the debug-only source-colour shading mode —
  Studio Solid and MatCap both ignore them — so they are presentation, not
  truth. A loaded sculpt vertex is given one neutral value
  (`kLoadedSculptVertexColor`, 0.5 in each channel).
* The Construction Undo/Redo stack. It is session history: a successful load
  starts a fresh session, and a failed load leaves the existing history
  untouched.
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
| 15 | headerFlags | `u8` | bit0 `hasCONS`, bit1 `hasSCUL`; all other bits zero |
| 16 | sectionCount | `u32` | exact number of sections that follow |
| 20 | fileBytes | `u64` | exact total file size |

The magic's trailing `1` is a **generation marker, not the version**. The
version is the `major`/`minor` pair.

`headerFlags` must agree with the sections actually present, or the file is
refused: a reader that trusted the flags without parsing would otherwise be told
something false.

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
present.

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
`projectKind = Sculpt`. Section version 1. Keyed by `ObjectId`, and it must carry
**one entry per SCNE body, in scene order**.

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
`CONS` companion; neither branch overwrites the other, and only the header's
`projectKind` decides which representation the project reopens in.

---

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
| A second `SCNE`, `CONS` or `SCUL` — judged on the **tag**, before the version is, so a duplicate at a version the reader cannot read is still a duplicate | `DuplicateSection` |
| `SCNE` absent; `CONS` absent for a Construction project; `SCUL` absent for a Sculpt project | `MissingRequiredSection` |
| A payload's own structure does not add up; a sculpt flags byte with a reserved bit | `BadPayload` |
| A count no project can have, or one whose byte size would overflow — refused **before any allocation** | `ImpossibleCount` |
| A value the live model refuses: a non-positive or non-finite dimension, a broken capsule relation, a non-finite position or rotation, a zero or negative scale, a duplicate or reserved `ObjectId`, an allocator that could mint a collision, an index out of range, an unknown primitive or feature code | `InvalidSemanticValue` |
| An active body no section carries, a `CONS`/`SCUL` body `SCNE` does not carry, a Sculpt project whose active body has no sculpt mesh | `UnresolvedReference` |
| A load attempted while a Construction edit is open (not a property of the file) | `RefusedEditInProgress` |

Semantic values are checked by calling the **domain's own** validators —
`validateDimensionMeters`, `validateCapsuleMeters`, `validateTransformValue`,
`validateScaleValue` — never by restating the rules, so a file can never carry a
value the editor would have refused.

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

The header's `hasCONS` / `hasSCUL` flags are checked against the section **tags
the file carried**, not against the sections this reader managed to decode.
Otherwise skipping an optional section it could not read would make the header
look like a lie.

### One thing the FORMAT allows and this RUNTIME does not

A document with no Construction branch is a legal `.forge` file: a Sculpt
project's `CONS` is optional. This build still refuses to LOAD one
(`MissingRequiredSection`), because every `SceneObject` has a Construction
Source and inventing a default Box for a body whose real shape the file
described would be fabricating project data. The refusal happens in
`forgeshape_project_state.cpp`, where the reason is "this build cannot evaluate
that project", rather than in the codec, where the file itself is not at fault.

### Deterministic writer

The same semantic document always produces byte-identical output: canonical
section order, bodies in scene order, features by ascending `LocalFeatureId`, and
every field a fixed-width little-endian encoding. `encode → decode → encode` is
byte-identical.

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

`.forge` is **not** an interchange format. GLB/glTF, OBJ and FBX are separate,
deliberately chosen import/export pipelines with their own stages; none of them
is implemented, and nothing in the product offers one.

---

## 10. Migration honesty

**There has never been a production `.forge` format before v1.**

`decodeProject` is the version dispatch seam and it has exactly one branch. There
is deliberately **no v0**, and no v0→v1 migration is claimed or tested. The first
real schema bump must add a branch there, **keep the v1 fixtures below**, and
bring a real compatibility/migration test with it.

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
  the file explicitly rather than loading part of it.
* A new primitive takes the next unused **file** code and one more parameter
  block appended to the `CONS` body record, behind a `CONS` section-version bump.
* A new feature kind takes the next unused feature-kind code and appears in a
  body's feature list. `LocalFeatureId` is numbered **per body**, so a feature
  graph can grow without renumbering anything that already exists.
* Persistent topology references, when they arrive, must be expressed as feature
  lineage — never as a render-triangle index, which is derived and unstable.
* The `major` is bumped only for a change an older reader must not attempt.
