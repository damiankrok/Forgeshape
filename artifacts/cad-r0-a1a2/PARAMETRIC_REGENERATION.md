# Parametric regeneration

One path - `generateCadMesh` - run at:

| Trigger | Where |
| --- | --- |
| commit of a sketch | `ConstructionScene::addCadBody` regenerates ONCE before minting an id; `publishSceneObject` regenerates for the store |
| a sketch-size or depth edit | `CadBody::applyState` regenerates the whole candidate into scratch; only on Ok is the state written; then `publishSceneObject` |
| project load | `validateProjectDocument` runs `validateCadBodyState` (extraction included); `loadProjectDocument` publishes through `publishSceneObject` off the scene before the commit step |
| Undo / Redo | `ConstructionHistory::applyState` restores the `CadBodyState` and republishes only when it differs |
| GLB export | `captureGlbExportScene` regenerates from the state, never from the store |

Atomic from the project's perspective: a refused edit writes nothing
(`CADR0_12_invalid_rectangle_edit_refused_atomically`,
`CADR0_14_state_that_does_not_regenerate_is_refused_whole`), an identical edit
records nothing (`CADR0_12_identical_edit_changes_nothing`), and no
half-regenerated body exists: the mesh is produced from the accepted state and
published as one revision.

Deterministic: the same state produces bit-identical vertices and indices
(`CADR0_14_regeneration_is_deterministic`), on both ABIs.

Fail-closed at every layer: the domain refuses by `CadStatus`; JNI maps a CAD
refusal to the `APPLY_*` vocabulary the shape editor already speaks
(`APPLY_REJECTED_NOT_POSITIVE` for a bad length, `APPLY_REJECTED_CAD` for a
profile problem) and keeps the exact reason in `cadLastStatus()` and the log.
