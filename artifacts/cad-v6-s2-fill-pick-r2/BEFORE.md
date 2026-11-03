# BEFORE — `CAD-V6-S2-CORRECTION-FILL-PICK-R2`

Recorded from source on the task branch `feature/cad-v6-s2-fill-pick-r2` at
`13ff275` (the research commit), BEFORE any product edit. Every claim below was
re-read in the file named; the research artifacts in
`artifacts/cad-v6-s2-research/` were used as a map, not as proof.

Refs at start: `origin/main` `6c9f156c1df91fe1a55445aed6186105a075ab31`;
`origin/feature/cad-v6-sketch-face-r1` `bbae765645a318f83451e9a61b6a45cf2cb17e33`;
research `13ff275e763a406ccfb9fb78e9fdb5fbfb3c4093`, parent `bbae765`. The
session-branch merge `fe7b19b` is NOT the base.

Paths are under `app/src/main/cpp/` unless stated.

## 1. ADD merge-validates the whole proposed set

`forgeshape_sketch_session.cpp:1635` `SketchSession::togglePlanarFace`:

- `:1652` `if (!removed) {` — the add branch.
- `:1656-1658` the cap (`kMaxPlanarFaceSelection` → `TooManyRegions`).
- `:1659-1664` every face of the PROPOSED set (`next` = current + tapped) is
  resolved to an index.
- `:1665` `mergePlanarFaceSelection(arrangement_, indices, nullptr)` over that
  whole set; `:1666-1668` any non-`Ok` refuses the tap and the selection stands.

So whether a face can be ADDED depends on which other faces are selected.

## 2. REMOVE does not validate

Same function: when `removed` is true the `if (!removed)` block is skipped and
`:1670` `setPlanarSelection(std::move(next))` stores the reduced set with no
merge. A removal can therefore leave a set that an add would have refused
(the OWNER's 7 → 6 sequence; `FILL_SELECTION_MODEL.md` §1.2).

## 3. The merge refuses a node reused ACROSS loops

`forgeshape_sketch_arrangement.cpp:2127` `mergePlanarFaces`:

- `:2200` `std::vector<uint8_t> nodeUsed(arrangement.nodes.size(), 0);` — ONE
  vector for EVERY output loop of the whole selection.
- `:2207-2212` each walked half-edge's origin node: `if (… nodeUsed[node] != 0u)
  return ArrangementStatus::PinchedSelection;`.
- `forgeshape_cad_body.cpp:710` maps `PinchedSelection` →
  `CadStatus::PlanarFacesTouchAtPoint` (code 61, `forgeshape_sketch.h:238`).
- The header comment (`forgeshape_sketch_arrangement.h:85-89`) states the
  intent: "whether that makes one loop revisit a node or two loops share one".

Two chosen faces that meet only at a point (no shared fragment) therefore pinch
the whole selection, even though they would be two separate components.
`forgeshape_cad_body.cpp:893-905` (`validateCadFeatureGeometry`) uses the same
merge, so such a stored selection is also refused on load
(`CADV6S2_REG_04`, `forgeshape_cad_feature_selftest.cpp:3856`), and
`S2CORR_TAP_05` (`:2238`) pins the tap refusal as intended behaviour.

The components are sorted canonically at the end (`:2315-2318`, by outer
fragment cycle), so their order does not depend on walk order.

## 4. Ready touch arbitration ORDER

The Android view tree (`app/src/main/java/com/forgeshape/app/`):

1. `EditorWorkspaceView` (FrameLayout): child 0 the viewport
   (`EditorWorkspaceView.java:480`), then `chromeRoot` (`:586`), then
   `overlayRoot` (`:725`). No container overrides `onInterceptTouchEvent`; the
   topmost visible clickable child containing the point gets the Down.
2. `overlayRoot` holds `CadExtrudeCanvasView` (`:807-808`), whose clickable
   children consume a Down before the viewport ever sees it: `reading`
   (`CadExtrudeCanvasView.java:273-274`, a `TextView` with ≥ 48 dp height),
   `panelProxy` (`:310-320`, invisible, clickable), `actionsPalette` (`:327-389`,
   clickable when open), `secondReading` (`:418-426`). None of them logs
   anything today.
3. Only an unclaimed Down reaches `ForgeShapeSurfaceView` and then JNI
   `NativeViewport.touchEvent` (`forgeshape_jni.cpp:7525`).

Inside JNI, with a sketch session active (`forgeshape_jni.cpp:7750-7790`):

1. `:7755` `sketch.onTouch(…, g_camera.snapshot(), …)` FIRST, with the camera
   as it stands BEFORE this event.
2. In Ready the session runs `SketchSession::onExtrudeTouch`
   (`forgeshape_sketch_session.cpp:783`): Down arms the tap (`:801-804`),
   hit-tests the arrow corridor (`:815-818`), disarms on the DRAWN arrow
   (`:823-826`), and captures a drag in the corridor (`:827-831`, returns
   `true` = consumed).
3. `:7769-7779`: if consumed (arrow corridor) → `g_camera.resetGesture()`;
   otherwise, in Ready, `g_camera.onTouch(…)` with the SAME event.

So the order is: Android HUD children → SketchSession (arrow corridor
first, then tap arming) → camera. The camera never learns whether a tap is
armed.

## 5. The camera orbits on a sub-slop Move

- JNI hands every unconsumed single-pointer Ready event to the camera
  (`forgeshape_jni.cpp:7771-7776`), regardless of the session's armed tap.
- `CameraController::onTouch` (`forgeshape_camera.cpp:251`), one pointer
  (`:270-287`): the first event re-anchors; every later `Move` calls
  `applyOrbit(p.x - lastX_, p.y - lastY_)` (`:281-285`). There is NO slop.
- The session keeps the tap armed until travel exceeds
  `kSketchTapSlopPixels` = 24 px (`forgeshape_sketch_session.h:177`;
  `forgeshape_sketch_session.cpp:836-841`).

A finger that jitters 8 px therefore orbits the camera AND is still a tap.

## 6. The tap resolves with a camera that may have moved since Down

- Navigating path, Up: `forgeshape_sketch_session.cpp:865-873` →
  `toggleRegionAt(camera, regionTapX_, regionTapY_, …)` where `camera` is the
  snapshot JNI took for THIS (Up) event — after every Move already orbited it.
- Corridor path, Up: `:883-889`, the same call with the same current camera.
- The Down pixel is used, but the Down camera is not stored anywhere
  (`forgeshape_sketch_session.h:710-713` hold only armed / pointer / x / y).

## 7. The drawn-arrow still-tap claim

- `forgeshape_sketch_session.cpp:823-826`: on Down, if
  `extrudeDrag_.onDrawnArrow(…)` the tap is disarmed.
- `forgeshape_cad_extrude_tool.cpp:447-460` `onDrawnArrow`: either side,
  `arrowWithin(…, kCadExtrudeTapOnArrowUnits)` — the distance to the projected
  segment from the BASE to the drawn point (`:479-490`), i.e. the whole shaft
  plus the head.
- `kCadExtrudeTapOnArrowUnits` = 10 reference units
  (`forgeshape_cad_extrude_tool.h:286`); the grab corridor is 24
  (`:275`).
- The head is drawn from `tip - axis·h` to `cadExtrudeArrowPoint` =
  `tip + axis·h`, `h = controlWorld · kCadExtrudeArrowHeadLengthFraction`
  (`forgeshape_cad_extrude_tool.cpp:594-631, 636-640`).
- `S2CORR_TAP_03` (`forgeshape_cad_feature_selftest.cpp:2200-2217`) pins that a
  still tap at the MID-SHAFT toggles no cell.

So any still tap on the shaft — which stands on the chosen area and, in the
oblique view, crosses neighbouring cells — is eaten and changes nothing.

## 8. Format / corpus baseline

- `testdata/forge/`: **57** `.forge` fixtures (`find testdata -name '*.forge'
  | wc -l` → 57).
- `scripts/build-forge-corpus.ps1` is the independent encoder; `CI FAST`
  regenerates all 57 and requires byte identity (`docs/CI_CLOUD.md`).
- `DATA_PACKAGE_SPEC.md:1052`: "a face selection whose UNION pinches -- two
  chosen faces meeting at one point → `PlanarFacesTouchAtPoint`". This is a
  semantic validation row, not a layout.
- No committed fixture depends on the pinch refusal: the v6 refusal fixtures
  are `cad_bad_sketch_ref`, `cad_duplicate_sketch_id`, `cad_bad_selection_kind`,
  `cad_noncanonical_face`, `cad_unresolved_face`, `cad_overlap_face`
  (`PlanarFaceAmbiguousOverlap`).

## Plan consequences (binding for this slice)

- The tap's ADD checks state, resolution and the cap only.
- `mergePlanarFaces` partitions the chosen faces by SHARED FRAGMENT first and
  walks each edge-connected group with its OWN node bookkeeping; a group that
  revisits a node is still `PinchedSelection`. The canonical component sort is
  unchanged, so every previously valid selection derives bit-identically.
- Ray-plane math (`buildPickRay`, `screenToSketch`, `intersectRayPlane`) is not
  touched; no 3° band is added.
