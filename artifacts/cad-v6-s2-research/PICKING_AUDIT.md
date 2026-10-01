# Picking audit — "a visible cell is untappable from one side, tappable from another"

Task `CAD-V6-S2-RESEARCH-FILL-PICK-HUD3D-R1`, read-only. Baseline `bbae765`.
Native paths are under `app/src/main/cpp/`; Java paths are under
`app/src/main/java/com/forgeshape/app/`.

## 1. Verdict

**`PICK-EVENT-ROUTING` + `PICK-HUD-INTERCEPT`, with `PICK-EDGE-ON-EXPECTED`
only at true edge-on. NOT `PICK-NATIVE-MATH`.**

- **Native math is correct from both sides.** Ray → plane → `(u, v)`
  round-trips to < 3e-5 m from front, back, six octants and orthographic. The
  direct face hit succeeds for 100% of visible cells in every non-edge-on view.
- **Event routing (physical-only).** In Ready, a single finger that misses the
  arrow is handed to the CAMERA, which orbits on every Move with no slop. The
  tap is then resolved at the DOWN pixel against the POST-orbit camera. Taps
  injected on the emulator have zero travel, so CI never sees this.
- **HUD intercept, two layers, both camera-placed.**
  - Java: the value label's and the action panel's ≥ 48 dp invisible proxies
    consume the Down over whatever cells lie under them.
  - Native: a still tap within 10 reference units of the DRAWN arrow is the
    arrow's, and toggles nothing.

  Both follow the projected arrow, so the cells they cover change as the user
  orbits.
- **A confound that looks like picking.** A tap that IS delivered can be
  refused by `PlanarFacesTouchAtPoint` because of the other selected cells
  (`FILL_SELECTION_MODEL.md`). To the user that is also "this cell will not
  select". It is fixed by the selection model, not here.

Confidence: **high** for the mechanisms (source plus reproduction).
**Medium** for which one dominated on the OWNER's phone, because no physical
log exists. The implementation adds the debug tokens in §6 so the next
physical review attributes each miss.

## 2. The real path, step by step

| # | stage | source | can a visible-cell tap fail here? |
| --- | --- | --- | --- |
| 1 | Window → `EditorWorkspaceView` (FrameLayout): child 0 the viewport, child 1 `chromeRoot`, child 2 `overlayRoot` | `EditorWorkspaceView.java:480, 576-587, 725` | Yes, if a VISIBLE clickable descendant of `overlayRoot` contains the point. No container overrides `onInterceptTouchEvent`. |
| 2 | `overlayRoot` → `CadExtrudeCanvasView` (MATCH_PARENT, never clickable itself) | `CadExtrudeCanvasView.java:1351-1359` | Its children can intercept: `reading` / `secondReading` (`valueText`, clickable, min 48 dp), `panelProxy` (clickable `View`, ≥ 48 dp), `actionsPalette` when open, `editor` when open. |
| 3 | `ForgeShapeSurfaceView.onTouchEvent` | `ForgeShapeSurfaceView.java:58-102` | No. View-local `getX/getY`, no GestureDetector, no offset (full-bleed), always returns true. |
| 4 | JNI `NativeViewport.touchEvent` → sketch arbitration | `forgeshape_jni.cpp:7525, 7655, 7752-7777` | The sketch is handed `g_camera.snapshot()` BEFORE the camera sees the event. In Ready an unconsumed single pointer then goes to `g_camera.onTouch`. |
| 5 | `CameraController::onTouch`, one pointer | `forgeshape_camera.cpp:270-287` | Orbits on EVERY Move: `applyOrbit(dx, dy)`, `kOrbitRadiansPerPixel = 0.005`. There is no slop. |
| 6 | `SketchSession::onExtrudeTouch` Down | `forgeshape_sketch_session.cpp:795-833` | Arms the tap at the Down pixel (`:803`). If the Down lands in the 24-unit grab corridor, the drag is captured (camera reset). If it lands within 10 units of the DRAWN arrow (`:823`, `kCadExtrudeTapOnArrowUnits`), the tap is disarmed. |
| 7 | Move | `:835-845` | Disarms past `kSketchTapSlopPixels = 24` RAW pixels, not dp: ≈ 6–8 dp on a 3–4× phone. Sub-slop travel keeps the tap but has already orbited the camera (step 5). |
| 8 | Up → `toggleRegionAt(camera, regionTapX_, regionTapY_)` | `:865-873` (navigating), `:885-889` (corridor) | Uses the Down pixel with the CURRENT camera, i.e. after the orbit. |
| 9 | `screenToSketch` → `buildPickRay` → `intersectRayPlane` | `:601-622`; `forgeshape_picking.cpp:46`; `forgeshape_gizmo.cpp:501` | Only when \|n·dir\| < `kGizmoPlaneParallelEpsilon` (1e-3, ≈ 0.057° from edge-on). The denominator may take either sign, `t` is unconstrained, and there is no front-face cull. |
| 10 | Point-in-face over `faceShapes_`, smallest area wins | `:1484-1496` | Only for a point outside every face (the exterior) or exactly on an edge. |
| 11 | `togglePlanarFace` | `:1635-1673` | Refused on add by `PlanarFacesTouchAtPoint` (selection confound, above). |

Other candidate causes, eliminated:
- **Viewport or chrome offsets.** The SurfaceView is full-bleed and receives
  view-local coordinates. Insets move only HUD placement
  (`ViewportAnchorSpace`), never native input.
- **Preview/overlay coordinate space.** The overlay, the hit test and the HUD
  anchors all use one `CameraSnapshot`, and the round-trip is exact (§3).
- **Edit Sketch chip, sketch navigator, line dimension.** All GONE in Ready
  (`EditorWorkspaceView.java:3280-3291`; `forgeshape_jni.cpp:5306-5309`).

## 3. Reproduction matrix (scratch harness, real `SketchSession` + `CameraController`)

**Harness.** A 1080×2400 viewport, orbit target at the origin, distance 9 m,
ortho half-height 4 m. The 7-cell sketch from `FILL_SELECTION_MODEL.md` §1.2.

**Columns.**
- *round-trip*: project the cell's interior point → `screenToSketch` →
  same `(u, v)` and the same cell.
- *direct*: `toggleRegionAt` at that pixel.
- *real*: Down/Up through the JNI ordering above (sketch first, then camera).

"preview" = the large rectangle cell selected first, so the arrow, leader and
corridor are live.

| camera | proj | preview | visible | round-trip | direct | real (zero travel) | misses |
| --- | --- | --- | --- | --- | --- | --- | --- |
| front (yaw 0, pitch .35) | persp | no | 7 | 7 | 7 | 7 | – |
| back (yaw π, pitch .35) | persp | no | 7 | 7 | 7 | 7 | – |
| 6 octants (±yaw, ±pitch .6) | persp | no | 7 each | 7 | 7 | 7 | – |
| front / back | ortho | no | 6 | 6 | 6 | 6 | – |
| octant (3.9, −.6) | ortho | no | 7 | 7 | 7 | 7 | – |
| grazing 15° | persp | no | 7 | 7 | 7 | 7 | – |
| 1° off edge-on | persp | no | 7 | 7 | 7 | 7 | – |
| **edge-on (0°)** | persp | no | 7 | 0 | 0 | 0 | all (expected) |
| front, back, 6 octants, ortho ×3 | both | **yes** | 5–6 | all | all | all | – |
| **grazing 15°** | persp | **yes** | 6 | 6 | 6 | **3** | 3 cells: tap on the DRAWN arrow |
| **1° off edge-on** | persp | **yes** | 6 | 6 | 6 | **1** | 5 cells: tap on the DRAWN arrow |

Max round-trip error over every non-edge-on view: 2.9e-5 m (ortho octant);
perspective ≤ 1.5e-6 m. **Front and back results are identical, value for
value.**

**Sub-slop finger travel through the real path.** For each view, ~2,250 taps
on a 0.15 m grid of sketch points inside cells. Down at p, one Move to p+j with
|j| < 24 px (still a tap), Up. Each tap is checked for whether the cell it
toggled is the one under the finger at Down.

| view | travel | orbit during the tap | wrong cell | no cell | drift mean / max |
| --- | --- | --- | --- | --- | --- |
| front / back persp | 8 px | 0.04 rad | 18 | 0 | 0.014 / 0.047 m |
| octants persp | 8 px | 0.04 rad | 48–69 | 36–57 | 0.043–0.048 / 0.136–0.196 m |
| octants persp | 16 px | 0.08 rad | 106–125 | 98–123 | ≈0.09 / 0.29–0.42 m |
| ortho octant | 8 px | 0.04 rad | 57 | 62 | 0.045 / 0.135 m |
| grazing 15° | 8 px | 0.04 rad | 145 | 112 | 0.10 / 0.44 m |
| grazing 15° | 16 px | 0.08 rad | 261 | 225 | 0.22 / 1.16 m |
| 1° off edge-on | 4 px | 0.02 rad | 257 | 495 | 1.5 / 23.5 m |

"No cell" means the post-orbit ray left every cell (the exterior), so the tap
did nothing. That is the OWNER's "untappable" cell.

The miss rate grows with obliqueness and with distance from the orbit target,
and the Ready view is DELIBERATELY oblique (`cadFeatureViewPose`, a 0.62 rad
lean). That is why it reads as "one side/quadrant": the cells far from the
target, on the side the oblique view foreshortens, are the ones a small orbit
throws off. Front and back of the PLANE are not the variable; the angle and the
distance are.

**HUD intercept (Java).** Not reproducible on the host. Established from
source:
- `panelProxy` is about 96×48 dp at scale 1.0 (154×58 dp at 1.6), invisible
  and clickable, placed past the projected arrow tip and sliding back along the
  shaft at an edge.
- The value label is a clickable `TextView` of ≥ 48 dp height whose visible
  text is about 15 dp tall, so an invisible band about 16 dp above and below
  it takes the touch.

All three move with the camera.

## 4. Root cause by class

| class | present? | evidence | fix belongs in |
| --- | --- | --- | --- |
| `PICK-NATIVE-MATH` | **No** | §3 round-trip and direct columns; `intersectRayPlane` is two-sided; front ≡ back | nothing — **do not touch ray-plane math** |
| `PICK-EVENT-ROUTING` | **Yes** (physical-only) | camera orbit on sub-slop Move + tap resolved against post-orbit camera (§2 steps 4–8; §3 travel table) | native: resolve the tap against the camera captured at Down; and do not orbit the camera while a Ready tap is still armed |
| `PICK-HUD-INTERCEPT` (native arrow) | **Yes** | grazing/near-edge rows with preview: 3/6 and 5/6 cells eaten by the drawn-arrow disarm | native: only the drawn arrow HEAD claims a still tap; a still tap on the SHAFT toggles the cell under it (a still tap on the arrow does nothing today anyway — `endDrag` with no movement) |
| `PICK-HUD-INTERCEPT` (Java proxies) | **Yes** (source) | invisible ≥ 48 dp proxies over cells, camera-placed | HUD v3: one badge instead of a 3-glyph plate; a proxy claims a Down only inside its visible shape ∪ the 48 dp floor (`HUD3D_OPTIONS.md`) |
| `PICK-EDGE-ON-EXPECTED` | at 0° only | edge-on row | explicit policy (§5) |
| selection confound | **Yes** | `FILL_SELECTION_MODEL.md` | selection model |

## 5. Edge-on policy (distinguished from wrong-side behaviour)

- **Wrong side does not exist.** The plane is two-sided by construction and
  measured identical from front and back. No culling may ever be added.
- **Edge-on is real and ill-conditioned.** Below a grazing angle, one pixel
  maps to metres on the plane: at 1°, a 4 px travel threw the hit up to 23 m.
  The 1e-3 epsilon refuses only the exact edge. **Proposed:** Ready taps
  resolve only when the sine between the pick ray and the plane is ≥
  `kSketchTapMinGrazingSine` = sin 3° ≈ 0.052 (OWNER-tunable, presentation
  policy). Below it the tap is refused with a debug token
  (`FORGESHAPE_SKETCH_TAP_REFUSED:edge_on`), never a guess.

  This is stated as a threshold, so "edge-on" is a number a test can pin, not
  an accident of float precision. It does not touch `intersectRayPlane`,
  which the gizmo shares.

## 6. Diagnostic tokens for the next physical review (debug-only)

- `FORGESHAPE_SKETCH_TAP:<resolved|exterior|edge_on|on_arrow_head|travel>` on
  every Ready Up that was a tap candidate.
- `FORGESHAPE_CAD_HUD_TOUCH:<value|badge|palette>` when a HUD view consumes a
  Down in Ready.

With these, every miss on the phone is attributable from one logcat capture
(enlarge the buffer first, per `CLAUDE.md`).
