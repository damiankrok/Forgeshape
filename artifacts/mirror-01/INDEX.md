# MIRROR-01 — Construction Mirror across world XY / XZ / YZ

**Construction-only.** One new body, reflected across one principal world plane,
carried by a **proper rotation** and never by a negative scale.

Run under `TEST-OWNER-03` (reduced testing): no `-FullSharded`, no screenshot
matrix, no contact sheet.

## What this stage added

| Concern | Where it lives |
| --- | --- |
| The mirror arithmetic and the eligibility predicate, as a pure function over values | `app/src/main/cpp/forgeshape_body_mirror.{h,cpp}` |
| `mirrorSceneBody`, the transaction, and the shared `derivedBodyName` rule | `app/src/main/cpp/forgeshape_body_commands.{h,cpp}` |
| `MIRROR01-01..12` | `app/src/main/cpp/forgeshape_mirror_selftest.{h,cpp}` |
| JNI: `sceneBodyCanMirror`, `sceneMirrorBody`, the two status codes, the suite hook | `app/src/main/cpp/forgeshape_jni.cpp` |
| The row's Mirror control and the inline plane chooser | `app/src/main/java/com/forgeshape/app/ObjectsSectionView.java` |
| The bindings, the `OBJCMD_*` codes and the `MIRROR_PLANE_*` transport | `app/src/main/java/com/forgeshape/app/NativeViewport.java` |
| The one device journey | `app/src/androidTest/java/com/forgeshape/app/MirrorSmokeTest.java` |

## The mathematics, stated once

With `Qx = diag(-1, +1, +1)` (the primitive's own local-X symmetry) and `F` the
world reflection — `diag(+1,+1,-1)` for XY, `diag(+1,-1,+1)` for XZ,
`diag(-1,+1,+1)` for YZ:

```
p' = F · p          R' = F · R · Qx          S' = S
det(R') = det(F) · det(R) · det(Qx) = (-1)(+1)(-1) = +1
```

`R'` is therefore a PROPER rotation and no negative persisted scale is required.
`Qx` is algebra: it is never persisted, never reaches a `.forge` byte and never
appears in a history step.

## The proof, which is not the determinant

The determinant only says the result is a legal orientation. The geometric claim
is the identity

```
Model_mirror(q)  ==  F · Model_source(Qx · q)      for every local q
```

which holds because `Qx` and `S` are both diagonal and therefore commute:

```
T' + R'·S·q = F·T + F·R·Qx·S·q = F·(T + R·S·(Qx·q))
```

No symmetry is assumed for that. `MIRROR01-06` asserts it over probe points no
primitive generates. The SYMMETRY then adds that `Qx` maps each generated local
vertex set onto itself — all six primitives are origin-centred and their rings
carry `kPrimitiveRadialSegments` (32, divisible by four) — so
`WorldGeometry(mirror) == F · WorldGeometry(source)` as a set.
`MIRROR01-07/08` assert both halves over **six primitives × three planes × ten
poses**, the poses including a body on the plane, one crossing it, a non-uniform
scale, an extreme valid scale (1e-4 and 1e4) and an angle past a whole turn.

## Verification (2026-09-07)

| Evidence | Result |
| --- | --- |
| Startup self-tests, debug launch on `emulator-5580` (`ForgeShape_Stage006`), 64 MiB ring | **twenty-two** `*_SELFTEST_OK` then `FORGESHAPE_NATIVE_VIEWPORT_OK`, zero failures |
| `FORGESHAPE_MIRROR_SELFTEST_OK` | **62 checks**, `MIRROR01-01..12` |
| `FORGESHAPE_BODY_DIMENSIONS_SELFTEST_OK` | 97 checks, unchanged by the `derivedBodyName` refactor |
| `FORGESHAPE_PROJECT_SELFTEST_OK` | 256 checks, golden `.forge` digests unchanged |
| Standalone NDK runner (`x86_64-linux-android26-clang++`, run on `emulator-5580`) | `MIRROR OK (62 checks, 0 failed)` / `DIMENSIONS OK (97 checks, 0 failed)` |
| `MirrorSmokeTest` (`E2E-MIRROR01-01`), focused instrumented | **OK (1 test)**, 5.2 s |
| `ObjectCommandsTest` (Stage 018A regression, the reflowed strip) | **OK (1 test)**, 11.4 s |
| `:app:assembleDebug` / `:app:assembleRelease` | both successful |
| Release `.so` self-test symbols | `llvm-readelf --dyn-syms \| grep -ci selftest` = **0** |
| `verify-device-guards.ps1` | all PASS over 19 surfaces |

**No `-FullSharded`**, by `TEST-OWNER-03` policy. No `FULL_SHARDED_SUITE_PASS`
is claimed for `MIRROR-01`, and this focused evidence cannot stand in for the
exhaustive gate.

## Persistence

**No `.forge` field, section or version changed.** A mirrored body is an
ordinary Construction body wearing an ordinary transform, so nothing stores
which plane made it. `MIRROR01-12` proves it the only way worth proving: a
project reached by mirroring encodes **byte-identically** to one whose second
body was placed at the same numbers by hand, and it decodes and reloads through
the ordinary fail-closed path with a fresh, empty history.

## Refusals, by name

| Case | Status | Reported eligibility |
| --- | --- | --- |
| Imported Mesh | `RefusedNotMirrorable` | `NotConstruction` |
| CAD Body | `RefusedNotMirrorable` | `NotCad` |
| Construction Body with a Frozen Sculpt Mesh | `RefusedNotMirrorable` | `HasSculptTruth` |
| Non-finite source transform | `RefusedNotRepresentable` | — |
| Unknown body | `UnknownBody` | — |
| A Construction edit already open | `RefusedEditInProgress` | — |
| Sculpt mode or an open sketch | `OBJCMD_REFUSED_IN_SCULPT` (JNI) | — |

Every refusal is asked before an `ObjectId` is minted or an edit opened, so none
of them creates a body, records a step or moves the fingerprint. The row simply
has no Mirror control for the first three (`sceneBodyCanMirror`), and the domain
guard stays regardless.

## Deliberately not this stage

Imported Mesh, CAD and Sculpt mirror; sketch entity mirror; Sculpt stroke X
symmetry; an arbitrary plane, a selected face as a plane, or a custom workplane;
hierarchy subtree and multi-select mirror; a live symmetry modifier; a linked
instance; a boolean mirror; a negative Scale; Stage 020D Directional Scale; and
`SCULPT-DIM-01`.

See `OWNER_LATER_TEST_PACK.md` for what no emulator settles.
