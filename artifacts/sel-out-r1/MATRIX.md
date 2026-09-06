# `SELOUTR1-01..36` and `E2E-SELOUTR1-01..14` — what covers each

Three kinds of coverage appear below, and the distinction is deliberate.
**Native** is the debug-only render-shading suite (`seloutr1_*` checks), which
proves the *policy and the rule* deterministically on the CPU. **Device** is
`SelectionOutlineTest`, which proves the *renderer actually ran the path* and
that nothing it does reaches project truth. **Frame** is a measured capture from
`SelectionOutlineVisualEvidenceTest`, whose band thickness, colour and bounding
box are scanned out of the bitmap rather than eyeballed.

No item is marked covered by prose alone.

## `SELOUTR1-01..36`

| # | requirement | covered by |
| --- | --- | --- |
| 01 | no selected object → no outline | Native `an_empty_mask_paints_nothing`; Device `seloutr1_01` (the preview has no selected object) and `seloutr1_35` (Home) |
| 02 | selected Construction outlined | Device `seloutr1_02`; Frame 01 (1144 band px) |
| 03 | selected Imported Mesh outlined | Device `seloutr1_03`; Frame 03 |
| 04 | selected current Sculpt outlined | Device `seloutr1_04_05` (across a real stroke); Frame 04 |
| 05 | retained Sculpt geometry outlined | Device `seloutr1_04_05` (back in Construction over the retained mesh) |
| 06 | selected CAD outlined | Device `seloutr1_06`; Frame 05 |
| 07 | transform matches the base render transform | Device `seloutr1_07` (translated, rotated, non-uniformly scaled, both projections). Structural: the mask pass composes the SAME two multiplies from the SAME snapshot — there is no second interpretation to disagree |
| 08 | selection switch removes old / adds new | Device `seloutr1_08` (16 switches); Native `a_selection_change_actually_moves_the_flag`; Frames 07/08 |
| 09 | no mesh tessellation on a selection switch | Device `seloutr1_09_10_17_18_19` and `seloutr1_09_10_27`: mesh revision 25 → 25 over 40 changes, fingerprint unmoved; Native `a_selection_change_republishes_no_mesh` (the same `RuntimeMeshPtr` survives) |
| 10 | no body mesh upload on a selection switch | Same. `syncBody` uploads only when the source revision or the surface shading changed, so an unmoved revision is an unmoved buffer |
| 11 | pulse remains short | Native `the_pulse_is_still_a_real_peak`, `the_pulse_is_still_short` (0.1 s < 0.22 s < 0.4 s) |
| 12 | outline remains after the pulse | Native `a_long_selected_body_carries_no_tint_at_all` (240 frames); Frame 01 shows the band with an untinted body |
| 13 | no full-object persistent glow | Native `no_persistent_whole_object_glow_remains` (`kSelectionRestingAlpha == 0`) and `settled_selection_tints_exactly_as_much_as_none`; Frame 01 |
| 14 | View/Overlay toggle present | Device `seloutr1_14` (ids, enabled, 44 dp floor measured, content descriptions, popover still compact, preferences did not move back in); Frame 11 |
| 15 | toggle OFF removes the outline | Device `seloutr1_15_16` twice — the chips, and the whole path stopping; Frame 02 measures **0** outline pixels |
| 16 | toggle ON restores it | Same |
| 17 | toggle does not dirty the project | Device `seloutr1_09_10_17_18_19` (dirty flag, fingerprint, native snapshot) |
| 18 | toggle creates no history | Same (both Construction depths and the Sculpt depth) |
| 19 | `.forge` bytes unaffected | Same — `encodeProject()` byte-identical after 4 toggles and 8 selection changes |
| 20 | visible contour obeys depth | Native `the_visible_half_is_still_outlined` + `the_cut_edge_is_outlined`; Frame 06 |
| 21 | no x-ray through a foreground object | Native `the_hidden_half_grows_no_x_ray_outline`; Frame 06. Structural: the composite reads an already depth-resolved mask |
| 22 | offscreen / clipped selected object safe | Native `a_body_flush_to_the_edge_is_outlined_on_its_open_side`, `nothing_is_painted_at_the_window_edge_itself`, plus null / zero-extent / zero-radius / out-of-range. The GPU sampler's opaque-black border is what implements it |
| 23 | Delete removes stale outline resources | Device `seloutr1_23_24_25_36`; the allocation count does not move and the outline draws the fallback body |
| 24 | selection fallback outline follows the existing fallback | Same — selection falls to the surviving body by the rule Delete already had |
| 25 | Undo / restoration safe | Same — Undo restores, Redo removes, the outline follows each |
| 26 | device / surface rebuild restores the outline | Device `seloutr1_26` through the debug injection seam; the allocation count rises, which is the resources being rebuilt rather than reused |
| 27 | repeated selection switch, bounded resources | Device `seloutr1_27` (10 selection + toggle + palette cycles) and `seloutr1_09_10_27` (40 changes): **0** allocations |
| 28 | Warm Graphite legible | Native measured **7.84:1**; Frame 01 |
| 29 | Neutral Charcoal legible | Native measured **8.56:1** |
| 30 | Light Charcoal legible | Native measured **6.14:1** |
| 31 | Warm Light legible | Native measured **6.25:1**; Frame 09 |
| 32 | Cool Light legible | Native measured **6.25:1**; Frame 10 |
| 33 | right-handed overlay menu safe | Device `seloutr1_14`/`seloutr1_15_16` run right-handed (the default); Frames 01–10, 12 |
| 34 | left-handed overlay menu safe | Frame 11, measured: rail zone at `x[21..200]`, popover at `x[461..1059]`, and the rail withdrawn while the popover is open |
| 35 | Home / bootstrap, no stale outline | Device `seloutr1_35` — the whole path stops when the project closes |
| 36 | no Delete / history semantic change | Device `seloutr1_23_24_25_36`; and no file behind Delete, the history, the codec, CAD or Sculpt appears in this stage's diff |

## `E2E-SELOUTR1-01..14`

| # | journey | driven by |
| --- | --- | --- |
| 01 | open a project with at least two objects | `e2eSeloutr1_02_03`, `e2eSeloutr1_04`, Frames 07/08 |
| 02 | tap object A → pulse, then persistent outline | `e2eSeloutr1_02_03`: a real `MotionEvent` DOWN/UP on the viewport, resolved by CPU picking. The pulse itself is the native suite's |
| 03 | select object B → the outline moves to B only | Same case: it taps B, asserts the selection and the outline moved, then taps back to A |
| 04 | Objects-list selection also moves the outline | `e2eSeloutr1_04`: `performClick()` on the body's own row |
| 05 | View/Overlay → Selection Outline OFF | `seloutr1_15_16` through the real chips; Frame 02 |
| 06 | OFF persists on the same lifecycle as Grid | `seloutr1_14_theOutlineChoiceOutlivesTheActivityExactlyAsTheGridDoes` — `recreate()`, both survive, and the reopened popover repaints from native truth |
| 07 | turn ON again | `seloutr1_15_16` |
| 08 | change among all five palettes while selected | `seloutr1_27` cycles the ground; Frames 09/10; contrast measured natively for all five |
| 09 | switch handedness Left → View/Overlay still usable | Frame 11, through the real Settings page |
| 10 | select an Imported Mesh and verify | `seloutr1_03`; Frame 03 |
| 11 | select a Sculpt body and verify | `seloutr1_04_05`; Frame 04 |
| 12 | select a CAD body and verify | `seloutr1_06`; Frame 05 |
| 13 | delete a deletable selected object → no stale outline, fallback follows | `seloutr1_23_24_25_36`; Frame 12 |
| 14 | Undo / Redo regression, automation only | `seloutr1_23_24_25_36`. **Explicitly NOT the OWNER Delete acceptance** |

## Where the coverage is thinner than the rest

* **SELOUTR1-22** is proven natively and by the sampler's border rule, not by a
  device frame of a body hanging off the screen edge. An earlier evidence run
  did capture one (a body at x = +2.6 gave a band at `x[1010..1079]`, flush to
  the window edge and correct); the final scene places both bodies fully on
  screen so the A/B frames show two complete silhouettes, which is the more
  useful picture.
* **SELOUTR1-28..32** are *measured contrast*, not an aesthetic judgement. That
  the band clears 6.14:1 on every ground says it is legible; whether it is the
  right weight is the OWNER's call and is not claimed here.
