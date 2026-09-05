# Sculpt and sculpt history — DEEP-AUDIT-R1 (Part H)

## What was read

`forgeshape_sculpt.{h,cpp}` (SculptMesh, SculptTopology, SculptStroke, SculptSession), `forgeshape_sculpt_history.{h,cpp}`, the sculpt branches of `forgeshape_jni.cpp` (gesture arbitration, `freezeToSculpt`, `historyUndo/Redo` dispatch), `forgeshape_scene.cpp` (`sculptSession()` rebinding, `buildSculptSourceMesh`), `forgeshape_sculpt_selftest.cpp` (494 checks) and the instrumented `SculptUndoTest`, `ImportedMeshSculptTest`, `EditorWorkspaceSculptRetentionTest`.

## Invariants verified

| Invariant | Where enforced | Evidence |
| --- | --- | --- |
| One completed stroke = one entry, recorded once at stroke close from the affected set captured at pointer-down | `SculptSession::endStroke/cancelStroke → recordActiveStroke → SculptStroke::buildDelta → SculptHistory::record` | `SCUNDO_01/03/04` |
| A stroke that moved nothing records nothing; a cancelled stroke whose positions stand records them | `buildDelta` drops unmoved vertices; both endings call `recordActiveStroke` | `SCUNDO_04` |
| Entry = sorted unique indices + before/after positions + edited flag both sides; never normals, never a document | `SculptStrokeDelta`; `record` refuses unsorted/repeated indices | read + `SCUNDO_*` |
| Bounds: 32 entries, 4 MiB per body, 1 MiB per entry; oldest evicted first, never the newest; oversize applies but says so (`NotRetained`) | `SculptHistory::record/evictToFit` | `SCUNDO_22`, `MEMORY_BOUNDS.md` (SCULPT-UNDO-R0) |
| `hasEdits` is a stored fact, not `undoDepth > 0` | `SculptMesh::hasEdits_`; restored from `.forge` | `SCUNDO_11/12` |
| Revisions monotonic across undo | `advanceRevision()` on every apply | `SCUNDO_13` |
| Never serialized | no `SculptHistory` reference in the codec (grep) | `CORPUS` digests unchanged by SCULPT-UNDO-R0 |
| A source is never written by sculpting; Construction undo never moves a sculpt vertex | `SculptSession` has no mutable access to sources; `SceneConstructionState` holds no vertex | `SCUNDO_09/10`, history suite |
| Freeze / destructive reset clears both stacks; Undo cannot cross it | `freezeToSculpt → history.clear()` | `SCUNDO_17` |
| Multi-touch never mutates: PENDING-then-promote in JNI, second finger abandons | `forgeshape_jni.cpp` arbitration | `EditorWorkspacePointerTest`, runtime log tokens |

## Aggregate memory estimate (report only; no global cap added)

Per body the retained history is bounded by `kMaxSculptHistoryBytes` = 4 MiB (undo + redo together, enforced in `record` and on every eviction), plus the `SculptMesh` itself (positions + normals + indices + adjacency; a 500 k-vertex stress mesh ≈ 20 MB). The theoretical scene maximum (4096 bodies) is irrelevant to a phone; the realistic bound is `(sculpted bodies) × 4 MiB` of history plus detached bodies held by the Construction history (≤ 64 steps, each holding whole `SceneObject`s only when a step created or deleted a body). No product path can exceed the per-body cap, and a body's history dies with the body or with a Freeze. **No proven P0/P1 exists here, so no global cap was added**, per the brief. Follow-up worth its own decision: a process-wide soft ceiling that evicts the oldest entries of the least recently sculpted body when total retained bytes pass a threshold (e.g. 64 MiB).

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| F-02 | P1 | A stroke still in flight when `loadProjectDocument` ran was closed AFTER the session rebound to the loaded active body, so its delta (the destroyed body's positions) landed in the loaded body's `SculptHistory`, and an Undo there wrote foreign positions into the loaded mesh. API-reachable, not UI-reachable (the JNI `loadProject` clears only `g_grabbing`/`g_strokePending`, not the session's own `stroke_`). | FIXED — the load closes the stroke against the old body first, and `SculptSession::bindTarget` drops an in-flight stroke on any target change. `DAR1_02` (project suite). |
| F-06 | P2 | `updateStroke` republishes the whole `SculptMesh` per accepted move (O(vertices)); Inflate recomputes normals O(triangles) per move; the affected set and picking are both linear scans. Measured free at 482–514 vertices; the baseline a partial/async path must beat. | Reported (existing debt, re-confirmed). |
| — | OK | Aggregate-memory 4 MiB-per-body estimate reported above; no global cap added (no proven P0/P1). | — |