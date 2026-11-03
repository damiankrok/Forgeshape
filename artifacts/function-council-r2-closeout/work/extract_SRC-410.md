# SRC-410: requirements extracted from Master Plan v4.1.0, v4.1.58 and History v4.1.58

Part of FUNCTION-COUNCIL-R2-CLOSEOUT-R1. This step only reads documents. No product
file, script or earlier dossier was changed. The rows are in
`work/extract_SRC-410.tsv`: 420 rows, 14 columns, ids `SRC-410-001` to
`SRC-410-420`.

**How quotes were checked.** A script compared every `statement_quote`
with its source file after collapsing whitespace. All 420 quotes match
word for word, and none is longer than 200 characters.

**What the authority labels are based on.** They are judged against the
current repo records at this tree (`CLAUDE.md`, `PROJECT_STATUS.md` v0.103.0,
`PRODUCT.md`, `ARCHITECTURE.md`, `DATA_PACKAGE_SPEC.md`), plus two source
lines in `app/src/main/java` cited for the ForgeShape mark. File:line
references were spot-checked against the files.

**What no row claims.** No row says anything about the canonical Master Plan
v4.1.61. That file is absent and was not reconstructed. A row that would
depend on it is marked `UNKNOWN_CURRENT_PLAN`.

## Document metadata

| | `FORGESHAPE_MASTER_PLAN_v4.1.0.md` | `FORGESHAPE_MASTER_PLAN_v4.1.58.md` | `FORGESHAPE_MASTER_PLAN_HISTORY_v4.1.58.md` |
|---|---|---|---|
| Title | FORGESHAPE MASTER PLAN | ForgeShape Master Plan v4.1.58 | ForgeShape Master Plan History v4.1.58 |
| Version / date | Docs revision 4.1.0, as of 2026-09-07 | 4.1.58, 2026-10-06 | 4.1.58, 2026-10-06 |
| Size | 619 lines, 60,416 B (SHA-256 matches MANIFEST) | 179 lines, 6,043 B | 41 lines, 1,396 B |
| What it says it is | The canonical owner of current product and system contracts, the roadmap, coordinator policy and active decision summaries (§0). Migrated from v4.0.58 plus PROJECT STATE v4.0.58, HISTORY v4.0.58, the SCULPT-H1 prompt, OWNER_DECISIONS v4.0.54 and PROMPT_D. | The OWNER's physical-review result for `MODELING-FOUNDATIONS-R1`, a source audit, and the binding correction scope (`MODELING-R1-OWNER-CORRECTION-PROJECT-SHELL-SKETCH-SUPPORT-PLANAR-R1`). | The history entry for that review: three blockers, the Cut-face debt promoted into the correction, and a decision of no integration. |
| Completeness | A full master, but **historical**: it predates CAD-VERTICAL-SLICE-R1, the Manifold kernel, Revolve, Freeform and Surface, drafting, CAD-R2, TEST-OWNER-04 and MODELING-R1-OWNER-CORRECTION. Its §8.3 retention inventory (about 90 task ids mapped to sections) is navigation, not requirements, so it was not extracted row by row. | **Partial.** It covers only one correction stage. It has no roadmap, no decision index and no policy sections. It is not a full master revision. | **Partial.** A single history event. |
| Referenced but missing from the archive | `FORGESHAPE_PROJECT_STATE_v4.0.58.md`; `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md` (the bodies of all 72 decisions, so most decision rows carry the title only); `PROMPT_D_FORGESHAPE.md`; the SCULPT-H1 prompt v2; the matching PROJECT STATE v4.1.0 and HISTORY v4.1.0; the "proposed release tiering" text behind `OQ-B`. `FORGESHAPE_MASTER_PLAN_v4.0.58.md` and `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.58(1).md` are in the archive but out of scope for SRC-410. | The OWNER screenshots; the source tree at `7e0224056c18bcff2446487e526cd5a703b1d36b` (not re-read here); the full v4.1.58 master body, if one existed. | The same screenshots. |

## Counts

Rows per document:

| Document | Rows | requirement | decision | gate | non_goal | deferral | status_claim |
|---|---|---|---|---|---|---|---|
| v4.1.0 | 346 | 107 | 128 | 19 | 19 | 30 | 43 |
| v4.1.58 | 61 | 27 | 7 | 9 | 0 | 0 | 18 |
| HISTORY v4.1.58 | 13 | 2 | 5 | 0 | 0 | 0 | 6 |
| **Total** | **420** | **136** | **140** | **28** | **19** | **30** | **67** |

Rows by kind and authority:

| kind \ authority | CURRENT_REPO_CONFIRMED | SUPERSEDED | HISTORICAL | OWNER_DECISION_REQUIRED | UNRESOLVED_AUTHORITY | UNKNOWN_CURRENT_PLAN | total |
|---|---|---|---|---|---|---|---|
| requirement | 107 | 6 | 3 | 0 | 3 | 17 | 136 |
| decision | 45 | 6 | 5 | 15 | 41 | 28 | 140 |
| gate | 9 | 5 | 7 | 2 | 0 | 5 | 28 |
| non_goal | 12 | 0 | 0 | 0 | 7 | 0 | 19 |
| deferral | 14 | 5 | 0 | 1 | 2 | 8 | 30 |
| status_claim | 9 | 8 | 50 | 0 | 0 | 0 | 67 |
| **total** | **196** | **30** | **65** | **18** | **53** | **58** | **420** |

Authority by document:

| Document | CURRENT_REPO_CONFIRMED | SUPERSEDED | HISTORICAL | OWNER_DECISION_REQUIRED | UNRESOLVED_AUTHORITY | UNKNOWN_CURRENT_PLAN |
|---|---|---|---|---|---|---|
| v4.1.0 | 155 | 22 | 41 | 17 | 53 | 58 |
| v4.1.58 | 37 | 5 | 18 | 1 | 0 | 0 |
| HISTORY v4.1.58 | 4 | 3 | 6 | 0 | 0 | 0 |

How well each row matches an existing matrix row (`function-council-r2/requirements_traceability.tsv`): 102 exact, 216 partial, 102 none.

Unmatched rows (`none`) by kind: requirement 6, decision 64, gate 8, non_goal 9, deferral 7, status_claim 8.

## Candidate gaps: requirements with no match (`match_quality=none`)

No row in the 681-row matrix carries any of these six requirements:

| src_id | Statement (source section) | Authority | Note |
|---|---|---|---|
| SRC-410-005 | "The approved MVP combines exact Construction/CAD workflows with direct Sculpt workflows and portable project persistence." (§1.1) | UNKNOWN_CURRENT_PLAN | The MVP scope belongs to the master, and v4.1.61 is absent. |
| SRC-410-051 | Stage 020S1, Contact / Body Magnet Snap (§2.2) | UNKNOWN_CURRENT_PLAN | No current repo record. Blocked by SPATIAL-R1. |
| SRC-410-052 | Stage 020S2, Alignment Guides + magnetic alignment (§2.2) | UNKNOWN_CURRENT_PLAN | Blocked by SPATIAL-R1 and OQ-03. |
| SRC-410-053 | GRID-SNAP-01, Construction Grid Snap (§2.2) | UNKNOWN_CURRENT_PLAN | Blocked by OQ-06. The matrix has only the sketch grid (CAD-017), which is a different thing. |
| SRC-410-055 | Stages 029–032, the UV MVP (§2.2) | UNKNOWN_CURRENT_PLAN | No current repo record at all. |
| SRC-410-133 | "UV MVP" as an approved MVP element (§3.10) | UNKNOWN_CURRENT_PLAN | Same gap as SRC-410-055. |

### Other unmatched rows worth noting

These rows are not requirements, but no matrix row covers them either:

- **Gates and checkpoints.**
  - `GATE-E2E`, the external-engine export verdict (SRC-410-013, -066, -309). It appears in no current repo record.
  - `GATE-USER-01` (SRC-410-067).
  - Stage 035, engine/interchange validation (SRC-410-058).
  - The v4.1.58 **OWNER physical re-review of the corrected APK** (SRC-410-406). `PROJECT_STATUS.md:4871-4880` still lists it as pending, but no matrix row tracks it as a gate.
- **Deferrals.**
  - CLOUD-R0 and cloud sync (SRC-410-061, -123).
  - UI-LAYOUT-CUSTOM (SRC-410-064).
  - UV MVP, final export validation and release hardening (SRC-410-025).
- **Open owner questions.** None of these is in the matrix or in any current repo record: `OQ-A` (UV half), `OQ-B`, `OQ-C`, `OQ-03`, `OQ-04` (matched only partially, through CON-078), `OQ-06`, `OQ-E` and `OQ-07` (partial). Only `OQ-01` and `OQ-02` are confirmed open by the repo (`PROJECT_STATUS.md:4972-4976`).
- **Title-only owner decisions with no repo record.** These are decisions whose full text is absent from the archive (only the title survives in v4.1.0) and that no current repo record cites: UI-OWNER-07, -14, -15, -18, -21, -24 (a one-way Construction → Sculpt project copy, with `BRIDGE-R1` / `Create Sculpt Copy` also absent), -26 (topology vocabulary), -31 (Surface Snap), -34 (Contact Snap), -36 (Custom Layout), -43; ARCH-OWNER-05; DOC-OWNER-02.
- **Non-goals no current record restates.** Physics, ray tracing, rigging/animation, Blender-class retopology, marketplace/social/live collaboration, AI model generation, and a plugin ecosystem (the repo only rules out a brush plugin surface).

## Conflicts and supersessions found while extracting

These are recorded in the TSV, not decided here.

1. **Two different TEST-OWNER-02 budgets.**
   - The source and `CLAUDE.md:1789-1792` give an aggregate warning at 90 minutes and a stop at 120, with two attempts.
   - `PROJECT_STATUS.md:2541-2550` gives a closeout budget of 30 minutes, stopping at 45.
   - TEST-OWNER-03 (20/30) is superseded by TEST-OWNER-04 (`PROJECT_STATUS.md:2552-2569`).
   - Rows: SRC-410-259, -263, -280, -282.
2. **Conditional Revolve-cap eligibility (v4.1.58 §B).** The source says partial-Revolve planar caps become sketch supports *if* they have stable references. The repo makes every Revolve face ineligible (`CLAUDE.md:700-701;735-737`). The row is marked SUPERSEDED. Whether the condition held is not recorded, so the OWNER may want to confirm (SRC-410-390).
3. **Stage 024 and CAD-A4.** Most of their scope has shipped as Add/Cut and Revolve New Body. Still, `PROJECT_STATUS.md:4898-4899` says not to start either id. What is left of each (Intersect; Revolve Add/Cut) has no current plan home (SRC-410-046, -047).
4. **OBJ export.** v4.1.0 schedules Stage 034 (OBJ export). The current rules say OBJ is absent in both directions (`CLAUDE.md:1158;1401-1402`). Whether OBJ is still planned depends on v4.1.61 (SRC-410-057, -134).
5. **Wording tensions.**
   - "Drafting" is a v4.1.0 non-goal, while the repo now ships a sketch drafting toolkit and excludes only drawing sheets (SRC-410-142).
   - UI-PREF is "OWNER accepted" in v4.1.0 but "implemented, none aesthetically approved" in `PROJECT_STATUS.md:2533-2540` (SRC-410-016, -230).
   - SEL-OUT's visual review is "OWNER LATER" in v4.1.0, while `PROJECT_STATUS.md:5019` says further testing was cancelled by the OWNER (SRC-410-116).
6. **Implementation status in v4.1.0's decision table is out of date.** UI-OWNER-03, -04, -33A, -33B, -38 and -39 are marked NOT_IMPLEMENTED there but are implemented in the current repo records. The decisions themselves still stand. Only the status column is old.
7. **BACKUP-01** (verify a private remote) is overtaken by `CI-CLOUD-R1`: GitHub `origin` is now the shared source of truth. The repo does not state whether the remote is private (SRC-410-068).
8. **The v4.1.58 correction's product decisions (A, B and C) are all CURRENT_REPO_CONFIRMED** in `CLAUDE.md:321-339` and `:629-641` / `:699-707`. The only part still open is its gate, the OWNER's physical re-review, which stays OWNER_DECISION_REQUIRED.
