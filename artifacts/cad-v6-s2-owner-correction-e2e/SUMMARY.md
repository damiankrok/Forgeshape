# CAD-V6-S2-OWNER-CORRECTION-E2E-R1 — `PASS-CAD-V6-S2-OWNER-CORRECTION-E2E`

TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / NOT MERGED. Task branch
`feature/cad-v6-s2-owner-correction-e2e-r1`, created at `b264cfe` (R2 HEAD).
`main` stays `6c9f156`; `feature/cad-v6-sketch-face-r1` stays `bbae765`. No
FullSharded run; S3 not started.

## Final candidate

| fact | value |
| --- | --- |
| final product SHA | `8ca3e6b` (`7ac555c` adds only evidence whitespace; `app/`, `scripts/`, `.github/` identical) |
| final focused DEVICE | `CI DEVICE` `36999488652` on `8ca3e6b` — PASS, **82/82** (11 classes) |
| final CI FAST | `CI FAST` `37001980955` on `7ac555c` — success |
| APK | artifact `ci-fast-evidence` id `11224257836`, path `app/build/outputs/apk/debug/app-debug.apk`, **13,078,746 bytes**, SHA-256 `431e233b6a33fa8dde8210a6f3291458629d6830dd008cfb91f8810ac5d948b1`, expires 2026-10-16T11:42:07Z |

## Phase A — fill/pick R2

**Starting defect.** DEV-R2-04's harness chose a pixel 15–35% along the
projected shaft that only proved it was on the viewport and under no chrome;
attempt `36987358382` tapped empty space there (`FORGESHAPE_SKETCH_TAP:exterior`).

**Harness fix (test-only, `746cdaa`).** The search is inverted: the handle of
every reachable cell is learned by a real still tap; dense interior samples of
those cells (0.05-unit step, 0.3 unit from every boundary, the spline loop
excluded) are projected with `sketchScreenPoint` and kept only when the pixel
lies ON the drawn shaft (≤ 3 px, 8–75% base→tip), at least 12 dp clear of the
head's own still-tap claim, on the viewport and under no clickable view. The
pose, cell, UV, pixel, shaft distance, head clearance and covering-view result
are recorded before anything is dispatched. The same pixel takes a jittered
still tap (exactly that cell toggles, `resolved`, depth unchanged), the head a
still tap (`arrow_head`, nothing toggles), and the same pixel a drag (depth
changes, no toggle). No product change was needed in Phase A.

**Device attempts (Phase A).**

| run | candidate | result | cause / action |
| --- | --- | --- | --- |
| `36984456789` | `e59a83d` | FAIL 35/37 | earlier R2 attempt (pre-task) |
| `36987358382` | `b264cfe` | FAIL 36/37 | DEV-R2-04 harness: arbitrary shaft pixel → `exterior` (pre-task) |
| `36992759739` | `746cdaa` | **PASS 6/6** (`CadFillPickR2Test`) | inverted search: REST pixel 0.002 px off the shaft, head clearance 157 px |
| `36993839597` | `746cdaa` | `DEVICE_STARTUP_UNRESOLVED`, 0 tests | pre-test infra: 3 startup captures 22/23 with liblog writer drops (load 3.96), 0 failure lines → same-candidate retry |
| `36995556354` | `746cdaa` | **PASS 37/37** (Phase-A union) | — |

## Phase B — HUD3D

**Architecture.** Native `cadExtrudeDockFor` (`forgeshape_cad_extrude_tool`)
frames a WORLD rectangle; `SketchSession::extrudeDock` reads it from the same
anchors and the same one control scale as the arrow; JNI exports it in
`cadExtrudeToolState` slots 45..58 (visible, alpha, 4 corners in reading order,
centre, sine, hide reason; the doc for 42..44 completed). Android
`CadExtrudeDockView` draws one badge into the quad through a 4-point
homography (`CadHud3dPresentation`, pure, JVM-pinned) and is the one touch
target. The screen-space panel (plate, three glyphs, edge slide, 0.35×
rotation, scale clamp, `layoutPanel`/`slideRange`/`panelRotationDegrees`, the
`bg_hud_panel` drawable and their JVM tests) is retired.

- **Frame.** `a` = primary axis; `m` = view direction made ⊥ `a`;
  `s = cross(a, m)`; when `|m|` collapses, sketch `u` then camera right, each
  made ⊥ `a`. Reading orientation by the leader's rule with an 8° vertical band
  (inside it the shaft reads bottom→top, so pixel noise cannot flip it); the
  quad is never mirrored. No state between frames.
- **Projection / quad.** Centre on the axis, past `cadExtrudeArrowPoint` by
  (24 + 4) reference units of screen gap; half-size across
  `clamp(0.26 × control pixels, 14, 24 units)`, along the axis that over
  `sqrt(max(sin, 0.35))` (modest foreshortening).
- **Hide policy.** Whole dock hidden at sin ≤ 0.35 (`kCadFeatureViewMinAxisSine`,
  linear fade-in to 0.45), when any corner or the centre is behind the eye, or
  when any corner is off the viewport. Never slid, clamped or re-sided.
- **Scale.** The one per-frame control scale read at the manipulator base.
- **Touch / accessibility.** Claims only the quad ∪ the 48 dp floor square on
  its centre; a Down elsewhere in its box is declined to the viewport. One
  focusable view (`cad_extrude_panel`, the act of opening the palette); its
  description names the operation, the extent and any refusal reason. Debug
  attribution `FORGESHAPE_CAD_HUD_TOUCH:dock` replaces `:panel`.
- **Density.** One operation badge (shape + tint + refusal ring). Extent,
  operations and Flip live in the existing full-size palette.
- **Dimension.** Leader and value untouched; the value keeps its own collapse
  rule and the dock no longer collapses with it.

**Device attempts (Phase B).**

| run | candidate | result | cause / action |
| --- | --- | --- | --- |
| `36996985535` | `6495115` | FAIL 55/56 | DEV-HUD3D-05 sampled uv (−1.8, −0.92), ON the square's edge → `exterior` (the dock correctly did not take it); test now samples 0.3 m inside. Screenshots showed the plate as a disc (fixed 18 dp corner), hiding the tilt → own rounded plate (22% corner, tinted edge), badge band 14..24 units |
| `36999488652` | `8ca3e6b` | **PASS 82/82** | final union below |

Final union classes: `CadFillPickR2Test`, `CadPlanarFaceOwnerCorrectionTest`,
`CadPlanarFaceRuntimeTest`, `CadHud3dOwnerTest`, `CadCanvasExtrudeTest`,
`SketchExtrudeTest`, `JniBoundaryHardeningTest` (tool-state array grew),
`CadVerticalSliceTest`, `Ui3dStateAuditTest`, `Ui3dStateCorrectionTest`,
`CadExtrudeExtentTest`.

Device facts (`hud3d-device-facts-36999488652.txt`): orbit 126/126 frames
drawn and attached, largest frame-to-frame offset jump 4.7 dp, largest reach
from the arrow point 45.3 dp (no rescue slide), 2 in-place 180° turns at the
vertical band, value on its leader 126/126; near the axis the dock fades below
sin 0.45 (alpha 0.93 at 0.443, 0.13 at 0.363) and hides (`near_axis`) at sin
0.343, with no flip; a real tap on
the dock logs `HUD_TOUCH:dock` and opens the full-size palette; a cell just
outside the dock's claim resolves (`SKETCH_TAP:resolved`, no `:dock`). R2-04 on
the final candidate: REST pixel 0.64 px off the shaft, `resolved`,
`arrow_head`, drag 1.5 → 1.88 m. Screenshots: `screens/` (oblique left/right,
near edge, near axis, palette open).

## Local gates on the final product (`8ca3e6b`)

- Host `HOST_SELFTESTS_OK (4005 checks, 0 failed)` (CAD_FEATURE 376, incl.
  HUD3D_01..06, 08..10).
- JVM 120/120 (`CadHud3dPresentationTest` = HUD3D-07 and the homography).
- `assembleDebug`, `assembleDebugAndroidTest`, `assembleRelease` green.
- Release guard PASS (release 0/0 both ABIs; debug 26 symbols / 392 strings).
- Fixture verdicts 57/57 unchanged (`CORPUS_VERDICTS_UNCHANGED`).
- Corpus byte parity 57/57 proven by CI FAST (`committed=57 generated=57
  mismatches=0`): local `pwsh` hangs in this container even for
  `Write-Output`, so the PowerShell encoder ran only on the CI VM.
- `git diff --check` clean over the task range and the main range.
- No `.forge` format, section, version or fixture change.

## OWNER physical review checklist

1. OWNER mixed sketch: tap all cells in two different orders; every cell adds
   and removes freely; cells touching at a point both stay chosen.
2. Wobble a finger while tapping cells from above and below the sketch plane:
   no orbit, the cell under the finger toggles.
3. Still tap on the arrow shaft over a cell → that cell toggles; drag from the
   same spot → depth changes; tap the arrow head → nothing toggles.
4. Orbit slowly round a staged extrusion: the badge stays on the arrow line
   past the head, tilts with it, never jumps sides or slides along an edge.
5. Zoom in until the badge would leave the screen: it disappears whole rather
   than moving.
6. Look nearly straight down the arrow: the badge fades and disappears without
   turning over.
7. Tap the badge: the full-size palette opens; extent, operation and Flip work.
8. Tap a cell just beside the badge: the cell toggles, the palette does not open.
9. The dimension value stays above its leader throughout.
10. Judge the badge size, corner and tilt (OWNER-TUNABLE).
