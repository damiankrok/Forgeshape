# EVIDENCE — `CAD-UX-S1-C1` (feature-preview camera, reachable Extrude drag)

**Baseline:** `59f0dcf1ebc57ba4b890c0f312ca8254ad7bb8c2` · worktree clean at
start · no remote · **no `.forge` byte, section, version, fixture or corpus
digest moved.**

**Automated elapsed:** 11 min 18 s (18:09:37Z → 18:20:55Z), against a
`TEST-OWNER-03` target of ≤ 20 min and a hard stop of 30 min.

**Device:** `emulator-5580`, confirmed `ForgeShape_Stage006` by
`adb -s emulator-5580 emu avd name`. `emulator-5554` was not attached and was
never contacted. `emulator-5560` / `emulator-5570` were seen in `adb devices`
and not touched.

---

## Root cause, and the exact policy

`OQ-CAD-UX-01` was not "the sketch camera is locked". The lock is what makes
authoring exact and it is untouched — `frameSketchView` still aims along the
support normal (`forgeshape_camera.cpp:161`) and `applyOrbit` still returns
early while `sketchView_` is true (`forgeshape_camera.cpp:350`). The defect was
that ONE view was doing two jobs: the view a sketch is drawn in is aimed exactly
along the support normal, and that normal IS the extrusion axis, so the arrow
drawn in it has zero screen extent and `solveAxisParameter` has nothing to
resolve against.

The correction is a bounded view transition, deliberately not a second gesture
model — a raw screen delta would give the drag a meaning the world axis does not
have.

`cadFeatureViewPose` (`forgeshape_cad_extrude_tool.{h,cpp}`) is the whole
policy, a pure function over values: a current pose, an optional prior pose, the
sketch frame and the anchors in; one `CameraController::Pose` and a
`CadFeatureViewSource` out. No scene, no session, no renderer, no Android type.

1. **`PriorView`** — the user's own pre-sketch 3D pose, given back whole
   (direction, distance, projection, span) with the TARGET moved onto the work
   anchor, when `cadFeatureViewAxisSine(priorDirection, axis) >=
   kCadFeatureViewMinAxisSine` (0.35, ≈ 20.5°, chosen clear of
   `kGizmoAxisParallelDenominator`'s 0.02 ≈ 8.1°).
2. **`ObliqueFallback`** — otherwise, and always for the first-project
   bootstrap, which has no prior view at all. The eye stays on the `+n` side the
   sketch was seen from and leans `kCadFeatureViewObliqueRadians` (0.62 rad,
   ≈ 35.5°) in the frame's own `(u, v)` at `kCadFeatureViewAzimuthRadians`.
   **No world up enters the construction**, which is why an XZ sketch's world
   `+Y` normal is not a special case. Because the orbit pose CLAMPS pitch, the
   candidate is re-derived from the clamped angles and re-measured; a failure
   steps the azimuth a quarter turn, bounded at
   `kCadFeatureViewAzimuthAttempts` (4) — at most one quadrant can aim the tilt
   up the meridian. The sketch's projection and orthographic span are kept, so
   the transition is a TILT and not a reframe.
3. **`Unavailable`** — a degenerate frame or invalid anchors install NO view.
   Fail closed, on the manipulator's own hold-the-last-good-value terms.

`beginExtrudeFeatureView` in `forgeshape_jni.cpp` is the adapter. It runs inside
the same `g_stateMutex` as the `finish()` that reached `Ready`, installs through
`restorePose` (the one clamped installer, which also leaves the sketch view),
and **does not consume `g_sketchSavedPose`** — so `endSketchView` still returns
the user's pre-sketch view on cancel and on commit, unchanged from S1.
`sketchBackToEditing` now calls `beginSketchView()`; `sketchBeginEdit` already
did.

One gesture rule changed, stated once: in `Ready` an unclaimed single pointer is
handed to the camera instead of being swallowed. While the sketch is being DRAWN
the old rule stands verbatim.

---

## Files changed

| File | What |
| --- | --- |
| `app/src/main/cpp/forgeshape_cad_extrude_tool.h` | section 4: `CadFeatureViewSource`, the four constants, `cadFeatureViewDirection` / `…YawPitch` / `…AxisSine` / `…Usable`, `cadFeatureViewPose` |
| `app/src/main/cpp/forgeshape_cad_extrude_tool.cpp` | the policy, plus file-local `clampToRange` and `unitLength` |
| `app/src/main/cpp/forgeshape_jni.cpp` | `beginExtrudeFeatureView`; the call in `sketchFinish` and its `FORGESHAPE_SKETCH_FEATURE_VIEW:` log; `beginSketchView()` in `sketchBackToEditing`; the `Ready` navigation case in the sketch touch arbitration |
| `app/src/main/cpp/forgeshape_sketch_ux_selftest.cpp` | `testFeaturePreviewView` — 13 new checks, `CADUXS1C1_02/03/05/09` |
| `app/src/androidTest/.../CadCanvasExtrudeTest.java` | `e2eCaduxs1_03` (the blocked-drag case) replaced by `e2eCaduxs1c1_03` (the real drag) and `e2eCaduxs1c1_06` (the round trip) |

No `.forge`, `DATA_PACKAGE_SPEC.md`, testdata, fixture, script or shader file
was touched.

---

## Acceptance map

| ID | Evidence | Verdict |
| --- | --- | --- |
| `CADUXS1C1-01` | baseline `59f0dcf1…` exact, `git status --short` empty at start, `git remote -v` empty; final diff is 5 source files + docs/artifacts, no CADB/schema/testdata change | **PASS** |
| `CADUXS1C1-02` | native `CADUXS1C1_02_a/b` over XY/XZ/YZ: the sketch view is still `sketchViewActive()`, orthographic, and aimed to 1e-5 along the support normal, and its axis sine is < 1e-4. Device `e2eCaduxs1c1_03` asserts a horizontal sketch span stays horizontal on screen while drawing | **PASS** |
| `CADUXS1C1-03` | native `CADUXS1C1_03_a..g`: the tool's orbit direction equals the camera's over four poses; the fallback is usable on all three world planes from a null prior (bootstrap); XZ / world `+Y` needs no special case and lands ≈ 0.2 rad inside the clamp; a face frame swept over 19 tilts × 8 azimuths (152 orientations) is always usable and always inside the clamp; the prior-view path and the head-on refusal are both proved; the fallback is bit-deterministic | **PASS** |
| `CADUXS1C1-04` | device `e2eCaduxs1c1_03` on `emulator-5580`: after Finish Sketch the half-shaft is > 20 px (it was ~0 before C1); a real `MotionEvent` drag from native's own projected tip along the projected axis raises the depth by > 0.05 m, never NaN, never negative, direction untouched. `CadCanvasExtrudeTest` **OK (9 tests)** | **PASS** |
| `CADUXS1C1-05` | native `CADUXS1C1_05_a` — the arrow hit-tests and a one-metre axial drag is one metre of depth from the preview view on all three planes, with `lastSolve() != Unresolvable`; `CADUXS1C1_05_b` — a preview drag and a typed value produce a bit-identical `CadBodyState` (`sameCadBodyState`) and Flip keeps a positive depth. Device: the Flip tap in `e2eCaduxs1c1_03`, and `e2eCaduxs1_04` / `_05` unchanged | **PASS** |
| `CADUXS1C1-06` | device `e2eCaduxs1c1_06`: Finish → preview usable → `Back to Sketch` → the aligned view is back (a sketch axis on a screen axis) with the undo depth still 0 → Finish again → usable again. `e2eCaduxs1_06and07` still reopens the retained sketch in one tap, edits and finishes into the SAME body as one more step | **PASS** |
| `CADUXS1C1-07` | native `CADUXS1C1_03_b` covers the bootstrap (null prior) and `_03_d` the face-supported frames; device `SketchExtrudeTest` **OK (9 tests)** covers the first-project and plane paths. The badge still reads `New Body` and `e2eCaduxs1_08` still finds no `Add`/`Cut` | **PASS** |
| `CADUXS1C1-08` | native `CADUXS1C1_09_a` (a `CadBodyState` is unchanged across the transition, and the anchors are bit-stable); device `e2eCaduxs1c1_03` asserts `constructionUndoDepth() == 0` after the drag, and `e2eCaduxs1_05` still leaves the document byte-identical after drag + Flip + type + Cancel. `build-forge-corpus.ps1 -VerifyOnly`: **30/30 fixtures `OnDisk=True`, 0 false**, every digest identical to the pinned values | **PASS** |
| `CADUXS1C1-09` | source-backed: `cadFeatureViewPose` takes and returns only camera-domain values, is in platform-neutral C++ beside the manipulator, and appears in no `CadBodyState`, no codec, no `.forge` field and no Java field. `CADUXS1C1_09_a` proves nothing is written into the session; `_09_b` proves it fails closed. `forgeshape_jni.cpp` holds the adapter and nothing else | **PASS** |
| `CADUXS1C1-10` | 11 min 18 s automated, inside the ≤ 20 min target and well inside the 30 min hard stop | **PASS** |

---

## Tests run, and their results

| What | Result |
| --- | --- |
| Standalone native sketch-UX runner (NDK x86_64 clang → `emulator-5580`) | **105 checks, 0 failed** (was 92 before C1) |
| Startup self-tests, 64 MiB ring buffer (`logcat -g` confirms) | **22/22 `*_SELFTEST_OK`, 0 `_SELFTEST_FAIL`/`_FAIL:`, 0 `chatty`**, then `FORGESHAPE_NATIVE_VIEWPORT_OK`. `FORGESHAPE_SKETCH_UX_SELFTEST_OK (105 checks)` |
| `CadCanvasExtrudeTest` on `emulator-5580` | **OK (9 tests)** |
| `SketchExtrudeTest` on `emulator-5580` (the touched path) | **OK (9 tests)** |
| `build-forge-corpus.ps1 -VerifyOnly` | **30/30 `OnDisk=True`, 0 false**, digests unchanged |
| `:app:assembleDebug` + `:app:assembleRelease` | **exit=0**, both APKs written |
| Release symbol sanity, both ABIs | `selftest` dynamic symbols **0** in `arm64-v8a` and `x86_64`; `cadFeatureView*` **6** and the extrude-tool symbols **13** present in each |
| `verify-device-guards.ps1` | **All device-guard checks PASS** (`DEV2-01..07`, `DEV3-01..06`) |

`-FullSharded` was **not** run, as instructed.

### Iterations

One red/green iteration. The first standalone run was 104/105: `CADUXS1C1_09_b`
failed because a degenerate frame normal normalizes to the zero vector rather
than failing, so the fallback built its lean out of `u` and `v` alone and
returned a confident view perpendicular to the axis. Fixed by `unitLength`,
which is the check that tells a degenerate frame axis from a good one; the
rerun was 105/105.

---

## Deviations, debt and discoveries

* **One CLAUDE.md rule was refined, not broken.** "While a sketch is open the
  single-finger gesture belongs to the sketch and never orbits" now distinguishes
  `Editing` (unchanged — an orbit would take the aligned view away under the
  finger placing a point) from `Ready` (the drawing is done and there is no
  aligned view left to protect). The task prompt §3.2 explicitly asks for
  orbit/pan/zoom under the general 3D camera contract after Finish Sketch.
* **The `PriorView` path moves the TARGET** onto the work anchor while keeping
  the user's direction, distance, projection and span. Without it the arrow can
  be off-screen when the sketch is far from where the camera was last pointed,
  which would defeat the reachability this correction exists to deliver.
* **The `ObliqueFallback` keeps the sketch's orthographic projection.** The two
  paths therefore differ in projection — path 1 gives back whatever the user
  had, path 2 changes only the direction. Each is defensible on its own terms
  and the alternative (forcing perspective) is a projection decision this
  correction was not asked to take. Listed for owner judgement in the addendum.
* **Commit and Cancel behaviour is unchanged**, per §3.5: `endSketchView` still
  restores the pre-sketch pose, so Apply returns the user to the view they had
  before the sketch rather than leaving them in the preview. Noted in the
  addendum as something to look at.
* **Two known items stay unfixed and were not touched**, exactly as instructed:
  `SketchOverlayStyle::Dimension` renders fully transparent
  (`forgeshape_renderer.cpp:1720-1741` has no case for it), and
  `beginSketchView` reads the sketch's authoring frame rather than its view
  frame, so the orientation navigator's flip and roll may not move the camera.
  Neither is claimed fixed anywhere.
* **No `CAD-EXT-R1` work, no `Add`/`Cut`/boolean, no `CADB` v4/v5, no schema or
  fixture change, and no broad camera/navigation redesign.**
