# Stage 025 (`SCULPT-FCM-R1`) — evidence index

Task: **Stage 025** (prompt revision R1) — Flatten, Crease and Mask, so the MVP
sculpt set is exactly seven brushes.

Baseline HEAD: `ee98c9898a3cd1f009b606e3ffdf3392ba02de31` (`SCULPT-H1`), clean
worktree at start, no remote configured.

Device: `emulator-5580`, confirmed `ForgeShape_Stage006` by
`adb -s emulator-5580 emu avd name` before use. `emulator-5554` was never
contacted — it was not even attached (`adb devices -l` listed one target).

| File | What it holds |
| --- | --- |
| `OWNER_LATER_TEST_PACK.md` | what the owner still has to judge — the three tools, the overlay, the rail, and the two feel constants put to them |
| `STARTUP_SELFTEST_LOG.txt` | the debug launch: 22 `*_SELFTEST_OK` tokens (3496 checks), 0 failures, `FORGESHAPE_NATIVE_VIEWPORT_OK`, and all 16 golden `.forge` fixture digests |
| `FOCUSED_STAGE025_RUN.txt` | `SculptBrushStage025Test` — **OK (8 tests)**, 55.25 s |
| `FOCUSED_REGRESSION_RUN.txt` | `SculptUndoTest` + `SculptHistoryNavigatorTest` + `ImportedMeshSculptTest` + `EditorWorkspaceControlsTest` |
| `NATIVE_RUNNER_FCM.txt` | the 94 `FCM-*` checks and the 15 `fcm_20` metric checks, green on the standalone NDK runner |

## What was run, and what was not

**Run under `TEST-OWNER-03` / TEST POLICY v3, the reduced-testing policy.**
`-FullSharded` was **NOT run** and **no `FULL_SHARDED_SUITE_PASS` is claimed**.
This is focused evidence and cannot stand in for the exhaustive gate.

- Native domain: `FCM-01..19` (94 checks) plus `fcm_20` (15 checks, produced by
  the EXISTING Stage 020R3 metric blocks looping over `kSculptToolCount`), green
  on the standalone NDK runner and again inside the app launch. The sculpt suite
  went 533 → **655** checks; every other suite's count is unchanged.
- Device: the 8 Stage 025 cases above, plus the four-suite regression quad.
- Build: `assembleDebug` and `assembleRelease` both successful.
  `llvm-readelf --dyn-syms | grep -ci selftest` = **0** on the release
  `libforgeshape_native.so` for both `x86_64` and `arm64-v8a` (F-16).
- `verify-device-guards.ps1`: green over 19 surfaces
  (`DEV2-01..07`, `DEV3-01..06`).

## What did not change

No `.forge` field, section or version. No corpus fixture byte
(`git status testdata/` is empty), and all sixteen golden digests printed at
launch are byte-for-byte what `SCULPT-H1` printed. No `DATA_PACKAGE_SPEC.md`
edit. No `SculptHistory` capacity or budget — `kMaxSculptHistoryEntries` (32),
`kMaxSculptHistoryBytes` (4 MiB) and `kMaxSculptHistoryEntryBytes` (1 MiB) are
untouched, and `FCM-19` asserts each of the three by value. No second history
stack. No topology mutation: `SculptMesh` still offers exactly one mutation
beyond the mask, and it is a vertex POSITION.

## What DID change outside Sculpt, and why

Three things, all of them the minimum the mask overlay needs:

1. **`MeshVertex` gained `float mask`** — a derived presentation channel on the
   same terms as the `color` beside it. Every other producer leaves it at zero
   by value-initialisation; no publication signature changed.
2. **`RenderVertex` gained `float mask`**, copied verbatim from the source
   vertex in `buildRenderMesh` and clamped there, once.
3. **The surface pipeline gained a fourth vertex attribute** (location 3,
   `VK_FORMAT_R32_SFLOAT`) and the two shaders one line each. The outline-mask
   pipeline binds position at offset 0 with `sizeof(RenderVertex)` as its
   stride, so it needed no change at all.

## The one existing assertion that moved

`ImportedMeshSculptTest`'s `IMP01B-24` asserted "the brush set is still exactly
four tools". It now asserts **seven**, and its comment says that the number
moving is what a stage costs. That is the only pre-existing test line this stage
changed.

## Timing

Automated verification: the standalone native loop in seconds per iteration,
`assembleDebug` and `assembleRelease` a few minutes each, the focused Stage 025
suite 55.25 s of instrumentation, the regression quad under three minutes of
instrumentation. Inside the TEST POLICY v3 20-minute target for the verification
itself; the wall clock includes three iterations of the focused suite, two of
which failed on the test's own setup rather than on the product — see the report.
