# MIRROR-01 — automated results

Run under `TEST-OWNER-03`: target ≤ 20 minutes of automated verification, hard
stop 30. **No `-FullSharded`.** Focused, high-value correctness only.

Device: `emulator-5580`, confirmed `ForgeShape_Stage006` by
`adb -s emulator-5580 emu avd name` before anything was installed.
`emulator-5554` was never contacted. A second, foreign target
(`emulator-5560`, `CarPassport_API_36`) was attached throughout; every `adb`
and every Gradle/runner invocation was explicitly scoped to `emulator-5580`.

## A. The mirror arithmetic and the domain act — `MIRROR01-01..12`

`forgeshape_mirror_selftest.cpp`, run two ways: as a standalone NDK-clang binary
on the device (the fast loop) and in the app at startup.

```
FORGESHAPE_MIRROR_SELFTEST_OK (62 checks, 0 failed)
```

| Case | What it establishes | Result |
| --- | --- | --- |
| MIRROR01-01 | XY reflects world Z exactly and leaves X and Y bit-identical; a zero coordinate reflects to `+0.0`, never `-0.0`; the plane→axis mapping and the transport index round-trip and refuse out of range | PASS |
| MIRROR01-02 | XZ reflects world Y exactly, X and Z untouched | PASS |
| MIRROR01-03 | YZ reflects world X exactly, Y and Z untouched | PASS |
| MIRROR01-04 | `det(R') ≈ +1` for **three planes × ten poses** — so the mirrored orientation is a PROPER rotation in every case, never a reflection smuggled into the matrix | PASS |
| MIRROR01-05 | Absolute Scale is carried across unchanged and strictly positive, and the position reflects exactly, over the same 30 combinations | PASS |
| MIRROR01-06 | The identity `Model_mirror(q) == F · Model_source(Qx·q)` over three planes × ten poses × six probe points, including points no primitive generates — the algebra, with no symmetry assumed | PASS |
| MIRROR01-07 | A box's local geometry is closed under `Qx`, **and** its mirrored world geometry IS the reflection of the source's, over every plane and pose | PASS |
| MIRROR01-08 | The same, parameterized over **all six** current Construction primitives (box, cylinder, sphere, cone, capsule, plane) | PASS |
| MIRROR01-09 | A fresh `ObjectId`; the source's shape copied exactly; the SOURCE unchanged in placement, shape **and** mesh revision (not re-tessellated); the solved placement applied; the derived `Bracket Mirror` / `Bracket Mirror 2` name; no Frozen Sculpt Mesh; hidden and locked inherited while the source keeps both; Duplicate's own `Bracket copy` unchanged by the shared rule | PASS |
| MIRROR01-10 | One Mirror is exactly one step; Undo removes ONLY the reflection and restores the previous active body; Redo restores the SAME `ObjectId` with the same placement, name and source, active again; an undone mirror's id is never reused | PASS |
| MIRROR01-11 | Imported Mesh → `NotConstruction`, CAD Body → `NotCad`, sculpt truth → `HasSculptTruth`, unknown body, an open edit, and a non-finite placement — each refused by name, no `ObjectId` minted, no history step, nothing written | PASS |
| MIRROR01-12 | A mirrored project encodes **byte-identically** to one whose second body was hand-placed at the same numbers; it decodes through the ordinary decoder as a plain Construction project with no CAD and no import, reloads with the reflection's placement exact, and starts a fresh empty history | PASS |

The poses `MIRROR01-04..08` run over are Section 12's whole list in one table:
identity, translated off every plane, turned about each axis alone, turned about
all three at once with a non-uniform scale, standing exactly ON the plane,
crossing it, at the extremes of a valid positive scale (`1e-4` and `1e4`), and
past a whole turn (400° / −390° / 725°). The plane primitive with its
zero-thickness local Y is one of the six in `MIRROR01-08`.

## B. The one device journey — `E2E-MIRROR01-01`

`scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass
com.forgeshape.app.MirrorSmokeTest`

```
com.forgeshape.app.MirrorSmokeTest:.
Time: 5.193
OK (1 test)
```

`MODE=FOCUSED_SUBSET (not full-suite evidence)`, as the runner itself reports.

The journey: place the body at `(2.5, −1.5, 0.75)` turned `31° / −47.5° /
118.25°` with a non-uniform scale `(1.5, 2.5, 0.5)`; open Objects; open the
row's overflow; press **Mirror**; assert the chooser offers all three planes,
each at the 48 dp floor with a content description, and that **opening it
created nothing and recorded no step**; choose **YZ**; assert one new body, one
history step, a fresh `ObjectId`, the source's nine values bit-identical, the
reflection's X negated with Y and Z untouched, the Absolute Scale carried across
and strictly positive, a new row, and the chooser closed; **Undo** and assert
only the reflection left and the previous selection restored; **Redo** and
assert the same `ObjectId` back, with the same reflected placement, active.

## C. Regression and build sanity

| Check | Result |
| --- | --- |
| `ObjectCommandsTest` — Stage 018A's own journey over the REFLOWED two-line strip | **OK (1 test)**, 11.4 s |
| `FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK` — unaffected by the `derivedBodyName` refactor | 97 checks, 0 failed |
| `FORGESHAPE_PROJECT_SELFTEST_OK` — the golden `.forge` digests | 256 checks, 0 failed |
| Startup, 64 MiB ring buffer confirmed by `logcat -g` | **twenty-two** `*_SELFTEST_OK` then `FORGESHAPE_NATIVE_VIEWPORT_OK`, zero `_SELFTEST_FAIL` and zero `_FAIL:` |
| `:app:assembleDebug` | successful |
| `:app:assembleRelease` | successful |
| Release `.so` self-test symbols (`llvm-readelf --dyn-syms \| grep -ci selftest`) | **0** |
| Release `.so` Mirror JNI entry points present | 2 |
| `verify-device-guards.ps1` | all PASS, 19 surfaces |

## What was NOT run, and why

- **`-FullSharded`.** Forbidden by this stage's instructions and by
  `TEST-OWNER-03`. No `FULL_SHARDED_SUITE_PASS` exists for `MIRROR-01` and none
  is claimed.
- **All three planes on device**, and all six primitives on device. The domain
  suite covers both exhaustively; repeating them through the UI would re-prove
  arithmetic rather than the journey. They are in the OWNER LATER pack.
- **A screenshot matrix, palettes and handedness.** Visual and ergonomic
  validation is OWNER LATER by policy.
