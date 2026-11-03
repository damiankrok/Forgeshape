# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — SUMMARY

**Result: `AUDIT-CLOSEOUT-PARTIAL-SOURCE-GAP`.**
**NO CURRENT FULL MASTER PLAN COMPLIANCE CLAIM.**

This closeout is audit-only:
- no product, test, script, fixture, workflow or format file changed;
- nothing merged; no FullSharded run;
- no OWNER decision taken.

| Item | Value |
| --- | --- |
| Branch | `audit/function-council-r2-closeout-r1`, from exactly `2e5d8fc` (Council R2 head) |
| Product tree | `70a6c14` |
| Preflight | `BEFORE.md` |

## 1. Sources

The OWNER archive `FORGESHAPE_COUNCIL_R2_SOURCE_ARCHIVE.zip` (SHA-256
`90f5d581…20c4`) matches its manifest: 16/16 files, byte count and SHA-256.
Copies are kept unchanged under `sources/`.

Chronology, scope and gaps are in `SOURCES_RECONCILED.md`. The archive holds:
- Master v4.0.58 and its history;
- Master v4.1.0 (2026-09-07);
- the partial v4.1.58 and its history;
- addenda v4.1.62–66 and PROJECT_STATE v4.1.66.

**The canonical v4.1.61 is absent**, as are current OWNER decision bodies
(the archive has 72 titles only), the full current history, and PROJECT_STATE
between v4.1.0 and v4.1.66. Nothing was stitched together. What the OWNER
still has to supply is listed in `DECISION_GATES.md` Part 4.

## 2. Requirements reconciliation

Details: `PLAN_COVERAGE_VERDICT.md` and `REQUIREMENT_DELTAS.tsv`.

**The existing 681-row matrix checks clean:**
- 681 unique IDs, 24 columns, every verdict in the vocabulary;
- H0 251, H1 358, H2 71, NA 1;
- 0 cell mismatches against its published totals.

**1,273 historical statements were extracted from 15 source files:**

| Match to an existing row | Count |
| --- | --- |
| Exact | 291 |
| Partial | 596 |
| None | 386 |

| Authority | Count |
| --- | --- |
| CURRENT_REPO_CONFIRMED | 617 |
| SUPERSEDED | 110 |
| HISTORICAL | 157 |
| OWNER_DECISION_REQUIRED | 71 |
| UNRESOLVED_AUTHORITY | 146 |
| UNKNOWN_CURRENT_PLAN | 172 |

**318 statements (25 %) have no current authority.** None was promoted to H0.

**`REQUIREMENT_DELTAS.tsv`** has 832 rows: 18 changes to existing rows and
814 historical deltas. It proposes 78 new rows: 49 confirmed by the repo and
29 OWNER rows. The sum-check passes (`SUM_CHECK_OK`).

**What the reconciliation found:**
- **Required by an old plan and not done:** UV MVP, OBJ export, Sculpt X
  symmetry / pressure, remesh, grid / surface snap, among others. All are
  UNRESOLVED_AUTHORITY, because v4.1.0 marks its own MVP tiering as only
  `PROPOSED` under the open `OQ-B`.
- **Done without an authorisation trace in the archive:** Freeform, Surface,
  the drafting toolkit, Add/Cut with Manifold, Revolve, five palettes.
- **Stale descriptions** were found.
- **Real OWNER blocks** were found.

## 3. CR-01 … CR-07 (details: `CRITICAL_REPROS.md`, `CRITICAL_REPRO_MATRIX.tsv`)

| CR | Result |
| --- | --- |
| CR-01 GLB export, face-supported CAD | **REPRODUCED E1.** All 11 face-supported bodies are exported at their stored identity transform (the origin) instead of their derived placement. The 12 control bodies all match. Cap and side exports produce byte-identical GLBs. Four committed `CADB` v2 fixtures export misplaced. `GlbExportTest` (24/24 green in `40979514270`) has no face-supported case. |
| CR-02 transform / gizmo on such a body | **Domain effect REPRODUCED E1:** +1 Undo, the fingerprint moves, and the stored transform moves while the drawn body does not. The gizmo is drawn 3 m from the body, and the junk transform reaches `.forge` and decodes. **JNI and UI reachability is SOURCE-CONFIRMED (E2).** Same root cause as CR-01 (two placement readers), but two separate symptoms: a CR-02 refusal alone would not fix CR-01. |
| CR-03 Import GLB during a sketch | **REPRODUCED E1:** the import is accepted, a body is created and made active, a Construction step is recorded and cannot be undone while the sketch stays open. The sketch, encode and later commit are **not** corrupted (REFUTED). With the chooser open, the outcome is **OWNER-SEMANTICS-REQUIRED**. The Sculpt refusal control is REPRODUCED, and the bootstrap direct call is unreachable from the UI. |
| CR-04 Sculpt NotRetained | **REPRODUCED E1:** Undo and Redo land on states that never existed; `hasEdits` reads false while the deformation stands; navigator row 0 is unreachable. The "applies and is said by name" part is confirmed (REFUTED as a defect). The remedy waits on OD-22. |
| CR-05 Recover / Save-and-continue / recreation | E-02 and E-02b (the recovery question appears over a live or saved-unchanged project after recreation) are **REPRODUCED E1 (component, real classes)**. A-02 (recovered work protected nowhere) is **REPRODUCED E1 (component) + OWNER-SEMANTICS (OD-05)**. A-01, A-05 and A-06 are **SOURCE-CONFIRMED-NOT-REPRODUCED (E2)**, with A-01 also under OD-06. E-02c (the race) is **BLOCKED**. |
| CR-06 comma / navigator / Back / 48 dp | **Navigator REPRODUCED E1** at the native layer: the camera view is unchanged after Flip and ±90. **Comma: the parser and model legs are E1** ("2,5" commits 25), but end to end it is SOURCE-CONFIRMED, because the platform key filter cannot run on a host. **Revolve Back on API 36:** SOURCE-CONFIRMED (E2). **Bootstrap Back:** OWNER-SEMANTICS (OD-08). **Hit areas** (36.19, 36.95, 39.24, 43.43 dp): measured on the emulator in a historic run (E3), not re-measured. |
| CR-07 test infrastructure | **REPRODUCED E1:** PROJECT is at 256/256, so a 257th *failing* check would still print `_SELFTEST_OK`. Under FMA contraction 2 host oracles fail, and arm64 emits `fmadd` even at −O0 (an oracle tolerance problem, not shown to be a product numeric fault). CI FAST runs no host suites (E2). |

## 4. Council closeout (details: `COUNCIL_CHALLENGE_CLOSEOUT.md`, `ALIAS_TO_ROOT_CAUSE.tsv`)

- **Seats:** still 3 COMPLETE (E, G, I) and 7 PARTIAL. Every PARTIAL seat
  still has the missing plan and at least one physical, TalkBack or OWNER
  reason, so no seat was upgraded by more reading.
- **The 8 P2 findings not panel-checked before:** each now has two
  independent checks, and all 8 were UPHELD. Severity and category changed
  for several:
  - H-07 and H-08 → PERFORMANCE_RISK;
  - I-01, I-05 and I-08 → TEST_HARNESS_DEFECT, mostly P3;
  - I-02 → PERFORMANCE_RISK;
  - I-03 → DOC_DIVERGENCE;
  - I-06 → DESIGN_HOLD.
- **The 10 downgraded positive claims:** none can be raised as worded. Each
  has narrower E1/E3 parts, and five are partly refuted.
- **Counting units:**
  - The three earlier numbers count different things: 37 DEFECT-CONFIRMED
    requirement rows, 38 P0–P2 seat findings, and 23 panel root causes.
  - `ALIAS_TO_ROOT_CAUSE.tsv` maps 251 aliases, and its script was re-run
    after repair.
  - **Single number: 33 unique current BUG mechanisms.** The strict reading
    (status current, without the narrowed ones) gives 27.
  - Confidence by best alias: E1 6, E2 15, E3 12.
  - Severity: P1 1, P2 11, P3 21.
  - Closeout-new findings, provisional: +3 BUG gives 36.
- **Live mechanisms by category (78 in total):**

  | Category | Mechanisms |
  | --- | --- |
  | BUG | 33 |
  | DESIGN_HOLD | 15 |
  | TEST_HARNESS_DEFECT | 14 |
  | DOC_DIVERGENCE | 6 |
  | PERFORMANCE_RISK | 6 |
  | FUTURE_OPPORTUNITY | 4 |

## 5. Android UX (details: `ANDROID_UX_REVIEW.md`)

Ten frictions are ranked. Four are new in this review:
- UX-2: the line-dimension label and editor partly under the navigator and over the Tool Rail's x-span;
- UX-5: a Surface staged-edit verdict that falls outside the visible sheet;
- UX-6: no cue on Transform over a locked body;
- UX-9: a ghost "Body #0" capsule with the IME up in the bootstrap.

All four are graded from historic emulator captures (E3), with E1 only for
UX-2's overlap arithmetic. A static capture is not treated as TalkBack, hover
or comfort evidence.

There are 42 function chains: 26 verified end to end and 15 with at least one
UNVERIFIED step. Physical-device items stay `OWNER-PHYSICAL-UNVERIFIED`.

## 6. Desktop (H2, proposal only; details: `DESKTOP_SCOPE_REVIEW.md`)

Re-verified on Linux in this closeout (E1):
- 98/98 platform-neutral TUs compile with g++ 13.3, with clang 18, and against the NDK's libc++;
- 100/101 headers compile on their own (the exception is `forgeshape_renderer.h`);
- the renderer TU compiles with 5 stubbed Android declarations;
- the JNI TU is not neutral;
- 10/10 shaders validate.

There is **no MSVC or Windows evidence**, so no port is called easy. The
three variants (native per-OS C++, JVM shell over JNI, UI-library candidate
behind an OWNER licence gate) are described by cost driver, adapters, the
compile test that would prove each, and licence risk.

## 7. Tests (details: `TESTS.md`)

**Fresh local runs:** host 4392/0 (25 suites, 17 oracles), JVM 196/0,
corpus decoder verdicts 72, builds, release guard PASS.

**Focused CI DEVICE:**
- `40979514270`: **63/63**.
- `40983965648`: **72/73**. The one failure is `uir4a04`, a stale test count
  (8 expected, 12 drawn): TEST_HARNESS_DEFECT, not a regression.

**Coverage:** 48 of 70 instrumented classes have now run on this product
tree; 22 have not. No FullSharded run.

## 8. Next (details: `DECISION_GATES.md`, `PROPOSED_NEXT_SINGLE_STAGE.md`)

**One proposed stage, not authorised and not started:
`CAD-FACE-SUPPORTED-PLACEMENT-R1`.** Every consumer of a face-supported CAD
body's placement — GLB export and transform/gizmo writes — would read the
one derived placement.

- **Why:** CR-01 and CR-02 confirm it at E1, it needs no OWNER decision, and
  it touches no format.
- **Gate:** the OWNER authorises it after, or alongside, the OD-27 physical
  review.

The OWNER decisions (OD-01 A2, OD-05 Recover, OD-27 physical review, and the
rest) are listed in `DECISION_GATES.md` Part 3 with what this closeout adds.
None of them was decided here.
