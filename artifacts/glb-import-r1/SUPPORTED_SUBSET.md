# GLB-IMPORT-R1 — the supported subset, and every refusal

R0 read exactly what ForgeShape's own exporter emits. R1 (`ARCH-OWNER-09`)
widens that to the class of **static** GLB another sculpting tool writes, so the
owner can look at a real low-poly character in the ForgeShape viewport.

Everything outside the subset still **fails closed with a named reason**. A
diagnostic that silently ignored a transform, a sparse accessor or a compression
extension would answer the owner's question wrongly, which is worse than
refusing to answer it.

`forgeshape_gltf_import.cpp` calls nothing in `forgeshape_gltf_export.*`. It
shares `forgeshape_json.*` (which knows only what JSON is) and
`forgeshape_math.h` (vectors and matrices, no glTF in either), and re-derives
every offset, length, stride and bound from the file.

## What R1 added to R0

| Feature | R0 | R1 |
| --- | --- | --- |
| Node `matrix` | refused (`NodeMatrix`) | **accepted**, column-major, affine only |
| Node `rotation` / `scale` | refused unless identity | **accepted**, composed as `T · R · S` |
| Several TRIANGLES primitives per mesh | appended into one list | **kept as separate draw batches**, so `doubleSided` survives per primitive |
| Primitives sharing one POSITION accessor | decoded once per primitive | **decoded once**, so the vertex count is the file's |
| Missing `NORMAL` | refused (`MissingAttribute`) | **generated**, area-weighted, from the baked positions |
| `COLOR_0` / `COLOR_1` / `TEXCOORD_0` / `TEXCOORD_1` | not encountered | **structurally validated and ignored** |
| Material `doubleSided` | not read | **read**, and it reaches preview culling and nothing else |
| `extras` | not read | still not read — explicitly ignored at every level |

## Supported

| Feature | Notes |
| --- | --- |
| GLB 2.0 binary container | magic, version and total length all checked against the bytes supplied |
| One JSON chunk | parsed by `forgeshape_json`, which refuses `nan`, `Infinity`, `1e400`, trailing commas, leading zeros and a second document |
| One embedded BIN chunk | `buffers` must be exactly one and must have no `uri` |
| `scene` / `scenes` | the named default scene, whose node list must be non-empty |
| Flat `nodes`, one or more | each must name a mesh; children are refused, never flattened |
| Node `matrix` | exactly 16 finite numbers, **column-major**, bottom row `[0,0,0,1]` |
| Node TRS | `translation`, a normalizable `rotation` quaternion `[x,y,z,w]`, and `scale`; each defaults to identity; composed `T · R · S` |
| `meshes` with one or more TRIANGLES `primitives` | up to 4096 per mesh; each becomes one draw batch |
| `POSITION` float32 `VEC3` | required; every value checked finite |
| `NORMAL` float32 `VEC3` | optional; count must equal POSITION's; carried through the inverse transpose and normalized |
| Indexed `TRIANGLES` | `UNSIGNED_BYTE`, `UNSIGNED_SHORT` and `UNSIGNED_INT`; every index bounds-checked against its own vertex block |
| `COLOR_0`, `COLOR_1`, `TEXCOORD_0`, `TEXCOORD_1` | accessor resolved, range and count checked, then **not decoded**. The preview draws one flat neutral and never claims to show them |
| `materials[].doubleSided` | the only material member read at all |
| `extras`, `asset.generator`, `extensionsUsed` | read by nothing |

## The transform, and what happens to it

`R1` **bakes** the complete node transform into the preview's positions, so
every draw item carries an identity model matrix and the placement exists in
exactly one place.

- positions become `M · p`;
- normals become `normalize(transpose(inverse(L)) · n)`, where `L` is `M`'s
  upper-left 3×3 — not `L · n`, which is only the same answer while the scale is
  uniform;
- a **negative determinant** mirrors the geometry, so triangle winding is
  corrected during the bake and front faces stay front faces. This is a preview
  decision only: the product still has no Mirror, and the exporter still refuses
  to write one (`MirroredTransform`);
- a determinant of zero, or a non-affine bottom row, is `SingularNodeTransform`.

**No coordinate conversion of any kind.** glTF is right-handed, +Y-up and
metric, and so is ForgeShape. There is no Blender-style Y/Z fix, no conversion
node and no global scale factor — the same fact that makes the exporter's lack
of one correct.

## Generated normals

When a primitive states no `NORMAL`, the reader generates one per vertex:

1. each triangle's **unnormalized** face normal is taken from the **baked**
   positions, so its magnitude is twice its area and a large face counts for
   more than a sliver;
2. it is accumulated into all three of its vertices;
3. the accumulated vector is normalized per referenced vertex;
4. a degenerate triangle contributes the zero vector, so it contributes nothing;
5. a referenced vertex left with no finite non-zero accumulation is
   `CannotGenerateNormals` — never a NaN and never an invented default;
6. a vertex no triangle names is recorded as the zero direction, which reaches
   no shading: the renderer derives its own normals from the positions.

This is a deterministic preview policy. It does not define the production import
shading contract, which is `IMPORT-01`'s to decide.

## Refused, by name

Each is a valid glTF feature this reader does not implement, or a file that
disagrees with itself. Each has its own status rather than one blanket
"unsupported", because the point of the diagnostic is to say precisely what
stopped it.

| Status | Category | Refused because |
| --- | --- | --- |
| `NoData` | unreadable | no bytes |
| `NotGlb` | unreadable | the header's magic is not `glTF` |
| `UnsupportedVersion` | unreadable | the container version is not 2 |
| `TruncatedFile` | unreadable | a declared length runs past the bytes, or the file disagrees with its own total |
| `ChunkMisaligned` | unreadable | a chunk length is not a multiple of 4 |
| `MissingJsonChunk` / `MissingBinChunk` | unreadable | a GLB must carry both, and the buffer must be embedded |
| `UnknownChunk` | unreadable | a chunk this reader will not skip silently, or a second JSON/BIN chunk |
| `MalformedJson` | unreadable | the JSON chunk is not a JSON document |
| `UnsupportedAssetVersion` | unreadable | `asset.version` is not 2.x |
| `NoScene` / `NoMeshes` / `NothingToImport` | unreadable | nothing to draw, or a node that names no mesh |
| `TooLarge` | unreadable | past a hard cap on bytes, meshes, vertices or primitives |
| `ExternalBuffer` | unsupported | a buffer or image with a `uri`, a second buffer, or any image at all |
| `UnsupportedExtension` | unsupported | any `extensionsRequired` — Draco and meshopt included |
| `HasAnimation` / `HasSkin` / `MorphTargets` | unsupported | animation, skinning and morph targets are all absent from this product |
| `SparseAccessor` | unsupported | a sparse accessor this reader would mis-read |
| `NonTriangleMode` | unsupported | a primitive that is not `TRIANGLES` |
| `NodeHierarchy` | unsupported | a node with children. Refused by name, never flattened: a flattened hierarchy is a different scene from the one the file describes |
| `NodeTransformConflict` | unsupported | a node stating BOTH a `matrix` and a TRS member, which glTF forbids |
| `MissingAttribute` | unsupported | no `POSITION` |
| `NonIndexedPrimitive` | unsupported | a primitive with no `indices` |
| `UnknownAttribute` | unsupported | an attribute outside the known ignore list, including `JOINTS_n`, `WEIGHTS_n`, `TANGENT` and any custom `_NAME` |
| `UnsupportedComponentType` / `UnsupportedAccessorType` | unsupported | an accessor shape this reader does not decode |
| `InterleavedAccessor` | unsupported | a `byteStride` that is not the tight packing |
| `SingularNodeTransform` | inconsistent | a node transform with no volume, or a non-affine bottom row |
| `AccessorOutOfRange` | inconsistent | an accessor reads past its bufferView or the buffer |
| `IndexOutOfRange` | inconsistent | an index names a vertex that does not exist |
| `IndexCountNotTriangles` | inconsistent | an index count that is not a multiple of three |
| `CountMismatch` | inconsistent | an attribute disagrees with `POSITION` on count |
| `CannotGenerateNormals` | inconsistent | a normal, supplied or generated, that cannot be made a unit direction |
| `NonFiniteValue` | inconsistent | a NaN or infinity anywhere in the geometry or the transform |

The three **categories** are what the user is shown, one sentence each. The
status token is what goes to the log and the diagnostics ring, where somebody
chasing a particular file can act on it.

## Still absent, deliberately

Production import (`IMPORT-01`), OBJ, FBX, materials, textures, UV editing,
animation, skinning, morph targets, compression extensions, external buffers or
images, editable hierarchy, and any durable imported object. R1 is still a
diagnostic: what it produces has no identity, cannot be edited, saved, autosaved
or re-exported, and is gone with the process.
