# UI-LAYOUT-R2 — owner visual review bundle

Packaging only. **No verdict is recorded here**, no runtime evidence was captured
for it, and no product, test or documentation file was touched to build it.

## What this is

Six representative cell × state pairs pulled out of
[`../uilayoutr2-correction/`](../uilayoutr2-correction/), each showing the shipped
frame beside the same frame with the measurement drawn on, so the corrected
edge-host composition can be judged without opening all 72 PNGs one at a time.

| Open this | For |
| --- | --- |
| [`OWNER_REVIEW.html`](OWNER_REVIEW.html) | the full-size pairs, one screen each, with the numbers and one review question per pair. Opens by double-clicking — local relative paths, no server, no network, no fonts or scripts fetched. |
| [`OWNER_REVIEW_CONTACT_SHEET.png`](OWNER_REVIEW_CONTACT_SHEET.png) | all twelve images on one sheet, for a first glance or for pasting into a message. |

Click any image in the HTML page to open that PNG at full resolution.

## The six pairs

Deltas are the recorded ones, read from `frame-measurements.csv`.

| # | Cell | State | Window | Δtop | Δright | Δwidth | Anchor |
|---|---|---|---|---:|---:|---:|---|
| 1 | C10 | TRANSFORM MOVE WORLD | 411×914 dp, font 1.0 | +0.5 | 0 | −4 | PASS |
| 2 | C10 | EXACT IME | 411×914 dp, font 1.0 | +0.5 | 0 | −4 | PASS |
| 3 | L10 | EXACT IME | 914×411 dp, font 1.0 | 0 | 0 | −4 | PASS |
| 4 | L13 | SCULPT DETAILS | 914×411 dp, font 1.3 | 0 | 0 | −4 | PASS |
| 5 | E10 | EXACT IME | 1280×800 dp, font 1.0 | −1 | 0 | −4 | PASS |
| 6 | E13 | SCULPT DETAILS | 1280×800 dp, font 1.3 | −1 | 0 | −4 | PASS |

Height is content-driven under Revision 1 and is shown for information only; it is
not an anchor. `Δwidth = −4` is the host measuring 68 dp against an accepted
72 dp — inside tolerance, and deliberately not widened to make the number equal.

## What is being asked

The measurement already says the frames conform. What it cannot answer is
whether the result *looks* right. For each pair:

1. Does the rail still read as a **narrow strip hugging the window edge**?
2. Does Exact / Details sit **inboard, to the left of the rail**, with the rail
   unmoved?
3. Is there any **visible overlap, jump, detached control grammar, or viewport
   lost for no reason**?

Pairs 3–6 are the ones the correction changed: before it, the panel was laid out
after the rail in the same row and pushed the rail off the window edge — about
320 dp in a short landscape window, about 360 dp on a tablet — for as long as
Exact or Details was open.

## Provenance

- Every image is **referenced in place** from `../uilayoutr2-correction/`. Nothing
  was copied, re-encoded, cropped or retouched, and the source package was not
  modified; the contact sheet is the only new pixels, and it is a downscaled
  composite of those same twelve files.
- Every number on both outputs is **read** from `frame-measurements.csv` and
  `configuration-matrix.csv` in that package. None was measured off an image.
- Filenames were resolved through the package's own
  [`INDEX.md`](../uilayoutr2-correction/INDEX.md).
- The contact sheet was composed with the repository's existing
  `../uilayoutr2-correction/pnglib.js`, read-only. No tool or dependency was
  installed, no device was contacted and no screenshot was retaken.

## After the review

The owner / coordinator gives UI-LAYOUT-R2 a visual PASS, or concrete rejection
notes naming the pair and the defect. One correction round of the maximum two
remains available if a defect is named.
