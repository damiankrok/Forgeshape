# Owner findings — before and after, measured

- **Before:** CI DEVICE run `36329102110` on `2380623`, the unfixed product
  (4/4 reproduction tests; frames `before/01..06`).
- **After:** CI DEVICE run `36462677660` on the tested candidate `759ed91`,
  `CadVerticalSliceTest` plus five regression classes, 57/57 (frames
  `after/*`, facts `after/facts-759ed91.txt`). The earlier run `36333563367`
  on `e191dad` measured the same HUD numbers to the last digit.
- **Same device both times:** the CI emulator, 1080 × 2400 px, 2.625 density.

Two commits landed after `e191dad`: the first-region tilt (`5fe9841`, under
F5) and the evidence capture waiting for presented frames (`db8ff6b`,
`TEST_EVIDENCE.md` §3). Every capture in `after/` waited for six presented
frames.

| # | Owner finding | Before (measured) | After (measured) |
| --- | --- | --- | --- |
| F1 | The One Side / Symmetric / Two Sides selector is a huge horizontal block | Cluster 1027 × 171 px = **6.78 %** of the viewport, nearly full width; three 60.2 dp text pills | Cluster **196 × 52 dp = 2.70 %** (labels off) and 177 dp wide with labels on. The three choices live in a palette that is closed at rest. |
| F2 | Text-first instead of CAD icons | Every extent choice and the badge are text | Icon controls with a 48 × 48 dp hit and a **26.7 dp** glyph (24.4 dp in the palette). Distinct shapes for One Side, Symmetric and Two Sides, and for New Body, Add and Cut. |
| F3 | Value, extent and operation are detached from the arrow | Value **99.49 dp** from the shaft anchor | Value **0.18 dp** from the shaft anchor, and **0.15 dp** after an orbit |
| F4 | Sketch chrome stays up during Extrude | Navigator 5.71 %, rail 9.29 %, precision panel auto-opened at **28.79 %** | Navigator absent, drawing tools absent, precision panel collapsed. The trailing host shrinks to Cancel, Back to Sketch and the precision toggle (3.02 %). |
| F5 | Rectangle + centred circle: Finish silently picks the circle | 1 profile, the circle auto-chosen | **2 regions, none chosen**, no arrow, Extrude absent. A tap selects the rectangle-with-hole: the preview measures ring area × depth = 9.2023 m² × 1 m in one shell, and the extruded body is 4.6011 m³ at 0.5 m. A tap on the disk switches the preview to the cylinder; a tap back returns to the ring. The first tap also tilts the view, so the arrow can be dragged: its half-shaft measures **106.7 px** on screen. |
| F6 | Must choose the region, not accept an arbitrary loop | Not possible | Tap to toggle in the viewport, with rows in the precision panel. The hatch leaves the hole empty. |
| F7 | Must choose Add / Cut / New Body before commit | The badge read "New Body" and was not a control; Add and Cut existed nowhere | Operation badge plus palette. On a face sketch the offered bitmask is **7** (all three). A world plane offers New Body only, and Add is refused `OPERATION_NEEDS_TARGET`. |
| F8 | Add positive, Cut destructive, not by colour alone | — | Shapes: a box, a plus and a notch. Colours: success and error. The descriptions say "Operation: Cut…". Preview tints are green (Add), red (Cut) and blue (New Body). |
| F9 | Preview before commit | Overlay lines only | The renderer draws the candidate mesh the commit will apply. An invalid one is named: "Cannot extrude: Cut misses the body — nothing would be removed." |
| F10 | Add/Cut must change the SAME body | A face sketch made body 3 beside body 2 | Add returns the **same id** with the body count unchanged and volume **4 → 4.32**; Cut gives **4 → 3.68**. One Undo each, and Undo/Redo restore exactly. |
| F11 | Features must stay editable (retained CAD) | One sketch + one extrusion per body | A feature list with two rows; row 2 reopens the Add in place. Re-extruding at 0.25 m is one step. An upstream base edit (1 → 1.5 m) carries the Add: **6.16 m³**, top at 1.75 m. A save/reopen round trip is byte-identical. |

## Frames to compare

- **HUD:** `before/01_ready_one_side` ↔ `after/04_compact_hud_one_side`
- **Region:** `before/04_rectangle_circle_after_finish` ↔
  `after/01_rectangle_circle_two_regions` and `after/02_ring_selected_hatched`
- **Same-body Add:** `before/06_after_second_extrude` ↔ `after/09_add_committed`
- **Cut:** `after/11_cut_committed`
- **Tool Labels:** `after/07_compact_hud_labels_on`
- **Ready chrome:** `after/15_ready_chrome`
