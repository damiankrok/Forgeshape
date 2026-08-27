# UI-R5A — structural shell, precision surface, contrast and gizmo legibility

Runtime evidence for the one bounded correction stage taken from the four-way
audit (`artifacts/uiaudit2a`, `2b`, `2c`, `2d`). Every measurement here was taken
from the running product on a ForgeShape-owned emulator, and every control was
located by its **stable semantic id** at the centre of its own runtime bounds —
no screen coordinate appears in the capture harness.

## Provenance

| fact | value |
| --- | --- |
| device | `ForgeShape_Stage006`, confirmed by `adb -s emulator-5580 emu avd name` — never by port |
| serial | `emulator-5580`, started only by `scripts\start-forgeshape-emulator.ps1` |
| build | `gradlew.bat :app:assembleDebug` (arm64-v8a + x86_64), installed with `adb -s emulator-5580 install -r` |
| launch health | **13/13** `*_SELFTEST_OK` then `FORGESHAPE_NATIVE_VIEWPORT_OK` — [`R00`](R00-selftest-transcript.txt) |
| compact portrait | 1080 × 2400 @ 420 dpi = 2.625 px/dp |
| short landscape | 2400 × 1080 @ 420 dpi |
| expanded | 1600 × 2560 @ 240 dpi, taken after a force-stop so the override reached the process |
| large text | `settings put system font_scale 1.3`, restored to `1.0` |
| appearance | confirmed from the renderer's own `FORGESHAPE_VIEWPORT_BACKGROUND` line, not from the popover's drawing — which is what was under review |
| device restored | `wm size reset`, `wm density reset`, `font_scale 1.0`, `accelerometer_rotation 1` |

## Screenshots

| id | artifact | what it shows |
| --- | --- | --- |
| S01 | `S01-portrait-construction-resting.png` | Construction at rest: the trailing cluster anchored at the top |
| S02 | `S02-portrait-transform-move.png` | Move: closed arrowheads, crossed plane squares, a neutral pivot mark |
| S03 | `S03-portrait-transform-rotate.png` | Rotate: three rings **and** the pivot mark Rotate never had |
| S04 | `S04-portrait-transform-scale.png` | Scale: the doubled uniform cube as the largest mark; space capsule correctly absent |
| S05 | `S05-portrait-exact-transform.png` | Exact Transform: Apply pinned below the body, unit chips under Position |
| S06 | `S06-portrait-long-negative-ime.png` | `-98765.4321098` **whole, sign first**, with the keyboard up; selectors turned on their side; Apply on screen |
| S07 | `S07-portrait-long-negative-applied.png` | the same value not being edited: `-98765.432…`, shortened at the END |
| S08a | `S08a-portrait-add-primitive-open.png` | Add Primitive open |
| S08b | `S08b-portrait-after-system-back.png` | after one System Back: the palette is gone and **ForgeShape is still on screen** |
| S09 | `S09-landscape-exact-transform.png` | short landscape: docked panel, cluster intact |
| S10 | `S10-fontscale13-transform.png` | `font_scale 1.3`, Transform held |
| S11 | `S11-fontscale13-exact-transform.png` | `font_scale 1.3` with the panel open — the configuration that measured 17.1 dp |
| S12 | `S12-expanded-exact-transform.png` | expanded 1067 × 1707 dp: docked column, Apply visible |
| S13 | `S13-warm-graphite-refusal.png` | a real refusal in the corrected error role, Warm Graphite |
| S14 | `S14-neutral-charcoal-refusal.png` | the same refusal, Neutral Charcoal |
| S15 | `S15-light-charcoal-refusal.png` | the same refusal, Light Charcoal |

## Measurement records

| id | artifact | contents |
| --- | --- | --- |
| R00 | `R00-selftest-transcript.txt` | 13 `*_SELFTEST_OK` + `NATIVE_VIEWPORT_OK` |
| R01 | `R01-trailing-cluster-bounds.txt` | every trailing-cluster bound, px and dp, across seven compact-portrait states |
| R02 | `R02-constrained-configurations.txt` | short landscape, `font_scale 1.3` and expanded |
| R03 | `R03-text-roles-per-appearance.txt` | which appearance each refusal shot was taken in, confirmed from the renderer |

## Before / after, measured

Before-values are UI-AUDIT2A's, measured on the same AVD at the same density.

### The trailing cluster

| state | control | before | after |
| --- | --- | --- | --- |
| compact portrait, panel open | `precision_toggle` | 48.0 × **37.0** dp | 48.0 × **48.0** dp |
| short landscape, Sculpt | `precision_toggle` | 48.0 × **29.0** dp | 48.0 × **48.0** dp |
| `font_scale 1.3` | `precision_toggle` | 48.0 × **17.1** dp | 48.0 × **48.0** dp |
| IME open | `transform_mode_group` | 56.4 × **2.3** dp | **160.8 × 56.4** dp |
| IME open | `transform_space_group` | **absent from the tree** | **108.6 × 56.4** dp |
| IME open | `precision_toggle` | **absent from the tree** | 48.0 × 48.0 dp |

### The anchor

Tool Rail top, compact portrait, in px:

| transition | before | after |
| --- | --- | --- |
| Construction resting | 955 | **296** |
| → Transform selected | 591 (−139 dp) | **296 (no movement)** |
| Move → Rotate | 591 | **296** |
| Rotate → Scale | 739 (+56 dp) | **296** |
| precision surface opens | 275 (−120 dp) | **296** |
| precision surface closes | 955 | **296** |

The precision toggle is equally still at 691 px through every transform-mode
change. The two identical taps that used to end in Rotate rather than Scale now
land on the same control twice, because the control has not moved.

### Exact Transform

| | before | after |
| --- | --- | --- |
| `-98765.4321098` while editing | rendered `765.4321098` — sign and leading digits silently gone | rendered **whole**, one type step smaller |
| the same value at rest | — | `-98765.432…`, shortened at the END, sign first |
| tapping a populated field | caret placed; typing produced `0-98765.4321098` | the value is selected; the first keystroke replaces it |
| Apply, compact portrait | **three swipes** below an unmarked fold | pinned below the body, on screen at rest and with the keyboard up |
| unit chips | under the Scale row, headed *Display unit* | under the Position fields, headed *Position unit* |
| body continues | no peek, fade or indicator | a fading bottom edge while there is more |

### Semantic text contrast

Measured against every ground either role is drawn on — the viewport ground, the
chrome surface, the floating material, the precision surface, a control fill and
a field well. The worst of the six is quoted.

| role | before (worst) | after (worst) |
| --- | --- | --- |
| `p1_text_secondary` | 3.58:1 | **4.61:1** |
| `p2_text_secondary` | 4.13:1 | **4.60:1** |
| `p3_text_secondary` | 3.13:1 | **4.62:1** |
| `p1_text_error` | 2.87:1 | **4.63:1** |
| `p2_text_error` | 3.27:1 | **4.99:1** |
| `p3_text_error` | 2.17:1 | **4.68:1** |

`UIR5A-11` measures all 36 combinations on the device; `EditorWorkspaceThemeTest`
now holds the caption role to the same 4.5:1 target as body text.

### The gizmo

Drawing only. No hit radius, handle set, grab point, solver or space rule moved,
and `UIR5A-14` asserts that by grabbing every handle in every mode.

| mark | before | after |
| --- | --- | --- |
| pivot | three arms in the three axis hues, part of the shafts, ~9.6 units | one **neutral** cross, drawn once, 15.4 units, inside the dead disc |
| pivot in Rotate | none at all | the same neutral cross |
| plane handle | a doubled hollow square in an axis hue | the same square **with its two diagonals** |
| arrowhead | four spokes, 14.4 units across | six lines — four spokes **closed** across their ends, 19.2 units across |
| uniform scale cube | 18 units, single outline, on the same point as the pivot mark | **24 units, doubled outline**, and the only mark on that point |
| vertices uploaded | 1056 | 1116 (one upload, at start, unchanged in kind) |

## What is NOT in this evidence

- **A real soft keyboard in `UIR5A-04`.** The instrumented case raises the IME and
  uses it when it appears; on this emulator it does not appear to the
  instrumentation, so the case dispatches an IME inset of the same size through
  the same path the platform uses and asserts the chrome actually took it before
  measuring. The real keyboard is covered here instead, by `S06` and `R01`.
- **Frame pacing or gesture feel.** This is an emulator. It gives layout, bounds,
  insets, text scaling and resolved colours; it does not give a frame-rate
  verdict, and none is claimed.
