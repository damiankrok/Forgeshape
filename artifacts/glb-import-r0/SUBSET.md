# GLB-IMPORT-R0 — the supported subset, and every refusal

The reader implements exactly what the current ForgeShape exporter emits.
Everything else **fails closed with a named reason**, because a diagnostic that
silently ignored a transform, a sparse accessor or a compression extension would
answer the owner's question wrongly — which is worse than refusing to answer it.

`forgeshape_gltf_import.cpp` calls nothing in `forgeshape_gltf_export.*`. It
shares `forgeshape_json.*` (which knows only what JSON is) and
`forgeshape_math.h` (vectors and matrices, no glTF in either), and re-derives
every offset, length, stride and bound from the file.

## Supported

| Feature | Notes |
| --- | --- |
| GLB 2.0 binary container | magic, version and total length all checked against the bytes supplied |
| One JSON chunk | parsed by `forgeshape_json`, which refuses `nan`, `Infinity`, `1e400`, trailing commas, leading zeros and a second document |
| One embedded BIN chunk | `buffers` must be exactly one and must have no `uri` |
| `scene` / `scenes` | the named default scene, whose node list must be non-empty |
| Flat `nodes` | one node per drawn object |
| `meshes` with one or more triangle `primitives` | several primitives on one mesh are appended with their indices rebased |
| `POSITION` float32 `VEC3` | every value checked finite |
| `NORMAL` float32 `VEC3` | count must equal POSITION's |
| Indexed `TRIANGLES` | `UNSIGNED_BYTE`, `UNSIGNED_SHORT` and `UNSIGNED_INT`; every index bounds-checked against the vertex count |
| Node `translation` | applied per glTF semantics; the only transform R0 supports |
| Absent or identity `rotation` / `scale` | an explicitly written `[0,0,0,1]` and `[1,1,1]` are accepted, because that is what the specification says the default is |

## Refused, by name

Each is a valid glTF feature this reader does not implement. Each has its own
status value rather than one blanket "unsupported", because the point of the
diagnostic is to say precisely what stopped it.

| Status | Refused because |
| --- | --- |
| `NotGlb` | the header's magic is not `glTF` |
| `UnsupportedVersion` | the container version is not 2 |
| `TruncatedFile` | a declared length runs past the bytes, or the file disagrees with its own total |
| `ChunkMisaligned` | a chunk length is not a multiple of 4 |
| `MissingJsonChunk` / `MissingBinChunk` | R0 requires the buffer to be embedded |
| `UnknownChunk` | an unknown or duplicated chunk. The specification permits skipping one; R0 does not, because silently skipping a chunk is how it would fail to report what a file contains |
| `MalformedJson` | the JSON chunk is not JSON |
| `UnsupportedAssetVersion` | `asset.version` is not 2.x |
| `NoScene` / `NoMeshes` / `NothingToImport` | the document names no drawable geometry, or a node draws nothing |
| `ExternalBuffer` | a `buffer` or `image` with a `uri`, a second buffer, or a bufferView pointing outside buffer 0 |
| `UnsupportedExtension` | any non-empty `extensionsRequired` — an extension is required precisely because a reader that ignores it reads the file wrong |
| `HasAnimation` / `HasSkin` | `animations` or `skins` present |
| `SparseAccessor` | an accessor with `sparse` |
| `MorphTargets` | a primitive with `targets` |
| `NonTriangleMode` | a primitive `mode` other than 4 |
| `NodeHierarchy` | a node with `children`; R0 reads a flat scene |
| `NodeMatrix` | a node stating a `matrix` |
| `NodeRotation` / `NodeScale` | a node stating a non-identity rotation or scale. **Never ignored** — ignoring one would move every vertex of the preview |
| `MissingAttribute` | POSITION, NORMAL or `indices` absent |
| `UnsupportedComponentType` / `UnsupportedAccessorType` | a component or element type outside the table above |
| `InterleavedAccessor` | a `byteStride` that is not tight packing. A valid file this reader would silently mis-stride, so it is refused by name rather than read wrong |
| `AccessorOutOfRange` | an accessor reads past its bufferView or past the BIN chunk |
| `IndexOutOfRange` | an index names a vertex that does not exist |
| `IndexCountNotTriangles` | an index count that is not a multiple of three |
| `CountMismatch` | POSITION and NORMAL disagree on the vertex count |
| `NonFiniteValue` | a NaN or infinity in a position, a normal or a translation |
| `TooLarge` | past the 512 MB file ceiling or the per-mesh vertex bound |

## How the refusals are tested

Every row in the second table has a case, and they are built by **taking a real
export and changing one thing in its document**, then checking that the reader
refuses for *that* reason. Hand-writing a whole GLB per case would test a
fixture rather than the file the product actually produces. The edits are made
by `withJson()` in `forgeshape_gltf_import_selftest.cpp`, which re-frames the
container around a rewritten JSON chunk.

## Bounds

| Limit | Value | Why |
| --- | --- | --- |
| File | 512 MB | matches the exporter's write ceiling, so a file ForgeShape can write is one it can read back |
| Meshes | 4096 | a malformed count cannot make the reader allocate before it has read anything |
| Vertices per mesh | 8,000,000 | the same |
| JSON nesting depth | 15 | glTF is four deep; a hostile file cannot recurse the stack away |
| JSON values | 1,000,000 | parsing cannot allocate without bound from a small input |
