# FUNCTION-COUNCIL-R2: requirements traceability

Audited tree: `70a6c14f703bfda2ab689b855126c1d12a541e41`. Its product tree
(`app/src/main`) is identical to `f3db953`; see [BEFORE.md](BEFORE.md).
The data is in [`requirements_traceability.tsv`](requirements_traceability.tsv):
681 requirement rows, one header, 24 tab-separated columns. Fresh test
executions and CI run ids are recorded once, in [TESTS.md](TESTS.md), which
the coordinator writes. This file cites no run id that a seat row does not
already carry.

## Status: PLAN-COVERAGE-PARTIAL

- **Missing plan sources.** The canonical `FORGESHAPE_MASTER_PLAN_v4.1.61.md`,
  `FORGESHAPE_OWNER_DECISIONS_*`, `MASTER_PLAN_HISTORY_*`, `PROJECT_STATE`
  and Master v4.1.0 are not available ([BEFORE.md](BEFORE.md), "Authoritative
  sources"; [SOURCE_REGISTER.md](SOURCE_REGISTER.md)).
- **Where horizons come from instead.** Every horizon was reconstructed from
  repo records: `CLAUDE.md` rule IDs, `PROJECT_STATUS.md` (Owner Decision
  Baseline, Next Stage, Known Issues, Technical Debt), `PRODUCT.md`,
  `ARCHITECTURE.md`, `DATA_PACKAGE_SPEC.md`, `docs/` and the `artifacts/*`
  dossiers. Each row's `source` column names the file and the section or ID.
- **Consequence.** A requirement that exists only in the unavailable Master
  cannot appear here. The matrix is therefore a lower bound on the planned
  scope, not the whole of it.
- **No adversarial verification file.** `work/VERIFICATION.md` does not exist
  at synthesis time. A filesystem search found no `VERIFICATION*` file newer
  than `BEFORE.md`. So no verdict here comes from an adversarial verification
  pass. What was applied instead are the Round 2 outcomes recorded in each
  `SEAT_*.md` "Round 2" section and in `work/delib_*.md`:
  - **B-07 withdrawn.** `delib_E_to_B.md` #3. `CON-074` already excludes it,
    and it remains only as hypothesis H-B4.
  - **C-03 narrowed.** The Keep Sketch half is withdrawn
    (`delib_F_to_C.md` #3). `CAD-063` carries only the Add/Cut half.
  - **J-05 narrowed.** The revolve-axis half is withdrawn
    (`delib_C_to_J.md` C-1). `ADV-063` is PASS-SOURCE-AND-RECORDED-RUNTIME.
  - **H-01 promoted.** It is now `RND-006` DEFECT-CONFIRMED
    (`delib_A_to_H.md` O-1; `delib_H_to_E.md` #10).

  If a VERIFICATION.md is produced later, rows whose verdict rests on a
  finding it refutes must be relabelled hypothesis or withdrawn, and the
  totals recomputed with the command below.
- **What each DEFECT-CONFIRMED rests on.** Every DEFECT-CONFIRMED row graded
  E2 is a source-derived proof that the seats did not reproduce at runtime.
  The grade is kept in `evidence_grade`.
- **What the evidence does not cover.** No emulator row is a physical-device
  PASS. Stylus, pressure, hover, hardware GPU, ARM64 runtime, 16 KB pages and
  thermal behaviour are BLOCKED-OWNER or OWNER-PHYSICAL-UNVERIFIED (for
  example `TST-041`, `MOD-115`, `CAD-113`, `RND-066`).

## Method

1. **Read every seat row.** The ten seat matrices `work/req_A.tsv` to
   `work/req_J.tsv` hold 880 rows in total. All have the same 19-column
   header, and every row is well-formed.
2. **Find duplicates by hand.** A duplicate is the same requirement stated by
   two seats: the same rule, the same observable behaviour, and the same
   scope. Rows that are related but distinct were kept apart. For example,
   `APP-022` (drawer row order) and `UI-006` (the drawer closes first) stay
   separate, and so do `APP-063` (Back in sketch, Freeform and Dimensions)
   and `UI-083` (owner records disagree on Back in sketch).
3. **Merge.** 199 duplicate rows were folded into 147 canonical rows, leaving
   681 rows. The canonical row lists every folded ID, with its seat, horizon,
   verdict and grade, in `merged_duplicates`, and as "duplicate of …" in
   `dependencies`. The pre-merge verdicts are in `original_verdicts`.
4. **Resolve verdicts between merged rows.**
   - **Both PASS grades, different strength.** The stronger grade was taken
     only when another seat's row reproduced the same requirement in this
     audit. This happened 18 times, each noted "verdict raised" with the
     added grade.
   - **Conflicts involving a non-PASS verdict.** These were decided
     explicitly, and each reason is in `synthesis_note`. The rows are
     `APP-065`, `DOC-038`, `DOC-044`, `DOC-063`, `DOC-086`, `RND-006`,
     `RND-008`, `ADV-001`, `ADV-013`, `UI-004` and `CON-052`.
   - **No finding was counted twice.** `CAD-001`'s PARTIAL is the bootstrap
     Back path, which is counted once as `UI-009`. `DOC-011`'s PARTIAL is the
     JNI-wrapper half, counted once as `DOC-087`.
5. **Normalise to the vocabulary.**
   - **Split verdicts.** A row graded "PASS for one half, IMPLEMENTED-UNTESTED
     for the other" takes the weaker half: `MOD-057`, `MOD-064`, `MOD-086`,
     `MOD-109` and `CON-052`.
   - **Annotated PASS verdicts.** "PASS-REPRODUCED (bounded host timings…)"
     becomes PASS-REPRODUCED: `MOD-091`, `MOD-111`.
   - **Horizons.**
     - "H1 (owner question…)" becomes H1 (`APP-038`).
     - "H2-pending-decision" becomes H2 (`ADV-018`, `ADV-051`).
     - "H0/H1" (`CAD-101`) was folded into `MOD-061` (H1).
     - "-" (`APP-076`) was folded into `PORT-026` (H0).
     - "- (evidence gap)" becomes `NA` (`UI-087`, OUT-OF-SCOPE). It is the
       only row that is not a product requirement.
6. **Assign each row a domain.** The `domain` column comes from the canonical
   ID's prefix, which is the domain of the seat that owns that row. The
   modelling rows (`MOD-*`) are split three ways: `MOD-068`..`MOD-091` are
   Freeform, `MOD-092`..`MOD-113` are Surface, and the rest are Sculpt plus
   cross-representation rules.

The merge is a script. Its duplicate map and override reasons are in the
script, not in prose:

```
python3 -I /tmp/claude-0/fc2-scratch/synth-trace/merge.py  /home/user/Forgeshape/artifacts/function-council-r2
python3 -I /tmp/claude-0/fc2-scratch/synth-trace/totals.py /home/user/Forgeshape/artifacts/function-council-r2/requirements_traceability.tsv
```

`merge.py` asserts the following:

- every seat file has the same header;
- every row has 19 fields;
- no ID is duplicated across seats;
- every merge target exists, and no row is both a target and folded;
- every output verdict is in the vocabulary.

`totals.py` asserts that the output IDs are unique and the verdicts valid. It
prints the table below and the anomaly list.

## Column meanings

| Column | Meaning |
| --- | --- |
| `domain` | Functional domain (from the owning seat; see Method 6) |
| `seat` | Seat whose row is canonical (A–J) |
| `ID` | Stable requirement ID (`APP`, `CON`, `CAD`, `MOD`, `DOC`, `UI`, `PORT`, `RND`, `TST`, `ADV`) |
| `source` | Repo record the requirement comes from (file + section / rule ID) |
| `horizon` | H0 = approved MVP, already due. H1 = later OWNER-authorised, should work today. H2 = future, blocked, optional or "Not this stage". NA = not a product requirement |
| `expected_behaviour` | The observable behaviour required |
| `implementation_status` | What the seat found in code |
| `entrypoint_android` / `jni_api` / `cpp_owner` | The path from Android control to JNI export to owning C++ (file:line where known) |
| `persisted_format` | `.forge` section or version touched, or `-` |
| `undo_history` | Which history records it, and how |
| `negative_refusal_path` | The named refusal, or the guard |
| `tests` | Host check IDs, JVM and androidTest classes |
| `evidence_run_sha` | The run and tree the evidence came from (as the seat recorded it; see [TESTS.md](TESTS.md)) |
| `evidence_grade` | E1 = reproduced in this audit. E2 = source proof (file:line). E3 = historic recorded run. E4 = docs only. E5 = inference |
| `verdict` | One of the thirteen vocabulary verdicts |
| `severity_gap` | P1/P2/P3/INFO, plus the finding ID (for example `F-01`, `H-01`) |
| `dependencies` | Related rows and findings, and "duplicate of …" for merged rows |
| `possible_improvement` | Isolated fix the seat proposed (not implemented; this audit changes no product file) |
| `owner_decision` | Open owner question, if any |
| `merged_duplicates` | Folded rows: ID (seat, horizon, verdict, grade) |
| `original_verdicts` | Every pre-merge verdict, canonical first |
| `synthesis_note` | Normalisation or override applied by this synthesis, with its reason |

## Totals: domain × horizon × verdict

Exact counts from `totals.py`. Column key: PR = PASS-REPRODUCED;
PSR = PASS-SOURCE-AND-RECORDED-RUNTIME; IU = IMPLEMENTED-UNTESTED;
PART = PARTIAL; DEF = DEFECT-CONFIRMED; REG = REGRESSION-CONFIRMED;
INA = INACCESSIBLE; DOC = DOC-DIVERGENCE; NIBD = NOT-IMPLEMENTED-BUT-DUE;
FND = FUTURE-NOT-DUE; BO = BLOCKED-OWNER; OOS = OUT-OF-SCOPE;
USG = UNDETERMINED-SOURCE-GAP. A dot means zero.

| Domain | Horizon | PR | PSR | IU | PART | DEF | REG | INA | DOC | NIBD | FND | BO | OOS | USG | Total |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Adversarial integrity | H0 | 4 | 3 | 3 | 1 | 1 | . | . | . | . | . | . | . | 1 | 13 |
| Adversarial integrity | H1 | 1 | 1 | . | . | 1 | . | . | 1 | . | . | 1 | . | . | 5 |
| Adversarial integrity | H2 | . | . | . | 1 | . | . | . | . | . | 2 | 3 | . | 1 | 7 |
| Android UI/UX | H0 | 3 | 7 | 1 | 1 | 2 | . | . | . | . | . | . | . | . | 14 |
| Android UI/UX | H1 | 10 | 13 | . | 2 | 7 | . | . | 1 | . | . | . | . | 1 | 34 |
| Android UI/UX | H2 | . | . | 1 | . | . | . | . | . | . | 1 | 2 | 2 | . | 6 |
| Android UI/UX | NA | . | . | . | . | . | . | . | . | . | . | . | 1 | . | 1 |
| App shell & project lifecycle | H0 | 2 | 14 | . | 1 | 2 | . | . | 1 | . | . | . | . | . | 20 |
| App shell & project lifecycle | H1 | 4 | 24 | 2 | 6 | 5 | . | . | 7 | . | . | 2 | . | . | 50 |
| App shell & project lifecycle | H2 | . | . | . | . | . | . | . | . | . | 3 | . | 1 | 1 | 5 |
| CAD & sketch | H0 | 14 | . | . | . | . | . | . | . | . | . | . | . | . | 14 |
| CAD & sketch | H1 | 79 | 4 | . | 3 | 2 | . | . | 1 | . | . | 1 | . | . | 90 |
| CAD & sketch | H2 | . | . | . | . | . | . | . | . | . | 5 | 1 | . | . | 6 |
| Construction & objects | H0 | 21 | 13 | . | 1 | . | . | . | . | . | . | . | . | . | 35 |
| Construction & objects | H1 | 25 | 16 | 1 | 1 | 3 | . | . | 1 | . | . | . | . | 1 | 48 |
| Construction & objects | H2 | . | . | . | . | . | . | . | . | . | 2 | 2 | . | . | 4 |
| Document, persistence & interchange | H0 | 14 | 12 | . | 2 | 3 | . | . | . | . | . | . | . | . | 31 |
| Document, persistence & interchange | H1 | 9 | 1 | . | 2 | 4 | . | . | 2 | . | . | . | . | . | 18 |
| Document, persistence & interchange | H2 | . | . | . | . | . | . | . | . | . | 1 | . | . | . | 1 |
| Freeform | H1 | 17 | . | 3 | 1 | . | . | . | . | . | . | . | . | . | 21 |
| Freeform | H2 | . | . | . | . | . | . | . | . | . | 1 | . | . | . | 1 |
| Portability | H0 | 12 | 2 | 8 | . | 2 | . | . | 1 | . | . | . | . | . | 25 |
| Portability | H1 | 3 | . | . | . | . | . | . | 1 | . | . | . | . | . | 4 |
| Portability | H2 | 2 | . | . | . | . | . | 1 | . | . | 22 | 2 | 1 | . | 28 |
| Renderer, picking & performance | H0 | 11 | 8 | 4 | 2 | 1 | . | . | 1 | . | . | . | . | . | 27 |
| Renderer, picking & performance | H1 | 5 | 4 | 3 | . | 1 | . | . | . | . | . | . | . | 1 | 14 |
| Renderer, picking & performance | H2 | . | . | . | . | . | . | . | . | . | 1 | 7 | . | . | 8 |
| Sculpt+cross-representation | H0 | 5 | 4 | . | . | . | . | . | . | . | . | . | . | . | 9 |
| Sculpt+cross-representation | H1 | 32 | 5 | 3 | 3 | 1 | . | . | . | . | . | 2 | . | . | 46 |
| Sculpt+cross-representation | H2 | . | . | . | . | . | . | . | . | . | 3 | . | . | . | 3 |
| Surface | H1 | 15 | . | 4 | . | . | . | . | . | . | . | . | . | . | 19 |
| Surface | H2 | . | . | . | . | . | . | . | . | . | 2 | . | . | . | 2 |
| Test & CI evidence | H0 | 31 | 11 | 2 | 7 | 2 | . | . | 7 | . | . | 2 | . | 1 | 63 |
| Test & CI evidence | H1 | 5 | 1 | . | 2 | . | . | . | . | . | . | 1 | . | . | 9 |
| **All** | **H0** | 117 | 74 | 18 | 15 | 13 | . | . | 10 | . | . | 2 | . | 2 | 251 |
| **All** | **H1** | 205 | 69 | 16 | 20 | 24 | . | . | 14 | . | . | 7 | . | 3 | 358 |
| **All** | **H2** | 2 | . | 1 | 1 | . | . | 1 | . | . | 43 | 17 | 4 | 2 | 71 |
| **All** | **NA** | . | . | . | . | . | . | . | . | . | . | . | 1 | . | 1 |
| **All** | **all** | 324 | 143 | 35 | 36 | 37 | 0 | 1 | 24 | 0 | 43 | 26 | 5 | 7 | 681 |

Reading notes:

- **Defects and regressions.**
  - **No regressions.** No row is REGRESSION-CONFIRMED.
  - **Defects are all H0 or H1.** All 37 DEFECT-CONFIRMED rows are H0 (13) or
    H1 (24). No H2 row carries a defect.
  - **Rows are not findings.** One finding can span several rows.
    - F-01 (48 dp hit areas) is 4 rows: `UI-011`..`UI-014`.
    - F-03 (Back in the bootstrap and the palettes) is 2 rows: `UI-009` and
      `UI-024`.
  - **Weakest-evidence defects.** E2-only defects are source proofs that
    were not reproduced. `PORT-031` rests on E1 host evidence; its arm64
    device consequence is E5.
- **NOT-IMPLEMENTED-BUT-DUE is 0.** Every H0 or H1 rule the seats traced has
  a code path. The gaps are missing guards or branches inside features that
  exist, and those are graded DEFECT-CONFIRMED, PARTIAL or DOC-DIVERGENCE
  (see "Unmatched requirements" below).
- **Anomaly from `totals.py`.** `ADV-007` is H2 and PARTIAL (a hostile
  `.forge` costs about 1 s of load CPU). It is a robustness bound with no
  owner record, not missing functionality.

## Unmatched requirements (documented, no implementing code path)

Each row below is an H0 or H1 statement in a repo record for which the seat
found no code that performs it. These are rule-level absences. They are not
whole features that are missing.

| ID | Horizon | Verdict | Documented statement (source) | What is absent |
| --- | --- | --- | --- | --- |
| `APP-026` (+`MOD-049`, `DOC-072`, `ADV-046`, `CAD-099`) | H1 | DEFECT-CONFIRMED | Creation refused below JNI and withdrawn above it while a sketch is drawn (CLAUDE.md CAD-R0-A1A2) | Import GLB has no sketch or chooser guard: `importGlbDurable` checks Sculpt only (`forgeshape_jni.cpp:8590-8597`), and the row is withdrawn only for Sculpt (`EditorWorkspaceView.java:2823`) |
| `CON-024` (+`TST-076`) | H1 | DEFECT-CONFIRMED | A face-supported CAD body is not independently transformable (CLAUDE.md CAD-A3, `isFaceSupportedCad`) | No gizmo, editor or `applyBoxTransform` refusal |
| `CON-093` | H1 | DOC-DIVERGENCE | `ARCHITECTURE.md:2606-2608` says `isFaceSupportedCad` makes the gizmo and editors leave the body alone | The guard the document names does not exist |
| `CON-025` (+`DOC-067`) | H1 | DEFECT-CONFIRMED | Export places every body where it is drawn (CLAUDE.md Export rule) | Export reads `body.transform()`, not `resolveWorldModel`, for a face-supported body |
| `UI-024` | H1 | DEFECT-CONFIRMED | System Back closes the topmost surface (CLAUDE.md APP-H1 "Back is one step") | Neither `SketchModifyView.paletteOpen` nor `CadExtrudeCanvasView.paletteOpen` is in `hasDismissibleSurface` |
| `UI-010` (+`CAD-115`) | H1 | DEFECT-CONFIRMED | Back is one step | `hasDismissibleSurface` (`:1112-1117`) has no Revolve term |
| `DOC-063` (+`MOD-042`) | H1 | DEFECT-CONFIRMED (recorded debt) | Sculpt never stands on a hidden active body (PROJECT_STATUS Stage027 GUARD-2 debt) | The load path enters Sculpt with no visibility check (`forgeshape_project_state.cpp:421-425`) |
| `DOC-084` | H1 | DEFECT-CONFIRMED | Save clears the recovery candidate (CLAUDE.md Autosave rule) | No ordering between an in-flight checkpoint write and Save's clear |
| `DOC-086` (+`RND-052`, `ADV-022`) | H0 | DEFECT-CONFIRMED | A checkpoint is written only when the project changed, and describes it (CLAUDE.md Autosave rule) | The fingerprint and the encode are taken at different instants |
| `ADV-013` (+`DOC-039`) | H0 | DEFECT-CONFIRMED | A failed write never destroys the previous valid copy (CLAUDE.md Autosave rule) | The kept `.pending` copy is never read back |
| `APP-041` (+`DOC-051`, `ADV-029`) | H1 | PARTIAL | `closeProject` refuses an open edit as load does | No edit-in-progress refusal in `closeProject` |
| `TST-029` (+`PORT-016`) | H1 | PARTIAL | Native domain checks gate every change (CLAUDE.md "fast native loop"; `docs/CI_CLOUD.md`) | `host-native-selftests.sh` is not run by CI FAST |
| `TST-027` | H0 | DOC-DIVERGENCE | Pushing a task branch runs CI FAST and CI DEVICE (CLAUDE.md CI-CLOUD-R1) | The push trigger is not implemented as stated |
| `APP-038` | H1 | BLOCKED-OWNER | Leaving a project with an open sketch: no recorded requirement asks the question | Owner decision 4 is open |

Recorded H2 absences (`FUTURE-NOT-DUE`, 43 rows; `BLOCKED-OWNER` H2, 17
rows) are not unmatched requirements. They are explicitly not due. Examples
are `CAD-109` (Intersect, Hole and fillet), `MOD-112` (NURBS and
T-Splines), `PORT-065` (a desktop client) and `UI-079` (mouse-only
navigation).

## Orphan implementations (code or UI with no requirement record)

| Item | Evidence | Grade | Row |
| --- | --- | --- | --- |
| Eight exported JNI natives with no caller in product Java, JVM tests or androidTest: `supportChooserSelect`, `supportChooserConfirm`, `freeformSelect`, `freeformClearSelection`, `sculptUndo`, `sculptRedo`, `sculptRedoAvailable`, `sketchToggleSelectEntity` | Seat J census (`SEAT_J.md` J-05). Re-checked in this synthesis: `grep -rn -E "\b<name>\(" app/src/{main/java,test,androidTest}` excluding `native` declarations gives 0 callers each, and each has 1 definition in `forgeshape_jni.cpp` | E2 | `ADV-062`, `ADV-058` |
| 66 natives never called from product Java (test seams). Two are used as device-test setup in place of the user's gesture: `sketchSetRevolveAxis` (`CadParametricHistoryOwnerTest:241`, `CadRetainedSketchOwnerTest:1262`) and `sketchToggleRegion` (`CadPlanarFaceRuntimeTest:529`) | `SEAT_J.md` J-05 | E2 | `ADV-064` |
| Test javadoc describes a product path that does not exist: `CadPlanarFaceRuntimeTest.toggleFace` claims the precision surface's row path | `SEAT_J.md` J-05; `SketchEditorView.java:538-545` | E2 | `ADV-064` |
| Freeform refuses edits on a locked body (`BodyLocked`). No rule states this, and `PRODUCT.md:537-542` says everything except moving still works | `SEAT_J.md` OD-J1; `ADV-053` | E2 | `ADV-053`, `MOD-067` (BLOCKED-OWNER) |
| `configChanges` lists `keyboardHidden` but not `keyboard`/`navigation`; no requirement states the intended unit or behaviour | `AndroidManifest.xml:31`; `SEAT_G.md` delib #13 | E2 | `PORT-056` (H2) |
| Freeform tap slop is 24 dp while the sketch's is 24 px; the code comment claims they are equal, and no requirement states the unit | `SEAT_G.md` (px vs dp slops) | E1 (probe) | `PORT-028` (DOC-DIVERGENCE) |

The debug key hooks (`ForgeShapeActivity.onKeyDown` → `debugMeshCommand`,
NDEBUG no-op) are not orphans. They have a record (`PORT-026` ← `APP-076`).

## Links

- Seat reports and their Round 2 sections: [SEAT_A](SEAT_A.md) ·
  [SEAT_B](SEAT_B.md) · [SEAT_C](SEAT_C.md) · [SEAT_D](SEAT_D.md) ·
  [SEAT_E](SEAT_E.md) · [SEAT_F](SEAT_F.md) · [SEAT_G](SEAT_G.md) ·
  [SEAT_H](SEAT_H.md) · [SEAT_I](SEAT_I.md) · [SEAT_J](SEAT_J.md)
- Seat matrices: `work/req_A.tsv` … `work/req_J.tsv`. Deliberations:
  `work/delib_*.md`.
- [SOURCE_REGISTER.md](SOURCE_REGISTER.md) ·
  [TEST_COVERAGE_MATRIX.md](TEST_COVERAGE_MATRIX.md) ·
  [UI_SCREEN_STATE_MATRIX.tsv](UI_SCREEN_STATE_MATRIX.tsv) ·
  [PORTABILITY_MATRIX.md](PORTABILITY_MATRIX.md) ·
  [VALIDATION_PLAN.md](VALIDATION_PLAN.md) · [TESTS.md](TESTS.md) (run ids)
