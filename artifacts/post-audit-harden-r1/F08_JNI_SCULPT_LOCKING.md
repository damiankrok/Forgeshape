# F-08 — JNI sculpt readers and the borrowed sculpt target

## The defect

`forgeshape::sculptSession()` (`forgeshape_scene.cpp`) re-points the one
process-scoped `SculptSession` at the active body's `FrozenSculpt` on EVERY
call (`bindTarget` writes `SculptSession::target_`). The autosave worker calls
it under `g_stateMutex` (`encodeProject`, `projectFingerprint`); seven JNI
entry points called it with no lock at all. Two threads storing the same pointer
is a formal data race, and — the observable half — a reader's `session.mesh()`
dereferences `target_` per call, so a read spanning several fields could be
re-pointed mid-read by the other thread and report one body's mesh under another
body's id.

## 3.1 — the seven audited entries (PAH-R1-01)

Line numbers are `forgeshape_jni.cpp` at baseline `af6a719`. "Thread" is what
the product calls it from; every entry is ALSO reachable from any Java thread,
which is what the new suite exercises.

| # | Java method | Native entry (line) | Thread | Rebinds via `sculptSession()`? | State read | Lock before | Lock after |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `NativeViewport.productMode()` | `Java_..._productMode` (1958) | UI | yes (1959) | `inSculptMode()` | none | `g_stateMutex` across the read |
| 2 | `NativeViewport.sculptState(double[])` | `Java_..._sculptState` (4855) | UI (and tests) | yes (4860) | 12 values from the session AND its target: mode, hasMesh, revision, vertex/index count, radius, strength, stale, strokes, objectId, tool, hasEdits | none | one hold across all twelve reads into a local array; `SetDoubleArrayRegion` after the hold |
| 3 | `NativeViewport.sculptTool()` | `Java_..._sculptTool` (4924) | UI | yes (4925) | `tool()` | none | `g_stateMutex` across the read |
| 4 | `NativeViewport.constructionPrimitive(double[])` | `Java_..._constructionPrimitive` (1801) | UI | no — reads `activeConstructionOrNull()`, i.e. `constructionScene().activeBody()`, unlocked | kind + six parameter sets of the active body | none | one hold across the scene read into a local array; JNI write after |
| 5 | `NativeViewport.constructionMeshRevision()` | `Java_..._constructionMeshRevision` (2474) | UI / tests | no — reads `meshStore()`, i.e. `activeBody().meshStore()`, unlocked | store revision | none | `g_stateMutex` across the read |
| 6 | `NativeViewport.setSculptBrush(double,double)` | `Java_..._setSculptBrush` (4885) | UI | yes (4887, BEFORE the lock), then the log read after it | tool, radius, strength for the log line | hold around the two setters only | `sculptSession()` fetched inside the hold; the three log values captured under it; log after |
| 7 | (debug) `debugMeshCommand(5)` and the stress thread | `logMeshDiagnostics` (1183) → `logSculptState` (1166) | UI (debug) and the DEBUG stress thread | yes (1167), plus unlocked `meshStore()` / `activeConstructionOrNull()` / `constructionScene()` reads | store, primitive, placement, the whole sculpt state | one inner hold around the transform values only | `logMeshDiagnostics` takes ONE hold after the atomic GPU snapshot; `logSculptState` locks itself; `logSculptStateLocked` is the lock-required body both share |

## Same-shape sites found while mapping (fixed on the same terms)

`tools/lock_context.js` (brace tracking with comments and strings stripped)
found 12 unlocked `sculptSession()` calls in 11 functions at baseline, not 7 —
`LOCK_CONTEXT_BEFORE.txt`. The five the audit did not list are the same defect:

| Function (baseline line) | Shape | Fix |
| --- | --- | --- |
| `setSculptTool` (4908) | session fetched before the hold; log and return read after it | fetched under the hold; the resulting tool captured under it |
| `freezeToSculpt` (2027) | the `FORGESHAPE_SCULPT_FROZEN` log read the mesh after the hold was released | the session pointer is captured inside the existing hold; the log block runs under a second bounded hold |
| `runHistoryStep` (3931) | the mode/sketch guards read before the hold | the guards moved under the step's own hold (a mode change can no longer slip between the check and the act); `bodyCount` for the log captured under it |
| `touchEvent` (5799, 5803) | the `PENDING`/`ABANDONED` tokens read the revision after the hold | `sculptRevisionAfter` captured at the end of the hold |
| `publishSculptRepresentation` (887), `publishActiveRepresentation` (905) | the publish helper fetched the session itself and was called both under the hold (`applyPrimitive`, `runSculptHistoryStep`, `touchEvent`) and outside it (`start`, `freezeToSculpt`, `enterSculptMode`, the stress freeze) | the helper now TAKES `SculptSession&`; locked callers pass the one they hold, unlocked callers capture `&sculptSession()` inside the hold they already take and publish after it. The session is a function-local static, so the pointer is stable; only the REBIND needs the lock |

Result: `LOCK_CONTEXT_AFTER.txt` — 54 `sculptSession()` calls, 0 outside a
`g_stateMutex` scope. That is the mechanical contract now (PAH-R1-02): every
`sculptSession()` in `forgeshape_jni.cpp` is inside a scope holding
`g_stateMutex`; the one helper that reads it on its caller's lock is named
`...Locked` and its call sites are checked by the same tool (exit code 1 on any
violation).

## 3.2 — why this is the smallest change (order, duration, no second lock)

* Only `g_stateMutex` is taken. No reader acquires `g_viewport.mutex`,
  `MeshStore`'s mutex or anything else, so no lock-order edge is added.
  `logMeshDiagnostics` reads the atomic GPU counters BEFORE the hold, so "GPU
  diagnostics never sit under the state mutex" still holds.
* Every new hold is bounded to a handful of field reads or one log line; no
  mesh generation, no publish and no JNI array write happens under a hold added
  by this stage (`sculptState` and `constructionPrimitive` copy into a local
  array and call `SetDoubleArrayRegion` after the hold).
* No second lock, no atomic mirror of `target_`, no snapshot object: an
  immutable snapshot returned by `sculptSession()` would have changed 50+ call
  sites for no safety the existing lock does not already give.
* No JNI entry point calls back into Java (no `Call*Method` in the file), so a
  Java caller can never re-enter one of these while `g_stateMutex` is held.
* Deadlock reasoning: the readers are leaf functions under the hold; the
  mutators they race with (`sceneSelectBody`, `loadProject`, `sceneDeleteBody`,
  `constructionUndo`, `setSculptBrush`, `setSculptTool`) already take the same
  single lock. PAH-R1-06 runs two reader threads against them with a 30 s join
  bound and both finish.

## 3.3 — tests (PAH-R1-03..07)

`app/src/androidTest/java/com/forgeshape/app/JniBoundaryHardeningTest.java`.
A reader thread — the autosave worker's shape — loops over ALL SEVEN readers
while the UI thread mutates, and every `sculptState` read must be internally
consistent: `hasMesh == 1 ⇒ objectId == A ∧ vertexCount == vA ∧ indexCount ==
iA`; `hasMesh == 0 ⇒ vertexCount == indexCount == 0`; `productMode` stays
Construction; `sculptTool` stays the fixture's tool; `constructionPrimitive`
is exactly A's box or exactly B's box (a torn read would mix them);
`constructionMeshRevision` is called for coverage. Every case is bounded (fixed
mutation count, capped reader loop, 30 s join), so a deadlock is an assertion
failure, never a hang.

| ID | Test | Mutation vs reader | After the run |
| --- | --- | --- | --- |
| PAH-R1-03 | `bodySwitchNeverReturnsCrossBodySculptState` | 400 `sceneSelectBody` switches A↔B | select A → bound to A with vA; select B → no mesh |
| PAH-R1-04 | `projectLoadLeavesNoStaleSculptTarget` | 60 `loadProject` alternations (document with A frozen ↔ baseline without a sculpt mesh) | after the sculpt document: active == A, bound to A, vA; after the baseline document: active == A, NO mesh (no stale target from the previous document) |
| PAH-R1-05 | `deleteAndUndoLeaveNoStaleSculptTarget` | 60 `sceneDeleteBody(A)` + `constructionUndo` pairs | Undo restores A with its sculpt state; Delete leaves B active with no mesh; Undo restores A again |
| PAH-R1-06 | `boundedConcurrentReadersAndBrushMutationsComplete` | two reader threads vs 400 brush/tool sets + 800 switches | both readers finish inside the bound, zero torn reads |
| PAH-R1-07 | existing `SculptUndoTest` (E2E-SCUNDO-01..10, SCUNDO-18..24) and the native sculpt suite (494 checks) | unchanged | `TEST_RESULTS.md`, `FULL_SHARDED.txt` |

Focused result on the fixed tree: `OK (5 tests)` in 27.3 s
(`FOCUSED_GREEN.txt`).

Why a Java reader thread and not a native seam: the lock lives in the JNI layer
and the racing thread in the product is a Java `HandlerThread`; a Java thread
calling the real entry points IS the path, and a native thread seam would have
been the test framework the stage forbids.

Why no RED for F-08: the race is benign at the store (same value, aligned), so
the pre-fix reader passes probabilistically and a RED would be luck. The lock
rule is proven mechanically instead (`LOCK_CONTEXT_BEFORE/AFTER.txt`); the RED
half of this stage is F-11, taken on the same pre-fix JNI in the same run.
