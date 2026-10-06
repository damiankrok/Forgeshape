# CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1 — `PASS-CAD-SKETCH-DRAFTING-TOOLKIT-E2E`

TECH PASS / OWNER PHYSICAL REVIEW REQUIRED / NOT MERGED. Task branch
`feature/cad-v8-sketch-drafting-toolkit-r1`, created at `5dcb73b` (the Revolve
head). `main` stays `6c9f156`, `feature/cad-v6-sketch-face-r1` stays `bbae765`,
`feature/cad-v6-revolve-newbody-r1` stays `5dcb73b`. No merge, no FullSharded,
S3 not started. Read `BEFORE.md` (the source audit) and `DESIGN.md` (the
decisions made before implementation) first.

## Final candidate

| fact | value |
| --- | --- |
| final product SHA | `df7db66` (later commits add only docs and artifacts: `app/`, `scripts/`, `.github/`, `testdata/` identical) |
| focused DEVICE | `CI DEVICE` `37414423576` on `df7db66` — `CadSketchDraftingOwnerTest` **PASS 14/14**, 23/23 startup tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure tokens |
| final DEVICE union | `CI DEVICE` `37415689406` on `df7db66` (12 classes) — RUNNING at the time of this commit |
| final CI FAST | `CI FAST` `37415698483` on `df7db66` — success: debug, release and androidTest builds, JVM **148/148** (15 classes), release self-test guard PASS, corpus parity **66/66** byte-identical, runtime / sharding / device-guard checks PASS, `git diff --check` |
| APK | artifact `ci-fast-evidence` id `11391450004` (zip 10,090,913 bytes, zip SHA-256 `7a881a22741fcf7f354e89ec7603c5ca9635b6b64ae421f7fc1b069665ce0998`, expires 2026-10-20T04:56:14Z); inside it `app/build/outputs/apk/debug/app-debug.apk`, **14,431,018 bytes**, SHA-256 **`60ea5d03b388117379e5fa413823388e3815a0ba956de3819238d6fb1990abab`** — downloaded and hashed from that artifact, not rebuilt |

## What was built

**Construction geometry.** `SketchEntityRole {Regular, Construction}` on every
entity. Material topology ignores Construction in exactly two places
(`extractClosedProfiles`, the arrangement's source edges); snap, Trim and Extend
derive the same arrangement over `cadSketchAllCurvesView`. Construction is
selectable, snappable, dimensionable, mirrorable and a valid Revolve / Mirror
axis; drawn dashed and quieter in its own overlay range.

**Dimensions** (`forgeshape_sketch_dimension.{h,cpp}`). Twelve kinds; a
dimension stores no number — its value is derived from authored geometry; a
Driving edit rewrites geometry through one path (Line length keeps `P0` and
direction, Line angle keeps `P0` and length, Rectangle keeps its centre, Circle
its centre). Driving is allowed for six kinds; DOF conflicts refused
(`SketchDimensionConflict`); Trim refuses a dimensioned entity
(`SketchDimensionDependency`), Extend a Driving length (`SketchDimensionLocked`);
Offset and Mirror copy no dimension. Labels write `R`, `Ø`, `°`, a Reference in
parentheses; visibility Selected / All / Off.

**Snaps** (`forgeshape_sketch_snap.{h,cpp}`): endpoint, intersection, midpoint,
centre, origin, horizontal / vertical guides, grid, in two apertures (inner
8 dp, then 24 dp).

**Trim, Extend, Offset, Mirror** (`forgeshape_sketch_drafting.{h,cpp}`): pure
functions from one sketch to another applied to the STAGED sketch, so a Finish
is one transaction and Cancel costs nothing. Tool matrix in `DESIGN.md` §5.

**Mobile UI.** The Modify control under the orientation navigator opens the
sketch actions palette (Select multiple, Dimension, Make Construction / Make
Regular, Trim, Extend, Offset, Mirror, Delete, Dimensions visibility); an active
mode becomes a compact capsule in the same corner. No bottom toolbar.

**`CADB` v8** (`DATA_PACKAGE_SPEC.md` §7i): a role byte on every entity and
each sketch's dimension table, written only when a sketch carries drafting
truth. Five fixtures (`cad_construction_v8`, `cad_dimension_driving_v8`,
`cad_dimension_reference_v8`, and the refused `cad_bad_dimension_ref_v8`,
`cad_dimension_conflict_v8`); corpus 61 → **66**, the 61 older fixtures
byte-identical (only five files were added under `testdata/`) and their
verdicts unchanged (host PROJECT / CAD_FEATURE suites).

### Corrections made in this continuation (after the first two DEVICE runs)

1. **A dimension label could cover the stroke it measures** (PRODUCT). Native
   placed a label 41 dp off the measured edge without knowing the drawn chip's
   width, so an ~80 dp chip off a vertical edge reached back over it. Native now
   reports the point on the geometry each label stands off from
   (`SketchDimensionAnnotation::attach`, label stride 8 → 10), and the chrome
   pushes the 48 dp touch box along that direction until it clears the point by
   8 dp (`SketchDraftingPresentation.standOffCentre`). Chips keep the 48 dp
   floor both ways. Host `DR_DIM_20`, two JVM tests and device DEV-DR-13 pin it.
2. **Two drawn labels could overlap near the viewport edge** (PRODUCT, attempt 2
   DR-03: `Rect(173,606-382,728)` vs `Rect(2,508-185,630)`). Collisions were
   resolved on unclamped boxes and the placement then clamped one inward. A
   label whose box would leave the viewport is now HIDDEN (never clamped back
   over the geometry), and placement is unclamped.
3. **Trim / Extend were withdrawn whenever anything was selected** (PRODUCT,
   attempt 2 DR-06 / DR-11a: `sketch_action_trim must exist`). Every drawing
   tool leaves what it drew selected, so Trim was hidden exactly when a user had
   finished drawing. Both are tap modes on the touched stroke and are now
   offered whatever is selected.
4. **Closing a label's value editor without a value left the label hidden**
   (PRODUCT, attempt 3 DEV-DR-13). The chip is withdrawn while its editor stands
   in for it; `closeEditor` now restores it.

## Tests

- Host: `HOST_SELFTESTS_OK (4157 checks, 0 failed)`, 23 suites (the drafting
  checks run inside CAD_FEATURE; no new startup token).
- JVM: 148/148 (`SketchDraftingPresentationTest` gained the stand-off and
  viewport rules; the palette tests now require Trim / Extend with a selection).
- Device: `CadSketchDraftingOwnerTest`, DEV-DR-01..13 (11a / 11b), 14 tests, all
  real window touches for what lands on the drawing.

### Harness rules the device class now follows

- One stated tap point per touch, its owner asserted from the window's hit
  order before the touch; no search over candidate points. The attempt-2
  `tapEntityPoints` / `circlePoints` / `rectPoints` search was removed.
- A drag requires only its DOWN to be free of chrome: the view that takes the
  Down receives the rest of the gesture (attempts 1–2 failed DR-04, DR-05 and
  DR-08 on a drag END under a label, which a finger is not affected by). DR-08's
  offset drag goes back to its original upward direction.
- Selections and clears a journey depends on are real taps (Select tool, tap on
  the entity / on empty drawing); the native selection door is used only to read
  labels and to clean up between cases. Attempt 2's native deselects in DR-04
  and DR-07 (used to dodge labels and the Trim/Extend gap) were removed.
- DR-03 asserts the collision POLICY per label (a hidden label is hidden only by
  a standing, higher-priority label whose box it meets, or because its box
  leaves the viewport; standing labels never meet) at Selected, All, after a
  pinch out, a pinch in and a pan — instead of attempt 2's lowered count
  (`drawn >= 4`).
- DR-11a and DR-10 open the sketch through the canvas Edit Sketch control (no
  native fallback; it must be shown); DR-11b reaches a REVOLVED body's sketch
  through the precision surface's Edit Sketch (no canvas control stands on a
  revolved body); DR-11a ends with the Cancel control.
- DEV-DR-13 (new): on a rectangle's Height label (vertical edge) and a circle's
  Diameter label (45°): a tap on the label opens it, closing it brings it back,
  its own point nearest the stroke (4 dp in) is still the label's, a tap on the
  measured stroke selects the entity, and a tap 6 dp beyond the label's far side
  on empty drawing clears the selection.

## DEVICE attempts

| run | SHA | classes | result | root cause → fix |
| --- | --- | --- | --- | --- |
| `37408594416` | `ab04046` | 1 | FAIL, 9 of 13 failed | six drags started off a 1080 px portrait viewport (scenes to u = −3.5 D, HARNESS); DR-02 tap under a neighbour's label, DR-04 / DR-08 drag END under a label (HARNESS); DR-03 `most labels stand: 5`, an arbitrary count (HARNESS) → `272b3cc` (scenes moved right), then this continuation |
| `37410559758` | `272b3cc` | 1 | FAIL, 4 of 13 failed | DR-05 drag END under a label (HARNESS); DR-03 overlapping drawn labels after the edge clamp (PRODUCT 2); DR-06 / DR-11a Trim absent with a selection (PRODUCT 3) → `97673d6` |
| `37413051431` | `97673d6` | 1 | FAIL, 1 of 14 failed | DEV-DR-13: a label closed without a value stayed hidden (PRODUCT 4) → `df7db66` |
| `37414423576` | `df7db66` | 1 | **PASS 14/14** | focused class green |
| `37415689406` | `df7db66` | 12 | RUNNING | final union |

Final union: `CadSketchDraftingOwnerTest`, `CadRevolveOwnerTest`,
`CadMultiFaceOwnerTest`, `CadFillPickR2Test`, `CadPlanarFaceOwnerCorrectionTest`,
`CadPlanarFaceRuntimeTest`, `CadHud3dOwnerTest`, `CadCanvasExtrudeTest`,
`SketchExtrudeTest`, `JniBoundaryHardeningTest`, `HomeFlowTest`,
`CadVerticalSliceTest`.

## Not done / not claimed

- Deferred by design: a persistent constraint solver, a fully-defined state,
  projected / linked edges ("Project" / "Use"), drawing sheets and title blocks,
  and any Spline dimension, Trim target, Extend or Offset.
- The SELECTED-LINE length label (`SKETCH-UX-R1` E, `sketch_dimension_value`) is
  unchanged: it is centred 30 dp off the line, so on a steep line its chip
  covers about 48 dp of the line's middle. It is a pre-existing surface with
  its own approved placement and audit tests; changing it was not in this
  task's scope. Listed for the OWNER below.
- `pwsh` does not run in this session's container, so the corpus builder and
  the PowerShell guard scripts were proven only by `CI FAST` (66/66, all PASS).
- Emulator evidence closes no physical-device gate: stylus, hover, hardware GPU
  and real-device feel remain the OWNER's.

## OWNER review checklist (physical device)

1. New CAD sketch: draw a rectangle and a line through it; Modify → Make
   Construction on the line — it is dashed and the fill ignores it.
2. Select the rectangle → Modify → Dimension → Width and Height; tap a number,
   type `12+3` (or a value), Apply — the size changes exactly, centre fixed.
3. Dimensions → All, pinch and pan: numbers follow, overlaps hide (never stack),
   and no number sits on the edge it measures; tap right beside a number on the
   edge — the edge is selected, not the number.
4. Draw two crossing lines; with the last one still selected, Modify → Trim is
   offered; tap a piece between crossings — it goes. Extend a short line to
   another line and to a circle.
5. Offset a line by dragging, then type an exact value, Confirm; Done without
   Confirm creates nothing.
6. Select multiple → three items → Mirror → tap a straight line → Confirm.
7. Try to Trim a dimensioned entity and Extend a Driving length — both refused
   by name; delete the dimension from its number, then both work.
8. Save, reopen: roles and dimensions come back, nothing selected.
9. Extrude a dimensioned rectangle, Edit Sketch on the body, change the Width,
   Finish — the same body regenerates, one Undo. Repeat on a Revolve body via
   the precision panel's Edit Sketch.
10. Check the selected-line length label on a steep line (pre-existing; see
    above) and say whether it should follow the same stand-off rule.
