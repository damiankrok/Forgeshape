# Implementation plan — two sequential slices

Task `CAD-V6-S2-RESEARCH-FILL-PICK-HUD3D-R1` (research only; nothing below is
implemented). Baseline `origin/main` `6c9f156`, branch
`feature/cad-v6-sketch-face-r1` `bbae765`.

**Two sequential tasks.** Source ownership separates cleanly:

| | Phase 1 | Phase 2 |
| --- | --- | --- |
| Covers | selection and picking | the HUD |
| Lives in | `forgeshape_sketch_arrangement.cpp`, `forgeshape_sketch_session.{h,cpp}`, the Ready arbitration in `forgeshape_jni.cpp`, and the arrow's tap-claim predicate in `forgeshape_cad_extrude_tool.{h,cpp}` | a new dock-frame function in `forgeshape_cad_extrude_tool.{h,cpp}`, new tool-state slots, `CadExtrudeCanvasView` and `CadHudPresentation` |

Phase 1 must land first:
- The OWNER cannot judge a HUD while taps on the cells under it are being lost
  or refused.
- Phase 2's touch claim relies on Phase 1's tap semantics.

---

## Phase 1 — `CAD-V6-S2-CORRECTION-FILL-PICK-R2` (next slice)

### Scope

1. **Selection is a set; the tap checks the cap only.**
   - `SketchSession::togglePlanarFace`: adding no longer calls
     `mergePlanarFaceSelection`. The only add refusal is `TooManyRegions` at
     `kMaxPlanarFaceSelection`, plus the existing state guards. Removing is
     unchanged.
   - The `reconcilePlanarSelection` merge checks become always-`Ok` for derived
     faces. Keep them as resolution checks.
2. **Point-touch components.** `mergePlanarFaces`: remove the cross-loop
   `nodeUsed` → `PinchedSelection` early return (`:2200-2211`) and keep a
   bounds check. Loops meeting at a node become separate simple loops through
   the existing tight-turn walk, and outers become separate components
   (`FILL_SELECTION_MODEL.md` §4, verified on all 127 subsets in scratch).
   - `appendPrism` already emits one vertex ring per loop. No change.
   - `PlanarFacesTouchAtPoint` (61) and `ArrangementStatus::PinchedSelection`
     are RETAINED, NOT EMITTED; comments say so.
3. **New Body / Add / Cut:** no rule change.
   - Point-touching components are one multi-shell solid.
   - Add/Cut remain ONE kernel boolean with a multi-shell tool (K3 ≡ K4,
     K5 ≡ K6).
   - `AddDisjoint`, `AddNoEffect`, `CutRemovesBody` and `CutNoIntersection`
     are measured on the result as today and named on the preview. The
     selection is never trimmed.
4. **Tap resolved against the Down camera.**
   - `onExtrudeTouch` stores the `CameraSnapshot` it was handed at Down
     (`regionTapCamera_`).
   - Both Up paths (`:865-873` navigating, `:885-889` corridor) call
     `toggleRegionAt(regionTapCamera_, regionTapX_, regionTapY_, …)`.
   - `screenToSketch`, `buildPickRay` and `intersectRayPlane` are NOT
     modified.
5. **No orbit while a Ready tap is armed.**
   - The session exposes `readyTapArmed()`.
   - JNI withholds single-pointer Moves from `g_camera` while it is true.
   - On the Move that disarms it (travel > `kSketchTapSlopPixels`), JNI calls
     `g_camera.resetGesture()` before forwarding, so the camera re-anchors at
     the current point and does not jump. This is standard slop behaviour.
   - Two pointers are unchanged.
6. **Only the drawn arrow HEAD claims a still tap.**
   - Add `CadExtrudeManipulator::onDrawnArrowHead` (segment tip →
     `cadExtrudeArrowPoint`, `kCadExtrudeTapOnArrowUnits`).
   - `onExtrudeTouch :823` uses it instead of `onDrawnArrow`.
   - A still tap on the shaft toggles the cell under it; a drag anywhere in the
     corridor still takes the arrow.
   - Today a still tap on the arrow ends a no-movement drag and changes nothing
     (`endDrag`), so this frees taps and loses no act.
   - `S2CORR_TAP_03` changes meaning accordingly: a still tap on the HEAD
     toggles no cell, and a still tap on the mid-SHAFT toggles the cell under
     it. This is a deliberate rule change, recorded, not a weakened test.
7. **Edge-on is a stated threshold.**
   - `kSketchTapMinGrazingSine` = sin 3° (OWNER-tunable).
   - In `toggleRegionAt` a tap below it is refused with a debug token and
     changes nothing.
8. **Diagnostics (debug-only).**
   - `FORGESHAPE_SKETCH_TAP:<resolved|exterior|edge_on|on_arrow_head|travel>`
     on every Ready tap candidate.
   - `FORGESHAPE_CAD_HUD_TOUCH:<value|panel|palette>` when a HUD view consumes
     a Down in Ready (Java).
9. **Docs.**
   - `CLAUDE.md`: the region/planar rule sentence "a node reused across or
     within loops is `PinchedSelection`, refused as `PlanarFacesTouchAtPoint`"
     is replaced by the selection/evaluation rule.
   - `DATA_PACKAGE_SPEC.md`: the validation-table row for pinched unions is
     replaced (now accepted; components rule).
   - `ARCHITECTURE.md`, `PROJECT_STATUS.md`.
   - No `CADB` version, layout or fixture byte. The corpus stays 57/57 with
     unchanged verdicts.

### Not in Phase 1

- No HUD layout change. The 3-glyph plate stays until Phase 2; only the debug
  token is added on the Java side.
- No renderer change, no format change.
- No S3, no merge.

### Tests — Phase 1

**Host native (new FILL-R2 group in the CAD-feature suite)**

| id | check |
| --- | --- |
| FILL-R2-01 | every atomic face of the 7-cell OWNER-style sketch toggles alone from empty, and also from every other single-face selection (42 ordered pairs, 0 refusals) |
| FILL-R2-02 | order permutation: all 5040 orders of the 7 faces end in the same full set; 200 seeded random toggle sequences equal the XOR of their taps |
| FILL-R2-03 | point-touch pair (A-only + B-only) selectable both orders; 2 components; regenerates; volume within 2% |
| FILL-R2-04 | disjoint pair selectable; 2 components |
| FILL-R2-05 | edge-adjacent pair selectable; 1 component; no internal wall (side-face count) |
| FILL-R2-06 | the OWNER sequence (all 7 → remove C-inside → 6 → remove and re-add rectangle) is `Ok` at every step; preview `Ok`; volume 12.000 |
| FILL-R2-07 | all 127 non-empty subsets merge and regenerate (the scratch proof, committed) |
| FILL-R2-08 | a selection whose Add is refused (`AddDisjoint`) KEEPS every selected face; the preview names it; Extrude withdrawn; removing the disjoint cell restores `Ok` |
| FILL-R2-09 | point-touching New Body + a later Add and a later Cut: `cadKernelValidateSolid` `Ok` on the base; exact volumes |
| FILL-R2-10 | a pinched ring (hole touching its outer at one node) merges to 1 component with 1 hole, triangulates, extrudes, and a Cut over the pinch is exact |
| FILL-R2-11 | `TooManyRegions` at 17 faces is the only add refusal |
| FILL-R2-12 | a v6 body with a point-touching selection encodes, decodes and regenerates; its bytes are deterministic; all 57 corpus fixtures keep their bytes AND verdicts |
| FILL-R2-13 | no derived atomic face self-pinches across the tangent, corner-touch and diagonal sketches (`finish()` never refuses for it) |

**Host picking (new PICK group)**

| id | check |
| --- | --- |
| PICK-01 | project → `screenToSketch` round trip ≤ 1e-4 m, perspective and ortho, front, back and 8 octants (yaw 45°+k·90°, pitch ±0.6) |
| PICK-02 | the direct face hit equals the projected cell in every view of PICK-01, with and without preview |
| PICK-03 | Down / Move (+4, +8, +16 px) / Up through the session with the camera ordering simulated as JNI does it (`readyTapArmed()` honoured): toggled cell == cell under the Down pixel in 100% of grid taps (BEFORE: up to 125/2256 wrong and 123 none); camera pose unchanged by a sub-slop tap |
| PICK-04 | a Move past the slop disarms the tap and the first orbit step starts from the re-anchor point (no jump > one Move's delta) |
| PICK-05 | with preview at grazing 15° and 1°-off-edge-on views: a still tap on the mid-shaft toggles the cell under it; a still tap on the head toggles nothing; a drag from either takes the arrow |
| PICK-06 | edge-on: below `kSketchTapMinGrazingSine` a tap changes nothing and names `edge_on`; at 4° it resolves |

**Device (focused `CI DEVICE` class `CadFillPickR2Test`)**

| id | check |
| --- | --- |
| DEV-01 | BEFORE / AFTER: real window taps, injected as MotionEvent Down / Move(+10 px) / Up through `ForgeShapeSurfaceView`, from two oblique views on opposite sides, with preview live. Each lands on the cell under the Down (native query by face index). The BEFORE run, on the unchanged APK, fails it. |
| DEV-02 | the OWNER sequence 7 → 6 → add, by real taps; the status line never shows the point-touch message |
| DEV-03 | Add and Cut commit with a point-touching selection; one Undo each |
| DEV-04 | a tap through the value label's invisible band logs `FORGESHAPE_CAD_HUD_TOUCH:value` (attribution only; the claim itself is Phase 2's) |

**Gates**
- Host `HOST_SELFTESTS_OK`.
- JVM.
- Release guard.
- Corpus 57/57.
- `CI FAST`.
- Focused `CI DEVICE` (`CadFillPickR2Test`, `CadPlanarFaceOwnerCorrectionTest`,
  `CadPlanarFaceRuntimeTest`, `CadCanvasExtrudeTest`).
- OWNER APK.

No FullSharded, no merge, no S3.

---

## Phase 2 — `CAD-V6-S2-HUD3D-R1` (after Phase 1 is OWNER-accepted)

### Scope

1. **Native dock frame.** `cadExtrudeDockFrame(anchors, camera, viewport,
   scale, …)` (`HUD3D_OPTIONS.md` §3):
   - axial billboard in the `(a, s)` plane;
   - centre on the axis past `cadExtrudeArrowPoint` by `clearWorld / r`;
   - size from the one per-frame control scale;
   - visible only while `sin(f, a) ≥ 0.35`, fading over [0.35, 0.45];
   - hidden whole when any corner is off-viewport or behind the eye.

   It exports 4 projected corners + visible + alpha as new tool-state slots
   45+. It also completes the doc comment for slots 42–44.
2. **Java.**
   - `CadExtrudeCanvasView` draws ONE operation badge (plate + `Path` glyph)
     through `Matrix.setPolyToPoly(…, 4)`.
   - The 3-glyph plate, the edge slide, the 0.35× rotation and the scale clamp
     in `CadHudPresentation.layoutPanel` are removed.
   - The badge proxy claims a Down only inside the projected quad ∪ the 48 dp
     floor (`onTouchEvent` returns false otherwise), and opens the EXISTING
     palette (extent, operations, Flip).
3. **Dimension label unchanged.**
4. **Optional:** a shared ±8° reading-side hysteresis for the leader, value and
   badge.
5. **Docs:** the `CLAUDE.md` HUD rule (`CAD-FOUNDATION-C2` panel sentences)
   rewritten to the dock rule; `PRODUCT.md` after runtime verification.

### Tests — Phase 2

| id | check |
| --- | --- |
| HUD-01 | 360° orbit sweep in 1° steps (yaw and pitch, persp and ortho): the dock centre's world position minus the arrow tip's ≡ the stated axis offset (drift 0 within 1e-5 m) |
| HUD-02 | no teleport: the projected centre moves continuously between adjacent sweep steps (bounded by the arrow tip's own screen motion + ε); an in-place 180° glyph turn at the reading wrap is the only discontinuity, and it moves no centre |
| HUD-03 | axis-on degeneracy: hidden at sin < 0.35, alpha monotone over [0.35, 0.45], never NaN; the precision surface still offers every choice |
| HUD-04 | off-viewport: any corner out → hidden whole; never a clamped or slid placement |
| HUD-05 | the hit predicate contains every pixel of the drawn quad, plus the 48 dp floor centred, and claims NO pixel outside their union (JVM, homography sampled) |
| HUD-06 | physical size bounds: the badge's projected extent follows `clamp(scale, 0.40, 1.60)` × reference |
| HUD-07 | device: from two oblique views a tap on the badge opens the palette; a tap 1 dp outside its claim reaches the cell beneath (selection changes) |
| HUD-08 | device: dimension value, typed editor and Two Sides labels are unchanged (the existing `CadCanvasExtrudeTest` assertions pass untouched) |

**Gates:** as Phase 1, plus an OWNER physical-review APK.
