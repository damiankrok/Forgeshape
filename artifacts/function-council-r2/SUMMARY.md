# FUNCTION-COUNCIL-R2 — SUMMARY

**Result: `PARTIAL-PLAN-SOURCE-GAP`.** The repository, its tests and its runtime
were audited end to end and the dossier is complete. The canonical Master Plan
v4.1.61, `OWNER_DECISIONS_*`, `MASTER_PLAN_HISTORY_*`, `PROJECT_STATE` and
Master v4.1.0 were not available ([SOURCE_REGISTER.md](SOURCE_REGISTER.md)).
So compliance with the Master Plan is **not** claimed. Horizons H0/H1/H2 are
rebuilt from the repo's own OWNER-decision records, and the requirement matrix
is a lower bound on the planned scope.

This is a research audit. No product, test, schema, fixture, script, Gradle or
workflow file changed. Nothing was merged and no FullSharded aggregate was run.
Emulator evidence closes no physical-device gate.

## Audited tree

| Item | Value |
| --- | --- |
| Product tree | `70a6c14f703bfda2ab689b855126c1d12a541e41` (C2 head; `app/src/main` = `f3db953`) |
| Audit branch | `audit/function-council-r2-full-product-ux-platform-r1`, from exactly `70a6c14`; only `artifacts/function-council-r2/**` is added |
| Refs verified | `main` `6c9f156`, R2 `ea24d20`, C1 `08406cf`, C2 `70a6c14`; all match the prompt and none moved ([BEFORE.md](BEFORE.md)) |

## Fresh execution

Full detail is in [TESTS.md](TESTS.md).

| Gate | Result |
| --- | --- |
| Host native | `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25/25 suites, 17 geometry oracles |
| JVM | 196/0/0, forced re-run (`--rerun-tasks`) |
| Builds | debug, androidTest and release built |
| Release self-test guard | PASS |
| Host corpus verdicts | 72 fixtures |
| CI FAST `40796992679` | success; corpus parity 72/72 byte-identical |
| CI DEVICE (emulator) | `40797031704` 32/32, `40800569636` 77/77, `40805804385` 90/90; **199/199** fresh across 28 classes, each run 25/25 startup tokens in order |

Device coverage: 39 of the 70 instrumented classes have run on this product tree
(28 in this audit, 11 in C2). The other 31 have only historical runs on older
trees, so any claim about the whole suite stays `REQUIRES-FULLSHARDED-OWNER-GATE`.

Screenshots are kept under `evidence/device-run-*/` (146 PNG incl. 6 startup/post-test captures, with
SHA-256 sums).

## Council

Ten seats (A–J) ran independently ([SEAT_A.md](SEAT_A.md) … [SEAT_J.md](SEAT_J.md)).

**Seat status:**
- **COMPLETE:** E (document/lifecycle), G (desktop portability), I (test/CI).
- **PARTIAL:** A, B, C, D, F, H, J.

Every PARTIAL seat delivered its full scope. What remains open in each is an
environment limit named in its completeness line:
- no physical device or TalkBack;
- some Java-layer defects proven from source only, not reproduced at runtime;
- the plan-source gap.

No seat is hidden as complete.

**Round 2.** Each seat was challenged by a different seat. Every objection and
response is recorded in [COUNCIL_DELIBERATION.md](COUNCIL_DELIBERATION.md) and
`work/delib_*.md`, and the remaining disagreements are listed there.

**Adversarial verification** ([work/VERIFICATION.md](work/VERIFICATION.md)):
- Three lenses judged each of the top 30 of 38 P0–P2 findings.
- **All 30 survived.** They reduce to **23 distinct root causes**.
- **10 of the 12 strongest positive claims were judged overstated** at their
  stated grade. They are kept at E2 and were downgraded in the matrix.

## Capability matrix

[REQUIREMENTS_TRACEABILITY.md](REQUIREMENTS_TRACEABILITY.md) and
[requirements_traceability.tsv](requirements_traceability.tsv) hold **681**
atomic requirements: 880 seat rows with duplicates merged. Each requirement is
counted once.

**By horizon:** H0 251, H1 358, H2 71, and 1 evidence-gap row.

**By verdict:**

| Verdict | Count |
| --- | --- |
| PASS-REPRODUCED | 313 |
| PASS-SOURCE-AND-RECORDED-RUNTIME | 154 |
| IMPLEMENTED-UNTESTED | 35 |
| PARTIAL | 36 |
| DEFECT-CONFIRMED | 37 (H0 13, H1 24) |
| REGRESSION-CONFIRMED | 0 |
| INACCESSIBLE | 1 |
| DOC-DIVERGENCE | 24 |
| NOT-IMPLEMENTED-BUT-DUE | 0 |
| FUTURE-NOT-DUE | 43 |
| BLOCKED-OWNER | 26 |
| OUT-OF-SCOPE | 5 |
| UNDETERMINED-SOURCE-GAP | 7 |

Every H0/H1 rule found in the repo records has a code path. The gaps are
missing guards or branches inside features that exist, not missing features.
Supporting files:
- every user-facing control and refusal path: [FEATURE_INVENTORY.md](FEATURE_INVENTORY.md)
- 53 journeys (31 positive, 22 negative or fail-closed): [END_TO_END_JOURNEYS.md](END_TO_END_JOURNEYS.md)
- 49 discrepancies between documents and code: [CONTRACT_CONFLICTS.md](CONTRACT_CONFLICTS.md)

## Verified defects (de-duplicated)

There is no P0 and no regression. Full ranking and fix proposals are in
[PRIORITISED_BACKLOG.md](PRIORITISED_BACKLOG.md) and [OPPORTUNITIES.md](OPPORTUNITIES.md).

1. **P1 — GLB export puts a face-supported CAD body in the wrong place.** The
   exporter uses the stored identity transform instead of the drawn, derived
   placement. Location: `forgeshape_gltf_export.cpp` (`captureGlbExportScene`).
   Evidence: E1 host diagnostic; panel 3/3 twice (B-02 = E-01).
2. **P1, waits on OWNER decision OD-05 — Recovered work is protected nowhere.**
   After Recover, the checkpoint is deleted and both baselines are set.
   Location: `EditorWorkspaceView.java:5652-5687`. Evidence: E2; panel 3/3
   (A-02). The fix would invert the pinned test `FSR1B-05`.
3. **P2 — A face-supported CAD body accepts transform writes and the gizmo.**
   The write records an Undo step and dirties the project, but nothing moves.
   Location: `forgeshape_jni.cpp` (`applyBoxTransform`). Evidence: E1;
   B-01 and B-03.
4. **P2 — Import GLB is offered and accepted while a sketch or the support
   chooser is open.** Location: `importGlbDurable` (`forgeshape_jni.cpp:8586-8604`)
   and `EditorWorkspaceView.java:2823`. Evidence: E1; A-04 = C-02 = D-02 = E-03.
5. **P2 — Undo after an over-cap (NotRetained) sculpt stroke lands on a state
   that never existed and clears `hasEdits`.** Location:
   `forgeshape_sculpt_history.cpp` (`record`). Evidence: E1 (D-01, old D4).
6. **P2 — Session memory held in View fields is lost when the Activity is
   recreated.** Effects: the saved baseline is forgotten (old D3), the recovery
   question reappears over the live project, and Save Copy / Export results
   from the picker can be dropped. Location: `EditorWorkspaceView.java` and
   `EditorUiState.java`. Evidence: E2 (A-05 = E-04, E-02, A-06).
7. **P2 — Three canvas editors drop the decimal comma, so "2,5" becomes 25.**
   Evidence: E2 plus E1 on the parser (F-05).
8. **P2 — Navigator Flip and ±90° change the label but not the camera.**
   Location: `beginSketchView` reads `frame()`. Evidence: E1 probe (C-01 = F-02,
   old D2).
9. **P2 — System Back during an in-project Revolve backgrounds the app on
   API 33+.** Evidence: E2 (F-04). A related case: in the CAD bootstrap, Back
   discards the drawn sketch while a panel is open (F-03, waits on OD-08).
10. **P2 — Smaller items:**
    - chrome hit areas below 48 dp wide (Flip 36 dp, Outline chips 37–39 dp; E1 rects);
    - a renderer that stops mid-session is not reported until the next resume (H-01);
    - two self-test oracles fail under FP contraction, the NDK's arm64 default (G-01; E1 with `-mfma`);
    - the PROJECT device suite is at its 256-check cap, past which checks are dropped silently (I-01);
    - no CI workflow runs the 4392 native checks or the platform-neutrality probe (G-02);
    - heavy CAD derivation (275–322 ms on the host at the cap) and whole-mesh sculpt republishing run synchronously (H-05, H-06; physical cost `OWNER-PHYSICAL-UNVERIFIED`).

## Previous Council debt

Recheck against the current code: [COUNCIL_DEBT_RECHECK.md](COUNCIL_DEBT_RECHECK.md).

| Item | Status | Note |
| --- | --- | --- |
| D1 | CLOSED | `ImportGlbSculptGuardTest` ran 3/3 fresh |
| D2 | CONFIRMED-OPEN | Navigator does not move the camera |
| D3 | CONFIRMED-OPEN, wider | Any recreation, not only a palette change |
| D4 | CONFIRMED-OPEN | Undo after an over-cap stroke |
| D5 | CLOSED | Docs still list it open (CC-26) |
| D6 | PARTIAL | Clear Mask: reachable only for masks over ~87k vertices |
| D7 | PARTIAL | `closeProject` vs load guard asymmetry; a recorded disagreement, OD-37 |
| Import during sketch | CONFIRMED-OPEN | Defect 4 above |

## UX

Full audit: [ANDROID_UI_UX_AUDIT.md](ANDROID_UI_UX_AUDIT.md). Screen state
matrix: [UI_SCREEN_STATE_MATRIX.tsv](UI_SCREEN_STATE_MATRIX.tsv) (36 states).
Screenshot index: [SCREEN_EVIDENCE_INDEX.md](SCREEN_EVIDENCE_INDEX.md).

The evidence supports **six** high-friction journeys, not eight:
- navigator orientation;
- comma-decimal entry;
- Back in the bootstrap sketch with a panel open;
- Back during an in-project Revolve;
- the Sculpt status line covered by the brush column;
- TalkBack, where the selected state is not announced.

Transform plus exact values, Settings and the object commands score well.

Untested, and marked UNKNOWN or `OWNER-PHYSICAL-UNVERIFIED`:
- tablet, foldable, mouse, stylus hover and TalkBack;
- font scale 1.3 / 1.5 in this audit's runs (they last ran in C1/C2).

## Desktop (design only, H2)

Detail: [PORTABILITY_MATRIX.md](PORTABILITY_MATRIX.md),
[DESKTOP_PORT_STRATEGIES.md](DESKTOP_PORT_STRATEGIES.md) and
[DESKTOP_POC_GATE.md](DESKTOP_POC_GATE.md).

**What is already portable.** The domain C++ compiles and runs on the host
today: 25 host suites, and a 101-header standalone probe in which 100 pass.

**Android-bound parts:**
- the Java shell (~33k lines);
- JNI, 275 functions with process-scoped state;
- the renderer's surface, swapchain and logging seam.

**Shell options:**
- **S1:** a native per-OS C++ shell with no new library.
- **S2:** a JVM desktop shell over the existing JNI API.
- **S3:** a library-assisted shell. This is a candidate only and needs an
  OWNER licence and architecture gate.
- The cheapest parity step is the existing APK on large-screen or
  desktop-mode Android. It needs the wheel, button and hover input vocabulary
  first.

**Recommended future boundary.** A platform-neutral application core
(document context, gesture arbitration, load/close orchestration, autosave
decision) that sits between any shell and the domain. This is not an
authorisation.

## Owner decisions

All open decisions are in [OWNER_DECISIONS_REQUIRED.md](OWNER_DECISIONS_REQUIRED.md),
OD-01 … OD-37 (three numbers unassigned). The ones that block the most:
- **OD-27:** physical review of the C2 APK and the pending checklists.
- **OD-01 (A2, world-plane retained sketch):** still
  `BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH`. Option 0, the
  current refusal, is not a decision.
- **OD-03:** FullSharded aggregate.
- **OD-04:** desktop priority.
- **OD-05 … OD-10:** the Recover, Save and Back semantics.

## Recommended next bounded stage (not started)

**`CAD-FACE-SUPPORTED-PLACEMENT-R1`** closes root cause RC-01, defects 1 and 3
above, which covers export plus the transform and gizmo guard. Details are in
[PRIORITISED_BACKLOG.md](PRIORITISED_BACKLOG.md) under "Best next bounded stage".

**What it does not need:**
- no OWNER decision, no format or fixture change, no library and no CI change;
- it does not touch the PROJECT suite, which is at its cap.

**Acceptance:**
- face-supported bodies export at their derived placement;
- exports of every other project are byte-identical to before;
- a named refusal below JNI, with the Transform entry and gizmo absent above it;
- host 0 failed.

**Gate:** host self-tests, then focused CI DEVICE (`GlbExportTest`, the gizmo
class and the CAD-A3 classes). It merges nothing before the OD-27 verdict.

## Limitations

- **Plan sources:** see the result line at the top.
- **Physical device:** none, so everything physical is
  `OWNER-PHYSICAL-UNVERIFIED`.
- **Toolchains:** no MSVC (Windows claims are E5) and no `pwsh` locally (CI
  FAST covered the corpus build).
- **Device coverage:** 31 instrumented classes have no run on this tree.
- **E2-only defects:** several Java-layer defects are proven from source and
  were not reproduced on a device; each carries its grade.
- **Diagnostics:** the host programs behind the E1 reproductions are under
  `diagnostics/`. They are audit-only and are never built into the product.
