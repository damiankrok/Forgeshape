# UI-ARCH-R1 evidence

Baseline: `717cf70ef085bb36007195757bc9367aa559e793` (`master`, clean at capture).

Implementation commits:

- `dbd2298` — structural product extraction.
- `6502d6d` — focused tests and current-truth documentation.

This directory records the bounded right-cluster ownership refactor. The stage
adds no UI-LAYOUT-R2 behavior. `before/` was captured from an APK rebuilt from
the exact baseline SHA; `after/` was captured from the final refactored source.
Both captures used only `ForgeShape_Stage006` on `emulator-5580`, after verifying
the AVD name.

Contents:

- `before/` and `after/`: eleven matched PNGs and a 175-row semantic bounds CSV.
- `capture-evidence.ps1`: reproducible stable-id capture harness.
- `geometry-equivalence.md`: state-by-state right-cluster bounds and comparison result.
- `source-ownership.md`: before/after ownership tree and responsibility boundary.
- `source-delta.md`: root complexity and test-seam reduction.
- `source-diff-summary.md`: exact responsibility moved and retained.
- `test-summary.md`: builds, native/JVM/instrumented/profile/device-guard results.

The new `workspace_trailing_host` row is absent in the baseline by definition.
Across the remaining 164 comparable rows, presence, bounds, enabled, selected
and checked values are identical.
