# SEL-OUT-R1 — evidence index

The persistent selection outline (`UI-OWNER-10`, `UI-OWNER-11`).

Baseline `538623c26e0122d97303150ebbb52eb3ea0d32b3`, clean. All device work on
the isolated AVD `ForgeShape_Stage006` (`emulator-5580`, confirmed by
`emu avd name` before every run). `emulator-5554` was never contacted.

---

## What was built

A **true renderer-derived silhouette**, in two steps per frame and only while a
drawable body is selected:

1. **the mask pass** — its own render pass, recorded before the frame's, over an
   `R8_UNORM` image and its own depth attachment at the render extent. Every
   scene body is drawn through a position-only pipeline with the *same* cull
   mode, winding and `proj·view·model` the surface pipeline uses; the fragment
   stage writes 1 for the selected body and 0 for every other. Occlusion is
   resolved by the depth test, so what survives is exactly the **visible** part
   of the selected body;
2. **the composite** — a full-screen triangle from `gl_VertexIndex`, recorded
   inside the main pass after the grid and before the gizmo, depth off, one
   blended draw. It discards inside the mask and otherwise paints where any of
   12 ring taps within the band radius is inside.

The persistent whole-object glow is gone: `kSelectionRestingAlpha` is **0**, so
the 220 ms acknowledgement pulse is the only thing that ever tints a surface.

---

## Files

| file | what it is |
| --- | --- |
| `PERFORMANCE.md` | the measured selection-loop numbers and the resource bound |
| `VISUAL_EVIDENCE.md` | the twelve captures with the facts measured at each |
| `OWNER_CONTACT_SHEET_SEL_OUT_R1.png` | the twelve frames, 260 px wide, four per row |
| `frames/` | the full-resolution captures and `facts.txt` |
| `TEST_RESULTS.md` | every gate, with what it reported |
| `MATRIX.md` | `SELOUTR1-01..36` and `E2E-SELOUTR1-01..14`, each against what actually covers it |
| `PREEXISTING_SHARD5_FAILURE.md` | the nine-step bisection proving shard 5's failure pre-dates this stage |
| `FULL_SHARDED_RUN1.txt` | the authoritative aggregate transcript |

---

## Gates

| gate | result |
| --- | --- |
| Native self-tests, debug launch | **twenty** `*_SELFTEST_OK` tokens then `FORGESHAPE_NATIVE_VIEWPORT_OK`; zero failures. `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` at **412 checks**, up from 345 |
| Native standalone runner (x86_64, NDK clang) | 412 checks, 0 failed, **71** of them `seloutr1_*` |
| `SelectionOutlineTest` (focused, device) | **OK (19 tests)** |
| `SelectionOutlineVisualEvidenceTest` (device) | **OK (1 test)**, twelve frames, every band assertion satisfied |
| JVM unit tests | BUILD SUCCESSFUL |
| `assembleDebug` (arm64-v8a + x86_64) | BUILD SUCCESSFUL |
| `assembleRelease` (arm64-v8a + x86_64) | BUILD SUCCESSFUL; **0** `run*SelfTests` symbols and **0** `seloutr1_` check-name strings in the release `.so` — F-16 still holds |
| `scripts\verify-device-guards.ps1` | all `DEV2-01..07` and `DEV3-01..06` PASS over 19 executable surfaces, including the new collector |
| `.forge` corpus, `-VerifyOnly` | all twenty-eight fixtures byte-identical; `git status` clean over the corpus. This stage touched no project-state code |
| `-FullSharded` aggregate | **shards 1–4 PASS (448/448)**, shard 5 `ASSERTION_FAILURE` on two `SpatialSketchTest` cases → `FULL_SHARDED_SUITE_FAIL`. **Proven pre-existing**: the same two failures reproduce at the clean baseline `538623c2` with this stage's work stashed — see `PREEXISTING_SHARD5_FAILURE.md` |

---

## The measurements that matter

**Depth correctness.** `frames/06_occluded_selected.png` — a slab stands between
the camera and the selected sphere. The band traces the visible arc, runs down
the edge where the slab covers it, and stops dead. Nothing crosses the slab.
This is not argued: the composite reads a mask that was already depth-resolved,
so an x-ray outline is not something it could draw.

**The band's thickness, measured out of the bitmap.** Median run of
outline-coloured pixels along a horizontal cut: **3 px** in every frame with the
outline on, against the renderer's reported 3.02 px half-width. **0 px** in
`02_construction_outline_off.png`.

**The palette actually reaches the renderer.** In every frame,
`measured_other_family_colour_pixels = 0`: the light grounds carry no
dark-family outline pixel and the dark grounds carry no light-family one.
Frames 08 and 09 are the same scene, and the band moves wholesale from
`255,184,66` to `148,51,0` with the ground.

**Contrast.** Measured with WCAG arithmetic in the render-shading self-test
against the authored grounds: **7.84** (Warm Graphite), **8.56** (Neutral
Charcoal), **6.14** (Light Charcoal), **6.25** (Warm Light), **6.25** (Cool
Light). All clear 4.5:1.

**Cost.** Over forty selection changes: **0** outline allocations, **0** mesh
republications, the fingerprint unmoved, and **0** composite draws recorded with
the toggle off. `renderer_mask_allocations` reads **2** in all twelve evidence
frames — unchanged across imports, a freeze, a real sculpt stroke, a CAD
extrude, five palette changes and a Delete.

---

## Handedness, measured

`frames/11_left_handed_view_overlay.png` reaches the left-handed workspace
through the real Settings page a user would use, and its facts are **read**
rather than captioned:

| fact | value |
| --- | --- |
| `handedness` (from `AppPreferencesStore`) | `LEFT` |
| `workspace_reports_left_handed` | `true` |
| `rail_zone_host_before_popover` | `[21,296][200,1473]` — the **left** edge |
| `display_popover` | `[461,275][1059,1253]` — the trailing top corner |
| `rail_zone_withdrawn_while_popover_open` | `true` |
| `popover_overlaps_the_rail_zone` | `false` |

Two separate things are shown. The rail zone really did move to the left edge
(x 21..200 of a 1080 px window), so a surface starting at x = 461 cannot reach
it. And while the Display popover is open the workspace **withdraws** the rail
zone entirely — `displayPopover.isOpen()` reaches the host's own visibility
decision — which is the product honouring "a surface may never partially cover
another live control", not a coincidence of geometry.

The measurement is taken **before** the popover opens for exactly that reason:
after it opens the host is absent, and an overlap fact computed then would be
vacuously true. The Display popover itself is not part of the handedness-mirrored
zone and never was — `UI-PREF-R1` mirrors the trailing host, a side-placed
precision surface, the brush controls and the Objects column, and this stage
changed none of it. The outline is handedness-independent by construction:
nothing in the mask pass, the composite or the width and colour policy reads a
preference.

## What is NOT claimed

* **No owner aesthetic approval.** Whether the band reads as the right weight
  under a real thumb on a real panel is the OWNER's, and nothing here decides
  it.
* **The Delete → Undo → Redo owner verdict of `IMPORT-01B` / `UI-OWNER-45` is
  still pending.** This stage's Delete cases are automated **non-collision**
  evidence — Delete, Undo and Redo behave exactly as before, one Delete is still
  one Undo, the existing selection fallback is unchanged, and the outline simply
  renders whatever selection is. They are not that verdict.
* **No pixel is asserted by `SelectionOutlineTest`.** It asserts that the
  renderer *ran* the outline path and that the resources are bounded; legibility
  is the native contrast measurement and the frames.
