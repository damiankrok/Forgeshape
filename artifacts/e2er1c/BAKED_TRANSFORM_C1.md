# E2E-R1C-C1 — the baked static transform

`ARCH-OWNER-07`, approved 2026-08-31 after the owner reviewed the first exported
files in Blender. It supersedes Stage 023's node-transform policy for static GLB
export; nothing else about the exporter changed.

## The contract

```
Model = T · L          where   L = Rz · Ry · Rx · S
```

| | Before (Stage 023) | Now (ARCH-OWNER-07) |
| --- | --- | --- |
| Node transform | one `matrix`, the whole `T·R·S` | `translation` only |
| Node rotation | inside that matrix | **absent** → identity |
| Node scale | inside that matrix | **absent** → identity |
| Vertex positions | body-local, unbaked | `L · p` |
| Vertex normals | body-local, unbaked | `normalize(transpose(inverse(L)) · n)` |
| Pivot | body local origin, positioned by the matrix | body local origin, positioned by the translation |
| Axis/unit conversion | none | none |
| Representation choice | per body, Sculpt then Construction | unchanged |
| `.forge` | nine authored values | unchanged |

Both forms are valid glTF and both place the object identically. The difference
is what the receiving tool has in its hands: a static asset arriving with a live
rotation and a non-uniform node scale is an object whose every downstream
operation — a modifier, a boolean, a physics shape, a normal recalculation, a
second export — has to keep re-deriving the shape the user actually made. Baked,
the shape simply *is* the mesh.

## The normal matrix, and why it is not a second answer

`N = transpose(inverse(L))`. For `L = R · S` with `R` orthonormal and `S`
diagonal and strictly positive, that equals `R · S⁻¹`, which is exactly the
product's existing `ConstructionTransform::normalMatrix()`. So the exporter
reuses the one normal authority rather than adding a numeric matrix inversion
beside it — and the equality is **checked numerically** in
`FSR1C_C1_06_the_normal_matrix_is_the_inverse_transpose_of_L`, against
`inverseModelMatrix()`, rather than asserted in a comment.

Carrying a normal by `L` instead is the classic error and it is invisible to the
obvious checks: the result still normalises to unit length and still points
roughly outward. What it stops being is **perpendicular to its own surface**.
Two cases attack that specifically:

- On a **sphere** — the same body exported twice, once unscaled and once at
  4:1:1, with both candidate matrices applied to the first export's normals in
  the test. The written normals match the inverse transpose and differ from `L`
  on essentially every vertex.
- On a **box** — every vertex normal must be exactly perpendicular to the
  triangles it belongs to, after a rotation and a 2/3/0.5 scale.

A box alone would prove nothing: its face normals lie along its local axes, and
for a normal parallel to a scale axis the two matrices give the same direction
and differ only in length, which normalising then hides. That is why the sphere
case exists.

## No recentre, proved on a cone

`L` is applied about the body's LOCAL ORIGIN. That origin is the pivot the gizmo
rotates about, the pivot `T` then positions, and the pivot every downstream tool
inherits, so moving it to a bounds centre would be a silent break.

The test uses a **cone**, and the choice is load-bearing: a box, a plane and a
sphere are all centrally symmetric about their local origin, and rotating a
centrally symmetric point set leaves its bounding box symmetric too — so a
recentre-on-bounds would be completely invisible in any of them. A cone has its
apex at +Y and its base disc at −Y, so once turned its bounds are genuinely
lopsided, and a recentre would force `min == -max` on every axis.

The corrected Construction sentinel shows this directly: `Body_4`, the cone,
exports bounds `min (−3.710, −4.000, −2.564)` / `max (4.995, 2.487, 1.469)`.

## The determinant rule

`det(L) = sx · sy · sz`, and the transform domain refuses a zero or negative
scale, so `det(L) > 0` always holds for an authored placement and winding
survives the bake untouched — no triangle is ever reversed.

The exporter checks anyway, at capture **and** again at the byte-writing
boundary, because both are public entry points:

| `det(L)` | Result |
| --- | --- |
| `> 0` | exported |
| `== 0` or non-finite | refused, `SingularTransform` |
| `< 0` | refused, `MirroredTransform` |

A mirror is refused rather than compensated. Reversing every triangle to keep
the winding right would be implementing the Mirror the domain says cannot exist,
in the exporter, which is not the exporter's decision to make. The native case
injects both into a hand-built snapshot to prove the writer refuses them, and
then re-asserts that `ConstructionTransform::setValues` still refuses a negative
and a zero scale at the source — removing the writer's guard is not removing
that one.

## The corrected sentinels, read back independently

Parsed on the host, from the committed bytes, without the exporter:

`construction_sentinel.glb` — 59884 bytes,
`8ac4fb6abb509361b911c9b4bbc2dd8385910668808b40fbd9faeab244f2c3ff`

```
nodes=6  meshes=6  buffers=1
"matrix" occurrences: 0     "rotation": 0     "scale": 0     "uri": 0

Body_1  T=(0.5, -0.25, 1.25)  24v  12t   min=(-1.584 -1.452 -1.374) max=(1.584 1.452 1.374)
Body_2  T=(1.0, -0.50, 2.50) 130v 128t   min=(-3.078 -3.868 -1.607) max=(3.078 3.868 1.607)
Body_3  T=(1.5, -0.75, 3.75) 482v 960t   min=(-2.558 -2.438 -1.971) max=(2.558 2.438 1.971)
Body_4  T=(2.0, -1.00, 5.00)  66v  64t   min=(-3.710 -4.000 -2.564) max=(4.995 2.487 1.469)
Body_5  T=(2.5, -1.25, 6.25) 514v 1024t  min=(-4.968 -2.620 -1.788) max=(4.968 2.620 1.788)
Body_6  T=(3.0, -1.50, 7.50)   4v   2t   min=(-1.518 -5.582 -5.867) max=(1.518 5.582 5.867)
```

- Node translations are unchanged from the fixture's authored positions
  (0.5·i, −0.25·i, 1.25·i) — the split moved nothing.
- No node carries a rotation, a scale or a matrix.
- The bounds now encode the baked `R`/`S`: `Body_1` was authored 2 × 1 × 0.5 m,
  so unbaked it would span ±1 / ±0.5 / ±0.25; it spans ±1.584 / ±1.452 / ±1.374.
- `Body_4`, the cone, is lopsided — the no-recentre evidence above.
- Six separate nodes and meshes, one buffer, no external `uri`.

`sculpt_sentinel.glb` — 2588 bytes,
`ae82a0720d9f503f77d0237edf40ab5c1152bae6938df08166e1ea9b31014f2c`

```
"matrix": 0   "rotation": 0   "scale": 0   "uri": 0

Body_1  T=(0.0, 0.0,  0.0)  24v 12t   min=(-1 -0.5 -0.25)     max=(1 0.5 0.25)
Body_2  T=(1.5, 0.5, -2.0)  12v  4t   min=(-1.231 0 0)        max=(~0 1.5 1.810)
```

- `Body_1` was never sculpted and is unrotated and unscaled, so its Construction
  box is still exactly its authored 2 × 1 × 0.5 m — an identity `L` bakes to the
  identity.
- `Body_2` is the sculpted tetrahedron: still 12 vertices and **4 triangles**,
  still not the 1.5 m sphere its Construction Source describes, and now turned,
  so its bounds are no longer the authored `(0,0,0)..(1.5,1.25,1.75)`.
- Its translation `(1.5, 0.5, −2.0)` is untouched.

Because the bake turns the tetrahedron, the device case identifies it by
properties a rotation cannot change — four triangles, four distinct corners, and
a longest inter-corner distance of 2.2079 m — rather than by an axis-aligned
bounding box that is no longer meaningful.

## Determinism

`scripts\run-glb-export-evidence.ps1` deletes the on-device files before each
run and refuses to produce anything from a failing one. The same project still
exports byte-identically, native (`FSR1C_C1_11`) and through the instrumented
path (`fsr1cC1_11_thesameProjectStillExportsByteIdentically`).
