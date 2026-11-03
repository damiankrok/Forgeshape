# CAD-V6-REVOLVE-NEWBODY-E2E-R1 — `PASS-CAD-V6-REVOLVE-NEWBODY-E2E`

TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / NOT MERGED. Task branch
`feature/cad-v6-revolve-newbody-r1`, created at `3e2443f` (the MULTIFACE head).
`main` stays `6c9f156`, `feature/cad-v6-sketch-face-r1` stays `bbae765`, the
MULTIFACE branch stays `3e2443f`. No merge, no FullSharded, S3 not started.
Read `BEFORE.md` first (the source audit and the architecture decision).

## Final candidate

| fact | value |
| --- | --- |
| final product SHA | `6507f2b` (later commits add only docs and artifacts: `app/`, `scripts/`, `.github/`, `testdata/` identical) |
| final focused DEVICE | `CI DEVICE` `37391067580` on `6507f2b` — **PASS 83/83** (11 classes), 23/23 startup tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure tokens |
| final CI FAST | `CI FAST` `37391069710` on `6507f2b` — success: debug, release and androidTest builds, JVM, release self-test guard PASS, corpus parity **61/61**, device-free runner/guard checks, `git diff --check` |
| APK | artifact `ci-fast-evidence` id `11381820030` (zip 9,619,744 bytes, zip SHA-256 `fe5b49289ecdcb5e9d7fd184373fce8f532e311790a1e2d45e74d5d846d4ba80`, expires 2026-10-19T23:59:13Z); inside it `app/build/outputs/apk/debug/app-debug.apk`, **13,647,374 bytes**, SHA-256 **`d41ad54af67f86e8d92177b1e542b932ee15ac3e1c4c836b02deecd91a2f99f6`** — downloaded and hashed from that artifact, not rebuilt |

## What was built

**Durable model** (`forgeshape_cad_body.{h,cpp}`). `CadFeatureKind {Extrude,
Revolve}` on `CadBodyState::baseKind`; `RevolveFeature` = retained sketch id,
the SAME selection an Extrude makes (`LoopRegions` or `PlanarFaces`, lent to
the region code through a transient carrier), the axis
`CadSketchEdgeRef{entityId, edgeLocalIndex}` (Line edge 0, Polyline segment i,
Rectangle side i), `angleDegrees` (binary64, `[0.001, 360]`, default 360, full
turn iff == 360) and `RevolveDirection`. R1 is New Body only and base-only.
Twelve new named refusals (codes 62..73): `InvalidFeatureKind`,
`RevolveAxisUnresolved`, `RevolveAxisNotStraight`, `RevolveAxisDegenerate`,
`RevolveAngleInvalid`, `RevolveDirectionInvalid`, `RevolveProfileCrossesAxis`,
`RevolveZeroRadius`, `RevolveComponentsOverlap`, `RevolvePayloadMismatch`,
`RevolveLaterFeatureUnsupported`, `RevolveNeedsAxis`.

**Geometry** (`forgeshape_cad_revolve.{h,cpp}`). Camera-free; 32 steps for a
full turn (the circle's own density); full turn wraps the ring with no caps and
no seam duplicate; partial turn has two triangulated caps; on-axis vertices are
apexes (one per fan on a full turn) so the mesh stays 2-manifold; holes and
several non-overlapping components supported; crossing the infinite axis line
refused (exact for circle/arc sub-arcs, bounded for spline spans); touching
legal; opposite-side components that could meet refused. Every revolved solid
passes `cadKernelValidateSolid` in `regenerateCadBody`. Faces derived, all
ineligible as supports in R1.

**Session / JNI / UI.** Revolve is volatile Ready-state intent in
`SketchSession`; axis by a real tap (Down pixel, Down camera, the Select tool's
hit tolerance); `revolveViewPose` leans to a 3/4 view once an axis is set; the
ring (axis, arc, spokes, diamond handle) is `SketchOverlay` geometry, and a
handle drag freezes the ring, writes whole degrees in [1, 360], never orbits,
and is restored by a second pointer or Cancel. JNI: `cadRevolveToolState`
(24 slots) + 6 acts; `cadFeatureInfo` / `cadState` carry kind, angle,
direction. Android: precision surface Revolve section (`Revolve…`, axis
summary, Angle, Flip, Change axis, Extrude instead, pinned Revolve), toolbar
`Revolve` commit (withdrawn while refused), canvas angle label/editor
(`CadRevolveAngleLabelView`), feature row "1. Revolve — N°" reopening it.

**Format.** `CADB` v7 (`DATA_PACKAGE_SPEC.md` §7h): v6 layout with a kind byte
after each feature id and the Revolve payload; written only when a body
revolves; fingerprint block `REV7` only for revolved bodies. Fixtures
`cad_revolve_full_v7` (332 B, `1e6d0214…d450`), `cad_revolve_partial_v7`
(`7b72037a…2fdd2`), `cad_revolve_bad_axis_v7` (`8be1bee7…5ecd1`,
`InvalidSemanticValue`), `cad_bad_feature_kind_v7` (`404e6485…a9fb5`,
`InvalidSemanticValue`). C++ writer and PowerShell builder agree byte for
byte; the 57 older fixtures' bytes and verdicts are unchanged
(`corpus-verdicts-baseline.txt` vs `corpus-verdicts-after.txt`: exactly the
four new lines). CI FAST parity count 57 → 61.

## Tests

- **Host** (inside the CAD-feature suite, no new startup token): `REV_01..25`,
  `REV_S01..S06` (session: axis pick, angle, Flip, drag, commit / Undo / Redo /
  edit / first project), `REV_FMT_02..10`. Aggregate `HOST_SELFTESTS_OK (4064
  checks, 0 failed)`. Timings (`FORGESHAPE_CAD_FEATURE_PERFORMANCE`):
  `revolve_disk_max_us≈3253` (2048 triangles), `revolve_square_max_us≈441`.
- **JVM** `CadRevolvePresentationTest` (entry/commit/label visibility, exact
  degree formatting and parsing, 28 dp handle proxy and nearby-tap rule, ring
  arc continuity); suite 128/128.
- **Device** `CadRevolveOwnerTest` (real window MotionEvents):

| case | proven on device |
| --- | --- |
| DEV-REV-01 | New Project → CAD, square + Line, Finish, Revolve…, real axis tap (`FORGESHAPE_REVOLVE_AXIS_TAP:2:0`, `SKETCH_TAP:resolved`), 360 preview 3.19636 m³ (Pappus 3.21699, 32-step ratio 0.99359), commit creates the project, body volume equals the preview |
| DEV-REV-02 | typed 90° commits; feature row reopens it (`1. Revolve — 90°`), typed 180°, one Undo step, same body |
| DEV-REV-03 | ring-handle drag 90° → 45°, camera pose identical before/after; a tap on the square beside the ring toggles the area, angle untouched |
| DEV-REV-04 | Flip: same volume 0.79909 m³, z range mirrored (0..1.2 ↔ −1.2..0), Flip twice identical |
| DEV-REV-05 | axis switched to the square's own side (edge 3): cylinder 1.59818 m³ (Pappus 1.60850) |
| DEV-REV-06 | crossing axis refused `RevolveProfileCrossesAxis` with its message, no commit drawn, nothing created; a valid axis then commits |
| DEV-REV-07 | 360 → 37.5° Negative; Undo restores 360/Positive and the exact volume, Redo 37.5/Negative |
| DEV-REV-08 | saved bytes are `CADB` v7 (332 B); reload restores body, angle, direction, axis ref `{2, 0}`, empty history; Cancel leaves bytes identical |
| DEV-REV-09 | Extrude path unchanged (HUD value shown, commit is Extrude, bytes < v7); MULTIFACE >16 proven by `CadMultiFaceOwnerTest` in the same union |

Screenshots (`device-37391067580/`): `01_360_preview`, `02_90_preview`,
`03_axis_highlight`, `04_angle_label`, `05_committed_body`,
`06_reopened_revolve`; facts in `device-37391067580/facts.txt` and
`device-facts-37388002146.txt`.

## DEVICE attempts

| run | candidate | classes | result | cause / action |
| --- | --- | --- | --- | --- |
| `37386653188` | `942e9e0` | 1 | **PASS 9/9** | `CadRevolveOwnerTest` alone |
| `37388002146` | `7a8beb6` | 11 | FAIL 82/83 | `CadCanvasExtrudeTest#e2eCaduxs1_08`: a pre-Revolve assertion forbade the word "Revolve" anywhere in the sketch panel; narrowed to the canvas cluster (`6507f2b`) |
| `37391067580` | `6507f2b` | 11 | **PASS 83/83** | final union |

Final union: `CadRevolveOwnerTest`, `CadMultiFaceOwnerTest`,
`CadFillPickR2Test`, `CadPlanarFaceOwnerCorrectionTest`,
`CadPlanarFaceRuntimeTest`, `CadHud3dOwnerTest`, `CadCanvasExtrudeTest`,
`SketchExtrudeTest`, `JniBoundaryHardeningTest` (the JNI surface grew),
`HomeFlowTest` (first-project path), `CadVerticalSliceTest` (feature list).

CI FAST: `37386656144` on `942e9e0` success; `37391069710` on `6507f2b` success.

## Not done / not claimed

- No physical-device evidence: stylus, real GPU, real-device performance and
  the ring/handle sizes on a phone are the OWNER's review. Emulator CI closes no
  physical gate.
- Not built (by scope): Revolve Add/Cut, a later Revolve, sketch on a revolved
  face, an axis that is not a sketch edge, symmetric/two-sided sweeps.
- Local environment note: the container's `pwsh` launcher was a self-recursive
  stub; the PowerShell corpus builder was run through a scratch host for
  `libhostfxr.so`. CI FAST re-ran the builder independently (61/61).

## OWNER review checklist

1. New Project → CAD; draw a rectangle and a separate vertical Line; Finish.
2. Open the exact fields (sliders icon) → **Revolve…**; the status asks for an
   axis; tap the Line. The view tilts; a full tube previews with the ring,
   handle and "360°".
3. Drag the handle round: the angle follows in whole degrees, the camera does
   not move. Tap the angle, type 37.5, Apply: exactly 37.5°.
4. Flip direction: the sweep goes the other way, same angle. Change axis → tap a
   rectangle side: a solid cylinder.
5. Add a short line whose extension crosses the rectangle and pick it as the
   axis: refused by name, no Revolve button.
6. Revolve (toolbar): one body; Undo/Redo; the body's panel lists "Revolve";
   tap it, change the angle, finish: one Undo step.
7. Save, reopen: same body, angle, direction and axis.
8. Regression: a fill selection past 16 cells still extrudes (MULTIFACE
   checklist), the extrude dock/HUD behaves as before.
