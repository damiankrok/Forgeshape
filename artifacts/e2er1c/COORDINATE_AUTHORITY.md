# Export-coordinate authority — E2E-R1C

The gate this stage had to pass before one line of exporter was written: what
ForgeShape's world coordinates *are*, decided from repository truth, and what
therefore has to happen to them on the way into a glTF 2.0 file.

**Verdict: PASS, with zero conversion.** ForgeShape's world convention and
glTF 2.0's are the same convention. No axis is swapped, no sign flipped, no
winding reversed and no scale factor applied. That is a finding, not a
convenience: had they differed, this file would have recorded the conflict and
the stage would have stopped.

---

## 1. World up axis — **+Y**

`app/src/main/cpp/forgeshape_transform.h:53` states it as the definition rather
than as a description:

> Right-handed world space with +Y up, column-vector math (`p' = M * p`).
> … THE definition; renderer and picking both obey it.

Corroborated, independently, by:

- `app/src/main/cpp/forgeshape_math.h:85` — the same sentence, plus
  "Storage is column-major (`m[column * 4 + row]`), matching GLSL".
- `app/src/main/cpp/forgeshape_grid.h:33` — the ground plane is built for a
  "+Y-up right-handed world".
- Every primitive generator: a cylinder, cone and capsule are "centred on the
  LOCAL origin, axis along local +Y".

**glTF 2.0:** "glTF uses a right-handed coordinate system. glTF defines +Y as
up." → identical.

## 2. Handedness — **right-handed**

Asserted in the same three headers, and checked rather than taken on trust: the
rotation matrices in `forgeshape_transform.h` were read element by element and
are right-handed (a positive rotation about +Y carries +Z toward +X).

**glTF 2.0:** right-handed. → identical.

## 3. Axis ownership and colour — X/Y/Z = red/green/blue

`forgeshape_gizmo.cpp`'s `canonicalAxis(0|1|2)` returns X, Y and Z, painted from
the red/green/blue palette in that order. The exporter writes nothing that
depends on this; it is recorded because it is the *user-visible* statement of
which axis is which, and it agrees with the headers. Nothing in the product
calls the vertical axis Z.

## 4. Body-local mesh coordinates — **metres, on the body's own origin**

`Meters` is a typedef in `forgeshape_transform.h`; every Construction dimension
is in it and every generated vertex is a `Meters` triple in the body's local
frame, centred on the body's Construction placement origin — which is the pivot
the gizmo, the picker and the renderer already share.

**glTF 2.0:** "The units for all linear distances are meters." → identical, so

> **1.0 ForgeShape metre == 1.0 glTF metre**, and the exporter applies no global
> scale factor of any kind.

`FSR1C-14` and `fsr1c14_metresSurviveUnscaledAndTheUpAxisIsY` assert this from
the far side of an independent reader: a 1 × 2 × 4 m box exports with extent
exactly 1 on X, 2 on **Y** and 4 on Z, centred on ±0.5 / ±1 / ±2.

## 5. Placement transform — `Model = T · Rz · Ry · Rx · S`, column-major

The one composition order in `forgeshape_transform.h`, with `S` LOCAL (a
world-axis scale of a turned body is a shear no diagonal `S` can express).
Storage is `m[column * 4 + row]`, so the translation lives in elements 12, 13,
14.

**glTF 2.0:** a node's transform may be one column-major `matrix`, or any of
`translation` / `rotation` / `scale`. → the composition order and the storage
order agree exactly, so no re-ordering or transposition is needed in either
form.

> **Superseded by `ARCH-OWNER-07` (2026-08-31).** This file originally recorded
> that the exporter copies `modelMatrix()` into the node verbatim. It no longer
> does: the placement is now SPLIT as `Model = T · L`, `L` is baked into the
> vertices and the node carries `translation` alone. What that decision rests
> on is the agreement recorded above — the split is possible precisely BECAUSE
> the two conventions match, and it changes no axis, no unit and no
> handedness. Sections 1, 2, 3, 4 and 6 stand exactly as written.

## 6. Winding and normals — **counter-clockwise seen from outside**

`ARCHITECTURE.md`, "Canonical winding and culling": front faces are
counter-clockwise from outside, and the pipeline is built with
`VK_FRONT_FACE_COUNTER_CLOCKWISE`. Normals are the crease-policy normals
`buildRenderMesh` derives (`kCreaseAngleDegrees = 40°`), and are presentation
products of a published `RuntimeMesh` — never truth.

**glTF 2.0:** "the winding order determines front- and back-facing … counter-
clockwise ... is front-facing", and normals must be unit length. → identical.
`fsr1c16_normalsAreUnitLengthAndTrianglesWindOutward` re-derives every face
normal from the exported indices and checks it points away from the body's own
origin.

---

## What follows from all six

The conversion this exporter performs is the identity. Every candidate for a
"fixer" — a root node with a −90° X rotation, a 0.01 metre/centimetre factor, a
Z-up transpose, a reversed index triple — would be a *defect* here, not a
correction, and each is asserted against:

| Would-be conversion | The case that would catch it |
| --- | --- |
| Y-up → Z-up node | `fsr1c14_noConversionNodeIsInsertedAboveTheBody` (no node matrix at all; one translation per body, never one more) |
| any global scale | `fsr1cC1_12_metresAndTheUpAxisAreUnchangedByTheBake` |
| translation scaled, or moved into the vertices | `fsr1cC1_01to03_theNodeCarriesTranslationOnlyWithIdentityRotationAndScale` |
| rotation or scale left on the node | the same case (both absent, checked on the node and on the raw JSON) |
| rotation or scale NOT baked | `fsr1cC1_04_rotationAndScaleAreBakedIntoTheGeometry` |
| geometry recentred by the bake | `fsr1cC1_05_bakingHappensAboutTheLocalOriginAndNothingIsRecentred` (on a cone; a symmetric primitive would hide it) |
| normals carried by `L` instead of `transpose(inverse(L))` | `fsr1cC1_06_normalsRideTheInverseTransposeUnderNonUniformScale` and `..._bakedNormalsStayPerpendicularToTheirFaces` |
| stale accessor bounds | `fsr1cC1_07_positionBoundsAreRecomputedFromTheBakedVertices` |
| a silent mirror | `FSR1C_C1_08_a_mirrored_bake_is_refused_and_never_compensated` (native) |
| reversed winding | `fsr1c16_normalsAreUnitLengthAndTrianglesWindOutward` |
