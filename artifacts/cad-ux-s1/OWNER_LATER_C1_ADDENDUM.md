# OWNER LATER — `CAD-UX-S1-C1` addendum (the Sketch → feature-preview view)

**Nothing in this document is OWNER ACCEPTED.** It extends
`OWNER_LATER_TEST_PACK.md` rather than replacing it: everything in that pack is
still to be judged, and this adds the one thing `CAD-UX-S1-C1` changed — the
camera transition that makes the extrude arrow reachable.

The technical side is proved by the native `CADUXS1C1-*` cases and by the device
`CadCanvasExtrudeTest` (9 tests, `emulator-5580` / `ForgeShape_Stage006`).
What is below is look-and-feel, which no emulator can judge.

---

## 1. What changed, in one sentence

A sketch is **drawn** through the exact straight-on view it always was, and
**Finish Sketch** now moves to a second view in which the extrusion axis has a
real screen projection — so the arrow can be dragged, where before it pointed
straight at the eye.

---

## 2. The provisional constants this added

| Constant | Value | Why this value | Where |
| --- | --- | --- | --- |
| `kCadFeatureViewMinAxisSine` | `0.35` (≈ 20.5°) | "Usable" measured as the sine between the view direction and the extrusion axis. `solveAxisParameter` degenerates below `kGizmoAxisParallelDenominator` (0.02, ≈ 8.1°); a view that merely cleared that would resolve and still be unusable, because the shaft would be a few pixels long. 0.35 leaves real headroom. Symmetric about edge-on, so it needs no direction convention | `forgeshape_cad_extrude_tool.h` |
| `kCadFeatureViewObliqueRadians` | `0.62` (≈ 35.5°) | How far off the support normal the fallback stands. sin = 0.58, comfortably past the threshold; far enough that the axis projects to a real segment, near enough that the profile still reads as the shape just drawn rather than edge-on | same |
| `kCadFeatureViewAzimuthRadians` | `0.90` | Where around the normal it stands, in the sketch frame's own `(u, v)`. Any azimuth gives the same axis projection, so this is a fixed choice purely so one sketch always produces one view | same |
| `kCadFeatureViewAzimuthAttempts` | `4` | The orbit pose clamps pitch, so the candidate is re-derived from the clamped angles and re-measured; a failure steps the azimuth a quarter turn. At most one quadrant can aim the tilt up the meridian, so four is a proof rather than a hope | same |

Each is a one-line change if a different feel is wanted. **None is approved.**

---

## 3. What to look at

Open Home → **New Project → CAD** (the first-project bootstrap, which has no
earlier view at all), and separately an existing project → **New Sketch** on a
face (which does).

1. **The transition itself.** Finish Sketch. Is the swing to the oblique view
   readable, or does it feel like a jump? It is instantaneous today — there is
   no animation, deliberately, because a camera tween is its own decision.
2. **Does the fallback look natural on each support?** Check **XY**, **XZ**,
   **YZ** and a planar **face** sketch. XZ is the interesting one: its normal is
   world **+Y**, and the fallback is built with no world up in it.
3. **Is the profile still legible from the oblique view**, or is 35.5° too far
   (the drawing foreshortens) or too near (the arrow is still short)?
4. **The prior-view path.** From a project you were already orbiting, does
   getting your own view back — re-centred on the new profile — feel right, or
   would the fixed oblique view be less surprising every time?
5. **Arrow visibility and occlusion**, near and far: does the shaft read against
   the extrusion preview, and is it ever hidden behind the solid it measures?
6. **The drag itself.** Grab the arrow and pull. Is the corridor findable, does
   the depth track the finger, and does the cluster stay out of the way?
7. **Back to Sketch and Edit Sketch** both restore the straight-on view. Is that
   return as legible as the departure?
8. **In `Ready`, a single finger that misses the arrow now orbits.** Is that the
   right meaning there, or should it stay inert as it was?
9. **Portrait and landscape**, both **handedness** settings, and all **five
   palettes**.
10. **Orientation navigator coexistence**: the navigator is a sketch control and
    is present while drawing. Does anything about the preview view read as
    contradicting it?

---

## 4. Screens worth capturing

1. the sketch mid-drawing (the straight-on view), and immediately after Finish
   Sketch (the preview) — the pair is the whole change;
2. the same pair on **XZ**, the world-up case;
3. a face-supported sketch, before and after Finish;
4. mid-drag on the arrow;
5. the first-project bootstrap (no prior view) beside a project where the prior
   view was restored;
6. one light ground and one dark ground.

---

## 5. Explicitly not in this correction, and not to be judged as missing

No animation or easing on the transition; no continuous auto-orbit (this is a
view *initialization*, after which the camera is an ordinary 3D one); no change
to the sketch authoring camera, the orientation navigator, the adaptive grid,
snapping or any authored coordinate; no change to `Add`/`Cut`/`Symmetric`/
`Two Sides`, which still do not exist. Two known items stay **unfixed** and were
not touched here: `SketchOverlayStyle::Dimension` renders fully transparent, and
`beginSketchView` reads the sketch's authoring frame rather than its view frame.
