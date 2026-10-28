# CAD-R2-C2 — BEFORE (CPU picking seam)

Written before any product edit, on `feature/cad-r2-c2-picking-seam-r1`
created from exactly `08406cfbbff471f6e53c576c24b45791f903301c` (the CAD-R2-C1
head). Remote refs re-fetched at start: `origin/main` `6c9f156c1df91fe1a55445aed6186105a075ab31`,
R2 `ea24d20805d9487d26221d9dd45e47dc16fb6ecb`, C1 `08406cfbbff471f6e53c576c24b45791f903301c`
(unchanged). Clean tree. C1's last product SHA is `cf340f0`; `08406cf`'s `app/`
tree equals it. Current green baseline: host `HOST_SELFTESTS_OK (4375 checks,
0 failed)`, JVM 196/196, DEVICE union 122/122 (`40752389411` + `40760227555`),
FAST `40766013297`.

## The claimed defect

C1's `HIT_OWNERSHIP.md` ("Pick crack") recorded that a ray through the exact
edge two triangles share can miss both at float precision with a far
orthographic eye: aiming C1_CH_04 at the rectangular pocket floor's centre
(0.5, −0.5, 0.6) gave `pickSceneSnapshot hit = 0`, and the support chooser
then answered with a world plane behind the face.

## The pipeline, as the code states it

pixel → `buildPickRay` (`forgeshape_picking.cpp:46`; an orthographic ray starts
on the view plane at `camera.eye`, which `CameraController::snapshot` places at
`kOrthoViewPlaneDistance = kFarPlane · 0.5 = 250 m` from the target,
`forgeshape_camera.cpp:468`, `forgeshape_camera.h:149`) → `transformRayToLocal`
(`:154`) → `pickTriangleMesh` (`:235`, nearest positive t, first index on an
exact tie) → `intersectRayTriangle` (`:185`, float32 Möller–Trumbore, front
faces only for a closed solid, containment widened by `kBarycentricEpsilon =
1e-6`, `forgeshape_picking.h:106`) → `pickSceneSnapshot`
(`forgeshape_selection.cpp:23`, nearest across bodies) → `SupportChooser::resolve`
(`forgeshape_support_chooser.cpp`, C1 occlusion rule) → `cadFaceRangesFromMesh`
→ semantic `CadFaceRange` → `TopoRef`.

Old invariants that must survive: nearest positive hit; front faces only for
closed solids (two-sided only for `renderBothSides`); `kMinRayDistance`;
degenerate / parallel triangles and non-finite input rejected;
`kBarycentricEpsilon = 1e-6` is deliberate (an arm64 FMA rounding case on a
physical device) and scale-free; a triangle index is never identity; C1's
world-plane occlusion rule.

## Prediction (before measurement)

`kBarycentricEpsilon` is a fraction of a triangle, but the rounding error of a
float32 Möller–Trumbore barycentric is NOT scale-free in the ray origin: `u =
dot(s, p)/det` with `s = origin − v0`, and with |s| ≈ 250 m the cancellation
error is ≈ ε·|s|·|p|/det ≈ 1e-7·250 ≈ several 1e-6 — larger than the 1e-6
overlap. If so, the failing layer is the containment arithmetic of
`intersectRayTriangle`, not ray construction, not scene arbitration, not the
chooser. To be proven by `BEFORE_REPRO.md`, not assumed.

## Repro recipe

`artifacts/cad-r2-c2-picking-seam/repro/seam_diag.cpp` (diagnosis only, linked
against the host objects of this tree): the C1 shop body (2 × 2 × 1 block, a
0.6 boss Added, a 0.6 × 0.6 pocket Cut 0.4 deep at (0.5, −0.5)), camera
`frameSketchView` at (0.5, −0.5, 0.6) looking along −(0.2, 0, 1) — exactly
C1_CH_04's `lookAlong` — viewport 1000 × 1000, the target's own projected
pixel. It prints every layer: eye, pixel, ray, `pickSceneSnapshot`, the two
floor triangles' float32 u / v / det / t next to a double-precision reference
over the SAME float inputs, the product's verdict per triangle, the chooser's
answer; then the same per-triangle data for the same direction with the origin
2 / 10 / 50 / 100 m from the target (near-eye control).
