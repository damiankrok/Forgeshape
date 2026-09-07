# EVIDENCE — `CAD-UX-S1`

Every command below was run on this machine against
`AVD ForgeShape_Stage006 / emulator-5580` (identity reconfirmed with
`adb -s emulator-5580 emu avd name` → `ForgeShape_Stage006`). `emulator-5554`
was never attached and never contacted. Two foreign emulators (`emulator-5560`,
`emulator-5570`) were attached and were read-only identified by
`adb devices -l` only — nothing was installed to, started on, stopped on or
tested on either.

---

## 1. Baseline and scope

```
git rev-parse HEAD   -> b46a33c67c596cd2dbda532da10ef9f913c313f5
git status --short   -> (empty)
git remote -v        -> (empty)
```

Final `git status --short` carries **no** `testdata/`, `DATA_PACKAGE_SPEC.md`,
`scripts/build-forge-corpus.ps1` or `.forge` entry.

## 2. Native self-tests — standalone runner

Built with the pinned NDK clang (`29.0.14206865`, x86_64) and run on
`emulator-5580` from `/data/local/tmp`, then removed.

```
SKETCH_UX 92 checks, 0 failed        (baseline was 52)
CAD       122 checks, 0 failed
CAD_A3     66 checks, 0 failed
HISTORY   147 checks, 0 failed
PROJECT   256 checks, 0 failed
SCENE     155 checks, 0 failed
GIZMO     167 checks, 0 failed
RUNNER_TOTAL_FAILED 0
```

The 40 new sketch-UX checks are the `CADUXS1-*` group:

| Group | Checks | Covers |
| --- | --- | --- |
| `testCanvasExtrudeAnchors` | 7 | `CADUXS1-02` — the axis is the support normal on all three world planes and on a non-principal FACE frame; the base is the profile's **area** centroid; the anchors are camera-free; the arrow exists only in Ready |
| `testCanvasExtrudeFlip` | 4 | `CADUXS1-03` — side reversed, exact depth and profile kept, two flips are the identity, refused by name outside Ready, and the generated solid grows on the other side |
| `testCanvasExtrudeScale` | 7 | `CADUXS1-09` — scale 1.0 at the reference distance, **strict monotonicity swept across the band**, both clamps engage and are named, the 48 dp floor arithmetic, degenerate camera produces nothing, draw and hit share one number, a farther camera draws smaller |
| `testCanvasExtrudeDrag` | 8 | `CADUXS1-04/06` — a one-metre axial drag adds one metre; **zoom does not change what a world displacement means**; the basis is frozen at pointer-down; a degenerate viewpoint never writes a guessed depth; a drag is clamped at the floor while a typed value is refused; a second pointer id cannot steer; cancel reports the pre-drag depth; the hit test takes the arrow and refuses what is clear of it |
| `testCanvasExtrudeSessionGesture` | 5 | `CADUXS1-04/06` at the session level — a Ready drag writes through the session; a pointer clear of the arrow still navigates; a second pointer cancels and restores; a Cancel restores; Editing still belongs to the drawing tool |
| `testCanvasExtrudeParityAndPurity` | 7 | `CADUXS1-05/07/10/11` — a typed depth and a drag reach a **bit-identical** `CadBodyState` and the same mesh; a drag + cancel + two flips leave the state bit-identical; only the head takes the control scale while the shaft is the depth; a held arrow uses the existing emphasis tag; a degenerate arrow draws nothing; the extrusion is two-valued and a third code is refused; one Extrude creates exactly one body |
| `testCanvasExtrudeRetainedSketch` | 2 | `CADUXS1-08` — Extrude consumes nothing (the committed state is bit-identical to the authored one) and the retained sketch still yields an anchor |

## 3. Startup self-test evidence

```
adb -s emulator-5580 logcat -G 64M
adb -s emulator-5580 logcat -g     -> main: ring buffer is 64 MiB
adb -s emulator-5580 logcat -c ; am force-stop ; am start ; sleep 45
adb -s emulator-5580 logcat -d
```

**22 of 22 `*_SELFTEST_OK`, 0 `_SELFTEST_FAIL` / `_FAIL:`, 0 `chatty` lines.**
`FORGESHAPE_NATIVE_VIEWPORT_OK` present. `FORGESHAPE_SKETCH_UX_SELFTEST_OK
(92 checks)`.

> A first capture through `logcat -d -s ForgeShape:V` showed 21 tokens with zero
> failures, missing only the sculpt line. Per the CLAUDE.md rule that a missing
> token with zero failures is a dropped capture until proven otherwise, it was
> re-captured unfiltered at 64 MiB and came back complete with no `chatty`
> marker. It was a capture-window artifact, not a suite that stopped.

Performance lines emitted unchanged in shape:
`FORGESHAPE_CAD_PERFORMANCE`, `FORGESHAPE_SKETCH_UX_PERFORMANCE`.

## 4. Device suites

Runner: `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass …`
(`MODE=FOCUSED_SUBSET (not full-suite evidence)`; no aggregate marker emitted,
correctly).

| Suite | Result |
| --- | --- |
| `com.forgeshape.app.CadCanvasExtrudeTest` (new) | **OK (8 tests)** |
| `com.forgeshape.app.SketchExtrudeTest` | **OK (9 tests)** |
| `com.forgeshape.app.SketchUxTest` | **OK (14 tests)** |
| `com.forgeshape.app.SpatialSketchTest` | **OK (8 tests)** |

`CadCanvasExtrudeTest` covers: the cluster absent while drawing and present in
Ready with the 48 dp floor held; Flip (side reversed, exact depth kept, panel
chips agreeing because the panel now holds no draft); the arrow axis facing the
eye so a drag holds rather than guesses (`OQ-CAD-UX-01`, asserted rather than
skipped); the exact typed depth, a refused zero, and panel/canvas parity;
drag + Flip + type + Cancel leaving the document **byte-identical** and the
fingerprint, body count and undo depth unmoved; Apply as one body and one
history step, then the retained sketch reopened in one tap, edited and finished
into the **same** body as one more step with no second body; Add/Cut/Symmetric/
Two Sides absent from both the cluster and the panel; and the camera-attached
scale staying inside its band under a real pinch while no authored value moves.

## 5. Persistence

```
scripts\build-forge-corpus.ps1 -VerifyOnly
```

**30 of 30 fixtures `OnDisk=True`** — the builder's freshly constructed bytes
are identical to the committed files. Every digest also matches the C++
encoder's own, read out of the standalone runner and out of logcat
(`FORGESHAPE_PROJECT_GOLDEN_SHA256*`). Spot values, unchanged from before the
stage:

```
construction         8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf
cad_rectangle        e2fd79c4070d1168ae883e064200438244c09a41bcf1682f9a0596b549d1b26b
mixed_cad_face       4b20f3c8ea05876850043dff28591f19855efa4c0566bebd43df11f1c0ff1529
cad_mixed_curve      3e2fa16f05e353f1745a36e165aedadbb5d5378db0c039167293aececad7078d
object_state         3c9bcd6305bcdc26ac2f6ad5db72d0f8a163acb26236a4e48f2afe12905ba608
```

`CADB` stays at v1/v2/v3, `SCNE` at v1/v2. `DATA_PACKAGE_SPEC.md` is untouched.

## 6. Builds and release symbol sanity

```
gradlew.bat :app:assembleDebug            -> exit 0
gradlew.bat :app:assembleDebugAndroidTest -> exit 0
gradlew.bat :app:assembleRelease          -> exit 0
```

`llvm-readelf --dyn-syms` over the stripped release libraries:

```
release arm64-v8a : selftest dyn-syms = 0 ; cadExtrude dyn-syms = 12
release x86_64    : selftest dyn-syms = 0 ; cadExtrude dyn-syms = 12
debug   x86_64    : selftest dyn-syms = 24   (contrast)
```

The self-tests stay out of release on both required ABIs, and the new tool is
present in both.

## 7. Device guards

```
scripts\verify-device-guards.ps1  ->  All device-guard checks PASS  (exit 0)
```

`DEV2-01..07` and `DEV3-01..06` all PASS, with no device attached required.

## 8. Cross-platform ownership

| Concern | Owner | Proof |
| --- | --- | --- |
| profile reference, depth, direction | `SketchSession` (C++) | `SketchEditorView`'s `draftDirection` int was **deleted** this stage; `git diff` shows the field gone and `currentDirection()` reading `SKETCH_EXTRUDE_DIRECTION` from native on every use |
| world anchors, growth axis | `CadExtrudeAnchors` (C++) | `CADUXS1_02_e` asserts two reads are identical and no camera enters; the type compiles and passes in the standalone NDK runner with **no JNI and no Android header** |
| the camera-attached size rule | `CadExtrudeControlScale` (C++) | `cadExtrudeControlScaleFor` is a pure function over one float, swept in `CADUXS1_09_b` with no camera at all |
| the drag | `CadExtrudeManipulator` (C++) | eight native cases; the Java layer never computes a depth |
| the flip | native | `sketchFlipExtrudeDirection` reads the session's own direction and writes the opposite; Java sends a verb, not a value |
| pixel positions, glyphs, IME, insets, scale application | Android shell | `CadExtrudeCanvasView` holds `shownDepth` for display and `sketchBodyId` for a click target, and nothing else |

`forgeshape_cad_extrude_tool.{h,cpp}` contains no `View`, `Activity`,
`MotionEvent`, `Surface`, `jobject`, `jni.h` or Vulkan type. It is compiled into
the standalone runner with the rest of the platform-neutral domain, which is the
mechanical proof.
