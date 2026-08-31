# GLB-IMPORT-R1 — the focused test table

`GLBIR1-01..21`, with where each is proved. Native cases run once from
`NativeViewport.start()` in a debug build and appear in the startup capture;
instrumented cases run on the isolated AVD through
`scripts\run-instrumented-tests.ps1 -Serial emulator-5580`.

| ID | Requirement | Where | Result |
| --- | --- | --- | --- |
| `GLBIR1-01` | A finite glTF node matrix parses and bakes correctly | native: `GLBIR1_01_the_node_matrix_is_baked_into_the_positions` — vertex 0 of the fixture lands on exactly `(-1.2109375, 1.2265625, -1.6875)` | **PASS** |
| `GLBIR1-02` | TRS parses and composes correctly; a matrix + TRS conflict is rejected | native: `GLBIR1_02_trs_composes_as_translation_rotation_scale` (a 90° Y turn with a 2× local-X stretch sends `(2,0,0)` to `(1,2,-1)` and `(0,0,1)` to `(2,2,3)`), `GLBIR1_02_a_node_with_no_transform_is_the_identity`, `GLBIR1_02_a_node_stating_both_a_matrix_and_a_trs_is_refused` | **PASS** |
| `GLBIR1-03` | Column-major matrix semantics, verified with an asymmetric sentinel | native: `GLBIR1_03_the_matrix_translation_column_is_read_as_a_column`; device: `E2E-GLBIR1-03` compares the baked world AABB against an independent third reader | **PASS** |
| `GLBIR1-04` | A singular or non-finite transform fails closed | native: a zero node scale, a flattened node matrix and a non-affine node matrix each give `SingularNodeTransform`; a non-finite scale is refused as `MalformedJson` by the number grammar before it can reach geometry | **PASS** |
| `GLBIR1-05` | The negative-determinant path preserves visible winding by explicit correction | native: `GLBIR1_05_a_negative_determinant_corrects_the_winding` — every triangle's last two indices swap against the identity-transform import, `windingCorrected` is set, and the positions are mirrored as instructed. `GLBIR1_05_a_positive_determinant_needs_no_winding_correction` is the negative control | **PASS** |
| `GLBIR1-06` | Seven TRIANGLES primitives all survive | native: `GLBIR1_06_all_seven_primitives_survive`, `GLBIR1_06_every_triangle_survives`, `GLBIR1_06_the_batches_tile_the_index_array_exactly`; device: `E2E-GLBIR1-01` | **PASS** |
| `GLBIR1-07` | A shared POSITION accessor across primitives works | native: `GLBIR1_07_a_shared_POSITION_accessor_is_decoded_once` — 1978 vertices, not 7 × 1978 | **PASS** |
| `GLBIR1-08` | uint16 and uint32 index decoding, bounds checked | native: `GLBIR1_08_uint16_indices_decode`, `GLBIR1_08_an_index_past_the_last_vertex_is_refused`, `GLBIR1_08_an_index_count_that_is_not_triangles_is_refused`, `GLBIR1_08_a_non_indexed_primitive_is_refused`; uint32 throughout the fixture | **PASS** |
| `GLBIR1-09` | A missing NORMAL generates deterministic finite area-weighted vertex normals | native: `GLBIR1_09_the_fixture_carries_no_NORMAL_so_normals_were_generated`, `..._every_generated_normal_is_finite_and_unit_length`, `GLBIR1_09_generated_normals_are_deterministic` (two imports, identical arrays), `GLBIR1_09_a_flat_sheet_generates_one_flat_normal`; device: `E2E-GLBIR1-02` | **PASS** |
| `GLBIR1-10` | A supplied NORMAL uses the inverse transpose correctly | native: `GLBIR1_10_a_supplied_normal_rides_the_inverse_transpose` — under a 4× Y stretch a 45° normal leans towards **X**, which is the opposite of what multiplying by the transform would do — and `GLBIR1_10_and_is_normalized_after_it` | **PASS** |
| `GLBIR1-11` | A degenerate or un-normalizable referenced vertex fails cleanly | native: `GLBIR1_11_a_wholly_degenerate_triangle_cannot_generate_a_normal` and `GLBIR1_11_a_supplied_zero_normal_is_refused_by_the_same_name`, both `CannotGenerateNormals` — never a NaN and never an invented default | **PASS** |
| `GLBIR1-12` | COLOR_0/COLOR_1/TEXCOORD_0 are structurally validated and safely ignored | native: the fixture carries all three and imports; `GLBIR1_12_a_colour_accessor_that_does_not_exist_is_refused` (`AccessorOutOfRange`), `GLBIR1_12_a_colour_accessor_that_disagrees_on_count_is_refused` (`CountMismatch`), `GLBIR1_12_an_unknown_custom_attribute_is_refused` (`UnknownAttribute`). No decode and no allocation for any of them | **PASS** |
| `GLBIR1-13` | Material `doubleSided` controls preview culling | native: `GLBIR1_13_double_sidedness_is_read_per_primitive` and `GLBIR1_13_and_each_batch_culls_its_own_way` — one mesh holding both answers at once — plus `GLBIR1_13_a_double_sided_material_reaches_preview_culling` on the fixture; device: `E2E-GLBIR1-04` | **PASS** |
| `GLBIR1-14` | `extras.nomad` is ignored and never becomes project truth | native: `GLBIR1_14_extras_are_ignored_and_only_the_node_name_is_kept`. Structurally: `ImportedMesh` has no field that could hold it, and the only string taken from a node is its name | **PASS** |
| `GLBIR1-15` | Animation, skinning, morph, compression and external-resource cases remain refused | native: seven cases on the fixture — Draco, meshopt, animation, skinning, morph targets, an external buffer `uri`, an image — plus `GLBIR1_15_child_nodes_are_refused_by_name_not_flattened`; device: `E2E-GLBIR1-07` | **PASS** |
| `GLBIR1-16` | The Import GLB SAF is separate from the native `.forge` Open | instrumented: `glbir1_16_importGlbAndOpenProjectAreDistinctActions`, `glbir1_16_cancellingTheImportPickerChangesNothing` | **PASS** |
| `GLBIR1-17` | The preview stays absent from ObjectIds, history, `.forge`, autosave, recovery and re-export | instrumented: `glbir1_17_theWidenedPreviewStillOwnsNoProjectState`, `glbir1_17_aPreviewIsNeverReExportedAsProjectContent`, `glbir1_17_aRefusalLeavesAnExistingPreviewWhole`; and the whole of `GlbImportPreviewTest` `GLBIR0-08..12` still passes | **PASS** |
| `GLBIR1-18` | Source↔Imported↔Clear preserves project state and navigation stays usable | instrumented: `glbir1_17_theWidenedPreviewStillOwnsNoProjectState` (bit-identical project bytes across the whole cycle) and `e2eGlbir1_05_viewportNavigationStillWorksOverAPreview` | **PASS** |
| `GLBIR1-19` | The Nomad-like ~2k vertex / ~3.8k triangle fixture imports and renders with no crash and no missing primitive | native: six checks on the fixture including byte-determinism; instrumented: `glbir1_19_theNomadLikeFixtureImportsThroughTheRealResultSeam` | **PASS** |
| `GLBIR1-20` | UI semantic ids, ≥48 dp targets and the R2 host/resting geometry unchanged | instrumented: `glbir1_20_theImportControlsKeepTheirIdsAndTheTouchFloor`; `GLBIR0-18`/`-19` unchanged and still green, and no control was added or moved — only the wording of three existing rows changed | **PASS** |
| `GLBIR1-21` | No OBJ/FBX/material-texture/production-import scope introduced | instrumented: `glbir0_20_noInterchangeScopeBeyondTheDiagnosticIsOffered` (the project surface offers no `obj`, `fbx`, `stl`, `collada`, `dae`, `usdz`, `material`, `texture` or `animation`, and the GLB group is worded as a preview) and `fsr1b18_noInterchangeFormatIsOfferedAnywhereInTheProjectSurface` | **PASS** |

## R0 cases this stage deliberately changed

Three R0 self-test cases asserted refusals that `ARCH-OWNER-09` explicitly
authorizes, so they were rewritten rather than deleted, and each now asserts the
narrower thing that is still true:

| Was | Is now |
| --- | --- |
| `a_node_matrix_is_refused` | `GLBIR1_02_a_node_stating_both_a_matrix_and_a_trs_is_refused` — the matrix is accepted; stating both forms is not |
| `a_non_identity_node_rotation_is_refused` / `..._scale_is_refused` | `GLBIR1_04_a_zero_node_scale_is_refused`, `..._a_flattened_node_matrix_is_refused`, `..._a_non_affine_node_matrix_is_refused` — a real rotation and scale are accepted; a singular one is not |
| `a_missing_NORMAL_is_refused` | `GLBIR1_12_an_unknown_custom_attribute_is_refused` — a missing NORMAL is generated; an attribute the reader has never heard of is still refused |
| `world_position_is_local_plus_translation` | `the_node_translation_is_baked_into_the_positions` + `world_position_reads_the_baked_position` — the transform moved from the draw item into the vertices |
| `the_draw_item_carries_the_files_translation` | `the_draw_item_model_is_identity_because_the_transform_is_baked` + `and_the_files_translation_is_in_the_vertices` |

Two instrumented wording assertions (`glbir0_20`, `fsr1b18`) asserted the R0
copy — "the GLB row must be framed as a check" — and now assert the R1 copy:
the row names the act (*Import GLB*) and the group names what it produces
(*preview*). The scope half of both — no OBJ, FBX, material, texture or
animation anywhere on the surface — is unchanged and still asserted.
