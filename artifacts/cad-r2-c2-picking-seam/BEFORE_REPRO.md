# CAD-R2-C2 — BEFORE REPRO (baseline `08406cf`, no product change)

Raw output: `repro/seam_diag_baseline_08406cf.txt` (program
`repro/seam_diag.cpp`, host objects built by `scripts/host-native-selftests.sh`
at `08406cf`, g++ 13.3.0, `-O1`).

## Result: REPRODUCED

```
eye=(49.5290,-0.5000,245.7452) |eye-target|=250.000 pixel=(500.000000,500.000122) proj=1
ray o=(49.5290375,-0.5000000,245.7451782) d=(-0.19611615,0.00000000,-0.98058069)
sceneHit=0 tri=-1 t=0.000000 pos=(0.000000,0.000000,0.000000)
tri 34 v0=(0.200,-0.800) v1=(0.800,-0.800) v2=(0.200,-0.200) det=0.353009  f32 u=0.500004292 v=0.5 u+v=1.00000429 t=250.000015 | f64 u=0.49999808 v=0.500000012 u+v=0.999998093 t=250.000006 | product=0
tri 35 v0=(0.200,-0.200) v1=(0.800,-0.800) v2=(0.800,-0.200) det=0.353009  f32 u=0.5 v=-5.40311521e-06 u+v=0.499994606 t=250.000015 | f64 u=0.499999988 v=-1.90734872e-06 u+v=0.49999808 t=250.000006 | product=0
chooser kind=1 (0 none 1 plane 2 face) plane=0 feature=1 facekind=0
exit=1
```

| Layer | Observation | Verdict |
| --- | --- | --- |
| Pixel | target (0.5, −0.5, 0.6) projects to (500.000, 500.000122) of 1000 × 1000 | correct |
| `buildPickRay` | orthographic, origin on the view plane 250 m out, direction −n̂ exactly | correct; the float origin carries ~1.5e-5 m rounding (ulp at 245 m), a lateral shift far below a pixel |
| `transformRayToLocal` | identity placement | not involved |
| Triangle 34 (0.2,−0.8)(0.8,−0.8)(0.2,−0.2), det 0.353 > 0 (front face) | float32 u+v = **1.00000429** > 1 + 1e-6 → rejected; **double over the same float inputs: u+v = 0.999998093 — INSIDE** | **wrong** |
| Triangle 35 (0.2,−0.2)(0.8,−0.8)(0.8,−0.2), det 0.353 | float32 v = −5.40e-6 → rejected; double v = −1.91e-6 (the rounded ray lies just on triangle 34's side) | correct rejection |
| `pickSceneSnapshot` | `hit = 0` | consequence |
| `SupportChooser::resolve` | `kind = WorldPlane, plane = XY` — the plane BEHIND the floor answers | consequence |

The face is genuinely hit: in exact arithmetic over the very float ray and
vertices the product uses, triangle 34 contains the point (u+v is 1.9e-6
inside). float32 evaluation moves u+v by +6.2e-6 and v by −3.5e-6.

## Near-eye control (same direction, origin d metres from the target)

float32 − double error of triangle 35's v, and whether the product hits:

| d | f32 v | f64 v | error | product hits |
| --- | --- | --- | --- | --- |
| 2 m | 4.22e-8 | −3.97e-8 | 8.2e-8 | tri 34 and 35 |
| 10 m | 3.38e-7 | −1.2e-14 | 3.4e-7 | tri 34 and 35 |
| 50 m | 1.35e-6 | 1.59e-7 | 1.2e-6 | tri 34 and 35 |
| 100 m | −2.70e-6 | −1.99e-7 | 2.5e-6 | tri 34 only |
| **250 m** (the product's orthographic eye) | −5.40e-6 | −1.91e-6 | 3.5e-6 (u+v 6.2e-6) | **neither** |

The error grows linearly with the ray origin's distance from the triangle —
cancellation in `s = origin − v0` and the dot / cross products that follow —
while `kBarycentricEpsilon` is a fixed 1e-6 of the triangle. Past roughly
50–100 m the float32 error exceeds the overlap and a ray on (or within a few
µm of) a shared edge can fall between both triangles.

## Root cause (earliest failing layer)

`intersectRayTriangle` (`forgeshape_picking.cpp:185-233`) evaluates the
barycentric containment in float32 from an origin 250 m away. Ray construction
is correct within its own float precision, scene arbitration and the chooser
only consume the miss. Raising `kBarycentricEpsilon` would not fix it (the
error scales with distance, and the overlap would widen hits beyond the
silhouette at near range); the arithmetic itself is what must not cancel.
