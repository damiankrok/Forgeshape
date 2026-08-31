# GLB-IMPORT-R1 — the owner sample this stage targets

**The owner's binary is not in this repository, and this stage did not have
it.** Everything below is the fingerprint the coordinator supplied. Nothing here
was derived from the file itself by any code in this repository, no attempt was
made to locate it on the machine, and no copy of it exists anywhere in the tree.

## The file, as the coordinator described it

`1 lowpoly.glb`

| Property | Value |
| --- | --- |
| size | `142820` bytes |
| SHA-256 | `59fd1bfe8473e9b78849170ef6c02023d45bf769a509b7ab1974127360ff3bd6` |
| GLB version | 2 |
| `asset.generator` | `nomad 11` |
| scenes / nodes / meshes | 1 / 1 / 1 |
| node | `mesh = 0`, a 4×4 glTF `matrix`, no child hierarchy needed |
| primitives | 7, all `mode = TRIANGLES`, all one material |
| POSITION | float32 `VEC3`, count `1987` |
| indices | 7 accessors, `UNSIGNED_INT` (5125), total `7908` — `2636` triangles |
| NORMAL | absent |
| COLOR_0 | normalized unsigned short `VEC4`, count `1987` |
| COLOR_1 | normalized unsigned byte `VEC4`, count `1987` |
| TEXCOORD_0 | float32 `VEC2`, count `1987` |
| material | `doubleSided = true`, alpha `OPAQUE` |
| `extras.nomad` | present |
| animation / skinning / external buffer / compression | none required |

## Every feature of it, and where it is covered

| The sample needs | R1 support | Proved by |
| --- | --- | --- |
| GLB 2.0, embedded BIN | supported since R0 | `GLBIR0-03` |
| a node 4×4 `matrix`, column-major | **new in R1** | `GLBIR1-01`, `GLBIR1-03`, `E2E-GLBIR1-03` |
| 7 TRIANGLES primitives on one mesh | **new in R1** (as draw batches) | `GLBIR1-06`, `E2E-GLBIR1-01` |
| all primitives over one POSITION accessor | **new in R1** (decoded once) | `GLBIR1-07` |
| `UNSIGNED_INT` indices | supported since R0 | `GLBIR1-08` |
| NORMAL absent | **new in R1** (generated) | `GLBIR1-09`, `E2E-GLBIR1-02` |
| COLOR_0 normalized u16 VEC4 | **new in R1** (validated, ignored) | `GLBIR1-12` |
| COLOR_1 normalized u8 VEC4 | **new in R1** (validated, ignored) | `GLBIR1-12` |
| TEXCOORD_0 float VEC2 | **new in R1** (validated, ignored) | `GLBIR1-12` |
| `doubleSided = true` | **new in R1** (reaches culling only) | `GLBIR1-13`, `E2E-GLBIR1-04` |
| `extras.nomad` | ignored | `GLBIR1-14` |
| no animation / skin / compression | still refused if present | `GLBIR1-15` |

## What was exercised instead

`forgeshape_glb_import_fixture.cpp` builds a deterministic **synthetic** GLB
carrying the same structural feature set — see `nomad_like_fixture.txt` for its
hash and counts. It is **not the owner's character**, it is not a character at
all, and no document in this bundle describes it as one. Its geometry is a
lopsided integer-polynomial surface chosen so a transposed, dropped or mirrored
node matrix changes the world bounds by a number a test can assert.

Its counts are the same order of magnitude as the sample's (1978 vertices, 3780
triangles, 7 primitives against 1987 / 2636 / 7), which is what makes it an
exercise of the same code paths at the same scale.

## What remains open

`E2E-GLBIR1-10` — opening the actual `1 lowpoly.glb` — is
**`OWNER_SAMPLE_RUNTIME_TEST_PENDING`**. It is pending only because the binary
is external to this coding environment, and it is the owner's manual step: tap
**Import GLB…**, choose the real file, and look at it.
