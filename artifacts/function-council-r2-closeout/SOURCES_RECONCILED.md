# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — sources reconciled

Audit only. Nothing under `app/`, `scripts/`, `testdata/`, `.github/`, the
repo records or `artifacts/function-council-r2/` was changed. This file
registers the OWNER source archive and states what it can and cannot settle.

**None of the sixteen files below is the canonical
`FORGESHAPE_MASTER_PLAN_v4.1.61.md`. They were NOT merged, concatenated or
stitched into one document, and no "v4.1.61" was reconstructed from them.**
Each is read as what it says it is, and each conflict between them is
recorded, not resolved.

## 1. Archive integrity

| Item | Value |
| --- | --- |
| Archive | `FORGESHAPE_COUNCIL_R2_SOURCE_ARCHIVE.zip`, 197,231 B, SHA-256 `90f5d5818a9294fc53a3b4784cfafb477e3af03762a344f61988018d65ef20c4` (`sources/ARCHIVE_SHA256.txt`) |
| Manifest | `sources/MANIFEST.md`, issued 2026-10-29 (Polish). It says the archive is "historical and partial", that the full v4.1.61 and its matching history and OWNER decisions are absent, and that they "must not be reconstructed by concatenating" the files |
| Hash check (E1, this closeout) | `sha256sum -c SHA256SUMS.txt` → 17 × `OK` (16 documents + `MANIFEST.md`), exit 0. Recorded in `diagnostics/RECONCILE/sha256_check.out` |
| Manifest's source priority | absent canonical Master > explicit current OWNER decisions > repo contracts (for implementation state) > historical Master v4.1.0 / v4.0.58 and partial 4.1.58 > coordination addenda. "Conflicts need verification, not automatic overwriting." |

## 2. The sixteen documents

Bytes and SHA-256 are from `MANIFEST.md`, and each was re-verified (§1).
"Rows" is the number of statements the closeout extractors took from that
file (`work/extract_SRC-*.tsv`).

| # | File | Bytes | SHA-256 | Version / date | Rows |
| --- | --- | ---: | --- | --- | ---: |
| 1 | `FORGESHAPE_MASTER_PLAN_v4.0.58.md` | 171,087 | `7a44d377…f3e7b` | 4.0.58; header "Updated 2026-09-01", content matches the history's v4.0.58 note of **2026-09-07** | 453 |
| 2 | `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.58(1).md` | 217,560 | `1baef80a…d436f` | 4.0.58; notes v2.3.0 (2026-08-19) to v4.0.58 (2026-09-07) | 0 (dates and supersession only) |
| 3 | `FORGESHAPE_MASTER_PLAN_v4.1.0.md` | 60,416 | `dc530063…40ae` | docs revision 4.1.0, as of 2026-09-07 | 346 |
| 4 | `FORGESHAPE_MASTER_PLAN_v4.1.58.md` | 6,043 | `0008c92b…0f95` | 4.1.58, 2026-10-06 | 61 |
| 5 | `FORGESHAPE_MASTER_PLAN_HISTORY_v4.1.58.md` | 1,396 | `80a260ab…952c` | 4.1.58, 2026-10-06 | 13 |
| 6 | `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.62.md` | 3,408 | `4b5beec1…7a33` | 4.1.62, 2026-10-28 | 57 |
| 7 | `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.62.md` | 1,619 | `4ca36ab0…dcd71` | 4.1.62, 2026-10-28 | 16 |
| 8 | `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.63.md` | 4,249 | `081fd542…0b66` | 4.1.63, 2026-10-28 | 49 |
| 9 | `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.63.md` | 1,905 | `e4a0af10…4b14` | 4.1.63, 2026-10-28 | 10 |
| 10 | `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.64.md` | 4,732 | `ef2d203a…fb697` | 4.1.64, 2026-10-28 | 49 |
| 11 | `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.64.md` | 2,688 | `5559ea7e…7df66` | 4.1.64, 2026-10-28 | 10 |
| 12 | `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.65.md` | 3,978 | `ed8d248d…c3f0` | 4.1.65, 2026-10-29 | 31 |
| 13 | `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.65.md` | 2,969 | `8e5284b6…e4ef` | 4.1.65, 2026-10-29 | 11 |
| 14 | `FORGESHAPE_MASTER_PLAN_ADDENDUM_v4.1.66.md` | 4,668 | `0bf3314a…fa80` | 4.1.66, 2026-10-29 | 55 |
| 15 | `FORGESHAPE_MASTER_PLAN_HISTORY_ADDENDUM_v4.1.66.md` | 1,785 | `8c9d7c01…dcd8` | 4.1.66, 2026-10-29 | 10 |
| 16 | `FORGESHAPE_PROJECT_STATE_v4.1.66.md` | 20,336 | `ac9b264f…db7b2` | 4.1.66, 2026-10-29; carries ARCHIVED v4.1.65 and v4.1.64 blocks | 102 |
| | **Total** | | | | **1,273** |

The full 64-hex digests are in `sources/SHA256SUMS.txt`.

## 3. What each document claims to be, its scope and completeness

| # | Claims to be | Scope | Completeness |
| --- | --- | --- | --- |
| 1 | The coordinator's master, one of a three-file set (with OWNER_DECISIONS and HISTORY). §1 is the only present-tense section | Product, UX, architecture, quality contracts, the binding roadmap (§7.1), risks, open questions, process | **Full for its date, historical.** Superseded as the master by #3, whose §8.1 maps every v4.0.58 section to a new home. Internally inconsistent: the OWNER decision count is 72, 67 or 57 depending on the section; CAD-R0-DATA is cited as v1, v2 and v3; some §7.1 status cells use values outside its own vocabulary |
| 2 | The report archive and every version note verbatim. Declares itself not current truth | Event chronology | Full for its range; v3.9.0 never published and v3.57–v3.59 lost (stated in the file). The master names it without the `(1)` suffix |
| 3 | "Canonical use: this document owns current product/system contracts, roadmap, coordinator policy and active decision summaries" | Whole product, roadmap, decision index (72 headings, titles only) | **Full master, historical.** Predates CAD-VERTICAL-SLICE-R1, the Manifold kernel, Revolve, Freeform, Surface, drafting, CAD-R2 and TEST-OWNER-04 |
| 4 | OWNER physical-review result for `MODELING-FOUNDATIONS-R1` and the binding correction scope | One correction stage | **Partial.** No roadmap, no decision index, no policy sections. Not a full master revision, despite the file name |
| 5 | History note for #4 | One event | Partial |
| 6 | Delta "to be appended to the full canonical v4.1.61, NOT used instead of it": OWNER provisional APK acceptance, CAD-R2 authorisation, SolidCraft technique-only policy | CAD-R2 | Delta only |
| 7 | History note for #6; quality quotas | CAD-R2 | Delta only |
| 8 | CURRENT EXECUTION delta to §1 of the absent canon; CAD-R2 closeout; C1 work order; open A2 | CAD-R2 / C1 | Delta only |
| 9 | Coordinator audit of the CAD-R2 report | CAD-R2 | Delta only |
| 10 | C1 TECH PASS; performance disposition; A2 options 0–3; C2 work order | C1 / C2 | Delta only |
| 11 | Coordinator audit of C1 (ZIP SHAs, counts) | C1 | Delta only |
| 12 | C2 TECH PASS; binding status; remaining risks | C2 | Delta only |
| 13 | Coordinator audit of C2 | C2 | Delta only |
| 14 | OWNER direction to run FUNCTION-COUNCIL-R2 (requirements 1–9); "product unchanged" | Audit order | Delta only |
| 15 | History note for #14 | Audit order | Delta only |
| 16 | Current state block (§1–5), then ARCHIVED v4.1.65 and ARCHIVED v4.1.64 kept "verbatim" | Coordinator checkpoint | Self-contained checkpoint for 2026-10-29; PROJECT_STATE v4.1.62 and v4.1.63 do not exist anywhere in the archive |

Every addendum (#6–#16) states that it is a delta to the absent v4.1.61 and
does not replace it. None defines H0 (the approved MVP). They point to
v4.1.61 and to `OQ-B` (full MVP or the proposed tiering), which #3 records as
`PROPOSED`, not approved, and #6 §E still lists as open.

## 4. Chronology

```
2026-08-19 .. 2026-09-07  HISTORY v4.0.58 (#2) notes v2.3.0 .. v4.0.58
2026-09-07                MASTER v4.0.58 (#1)  -> migrated into MASTER v4.1.0 (#3), same day
                          [v4.1.1 .. v4.1.57: no document in the archive]
2026-10-06                MASTER v4.1.58 (#4, partial) + HISTORY v4.1.58 (#5)
                          [v4.1.59, v4.1.60: no document]
                          [v4.1.61: the canonical full master — ABSENT]
2026-10-28                ADDENDUM + HISTORY ADDENDUM v4.1.62, v4.1.63, v4.1.64 (#6-#11)
2026-10-29                ADDENDUM + HISTORY ADDENDUM v4.1.65, v4.1.66 (#12-#15)
2026-10-29                PROJECT_STATE v4.1.66 (#16, with archived 4.1.65 / 4.1.64 blocks)
2026-10-29                MANIFEST (archive issued)
```

So the newest full master in hand (#3) is 51 days and 61 minor
revisions older than the canonical one. Everything after it is either partial
(#4, #5) or a delta (#6–#16). The repo records (`CLAUDE.md`,
`PROJECT_STATUS.md` v0.103.0) are newer than every full master here, and the
closeout extracts judge each historical statement against them.

## 5. Documents referenced by the sources but missing

Missing from the archive AND from the repo. Checked in this closeout with
`git ls-files | grep` and a filesystem `find` for the name stems
`MASTER_PLAN_v4.1.61`, `OWNER_DECISIONS`, `PROMPT_D_FORGESHAPE`,
`CAD_R0_DATA_DECISION`, `PROMPT_CAD_R2_ULTRACODE` and
`FUNCTION_COUNCIL_R2_FULL_PRODUCT`: 0 tracked for each, except two
`OWNER_DECISIONS` hits that are the audit-local
`artifacts/fable-cad-architecture-audit-r1/OWNER_DECISIONS.md` (not a canonical
OWNER_DECISIONS file). One exception outside the repo: a copy of
`PROMPT_CAD_R2_ULTRACODE_RETAINED_SKETCH_PARAMETRIC_QUALITY.md` exists among
this session's earlier OWNER uploads (`/root/.claude/uploads/…/5406184c-…`);
it is not in the archive, was not read for this reconciliation, and is listed
below only as not-in-archive. Nothing is reconstructed:

| Missing document | Referenced by | What its absence costs |
| --- | --- | --- |
| `FORGESHAPE_MASTER_PLAN_v4.1.61.md` (canonical) | every addendum, #16, MANIFEST | The current MVP (H0), roadmap and decision summaries. Every `UNKNOWN_CURRENT_PLAN` row depends on it |
| Full `FORGESHAPE_MASTER_PLAN_HISTORY` matching v4.1.61 | MANIFEST, #9, #11, #13 ("append to full, retained historical archive") | Event chronology from 2026-09-07 to 2026-10-28 |
| `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md` and any later `OWNER_DECISIONS_*` | #1, #3 (72 headings, titles only) | The bodies of every OWNER decision. Rows resting on a decision id (UI-OWNER-24, -25, -26, -31, -34, -36, OQ-A..OQ-E, OQ-03/04/06/07) rest on a title |
| `FORGESHAPE_PROJECT_STATE_v4.0.58.md`, `..._v4.1.0`, `..._v4.1.62.md`, `..._v4.1.63.md` and every state between 4.1.0 and 4.1.66 | #3 (migration source), #16 | Operational state between 2026-09-07 and 2026-10-28. v4.1.64 and v4.1.65 survive only as archived blocks inside #16 |
| `FORGESHAPE_CAD_R0_DATA_DECISION_v1/v2/v3.md`, `FORGESHAPE_CAD_R0_UX_DECISION_v1.md` | #1 | The CAD-R0 questionnaire answers |
| `PROMPT_D_FORGESHAPE.md`, `OPUS_5_HIGH_SCULPT_H1_HISTORY_NAVIGATOR.md` (SCULPT-H1 prompt v2), `FORGESHAPE_PROMPT_AUTHORING_RULES.md` | #1, #3 | Audit instruction and prompts behind v4.1.0 |
| The "proposed release tiering" text behind `OQ-B` | #3 | What a tiered MVP would drop |
| `PROMPT_CAD_R2_ULTRACODE_RETAINED_SKETCH_PARAMETRIC_QUALITY.md` | #6, #7 | CAD-R2's A1–A10 matrix in its original wording |
| `PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md` | #14, #15, #16 | The Council's original requirement text (1–9) |
| `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1` prompt | #13, #16 | The docs-only gate run with OWNER evidence |
| UI-SPEC-R0 accepted mocks (v4.0.9 SHA `9f1e8667…`, Rev1 SHA `9d032cad…`) | #1 | The ≤4 dp overlay gate's reference |
| OWNER screenshots / videos; `1 lowpoly.glb`; `/mnt/data/ForgeShape_CAD_R2_C1_OWNER_911f5c3.apk`, `..._C2_OWNER_14a3dbc.apk` | #2, #4, #11, #13 | Physical evidence; coordinator-local copies only |

Every repo dossier the addenda cite is present at this tree
(`artifacts/cad-r2-retained-sketch-parametric-ux/`, `cad-r2-c1-quality-gate/`,
`cad-r2-c2-picking-seam/`, `function-council-r1/`, `function-council-c1-d1/`).

## 6. Documents the OWNER must still supply

1. **The full `FORGESHAPE_MASTER_PLAN_v4.1.61.md`, or an authoritative
   successor** that states the current MVP (H0) and roadmap. Without it no
   full plan-compliance claim is possible (see `PLAN_COVERAGE_VERDICT.md`).
2. **`FORGESHAPE_OWNER_DECISIONS_*`, the current version**, with decision
   bodies, not titles. Needed at minimum for OQ-B, OQ-C, OQ-A (UV half),
   OQ-03/04/06/07/E, UI-OWNER-24 (one-way Sculpt copy), UI-OWNER-25 (import
   via Objects +), UI-OWNER-26, -31, -34, -36.
3. **`FORGESHAPE_MASTER_PLAN_HISTORY_*`, the full current version**
   (2026-09-07 to 2026-10-29 is covered only by partial #5 and the addenda).
4. **`FORGESHAPE_PROJECT_STATE_*` between v4.1.0 and v4.1.66** (at least
   v4.1.0, v4.1.58–v4.1.63).
5. The other referenced-but-missing files in §5, in particular the OQ-B
   tiering text, the FUNCTION-COUNCIL-R2 prompt and the CAD-R2 prompt.

## 7. Corrections to the closeout's own extract notes and to the previous dossier

These are recorded, not applied to the read-only inputs.

- **SRC-ADD-367 note "the D18 pointer looks wrong" — REFUTED (E2).** The
  note says `D18` in `CadRetainedSketchOwnerTest.java:471` is only the
  History rotation/background case. The test comment at
  `CadRetainedSketchOwnerTest.java:471-472` reads "History survives a
  rotation and a background trip while open; a world-plane sketch never
  offers Keep Sketch", and the method is
  `d18_history_survives_rotation_and_background_and_world_plane_keep_stays_absent`.
  So D18 does cover the world-plane refusal, and matrix row `CAD-089`'s test
  citation (`host R2_SK_06; CadRetainedSketchOwnerTest D18`) stands.
- **SRC-410-280 match quality "exact" to `TST-038` — overstated.** `TST-038`
  is the FullSharded fingerprint/resume rule. The TEST-OWNER-02 budget itself
  has no matrix row, and the repo states it twice with different numbers
  (`CLAUDE.md` 90/120-minute aggregate; `PROJECT_STATUS.md:2543-2550`
  30/45-minute closeout). Proposed as a new OWNER row (`DLT-E018`).
- **Previous dossier now stale on one point.** `function-council-r2/REQUIREMENTS_TRACEABILITY.md`
  ("Missing plan sources") and `SOURCE_REGISTER.md` §4 say Master v4.1.0,
  PROJECT_STATE and MASTER_PLAN_HISTORY are "not available". The OWNER
  supplied v4.1.0, v4.0.58, HISTORY v4.0.58, partial v4.1.58 and PROJECT_STATE
  v4.1.66 on 2026-10-29. The canonical v4.1.61 is still absent, so the
  dossier's PARTIAL-PLAN-SOURCE-GAP conclusion is unchanged.
