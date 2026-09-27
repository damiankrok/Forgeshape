# CAD Vertical Slice R1 — current truth before the change

This page records what the CAD product **was** at the start of this work,
read from the source at `103aa22` (Stage027 close-out) and measured on the
cloud emulator at `2380623` (the Phase 0 test-only commit). Everything below
describes the state *before* any change was made. Anything not confirmed by
reading the source or running the device is marked UNVERIFIED.

The device numbers come from CI DEVICE run `36329102110` on `2380623`, which
ran 4 of 4 reproduction tests with 0 failures. The raw measurements are in
`before/facts.txt` and the frames are `before/01..06`.

## 1. The model

- **A CAD Body was ONE sketch plus ONE extrusion.** Its state was a plain
  struct, `CadBodyState = {CadSketch, ExtrudeFeature}`. The header said this
  was deliberate, "so nothing pretends a feature tree exists"
  (`forgeshape_cad_body.h`). `ExtrudeFeature.profileEntityId` named one
  profile by one anchor entity id.
- **Mesh generation did not use a solid kernel.** `generateCadMesh`
  triangulated one simple polygon (ear clipping), then emitted two caps and
  one quad per profile edge in float. There was no boolean, no hole and no
  second feature. The hard rule "no third-party runtime, rendering, math or
  input library" held with no exception.
- **Face identity.** The faces were `CapPlane`, `CapFar` and one `Side` per
  profile edge, each named by `CadFaceToken`. Every face was assumed to belong
  to feature 1 (`kCadFeatureId`). A face sketch's `TopoRef` stored the
  producer's id, `producerLocalFeatureId = 1`, the token, and the §7c lineage
  signature of the producer's whole state.

## 2. Profiles (the owner's rectangle-with-a-circle finding)

- `extractClosedProfiles` tested every ordered pair of loops. When every vertex
  test put one loop strictly inside another and no segment touched, the
  **OUTER** loop was removed as `NestedProfileUnsupported` and the inner loop
  was kept.
- `SketchSession::finish` auto-selected the only surviving profile whenever
  exactly one survived. It ignored the rejections.
- **Measured** (`before/04`, `facts.txt`) for a rectangle around a centred
  circle:
  - Finish Sketch reported `profile_count=1`.
  - It auto-chose `chosen_profile_anchor=2` (the circle, `kind=2`), with area
    1.998 m².
  - The disk was what would be extruded. The rectangle-minus-circle could not
    be chosen at all.
- Profiles were identified by anchor entity id alone. No region or loop-set
  identity existed.

## 3. Operations (the owner's Add/Cut finding)

- **New Body was the only operation.** `SketchSession::commit` always went
  through `ConstructionScene::addCadBody`, which minted a new `SceneObject`.
  The only way to change an existing body was `commitEdit`, which replaces the
  body's whole single state.
- **Measured** (`before/05`, `before/06`):
  - A sketch on a planar face of body 2 finished with the badge reading
    `New Body` (`operation_badge_clickable=false`,
    `add_or_cut_control_present=false`).
  - Extruding it created body 3 (`bodies_after_base=2`,
    `bodies_after_second=3`).
  - Same-body Add and Cut did not exist.

## 4. The canvas extrude cluster (the owner's "the cluster is huge" finding)

Measured at One Side on a 1080 × 2400 viewport (density 2.625, control scale
0.9505), from `facts.txt`:

| Element | Bounds (px) | Share of viewport |
| --- | --- | --- |
| Whole cluster | `[0,1163][1027,1334]`, 1027 × 171 | **6.78 %** — nearly the full width |
| One Side / Symmetric / Two Sides chips | 198 / 229 / 222 × 158 | 1.21 / 1.40 / 1.35 % |
| Exact value | 114 × 158 | 0.69 % |
| Flip | 158 × 158 | 0.96 % |
| Operation badge (plain text, not a control) | 82 × 145 | 0.46 % |

- The extent chips were TEXT pills 60.19 dp tall. `R.dimen.cad_canvas_control`
  was 60 dp, and `setScaleX/Y` on each child scaled the glyph and the touch
  target together.
- The exact value stood **99.49 dp** from the arrow shaft's midpoint it
  measures: the cluster was centred on that anchor as a whole, not on the
  value.

## 5. Chrome in Ready (the owner's "the sketch UI stays up" finding)

Measured in Ready, after Finish Sketch:

- **Orientation navigator:** still shown, `[442,336][859,691]`, 5.71 % of the
  viewport. `refreshSketchViewportSurfaces` showed it whenever a session was
  open, Ready included.
- **Sketch Tool Rail:** still shown, `[880,231][1059,1576]`, 9.29 %. It held all
  seven drawing tools, although nothing could be drawn in Ready.
- **Precision panel:** auto-opened by `onFinishSketchRequested`
  (`setPrecisionOpen(true)`), `[21,1597][1059,2316]`, **28.79 %** of the
  viewport.
- **Selected-Line dimension label:** still shown in Ready if a Line stayed
  selected. From the source only; not in the measured frames.
- **Sketch grid and axes:** still drawn in Ready. From the source.

## 6. Persistence

- `.forge` `CADB` had four versions:
  - v1: world plane.
  - v2: face support (`TopoRef`).
  - v3: arc and spline.
  - v4: extent.
- Each version is a superset of the one below and is written only when needed.
- The committed corpus held **36** fixtures, and CI FAST's parity step
  regenerated all 36 with the independent PowerShell encoder.
- The lineage token (§7c) was FNV-1a 64 over the profile anchor id, the face
  count, and each face's token code and eligibility.

## 7. History and verification

- **History.** One Construction history. Creating or editing a CAD body was one
  `ScopedConstructionEdit`, and so one Undo. The sketch session was volatile.
- **Startup self-tests.** A clean debug launch emitted **twenty-two**
  `*_SELFTEST_OK` tokens, then `FORGESHAPE_NATIVE_VIEWPORT_OK`.
- **Device tests.** The existing classes pinned the pre-change contract, among
  them:
  - `CadCanvasExtrudeTest` (60 dp arithmetic, `New Body` text, no Add/Cut);
  - `SketchExtrudeTest` (the precision surface opens itself at Finish);
  - `SketchUxTest` / `HomeFlowTest` (navigator in the sketch);
  - the native `CADR0_21_nested_outer_profile_refused_inner_kept` (outer loop
    refused, inner loop kept).

## 8. What the environment allows

- **Cloud CI.** `CI FAST` (build, JVM tests, release guard, corpus parity) and
  `CI DEVICE` (a fresh API 36 x86_64 emulator, the 22 startup tokens and one
  focused instrumented class) run on GitHub-hosted runners.
- **Full suite.** `-FullSharded` is a Windows PowerShell runner driving an
  isolated local AVD. It does **not** exist in the cloud, and this session has
  no Windows host, so the milestone's full aggregate cannot be produced here.
