# Extrude HUD audit (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Read-only. Baseline `main = 509de02`. Paths: `cpp/` = `app/src/main/cpp`,
`java/` = `app/src/main/java/com/forgeshape/app`. Abbreviations in citations:
`tool` = `cpp/forgeshape_cad_extrude_tool`, `session` = `cpp/forgeshape_sketch_session`,
`jni` = `cpp/forgeshape_jni.cpp`, `CECV` = `java/CadExtrudeCanvasView.java`,
`CHP` = `java/CadHudPresentation.java`, `VAS` = `java/ViewportAnchorSpace.java`,
`EWV` = `java/EditorWorkspaceView.java`. Evidence tags: `SOURCE_CONFIRMED`,
`TEST_CONFIRMED` (READ, not run), `DEVICE_EVIDENCE`, `INFERRED`, `UNVERIFIED`.

Answers OWNER observation **O1** and required conclusion **C2**. File sizes:
`CECV` 1228 lines, `CHP` 328, `VAS` 245, `tool.cpp` 632 (`tool.h` 453),
`SketchChromePolicy.java` 67.

## 1. The complete call/data path

| Step | File / function | Data | Owner | Space |
| --- | --- | --- | --- | --- |
| Native CAD truth | `SketchSession::extrude_` (`session.h:630`), `operation_` (`:441`), volatile `oneSideDirection_` (`:652`), `frame_` (`:162-167`) | `ExtrudeFeature {regions, depth, secondDistance, extent, direction}` | `SketchSession` (process-scoped, `session.cpp:2071`) | world, metres, sketch (u, v) + normal |
| The ONE writer | `applyExtrudeFeature` (`session.cpp:1355-1399`): validates finiteness, direction, canonical extent, both distances; keeps regions; bumps `candidateRevision_` | | `SketchSession` | semantic |
| Manipulator state | `cadExtrudeAnchorsAt` (`tool.cpp:165-225`): `base` = first chosen region's area centroid (or `interiorPoint` when a hole swallows it, `:142-163`), `tip = base + n·d`, **`label = base + n·(d/2)` (the SHAFT MIDPOINT)**, per side; primary mirror for pre-`CAD-EXT-R1` readers | `CadExtrudeAnchors` (`tool.h:97-126`) | pure function, camera-free (`TEST_CONFIRMED` `CADUXS1_02_e`) | world |
| Control scale | `cadExtrudeControlScaleFor(mpp)` (`tool.cpp:231-258`): `scale = clamp((0.45 m / mpp) / 120 px, 0.80, 1.60)`; `pixels = scale·120`; `world = pixels·mpp`. Constants `tool.h:185-188`. | `CadExtrudeControlScale` | pure function | view-space depth → one scalar |
| Projection | `projectWorldToScreen` (`cpp/forgeshape_gizmo.cpp:404-440`), `worldMetersPerPixel` (`:442-471`) | | **the ONLY projection in the product**; no Java file contains projection arithmetic (grep) | world → viewport px |
| JNI read | `cadExtrudeToolState(double[32])` (`jni:4811-4902`; slot contract `:4760-4809`, mirrored `NativeViewport.java:2344-2406`): scale at **`anchors.base` depth** (`:4854-4860`); primary side label/tip px, second side label/tip px; ON_SCREEN = both label and tip project (`:4886-4897`); candidate status, availability bits, editing feature, revision | `double[32]` | JNI adapter | viewport px + scalar |
| Android refresh | `EWV.refreshSketchViewportSurfaces` (`:3265-3337`): `anchorSpace.beginPass()`, `cadExtrudeCanvas.refreshFromNative()` unconditionally, then dimension labels, ≤ 3 layout re-posts. Called from `syncFromNative` (`:2478→2608`) and the cheap `refreshWorldAnchoredUi` on EVERY pointer sample (`:499-522`) | | `EditorWorkspaceView` | |
| Control layout | `CECV`: `cluster` LinearLayout capsule = extent `IconControl` + value `TextView` + operation badge + Flip (`:268-313`); palettes (`:328-365`); editor `EditText` + Apply (`:368-389`); second cluster (`:396-433`); Edit Sketch chip (`:437-450`). `IconControl` container `setMinimumWidth/Height(48 dp)` (`:513-514`); glyph `ImageView` `glyphPx × glyphPx` (`:524-525, :655-661`); caption 11 sp hidden unless Tool Labels | | Android View tree | dp |
| Presentation constants | `CHP`: `HIT_DP 48` (`:31`), `GLYPH_BASE 28`, `GLYPH_MIN 24`, `GLYPH_MAX 32` (`:33-41`), `VALUE_PILL 32` (`:43`); `glyphDp(scale) = clamp(28·scale, 24, 32)` (`:75-78`); **`hitDp(scale)` returns 48 and ignores the argument by design** (`:89-91`) | | pure Java, `TEST_CONFIRMED` `CadHudPresentationTest` | dp |
| Placement | `placeCluster` (`CECV:944-972`): measure under AT_MOST, `valueCentreOffset` (`CHP:292-294`) so the VALUE's centre stands on the label anchor; `VAS.place` (`:128-174`) converts viewport px to translation and clamps the whole box into the viewport; `measureAndPlace(..., scale)` — every caller passes `1.0f` (`CECV:1009`, `BDLV:226`, `SDLV:197`), so nothing is `setScale`d | | `ViewportAnchorSpace` (one instance shared by three views, `EWV:130, 476, 805-816`) | viewport px → View translation |
| Draw (arrow) | `SketchSession::buildOverlay` (`session.cpp:1765-2075`) appends `appendCadExtrudeArrow(anchors, controlScale.world, …)` (`tool.cpp:474-501`) INSIDE the `Entities` range (`:2001-2020`); shaft = depth, head `0.42·world`, half-width `0.16·world`, base tick `0.18·world` (`tool.h:219-227`). Overlay's `mpp` comes from **`gizmoWorldScale(camera, {0,0,0}, h)` — the WORLD ORIGIN** (`jni:1859-1862`) | `SketchOverlay` | `SketchSession` → `Renderer::recordSketchOverlayDraw` (`cpp/forgeshape_renderer.cpp:1687-1735`, gizmo line pipeline, 1 px, depth off, drawn last `:3405-3408`) | world → clip |
| Draw (chrome) | Android draws the 48 dp containers WITH their capsule backgrounds (`bg_hud_member`, `bg_hud_value`); the value pill's 32 dp visible height is a background inset inside the 48 dp view (`dimens_cad_hud.xml:23`) | | Android | dp |
| Hit testing (arrow) | `CadExtrudeManipulator::hitTestSide` (`tool.cpp:294-332`): projects `base` and `headEnd = tip + axis·0.42·world`, point-to-segment distance in px against corridor `24 ref units · gizmoPixelsPerReferenceUnit` (`tool.h:237`, `tool.cpp:317`), **unscaled**; scale read at `anchors.base` depth (`:302-303`) | | native | screen px |
| Hit testing (chrome) | the 48 dp View rectangles themselves | | Android | dp |
| Gesture | `session.cpp:783-880`: second pointer or PointerDown cancels the drag and hands the gesture to the camera (`:787-791`); Down arms a region tap and hit-tests the arrow (`:813-825`); Move → `updateDrag` (`tool.cpp:368-401`, `depth = depthAtDown + (t − tAtDown)` via `solveAxisParameter`, floor `1 mm` `tool.h:247`) → `setExtrudeSide` → `applyExtrudeFeature` (`:836-846`); Up ends; Cancel restores (`:871-874`) | | `SketchSession` + manipulator | world axis parameter |
| Typed value | value `TextView` tap → `openEditor` (`CECV:1084-1104`) swaps in the `EditText` seeded with the shown value, IME shown; DONE/Apply → `submitField` (`:1196-1213`) → `LengthUnit.parse` → `onExtrudeSideEntered` (`EWV:3436-3460`, One Side re-mapped to a literal side) → `sketchSetExtrudeSide` (`jni:5102`) → `setExtrudeSide` → `applyExtrudeFeature`. Flip → `sketchFlipExtrudeDirection` (`jni:5120`) → `setExtrude(depth, opposite)`. Extent → `sketchSetExtrudeExtent` (`jni:5077`) → `extrudeFeatureWithExtent`. Operation → `sketchSetOperation` (`jni:4667`). | | | |
| Candidate preview | `touchCandidate` bumps the revision; `evaluateCandidate` (`session.cpp:1581-1614`) regenerates once per revision; `applyCadOperationPreviewLocked` (`jni:1655-1709`, render thread) substitutes the target's draw item mesh + `previewTint`, or appends a New Body item; tint through the existing `selectionTint` push slot (`renderer.cpp:3474-3476`) | `RuntimeMesh` keyed by `(revision, target)` | render thread reads the same evaluation `cadExtrudeToolState` reads (`jni:4838-4845`) | |
| Commit | `sketchCommit` (`jni:4336`) → `SketchSession::commit` → same evaluation, ONE `ScopedConstructionEdit` | | | |

Two Sides: native reports the primary side in slots 5–9 and the other in 15–19;
`secondValuePresent` only for Two Sides (`CHP:152-154`); `secondReading` is centred
on the `−N` shaft midpoint (`CECV:797-813`). Symmetric draws two arrows and ONE value.
The drag freezes WHICH side at Down (`tool.cpp:357`, `session.cpp:813-822`).

Off-screen: `ON_SCREEN` means "in front of the eye and finite" — `projectWorldToScreen`
has NO viewport bounds check (`gizmo.cpp:432-439`); the Java clamp (`VAS:162-169`)
is what keeps an off-viewport anchor's cluster at the edge. A non-projecting anchor
hides the chrome (`CECV:746-752, :801-805, :913-916`); `TEST_CONFIRMED`
`Ui3dStateCorrectionTest.assertExtrudeClusterAttached` (`:506-560`).

Tool Labels: `AppPreferences.toolLabels` → `setToolLabelsVisible` → next `applyIcon`
(`CECV:662-676`) shows an 11 sp caption under the glyph in the same 48 dp container
(the row grows in height, not width). Caption and glyph are
`IMPORTANT_FOR_ACCESSIBILITY_NO`; the container carries the description.

## 2. Answers

### Q1 — Why the controls stay large at zoom-out (`SOURCE_CONFIRMED`)

Six independent floors; no single constant removes the effect:

1. **Band floor** `kCadExtrudeControlMinScale = 0.80` (`tool.h:187`, `tool.cpp:243-245`).
   With `W = 0.45 m` and `S_ref = 120 px` it engages at `mpp > 0.0047 m/px`, about
   1.25× the default framing distance (`tool.h:170-172`).
2. **Glyph floor** `GLYPH_MIN_DP = 24` (`CHP:35`): `28 × 0.80 = 22.4 → 24`, so the glyph
   is constant at 24 dp for the band's whole lower half and varies only 24 → 32 dp.
3. **The 48 dp hit rectangle IS the drawn box**: `setMinimumWidth/Height(48 dp)` on every
   icon container (`CECV:513-514`), the value chip (`:567-568`), the field and Apply
   (`:591, :622`); the capsule backgrounds are drawn on those containers. `hitDp` ignores
   the scale by design (`CHP:89-91`); `measureAndPlace(…, 1.0f)` never `setScale`s.
4. **Fixed typography**: 14 sp value, 11 sp caption, 10 dp chip padding, 88 dp field
   minimum (`dimens.xml:333`) read no scale.
5. **The row**: a `LinearLayout` of 48 dp members with 2 dp capsule padding; One Side ≈
   200 dp × 52 dp (`dimens_cad_hud.xml:16-17`). The viewport clamp keeps it whole.
6. **The arrow head** world size floor `0.80 × 120 px × mpp`: at least ≈ 40 px long and
   ≈ 31 px wide on screen at any zoom-out (`tool.cpp:250-251, :436-437`). Only the
   SHAFT shrinks with the camera.

Net: at zoom-out only the shaft and the 24 → 32 dp glyph shrink; every box, text,
capsule, palette, editor and the arrow head are held at floors. This is by design
(`CHP:15-23`, `CECV:51-57`, `tool.h:177-181`): the 48 dp interactive floor was
chosen OVER perspective shrink, and the drawn surface was not separated from the hit
surface. The OWNER's observation is a faithful description of the code.

### Q2 — What already tracks the arrow correctly (`SOURCE_CONFIRMED` / `TEST_CONFIRMED`)

The arrow (world geometry through `viewProj`); the anchors (camera-free); the value's
CENTRE on the projected shaft midpoint on every gesture sample (≤ 4 dp after an orbit,
`CadVerticalSliceTest.compact_extrude_hud`; after orbit/zoom-out/zoom-in,
`Ui3dStateCorrectionTest.ui3dc1_11`; `DEVICE_EVIDENCE` 0.18 dp / 0.15 dp in
`OWNER_FINDINGS.md` F3); the second value on the `−N` midpoint; the Edit Sketch chip on
`anchors.base`; the hit test against the SAME projected base → headEnd segment the
renderer draws; the drag's world-axis meaning (one metre is one metre at any zoom,
`CADUXS1_04_b`); hiding on a non-projecting anchor.

### Q3 — What is Android View geometry and cannot perspective-scale (`SOURCE_CONFIRMED`)

Everything under `CadExtrudeCanvasView`: extent button, value pill, operation badge,
Flip, both palettes, both editors, the second cluster, the Edit Sketch chip, the
captions. Sized in dp by `LayoutParams` and typography, moved only by
`setTranslationX/Y` (`VAS:171-172`). The only camera-derived input is the scalar
`CAD_EXTRUDE_SCALE` sizing the glyph `ImageView`. A perspective shrink would need
either `setScale` on a DRAWN child that is not the hit child (today they are one view),
or renderer-drawn chrome.

### Q4 — The value control

Reading state: a `TextView` (`CECV:555-572`) with a full content description
("Depth %1$s. Drag the arrow, or tap to type an exact depth." / Each side / Side A,
`:780-785`). Editing: a tap swaps in a separate `EditText` (`TYPE_CLASS_NUMBER |
DECIMAL | SIGNED`, `IME_ACTION_DONE`, `:600-624`) plus Apply, seeded and `selectAll`ed,
IME shown explicitly. **Accessibility gap** (`SOURCE_CONFIRMED`): the `EditText` has no
`contentDescription`, no hint and no `labelFor`, in all three anchored views
(`CECV`, `BodyDimensionLabelsView`, `SketchDimensionLabelView`).

### Q5 — Tests that pin the constants (`TEST_CONFIRMED`, READ)

- JVM `CadHudPresentationTest` (323 lines): glyph band 24–32 for every scale in
  [0.80, 1.60]; `hitDp == 48` at every scale; pill 32 inside 48; value-centre offset;
  clamped start; palette hang; Flip One Side only; second value Two Sides only.
- Native `CADUXS1_09_a..j` (`cpp/forgeshape_sketch_ux_selftest.cpp:1250-1834`):
  band `[0.80, 1.60]`, monotonic, both clamps, `pixels = scale·120`, only the head
  takes the scale. `CADUXS1_04_a..j` (`:1371-1675`): corridor, frozen basis, 1 mm drag
  floor vs typed refusal, second pointer, cancel. `CADUXS1C1_*` view policy.
- Device: `CadCanvasExtrudeTest` (830 lines; never `setScale`d, ≥ 48 dp, scale band
  near/far), `CadExtrudeExtentTest` (592), `CadVerticalSliceTest.compact_extrude_hud`
  and `ui_context_withdrawal`, `Ui3dStateCorrectionTest.ui3dc1_11`,
  `Ui3dStateAuditTest.ui3d0809`. No `EditorWorkspace*Test` touches the HUD.

Consequence for any HUD change: the 48 dp floor and the [0.80, 1.60] band are pinned
in three layers; a redesign changes TEST EXPECTATIONS by design, not by accident.

### Q6 — Stale or contradicting comments (`SOURCE_CONFIRMED`)

1. **`session.cpp:2004-2010` is contradicted by code.** It says the drawn head and the
   hit test "are one number". The overlay's `metersPerPixel` is taken at the WORLD
   ORIGIN (`jni:1859-1862`), while `hitTestSide` (`tool.cpp:302`) and the HUD scale
   (`jni:4854`) use `anchors.base`. One formula, two inputs. In orthographic they
   coincide; in perspective any face sketch, off-centre profile or Add/Cut feature draws
   its head at one scale and hit-tests/sizes glyphs at another. Magnitude ∝ depth ratio
   origin/base — `UNVERIFIED` on device (`RUNTIME_VERIFICATION_GAPS.md` G1).
   `CADUXS1_09_f` proves the arithmetic identity for ONE `mpp`, not that both call
   sites feed the same `mpp`.
2. `tool.h:22-26` "HOW BIG the control is drawn and grabbed" — Android hit areas are
   NOT sized by it since `CAD-VERTICAL-SLICE-R1`; `tool.h:177-181` two paragraphs later
   states the new rule. The file contradicts itself.
3. `tool.h:163-164` "the Android cluster is drawn at `scale`" — stale; only the glyph is.
4. `tool.h:33-35` "`setExtrude`, the one door" — the door is `applyExtrudeFeature`
   (`session.h:586-588`); the drag lands through `setExtrudeSide`.
5. `CECV:36-43, :54-57` and `CHP:19-23` describe the removed 60 dp text-pill cluster at
   length; history, not contract.
6. `VAS:176-185` `measureAndPlace(…, scale)` — a vestigial parameter (three callers,
   all `1.0f`).
7. Slot name `CAD_EXTRUDE_ON_SCREEN` overstates (see §1, off-screen).

### Q7 — Docs versus code

`artifacts/cad-ux-audit-r1/CANVAS_UI_ARCH.md` is a PROPOSAL ("Nothing here is
implemented"). Adopted: the layer split (§2), one-more-overlay-producer (§3), the
pick contract, attach-then-clamp (§4b), badges as chrome (§4e). NOT adopted:
`CadFeatureTool` / intent struct (§1; the tool is pure functions + a manipulator owned
by `SketchSession`, `tool.h:11-16`), the `Dimension` style for the arrow (it is in
`Entities`, `session.cpp:2020`), soft blend (§4c), the frozen cluster anchor (§4f: the
label is re-derived every read and the code CLOSES editors during a drag instead,
`CECV:761-767`), a `ScopedConstructionEdit` per sample (§5), and a published preview
mesh (§6: a render-thread substitution). `artifacts/cad-vertical-slice-r1/UX_CONTRACT.md`
matches the shipped code throughout; neither document mentions the origin-vs-base split.

### Duplication (`SOURCE_CONFIRMED`)

Three views implement "chrome standing on a projected native anchor":
`BodyDimensionLabelsView` (317 lines, `bodyDimensionLabelPoint`),
`SketchDimensionLabelView` (284, `sketchScreenPoint`), `CadExtrudeCanvasView` (1228,
`cadExtrudeToolState`). Each carries its own `EditText` setup, IME show/hide,
parse-and-report `submitField`, and `placeAt` wrapper. Projection (native only) and
placement (`ViewportAnchorSpace`) are NOT duplicated. And the product ALREADY has a
world-space dimension annotation: `cpp/forgeshape_body_dimension_overlay.{h,cpp}` (the
Dimensions mode leaders: extension lines, dimension line, end ticks in the
`SketchOverlayStyle::Dimension` range) and the Line dimension (`SKETCH-UX-R1` E). The
extrude HUD does not use either.

## 3. Is this architecture still the right one for O1?

**Partly.** The half that is right and should stay: truth in `SketchSession`, one
writer, camera-free anchors, one native projection, the arrow as overlay geometry, the
drag contract, hidden-never-guessed, and Android as the text-input and accessibility
surface. The half that produces O1: **the drawn surface and the hit surface are the
same View**, so the accessibility floor became a visual floor; the value is CENTRED ON
the shaft rather than beside a dimension line; and the band floor of 0.80 was chosen
for a screen-sized cluster, not for a drawing annotation. The existing `Dimension`
leader machinery was not reused.

## 4. Three architectures

### A — mostly screen-space (today, tuned)

Keep the View cluster; lower the band floor (e.g. 0.50), let the capsule backgrounds
scale with the glyph, shrink the value text within a legibility band, remove the row
capsule so members float individually near their anchors.

- Advantages: smallest change; every test seam exists; accessibility unchanged.
- Disadvantages: still a screen HUD — nothing rotates with the line, the value still
  sits ON the shaft, boxes still cannot go below 48 dp because the drawn view IS the
  hit view unless that is decoupled first (which is most of C's work anyway).
- Android complexity: low. Desktop portability: none (all Android). Touch/A11y:
  unchanged. Rendering: none. Maintainability: unchanged (1228-line view stays).

### B — mostly world-space

Draw everything — arrow, dimension leader, value text, extent/operation/flip glyphs
— as renderer overlay geometry; Android keeps only invisible hit proxies and the
`EditText` editor.

- Advantages: perfect attachment, rotation and perspective scaling for free;
  portable to any platform the renderer runs on; one owner of what is drawn.
- Disadvantages: the renderer has no text path and no font (the product forbids a
  third-party font or text library: CLAUDE.md hard rules), so glyphs and digits would
  be hand-authored line geometry; world-scaled text becomes unreadable at zoom-out
  unless it is ALSO banded, which reintroduces the screen-space policy inside the
  renderer; TalkBack cannot read renderer geometry, so every element still needs a
  mirrored accessible View; palettes and the IME stay Android regardless.
- Android complexity: medium (proxies + mirrors). Native/rendering: high (a glyph
  atlas or line-font, a billboard vertex path, a new style range). Maintainability:
  worse — two mirrors of one control, one of them invisible.

### C — hybrid (recommended)

Split by what each layer is good at:

1. **World, in the renderer (already possible with no new pipeline)**: the arrow AND a
   technical-drawing dimension leader — extension lines from the base and the tip,
   a dimension line parallel to the axis at a perpendicular offset, end ticks — emitted
   by the existing `Dimension` style range exactly as `forgeshape_body_dimension_overlay`
   does for Dimensions mode. World-sized with a much lower floor (the head and tick
   keep a small band so they never vanish; the leader itself is pure world geometry).
2. **Screen-readable, Android, but placed and ORIENTED along the line**: the value is a
   `TextView` translated to the dimension line's midpoint anchor (offset to the
   "above the line" side, not on the shaft) and `setRotation`ed to the projected line
   angle (kept upright by flipping past ±90°, the DIMTIH/DIMTOH-style policy from
   `CAD_BENCHMARK.md`). Its text size follows the band inside a legibility window
   (for example 11–16 sp). Tapping it opens the EXISTING unrotated `EditText` editor
   at the same anchor, screen-aligned, so text input and TalkBack are untouched.
3. **Glyphs**: extent, operation, flip stay `ImageView`s (vector drawables), but the
   DRAWN glyph view is a child sized by the band (widened, e.g. 12–32 dp) with no
   background, and the 48 dp container becomes a transparent hit proxy with the content
   description. The row capsule goes; each control stands at its own anchor beside
   the leader (extent at the base, operation at the tip, flip at the tick), so the
   cluster reads as annotation on the drawing rather than a toolbar.
4. **Hit targets**: unchanged 48 dp proxies, unchanged 24-unit arrow corridor,
   unchanged `ViewportAnchorSpace` clamp. Visual size and interactive size are two
   numbers with two owners (`CHP.glyphDp` vs `CHP.hitDp`), which the code already
   half-states.

- Advantages: achieves O1 (attached, parallel, scales with the model, larger invisible
  hit area) with NO renderer pipeline change, NO font, NO new truth; reuses the
  dimension-leader code that exists; keeps every accessibility property; the
  world half is portable, the Android half is exactly the adapter it should be.
- Disadvantages: a rotated `TextView` needs its own measure/clamp rule; the
  `EditText` must not rotate (it does not, it is a separate view already); tests that
  pin 48 dp AS the drawn box and the [0.80, 1.60] band change by design.
- Android complexity: medium (decouple drawn child from hit container; rotation;
  per-control anchors instead of one cluster anchor). Native: small (leader emission
  in the overlay; anchors for the offset line and the per-control points; a viewport
  clip of the shaft for the label when partially off-screen). Rendering: none.
  Maintainability: better — the three anchored views can share one
  `AnchoredValueEditor` (Q7 duplication) as part of the same work.

**Prerequisite for any of the three**: fix Q6 item 1 so the overlay and the HUD read
ONE `metersPerPixel` at `anchors.base`; otherwise the leader and the glyphs will
disagree in perspective and the OWNER will see the "detached" feel come back on face
sketches.

### Edge cases under C

| Case | Rule |
| --- | --- |
| Off-screen | Native clips the projected base → tip segment to the viewport; the label anchor is the midpoint of the VISIBLE part; when nothing is visible, hidden (never a guess). Replaces "clamp the whole cluster to the edge". |
| Near camera | The band ceiling (today 1.60) still caps the head, tick and glyphs; the leader is world geometry and may legitimately exceed the viewport. |
| Edge-clamped | Only the value text is clamped, along the line, to stay inside the viewport; glyph proxies hide when their own anchor is outside. |
| Occluded | The overlay stays depth-off and drawn last (a drawing annotation is always readable; this is also what every reference product does for its manipulator). The Cut-inside-material question from `CANVAS_UI_ARCH.md` §3 stays answered that way. |
| Two Sides | Two leaders, one per side, each value above its own line; Symmetric one leader per side and ONE value on the primary (as today), labelled "Each side". |
| Tool Labels | Captions cannot scale with a drawing without becoming unreadable; under C the preference captions the PALETTES and the transparent proxies at fixed 11 sp only while a palette is open or a control is focused. OWNER decision D1c. |

## 5. Verdict (C2)

The current architecture is the right SPLIT (truth native, arrow world, text Android,
one projection) and the wrong PRESENTATION for a technical-drawing feel: it fused the
drawn surface with the hit surface and chose a screen-sized cluster. The
technical-drawing interaction the OWNER wants is reachable inside the existing
architecture by (1) drawing a `Dimension`-style leader the product already knows how
to draw, (2) decoupling drawn from hit in Android, (3) orienting the value along the
line, and (4) unifying the scale input. It does not need a renderer text path, a
second model of the extrusion, or any change to truth, history, the codec or the
fixtures. The band constants and the "how small may it get" answer are the OWNER's
(`OWNER_DECISIONS.md` D1).
