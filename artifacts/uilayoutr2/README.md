# UI-LAYOUT-R2 runtime evidence

Captured from the final debug APK on the isolated `ForgeShape_Stage006` AVD,
addressed only as `emulator-5580`. Every app interaction in
`capture-evidence.ps1` resolves the live view by stable resource id and taps the
centre of its current accessibility bounds.

## Screenshots

1. `01-construction-resting.png`
2. `02-shape-active.png`
3. `03-transform-active.png`
4. `04-move.png`
5. `05-rotate.png`
6. `06-scale.png`
7. `07-world.png`
8. `08-local.png`
9. `09-exact-transform-open.png`
10. `10-exact-transform-ime.png`
11. `11-exact-closed-restored.png`
12. `12-short-landscape.png` — 2400 × 1080 @ 420 dpi override
13. `13-expanded-tablet.png` — 1600 × 2560 @ 240 dpi override
14. `14-sculpt-resting.png`
15. `15-sculpt-details-open.png`
16. `16-sculpt-details-closed-restored.png`

## Walkthrough and records

- `uilayoutr2-walkthrough.mp4` — one continuous portrait walkthrough covering
  rest → Shape → Transform → Move → Rotate → Scale → World/Local → Exact →
  numeric field + IME → close → Objects/Add → Sculpt → all four brushes →
  Details → close. Container sanity found `ftyp`, `moov`, `mdat` and `avc1`.
- `R00-selftest-transcript.txt` — thirteen native suites, 2043 checks, viewport OK.
- `R01-validation-summary.txt` — build, JVM, instrumented, layout and guard totals.
- `capture-evidence.ps1` — guarded, repeatable semantic-ID capture procedure.

The screenshot set was visually inspected after capture. It demonstrates the
static right-host and bottom hide/restore contract, but visual acceptance remains
an owner/coordinator decision.
