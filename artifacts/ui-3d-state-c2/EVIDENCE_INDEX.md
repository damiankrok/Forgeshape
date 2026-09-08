# UI-3D-STATE-C2 — Evidence index

Every artifact this correction produced, and what it shows. The audit's own
evidence under `artifacts/ui-3d-state-audit-r1/` and the C1 record under
`artifacts/ui-3d-state-c1/` are **unchanged**.

**Device for every runtime artifact:** `ForgeShape_Stage006` / `emulator-5580`,
1080 × 2400, density 2.625, identity confirmed with
`adb -s emulator-5580 emu avd name`. Two foreign targets (`emulator-5560`
`CarPassport_API_36`, `emulator-5570` `BuildPlan_API_36_16K`) were attached
throughout and every `adb`, Gradle and runner invocation was explicitly scoped to
`emulator-5580`. **`emulator-5554` was never contacted and was not attached.**

## The "before" half

| where | what it shows |
| --- | --- |
| `artifacts/ui-3d-state-audit-r1/screenshots/ui3d07_01_line_selected.png` | the audit's frame: a selected straight Line, `sketchLineDimension` true at 1.6 m, the `1.6 m` chip drawn, and **no extension lines, dimension line or ticks anywhere** |
| `artifacts/ui-3d-state-audit-r1/screenshots/ui3d05_05_after_zoom_out.png` | the Stage 020M leaders drawn normally with **no** axis active — the contrast that located the defect on the active range alone |
| `screenshots/ui3dc2_04_before_nothing_selected.png` | the same absence on the FINAL tree, as a number: 0 annotation pixels along the strip, in a frame carrying 192 583 ink pixels of grid, axes and stroke |
| `screenshots/ui3dc2_05_before_no_active_axis.png` | all three Stage 020M leaders standing in the neutral style, 0 annotation pixels in the viewport |

## The "after" half

| file | what it shows |
| --- | --- |
| `screenshots/ui3dc2_04_after_line_selected.png` | the direct answer to `ui3d07_01`: the two extension lines, the dimension line, its end ticks and the `1.6 m` chip standing on it. **270** annotation pixels along the measured strip |
| `screenshots/ui3dc2_05_after_x_axis_active.png` | the X leader (`2 m`) drawn in the annotation colour while the Y (`1 m`) and Z (`0.5 m`) leaders stay neutral. **86** sampled annotation pixels, from 0 |
| `screenshots/ui3dc2_05_product_path_x_editor_open.png` | the state reached by tapping the product's own X label — the compact editor open over the viewport — captured so the measured frame is not the only one on record |

## Ledgers

| file | what it is |
| --- | --- |
| `screenshots/FACTS_ui3dc204_theSelectedLineDimensionIsDrawn.txt` | the sketch measurement: the entity and its reported dimension, the projected dimension-line endpoints, the 78.75 px stand-off from the stroke, and the two amber counts |
| `screenshots/FACTS_ui3dc205_theActiveAxisDimensionLeaderIsDrawn.txt` | the leader measurement: both amber counts, both ink counts (183 979 → 183 979, identical) and the projected X anchor |

## Logs

| file | what it is |
| --- | --- |
| `logs/startup-selftests.log` | the debug launch on the final tree: **22** `*_SELFTEST_OK` tokens (3596 checks, from 3582) then `FORGESHAPE_NATIVE_VIEWPORT_OK`, zero failures. `FORGESHAPE_GIZMO_SELFTEST_OK (176 checks)`, from 167. Captured with the ring buffer at 64 MiB; the 3425 per-check `selftest pass:` lines are trimmed out, the tokens and every failure line are not |
| `logs/native-standalone-gizmo.log` | the same gizmo suite on the standalone NDK runner: `gizmo checks=176 failed=0` |
| `logs/native-standalone-guard-probe.log` | **the guard proved in both layers**: with a temporary sixth enum value the mapping emits `-Wswitch` at compile time AND two checks fail (`failed=2`); without it, `failed=0` |
| `logs/run-01-visibility.log` | the device suite, first run: `OK (2 tests)` in 25.8 s |
| `logs/run-02-visibility.log` | the same suite re-run after the ledger was named per case: `OK (2 tests)` in 23.6 s. The two cases passed on both runs; the re-run exists because one shared `FACTS.txt` had let the second case overwrite the first |
| `logs/run-03-c1-regression.log` | the `UI-3D-STATE-C1` regression guard, `Ui3dStateCorrectionTest`: **`OK (12 tests)`** in 57.0 s |
| `logs/builds-and-guards.md` | `assembleDebug`, `assembleRelease`, the release symbol check on both ABIs, and `verify-device-guards.ps1` |

## How to read the two measurements

Both are **differential**, and in both the frames being compared differ in
exactly one thing.

- **The sketch annotation.** The scanned strip is the dimension line itself,
  placed from `sketchLineDimension`'s anchor plus the selected entity's own
  endpoints and projected with `sketchScreenPoint` — no annotation constant is
  mirrored into the test. It stands 78.75 px clear of the stroke, which matters
  because a SELECTED stroke is drawn in the same colour; a strip that touched it
  would prove nothing. The numeric chip is excluded by its own view bounds.
- **The Stage 020M leaders.** The two frames are geometrically identical: all
  three leaders stand in both, and both carry the same 183 979 ink pixels. The
  only difference is that the X range moved from `Entities` to `Dimension`. The
  measured frame closes the compact editor and asks native alone for the active
  axis, so no editor or soft keyboard stands over one frame and not the other;
  the product path that reaches the same state is asserted separately and
  captured in its own frame.

In both, the selection outline — the one other amber the viewport draws — is
switched off for the whole measurement and restored afterwards, which is a
session-only display setting and costs the project nothing.

## Product diff

Four product files, one of them a build-list line:

```
app/src/main/cpp/forgeshape_sketch_overlay.h    +34
app/src/main/cpp/forgeshape_sketch_overlay.cpp  new, 67 lines
app/src/main/cpp/forgeshape_renderer.cpp        +12 -29
app/src/main/cpp/CMakeLists.txt                 +1
```

Two test files:

```
app/src/main/cpp/forgeshape_gizmo_selftest.cpp                  +119 (9 checks)
app/src/androidTest/java/.../Ui3dDimensionVisibilityTest.java   new
```

No shader, no producer, no Java product file, no resource, no `.forge` byte, no
`testdata/`, no codec, no `DATA_PACKAGE_SPEC.md` and no corpus script.
