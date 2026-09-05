# Test results — POST-AUDIT-HARDEN-R1

Device: `emulator-5580` = `ForgeShape_Stage006` (confirmed by `emu avd name`
before every device step; `emulator-5554` never contacted). Every `adb` call
carried `-s emulator-5580`; every instrumented run went through
`scripts\run-instrumented-tests.ps1 -Serial emulator-5580`.

## Focused `PAH-R1-01..16`

| ID | Requirement | Evidence | Result |
| --- | --- | --- | --- |
| PAH-R1-01 | all seven audited JNI sculpt-reader entries enumerated | `F08_JNI_SCULPT_LOCKING.md` §3.1 (Java method, native entry + line, thread, rebind, state read, lock before/after); `LOCK_CONTEXT_BEFORE.txt` lists every `sculptSession()` call at baseline with its lock context (12 unlocked in 11 functions, the seven included) | PASS |
| PAH-R1-02 | each relevant entry follows the state-lock contract | `LOCK_CONTEXT_AFTER.txt`: `TOTAL calls=54 unlocked=0`, tool exit 0; the contract is "every `sculptSession()` in `forgeshape_jni.cpp` is inside a `g_stateMutex` scope, and a `...Locked` helper's call sites are locked" | PASS |
| PAH-R1-03 | body switch cannot return cross-body Sculpt state | `JniBoundaryHardeningTest.bodySwitchNeverReturnsCrossBodySculptState` — `FOCUSED_GREEN.txt` | PASS |
| PAH-R1-04 | project load cannot leave a stale Sculpt target | `...projectLoadLeavesNoStaleSculptTarget` — `FOCUSED_GREEN.txt` | PASS |
| PAH-R1-05 | Delete/Undo cannot leave a stale Sculpt target | `...deleteAndUndoLeaveNoStaleSculptTarget` — `FOCUSED_GREEN.txt` | PASS |
| PAH-R1-06 | bounded concurrent query/mutation completes without deadlock | `...boundedConcurrentReadersAndBrushMutationsComplete` (two reader threads, 30 s join bound) plus the bounded join in every case above — `FOCUSED_GREEN.txt` | PASS |
| PAH-R1-07 | existing Sculpt stroke/Undo/Redo behaviour unchanged | native sculpt suite 494 checks (`DEVICE_STARTUP.txt`); `SculptUndoTest`, `ImportedMeshSculptTest`, `EditorWorkspaceSculptRetentionTest` in `FULL_SHARDED.txt` | PASS |
| PAH-R1-08 | debug build still contains/runs self-tests | `DEVICE_STARTUP.txt`: 20 `*_SELFTEST_OK`, 2981 checks, 0 `_SELFTEST_FAIL`/`_FAIL:`, `FORGESHAPE_NATIVE_VIEWPORT_OK`; Debug `compile_commands.json` has the 20 self-test TUs | PASS |
| PAH-R1-09 | release arm64 `.so` excludes self-test symbols | `BUILD_SYMBOLS_SIZE.md`: 0 `*SelfTests*` dynamic symbols, 0 check-name strings (was 20 / 181) | PASS |
| PAH-R1-10 | release x86_64 `.so` excludes self-test symbols | same: 0 / 0 (was 20 / 181) | PASS |
| PAH-R1-11 | release `.so` size delta measured | arm64 2 084 968 → 1 171 552 (−913 416); x86_64 2 339 080 → 1 236 296 (−1 102 784) | PASS |
| PAH-R1-12 | `touchEvent` exception checked before the dependent array read | `forgeshape_jni.cpp` `touchEvent`: `ExceptionCheck` after each of the three required region reads; `F11_JNI_EXCEPTION_ORDER.md` | PASS |
| PAH-R1-13 | JNI failure seam does not crash/leak | RED on the pre-fix JNI: CheckJNI abort `GetFloatArrayRegion called with pending exception` (`FOCUSED_RED_PREFIX_JNI.txt`, `F11_RED_CRASH_LOGCAT.txt`); GREEN on the fix: `touchEventDropsShortRequiredArraysWithoutPendingException` passes, three malformed events dropped, boundary still records afterwards | PASS |
| PAH-R1-14 | audited helper has internal linkage | `F15_LINKAGE.md`; `buildSpherifiedBox` dynamic symbol 1 → 0 in all four libraries; release and debug link | PASS |
| PAH-R1-15 | dead reserved-chip Java method/resource removed only if truly unused | `F18_DEAD_UI_RESOURCE.md`: grep proof of no caller/reference before removal; method + both drawables removed; builds and resource link succeed | PASS |
| PAH-R1-16 | no out-of-scope behaviour change | `git diff --stat` = `CMakeLists.txt`, `forgeshape_jni.cpp`, `forgeshape_mesh_fixtures.cpp`, `EditorControlStyles.java`, two deleted drawables, one new test class, docs; no file of F-06/F-07/F-09/F-10/F-13/F-14 scope touched (`INDEX.md` §Out of scope) | PASS |

Focused class runs (`JniBoundaryHardeningTest`, 5 tests):

| Run | Tree | Result |
| --- | --- | --- |
| RED | pre-fix `forgeshape_jni.cpp` (HEAD), rest of the stage applied | `Process crashed.` — CheckJNI abort in `touchEvent` (`FOCUSED_RED_PREFIX_JNI.txt`) |
| GREEN 1 | fixed tree | `OK (5 tests)`, 27.3 s |
| GREEN 2 | fixed tree, fixture made process-state-independent (below) | `OK (5 tests)`, 25.7 s (`FOCUSED_GREEN.txt`) |

## Regression gates (§11)

| Gate | Evidence | Result |
| --- | --- | --- |
| 1. full native self-tests | `DEVICE_STARTUP.txt` — 20 suites / 2981 checks / 0 failures, then `FORGESHAPE_NATIVE_VIEWPORT_OK`, ForgeShape the resumed activity | PASS |
| 2. Sculpt + SculptHistory | native `SCULPT_BRUSH_KERNEL` 494 checks; `SculptUndoTest`, `ImportedMeshSculptTest`, `EditorWorkspaceSculptRetentionTest` (aggregate) | PASS |
| 3. project load/save/history | native `PROJECT` 251 + `CONSTRUCTION_HISTORY` 147 checks; `ProjectAutosaveRecoveryTest`, `ProjectProcessDeathTest`, `ProjectTransferTest`, `EditorWorkspaceHistoryTest`, `EditorWorkspaceProjectActionsTest` (aggregate) | PASS |
| 4. Home / CAD-A3-C2 representative | `HomeFlowTest`, `SketchUxTest`, `SketchExtrudeTest`, `SpatialSketchTest`, `CadA3VisualEvidenceTest`, `SketchUxVisualEvidenceTest` (aggregate); native `CAD` 122 / `CAD_A3` 57 / `SKETCH_UX` 52 checks | PASS |
| 5. Imported Mesh / Delete | `ImportedMeshDurableTest`, `ImportedMeshSculptTest`, `ObjectsDeleteTest`, `GlbImport*`/`GlbExportTest` (aggregate); native `GLTF_IMPORT` 189 / `GLTF_EXPORT` 93 checks | PASS |
| 6. JVM | `:app:testDebugUnitTest` — 70 tests, 0 failures, 0 errors, 0 skipped (task executed, not up-to-date) | PASS |
| 7. debug/release arm64-v8a + x86_64 | debug built by the runner; `:app:assembleRelease` `BUILD SUCCESSFUL` (`RELEASE_BUILD.txt`); four libraries measured (`BUILD_SYMBOLS_SIZE.md`), LOAD alignment 0x4000 | PASS |
| 8. device guards | `DEVICE_GUARDS.txt` — DEV2-01..07, DEV3-01..06 all PASS | PASS |
| 9. corpus VerifyOnly | `CORPUS_VERIFYONLY.txt` — 28 fixtures verified, exit 0; the six v3 digests equal the device's `FORGESHAPE_PROJECT_GOLDEN_SHA256*` prints | PASS |

## Authoritative FullSharded (final runtime/test tree)

**Attempt 1** (`FULL_SHARDED_ATTEMPT1_FAIL.txt`): discovery PASS (40 classes,
524 tests), shard 1 PASS 106/106, shard 2 ASSERTION_FAILURE — two cases of the
NEW class failed on assumptions about the scene, not on the product: the shard
runs seven classes in one process, an earlier class left the baseline project
with extra bodies and a retained sculpt mesh on body 1, and the ordinary reset
keeps both. `projectLoadLeavesNoStaleSculptTarget` then saw body 1's stale
482-vertex mesh come back with the baseline document (correct load behaviour,
wrong test expectation), and `deleteAndUndoLeaveNoStaleSculptTarget` expected
body B to become active after deleting A when scene order put a leftover body
first. The fixture now starts from a fresh one-body project (close + seed, the
same route `resetToBaselineConstruction` takes for a CAD-only scene) and
asserts that precondition by name. No product file changed for this. The
aggregate was rerun from shard 1 on the corrected tree, as the policy requires.

**Attempt 2 — authoritative** (`FULL_SHARDED.txt`):

```
discovered_classes=40
discovered_tests=524
shard_count=5
shard_1_tests=106
shard_1_status=PASS
shard_2_tests=105
shard_2_status=PASS
shard_3_tests=104
shard_3_status=PASS
shard_4_tests=103
shard_4_status=PASS
shard_5_tests=106
shard_5_status=PASS
assigned_union=524
executed_union=524
missing=0
duplicates=0
unexpected=0
execution_missing=0
failed_shards=0
aborted_shards=0
aggregate=PASS
FULL_SHARDED_SUITE_PASS
```

Discovery PASS, assigned = executed = 524, missing = duplicates = unexpected = execution_missing = 0, all five shards PASS with exact expected counts, aborted = 0, `FULL_SHARDED_SUITE_PASS`. Taken on the final runtime/test tree; only documentation and evidence files changed afterwards.
