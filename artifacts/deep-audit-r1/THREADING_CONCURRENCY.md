# Threading and concurrency — DEEP-AUDIT-R1

## Threads

| Thread | Created by | Runs | Ends |
| --- | --- | --- | --- |
| Android main (UI) | platform | every `NativeViewport` call except the three autosave calls; all touch input; all chrome | process |
| `renderThreadMain` | `NativeViewport.start()` | Vulkan device/swapchain, per-frame snapshot → draw → present, device-loss recovery | `NativeViewport.stop()` (joined) |
| `forgeshape-autosave` (`HandlerThread`) | `AutosaveController` per `EditorWorkspaceView` | `projectOpen()`, `projectFingerprint()`, `encodeProject()`, `ProjectCheckpoint.write` | `release()` → `quitSafely` in `Activity.onDestroy` |

No other thread exists (grep: no `std::thread` outside `renderThreadMain`, no `AsyncTask`/`Executor` in Java).

## Locks and what they guard

| Lock | Guards | Taken by |
| --- | --- | --- |
| `g_stateMutex` (JNI) | camera, selection, scene, both histories, sketch session, support chooser, gizmo, sculpt session, gesture routing flags (`g_grabbing`, `g_strokePending`) | UI-thread JNI entries (write and most reads), autosave thread (`projectFingerprint`, `encodeProject`), render thread once per frame while taking the scene/gizmo/overlay snapshot |
| `g_viewport.mutex` + condvars | surface handshake (window pointer, attach/detach acks, resize request, stop flag) | UI thread (`surfaceCreated/Changed/Destroyed`, `stop`) and the render thread |
| `MeshStore::mutex_` (per body) | the published revision chain | publisher (UI thread) and the render thread's snapshot/upload; inner lock, never held while taking `g_stateMutex` |
| `std::atomic` | display settings, renderer lifecycle state, rebuild counters, `g_activeBodyMisuse` | any thread |
| Java `synchronized (this)` in `AutosaveController` | counters and `released` | UI + worker |
| Java `synchronized` in `DiagnosticLog` | the ring | any |

Lock order observed everywhere: `g_viewport.mutex` is never held while taking `g_stateMutex` and vice versa; `MeshStore::mutex_` is only ever innermost. No deadlock path found.

## Contracts checked

* **Render thread never touches a destroyed window**: `surfaceDestroyed` waits (≤ 5 s) for the render thread's detach ack under `g_viewport.mutex`; the render thread checks the detach request between frames and after every blocking wait. Bounded waits: `releaseBodiesAbsentFromScene` (100 ms fence wait), swapchain acquire timeouts; the one unbounded wait (`waitForMeshBuffersIdle` before an upload) is only reachable while the loop is still presenting, and a failed submit stops the loop first (`Unrecoverable → StopRestartRequired`). Verdict: contract holds; the 5 s timeout is defensive.
* **Gesture → mesh mutation**: a stroke is promoted only on the UI thread under `g_stateMutex`; the render thread reads sculpt geometry only through a `MeshStore` revision it copies under the store's own lock. No cross-thread writer to `SculptMesh`.
* **Autosave reads while the UI edits**: `projectFingerprint`/`encodeProject` hold `g_stateMutex` for the whole capture, so a checkpoint is a consistent document; the UI thread blocks for that duration (encode of a large sculpt project is the cost — measured `AUTOSAVE_CHECKPOINT ms=` in the diagnostics ring; not on the frame budget because the render thread does not need `g_stateMutex` except for its snapshot).
* **Snapshot per frame under `g_stateMutex`**: `constructionScene().snapshot()` runs `resolveWorldModel` for every body, which for a face-supported CAD body re-runs `cadTopologySignature` + `resolveCadFace` (full profile extraction) up the producer chain — every frame, holding the lock. Measured by `FORGESHAPE_CAD_A3_PERFORMANCE`: depth 1 = 45 µs, depth 8 = 441 µs, depth 32 = 4.35 ms per resolve. P2 perf finding F-07 (`PERFORMANCE_MEMORY.md`).

## Findings

| ID | Sev | Finding |
| --- | --- | --- |
| F-08 | P2 | **Unlocked JNI readers rebind the sculpt target.** `productMode`, `sculptState`, `sculptTool`, `constructionPrimitive`, `constructionMeshRevision`, `setSculptBrush`'s log read and `logMeshDiagnostics` call `sculptSession()` without `g_stateMutex`; `sculptSession()` WRITES `SculptSession::target_` on every call. The autosave thread's `encodeProject()` calls the same accessor under the lock, so two threads can store the pointer concurrently — formally a data race (UB), practically benign (aligned pointer store of the same value, since only the UI thread changes the active body). Fix is local (take the lock in those seven readers) but is a refactor outside the audit's "proven P0/P1" rule; reported. |
| F-07 | P2 | Per-frame re-extraction for face-supported CAD chains under `g_stateMutex` (above). |
| F-11 | P3 | `touchEvent` performs three JNI array reads before its one `ExceptionCheck`; JNI forbids further JNI calls while an exception is pending. Unreachable from the product (arrays are always length 6). |
| — | OK | `AutosaveController.checkpointedFingerprint`/`everCheckpointed` are worker-thread-only fields; `noteProjectPersisted` posts to the worker rather than writing from the UI thread — correct. |
| — | OK | `Diagnostics` static `handlerInstalled`/`previousExitChecked` are guarded by `synchronized` static methods. |

Sanitizer status: TSan NOT AVAILABLE (no host build, no NDK TSan configuration in the project). Reasoning above is by reading every lock site (`grep -n g_stateMutex` → 190 sites, all inspected).
