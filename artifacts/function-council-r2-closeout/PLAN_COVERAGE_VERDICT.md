# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — plan coverage verdict

**NO CURRENT FULL MASTER PLAN COMPLIANCE CLAIM.**

**Verdict: PARTIAL-PLAN-SOURCE-GAP.**

Audit only; nothing in the product, the repo records or the previous dossier
was changed. Every number below was printed by a script in
`diagnostics/RECONCILE/` (commands and exit codes in
`diagnostics/RECONCILE/CMD.txt`); the outputs are quoted, not retyped from
memory. Inputs: the 681-row matrix
`artifacts/function-council-r2/requirements_traceability.tsv` and the three
closeout extracts `work/extract_SRC-410.tsv`, `work/extract_SRC-ADD.tsv`,
`work/extract_SRC-4058.tsv`. **All three extracts are present**; none of the
extractors failed.

## 1. The existing 681-row matrix (E1, this closeout)

`python3 -I check_matrix.py …/function-council-r2` → exit 0
(`check_matrix.out`):

```
HEADER_COLUMNS 24
DATA_ROWS 681
ROWS_WRONG_COLUMN_COUNT 0 []
UNIQUE_IDS 681 DUPLICATES []
VERDICTS_OUTSIDE_VOCAB []
HORIZONS {'H1': 358, 'H0': 251, 'H2': 71, 'NA': 1}
PUBLISHED_TABLE_LINES 39
CELL_MISMATCHES 0
RECOMPUTED_ALL {'PR': 313, 'PSR': 154, 'IU': 35, 'PART': 36, 'DEF': 37, 'REG': 0, 'INA': 1, 'DOC': 24, 'NIBD': 0, 'FND': 43, 'BO': 26, 'OOS': 5, 'USG': 7} TOTAL 681
```

The domain × horizon × verdict table in
`function-council-r2/REQUIREMENTS_TRACEABILITY.md` matches the TSV in every
one of its 39 lines × 13 verdict cells, and every published row total equals
the sum of its cells. **No mismatch.**

## 2. Historical statements extracted

Every count is from `build_deltas.py` → exit 0, `SUM_CHECK_OK`
(`build_deltas.out`).

### 2a. Per source

| Extract | Source file(s) | Statements | exact | partial | none |
| --- | --- | ---: | ---: | ---: | ---: |
| SRC-4058 | Master v4.0.58 (History v4.0.58 used for dates only) | 453 | 91 | 197 | 165 |
| SRC-410 | Master v4.1.0 (346), partial Master v4.1.58 (61), History v4.1.58 (13) | 420 | 102 | 216 | 102 |
| SRC-ADD | Addenda + history addenda v4.1.62..66 (298), PROJECT_STATE v4.1.66 incl. archived 4.1.65/4.1.64 (102) | 400 | 98 | 183 | 119 |
| **Total** | 15 of 16 files (HISTORY v4.0.58 contributes no rows by design) | **1,273** | **291** | **596** | **386** |

```
EXTRACT_ROWS 1273 UNIQUE_SRC_IDS 1273
UNIQUE_NORMALISED_QUOTES 1244 QUOTES_REPEATED_ACROSS_ROWS 18
TOTAL_MQ {'none': 386, 'partial': 596, 'exact': 291}
```

The unique count is **1,273 statements** (one id each). By normalised quote
text there are 1,244, because 18 quotes recur (mostly v4.1.0 restating
v4.0.58); they are kept as separate rows because their source, date and
authority differ, and they are never merged into one "current" statement.

### 2b. Per authority class

| Authority | SRC-4058 | SRC-410 | SRC-ADD | Total | exact | partial | none |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| CURRENT_REPO_CONFIRMED | 183 | 196 | 238 | 617 | 256 | 304 | 57 |
| SUPERSEDED | 54 | 30 | 26 | 110 | 18 | 51 | 41 |
| HISTORICAL | 24 | 65 | 68 | 157 | 2 | 85 | 70 |
| OWNER_DECISION_REQUIRED | 24 | 18 | 29 | 71 | 15 | 26 | 30 |
| UNRESOLVED_AUTHORITY | 61 | 53 | 32 | 146 | 0 | 60 | 86 |
| UNKNOWN_CURRENT_PLAN | 107 | 58 | 7 | 172 | 0 | 70 | 102 |
| **Total** | **453** | **420** | **400** | **1,273** | **291** | **596** | **386** |

By kind: requirement 393, decision 359, status_claim 220, non_goal 142,
gate 115, deferral 44 (sum 1,273).

**318 statements (146 + 172) have no current authority** — the plan says it,
no current repo record confirms or cancels it, and its standing depends on the
absent v4.1.61 or on an OWNER decision whose body is missing. None of them was
promoted to a current H0 or H1.

## 3. Disposition of every statement (sum-check)

```
CATEGORIES {'status_claim_nonexact_no_requirement_delta': 168, 'delta_unresolved': 304,
            'delta_extend': 269, 'delta_new_row_owner': 28, 'delta_no_row': 139,
            'exact_match_no_delta': 291, 'delta_new_row': 49, 'delta_owner_note': 25} SUM 1273
DELTA_ROWS_EXISTING 18 DELTA_ROWS_HISTORICAL_NONEXACT 814 TSV_DATA_ROWS 832
PROPOSED_NEW_ROWS_STATEMENTS 77 DISTINCT_BY_NORMALISED_QUOTE 76
SUM_CHECK_OK
```

| Category | Statements | In `REQUIREMENT_DELTAS.tsv`? |
| --- | ---: | --- |
| Exact match to an existing row | 291 | no (nothing to change) |
| Non-exact status claim (a dated fact, not a requirement) | 168 | no; the stale and wrong ones are named in §5 |
| Non-exact normative statement → **UNRESOLVED_AUTHORITY** (164 UNKNOWN_CURRENT_PLAN + 140 UNRESOLVED) | 304 | yes |
| → CURRENT_REPO_CONFIRMED, **extend an existing row's scope** (no new row) | 269 | yes |
| → CURRENT_REPO_CONFIRMED, **new row** (H1, or H2 for non-goals/deferrals) | 49 | yes |
| → OWNER_DECISION_REQUIRED, **new row** (H2 BLOCKED-OWNER) | 28 | yes |
| → OWNER_DECISION_REQUIRED, owner_decision note on an existing row | 25 | yes |
| → HISTORICAL (82) or SUPERSEDED (57): no row | 139 | yes |
| **Sum** | **1,273** | 814 delta rows |

291 + 168 + 304 + 269 + 49 + 28 + 25 + 139 = **1,273** = the unique count.
The 814 historical delta rows (304 + 269 + 49 + 28 + 25 + 139) plus the
**18 existing-row deltas** make the **832** data rows of the TSV.

`python3 -I diagnostics/RECONCILE/sumcheck_deltas.py REQUIREMENT_DELTAS.tsv`
→ exit 0 (`deltas_sumcheck.out`):

```
DATA_ROWS 832 BAD_COLS 0 UNIQUE_IDS 832
EXISTING_ROW_DELTAS 18 HISTORICAL_NONEXACT_DELTAS 814
PROPOSED_CLASSES {'existing-row change': 18, 'UNKNOWN_CURRENT_PLAN': 164, 'extend scope': 269, '(new row)': 77,
                  'SUPERSEDED by': 57, 'HISTORICAL (': 82, 'UNRESOLVED_AUTHORITY (no horizon': 140,
                  'add owner_decision': 25} SUM 832
CONFIDENCE {'E4': 317, 'E2': 515}
NEW_ROW_PROPOSALS_(new row) 77 PLUS_EXISTING_ROW_DELTAS_PROPOSING_A_NEW_ROW 1
```

**Proposed new rows: 78** = 77 statements (76 distinct texts) + 1 from
`DLT-E018` (the TEST-OWNER-02 budget conflict). Of these, 49 are
repo-confirmed (they are rules the repo states but the matrix never traced)
and 29 are OWNER rows. Confidence is E2 where the extract cites a current repo
record by file:line and E4 where it rests on the plan text alone; **no row is
E1** — this reconciliation ran no product code, so nothing in it is
REPRODUCED.

## 4. Proposed changes to existing rows (18, `DLT-E001`..`DLT-E018`)

No verdict is raised and nothing is moved to H0.

| Delta | Row | Change | Type |
| --- | --- | --- | --- |
| E001 | `APP-078` | OUT-OF-SCOPE → FUTURE-NOT-DUE / UNRESOLVED_AUTHORITY for accounts/cloud (a deferred plan stage, CLOUD-R0, not an exclusion) | verdict + authority |
| E002 | `APP-079` | "first run / onboarding / empty state" has a historical answer: v4.0.58:814 maps it to the APP-H1 "no project" frame; residual stays USG | authority + scope |
| E003, E004 | `APP-052`, `UI-086` | OBJ export is in the historical approved MVP (v4.1.0:189, Stage 034) but absent by `CLAUDE.md` rule → OWNER question | owner_decision |
| E005..E007 | `MOD-017`, `MOD-007`, `DOC-085` | X symmetry, pressure, subdivide/remesh, UV MVP: historical approved-MVP items, now H2; authority UNRESOLVED_AUTHORITY | authority |
| E008 | `MOD-035` | One-way Create Sculpt Copy (BRIDGE-R1, UI-OWNER-24) vs the shipped reversible workflow → OWNER question | owner_decision |
| E009 | `CAD-083` | v4.1.58 §B conditional Revolve-cap supports vs all Revolve faces ineligible → OWNER confirmation | owner_decision |
| E010 | `CAD-109` | Source refreshed: `PROJECT_STATUS.md:4899` "do not start Stage024 / CAD-A4" is stale; residual (Intersect, Revolve Add/Cut) has no plan home | source + authority |
| E011 | `RND-019` | Historical GATE-PERF-SCULPT names the budget the row calls "(no recorded budget)" | source + authority |
| E012 | `APP-064` | UI-PREF aesthetic verdict pending (`PROJECT_STATUS.md:2533-2541`) vs v4.1.0 "OWNER accepted" | owner_decision |
| E013..E015 | `CON-033`, `CON-054`, `CON-048` (+ group rows) | Stage 018A / 020M / MIRROR-01 OWNER UX verdicts never recorded | owner_decision |
| E016 | `CAD-113` | OD-27 C2 checklist; OWNER APK `262076…` expires **2026-11-11T23:31:32Z**; MODELING-R1 correction re-review pending | owner_decision |
| E017 | `APP-022` | UI-OWNER-25: import should converge on Objects + (current entry: project drawer) | owner_decision |
| E018 | `TST-038` | Extract match "exact" overstated; TEST-OWNER-02 budget gets its own OWNER row (90/120 in `CLAUDE.md` vs 30/45 in `PROJECT_STATUS.md:2543-2550`, same decision id) | new row |

## 5. What the reconciliation found

**Required by a historical plan and not done** (all UNRESOLVED_AUTHORITY or
OWNER_DECISION_REQUIRED; none promoted): UV MVP (Stages 029–032), OBJ export
(Stage 034), Sculpt Stroke X Symmetry and pressure (Stage 026), Subdivide /
voxel remesh (Stage 028), Surface Snap, Construction Grid Snap
(GRID-SNAP-01), Contact/Body Magnet Snap and Alignment Guides (020S1/020S2),
2–3 sample `.forge` projects in the APK, release identity, cloud/account
(CLOUD-R0), engine validation (Stage 035), GATE-E2E external import,
GATE-USER-01, the million-triangle import test, and the one-way Create Sculpt
Copy. v4.1.0 line 189 lists several of these as "approved MVP", and line 189
itself says the tiering under `OQ-B` is only `PROPOSED`. With v4.1.61 absent
and `OQ-B` open, whether any of them is due today cannot be decided.

**Done without an authorisation trace in the archive.** The repo records the
stage ids that delivered these, but no archive document carries the
authorising decision: Freeform and Surface bodies (only named by v4.1.58's
review of MODELING-FOUNDATIONS-R1 and by v4.1.66), the sketch drafting
toolkit (v4.1.0:191 lists "drafting" as a non-goal), CAD-VERTICAL-SLICE-R1
Add/Cut and the Manifold kernel (v4.0.58 names GATE-KERNEL, but its
"coplanar box − cylinder < 200 ms on physical ARM64" criterion has no repo
evidence, SRC-4058-382), Revolve New Body (CAD-A4 was BLOCKED in v4.1.0),
and five palettes (v4.0.58: three). SolidCraft's "technique only, no code
copied" exists only in the addenda, with no repo provenance record.

**Stale descriptions.** `PROJECT_STATUS.md:4895-4900` still says do not start
Stage024 / CAD-A4; v4.1.0's decision table marks UI-OWNER-03, -04, -33A,
-33B, -38 and -39 NOT_IMPLEMENTED though the repo implements them; v4.0.58's
header narrative stops at v4.0.41; the addenda's "FC-R2 NOT STARTED" is
superseded by `PROJECT_STATUS.md` v0.103.0; the previous dossier says Master
v4.1.0 is "not available" (it now is — `SOURCES_RECONCILED.md` §7).

**Wrong or unsupported DONE claims.** v4.1.0's "UI-PREF OWNER accepted"
(SRC-410-016) against the repo's "none aesthetically approved"; the repo's
GATE-KERNEL "passed" without evidence for the ARM64 timing criterion;
SRC-410-280's "exact" match to `TST-038`. One closeout extract claim is
**REFUTED**: SRC-ADD-367's "D18 pointer looks wrong" —
`CadRetainedSketchOwnerTest.java:471-472` shows D18 does cover the
world-plane Keep Sketch refusal (E2).

**Real OWNER blocks.** `OQ-B` (full MVP or tiering — decides H0), `OQ-C` /
GATE-VIABILITY, `OQ-A` (UV half), `OQ-01` / `OQ-02` (still blocking Stage
020D and SCULPT-DIM-01), `OQ-03/04/06/07/E`, A2 world-plane retained sketch
(OD-01), the C2 physical review OD-27 (APK expiry 2026-11-11T23:31:32Z), the
MODELING-R1 correction physical re-review, release readiness, the OBJ
conflict, the one-way Sculpt copy conflict, and the TEST-OWNER-02 budget
conflict.

## 6. Why the verdict is PARTIAL-PLAN-SOURCE-GAP

1. **The canonical `FORGESHAPE_MASTER_PLAN_v4.1.61.md` is absent** (MANIFEST,
   every addendum). The newest full master in hand is v4.1.0, dated
   2026-09-07, 51 days and 61 revision numbers earlier. It was not
   reconstructed, and the sixteen files were not merged.
2. **H0 cannot be defined from the sources.** The addenda never define the
   MVP; v4.1.0 defines one but records its tiering as `PROPOSED` under the
   still-open `OQ-B`. The matrix's 251 H0 rows therefore rest on repo records,
   which make them due but not provably "the approved MVP".
3. **318 of 1,273 statements (25.0 %) have no current authority**, and 172 of
   them depend on v4.1.61 specifically.
4. **The OWNER decision bodies are absent** (72 titles only), so every row
   resting on a decision id rests on a title.
5. **The matrix itself is sound** (§1: 681 rows, unique ids, 0 total
   mismatches) and covers **291 statements exactly and 596 partially**; 386
   have no matching row, of which 78 are proposed as new rows and the rest
   are unresolved, historical or superseded.

Coverage can be raised to a full compliance verdict only after the OWNER
supplies the documents listed in `SOURCES_RECONCILED.md` §6, starting with
v4.1.61 (or an authoritative successor) and the current OWNER_DECISIONS.

## Key numbers

| | |
| --- | ---: |
| Existing matrix rows (checked, 0 mismatches) | 681 |
| Historical statements extracted (unique ids) | 1,273 |
| … exact / partial / none | 291 / 596 / 386 |
| … no current authority (UNRESOLVED + UNKNOWN_CURRENT_PLAN) | 318 |
| Delta rows in `REQUIREMENT_DELTAS.tsv` | 832 (18 existing-row + 814 historical) |
| Proposed new rows | 78 (49 repo-confirmed + 29 OWNER) |
| Verdict | PARTIAL-PLAN-SOURCE-GAP |
