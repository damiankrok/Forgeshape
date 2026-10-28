# CAD-R2-C2 — TESTS

## T0 — git

Branch `feature/cad-r2-c2-picking-seam-r1` from exactly `08406cf`; remote refs
re-fetched (main `6c9f156`, R2 `ea24d20`, C1 `08406cf`); `git diff --check`
clean before every commit; the only product file changed is
`app/src/main/cpp/forgeshape_picking.cpp` (`0fa12c9`). `app/src/main` is frozen
at `f3db953` (the fix plus the PICK_C2 self-tests); later commits are device
tests and docs.

## T1 — host native

**Red** (`repro/host_red_before_fix.txt`, product picking as on `08406cf`):
`HOST_SELFTESTS_FAIL (4391 checks, 7 failed)` — PICK_C2_01, 02, 03, 11
(PICKING) and 12, 13, 14 (CAD_FEATURE); PICK_C2_17 added later was likewise
run with the fix reverted: `CAD_FEATURE (638 checks, 4 failed)` (12, 13, 14,
17).

**Green** (`repro/host_green_after_fix.txt` and the final run):
`bash scripts/host-native-selftests.sh` → **`HOST_SELFTESTS_OK (4392 checks, 0
failed)`**, all 25 suites in order (C1 baseline 4375 + 17 PICK_C2; PICKING 174
→ 185, CAD_FEATURE 632 → 638, every other suite unchanged),
`CAD_R2_ORACLE_PASS=17`, every C1_CH_01..06, R2_* and earlier check kept.

| Check | Suite | Asserts | Red before fix |
| --- | --- | --- | --- |
| PICK_C2_01 | PICKING | far (250 m) orthographic ray on the shared diagonal hits at t ≈ 250, at the centre | **yes** |
| PICK_C2_02 | PICKING | same, triangle order swapped | **yes** |
| PICK_C2_03 | PICKING | 39 points along the diagonal, both orders | **yes** |
| PICK_C2_04 | PICKING | near (2 m) and perspective (3 m) seam rays hit | no (control) |
| PICK_C2_05 | PICKING | 2 mm either side of the seam lands in the triangle the plane names (x + y sign) | no |
| PICK_C2_06 | PICKING | 10 µm outside the outer edge misses (far and near); 0.1 mm inside hits | no |
| PICK_C2_07 | PICKING | reversed winding: culled; two-sided only on request | no |
| PICK_C2_08 | PICKING | degenerate, NaN vertex, zero direction, bad index fail closed | no |
| PICK_C2_09 | PICKING | rotated + translated body, far seam ray via `transformRayToLocal` | no |
| PICK_C2_10 | PICKING | nearer of two stacked copies wins, identically on 8 repetitions | no |
| PICK_C2_11 | PICKING | origins 50…500 m (step 25) × 9 diagonal points all hit | **yes** |
| PICK_C2_12 | CAD_FEATURE | `pickSceneSnapshot` hits the C1 pocket floor on its seam | **yes** |
| PICK_C2_13 | CAD_FEATURE | the chooser names the exact floor (feature 3, `CapFar`, lineage) twice, fingerprint unchanged — never the XY plane behind | **yes** |
| PICK_C2_14 | CAD_FEATURE | the same state regenerated into a new scene → the same semantic face | **yes** |
| PICK_C2_15 | CAD_FEATURE | a through hole: no hit of the body, no face chosen | no |
| PICK_C2_16 | CAD_FEATURE | the edge between the top cap and +x side: one of exactly those two, identical on 7 taps | no |
| PICK_C2_17 | CAD_FEATURE | the device D19 scene (block + pocket, orbit orthographic yaw 0.197, 1080 × 2400) resolves feature 2's floor on its seam | **yes** |

## T2 — JVM

`./gradlew :app:testDebugUnitTest` → **196 tests, 0 failures, 0 errors**. No
new JVM tests: no Java behaviour changed (the fix is native arithmetic; the
device classes are instrumentation).

## T3 — builds, release guard

`./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease`
→ BUILD SUCCESSFUL. `scripts/ci-release-selftest-guard.sh` →
`RELEASE_SELFTEST_GUARD=PASS`: release x86_64 3,256,192 B / arm64-v8a
2,916,104 B, 0 `SelfTests` symbols and 0 self-test strings; debug control 35 /
398 on both ABIs.

## T4 — DEVICE

Focused attempts (`CadRetainedSketchOwnerTest`):

| Run | Head | Result | Cause | Remediation |
| --- | --- | --- | --- | --- |
| 40771034694 | `f3db953` | 21/22 | D19, D20, D22 PASS (`d19.kind=3`, kept on feature 2 `CapFar`; outside → XY 0; hole → XY 0; hover → 3, nothing selected). D21: after the rotation a tap read kind −1 | instrumented D21 (`8e40a4d`) |
| 40775423761 | `8e40a4d` | D21 only: 0/1 | diagnostics: before the tap `kind=3 active=true owner=viewport_surface projection=1`, after `kind=-1 active=false` — the aim SURVIVED the rotation and the tap confirmed it (the chooser closed on the sketch). **Harness** expectation | D21 asserts the aim survives and one tap opens the face sketch (`b227a0b`); no product change |

Final union: see below.
