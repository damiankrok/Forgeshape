# E2E-R1A / Stage 021 — evidence package

Everything here was produced on the ForgeShape-owned isolated AVD
`ForgeShape_Stage006` / `emulator-5580`, whose identity was confirmed with
`adb -s emulator-5580 emu avd name` before any evidence-sensitive command. The
reserved `emulator-5554` was never contacted, and every device command was
scoped with an explicit `-s`.

This package records what was measured. It records no verdict.

| File | What it is |
| --- | --- |
| `native-launch.txt` | A clean debug launch: fourteen `*_SELFTEST_OK` tokens totalling 2163 checks, `FORGESHAPE_PROJECT_GOLDEN_SHA256`, then `FORGESHAPE_NATIVE_VIEWPORT_OK`. The project suite is `FSR1A-01..15`, 120 checks. |
| `corpus-digests.txt` | The seven committed `.forge` v1 fixtures with their sizes and SHA-256 digests, re-verified from disk by `scripts\build-forge-corpus.ps1 -VerifyOnly`. The two canonical digests match the ones the launch log prints, which is the cross-implementation agreement between the C++ encoder and the independent PowerShell one. |
| `abi-and-alignment.txt` | Both supported ABIs present in the release APK, and the first `LOAD` segment of each of the four native binaries (debug and release, both ABIs) showing `p_align 0x4000` — 16 KB. |
| `focused-project-actions.txt` | `EditorWorkspaceProjectActionsTest`: `E2ER1A-01`, `-04`, `-05`, `-06`. |
| `process-death-e2e.txt` | `scripts\run-project-persistence-e2e.ps1`: `E2ER1A-02` and `-03` across a real process death, with the absent-process check between each pair of stages. |
| `right-host-regression.txt` | `EditorWorkspaceUnifiedRightHostTest`, `EditorWorkspaceRightHostPlacementTest` and `EditorWorkspaceCorrectionTest` re-run together, to show the accepted UI-LAYOUT-R2 arrangement did not move. |
| `full-sharded.txt` | The authoritative exhaustive-sharded instrumented aggregate: live discovery, the deterministic exactly-once partition, every shard's result, and the aggregate marker. |

## What is NOT here, and why

No screenshots. E2E-R1A changed one icon control and added one small anchored
surface; the visual arrangement it stands on was accepted at UI-LAYOUT-R2 and
its measurement is unchanged and unrepeated — `artifacts/uilayoutr2-correction/`
remains the current record of that. The claim made here is that the arrangement
did not move, and `right-host-regression.txt` is the mechanical form of it.

No two-device transfer. The portability claim is made at the level the stage
authorized: a fixed-width, file-owned schema, deterministic golden bytes, a
platform-neutral codec, both supported ABIs, and an independent second
implementation that produces the same bytes. No physical device-to-device UX
test was performed, and none is claimed.
