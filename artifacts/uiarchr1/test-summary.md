# Verification summary

Target: isolated `ForgeShape_Stage006`, serial `emulator-5580`. Reserved
`emulator-5554` was never contacted.

- Build: `assembleDebug`, `assembleDebugAndroidTest`, `testDebugUnitTest` PASS.
- Native ABIs: `arm64-v8a` and `x86_64` both configured and built.
- Native clean launch: 13 `SELFTEST_OK` suites, 2043 checks, zero fail tokens,
  followed by `FORGESHAPE_NATIVE_VIEWPORT_OK`.
- JVM: 59/59, zero failures/errors/skips.
- UI-ARCH-R1: `UIAR1-01..12`, 12/12.
- Final architecture + layout focused pair: 22/22.
- Full instrumented inventory: 300/300, zero failures, through five supported
  `run-instrumented-tests.ps1 -TestClass` shards: 57 + 61 + 72 + 58 + 52.
- Short landscape: dedicated contract 4/4 at 2400 × 1080 @ 420 dpi.
- Expanded/tablet: dedicated contract 4/4 at 1600 × 2560 @ 240 dpi.
- Device isolation: DEV2-01..07 and DEV3-01..06 all PASS.
- Evidence: eleven before/after pairs; 164 comparable semantic rows, zero deltas.

Two monolithic full-suite attempts were discarded after the Android emulator
system accumulated state, entered kernel wait and emitted watchdog/ANR
diagnostics. Neither produced an assertion failure or application exception.
Reinstalling between five exhaustive class shards kept the same official runner
and covered every one of the 300 tests without reproducing the system freeze.

A separate 64-test general layout subset was deliberately not counted under a
global landscape override: portrait-contract `UILR1-05` then observes the
intentional short-window selector reflow. The four tests explicitly authored for
the short-landscape profile passed 4/4 on the same physical override.
