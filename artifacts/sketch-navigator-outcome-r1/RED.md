# SKETCH-NAVIGATOR-OUTCOME-R1 — RED (the defect, measured before any product edit)

## 1. Host: the closeout CR-06 diagnostic re-run on `8a797ff`

The diagnostic's copy of `beginSketchView` is byte-identical to `8a797ff`'s.
Output: `evidence/red/cr06_native_on_8a797ff.out`.

| Act | Session `viewFrame()` | Camera after `beginSketchView` | Control: camera framed on `viewFrame()` |
| --- | --- | --- | --- |
| B0 start | u=(1,0,0) v=(0,1,0) n=(0,0,1) | eye (0,0,250) | — |
| B1 Flip | v=(0,−1,0) n=(0,0,−1) | eye (0,0,250), view identical to B0 | eye (0,0,−250), view differs |
| B2 +90 | u=(0,−1,0) v=(1,0,0) | identical to B0 | differs |
| B3 −90 | u=(0,1,0) v=(−1,0,0) | identical to B0 | differs |

## 2. Device: `CI DEVICE` `41507903710` on `d17adef` (product = `8a797ff`)

Startup: 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines,
`liblog_dropped=0`.

**Result: `SketchNavigatorCameraTest` 9 run, 7 FAILED**, all
product-caused. Every navigator act was a real tap through the window. Every
view assertion read where U=(1,0) and V=(0,1) land on screen through the live
camera.

| Test | Failure on the baseline |
| --- | --- |
| nav02 (+90 / −90) | "+90: U at dx=253.47 dy(up)=0.0, expected up": the drawing did not turn |
| nav03 (Flip) | "flipped: V at dx=0.0 dy(up)=253.47, expected down": the view did not flip |
| nav04 (planes × flip × roll) | "plane 0 flip 0 turns 1: U at dx=253.47 …" |
| nav05 (face) | "and refused below JNI. Actual: 0": a face sketch's flip was accepted natively |
| nav06 (draw after turn + flip) | its first view assertion failed: the view never turned |
| nav08 (Back to Sketch honest) | "Back to Sketch: not flipped expected:<0.0> but was:<1.0>": the navigator kept a flip the camera never showed |
| nav10 (orientation × font) | "font 1.0 orientation 1 +90: U at dx=253.47 …" |

**Passed on the baseline, as they should:**
- **nav01:** a new CAD project is XY along +Z, ortho.
- **nav09:** with no sketch the acts are refused and the camera does not
  move.

These already worked; they are regression guards, not RED claims.

**Log:** `evidence/red/device-run-41507903710/job.log`.
**Artifacts:** `13173151620` (evidence) and `13172467304` (gradle reports).

## 3. Mutations on the GREEN tree

Host SKETCH_UX, `evidence/red/host_mutations.txt`. Each part of the fix
reverted alone:

| Part reverted | Result |
| --- | --- |
| The camera framed on `frame()`, as on the baseline | **6 failed** (NAVCAM_02 ×2, 03, 04 ×2, 05 roll) |
| `backToEditing` keeping flip and roll | **1 failed** (NAVCAM_08) |
| Face flip allowed | **2 failed** (NAVCAM_05 ×2) |
