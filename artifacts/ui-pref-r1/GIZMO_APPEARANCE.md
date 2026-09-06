# Gizmo appearance — UI-PREF-R1 Parts E, F, G (UI-OWNER-32)

## Audit of the accepted gizmo (Part E1)

Before this stage the gizmo had ONE scale, `GizmoSnapshot.worldPerReferenceUnit`
(camera-derived, `gizmoWorldScale`), used by the renderer's model matrix, by
`gizmoHandleGrabPoint` and by the hit test for handle POSITIONS; the hit
CORRIDORS (`kGizmoHitSlopUnits`, `kGizmoPlaneHitRadiusUnits`,
`kGizmoUniformHitRadiusUnits`, `kGizmoPivotDeadRadiusUnits`, all 24 units)
were already measured in density pixels (`gizmoPixelsPerReferenceUnit`), and
the stroke geometry was authored once in canonical reference units and
uploaded once. Visual and hit geometry were therefore coupled only through the
placement scale; the corridors were already independent.

## Visual size (Part E) — exact bounds

| | value |
| --- | --- |
| domain constants | `kGizmoMinVisualScale = 0.9`, `kGizmoDefaultVisualScale = 1.0`, `kGizmoMaxVisualScale = 1.5` (`forgeshape_gizmo.h`) |
| presets offered | 0.9 (Smaller — 90%), 1.0 (Default — 100%), 1.25 (Larger — 125%), 1.5 (Largest — 150%) |
| below JNI | `GizmoSession::setVisualScale` REFUSES non-finite and out-of-range (returns false, value stands); `NativeViewport.setGizmoVisualScale` returns the answer |
| above JNI | `AppPreferences.clampGizmoVisualScale`: non-finite → 1.0, finite out-of-range → clamped, so what the store holds is always accepted below |

Decoupling: `GizmoSnapshot` gains `visualScale`, and
`gizmoPlacementScale(state) = worldPerReferenceUnit × visualScale` is the ONE
scale every handle is PLACED at — the renderer's model matrix, `gizmoHandleGrabPoint`
(planes, shaft middles, ring grab points), and the hit test's ring radius and
shaft span all read it, so what is seen and what can be grabbed still cannot
differ. The corridors do not scale (density pixels), so the 48-unit floor holds
at 0.9. The scale-drag reference (`scaleReferencePixels_`) is measured on a
CANONICAL copy of the snapshot with `visualScale = 1`, so the same pixel drag
stretches by the same factor at every size; the move solvers are ray
arithmetic and the ring solver an angle about the pivot, neither reading a
size.

Why the floor is 0.9 and not 0.75: a plane handle is grabbed within 24 units of
its centre and the pivot's dead disc is 24 units (both fixed). From the
self-test's oblique three-quarter camera the XZ plane centre's projected
distance from the pivot is ≈ 0.68 × its standoff: 27.1 units at 1.0 (the
accepted gizmo's 3-unit margin), 24.4 at 0.9, 20.3 at 0.75 — where
`uipref_every_handle_is_pickable_at_both_visual_bounds` failed on the first
run because the plane handle fell inside the dead disc. The bound was moved,
not the corridor: changing the disc would change the accepted hit semantics.

Why the ceiling is 1.5: the Rotate rings are 78 × 1.5 = 117 units in radius,
234 units across, which still leaves a 360-dp window a margin on both sides;
`19_gizmo_largest_bold` and `20_gizmo_largest_bold_rotate` show it at that size
on the 411-dp emulator.

Proofs (`FORGESHAPE_GIZMO_SELFTEST_OK`, 167 checks, +22 this stage):
`uipref_visual_scale_defaults_to_one`, `…bounds_are_the_documented_ones`,
`…refuses_out_of_range_and_non_finite_not_clamps`, `…accepts_both_bounds`,
`…records_nothing`, `uipref_snapshot_carries_the_visual_scale_beside_the_camera_scale`,
`uipref_every_handle_is_pickable_at_both_visual_bounds` (every handle of every
mode hit at its own pixel at 0.9 and 1.5),
`uipref_handles_stand_further_out_at_a_larger_visual_size` (ratio 1.5/0.9 exact),
`uipref_the_hit_corridor_keeps_its_floor_at_the_smallest_size` (20 units off the
shaft still grabs it at 0.9),
`uipref_the_same_pixel_drag_scales_by_the_same_factor_at_every_visual_size`,
`uipref_a_uniform_drag_is_the_same_factor_at_every_visual_size`,
`uipref_the_same_two_pixels_move_the_body_the_same_distance_at_every_size`.
On the device, `SettingsPreferencesTest.uiprefr1_30_35_36` asks native for
every handle's pixel in every mode at 0.9, 1.5 and 1.0 and asserts
`gizmoHitTest` names that handle.

## Thickness (Part F) — exact presets

`GizmoStrokeWeight` (display store, index 0 Thin / 1 Regular / 2 Bold):

| recipe | spread | shaft bundle | arrow bundle | ring passes | square passes | end-cube outlines | uniform outlines | vertices |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Thin | 0.55 u | 5 | 1 | 2 | 2 | 1 | 2 | 1116 |
| Regular (default) | 1.1 u | 5 | 1 | 2 | 2 | 1 | 2 | 1116 |
| Bold | 1.1 u | 13 | 5 | 4 | 3 | 2 | 3 | 2268 |

Regular is the pre-preference gizmo BYTE FOR BYTE: the two-argument
`generateGizmoVertices` and the Regular recipe compare equal with `memcmp`
(`uipref_regular_is_the_pre_preference_gizmo_byte_for_byte`), the counts are
pinned by `static_assert`, and the standalone runner's FNV-1a 64 of the Regular
list is `84976216ea9f24b8` (1116 vertices) both at baseline `565d400` and after
this stage (`NATIVE_STANDALONE.md`). Every recipe is one-pixel lines — no
`wideLines` device feature. Hit testing never reads the weight
(`uiprefr1_33`: the three axis handles' pixels and hits are identical across
Bold and Thin). The renderer keeps one buffer sized to Bold and
`syncGizmoGeometry` re-uploads only when the store's weight differs from the
uploaded one; `FORGESHAPE_GIZMO_UPLOAD_OK weight=…` names it.

At min and max: Thin at 0.9 and Bold at 1.5 are the frames `18_gizmo_smallest_thin`
and `19_gizmo_largest_bold`; no z-fighting is possible (depth test and write
are off for the gizmo pipeline, unchanged) and no vertex is non-finite
(`uipref_every_recipe_is_finite`).

## Handle style (Part G) — deferred by renderer contract

`GIZMO_STYLE_DEFERRED_BY_RENDERER_CONTRACT`. The gizmo is drawn as a LINE LIST
through one pipeline that deliberately has no solid-geometry path ("giving them
solid geometry would put a second kind of surface in a renderer whose one
surface pipeline exists for Construction Bodies", `forgeshape_gizmo.h`). A
"Filled" variant would therefore be a substantial renderer rewrite (a second
gizmo pipeline, filled arrowheads/cubes with their own depth and blend terms),
and every in-contract alternative examined — dropping the plane diagonals,
single-outline cubes, a different arrowhead — is a cosmetic variation with no
coherent meaning across translation and scale handles, which the stage
forbids ("no ambiguous iconography"). No row is drawn; the preference model
carries no field for it; `AppPreferencesTest.thereAreExactlyTheApprovedPreferencesAndNoOthers`
and `SettingsPreferencesTest.uiprefr1_40` assert its absence.
