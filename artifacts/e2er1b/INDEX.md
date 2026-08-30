# E2E-R1B / Stage 022 — evidence package

Produced on the ForgeShape-owned isolated AVD `ForgeShape_Stage006` /
`emulator-5580`, whose identity was confirmed with
`adb -s emulator-5580 emu avd name` before any evidence-sensitive command. The
reserved `emulator-5554` was never contacted, and every device command was
scoped with an explicit `-s`.

This package records what was measured. It records no verdict.

| File | What it is |
| --- | --- |
| `native-launch.txt` | A clean debug launch: fifteen `*_SELFTEST_OK` tokens totalling 2199 checks, then `FORGESHAPE_NATIVE_VIEWPORT_OK`. The project suite is 132 checks (`FSR1A-01..15` plus `FSR1B-01/02/13`); the new render-recovery suite is 24 (`FSR1B-16`). The golden `.forge` digests are unchanged from E2E-R1A, which is the R1A-compatibility gate. |
| `diagnostic-report-sample.txt` | A **real** report, pulled from app-private storage with `run-as`. 725 bytes. It doubles as the autosave/recovery lifecycle transcript: `ACTIVITY_STOP` → `AUTOSAVE_CHECKPOINT` → `ACTIVITY_CREATE` → `CANDIDATE_OFFERED`. Contains build and coarse device facts and event tokens — and no dimension, no `.forge` magic, no path and no `Uri`. |
| `focused-fsr1b.txt` | `ProjectAutosaveRecoveryTest`, `ProjectTransferTest` and `DiagnosticsAndRendererLossTest` run together: `FSR1B-01..18` and `E2ER1B-03..08`. |
| `process-death-e2e.txt` | `scripts\run-project-persistence-e2e.ps1`: eight stages, four real process deaths, each with the absent-process check between the halves. Stages 5-8 are `E2ER1B-01/02` — work that was never saved. |
| `regression-focused.txt` | `EditorWorkspaceProjectActionsTest` (E2E-R1A), plus the UI-LAYOUT-R2 right-host, placement, correction and legibility suites, run together. |
| `device-guards.txt` | `DEV2-01..07` and `DEV3-01..06`. |
| `full-sharded.txt` | The authoritative exhaustive-sharded instrumented aggregate. |

## Which device-loss branch is implemented

**The rebuild branch**, not the fail-closed one. The transcript in
`native-launch.txt`'s sibling logcat during the focused run shows the full
sequence:

```
FORGESHAPE_RENDER_DEVICE_LOSS_INJECTED
FORGESHAPE_RENDER_DEVICE_LOST:acquire attempt=1
FORGESHAPE_RENDER_DEVICE_REBUILT:attempt=1 completed=1
FORGESHAPE_NATIVE_VIEWPORT_OK
```

— a lost device at `vkAcquireNextImageKHR`, a complete teardown of the device
and everything on it, a rebuild, and the viewport presenting again about 130 ms
later. The bounded `RestartRequired` branch exists behind it (two attempts, then
stop) and is covered by the native policy suite and by the instrumented case's
other arm.

## What is NOT here, and why

**No screenshots.** E2E-R1B added three rows to an existing surface and one
question; the visual arrangement it stands on was accepted at UI-LAYOUT-R2 and
its measurement is unchanged and unrepeated. The claim made here is that the
arrangement did not move, and the right-host regression is the mechanical form
of it.

**No tap-through of the system document picker.** That UI belongs to another app
and differs per device. What is tested instead is the production result handlers
end to end against a real `ContentResolver`, plus the exact Intents that would be
sent. "SAF works" would be a bigger claim than this evidence makes.

**No real GPU device loss.** Provoking one would mean destabilising the
authoritative emulator's GPU, which the repository forbids. The injection seam
enters the recovery path at exactly the point a real `VK_ERROR_DEVICE_LOST`
would and runs every line after it; driver behaviour after a genuine loss is not
covered and is not claimed.
