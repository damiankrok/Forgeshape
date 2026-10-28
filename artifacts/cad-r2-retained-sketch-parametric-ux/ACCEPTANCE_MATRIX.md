# CAD-R2 — ACCEPTANCE MATRIX

Phase-0 plan. The **Verdict** column is filled in at the end of each phase
from evidence in `TESTS.md`; nothing here is a verdict until that column says
so.

Legend — Format impact: `none` everywhere (see `DESIGN.md` §0).

## P0 — retained sketch lifecycle and browser

| Item | Already implemented | Gap | Location | Risk | Host | JVM | DEVICE | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| P0.1 inventory | timeline lists sketches incl. Unused | consumers, support kind, permitted acts | `forgeshape_cad_sketch_inventory.*`, JNI `cadSketchInventory` | low | R2_SK_01..04 | SBP rows | D01 | pending |
| P0.2 browser | History surface | Sketches segment, act strip | `FeatureHistoryView`, `SketchBrowserPresentation` | med (touch) | — | SBP | D01 | pending |
| P0.3 new retained sketch | face sketch session | Keep Sketch commit | `SketchSession::commitSketch` | med | R2_SK_05..07 | SBP acts | D02, D03 | pending (world plane: BLOCKED-DESIGN-DECISION) |
| P0.4 edit exact sketch / feature | `beginEditFeature` by id | unused-sketch edit, act by sketch id | `SketchSession::beginEditSketch` | med | R2_SK_08, R2_HIST_01 | — | D05, D07 | pending |
| P0.5 reuse | `appendCadLaterFeature` | session, JNI, UI | `SketchSession::beginReuseSketch` | med | R2_SK_09..12 | SBP | D05 | pending |
| P0.6 safe delete | — | everything | `removeUnusedCadSketch`, `deleteCadSketch` | low | R2_SK_13..17 | SBP | D06 | pending |
| P0.7 shared affordances | Edit Sketch chip, drawer New Sketch | one path per act | `EditorWorkspaceView` | low | — | SBP | D03, D12 | pending |

## P1 — feature actions and history

| Item | Already | Gap | Location | Host | JVM | DEVICE | Verdict |
| --- | --- | --- | --- | --- | --- | --- | --- |
| P1.1 action menu | row tap = one fixed act | Features segment: Edit Feature / Edit Sketch / Dependencies | `FeatureHistoryView` | — | SBP feature acts | D07 | pending |
| P1.2 exact parameter edit | `beginEditFeature(startReady)` | evidence for later feature typed values | existing | R2_HIST_02..03 | — | D07, D11 | pending |
| P1.3 dependency feedback | first failure + issue card | evidence through reuse/shared paths | existing | R2_HIST_04..05 | existing | D08 | pending |
| P1.4 states | timeline states | sketch row states agree with timeline | inventory | R2_HIST_06 | SBP | — | pending |
| P1.5 undo boundaries | ScopedConstructionEdit | new acts each one step | new commits | R2_DATA_* | — | D02, D06 | pending |

## P2 — interaction, regions, support

| Item | Already | Gap | Host | DEVICE | Verdict |
| --- | --- | --- | --- | --- | --- |
| P2.1 region selection | set toggle, 9216 bound | Clear / Select all (bounded, same evaluator) | R2_PF_01..03 | D09 | pending |
| P2.2 complex topology | point-touch split | re-check crossings, 20+ cells, two components, permutation invariance | R2_PF_04..08 | D09 | pending |
| P2.3 support lifecycle | per-face eligibility, stale re-validation | evidence | R2_SUP_01..05 | D04, D10 | pending |
| P2.4 hit ownership | DEV-OSS-03 fix | browser strip vs viewport | — | D12 | pending |
| P2.5 drafting continuity | toolkit | regression only | existing suites | existing | pending |
| P2.6 visible status | status line | Keep Sketch / reuse refusals named | — | D05 | pending |

## P3 — oracles and data

| Item | Gap | Host | Verdict |
| --- | --- | --- | --- |
| P3.1 ≥ 12 independent oracles | everything | R2_OR_01..12+ | pending |
| P3.2 tolerances | exact vs chordal separated | R2_OR_* | pending |
| P3.3 metamorphic | translation/rotation, permutation, undo/redo fingerprint, repeat regen | R2_OR_*, R2_PF_* | pending |
| P3.4 persistence | new states round-trip; corpus unchanged | R2_DATA_*, CI FAST parity | pending |
| P3.5 repro capture | `CAD_R2_ORACLE_PASS=<N>`, `CADR2_*` ids | host log | pending |

## P4 — performance, consistency, lifecycle

| Item | Plan | Evidence | Verdict |
| --- | --- | --- | --- |
| P4.1 measurement | `cad_r2_*` timings in `FORGESHAPE_CAD_FEATURE_PERFORMANCE` baseline vs candidate | host log | pending |
| P4.2 shared commands | every door → one native entry | code review table in SUMMARY | pending |
| P4.3 lifecycle | rotation / Back / Undo during chooser | D10, D12 | pending |
| P4.4 scope | CAD + bridge + UI + tests only | diff inventory | pending |

## A1..A10

| Code | Evidence planned | Verdict |
| --- | --- | --- |
| A1 | R2_SK_01..04 + SBP + D01 | pending |
| A2 | R2_SK_05..07 + D02/D03 | pending |
| A3 | R2_SK_09..12 + D05 + save/reopen | pending |
| A4 | R2_SK_13..17 + SBP + D06 | pending |
| A5 | R2_HIST_01..03 + D07/D11 | pending |
| A6 | R2_HIST_04..05 + D08 | pending |
| A7 | R2_PF_* + R2_OR_* + D09 | pending |
| A8 | R2_SUP_* + D04/D10/D12 | pending |
| A9 | corpus parity + R2_DATA_* | pending |
| A10 | R2_OR_* (≥ 12) + perf + release guard | pending |
