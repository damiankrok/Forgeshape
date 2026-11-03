# CAD-R2-C2 — DESIGN (the seam fix)

## The earliest failing layer

`BEFORE_REPRO.md`: the screen ray is correct within its own float precision;
`intersectRayTriangle`'s float32 containment test rejects a point that exact
arithmetic over the SAME float inputs places inside triangle 34 (u+v =
0.999998093). Scene arbitration and the chooser only consume that miss. So the
fix belongs in `intersectRayTriangle` (`forgeshape_picking.cpp`) and nowhere
downstream — no chooser fallback, no invented face.

## Options weighed

| Option | Verdict |
| --- | --- |
| Raise `kBarycentricEpsilon` | **Rejected.** The float32 error scales with the origin's distance (8e-8 at 2 m, 3.5e-6 at 250 m, ~7e-6 at 500 m — the far plane), so any fixed overlap is either too small far away or a visible widening of the silhouette near by; and the constant is a documented arm64-FMA decision the prompt forbids touching blindly. |
| Re-base the ray origin near the mesh (move it forward by the distance to the mesh's bounds, add it back to t) | Workable, but it moves the origin by a float multiply-add at 250 m (≈1.5e-5 m rounding), needs per-mesh bounds the view does not carry, and changes the ray every caller sees. More moving parts than the defect. |
| Watertight edge-function test (Woop et al.) | Exact edge consistency by construction, but a different algorithm: new culling and tie conventions to re-prove against every existing check, and a larger diff in the hottest function. |
| **Evaluate the same Möller–Trumbore in double from the float inputs** | **Chosen.** Same algorithm, same branches, same thresholds, same float interface; only the precision of the intermediate arithmetic changes. |

## Why double is enough (error bound)

Inputs are float32 (24-bit significands). In double (53 bits): the difference
of two floats is exact whenever their exponents differ by ≤ 29 (any two
coordinates of a scene within ±1e8 of each other relative to a 1e-1 triangle —
far beyond the far plane of 500 m); the product of two floats is exact (48
bits); the cross / dot products then carry ≤ 2–3 roundings at 2⁻⁵³ relative.
For `u = dot(s, p) / det` with |s| ≤ 500 m, |p| ≈ |e2| ≤ a few m, det ≈ 0.35:
error ≲ 3 · 2⁻⁵³ · 500 · 3 / 0.35 ≈ 1.4e-12 — six orders below the 1e-6
overlap. So a ray exactly on a shared edge falls within at least one triangle's
widened bounds for every origin the camera can produce, while a ray more than
~1e-6 of a triangle outside it is still rejected (`PICK_C2_06`: 10 µm outside a
0.6 m edge misses, far and near).

## Invariants and why each holds

| Invariant | Holds because |
| --- | --- |
| Front faces only for a closed solid; two-sided only for `renderBothSides` | the `det` sign test is the same, computed more exactly (`PICK_C2_07`, every existing culling check) |
| Degenerate, parallel, non-finite, behind-origin fail closed | same `kRayTriangleEpsilon` / `kMinRayDistance` thresholds, same `isfinite` checks; `vec3Finite` on inputs unchanged (`PICK_C2_08`) |
| Nearest positive hit, first index on an exact tie | `pickTriangleMesh` untouched; t is computed in double and rounded to float once (`PICK_C2_10`) |
| No widening of silhouettes, holes stay empty | `kBarycentricEpsilon` unchanged (`PICK_C2_06`, `PICK_C2_15`) |
| Semantic identity | triangle index → `CadFaceRange` → `TopoRef` unchanged; the seam resolves to the exact floor face, and a regenerated mesh to the same face (`PICK_C2_13`, `PICK_C2_14`) |
| An edge between two DIFFERENT faces | resolves to one of exactly those two, by the existing rule (nearest t, then lower triangle index), identically on repetition (`PICK_C2_16`); no nearest-face heuristic, no retarget |
| C1 world-plane policy | `SupportChooser::resolve` untouched; C1_CH_01..06 still pass — a curved wall still occludes the plane behind it, a plane in front still wins |
| Transforms | `transformRayToLocal` untouched (`PICK_C2_09`, rotated + translated body) |
| x86_64 / arm64 | the result no longer depends on whether the compiler contracts the float dot products into FMAs at the ~1e-6 level; any remaining ABI difference is at ~1e-12 |
| No new state, no JNI, no renderer, no kernel, no format | the change is local arithmetic in one function |

## Callers

`pickTriangleMesh` (viewport selection through `pickMesh` / `pickSceneSnapshot`,
the support chooser, Sculpt's stroke and hover picks) and Freeform's cage pick
(`forgeshape_freeform_session.cpp`, two direct calls). All gain the same
correctness; their suites (SCENE, SCULPT_BRUSH_KERNEL, FREEFORM, GIZMO,
CAD_A3) pass unchanged.

## Cost

`PERF.md`: +2.4 % to +3.0 % per triangle test on a 20,000-triangle mesh,
product-only linking, 360 samples per side; the CAD scene pick unchanged.
