# UI-LAYOUT-R2 — correction round 1, measured against the OWNER AUTHORITY BLOCK

**Verdict: TECHNICAL PASS / OWNER VISUAL REVIEW REQUIRED.**
All 66 host rows conform, intra-cell drift is 0 dp in every cell, and the measured
defect this round exists for — Exact/Details translating the right host inward in
the short-landscape and expanded cells — is gone.

## Authority

Expected geometry came from the **immutable OWNER AUTHORITY BLOCK carried in the
correction prompt**, which is the coordinator's execution transmission of the
already OWNER-accepted `UI-SPEC-R0 Revision 1`. Per that block, no external
UI-SPEC file was required, searched for, created, copied or hash-checked, and the
provenance hash below is informational provenance rather than a filesystem gate.

| Item | Value |
| --- | --- |
| Authority source | the correction prompt's immutable OWNER AUTHORITY BLOCK |
| Coordinator-side provenance SHA-256 | `9d032cadc35497a1fb73b185e8889be6031d00b5b0c95fadbcd2642f2a063151` |
| Owner acceptance | 2026-08-29 — *"Zdam się na ciebie, spróbuj dopasować samemu wymiary, tylko trzymaj się tego layoutu, żeby był przy krawędziach."* |
| Repo HEAD measured | `6d05814ddb89ef4e06d2e78d62af78d8b2262af1` (measured build; the commit carries exactly the tree that was measured) |
| Device | `emulator-5580`, AVD identity confirmed `ForgeShape_Stage006` |

Binding geometry, transcribed verbatim into `measure.js` and `overlay.js`:

| Cell | Window / font | top | right | width |
|---|---|---:|---:|---:|
| C10 | 411×914 dp / 1.0 | 112 dp | 8 dp | 72 dp |
| C13 | 411×914 dp / 1.3 | 112 dp | 8 dp | 72 dp |
| L10 | 914×411 dp / 1.0 | 88 dp | 8 dp | 72 dp |
| L13 | 914×411 dp / 1.3 | 88 dp | 8 dp | 72 dp |
| E10 | 1280×800 dp / 1.0 | 108 dp | 8 dp | 72 dp |
| E13 | 1280×800 dp / 1.3 | 108 dp | 8 dp | 72 dp |

Tolerance is ≤4 dp independently per anchor. **No expected value is derived from
`dimens.xml`, `EditorControlStyles`, runtime bounds or a screenshot.** The
superseded 132–152 dp R0 host geometry is deliberately absent from this package:
it is no longer correction authority and is not drawn as if it were.

## What was wrong, and what changed

`EditorWorkspaceView.placeInspector` **appended** the precision surface to the
horizontal middle row, so in every window that seats it beside the model it became
a laid-out sibling *after* `WorkspaceTrailingHostView`. A laid-out sibling in a
horizontal row costs width, so the host was pushed inward by the panel plus its
margins — exactly the numbers the previous closeout measured:

```
L: 8 (host inset) + 300 (sideOverlayWidthDp 914) + 8 + 4 = 320 dp
E: 8 (host inset) + 340 (sideDockWidthDp  1280) + 8 + 4 = 360 dp
```

The panel is now seated at `middleRow.indexOfChild(trailingHost)`, so the host
stays the row's trailing child and its own `brush_gap` margin is the only term in
its trailing inset. The panel's right margin became `overlay_anchor_gap` — the
standoff between the two surfaces rather than an inset from the window edge.
`trailingLimitFor` was widened to clamp anchored surfaces at the leading edge of
*whichever* of the host and an open side panel comes first, so the
"never partially cover a live control" rule survives the reordering.

No dimension, breakpoint or width arithmetic was changed. The host's resting edge
density is untouched: it still measures 68 dp, which conforms at Δwidth = −4 dp,
and was deliberately **not** widened to make the number equal 72.

## Configuration method

Each cell is an exact `wm size` / `wm density` / `font_scale` triple, so window dp
is arithmetic rather than an approximation. Requested and actual are recorded per
cell in `configuration-matrix.csv`; all six matched.

| Cell | px | density | dp | font scale |
| --- | --- | ---: | --- | ---: |
| C10 | 822×1828 | 320 | 411×914 | 1.0 |
| C13 | 822×1828 | 320 | 411×914 | 1.3 |
| L10 | 1828×822 | 320 | 914×411 | 1.0 |
| L13 | 1828×822 | 320 | 914×411 | 1.3 |
| E10 | 1280×800 | 160 | 1280×800 | 1.0 |
| E13 | 1280×800 | 160 | 1280×800 | 1.3 |

Bounds are read from the live hierarchy through existing semantic ids
(`workspace_trailing_host`, `objects_capsule`, `history_group`, `viewport_surface`,
…). **No control is located by remembered screen coordinate**: every tap resolves
its target by id in the hierarchy captured immediately beforehand. Where a short
window makes the host taller than the window, the harness scrolls the host's own
internal container — which is the accepted way that window absorbs height
pressure — and then re-resolves the target by id.

## Results

### Right-host frame — 66/66 PASS, 0 dp intra-cell drift

| Cell | top | Δtop | right | Δright | width | Δwidth | drift | anchor |
|---|---:|---:|---:|---:|---:|---:|---|---|
| C10 | 112.5 | +0.5 | 8 | 0 | 68 | −4 | 0 dp | PASS |
| C13 | 112.5 | +0.5 | 8 | 0 | 68 | −4 | 0 dp | PASS |
| L10 | 88 | 0 | 8 | 0 | 68 | −4 | 0 dp | PASS |
| L13 | 88 | 0 | 8 | 0 | 68 | −4 | 0 dp | PASS |
| E10 | 107 | −1 | 8 | 0 | 68 | −4 | 0 dp | PASS |
| E13 | 107 | −1 | 8 | 0 | 68 | −4 | 0 dp | PASS |

Every one of the 11 states in a cell reports the identical top, right and width,
so the table above is the whole 66-row answer; `frame-measurements.csv` carries
each row individually. Height is content-driven under Revision 1 and is recorded
rather than gated — 162–235 dp in the short cells, 194–448 dp elsewhere.

### The corrected defect

Resting trailing inset is 8 dp in all six cells, and stays 8 dp through
`EXACT_OPEN`, `EXACT_IME` and `SCULPT_DETAILS` in all six. The 320 dp / 360 dp
translation is gone. See `analysis.txt` section A2.

### Resting restoration — Δ = 0 dp everywhere

`global_toolbar`, `objects_capsule`, `history_group`, `objects_section`,
`brush_edge_controls` and `viewport_surface` return to identical bounds after both
the Construction Exact and the Sculpt Details lifecycle, in all six cells. The
Construction history capsule is absent in every Sculpt state of every cell —
Sculpt still has no Construction Undo/Redo. See `analysis.txt` section B.

### Intrinsic hit areas — no violations

1299 control rows. **Zero intrinsic violations**: every user-operated control's
own box is ≥48×48 dp. 93 rows report a *short visible intersection* because a
scrolling container clipped the control at that offset; those are recorded in
their own `clipped` column and never counted as size failures. `hits.js` holds the
one rule both the harness and the analysis use, and each row says whether its
intrinsic box was resolved from its own cell (`CELL`) or, where the harness never
caught it unclipped in that cell, from the same control elsewhere in the run
(`RUN`) — five (cell, control) pairs, all listed in `analysis.txt` section C. The
unclipped in-process authority for the floor is the instrumented case
`UILR2C-11`, which reads each control's laid-out box directly.

### IME

Host top/right/width identical with the IME up in all six cells; the
`viewport_surface` is the whole window before and after in all six; host height is
allowed to shrink and does so in the compact cells (448 → 434.5 / 421 dp). See
`analysis.txt` section D.

### One external host in both modes

Construction and Sculpt report the same top/right/width in every cell — one
surface, not two grammars. See `analysis.txt` section E.

## Files

| File | Contents |
| --- | --- |
| `measure.js` | the device harness; expected geometry transcribed from the authority block |
| `hits.js` | the one intrinsic-vs-clipped rule, shared by the harness and the analysis |
| `analyse.js` | derives every verdict from `frames.json`; writes `analysis.txt` and `INDEX.md` |
| `overlay.js` / `pnglib.js` | mechanical overlays; every drawn rectangle comes from the authority block or a measured row |
| `frame-measurements.csv` | 66 host rows: cell/state/expected/measured/delta/verdict |
| `resting-bounds.csv` | before / during / after and Δ for the persistent binding set |
| `hit-areas.csv` | intrinsic bounds, the clipped visible intersection, and how intrinsic was resolved |
| `configuration-matrix.csv` | requested vs actual px / density / dp / font scale per cell |
| `frames.json` | the raw measured record every derived file is computed from |
| `analysis.txt` | derived verdicts, sections A–E |
| `INDEX.md` | owner review index: 36 raw screenshots beside 36 overlays |
| `commands.txt` | every adb command the harness issued, in order |
| `native-launch.txt` | a clean debug launch on the corrected build: thirteen `*_SELFTEST_OK`, 2043 checks, zero failures, `FORGESHAPE_NATIVE_VIEWPORT_OK` |
| `raw/` , `overlays/` | 36 + 36 images, six visual-critical states × six cells |

Visual-critical states: Construction Shape rest, Transform Move/World, Transform
Scale, Exact + IME, Sculpt rest, Sculpt Details.

## Scope of this package

Evidence and harness only. The product change is one commit of its own; nothing in
this directory is read by the app. This is a **technical** result: the owner /
coordinator visual verdict on UI-LAYOUT-R2 remains separate and is not claimed
here.
