# UI-3D-STATE-C2 — Summary

**Result:** `PASS-UI-3D-STATE-C2-OWNER-LATER`

The last open finding of `UI-3D-STATE-AUDIT-R1` is closed. `UI3D-F-005` —
`SketchOverlayStyle::Dimension` rendering fully transparent — is fixed in the
renderer, and only there. No feature, no format, no contract and no domain
semantic moved.

| | |
| --- | --- |
| **Start HEAD** | `1d0c14a391642dab8bc55b0157fb21780d461d24`, clean worktree, no remote |
| **Device** | `ForgeShape_Stage006` / `emulator-5580`, 1080 × 2400, density 2.625. Identity confirmed by `adb -s emulator-5580 emu avd name`. `emulator-5554` was never contacted and **was not attached** |
| **Product change** | 4 files: one new `.cpp`, one header, the renderer call site, one `CMakeLists.txt` line |
| **Test change** | 2 files: nine checks added to the gizmo self-test, one new device suite |
| **Attempts** | one complete gate; the device suite was re-run once for evidence completeness (its two cases passed on both runs) |
| **Automated runtime** | ≈ 9 minutes against a 20-minute target and a 30-minute hard stop. No `-FullSharded` |

## The defect, and why it could hide

`Renderer::recordSketchOverlayDraw` switched on `SketchOverlayStyle` with cases
for `GridMinor`, `GridMajor`, `Axes` and `Entities`, **no case for `Dimension`
and no `default:`**. `GizmoPush push{}` is zero-initialised inside the loop and
`gizmoHighlightColor` writes rgb only, so `push.highlight[3]` — the base alpha
`gizmo.vert` multiplies every vertex by — stayed `0.0f`, and the gizmo pipeline
blends with `SRC_ALPHA`.

The failure mode is the reason it survived two shipped features: a missing case
is not a crash, not a warning at any level the build enables, and not a failing
test. It is a range that is *drawn* and cannot be *seen*. Both producers were
correct and remain untouched — `forgeshape_sketch_session.cpp` styles the whole
line annotation `Dimension`, and `forgeshape_body_dimension_overlay.cpp` styles
the **active-axis** range `Dimension` while the other two stay `Entities`.

## The fix, and the guard

**One case, plus a place to put it that can be tested.**
`sketchOverlayStyleWeights` (`forgeshape_sketch_overlay.{h,cpp}`) is the mapping
from a style to the two scalars that differ between styles — the grey a hue-less
vertex takes (`axisX.w`) and the alpha the whole range draws at
(`highlight.w`). It is a pure function over values: no renderer, no device, no
frame, no Vulkan type. Everything a range does **not** vary — the
view-projection, the three axis hues, the highlight colour, the emphasis tag —
stayed in the renderer, which now reads two numbers and refuses a range it has
no mapping for rather than recording a draw nothing could see.

`Dimension` takes the entity LEVEL and `kGizmoAxisAlpha × 0.85` — just under the
entity weight, because an annotation is read against the geometry it measures
and must not out-read it. Its vertices all carry the emphasis tag, so the colour
it is actually drawn in is the gizmo's held-handle amber: **no new hue enters
the viewport for it.**

**The guard is stated twice.** The mapping's switch has no `default:`, so a
value added to the enum is a `-Wswitch` diagnostic; and the gizmo self-test
walks **every** value of the enum across **all five** grounds and fails if any
maps to an invisible alpha. Both halves were proved rather than asserted: adding
a temporary sixth value produced
`warning: enumeration value 'ProbeStyleTemporary' not handled in switch
[-Wswitch]` **and** two failing checks, and the value was removed again.

The four pre-existing styles keep their weights **to the value, on every
ground** — asserted by exact float equality, so this is one case added and not a
re-weighting.

## Measured, not looked at

The audit adjudicated the defect in a frame. This correction answers in numbers,
and both measurements are **differential**, so what moves is attributable to the
style and to nothing else. The selection outline — the one other amber the
viewport draws — is switched off for every frame and restored afterwards.

| measurement | before | after |
| --- | --- | --- |
| the annotation's colour along the dimension line of a selected 1.6 m Line, in a strip standing 78.75 px clear of the stroke | **0** (nothing selected) | **270** (the Line selected) |
| the same colour over the whole viewport with the Stage 020M leaders standing | **0** (no active axis) | **86** sampled pixels (X active) |

The Dimensions frames are **geometrically identical** — all three leaders stand
in both, and the ink count is the same 183 979 in each — so the only difference
between them is that one range moved from `Entities` to `Dimension`.

`ui3dc2_04_after_line_selected.png` is the direct answer to the audit's
`ui3d07_01_line_selected.png`: the extension lines, the dimension line, its two
end ticks and the `1.6 m` chip on it, in one frame.

## `UI3DC2-01..10`

| case | result |
| --- | --- |
| UI3DC2-01 baseline / scope | **PASS** — exact HEAD, clean start, renderer-only product diff, `emulator-5554` untouched |
| UI3DC2-02 source / root-cause closure | **PASS** — both producers still emit `Dimension`; the renderer has an explicit visible mapping; no alpha-zero fallback survives |
| UI3DC2-03 exhaustive style mapping test | **PASS** — nine checks over every enum value × five grounds, proved to fail on an omitted value |
| UI3DC2-04 sketch dimension on device | **PASS** — 0 → 270 amber, before/after frames |
| UI3DC2-05 Stage 020M leader on device | **PASS** — 0 → 86 amber over identical frames, label attachment intact |
| UI3DC2-06 other overlay regression | **PASS** — the four older styles hold their exact weights on every ground; the sketch grid, axes and entities measure 192 583 ink in the very frame the annotation is absent from |
| UI3DC2-07 C1 regression guard | **PASS** — `Ui3dStateCorrectionTest` **OK (12 tests)** in 57.0 s |
| UI3DC2-08 build / release guard | **PASS** — `assembleDebug`, `assembleRelease`, 22 `*_SELFTEST_OK` tokens (3596 checks), 0 self-test symbols in either release `.so`, device guards green over 19 surfaces |
| UI3DC2-09 persistence / data neutrality | **PASS** — no byte of `DATA_PACKAGE_SPEC.md`, `testdata/`, the codec or the fingerprint moved |
| UI3DC2-10 final repository state | **PASS** — clean tree, `git diff --check` clean, no remote, no next stage started |

## What did NOT change

No `.forge` byte, section, version, fixture or corpus digest. No codec, schema,
history, fingerprint, `TopoRef`, CAD extent or Construction dimension. No
shader — `gizmo.vert` already multiplied by the alpha the renderer sends, which
is why the repair is a value and not a pipeline. No producer, no anchor
semantics, no `UI-3D-STATE-C1` conversion, refresh path or clamp. No Android
imitation of a dimension line was built: the pixels counted are the ones the
Vulkan renderer drew.

## OWNER LATER

`OWNER_LATER.md` carries the six questions this correction deliberately does not
answer — the exact colour and alpha across all five palettes, line weight on a
phone against a tablet, whether the annotation competes with the selection
outline or the gizmo, readability on the light and dark sides of a model under
both shading models, sketch/body consistency, and portrait against landscape and
handedness. **Nothing here is marked OWNER accepted.**
