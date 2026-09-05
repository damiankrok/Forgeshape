# Baseline — POST-AUDIT-HARDEN-R1

| Item | Value |
| --- | --- |
| Required prefix | `af6a719` |
| Resolved HEAD | `af6a719a7fad847fc3ad06289cdfd5a077c46822` |
| `git status --short` | empty |
| Branch | `master` (local only; no remote) |
| Device | `emulator-5580` = `ForgeShape_Stage006` (`adb -s emulator-5580 emu avd name`), state `device`; `emulator-5554` never contacted |

Native, Java and resource sources at `af6a719` are byte-identical to `d783d4b`
(`git diff --stat d783d4b af6a719 -- app/src/main app/build.gradle` is empty;
`af6a719` is the audit's docs-only commit), so the release and debug `.so`
files the deep audit built at `d783d4b` ARE the baseline binaries. Their
stripped copies were taken as the "before" evidence for F-16
(`BUILD_SYMBOLS_SIZE.md`).

Audit findings in scope, read from `artifacts/deep-audit-r1/`:

| Finding | Source | Summary |
| --- | --- | --- |
| F-08 | `THREADING_CONCURRENCY.md`, `FINDINGS.md` | seven unlocked JNI readers call `sculptSession()`, which writes `SculptSession::target_`; the autosave thread writes it under the lock |
| F-16 | `BUILD_RELEASE_ABI.md`, `DEAD_CODE_TODO.md` | the 20 self-test translation units are on the CMake source list unconditionally; release `.so` exports `run*SelfTests` and carries the check-name strings |
| F-11 | `NATIVE_CPP_JNI.md` | `touchEvent` makes three `Get*ArrayRegion` calls before one `ExceptionCheck` |
| F-15 | `DEAD_CODE_TODO.md` | `buildSpherifiedBox` has accidental external linkage |
| F-18 | `DEAD_CODE_TODO.md` | `EditorControlStyles.setChipReserved` and the reserved-chip drawable have no caller |
