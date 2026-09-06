# Stage 020M — automated results

Run under `TEST-OWNER-03`: target ≤ 20 minutes of automated verification, hard
stop 30. **No `-FullSharded`.** Focused, high-value correctness only.

Device: `emulator-5580`, confirmed `ForgeShape_Stage006` by `adb -s
emulator-5580 emu avd name` before anything was installed. `emulator-5554` was
never contacted.

## A. The resize solver and the domain acts — `DIM020M-01..16`

`forgeshape_body_dimensions_selftest.cpp`, run two ways: as a standalone
NDK-clang binary on the device (the fast loop) and in the app at startup.

```
FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK (97 checks, 0 failed)
```

| Case | What it establishes | Result |
| --- | --- | --- |
| DIM020M-01 | `dimension = local extent × absolute scale`, for all six primitives; the plane's local Y is a truthful zero | PASS |
| DIM020M-02 | Three non-zero Euler angles and a moved position leave the reported dimensions bit-identical | PASS |
| DIM020M-03 | An exact dimension moves only the intended scale axis; rotation and the other two dimensions are untouched | PASS |
| DIM020M-04 | The negative-side world point is stationary; the opposite face is the one that moved | PASS |
| DIM020M-05 | The positive-side world point is stationary | PASS |
| DIM020M-06 | Centre writes no position, on any axis, while still resizing all three | PASS |
| DIM020M-07 | Both one-sided anchors hold on a body turned 23°/−61°/142°, **and** on deliberately NON-CENTRED bounds | PASS |
| DIM020M-08 | Zero, negative, NaN, ±∞, an out-of-range axis and one bad multiplier all refuse, mutate nothing and record no step | PASS |
| DIM020M-09 | A plane's zero-thickness Y is refused `DegenerateAxis`; its real axes still resize | PASS |
| DIM020M-10 | One commit is one step; Undo restores Position **and** Absolute Scale exactly; Redo reapplies both; a repeat is `Unchanged` and records nothing | PASS |
| DIM020M-11 | The identity is 1, and committing it changes nothing and records nothing | PASS |
| DIM020M-12 | `6.64 × 2 = 13.28` per axis; no position moves; rotation untouched | PASS |
| DIM020M-13 | Reopening after a commit starts from the identity, not from the last multiplier | PASS |
| DIM020M-14 | One Relative Scale Apply is one step; Undo/Redo exact | PASS |
| DIM020M-15 | Locked, hidden, unknown and Imported all refuse by their own name; nothing mutates; the controls are withdrawn for each | PASS |
| DIM020M-16 | A project reached by a dimension edit encodes **byte-for-byte identically** to one reached by typing the same numbers into the existing transform, and decodes through the ordinary decoder | PASS |

Tolerances are stated in the suite: `1e-12` for the pure-double scale and
dimension arithmetic, `1e-5` m for a world point recomposed through
`rotationMatrixFromEuler`, whose matrix is float by renderer contract. That is
"exact within the current transform tolerance" and is what DIM020M-07 asks for.

## B. Startup self-tests — twenty-one suites, 3273 checks

```
FORGESHAPE_CAMERA_SELFTEST_OK (119)          FORGESHAPE_PROJECT_SELFTEST_OK (256)
FORGESHAPE_PICKING_SELFTEST_OK (174)         FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK (24)
FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK (91)     FORGESHAPE_GLTF_EXPORT_SELFTEST_OK (93)
FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK (100)   FORGESHAPE_GLTF_IMPORT_SELFTEST_OK (189)
FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK (121)  FORGESHAPE_CAD_SELFTEST_OK (122)
FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK (126)  FORGESHAPE_CAD_A3_SELFTEST_OK (66)
FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK (105)     FORGESHAPE_SKETCH_UX_SELFTEST_OK (52)
FORGESHAPE_CONE_CAPSULE_SELFTEST_OK (163)    FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK (97)
FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK (494)
FORGESHAPE_RENDER_SHADING_SELFTEST_OK (412)  FORGESHAPE_NATIVE_VIEWPORT_OK
FORGESHAPE_SCENE_SELFTEST_OK (155)
FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK (147)
FORGESHAPE_GIZMO_SELFTEST_OK (167)
```

Zero `_SELFTEST_FAIL` and zero `_FAIL:` lines. Captured with the log ring buffer
at 64 MiB, confirmed by `adb -s emulator-5580 logcat -g` before the capture.

**`FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK` is the twenty-first token**, emitted
last. `CLAUDE.md`, `README.md` and `PROJECT_STATUS.md` were updated to say
twenty-one rather than twenty.

**DIM020M-16 has a second, independent witness**: every one of the thirty
`.forge` corpus digests the project suite pins is unchanged, which is what
`FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)` above means.

## C. The one device journey — `E2E-DIM020M-01`

`scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass
com.forgeshape.app.BodyDimensionsSmokeTest`

```
com.forgeshape.app.BodyDimensionsSmokeTest:.
Time: 5.784
OK (1 test)
MODE=FOCUSED_SUBSET (not full-suite evidence)
```

One flow, driven entirely through semantic ids and never a screen coordinate:

1. Transform offers **Dimensions** and **Relative scale**, both meeting the
   48 dp hit floor and both carrying a content description.
2. The transform gizmo is up before Dimensions is entered.
3. Entering Dimensions **withdraws the gizmo**, brings up the anchor selector on
   centre, and stands the three labels in the viewport.
4. Typing `3` into the X label in centre mode sets the X dimension to exactly
   3 m, **moves no position**, and is exactly one history step.
5. Switching the anchor to the negative side and typing `6` sets the X dimension
   to 6 m, **does** move the position, and is again one history step.
6. Opening Relative Scale closes Dimensions mode, and all three multipliers read
   `1`.
7. Applying `×2` on X makes the stored Absolute Scale exactly twice what it was,
   in one history step.
8. **Reopening Relative Scale shows `1`, `1`, `1` again.**

## D. Build sanity

`gradlew.bat :app:assembleDebug :app:assembleRelease --offline` — `exit=0` on the
final tree. `llvm-readelf --dyn-syms` over the release
`arm64-v8a/libforgeshape_native.so` finds **0** symbols matching `selftest`, so
the new suite is debug-only exactly as every other one is. No broad or unrelated
suite was rerun; no aggregate was attempted.

## E. Confirming run on the final tree

Every result above was re-taken after the last source edit, because editing any
source invalidates an earlier pass:

- twenty-one `*_SELFTEST_OK`, **0** `_SELFTEST_FAIL` / `_FAIL:`, then
  `FORGESHAPE_NATIVE_VIEWPORT_OK`;
- `BodyDimensionsSmokeTest` **OK (1 test)** in 5.784 s;
- `FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK (97 checks, 0 failed)` on the
  standalone runner, which was then removed from `/data/local/tmp`;
- `verify-device-guards.ps1` — all `DEV2-01..07` and `DEV3-01..06` PASS over 19
  executable surfaces, with no device attached to it.

## Elapsed

| Step | Wall clock |
| --- | --- |
| Standalone NDK-clang compile + push + run of `DIM020M-01..16` | ~2 min |
| `:app:assembleDebug` | ~3 min |
| Install, launch, settle, log capture | ~2 min |
| Focused instrumented run (test-APK build + install + run) | ~2 min |
| Final-tree confirming pass: debug + release build, reinstall, relaunch, re-run both suites, device guards | ~7 min |
| **Total automated verification** | **≈ 16 min** |

Inside the 20-minute target and well inside the 30-minute hard stop.
`EVIDENCE-TIME-BLOCKED` was not reached.

## Not captured, and why

**No screenshot of Dimensions mode.** It was offered only "if essentially
free", and it is not: reaching the mode by hand needs viewport taps by screen
coordinate, which no test or evidence script in this repository may do, and
adding a visual-evidence suite for one optional image is exactly the testing
this stage was told to leave to the OWNER. The mode's appearance is item 1 of
`OWNER_LATER_TEST_PACK.md`.

**No `-FullSharded` aggregate**, by policy. The `SpatialSketchTest`
suite-isolation defect recorded before this stage is unfixed and unchanged; it
blocks nothing while `TEST-OWNER-03` forbids an aggregate, and it will still be
waiting for the next test-hardening batch.
