# UI-LAYOUT-R1 runtime evidence

Captured from the final debug APK on the isolated `ForgeShape_UILAYOUTR1` AVD,
addressed only as `emulator-5580`. Every app interaction in
`capture-evidence.ps1` resolves the live view by stable resource id and taps the
centre of its current accessibility bounds.

## Screenshots

1. `01-construction-resting.png`
2. `02-construction-move.png`
3. `03-construction-rotate.png`
4. `04-construction-scale.png`
5. `05-exact-transform.png`
6. `06-exact-transform-ime.png`
7. `07-add-primitive.png`
8. `08-objects.png`
9. `09-display.png`
10. `10-short-landscape.png` — 2400 × 1080 override
11. `11-expanded-tablet.png` — 1600 × 2560 @ 240 dpi override
12. `12-sculpt-resting.png` — captured after the five-second transient expired
13. `13-sculpt-details.png`
14. `14-sculpt-objects.png`

## Walkthrough and records

- `uilayoutr1-walkthrough.mp4` — one continuous Construction → transforms →
  Exact+IME → Add → Objects → Display → Sculpt → Details → Objects workflow.
  Size: 5,138,202 bytes. Container sanity check found `ftyp`, `moov`, `mdat` and
  `avc1` markers.
- `R00-selftest-transcript.txt` — thirteen native suites, 2043 checks, viewport OK.
- `R01-validation-summary.txt` — build, JVM, instrumented, expanded and guard totals.
- `capture-evidence.ps1` — guarded, repeatable capture procedure.

The screenshot set was visually inspected after capture. Visual acceptance is
still an owner/coordinator decision; these files do not self-approve the look.
