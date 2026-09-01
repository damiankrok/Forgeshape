# Safe corrections applied — ForgeShape architecture health review (2026-09-01)

Policy: concrete, bounded, provable; no reshuffles, renames, DI, framework,
renderer, JNI-surface, UI or `.forge` changes. Every item below is either a
behaviour-preserving fix inside one function or a comment/doc correction.

## 1. `applyPrimitive` now holds `g_stateMutex` (defect: data race)

- File: `app/src/main/cpp/forgeshape_jni.cpp`, `applyPrimitive()` (~L772).
- Was: the shape Apply wrote Construction parameters, the remembered
  parameter sets and the stale-source flag with no lock held, while the
  `forgeshape-autosave` worker reads them under `g_stateMutex` in
  `projectFingerprint()` and `encodeProject()`. A checkpoint could therefore
  encode half of one primitive and half of another. `applyBoxTransform`
  beside it already locked.
- Now: `std::lock_guard<std::mutex> lock(g_stateMutex);` before the
  `ScopedConstructionEdit`, held across generation exactly as `runHistoryStep`
  and `loadProject` already do.
- Deadlock analysis: the domain never takes `g_stateMutex` (no global, no
  include); `MeshStore`'s mutex is always taken inside it (lock order
  unchanged); the debug driver cases 6–9 that call `applyPrimitive` hold no
  outer lock. Verified at runtime: 17/17 self-tests, startup token unchanged,
  `ProjectAutosaveRecoveryTest` 14/14, `EditorWorkspaceHistoryTest` 20/20.
- JNI surface, Java, `.forge`: untouched.

## 2. `publishConstructionObject` no longer tessellates twice (waste)

- File: `forgeshape_jni.cpp`, `publishConstructionObject(const char*)` (~L706).
- Was: `object.generateMesh()` was called once for the log line's counts and
  again inside `forgeshape::publishConstructionObject` for the real publish —
  every shape Apply, history step and load generated each mesh twice.
- Now: counts are read from `meshStore().current()` after the publish (0 when
  nothing is published). Same log token, same values: startup still prints
  `FORGESHAPE_CONSTRUCTION_PUBLISHED:1:8:36`.

## 3. Comment corrections (no behaviour)

| File | Correction |
| --- | --- |
| `forgeshape_jni.cpp` `sceneAddBody` | removed the false claim that no lock is held across generation anywhere; explains why this call alone may publish unlocked |
| `forgeshape_transform.h` | `constructionTransform()` no longer described via the removed `constructionObject().transform()` |
| `forgeshape_scene.h` (header + accessor comment) | scene holds Construction Bodies OR Imported Meshes; names `activeConstructionOrNull()` |
| `forgeshape_scene.cpp` accessor block | migration prose replaced by the invariant (pointer-returning Construction accessor) |
| `forgeshape_history.h` `sameSceneConstructionState` | states `activeBodyId` is deliberately not compared and representation is |

## 4. `ARCHITECTURE.md` corrections (no behaviour)

- Platform services row names `ProjectSlot`, `ProjectCheckpoint`,
  `ProjectTransfer` (was pre-persistence wording).
- Serialization row describes the shipped `.forge` codec and points at
  `DATA_PACKAGE_SPEC.md` (was "not yet").
- History "Bounded" paragraph: the checkpoint and manual slot carry the
  PROJECT and never a step.
- Threading section: adds the autosave worker and states what
  `g_stateMutex` guards after IMPORT-01A (every body's Construction Source,
  placement, sculpt safety state, and the history).
- Lifecycle contract: what survives a process restart is what Save or
  autosave wrote.

## Not applied, on purpose

See `HOTSPOTS.md` §B, §E, §K (log read), §L and `FEATURE_CHANGE_SURFACE.md`.
Each would cross the policy line (renderer, exporter refactor, `.forge`
validation rule) or has no consequence worth a diff.
