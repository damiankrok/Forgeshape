# TEST_RESULTS — SCULPT-UNDO-R0

## Where each half is proved

The DOMAIN invariants are proved by the native self-test suite, which builds its
own scenes, sessions and histories and depends on no live session — so a result
never depends on what another case left behind. What only a device can prove —
the two real chrome controls, the native mode dispatch behind them, the
separation from the Construction history in a running session, and the
persistence rule — is proved by `SculptUndoTest`.

## Native self-tests

`FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK (494 checks)`, of which **84 are the
new `SCUNDO_*` checks**. Zero failures, first run.

All seventeen suites, all green, **2712 checks total** (2628 before this stage,
+84):

```
FORGESHAPE_CAMERA_SELFTEST_OK (119 checks)
FORGESHAPE_PICKING_SELFTEST_OK (174 checks)
FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK (91 checks)
FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK (100 checks)
FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK (121 checks)
FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK (125 checks)
FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK (105 checks)
FORGESHAPE_CONE_CAPSULE_SELFTEST_OK (163 checks)
FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK (494 checks)   <- +84
FORGESHAPE_RENDER_SHADING_SELFTEST_OK (329 checks)
FORGESHAPE_SCENE_SELFTEST_OK (79 checks)
FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_OK (147 checks)
FORGESHAPE_GIZMO_SELFTEST_OK (145 checks)
FORGESHAPE_PROJECT_SELFTEST_OK (219 checks)
FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK (24 checks)
FORGESHAPE_GLTF_EXPORT_SELFTEST_OK (93 checks)
FORGESHAPE_GLTF_IMPORT_SELFTEST_OK (184 checks)
FORGESHAPE_NATIVE_VIEWPORT_OK
```

Captured with a 64 MiB log ring buffer, confirmed by `logcat -g` before the run.
No `_SELFTEST_FAIL`, no `_FAIL:`, no `CASE_FAIL`.

No eighteenth token was added: the sculpt history is the sculpt domain, so its
checks live in the sculpt suite. The seventeen-token contract in `CLAUDE.md` is
unchanged.

## Focused test table — `SCUNDO-01..24`

| ID | Requirement | Where | Result |
| --- | --- | --- | --- |
| SCUNDO-01 | one completed Construction Sculpt stroke = one entry | `SCUNDO_01_one_completed_stroke_is_one_entry` + `E2E-SCUNDO-01` | PASS |
| SCUNDO-02 | one completed Imported Sculpt stroke = one entry | `SCUNDO_02_one_completed_imported_stroke_is_one_entry` + `E2E-SCUNDO-02` | PASS |
| SCUNDO-03 | repeated pointer moves in one stroke do not create multiple entries | `SCUNDO_03_and_four_moves_did_not_make_four_entries`, with `SCUNDO_13_the_revision_advanced_more_than_once_inside_the_stroke` proving they were separate batches | PASS |
| SCUNDO-04 | no-op/cancelled stroke creates no entry | `SCUNDO_04_a_stroke_that_moved_nothing_records_no_entry`, `SCUNDO_04_and_a_cancel_with_no_movement_records_nothing_either` | PASS (see DEVIATIONS) |
| SCUNDO-05 | Undo restores exact pre-stroke positions | `SCUNDO_05_and_restores_the_exact_pre_stroke_positions` (bit-exact, every vertex) + `E2E-SCUNDO-01` (bit-exact `SCUL` bytes) | PASS |
| SCUNDO-06 | Redo restores exact post-stroke positions | `SCUNDO_06_and_restores_the_exact_post_stroke_positions` + `E2E-SCUNDO-01` | PASS |
| SCUNDO-07 | two strokes undo/redo in correct order | `SCUNDO_07_*` (five checks: seed / after-A / after-B in both directions) + `E2E-SCUNDO-03` | PASS |
| SCUNDO-08 | new stroke after Undo clears Redo | `SCUNDO_08_and_clears_the_redo_stack`, `SCUNDO_08_leaving_the_undo_stack_correct` + `E2E-SCUNDO-04` | PASS |
| SCUNDO-09 | source Construction/Imported truth unchanged | `SCUNDO_09_*` (positions, normals, topology, no invented Construction Source) + `E2E-SCUNDO-02` against the document's own `IMPT` bytes | PASS |
| SCUNDO-10 | SceneObject transform unchanged | `SCUNDO_10_the_body_placement_is_unchanged` | PASS |
| SCUNDO-11 | fresh-first-stroke Undo restores correct `hasEdits=false` baseline | `SCUNDO_11_a_fresh_first_stroke_undone_reports_no_edits` + `E2E-SCUNDO-01`; visible in the device log as `edits=0` on a step whose revision went UP | PASS |
| SCUNDO-12 | loaded-edited-sculpt Undo preserves `hasEdits=true` | `SCUNDO_12_and_hasEdits_is_still_true_because_the_file_carried_edits`, with `SCUNDO_12_which_is_not_what_undo_depth_would_have_said` naming the trap | PASS |
| SCUNDO-13 | runtime revisions remain monotonic across Undo/Redo | `SCUNDO_13_and_the_revision_still_went_forwards`, `SCUNDO_13_and_the_revision_forwards_again` | PASS |
| SCUNDO-14 | Construction and Imported use same history implementation | By construction — one `SculptHistory` type, one `record`, one apply, reached by both cases through the same `SculptSession`. Stated at the head of the imported block. | PASS |
| SCUNDO-15 | body A/B histories never cross | `SCUNDO_15_*` (six checks) + `E2E-SCUNDO-06` | PASS |
| SCUNDO-16 | Back to Resume retains runtime history | `SCUNDO_16_*` + `E2E-SCUNDO-05` | PASS |
| SCUNDO-17 | destructive reset clears history | `SCUNDO_17_*` (three checks) + `E2E-SCUNDO-09` | PASS |
| SCUNDO-18 | body Delete clears history safely | `scundo18_deletingASculptedBodyIsSafeAndUndoRestoresItWholly` | PASS |
| SCUNDO-19 | project history depth unchanged by Sculpt stroke/Undo/Redo | `scundo19_theProjectHistoryIsUntouchedBySculptAndStillWorksAfterwards` | PASS |
| SCUNDO-20 | save/reopen preserves current mesh but history is empty | `scundo20and21_neitherSaveNorAutosaveEverSerializesTheSculptHistory` | PASS |
| SCUNDO-21 | autosave/recovery likewise never serializes history | same case: the checkpoint is byte-compared against the manual encode | PASS |
| SCUNDO-22 | step-count and byte-budget eviction deterministic | `SCUNDO_22_*` (nine checks) + `E2E-SCUNDO-10` | PASS |
| SCUNDO-23 | oversized single entry handled without corruption/unbounded allocation | `SCUNDO_23_*` (seven checks, including the three malformed-delta refusals) | PASS |
| SCUNDO-24 | UI/JNI contextual dispatch and enabled states correct | `SCUNDO_24_*` (six checks) + `assertHistoryControls` at fourteen points across `E2E-SCUNDO-01/03/05/06/09/10` | PASS |

## How geometry is asserted

Two independent routes, both bit-exact, because an Undo restores stored floats
verbatim rather than recomputing them:

- **Native:** `allPositions(mesh)` / `samePositions` — every vertex of the frozen
  mesh, compared with `!=` on each float component, not with a tolerance.
- **Device:** the `.forge` document's `SCUL` section payload, which is where a
  Frozen Sculpt Mesh's positions are project truth. The same route
  `ImportedMeshSculptTest` reads `IMPT` by. Nothing reads a vertex through an
  accessor written for a test.

`SCULPT_REVISION` deliberately cannot serve as the geometry witness: it is
monotonic by design and goes FORWARD across an Undo, so it proves that something
changed and never what it changed to. That is why the `SCUL` bytes are the
witness and the revision is asserted separately, as monotonicity.

## Instrumented suites

| Class | Tests | Result |
| --- | --- | --- |
| `SculptUndoTest` (**new**) | 10 | OK |
| `EditorWorkspaceHistoryTest` | 20 | OK |
| `ImportedMeshSculptTest` | 9 | OK |
| `EditorWorkspaceSculptRetentionTest` | 2 | OK |
| `ImportedMeshDurableTest` | 12 | OK |
| `ObjectsDeleteTest` | 8 | OK |
| `ProjectAutosaveRecoveryTest` | 14 | OK |
| `GlbImportPreviewTest` | 23 | OK |

## One shared test helper was changed, and it was a real gap

`WorkspaceTestSupport.sculptTheViewport` called
`NativeViewport.touchEvent(...)` directly, bypassing the `SurfaceView` the user
actually touches. That exercised the brush but never the production path's last
step: on `ACTION_UP` the surface calls `onViewportGestureSettled`, which is the
**only** moment the Android layer ever learns a stroke happened — a sculpt stroke
is resolved entirely in native code and passes through no Java.

The first device run caught this exactly: Undo and Redo worked when clicked, and
their ENABLED state was stale, because the chrome was never told. The helper now
dispatches real `MotionEvent`s to the viewport surface, so the gesture takes the
whole production path. Same coordinates, same step pattern, same stroke — plus
the edge the chrome depends on.

A helper that stopped one call short of the production path would have passed
while the real Undo control stayed grey, which is the defect class this stage
exists to remove. Six suites share the helper and all were re-run.

## Two pre-existing assertions changed, and only these two

Both asserted the withdrawal this stage supersedes; both still assert the part
that did NOT change.

| Case | Was | Now |
| --- | --- | --- |
| `EditorWorkspaceHistoryTest.s01920` | `historyGroup` is `GONE` in Sculpt | `VISIBLE` in Sculpt, where it means the Sculpt history. Still asserts `constructionUndo()` and `constructionRedo()` return `HISTORY_REFUSED_IN_SCULPT` and that the refusal moves no Construction step. |
| `ImportedMeshSculptTest.imp01b06` | `historyGroup` is `GONE` in Sculpt | `VISIBLE`. Still asserts the Construction refusal below JNI and that a Construction undo taken after leaving Sculpt moves no sculpted vertex. |

No other test's expectations were altered, and no test was deleted, skipped or
weakened.

## JVM unit tests

`70 / 70`, zero failures, zero errors.

## Builds

| Gate | Result |
| --- | --- |
| `:app:assembleDebug` (arm64-v8a + x86_64) | BUILD SUCCESSFUL |
| `:app:assembleRelease` (arm64-v8a + x86_64) | BUILD SUCCESSFUL |
| `:app:testDebugUnitTest` | BUILD SUCCESSFUL, 70/70 |
| `:app:compileDebugAndroidTestJavaWithJavac` | BUILD SUCCESSFUL |

NDK stayed pinned at `29.0.14206865`. No AGP, Gradle, JDK or CMake change.

## Device guards

`scripts\verify-device-guards.ps1`: **all PASS** — `DEV2-01..07` and
`DEV3-01..06`. No repo script issues a bare, unscoped `adb` call.

## `.forge` corpus

`scripts\build-forge-corpus.ps1 -VerifyOnly`: all twelve fixtures byte-identical,
and all seven canonical digests match what the build prints at runtime
(`FORGESHAPE_PROJECT_GOLDEN_SHA256`, `..._IMPORTED`, `..._IMPORTED_SCULPT`):

```
construction:          8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf
sculpt:                112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784
imported_only:         0f42be318da9faf4aa780e152b8550171085a267882d5e6e69cc9ef29a1539a8
construction_imported: 539e10e7a9e388ab1ca72867b78c1e461d3bbe0ef5876d87fa54bc6bd6ae7a51
mixed_imported:        3fdc82a099da69b93552d7c84c56002ed8ae24ba7086ddbc6a69a9bbf671f1fb
imported_sculpt:       b82430cf6dbb82fddf075722d7ae335460f687d2a06cde09db43817323729f76
mixed_imported_sculpt: ab709ecea27ec29f21b6fbef126e8cdc15dc5c733d9b751bd1c8832907f27a2b
```

**Expected, and confirmed: no schema change, no version bump, no digest drift.**
The Sculpt history is not in the format.

## Device discipline

Every device act used `adb -s emulator-5580`, confirmed
`ForgeShape_Stage006` by `adb -s emulator-5580 emu avd name` before any install.
The reserved `emulator-5554` (`Medium_Phone_API_36.1`) was attached throughout
and was **never contacted**: not installed to, not logged, not screenshotted, not
started, not stopped, not sent input. No bare `adb devices` enumeration drove any
action, no unscoped `connectedAndroidTest` was run, and `adb kill-server` was
never used.
