# CAD-R2 — ACCEPTANCE MATRIX

Each row: what existed, what this stage added, where, the evidence, and the
verdict. Format impact is `none` everywhere (see `DESIGN.md` §0): no `CADB`
version, section, code, fixture or lineage rule moved; `CadStatus` (runtime
JNI vocabulary) gained two APPENDED codes. Evidence ids: host checks
(`TESTS.md`), JVM `R2-JVM-nn`, device `D01..D14` (`CadRetainedSketchOwnerTest`).

## P0 — retained sketch lifecycle and browser

| Item | Existed | Added (location) | Host | JVM | Device | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| P0.1 inventory | timeline sketch rows | `buildCadSketchInventory` (`forgeshape_cad_sketch_inventory.cpp`); JNI `cadSketchInventory`, `cadSketchConsumers` | R2_SK_01..04, 18 | 01..03, 12, 17 | D01, D13 | DELIVERED |
| P0.2 browser | History surface | Sketches segment, act strip (`FeatureHistoryView`, `SketchBrowserPresentation`, `SketchBrowserText`) | — | 04..06, 09..11, 13..15 | D01, D02, D12 | DELIVERED |
| P0.3 new retained sketch | face sketch session | `SketchSession::commitSketch` / `canKeepSketch`, Keep Sketch control (`WorkspaceTrailingHostView`) | R2_SK_05, 07 | — | D02, D03, D04 | DELIVERED for body-feature faces; **world plane: BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH** (reproducer R2_SK_06) |
| P0.4 edit exact sketch / feature | `beginEditFeature` by id | `beginEditSketch` (consumed → first consumer; unused → sketch-only), Edit Sketch / Edit Feature acts by durable id | R2_SK_08, 11, R2_HIST_01 | 07 | D05, D07, D08 | DELIVERED |
| P0.5 reuse | `appendCadLaterFeature` (unreached) | `beginReuseSketch`, Use for Add / Cut, Back to Sketch refused | R2_SK_09..13 | 04 | D05 | DELIVERED (Revolve Add/Cut refused by name, out of scope) |
| P0.6 safe delete | — | `removeUnusedCadSketch`, `deleteCadSketch`, JNI `cadDeleteSketch` | R2_SK_14..17 | 05 | D06, D13 | DELIVERED |
| P0.7 shared affordances | Edit Sketch chip, drawer New Sketch | every door reaches one native entry (table in `SUMMARY.md`); chip withdrawn during the chooser | — | — | D12 (chip absent in the chooser), D10 | DELIVERED |

## P1 — feature actions and history

| Item | Evidence | Verdict |
| --- | --- | --- |
| P1.1 action menu (Features segment: Edit Feature / Edit Sketch / dependencies) | JVM 07, 08; D07, D11 | DELIVERED |
| P1.2 exact parameter edit of a LATER Extrude; Revolve angle | R2_HIST_01..03; D07 (volume, bounds), D11 (212.5°, Undo) | DELIVERED |
| P1.3 first failure + NotRegenerated + Fix/Cancel | R2_HIST_04/05, R2_SK_12; D08 (issue card "Extrude 2 · Cut can no longer be rebuilt", Cancel byte- and fingerprint-identical) | DELIVERED |
| P1.4 states consistent, reads idempotent | R2_HIST_06; JVM 10, 15 | DELIVERED |
| P1.5 one transaction per act, preview/cancel record nothing | R2_SK_05, 08, 14, 15, 17, R2_DATA_02; D02, D06 | DELIVERED |

## P2 — interaction, regions, support

| Item | Evidence | Verdict |
| --- | --- | --- |
| P2.1 Select All / Clear, bounded, same evaluator | R2_PF_13/14; D14 | DELIVERED (D14 passed in union part B, run 40689211847) |
| P2.2 complex topology (grid 24/36, two components, point touch, pinched holes, permutation, crossing and tangent circles, nested loops, winding, spline) | R2_PF_01..12, R2_OR_12/13; D09 (22 cells by real taps, one solid, 11 m³ exact, save/reopen) | DELIVERED |
| P2.3 support lifecycle (Cut floor/wall yes, curved / partial Revolve no, stale refused) | R2_SUP_01..06; D04, D10 | DELIVERED |
| P2.4 hit ownership, 48 dp, portrait/landscape, stylus | JVM 13/14; D12 (both orientations, every act owns its centre, a stylus press) | DELIVERED (font scale: not exercised — see Known issues) |
| P2.5 drafting continuity | existing SKETCH_UX / CAD_FEATURE drafting checks unchanged; `CadSketchDraftingOwnerTest` in union part A | DELIVERED (regression evidence only) |
| P2.6 visible status | named refusals (`CadStatusMessages` 91/92), status lines; D01 detail text | DELIVERED |

## P3 — oracles and data

| Item | Evidence | Verdict |
| --- | --- | --- |
| P3.1 ≥ 12 independent oracles | 17 (`R2_OR_01..17`) | DELIVERED |
| P3.2 separate exact / chordal tolerances | `forgeshape_cad_oracle_selftest.h` contract; OR-09..11 | DELIVERED |
| P3.3 metamorphic (placement, permutation, Undo→Redo fingerprint, repeat regeneration, malformed JNI) | OR-16, R2_PF_06, R2_DATA_02, R2_PF_11; D13 | DELIVERED |
| P3.4 persistence; corpus unchanged | R2_DATA_01..04, OR-17; corpus parity in final CI FAST | DELIVERED |
| P3.5 repro capture | `CADR2_*` lines, `CAD_R2_ORACLE_PASS=17` | DELIVERED |

## P4 — performance, consistency, lifecycle

| Item | Evidence | Verdict |
| --- | --- | --- |
| P4.1 same-machine baseline vs candidate | `perf_same_machine.md` (11 interleaved runs each); `cad_r2_*` timings | DELIVERED with one recorded **performance risk** (see SUMMARY) |
| P4.2 shared-command consistency | door → native table in `SUMMARY.md` | DELIVERED |
| P4.3 lifecycle (rotation and Undo mid-choice, Cancel, reopen) | D10, D08, D02 | DELIVERED (Home→Resume not exercised by this class; existing lifecycle classes unchanged) |
| P4.4 scope | diff inventory in `SUMMARY.md` | DELIVERED |

## A1..A10

| Code | Verdict | Where |
| --- | --- | --- |
| A1 | DELIVERED | `buildCadSketchInventory`, `FeatureHistoryView` Sketches; R2_SK_01..04; JVM; D01 |
| A2 | DELIVERED (face supports); world plane BLOCKED-DESIGN-DECISION | `SketchSession::commitSketch`; R2_SK_05..07; D02, D03 |
| A3 | DELIVERED | `beginReuseSketch`; R2_SK_09..13; D05 incl. save/reopen |
| A4 | DELIVERED | `deleteCadSketch`; R2_SK_14..17; JVM 05; D06 |
| A5 | DELIVERED | `beginEditSketch`/`beginEditFeature`; R2_HIST_01..03; D07, D11 |
| A6 | DELIVERED | evaluation + issue card; R2_HIST_04/05, R2_SK_12; D08 |
| A7 | DELIVERED | R2_PF_*, R2_OR_12/13; D09 |
| A8 | DELIVERED | R2_SUP_*; D04, D10, D12 |
| A9 | DELIVERED | R2_DATA_*; corpus 72/72 (CI FAST 40694343402) |
| A10 | DELIVERED | 17 oracles, `perf_same_machine.md`, release guard PASS |
