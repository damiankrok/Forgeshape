# Extrude contract

`generateCadMesh(const CadBodyState&, ConstructionMesh*) -> CadStatus`

Inputs: the sketch, the chosen profile's anchor id, `depth` (a Construction
length: finite, positive, float-resolvable, <= 1e5 m), `direction`
(`AlongNormal` | `AgainstNormal`).

| Property | Rule |
| --- | --- |
| Where the solid is | from its -N face to its +N face; `AlongNormal` puts the -N face on the sketch plane (offsets 0 .. +depth), `AgainstNormal` the +N face (-depth .. 0) |
| Vertices | `2n`: the profile at the near offset, then at the far offset; shared by caps and sides |
| Triangles | `2(n - 2)` cap triangles + `2n` side triangles; index count `3(4n - 4)` |
| Watertight | every undirected edge on exactly two triangles, traversed in opposite directions (`watertight()` in the self-test) |
| Winding | canonical counter-clockwise seen from outside on every face: +N cap keeps the CCW profile order, -N cap reverses it, side quad `(lower_i, lower_i+1, upper_i+1)` has normal `edge x N` = outward for a CCW profile. Proved by `meshObeysCanonicalWinding` (convex) and by signed volume (exact for the concave L: 5.0) |
| Normals | derived by the render layer from these positions; render-only vertex duplication gives hard edges, exactly as for a box |
| Degenerate triangles | none: the profile has no duplicate edge and non-zero area by the loop rules |
| Finite | every position checked; `RegenerationFailed` otherwise |
| Placement | never baked into the geometry; `SceneObject::transform()` stays the body's, and a CAD Body starts at the identity |

New Body only. No Add/Cut, no taper, no symmetric extrusion.

Proved by `CADR0-24..27` (rectangle, circle, 7-gon, concave L, XZ and YZ,
against-normal) and exported through the ordinary GLB path (`CADR0-37`).
