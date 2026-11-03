# SKETCH-NAVIGATOR-OUTCOME-R1 — ACCEPTANCE MATRIX

**Evidence sources**

| Label | Source |
| --- | --- |
| H-RED | CR-06 host diagnostic on `8a797ff` |
| D-RED | `CI DEVICE` `41507903710` on `d17adef` (product = `8a797ff`): 7/9 FAIL |
| M | host mutations on the GREEN tree |
| H | SKETCH_UX `NAVCAM_*` with a real `CameraController` through `frameCameraOnSketchView` |
| D | `CI DEVICE` `41517008402` on `28cb315`: 69/69 |

On the device, every view assertion reads where U=(1,0) and V=(0,1) land on
screen through the live camera. A navigator flag alone is never the evidence.

| ID | Observable result | Before | After | Evidence |
| --- | --- | --- | --- | --- |
| NAV-01 | A new CAD project opens XY along +Z, exact ortho, ready to draw | already correct | correct | D `nav01` (real taps Home → New Project → CAD: world +X right, +Y up, ortho, U right / V up); H `NAVCAM_01` (eye on +n, det +1) |
| NAV-02 | +90 / −90 turn the real view; inverses and 4×90 restore it | camera unchanged: H-RED B2/B3 identical; D-RED nav02 "U at dx=253, expected up" | +90: U up, V left. −90: U down, V right. Inverse and 4×90 within 1.5 px. | D `nav02`; H `NAVCAM_02` (exact view-matrix equality); M: 2 fail |
| NAV-03 | Flip shows the back of the plane, flips back, no mirror | camera unchanged: H-RED B1; D-RED nav03 "V … expected down" | U right, V down; eye on −n; view det +1; flip twice = start | D `nav03`; H `NAVCAM_03`; M: 1 fail |
| NAV-04 | XY / XZ / YZ × Flip × roll: consistent navigator, ortho, correct axes | D-RED nav04 fails at the first roll | every one of 3 planes × 2 flips × 4 turns matches the rule, ortho, proper, eye on the named side | D `nav04` (plane chips by real taps); H `NAVCAM_04` ×3; M: 2 fail |
| NAV-05 | Face sketch: camera on the real support; TopoRef / lineage untouched; no world plane; no Flip | native flip ACCEPTED on a face (D-RED nav05 "Actual: 0"); roll did not move | Flip chip disabled AND refused below JNI (`InvalidWorkplane`); roll turns the view; TopoRef producer and lineage unchanged | D `nav05` (sketch on a real CAD body's far cap via the spatial chooser); H `NAVCAM_05` ×2 (a rotated, offset face frame); M: 2 fail |
| NAV-06 | After a turn and a flip, drawing and picking land where the user sees | first view assertion failed (view never turned) | a real drag between the pixels of rendered world (1,0,0) and (2,1,0) draws exactly (1,0)–(2,1) | D `nav06`; H `NAVCAM_06` (touch → drawn round trip for asymmetric points in all 24 views) |
| NAV-07 | View acts change no geometry, unit, dimension, history, fingerprint, `ObjectId` or `.forge` | — | entities, project bytes (294 B), fingerprint `22a5519654955ab0`, Undo 0 / Redo 0 and body count identical before and after 4 view acts | D `nav06` truth string; H `NAVCAM_07` (`sameCadSketch`, frame unchanged) |
| NAV-08 | Cancel, Finish / Back to Sketch and Commit restore the right view; no unwanted project change | Back to Sketch left the navigator "flipped" over an aligned camera (D-RED nav08) | Cancel and Commit restore the pre-sketch PERSPECTIVE pose and projection (pixels within 1.5 px). Back to Sketch: aligned view and a navigator that says so. Commit is exactly one Undo step. | D `nav08`; H `NAVCAM_08` ×2; M: 1 fail |
| NAV-09 | Support-plane change only while empty; refusals with no sketch and on a face | — | `SketchNotEmpty` after geometry (plane stays XY); `NotSketching` for flip / roll with no sketch; camera pixels unchanged | D `nav04` tail, `nav09`; H `NAVCAM_09`; existing `CADUXR1_15/16` |
| NAV-10 | Real taps; portrait / landscape × font 1.0 / 1.5 | D-RED nav10 fails at +90 | +90 and Flip by real taps work in all four, with captures | D `nav10` (`touchId` asserts each tap lands on the chip itself, not on anything over it) |
| NAV-11 | Regression: sketch → profile → Extrude / staged edit, history, import 1006, body placement, face choice | — | `SketchUxTest` 14, `SpatialSketchTest` 8, `SketchExtrudeTest` 9, `CadFaceSupportedPlacementTest` 2, `ImportGlbSketchGuardTest` 5, `CadRetainedSketchOwnerTest` 22: all pass | D |
| NAV-12 | No format or fixture change; gates; artifact SHAs | — | no `.forge` / `CADB` / spec / fixture diff; corpus 72; FAST parity 72/72 | `TESTS.md`, `SUMMARY.md` |

**Risks noted, not changed**
- `sketch_navigator_flip` hit area: 36.19 dp historically (CR-06-S6 F-01).
- Physical device, stylus, TalkBack: UNVERIFIED.
