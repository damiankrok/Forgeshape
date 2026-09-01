# Test results — ForgeShape architecture health review (2026-09-01)

All runs on the corrected tree. Device: `emulator-5580`, confirmed
`ForgeShape_Stage006` via `adb -s emulator-5580 emu avd name`; log buffer
64 MiB (`logcat -g`). `emulator-5554` was never contacted (all adb calls
`-s emulator-5580`; `scripts\run-instrumented-tests.ps1` refuses 5554).

| Check | Command | Result |
| --- | --- | --- |
| Build | `gradlew.bat :app:assembleDebug` | EXIT=0 (run twice: after the code fixes, and again after the final `forgeshape_history.h` comment edit) |
| JVM unit | `gradlew.bat :app:testDebugUnitTest` | 70/70 pass |
| Native self-tests | launch + `logcat -s ForgeShape:V` → `startup-logcat.txt` | 17/17 `*_SELFTEST_OK`, `FORGESHAPE_NATIVE_VIEWPORT_OK`, 0 `_SELFTEST_FAIL`, 0 `_FAIL:` |
| Startup publish token | same log | `FORGESHAPE_CONSTRUCTION_PUBLISHED:1:8:36` (unchanged after fix #2) |
| `ProjectAutosaveRecoveryTest` | runner `-TestClass` | 14 OK (`instrumented-ProjectAutosaveRecoveryTest.txt`) |
| `EditorWorkspaceHistoryTest` | runner `-TestClass` | 20 OK |
| `ImportedMeshDurableTest` | runner `-TestClass` | 12 OK |
| `EditorWorkspaceControlsTest` | runner `-TestClass` | 23 OK |
| `EditorWorkspaceSculptRetentionTest` | runner `-TestClass` | 2 OK |

Focused instrumented classes are subset evidence, chosen to cover the two
behaviour changes (autosave + shape Apply under lock; publish counts on
history steps, import and Construction publish). No `FULL_SHARDED_SUITE_PASS`
is claimed. No screenshots were taken: no UI behaviour changed.

Measured (not inferred) performance datum from the startup log:
`RENDER_MESH_BUILD ... src=8:36 render=24:36 ms=0.034`, one `MESH_UPLOAD_OK`.
