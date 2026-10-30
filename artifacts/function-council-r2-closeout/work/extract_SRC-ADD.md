# SRC-ADD — extraction of the v4.1.62..66 addenda, history addenda and PROJECT_STATE v4.1.66

Seat: SRC-ADD (FUNCTION-COUNCIL-R2-CLOSEOUT-R1, audit only). Output table:
`work/extract_SRC-ADD.tsv` (400 rows, `SRC-ADD-001..400`). Builder and
verification: `diagnostics/SRC-ADD/build_extract.py`, command in
`diagnostics/SRC-ADD/CMD.txt`, recorded run in
`diagnostics/SRC-ADD/build_extract.out` (exit 0).

**Mechanical checks (E1, run in this closeout):** every `statement_quote` is a
verbatim substring of its source file (400/400) and is at most 200 characters;
every ID in `matches_existing_ids` exists in
`artifacts/function-council-r2/requirements_traceability.tsv`; every
`SUPERSEDED` row names its successor. What the checks do NOT prove: the
classification judgements (kind, authority, match quality) are the seat's
reading of the sources against the current repo records (E2/E4), not a run.

## 1. Document metadata

All eleven files are Polish/English coordinator documents (written by the
coordinator, "ChatGPT" in STATE v4.1.64). Each says outright that it is a
delta to the ABSENT canonical `FORGESHAPE_MASTER_PLAN_v4.1.61.md` and does not
replace it. None of them defines H0 (the approved MVP). They point to v4.1.61
and to `OQ-B`, which is still open in the partial Master v4.1.58.

| File | Title / version | Date | What it claims to be | Completeness |
| --- | --- | --- | --- | --- |
| `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.62.md` | Master Plan addendum v4.1.62 / CAD-R2 | 2026-10-28 | OWNER provisional APK acceptance; authorises CAD-R2 (5 phases); test gates; SolidCraft policy; out of scope | Delta only; "to be appended to v4.1.61, NOT used instead of it" |
| `..._HISTORY_ADDENDUM_v4.1.62.md` | History addendum v4.1.62 | 2026-10-28 | History note for the same decision; quality quotas (30+/15+/10+) | Delta only |
| `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.63.md` | Addendum v4.1.63 (CAD-R2 closeout / C1 gate) | 2026-10-28 | CURRENT EXECUTION delta (CAD-R2 TECH PASS), the C1 work order, open A2 question | Delta to §1 of the absent canon |
| `..._HISTORY_ADDENDUM_v4.1.63.md` | History addendum v4.1.63 | 2026-10-28 | Coordinator audit of the CAD-R2 report | Delta only |
| `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.64.md` | Addendum v4.1.64 | 2026-10-28 | C1 TECH PASS; performance disposition; A2 options 0–3; C2 work order and not-authorised list | Delta only |
| `..._HISTORY_ADDENDUM_v4.1.64.md` | History addendum v4.1.64 | 2026-10-28 | Coordinator audit of C1 (ZIP SHAs, counts) | Delta only |
| `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.65.md` | Addendum v4.1.65 | 2026-10-29 | C2 TECH PASS; binding status; remaining risks | Delta only |
| `..._HISTORY_ADDENDUM_v4.1.65.md` | History addendum v4.1.65 | 2026-10-29 | Coordinator audit of C2 | Delta only |
| `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.66.md` | Addendum v4.1.66 | 2026-10-29 | OWNER direction: run FUNCTION-COUNCIL-R2 (requirements 1–9) | Delta only; "product unchanged" |
| `..._HISTORY_ADDENDUM_v4.1.66.md` | History addendum v4.1.66 | 2026-10-29 | History note for the Council order | Delta only |
| `FORGESHAPE_PROJECT_STATE_v4.1.66.md` | Project state v4.1.66 | 2026-10-29 | Current state block (§1–5), then **ARCHIVED v4.1.65** and **ARCHIVED v4.1.64**, each kept "verbatim" | Self-contained checkpoint; the archived blocks are extracted as their own versions (doc_version column) |

**Internal references to documents that are NOT in the archive or the repo:**

- `FORGESHAPE_MASTER_PLAN_v4.1.61.md` and the full HISTORY / OWNER decisions for
  v4.1.61. Every file names this canonical master and every file says it is absent.
- `PROMPT_CAD_R2_ULTRACODE_RETAINED_SKETCH_PARAMETRIC_QUALITY.md` (A62, H62).
- `PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md`
  (A66, H66, S66; STATE gives the path `/mnt/data/...`).
- `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1` prompt (H65, S65, S66).
- Neither the C1 nor the C2 prompt is named as a file.
- `FORGESHAPE_PROJECT_STATE_v4.1.62.md` and `..._v4.1.63.md` are absent. v4.1.64
  and v4.1.65 survive only as the ARCHIVED blocks inside v4.1.66.
- The OWNER physical "pakiet ze STATE v4.1.65" survives as S65 §3.
- `/mnt/data/ForgeShape_CAD_R2_C1_OWNER_911f5c3.apk` and
  `..._C2_OWNER_14a3dbc.apk` are coordinator-local copies, not in the repo.
- Every repo dossier these files cite is present at this tree:
  `artifacts/cad-r2-retained-sketch-parametric-ux/`,
  `artifacts/cad-r2-c1-quality-gate/`, `artifacts/cad-r2-c2-picking-seam/`,
  `artifacts/function-council-r1/` and `artifacts/function-council-c1-d1/`.
- The repo `PROJECT_STATUS.md` versions they cite (v0.100.0 / 0.101.0 / 0.102.0)
  are superseded by v0.103.0.

**SolidCraft** (`tangency-labs/solidcraft@504d7c9…`) is named only in these
sources. It does not appear in `CLAUDE.md`, `PROJECT_STATUS.md`, `PRODUCT.md`,
`ARCHITECTURE.md` or any `artifacts/cad-r2*` dossier. The "no code copied"
claim therefore has no repo record.

## 2. Counts

400 rows. Source breakdown (one row per atomic item; each enumerated list is
split item by item): A62 57, H62 16, A63 49, H63 10, A64 49, H64 10, A65 31,
H65 11, A66 55, H66 10, S66 (current block) 28, S65 (archived) 35, S64
(archived) 39.

| kind | n |
| --- | ---: |
| requirement | 73 |
| decision | 74 |
| gate | 40 |
| non_goal | 97 |
| deferral | 0 |
| status_claim | 116 |

No row is classed `deferral`. The sources do not defer work to a named later
stage. They say "out of scope / not authorised / not this stage", and every
such item is classed `non_goal`.

| authority | n |
| --- | ---: |
| CURRENT_REPO_CONFIRMED | 238 |
| HISTORICAL | 68 |
| OWNER_DECISION_REQUIRED | 29 |
| UNRESOLVED_AUTHORITY | 32 |
| SUPERSEDED | 26 |
| UNKNOWN_CURRENT_PLAN | 7 |

| match_quality | n |
| --- | ---: |
| exact | 98 |
| partial | 183 |
| none | 119 (39 status_claim + 80 others) |

kind × authority (the non-zero cells):

- requirement: CURRENT 43, HISTORICAL 25, UNRESOLVED 3, UNKNOWN_PLAN 2
- decision: CURRENT 31, OWNER 17, HISTORICAL 11, UNRESOLVED 10, UNKNOWN_PLAN 4, SUPERSEDED 1
- gate: CURRENT 16, HISTORICAL 11, OWNER 11, UNRESOLVED 2
- non_goal: CURRENT 81, UNRESOLVED 12, HISTORICAL 3, UNKNOWN_PLAN 1
- status_claim: CURRENT 67, SUPERSEDED 25, HISTORICAL 18, UNRESOLVED 5, OWNER 1

**Supersession that matters:** these claims are overtaken by later sources or
by `PROJECT_STATUS.md` v0.103.0:

- "FC-R2 NOT STARTED / NOT VERIFIED" (A66, H66, S66) is superseded by
  `PROJECT_STATUS.md:3-22`: the Council ran, with the verdict `PARTIAL-PLAN-SOURCE-GAP`.
- "PERF INVESTIGATION OPEN" (A63, H63) is superseded by the C1 disposition
  `NO-REPRO-STABLE` (`PROJECT_STATUS.md:56-60`).
- "D04 curved-wall tap may choose YZ" and "font scale >1.0 open" are
  superseded by the C1 fixes.
- "pick seam … nadal open" (H64, S64) is superseded by the C2 fix `0fa12c9`.
- "host / oracles not separately executed" (A63, H63, A64, A65, H65, S65) is
  superseded for the C2 product tree: FC-R2 ran host 4392/0 with the 17
  oracles (`PROJECT_STATUS.md:13`). The C2 per-triangle perf figure
  (+2.4–3.0 %) is still recorded but was not reproduced.
- No older H0 was promoted. The rows that would define due scope ("całą
  dotychczas należną funkcjonalność według zatwierdzonego Master Planu", "every
  approved feature expected by the Master Plan", "ani nie ogranicza przyjętego
  MVP", OQ-B) are `UNKNOWN_CURRENT_PLAN` or `OWNER_DECISION_REQUIRED`.

**Open OWNER items carried by these sources:**

- A2 / `OQ-CAD-R2-WORLD-PLANE-RETAINED-SKETCH`, options 0–3. Option 0 is the
  current refusal and is not a decision. This is OD-01.
- The C2 physical review: an exact-APK checklist of 8 items, each answered
  PASS / FAIL / NOT TESTED. This is OD-27.
- `OQ-B` (full MVP or tiering). It is absent from the dossier's OD register.
- Release readiness.
- Physical aesthetic acceptance and ergonomics.

**Other rows:**

- The ARCHIVED S64 evidence hierarchy (evidence sources) and MANIFEST's
  "Priorytet źródeł" (plan sources) rank different things. They are not an
  explicit supersession. That row (SRC-ADD-363) is UNRESOLVED_AUTHORITY.
- S64 cites "`R2_SK_06`, `D18`" as the refusal. In the repo, `R2_SK_06` is the
  refusal check (`forgeshape_cad_r2_selftest.cpp:439`). `D18` in
  `CadRetainedSketchOwnerTest.java:471` is the History rotation/background
  case, so the D18 pointer looks wrong (SRC-ADD-367, E2).

## 3. Candidate gaps: requirements with `match_quality = none`

There are 80 non-`status_claim` rows with no matching trace ID: 28 + 26 + 26.
They fall into three groups.

### 3a. No match and no current repo basis: real candidate gaps (28)

These are `UNRESOLVED_AUTHORITY`, `OWNER_DECISION_REQUIRED` or
`UNKNOWN_CURRENT_PLAN`. Neither the 681-row matrix nor any current repo record
carries them.

| src_id(s) | Statement (short) | Authority | Why it is a gap |
| --- | --- | --- | --- |
| 035, 037, 114, 178, 395 (+ 366 status) | SolidCraft reference-only; no copying of modules / icons / texts / UI / code | UNRESOLVED | No provenance or licence record in the repo; the claim rests on coordinator text only |
| 046, 047, 111, 177 | No AI / MCP / public MCP | UNRESOLVED | The repo forbids neither (only "no network permission") |
| 006, 061, 221 | No store premiere, no release, no release AAB without a separate gate | UNRESOLVED | The repo has no release / AAB / signing gate at all |
| 052 | Release readiness undecided | OWNER | No repo definition of release readiness |
| 051 | `OQ-B` full MVP vs tiering open | OWNER | Determines H0; not in OWNER_DECISIONS_REQUIRED.md |
| 008 | "ani nie ogranicza przyjętego MVP" | UNKNOWN_PLAN | The MVP is defined in the absent v4.1.61 |
| 057, 074 | Preserve canon's OWNER decisions / changelog; only canon §1 may state current status | UNKNOWN_PLAN | Governance of the absent canon |
| 094 | "never sweep away a real issue as 'container noise' without evidence" | UNRESOLVED | No durable perf-evidence policy (closest: TST-013) |
| 121 | Never reconstruct missing history / never overwrite with shortened text | UNRESOLVED | Restated by MANIFEST, not a repo record |
| 232, 301 | RESULT-GATE-R1 docs-only prompt is run only with OWNER evidence | UNRESOLVED | Repo names OD-27 instead; the prompt file is absent |
| 323 | No Council TECH PASS until coordinator verdict | UNRESOLVED | No coordinator verdict on FC-R2 is recorded |
| 339 | OWNER report format PASS/FAIL/NOT TESTED + device / Android / input | OWNER | Not in repo records |
| 348, 349, 350 | Physical verdict rule (any FAIL → OWNER-PHYSICAL-FAIL; NOT TESTED → PARTIAL; all PASS → FULL-PASS) | UNRESOLVED | The verdict rule exists only in coordinator state |
| 360 | On OWNER FAIL, prepare a correction prompt directly | UNRESOLVED | Process only |
| 363 | Evidence hierarchy (refs/CI > PROJECT_STATUS > Claude > old plans) | UNRESOLVED | Differs in scope from MANIFEST's plan priority |

Other gap rows with only a partial trace match (not counted above): 146
(benchmark code-layout control, partial TST-013); 147 (no x86 compiler flags
in the ARM64 APK, partial PORT-036, relates to OD-17); 216 and 337 (edge-tie
between two faces may depend on order/ABI; stated only in the C2 SUMMARY);
215 (dense Sculpt cost of double-precision picking); 333 (`u+v` repro numbers
only in BEFORE_REPRO.md); 269 (accessibility has no spec, OD-21).

### 3b. No trace match, but the current repo states it: matrix gap only (26)

The rule exists in `CLAUDE.md` / `PROJECT_STATUS.md` / `PRODUCT.md`; only the
traceability matrix has no row for it:

- 032: the debug APK is the OWNER APK.
- 064, 115, 252, 317: no MVP widening (`CLAUDE.md:132`).
- 072, 256: no PASS without a basis (`CLAUDE.md:1710`).
- 090: a PASS token is not full-scope acceptance.
- 165: artifact / APK hash recorded.
- 175, 389: no renderer rewrite (`CLAUDE.md:134-135`).
- 189, 207, 231: coordinator verdict and "no coding stage now"
  (`PROJECT_STATUS.md` Next Stage).
- 235, 247: the FC-R2 order and branch.
- 261, 318, 321, 322: audit-only, no product/test/CI change.
- 281, 282: OD register and one recommended stage.
- 306, 313: Council scope rules.
- 315: no rebase / force push (`CLAUDE.md:117`; cherry-pick is not named there).
- 326: the PROJECT_STATUS entry was written.

Status claims in the same position:

- 143/144/218/370: the C1 performance disposition (`PROJECT_STATUS.md:56-60`).
  No trace row covers it.
- 193/224: the product tree is frozen at `f3db953`.
- 204: the current OWNER APK SHA `262076…`, with an expiry of
  2026-11-11T23:31:32Z. That expiry is an actual deadline for OD-27.

### 3c. No match and HISTORICAL or SUPERSEDED (26): no action

These are stage-internal work orders, methods and gates of the closed
CAD-R2 / C1 / C2 stages: 002, 003, 009, 010, 013, 020, 031, 039, 058, 059,
070, 091, 092, 093, 102, 130, 160, 164, 166, 270, 278, 279, 280, 303, 305,
387. They also include the "signed APK SHA" wording (031): it asks for a
signed APK while every recorded artifact is the debug APK.

## 4. Classification notes and limits

- A stage-scoped prohibition is `CURRENT_REPO_CONFIRMED` only when a current
  record carries the same rule: `PROJECT_STATUS.md:4895-4900` "Do not start …",
  `CLAUDE.md` library / format / merge / emulator rules, or A2 at
  `CLAUDE.md:674-681` + `PROJECT_STATUS.md:4878-4880`. Otherwise it is
  `HISTORICAL` (stage-only) or `UNRESOLVED_AUTHORITY` (framed as a general
  rule).
- STEP/IGES rows are confirmed through the Export rule "every other
  interchange format stays absent" (`CLAUDE.md:1400-1403`). It covers export
  and import of other formats, not CAD exchange specifically. They are
  matched as `partial` to APP-052.
- Physical-device items (ARM64, GPU, stylus/hover, 16 KB, thermal) are
  `CURRENT_REPO_CONFIRMED` as OPEN through `CLAUDE.md:122-126`. No row claims
  any of them is closed. The emulator is never physical.
- Nothing here was compiled or executed except the extraction verifier. No
  claim in this file is REPRODUCED in the product sense.
