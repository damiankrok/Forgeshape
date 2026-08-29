# UI-LAYOUT-R2 — measured closeout against the accepted UI-SPEC-R0

**Verdict: MEASURED FAIL / CORRECTION REQUIRED.**
This package is evidence only. No product code, resource or test was changed.

## Authority

| Item | Value |
| --- | --- |
| Accepted artifact | `D:\TRAVELAPPS\FORGESHAPE_UI_SPEC_R0_ACCEPTED.html` |
| Expected SHA-256 | `9f1e866795a2b2169273a7db62807e544747e673758598779a35fd25553dbe64` |
| Observed SHA-256 | `9f1e866795a2b2169273a7db62807e544747e673758598779a35fd25553dbe64` (match) |
| Local copy | `accepted-spec-copy.html`, byte-identical (same SHA-256) |
| Owner acceptance | 2026-08-29, quoted in the artifact's section 10: *"Owner accepted the proposed ForgeShape UI-SPEC-R0 visual direction and measurements on 2026-08-29 with the exact statement: Wygląda ok, zatwierdzam."* |
| Repo HEAD measured | `f4416e750b53004df3a7e6cccb038e32b9d8dd29` |
| Device | `emulator-5580`, AVD identity confirmed `ForgeShape_Stage006` |

Expected geometry is taken **only** from the accepted artifact. No expected value
was derived from `EditorControlStyles`, `dimens.xml`, runtime bounds or screenshots.

## Configuration method

Each cell is an exact `wm size` / `wm density` pair, so window dp is exact
arithmetic and not an approximation — see `configuration-matrix.csv`.

| Cell | px | density | dp | font scale |
| --- | --- | ---: | --- | ---: |
| C10 | 822×1828 | 320 | 411×914 | 1.0 |
| C13 | 822×1828 | 320 | 411×914 | 1.3 |
| L10 | 1828×822 | 320 | 914×411 | 1.0 |
| L13 | 1828×822 | 320 | 914×411 | 1.3 |
| E10 | 1280×800 | 160 | 1280×800 | 1.0 |
| E13 | 1280×800 | 160 | 1280×800 | 1.3 |

Bounds are read from the live hierarchy through existing semantic ids
(`workspace_trailing_host`, `objects_capsule`, `history_group`, …). No control is
located by remembered screen coordinate; every tap resolves its target by id in
the hierarchy captured immediately beforehand. Where a short window makes the
host taller than the window, the harness scrolls the host's own
`tool_rail_scroll` / `inspector_scroll` — the accepted spec's own rule that
"internal vertical scroll absorbs height pressure".

## Result summary

| Contract | Result |
| --- | --- |
| Absolute host anchor ≤ 4 dp | **FAIL — 0 of 66 rows pass** |
| Intra-cell drift 0 dp (top/right/width) | **FAIL in L10, L13, E10, E13** (host translates left when Exact/Details opens); PASS in C10, C13 |
| Resting-bounds Δ after close = 0 dp | PASS — every accepted section-4 element, all six cells |
| 48 dp intrinsic hit area | PASS — no control's largest measured hit area is below 48 dp |
| 48 dp at every scroll offset | 31 rows partially clipped in constrained-height states (see below) |
| IME keeps host top/right/width | PASS in all six cells |
| IME does not resize the Vulkan viewport | PASS — viewport stays full-window in all six cells |
| Sculpt shares the single external host | PASS in all six cells |
| Sculpt has no Construction history | PASS — absent in every Sculpt state and cell |

### Systematic anchor deltas (identical for all 11 states of a cell)

| Cell | Δtop | Δright | Δwidth |
| --- | ---: | ---: | ---: |
| C10 | +24.5 | −4 | −64 |
| C13 | +24.5 | −4 | −72 |
| L10 | +16 | −4 | −64 |
| L13 | +16 | −4 | −72 |
| E10 | +11 | −8 | −76 |
| E13 | +11 | −8 | −84 |

The right host measures **68 dp wide in every cell and state**. The accepted spec
requires 132/140/144/152 dp and varies width with font scale; the implementation
does not vary it at all.

### Intra-cell drift

In L10, L13, E10 and E13 the host's right inset changes from 8 dp to 320 dp (L)
or 360 dp (E) while Exact or Details is open, then returns to 8 dp on close. Top
and width never drift. Compact portrait shows no drift at all, which is why the
existing portrait-only `UILR2-06` did not detect this.

### Sub-48 dp rows

All 31 are partial visibility at a scroll offset, not intrinsic geometry: every
control listed (`precision_toggle`, `transform_mode_*`, `transform_space_local`,
`tool_rail_*`, `field_pos_*`) also measures ≥ 48×48 dp in other states of the
same run. See the tail of `analysis.txt`.

## Contents

| File | Content |
| --- | --- |
| `accepted-spec-copy.html` | byte-for-byte copy of the accepted artifact |
| `frame-measurements.csv` | all 66 cell×state right-host rows |
| `resting-bounds.csv` | 462 before/during/after rows for every persistent element |
| `hit-areas.csv` | 1299 measured user-operated controls |
| `configuration-matrix.csv` | requested vs actual px/density/dp/font scale |
| `analysis.txt` | derived verdicts A–E |
| `INDEX.md` | owner-review index of the 36 raw/overlay pairs |
| `raw/` | 36 device screenshots |
| `overlays/` | 36 overlays, drawn from the expected table and `frames.json` |
| `commands.txt` | every command run, serials preserved, restoration included |
| `measure.js`, `analyse.js`, `overlay.js`, `pnglib.js` | the harness, kept so the package is reproducible |

Earlier R2 runtime evidence in `artifacts/uilayoutr2/` is referenced, not
rewritten or deleted.

## Device state

Every `wm size`, `wm density` and `font_scale` override is reset by the harness's
`finally` block on success and failure paths alike, and was verified reset
afterwards: `Physical size: 1080x2400`, `Physical density: 420`, `font_scale 1.0`,
with no override lines.
