# CAD-FOUNDATION-C1 — union of regions and a technical-leader extrude HUD

Baseline `origin/main = 509de02`. Branch `feature/cad-foundation-c1-union-hud-r1`.
Tested product candidate: `1fa1bec` (product `262cfc1` + the test-only
correction `1fa1bec`; docs-only commits follow it). BEFORE evidence: `BEFORE.md` in this directory.

## What changed

| Area | Change | Owner |
| --- | --- | --- |
| S3 one scale fact | `cadExtrudeManipulatorScale` reads `metersPerPixel` ONCE at the arrow's base; `SketchSession::extrudeViewFacts` hands the same fact to the overlay, the tool-state slot and the chip; the hit test calls the same function. Without the frame's facts no arrow is drawn. | `forgeshape_cad_extrude_tool`, `forgeshape_sketch_session`, `forgeshape_jni` (render path, tool state, chip) |
| S4 overlay revision | A rebuild caused by another `worldPerUnit` or other view facts advances `overlayRevision_`; an authored change still bumps once. | `SketchSession::overlay` |
| Region union | `mergeSelectedRegions`: a loop bounds the union exactly when selection membership differs across it. `validateRegionSelection` allows a region beside its OWN direct hole and keeps every touch/cross and material-overlap refusal. `toggleRegion` is a pure toggle; an unmergeable addition is refused by name. Faces, lineage token, solid (one prism per component), components count, preview hatch/edges and arrow anchor read the union. | `forgeshape_sketch_region`, `forgeshape_cad_feature`, `forgeshape_cad_body`, `forgeshape_sketch_session`, `forgeshape_cad_extrude_tool` |
| Leader HUD (native) | Dimension leader (extension lines, dimension line, 45° ticks) in the `Dimension` overlay range, on the reading-up side of the shaft; band 0.40..1.60; tool-state slots 32..41. | `forgeshape_cad_extrude_tool`, `forgeshape_sketch_session`, `forgeshape_jni` |
| Leader HUD (Android) | No row, no capsule. Invisible unscaled ≥ 48 dp proxies placed on the projected leader; glyph `28 dp × clamp(scale, 0.40, 1.60)` with a disc its own size; value text above the leader, rotated and upright, `clamp(14 sp × scale, 11, 18)`; off-screen glyphs hidden; palettes stay screen chrome and carry Tool Labels captions. | `CadHudPresentation` (pure policy), `CadExtrudeCanvasView` (placement only), `bg_hud_glyph*.xml` |

`EditorWorkspaceView` is unchanged (0 lines). `forgeshape_jni.cpp` grew by 55
lines (the frame's view facts, slots 32..41, the chip's scale call).

## Rectangle O + circles A, B — truth table (native `CADFC1_REG_01..07`, device J1/J2)

| Selection | Union components | Area |
| --- | --- | --- |
| O | O with holes A, B | rect − A − B |
| A | disk A | A |
| B | disk B | B |
| O+A | O with hole B | rect − B |
| O+B | O with hole A | rect − A |
| A+B | disk A, disk B (two components) | A + B |
| O+A+B | O, no hole | rect |

## Persistence

No `CADB` byte, section or version changed. The stored selection is still the
atomic `ProfileRegionRef` list (`CADFC1_PER_02`), a union selection round-trips
as the unchanged v5 record (`CADFC1_PER_03`), a nesting edit under it is still
`ProfileRegionMismatch` (`CADFC1_PER_01`). `forgeshape_project_document.*`,
`scripts/build-forge-corpus.ps1` and `testdata/` are untouched; the independent
corpus regenerates 44/44 byte-identical (local and `CI FAST`); the project
suite's golden digests are unchanged. `DATA_PACKAGE_SPEC.md` states the union
reading and that an older build refuses a region-beside-its-hole list by name.

## Evidence

| Gate | Where | Result |
| --- | --- | --- |
| Tier 0 | `git diff --check` over every commit | clean |
| Tier 1 native | `scripts/host-native-selftests.sh` (all 23 suites on the host) | `HOST_SELFTESTS_OK (3797 checks, 0 failed)`; `CAD_FEATURE` 168, `SKETCH_UX` 118, `CAD` 155, `CAD_A3` 66, `PROJECT` 256. Baseline was 3757. |
| Tier 1 JVM | `:app:testDebugUnitTest` | 112 tests, 0 failures; `CadHudPresentationTest` 24 |
| Compile | `:app:compileDebugAndroidTestJavaWithJavac`, `:app:assembleDebug` | green |
| Corpus | `build-forge-corpus.ps1` regenerated locally and in CI | 44/44 byte-identical |
| `CI FAST` | `36725810385` on `262cfc1`; `36728188104` on `1fa1bec` | success, success (`FORGE_CORPUS_PARITY=PASS (44/44)`) |
| `CI DEVICE` attempt 1 | `36725813941` on `262cfc1`, union list of five classes | `FAIL-CI-CLOUD-DEVICE-PRODUCT`: 45 run, 3 failed, 480 s — all three test-side (below) |
| `CI DEVICE` attempt 2 | `36728183647` on `1fa1bec`, the same union list | **PASS**: 23/23 startup tokens in order (capture 2; capture 1 had a proven liblog drop of 262 lines and was relaunched by policy), `OK (45 tests)`, 507 s instrumentation, about 16.5 min wall |

Union list, feature under test first: `CadVerticalSliceTest`,
`CadCanvasExtrudeTest`, `SketchExtrudeTest`, `CadExtrudeExtentTest`,
`Ui3dStateCorrectionTest`. `JniBoundaryHardeningTest` was not added: no JNI
signature changed (`cadExtrudeToolState` takes the same `double[]`, now read to
42 slots). `Ui3dStateAuditTest` (a recorder that asserts only its harness) was
updated and compiled but not run.

Attempt 1's three failures, each root-caused and fixed in the TEST (`1fa1bec`),
none in the product:

1. `owner_rectangle_circle_region` — with the ring and the disk chosen, the
   union's arrow stands at the rectangle's centre, on the disk; the test's tap
   at (0,0) pressed the arrow (a drag), not the disk. Taps now pick the region
   point farthest from the projected shaft.
2. `merged_selection_feeds_same_body_add_and_cut` — a 0.2 m circle on the face
   sketch's framing placed nothing (`entities` stayed 1). Larger geometry, and
   the entity count is asserted before Finish.
3. `Ui3dStateCorrectionTest.ui3dc1_11` still pinned the old 0.80 floor.

Device measurements from attempt 1 (the same product bytes; attempt 2's
markers were dropped by liblog, its assertions passed):

- J1 `owner_rectangle_two_circles_union`: rectangle 11.2 m², A = B = 0.4994 m²;
  committed O+A body 5.350284 m³ = (11.2 − 0.4994) × 0.5.
- J5 `cadFoundationC1_theLeaderHudShrinksWithZoomAndKeepsItsProxies`
  (Perspective, distances 5/9/16/30): scale 1.559 / 0.866 / 0.487 / 0.400,
  glyph 115 / 64 / 36 / 29 px, value 18 / 12.1 / 11 / 11 sp; every proxy
  ≥ 48 dp; the value on its leader at each step.
- `compact_extrude_hud`: extent, operation and Flip proxies 126 × 126 px
  (48 dp at 420 dpi) on the leader's line; the value rotated −28.1°, 13.3 sp.

**FullSharded NOT RUN** (TEST-OWNER-04: focused evidence only).

## OWNER review APK

- Built by `CI FAST` `36728188104` from `1fa1bec` (the tested product candidate;
  `1fa1bec` differs from `262cfc1` in androidTest sources only).
- `app/build/outputs/apk/debug/app-debug.apk`, **10,879,410 bytes**,
  SHA-256 **`a600d784e4ed422a4ee92f3e3c7a50f683e9bcb7a837b7d552a767f44316d35b`**.
- Durable copies: the `ci-fast-evidence` artifact `11103603365` of that run
  (expires 2026-10-14), and the file handed to the OWNER in the session.
- This is emulator evidence only; it closes no physical-device gate.

## OWNER review (physical device) — OWNER REVIEW REQUIRED

Provisional, OWNER-TUNABLE values: band 0.40..1.60; glyph 28 dp at scale 1;
value 14 sp in 11..18 sp; value gap 3 dp; glyph gap 4 dp; leader offset 0.55,
extension gap 0.12, overshoot 0.12, tick 0.10 (fractions of the control world
size). Checklist:

1. rectangle + two circles selection;
2. O+A leaves only B as a hole;
3. all selected becomes a solid rectangle;
4. the leader/value feels attached to the geometry;
5. zoom out visibly shrinks leader and glyphs;
6. zoom in grows them within the upper band;
7. controls stay tappable despite smaller visible glyphs;
8. Two Sides stays understandable;
9. orbit/zoom does not make the HUD drift.

Known presentation consequence for review: a union's arrow stands on the
union's centroid, which can lie on a disk the user wants to tap again; a press
on the arrow is a drag, so that disk is tapped off the arrow's corridor.

## Deferred

`CAD-SKETCH-IDENTITY-R1` (sketch ids, body sketch table, feature → sketch
references, reuse, `CADB` v6) is NOT started.
