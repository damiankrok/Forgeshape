# Alias counts — what 37, 38 and 23 count, and how many defects there are

This is part of FUNCTION-COUNCIL-R2-CLOSEOUT-R1, an audit-only closeout. Nothing in the product is changed here.

| | |
| --- | --- |
| Map | `artifacts/function-council-r2-closeout/ALIAS_TO_ROOT_CAUSE.tsv` (251 alias rows) |
| Generator | `diagnostics/ALIAS/build_alias_map.py` (writes the map and asserts that it agrees with the dossier's TSV) |
| Counter | `diagnostics/ALIAS/count_aliases.py` |
| Recorded output | `diagnostics/ALIAS/count_aliases.out` |
| Commands and exit codes | `diagnostics/ALIAS/CMD.txt` |

## How to reproduce the counts

```
cd /home/user/Forgeshape
python3 -I artifacts/function-council-r2-closeout/diagnostics/ALIAS/build_alias_map.py /home/user/Forgeshape   # exit 0
python3 -I artifacts/function-council-r2-closeout/diagnostics/ALIAS/count_aliases.py  /home/user/Forgeshape   # exit 0
```

All numbers below are copied from `count_aliases.out`. None are hand-tallied.

## The four counting units are different things

The previous dossier (FUNCTION-COUNCIL-R2) gave three numbers: 37, 38 and 23. They are not interchangeable. Each counts a different unit, so none can be derived from another.

| Unit | What one item is | Count | Source |
| --- | --- | --- | --- |
| **A. DEFECT-CONFIRMED requirement rows** | One expected behaviour that the synthesis graded as broken. | **37** (P1 2, P2 17, P3 18) | `requirements_traceability.tsv`, column `verdict` |
| **B. P0–P2 seat findings** | One heading in a seat file, at the seat's own grade, before Round 2 regrading. | **38** (30 panel-verified + 8 not panel-verified) | `work/VERIFICATION.md` |
| **B′. Panel-verified root causes** | One of the 30 panel rows, after removing the dossier's own 7 duplicate ids. | **23** | `work/VERIFICATION.md` |
| **C. Mechanism** (introduced by this closeout) | One faulty piece of logic at one code site, needing its own fix. | see below | `root_cause_id` in the map |

The 7 duplicate ids removed to get from 30 to 23 are X-A01=A-01, E-01=B-02, C-02=D-02=E-03=A-04, F-02=C-01 and E-04=A-05.

### Why 37 ≠ 38

- **Unit A excludes some of unit B's findings.** Several P2 findings were never graded DEFECT-CONFIRMED:
  - A-03 and A-07 are BLOCKED-OWNER.
  - C-06, G-02, I-05, I-06 and I-08 are PARTIAL (gaps in coverage or CI).
  - H-05, H-06 and H-07 are risks with no recorded budget.
  - I-02 is PARTIAL.
  - I-03 and B-03 are DOC-DIVERGENCE.
- **Unit A includes 18 P3 rows.** P3 is outside the P0–P2 band, so these are not in unit B.
- **Unit A already merges duplicates.** Cross-seat duplicates were folded into one row through `merged_duplicates`. Unit B still lists each duplicate under its own seat id.
- **One mechanism can break several requirements.** It then appears as several rows:
  - RC-20 (the 48 dp width) is 4 rows: UI-011..UI-014.
  - RC-08a (the bootstrap Back order) is 2 rows: UI-009 and UI-024.

### Why 23 ≠ 37

- 23 covers only the panel-verified P0–P2 findings.
- It excludes every P3 and the 8 P2 findings that were not panel-verified.
- It includes non-defects: A-03 and A-07 (OWNER holds), B-03 (a doc divergence), C-06, G-01 and G-02 (test or CI issues), and H-05 and H-06 (performance risks).

**Overlap at the mechanism level** (computed):

- 16 mechanisms are shared by the 23 roots and the 37 rows.
- 7 mechanisms are only in the 23: RC-01c, RC-06c, RC-06d, RC-13b, RC-14a, RC-15a, RC-15b.
- 17 mechanisms are only in the 37: RC-05a/b/c, RC-07a/b/d, RC-10b, RC-13c/d/e, RC-16a/b, RC-18a, RC-NEW-01/03/12/30.

## Counts per unit

### Unit A: the 37 DEFECT-CONFIRMED rows

- **33 distinct mechanisms**, under **20 parent RC numbers**.
- Rows by category of the mechanism: BUG 29, DESIGN_HOLD 3, TEST_HARNESS_DEFECT 5.
- The 3 DESIGN_HOLD rows are:
  - APP-057 (A-02, Recover semantics, OD-05);
  - UI-009 and UI-024 (F-03, bootstrap Back, OD-08 / disagreement D-5).
- The TSV calls all three DEFECT-CONFIRMED, but the written sources conflict, so this map holds them for the OWNER.

### Unit B: the 38 P0–P2 seat findings

- **29 distinct mechanisms**, under **14 parent RCs**.
- By category: BUG 20, TEST_HARNESS_DEFECT 8, DESIGN_HOLD 4, PERFORMANCE_RISK 4, DOC_DIVERGENCE 2.
- The 23 panel roots map one-to-one onto 23 distinct mechanisms. The closeout's split therefore reproduces the dossier's de-duplication exactly.

### Unit C: every seat finding, all severities

There are 152 seat-finding alias rows:

| Status | Rows |
| --- | --- |
| current | 92 |
| narrowed | 13 |
| hypothesis | 40 |
| withdrawn | 7 |

- The 7 withdrawn rows are B-07, the C-03 Keep Sketch sub-claim, the J-05 revolve-axis half, H-B1, H-B2, and the never-assigned ids H-09 and H-12.
- There are 112 non-hypothesis findings. 74 of them are P3, INFO or ungraded, and so lie outside the 38.

### The other alias kinds

| Kind | Rows |
| --- | --- |
| requirement rows | 37 |
| `opp` rows (OPP-01..45, OPP-G1..G3) | 48 |
| `backlog_row` rows | 14 |

The 14 backlog rows are the three backlog entries without an OPP id (Class 1 ranks 17, 18 and 19) plus IDEA-01..11. Every other backlog row is keyed by an OPP id, which is already in the map.

## Root-cause ids

- **RC-01..RC-21 keep the dossier's meaning** (`COUNCIL_DELIBERATION.md` §4). Where one dossier RC groups mechanisms that need different fixes at different code sites, this closeout adds a letter. For example:
  - RC-13 becomes RC-13a..h: FP-fragile oracles, no host suites in CI, uninitialised reads, log truncation, result cap, per-start re-run, release never run, no count comparison.
  - RC-01 becomes 01a (export), 01b (transform/gizmo guard) and 01c (the ARCHITECTURE.md claim).
- **RC-NEW-01..38 are minted** for singletons and hypotheses the dossier left unnumbered. RC-NEW-11 is unused: it was never minted, so there is a gap in the numbering.
- **Categories:**
  - **BUG**: the current code violates a written repo rule, and no other written source contradicts that rule.
  - **DESIGN_HOLD**: the written sources conflict, or none states the expected behaviour (OWNER-SEMANTICS-REQUIRED).
  - **TEST_HARNESS_DEFECT**: test, CI or evidence-currency problems, including coverage gaps. The product is not wrong.
  - **PERFORMANCE_RISK**: measured or reasoned cost with no recorded budget.
  - **DOC_DIVERGENCE**: a document or comment disagrees with the code.
  - **FUTURE_OPPORTUNITY**: FUTURE-NOT-DUE items and ideas.

## Mechanisms per category

These figures use seat findings and requirement rows only. 94 distinct mechanism ids are in use.

"Live" means some alias has status current or narrowed. "Hypothesis only" means every alias is a hypothesis.

| Category | Live | Hypothesis only |
| --- | --- | --- |
| BUG | 33 | 9 |
| DESIGN_HOLD | 14 | 1 |
| TEST_HARNESS_DEFECT | 16 | 2 |
| DOC_DIVERGENCE | 6 | 0 |
| PERFORMANCE_RISK | 6 | 3 |
| FUTURE_OPPORTUNITY | 5 | 1 |

Notes on this table:

- RC-21 is one documentation-drift bucket. It has 11 aliases, itemised in `CONTRACT_CONFLICTS.md`.
- RC-13f is counted under both TEST_HARNESS_DEFECT (I-02) and PERFORMANCE_RISK (H-15).
- RC-19 is counted under both DESIGN_HOLD (J-03) and FUTURE_OPPORTUNITY (G-06).

## The number: unique current defect mechanisms

**33 unique current defect mechanisms** (category BUG, status current or narrowed), under 22 parent RC numbers.

- 27 of them have at least one alias with status current. The other 6 survive only in narrowed form; they are still present on the product tree, in reduced scope:
  - RC-06e, the stale checkpoint after a clean leave;
  - RC-10a, Clear Mask drawn then refused;
  - RC-10c, Freeform Symmetry or level drawn then refused;
  - RC-17a, the lock guard is checked only at activation, which is latent;
  - RC-NEW-02, a raw token in the New Freeform refusal;
  - RC-NEW-08, Resume Sculpt has no native sketch guard, which is latent.
- **If you require the strict reading** "status current" (excluding narrowed), the figure is **27**.

The 33, listed: RC-01a, RC-01b, RC-02, RC-03, RC-04a, RC-04b, RC-04c, RC-05a, RC-05b, RC-05c, RC-06a, RC-06e, RC-07a, RC-07b, RC-07d, RC-08b, RC-09, RC-10a, RC-10b, RC-10c, RC-11, RC-12, RC-16a, RC-16b, RC-17a, RC-18a, RC-20, RC-NEW-01, RC-NEW-02, RC-NEW-03, RC-NEW-05, RC-NEW-08, RC-NEW-12.

### Confidence

Each mechanism is graded by the best-graded alias that names it.

**E1: 9 mechanisms, reproduced in this closeout with recorded output.** Each was reproduced on the host. None was reproduced on a device.

| Mechanism | Evidence |
| --- | --- |
| RC-01a | `diagnostics/CR-01`: 11 face-supported bodies MISMATCH, every independent body MATCH |
| RC-01b | `diagnostics/CR-02`: dependent `storedMoved=1 worldMoved=0` with `undoDelta=1` |
| RC-02 | `diagnostics/CR-03` scenarios A–C accepted; the Sculpt control is refused |
| RC-03 | `diagnostics/CR-06` `cr06_native.out`: `camera_view_identical_to_B0=1` |
| RC-09 | `diagnostics/CR-06` `CommaDiag.out` |
| RC-11 | `diagnostics/CR-04`: K3–K7 FAIL only in the subject scenarios |
| RC-06a, RC-04a, RC-04c | `diagnostics/CR-05`. This is a host run of the real `ProjectSlot`, `ProjectCheckpoint`, `AutosaveController` and `EditorUiState` classes, plus a line-cited transcription of `EditorWorkspaceView` with Android shims. It is E1 for the persistence logic but not for the View or Activity runtime. |

**E2: 12 mechanisms, source-confirmed with file:line and not reproduced** (SOURCE-CONFIRMED-NOT-REPRODUCED). These are RC-04b, RC-05a, RC-05b, RC-05c, RC-06e, RC-07b, RC-08b, RC-10b, RC-12, RC-NEW-01, RC-NEW-02 and RC-NEW-08.

- The race mechanisms RC-05a and RC-05b are E2 for the code path and E5 for their timing.
- RC-08b, Back during an in-project Revolve, was not run on any device.

**E3: 12 mechanisms, from recorded FC2 runs not repeated here.** These are RC-07a, RC-07d, RC-10a, RC-10c, RC-16a, RC-16b, RC-17a, RC-18a, RC-20, RC-NEW-03, RC-NEW-05 and RC-NEW-12.

**Sensitivity of the headline:**

- **+1 if the OWNER rules that CLAUDE.md "Autosave protects work" governs Recover.** RC-06b (A-02, the dossier's only remaining standalone P1 besides the export bug) would then become a BUG.
- **+1 if the OWNER rules F-03 a defect.** That is RC-08a (disagreement D-5).
- **−2 if latent defensive-guard gaps are not counted.** That is RC-17a and RC-NEW-08, which have no reachable UI path today.
- RC-13a (G-01, FP-fragile oracles) and RC-13e (I-01, the silent result cap) are deliberately counted as TEST_HARNESS_DEFECT, not BUG, because the product output is unaffected.
- **Not used:** the coordinator's focused CI DEVICE run was not visible to this task and is not cited. No emulator or device evidence is treated as physical.

### Severity of the 33

This is the dossier's post-Round-2 grade, taken from the highest alias.

| Severity | Mechanisms |
| --- | --- |
| P1 | 1: RC-01a (GLB export of a face-supported body) |
| P2 | 11: RC-01b, RC-02, RC-03, RC-04a (P2 per seat E, P3 per seat A), RC-04c, RC-06a, RC-08b, RC-09, RC-11, RC-12, RC-20 |
| P3 | 21: the remainder |
