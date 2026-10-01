# CAD-FOUNDATION-C1 — BEFORE (source re-verification)

Baseline `origin/main = 509de02331ca1a8dfc20528b42d83a18392cef9f`, read on the
task branch `feature/cad-foundation-c1-union-hud-r1` BEFORE any product byte
changed. Paths: `cpp/` = `app/src/main/cpp`, `java/` =
`app/src/main/java/com/forgeshape/app`. Every line number is the baseline's.
Host baseline: `scripts/host-native-selftests.sh` → `HOST_SELFTESTS_OK (3757
checks, 0 failed)`.

The Fable audit (`artifacts/fable-cad-architecture-audit-r1/`) was re-checked
against this tree; every finding below that it names is confirmed, and two
jni line numbers moved by ten lines since the audit was written (noted).

## A1 — the extrude HUD path

```
SketchSession::extrude_ (one writer: applyExtrudeFeature, session.cpp:1355)
→ SketchSession::extrudeAnchors            session.cpp:1440   (camera-free)
  → extrudeSelectionAnchorPoint             tool.cpp:139       (FIRST chosen region)
  → cadExtrudeAnchorsAt                     tool.cpp:162       base / per-side tip / shaft-midpoint label
→ metersPerPixel / control scale
  → cadExtrudeControlScaleFor               tool.cpp:231       clamp((0.45 m / mpp) / 120 px, 0.80, 1.60)
→ sketch overlay arrow
  → SketchSession::buildOverlay             session.cpp:1765   appendCadExtrudeArrow(controlScale.world)
→ JNI tool state
  → cadExtrudeToolState(double[32])         jni.cpp:4817       slots 0..31
→ CadExtrudeCanvasView.refreshFromNative    java CECV:736
→ Android anchor conversion                 ViewportAnchorSpace.place
→ hit target / glyph / value presentation   CadHudPresentation (HIT 48, glyph clamp(28·s, 24, 32))
```

### Where `metersPerPixel` is read — TWO depths (confirmed: the S3 defect)

| Consumer | Where | Depth the scale is read at |
| --- | --- | --- |
| Drawn arrow head / base tick | `jni.cpp:1859-1862` `gizmoWorldScale(camera, {0,0,0}, h, &worldPerUnit)` → `sketchSession().overlay(worldPerUnit)` → `session.cpp:1780-1782` `perPixel = worldPerUnit / gizmoPixelsPerReferenceUnit()` → `session.cpp:2013` `cadExtrudeControlScaleFor(perPixel)` | **the WORLD ORIGIN** |
| Arrow hit test (head extension) | `tool.cpp:304` `cadExtrudeControlScale(camera, anchors.base, h)` | `anchors.base` |
| HUD scale slot `[10]` / glyph | `jni.cpp:4864` (audit cited :4854) `cadExtrudeControlScale(g_camera.snapshot(), anchors.base, h)` | `anchors.base` |
| Retained-sketch chip scale | `jni.cpp:5186` | `anchors.base` |

One formula, two inputs. The comment at `session.cpp:2004-2010` ("what is drawn
and what is hit-tested are one number") is contradicted by `jni.cpp:1859`. In
orthographic projection the two coincide; in perspective any sketch whose
profile is off the world origin (every face sketch, every Add/Cut feature)
draws its head at one scale and hit-tests/sizes glyphs at another.

### Current fixed floors (confirmed)

| Floor | Where | Value |
| --- | --- | --- |
| Band | `tool.h:185-188` | `W` 0.45 m, `S_ref` 120 px, min **0.80**, max 1.60 |
| Glyph | `CadHudPresentation.java:33-41,75-78` | `clamp(28·scale, 24, 32)` dp |
| Hit | `CadHudPresentation.java:31,89-91`, `CECV:513-514,567-568` | 48 dp, and the 48 dp container IS the drawn capsule member |
| Value | `CadHudPresentation.java:43`, `bg_hud_value.xml` | 32 dp pill inside a 48 dp row, 14 sp text, never rotated |
| Row | `CECV:268-324` | one `LinearLayout` capsule: extent, value, badge, Flip |
| Arrow head | `tool.h:219-227` | `0.42·world` long, `0.16·world` half-width, tick `0.18·world` |

### Current anchor (confirmed)

Slots `[6],[7]` are the projected **shaft midpoint** (`tool.cpp:201-208`
`label = base + axis·d/2`); the cluster is placed so the VALUE's centre stands
ON it (`CECV:944-972`, `CadHudPresentation.valueCentreOffset`). The value is
therefore drawn on top of the shaft it measures, horizontal, in a screen-sized
capsule. The arrow lives in the `Entities` overlay range (`session.cpp:2020`),
not the `Dimension` range.

### S4 — overlay revision on a zoom rebuild (confirmed)

`SketchSession::overlay` (`session.cpp:1759`) rebuilds when
`overlayWorldPerUnit_ != worldPerUnit` but does NOT advance `overlayRevision_`;
`buildOverlay` stamps the old revision (`session.cpp:1767`). The renderer's
upload gate (`renderer.cpp:1602`) skips the upload when the revision AND the
vertex count are unchanged, so a zoom that changes only vertex POSITIONS (the
arrow head, the snap marker, the hatch spacing at an unchanged line count, the
Line dimension) keeps drawing the previous zoom's vertices.

## A2 — the region path

```
sketch loops      extractClosedProfiles                     sketch.cpp:1076
→ regions         extractSketchRegions                      region.cpp:170   one region per loop = loop − direct children
→ ref             ProfileRegionRef {outer, holes}           region.h:66
→ toggle          SketchSession::toggleRegion               session.cpp:1294
→ validation      validateRegionSelection                   region.cpp:276
→ preview         buildOverlay hatch/preview per region     session.cpp:1827-1998
                  + evaluateCandidate → regenerateCadBody
→ ExtrudeFeature  profileEntityId + profileHoleIds + additionalRegions  cad_body.h:123-157
→ prisms          appendCadFeatureSolid: ONE PRISM PER SELECTED REGION  cad_feature.cpp:305
```

### The exact share-a-loop refusal (confirmed)

`region.cpp:329-334`:

```
if (extraction.loopContains(a, b) && !insideAHole(*chosen[i], b)) return OverlappingRegions;
if (extraction.loopContains(b, a) && !insideAHole(*chosen[j], a)) return OverlappingRegions;
```

with `insideAHole` skipping `h == loop` (`region.cpp:314-321`). When region j's
outer loop IS one of region i's own holes (its DIRECT child), no other hole
contains it and the pair is refused `OverlappingRegions`. For rectangle R with
circles A, B inside: `R+A`, `R+B` and `R+A+B` are all refused.

### The exact silent-drop path (confirmed)

`session.cpp:1318-1327` (`toggleRegion`): before adding the tapped region, each
currently selected region is kept ONLY if `validateRegionSelection({ref, new})`
is Ok; anything else is silently removed ("a region it overlaps, touches or
shares a loop with is replaced by it"). With R selected, a tap on disk A
deselects R and selects A — no refusal, no status, the selection "jumps".
Device test `CadVerticalSliceTest.owner_rectangle_circle_region` and native
`CADVS_SES_05` assert this replacement.

### Persisted bytes for a selection (confirmed)

`CADB` v5 `REGIONS` (`project_document.cpp:974-986` write, `:1637-1682`
read): `u32 holeCount, holes…` for the first region (its outer anchor is the
v1 `profileEntityId` field), then `u32 additionalCount`, per further region
`{u32 outer, u32 holeCount, holes…}`. `DATA_PACKAGE_SPEC.md` §7f.

### Why no `CADB` change is needed for union semantics

The stored form is already "the list of selected ATOMIC regions, each with the
holes it was chosen with". `[{R,{A,B}}, {A,{}}]` is byte-representable today;
it is refused only by the `validateRegionSelection` pair rule above (reached at
load through `validateCadBodyState`). The union is a DERIVED reading of that
same list (a loop bounds the union exactly when selection membership differs
across it), so:

- the durable representation does not change, and `ProfileRegionMismatch`
  (stored holes ≠ derived holes) keeps guarding a nesting edit;
- every selection that was legal before has no region beside its own direct
  child, so its union components ARE its regions, in the same order, with the
  same holes: identical faces, lineage signatures and meshes;
- no fixture in the 44-file corpus selects a region beside its own hole
  (`cad_bad_region` is refused by `ProfileRegionMismatch`, not by the pair
  rule), so every fixture keeps its bytes and its verdict;
- an older build reading a C1 file that selects `R+A` refuses it by name
  (`OverlappingRegions`) — fail-closed, with no half-opened body.

No format version is needed or created.

## Tests that pin the BEFORE behaviour (will change by design)

- native `CADVS_REG_06`, `CADVS_REG_10` (region beside own hole refused),
  `CADVS_SES_05` (tap replaces), `CADUXS1_09_*` (band [0.80, 1.60]);
- JVM `CadHudPresentationTest` (glyph 24..32, value-centre offset);
- device `CadVerticalSliceTest.owner_rectangle_circle_region` (replacement),
  `compact_extrude_hud` / `assertIconControl` (glyph 24..32, value centre on
  the shaft midpoint), `CadCanvasExtrudeTest` (scale band near/far),
  `Ui3dStateCorrectionTest.assertExtrudeClusterAttached` and
  `Ui3dStateAuditTest` (value centre on the shaft midpoint).
