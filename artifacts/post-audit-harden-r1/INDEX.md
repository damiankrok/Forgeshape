# POST-AUDIT-HARDEN-R1 — evidence index

**Result: `PASS-POST-AUDIT-HARDEN-R1-OWNER-RETEST-READY` — five findings closed, focused PAH-R1-01..16 PASS, all regression gates PASS, `FULL_SHARDED_SUITE_PASS` (40 classes, 524 tests, 5/5 shards) on the final tree.**

A narrow maintenance stage after `DEEP-AUDIT-R1` that closed exactly five
audit findings — F-08, F-16, F-11, F-15, F-18 — and nothing else. Baseline
`af6a719a7fad847fc3ad06289cdfd5a077c46822`, clean tree.

| File | Content |
| --- | --- |
| `BASELINE.md` | resolved baseline hash, clean-tree proof, device identity, the five findings as the audit stated them |
| `F08_JNI_SCULPT_LOCKING.md` | the seven-entry map (Java method, native entry, thread, rebind, state read, lock before/after), the five same-shape sites, the lock-order/duration argument, the concurrency tests |
| `LOCK_CONTEXT_BEFORE.txt` / `LOCK_CONTEXT_AFTER.txt` | `tools/lock_context.js` output on the baseline and the fixed `forgeshape_jni.cpp`: 12 unlocked `sculptSession()` calls → 0 |
| `F16_RELEASE_SELFTESTS.md` | the CMake source-list split and why the two fixture files stay |
| `BUILD_SYMBOLS_SIZE.md` | release and debug `.so` sizes, self-test symbol and string counts, JNI export counts, the F-15 symbol, per-configuration TU counts — before and after, both ABIs |
| `RELEASE_BUILD.txt` | `:app:assembleRelease` output |
| `DEVICE_STARTUP.txt` | debug launch logcat: twenty `*_SELFTEST_OK` (2981 checks), `FORGESHAPE_NATIVE_VIEWPORT_OK` |
| `F11_JNI_EXCEPTION_ORDER.md` | the `touchEvent` ordering fix, RED and GREEN |
| `F11_RED_CRASH_LOGCAT.txt` | the CheckJNI abort on the pre-fix JNI |
| `FOCUSED_RED_PREFIX_JNI.txt` | runner output of the RED run (`Process crashed.`) |
| `FOCUSED_GREEN.txt` | runner output of the final focused run (`OK (5 tests)`) |
| `F15_LINKAGE.md` | `buildSpherifiedBox` internal linkage, consumers, proof |
| `F18_DEAD_UI_RESOURCE.md` | no-caller/no-reference proof and what was removed |
| `TEST_RESULTS.md` | PAH-R1-01..16 table, the nine regression gates, both FullSharded attempts |
| `DEVICE_GUARDS.txt` | `verify-device-guards.ps1` — DEV2-01..07, DEV3-01..06 |
| `CORPUS_VERIFYONLY.txt` | `build-forge-corpus.ps1 -VerifyOnly` — 28 fixtures |
| `FULL_SHARDED_ATTEMPT1_FAIL.txt` | the first aggregate, failed by two fixture assumptions in the new test (see `TEST_RESULTS.md`) |
| `FULL_SHARDED.txt` | the authoritative aggregate on the final runtime/test tree |
| `tools/lock_context.js` | the lock-context classifier (`node tools/lock_context.js app/src/main/cpp/forgeshape_jni.cpp`; exit 1 on any unlocked call) |

## Files changed

| File | Finding | Change |
| --- | --- | --- |
| `app/src/main/cpp/forgeshape_jni.cpp` | F-08, F-11 | every `sculptSession()` call under `g_stateMutex`; `publishSculptRepresentation` takes the session; `logSculptStateLocked`; per-read `ExceptionCheck` in `touchEvent` |
| `app/src/main/cpp/CMakeLists.txt` | F-16 | `FORGESHAPE_PRODUCT_SOURCES` + `FORGESHAPE_SELFTEST_SOURCES`, the latter only for `CMAKE_BUILD_TYPE` Debug |
| `app/src/main/cpp/forgeshape_mesh_fixtures.cpp` | F-15 | `buildSpherifiedBox` in an anonymous namespace |
| `app/src/main/java/com/forgeshape/app/EditorControlStyles.java` | F-18 | `setChipReserved` removed; one javadoc sentence rewritten |
| `app/src/main/res/drawable/bg_control_reserved.xml`, `bg_capsule_reserved.xml` | F-18 | deleted |
| `app/src/androidTest/java/com/forgeshape/app/JniBoundaryHardeningTest.java` | F-08, F-11 | new: PAH-R1-03..06, PAH-R1-12/13 |
| `ARCHITECTURE.md` | F-08 | the sculpt-session reader rule added to the threading paragraph |
| `PROJECT_STATUS.md` | all | result, the F-16 statement, the debt list, the next step |

## Out of scope — confirmed unchanged

No file of the deferred debt was touched: `forgeshape_sculpt.{h,cpp}` /
`forgeshape_sculpt_history.*` (F-06, global budget), `forgeshape_cad_face.*`
and `forgeshape_scene.{h,cpp}` (F-07, F-10, `nextObjectId`),
`forgeshape_sketch.*` / `forgeshape_cad_body.*` (F-09), the representation
branches in export/roundtrip/publish/sculpt-source/capture (F-13; the
`publishSculptRepresentation` signature change is the same branch with one
parameter added, not a unification), no decomposition of `forgeshape_jni.cpp`
(F-14), no `EditorWorkspaceView` split, no spatial acceleration, no boolean,
fillet, solver, format or Home/Sketch feature change. `git diff --stat` is the
list above and nothing else.
