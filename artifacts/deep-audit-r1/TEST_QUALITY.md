# Test quality — DEEP-AUDIT-R1 (Part R)

## Inventory

20 native self-test suites (2981 checks on the `d783d4b` device launch, +11 over the 2970 baseline from the three DAR1 groups); 18 of them run in the platform-neutral standalone runner (2628 checks; render-shading + render-recovery need the render/GPU seam). 39 instrumented classes (519 `@Test`), 8 JVM classes (70 `@Test`). The instrumented suites share one process, so no case may assume a body count, active body or mode — verified in `WorkspaceTestSupport.resetToBaselineConstruction`, which closes a CAD-only project and rebuilds a Construction baseline.

## TEST_TRUST_MATRIX

| Feature | production-path test | domain (native) test | persistence test | device test | gap |
| --- | --- | --- | --- | --- | --- |
| Construction primitives / apply | `EditorWorkspaceControlsTest` | box/primitive/sphere/cone_capsule (615) | golden corpus | `ImportedMeshDurableTest` roundtrips | — |
| Transform / gizmo | `EditorWorkspaceGizmoTest` (46) | gizmo (145), transform (121) | corpus 370°/non-uniform | gizmo suite | — |
| Sculpt strokes + Undo | `SculptUndoTest`, `EditorWorkspaceSculptRetentionTest` | sculpt (494) incl. `DAR1_02` | project (`SCUL`) | retention suite | F-02 was the persistence×sculpt gap — now closed |
| Delete | `ObjectsDeleteTest` | scene / body_delete | corpus | delete suite | — |
| CAD sketch/extrude/curves/face/edit | `SketchExtrudeTest`, `SpatialSketchTest`, `SketchUxTest` | cad (122), cad_a3 (57), sketch_ux (52) incl. `DAR1_01` | v1/v2/v3 corpus | three suites | live-length-during-drag deferred (brief E2) |
| `.forge` codec | — | project (251) incl. `DAR1_02` | golden corpus + negatives | `ProjectProcessDeathTest`, `ProjectAutosaveRecoveryTest` | curve-specific kill/reopen device case deferred (brief L15; bytes proven by CADUXR1-26/29/38) |
| GLB import/export | `GlbExportTest`, `ImportedMeshDurableTest`, `GlbImportExternalR1Test` | gltf_export (93), gltf_import (189) incl. `DAR1_03` | `IMPT` corpus | those suites | — |
| Home / lifecycle | `HomeFlowTest` | scene, bootstrap | recovery | `HomeFlowTest`, process-death | — |
| Fingerprint / dirty guard | `ProjectAutosaveRecoveryTest` | project, cad, cad_a3 fingerprint + `DAR1_01` | autosave | recovery suite | — |

## Items the brief asked to revisit

| Item | Verdict |
| --- | --- |
| Sculpt helper bypassing `onViewportGestureSettled` | `WorkspaceTestSupport.sculptTheViewport` dispatches to the real `SurfaceView`, so the `UP` DOES reach the settled listener — a helper calling `touchEvent` directly would miss the chrome edge; the comment says so and the code matches. No bypass. |
| Palette visibility count | `EditorWorkspaceMobileTest.countTiles` recurses and tests visibility (the CAD-R0 fix); expects the six primitives, and the New Sketch tile is counted where the case includes it. Correct after that stage's fix. |
| Home reset / order dependency | Fixed by CAD-A3-C2: `resetToBaselineConstruction` closes a project with no Construction Body and rebuilds one (a CAD-only project is not a baseline). The order dependency two classes tripped over is gone. |
| IME timing | `WorkspaceTestSupport` asks Android for the real keyboard first and waits for the root's target inset rather than a fixed duration; falls back to a deterministic dispatched inset. Sound. |
| FullSharded shard order | Deterministic greedy partition (largest class → lightest shard, stable name/index tie-break) in `instrumented-sharding.ps1`; discovery proves exactly-once union before execution. Verified by reading and by the run (`FULL_SHARDED.txt`). |

## New audit tests (added this stage)

`DAR1_01` (sketch_ux, ×2): moving an arc/spline point in place moves the fingerprint. `DAR1_02` (project, ×4): a load closes an in-flight stroke against the old body and records nothing on the loaded body, the loaded mesh is exactly the document's, and rebinding mid-stroke records the stroke nowhere. `DAR1_03` (gltf_import, ×5): the sanitizer keeps a 4-byte sequence, a JSON surrogate pair decodes to it, and `utf8ToUtf16` is exact for 1/2/3/4-byte sequences and maps malformed input to U+FFFD. All five groups fail against the pre-fix production files (proven: the RED standalone run reported 7 failing checks) and pass after the fix.

## Gaps (reported, not filled)

* No sanitizer / TSan build (F-08 is reasoned, not machine-checked). Recommended follow-up.
* The `renderReport` throwable branch has no case (forcing an uncaught exception kills the instrumentation process) — existing, PROJECT_STATUS.
* A real GPU device loss is only injected, never provoked (forbidden on the authoritative emulator) — existing.
