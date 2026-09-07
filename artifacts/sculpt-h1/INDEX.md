# SCULPT-H1 — evidence index

Task: **SCULPT-H1** (prompt revision R2) — compact Sculpt History navigator,
tap-to-jump, and an optional real-stylus hover preview.

Baseline HEAD: `3283089febc5fc031304186a77ba0186d78fa983` (MIRROR-01), clean
worktree at start.

Device: `emulator-5580`, confirmed `ForgeShape_Stage006` by
`adb -s emulator-5580 emu avd name` before use. `emulator-5554` was never
contacted.

| File | What it holds |
| --- | --- |
| `OWNER_LATER_TEST_PACK.md` | what the owner still has to judge, including the hover-preview decision |
| `STARTUP_SELFTEST_LOG.txt` | the debug launch: 22 `*_SELFTEST_OK` tokens (3374 checks), 0 failures, `FORGESHAPE_NATIVE_VIEWPORT_OK` |
| `FOCUSED_NAVIGATOR_RUN.txt` | `SculptHistoryNavigatorTest` — **OK (9 tests)**, 48.21 s |
| `FOCUSED_REGRESSION_RUN.txt` | `SculptUndoTest` + `EditorWorkspaceHistoryTest` + `EditorWorkspaceChromeCompositionTest` — **OK (42 tests)**, 130.40 s |

## What was run, and what was not

**Run under `TEST-OWNER-03` / TEST POLICY v3, the reduced-testing policy.**
`-FullSharded` was **NOT run** and **no `FULL_SHARDED_SUITE_PASS` is claimed**.
This is focused evidence and cannot stand in for the exhaustive gate.

- Native domain: `SCHNAV-01..12`, **39 checks**, green on the standalone NDK
  runner and again inside the app launch. The sculpt suite went 494 → **533**
  checks; every other suite's count is unchanged.
- Device: the 9 navigator cases above, plus 42 regression cases over the two
  suites that share this area and the chrome-composition suite.
- Build: `assembleDebug` and `assembleRelease` both successful.
  `llvm-readelf --dyn-syms | grep -ci selftest` = **0** on the release
  `libforgeshape_native.so` for both `x86_64` and `arm64-v8a`.
- `verify-device-guards.ps1`: green over 19 surfaces.

## What did not change

No `.forge` field, section or version. No corpus fixture byte
(`git status testdata/` is empty). No `DATA_PACKAGE_SPEC.md` edit. No
`SculptHistory` storage, capacity or budget — `kMaxSculptHistoryEntries` (32),
`kMaxSculptHistoryBytes` (4 MiB) and `kMaxSculptHistoryEntryBytes` (1 MiB) are
untouched, and `E2E-SCHNAV-07` proves a navigator round trip encodes
byte-identically and records no Construction step.
