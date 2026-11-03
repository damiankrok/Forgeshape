# FUNCTION-COUNCIL-R2 — Owner decisions required

Audited tree `70a6c14` (branch `audit/function-council-r2-full-product-ux-platform-r1`;
`app/src/main` = `f3db953`). This file lists the choices that **only the OWNER** can make.
None of them is made here. The audit edited no product file and no owner document.

**Sources.** `OPPORTUNITIES.md` (OPP-/IDEA- cards), `PRIORITISED_BACKLOG.md` (order and
slices), `COUNCIL_DELIBERATION.md` (RC-nn, D-n disagreements), `CONTRACT_CONFLICTS.md` (CC-nn),
`RISK_REGISTER.md` (R-nn, §7 OWNER holds), `COUNCIL_DEBT_RECHECK.md`, the seat files
`SEAT_A.md` … `SEAT_J.md` and `work/VERIFICATION.md`. Fresh test executions and CI run ids are
owned by `TESTS.md`; evidence copies are under `evidence/`.

**Plan-source gap.** The canonical Master Plan, `OWNER_DECISIONS_*` and `MASTER_PLAN_HISTORY_*`
are not available (**PARTIAL-PLAN-SOURCE-GAP**, `BEFORE.md`). Horizons come from repo records
only. A decision below may already exist in those documents. If it does, that record wins, and
the matching row here should be closed with a citation.

**Verification.** `work/VERIFICATION.md` now exists. All 30 panel-verified P0–P2 findings
survived (at least 2 of 3 lenses each), giving 23 distinct root causes. 10 of the 12 strongest
positive claims were judged **overstated** at their stated grade. Earlier synthesis files said
the verification file was missing. That sentence is superseded. No finding they list was
refuted.

## Numbering

These ids are already cited in `OPPORTUNITIES.md`, `RISK_REGISTER.md` and the seat files.
Their numbers and meanings are kept exactly:

- **Global ids** (from `OPPORTUNITIES.md`): OD-01, OD-03 … OD-14, OD-16 … OD-24 and OD-26.
  OD-01 is A2. OD-04 is the desktop gate.
- **Gaps:** OD-02, OD-15 and OD-25 were never assigned. They stay **unassigned** and are not
  reused, so that no earlier citation can change meaning.
- **New ids** start after the highest existing number: OD-27 … OD-37.
- **Seat-local ids** map onto global ids as follows:
  - Seat J: OD-J1 → OD-12, OD-J2 → OD-29, OD-J3 → OD-13, OD-J4 → OD-30.
  - Seat A decisions: 1 → OD-07, 2 → OD-05, 3 → OD-06, 4 → OD-10, 5 → OD-28.
  - Seat E: OD-3 → none (the rule already answers it, see OPP-03); OD-4 → OD-24.
  - Seat B: decision 1 → OD-16 (decoder half), decision 2 → OD-12, decision 3 → OD-31.
  - Seat C: decision 1 → OD-01, decision 2 → OD-14, decision 3 → OD-32, decision 4 → OD-27.
  - Seat D: D-OD1 → OD-22, D-OD2 → OD-23, D-OD3 → OD-12, D-OD4 → OD-36, D-OD5 → recorded H2
    (IDEA-11 family, no decision due).
  - Seat F: decision 1 → OD-08 + OD-28, 2 → OD-35, 3 and 5 and 6 → OD-21, 4 → OD-04,
    7 → OD-27.
  - Seat G: decision 1 → OD-04, 2 → OD-17, 3 → OD-13, 4 → OD-20, 5 → OD-04, 6 → OD-18.
  - Seat H: decisions 1, 2 and 4 → OD-19; decision 3 → none (the GPU rule already says yes,
    OPP-10).
  - Seat I: decisions 1, 2 and 5 → OD-18; 3 → OD-26; 4 → OD-03.
- `CONTRACT_CONFLICTS.md` uses seat-local numbers in a few rows: "OWNER decision 1" in CC-06(a)
  is OD-08, "decision 2" in CC-04 is OD-05, and "decision 3" in CC-20 is OD-06.

## Summary (ordered by what each decision unblocks)

| OD | Decision | Kind | Unblocks | Urgency |
| --- | --- | --- | --- | --- |
| OD-27 | Physical-device review of the `CAD-R2-C2` APK and the pending checklists | verdict | every merge of the v6 line (`PROJECT_STATUS.md` Next Stage) | **now**: the one named next step |
| OD-01 | A2 world-plane retained sketch: option 0 (hold), 1, 2 or 3 | design | IDEA-01; the current hold stays in force until decided | with OD-27 (named together in `PROJECT_STATUS.md`) |
| OD-03 | When to run the Tier-5 FullSharded aggregate, and which classes it holds | gate | merge of the v6 line to `main`; any aggregate claim | before the v6 merge |
| OD-05 | Recover semantics against FSR1B-05 and `PRODUCT.md:193-194` | product | OPP-G1 (A-02, P1) | high |
| OD-06 | "Save and continue" before Open Saved Project | product | OPP-G2 (A-01, P2) | high |
| OD-08 | Bootstrap Back order when a panel is open | product | OPP-G3 (F-03, P2); scope of OPP-08 / OPP-37 | medium |
| OD-28 | System Back inside in-project modes | product | A-10 mode half; UI-083 | medium |
| OD-07 | Saved slot reachable from Home | product | A-03 (P2 BLOCKED-OWNER), IDEA-05, CC-11 | medium |
| OD-10 | Does an open sketch count as unsaved work when leaving? | product | A-07 | low–medium |
| OD-11 | Protection after Discard followed by a cancelled or refused Open | product | E-08 | low–medium |
| OD-09 | Navigator view after Back to Sketch / Edit Sketch | product | option (b) of OPP-06 only | low (option (a) needs none) |
| OD-12 | Hidden-body editing; lock semantics for Freeform and for every representation | product | J-01, B-06, D-09, CC-10 | medium |
| OD-18 | CI policy: host suites in CI FAST, task-branch triggers, release smoke, arm64 | CI | OPP-31, CC-12, CC-15, IDEA-08, OPP-41 (CC-12 text) | medium |
| OD-26 | Run the self-tests once per process (debug-path change) | product (debug) | OPP-32; an affordable OD-03 run | medium |
| OD-19 | Performance budgets: UI-thread block, stroke frame time, frame pacing, minimum pickable size | budget | OPP-39, OPP-40, IDEA-07, H-10 part of OPP-44 | medium |
| OD-17 | FP-contraction policy | build | option (b) of OPP-11; an FMA CI job; G0-3 | low (OPP-11 (a) needs none) |
| OD-04 | Desktop priority and gate (G0-1, G0-2) | strategy | IDEA-02, IDEA-03 | low (H2) |
| OD-13 | Is an allocator-only difference a change? | format/product | J-03, G-06, IDEA-04, G0-4 | low |
| OD-14 | Should kept sketches share the 16-slot table? | format policy | OPP-20 wording; any table change | low |
| OD-16 | Decoder policy for a stored transform on a face-supported body | format | only if the codec should change after OPP-02 | low |
| OD-20 | Unit policy for interaction thresholds (px vs density units) | UX policy | G-05, CC-41, IDEA-03 | low |
| OD-21 | Accessibility specification: TalkBack, form factors, focus-ring contrast | spec | OPP-45, IDEA-09, F-08, UI-047 | low |
| OD-22 | NotRetained stroke: clear Undo (default) or refuse the stroke | product | option (b) of OPP-04 only | low |
| OD-23 | What Clear Mask does over the per-entry cap | product | OPP-21 (Clear Mask half) | low |
| OD-24 | Write-side `.forge` size limit | format policy | IDEA-10 | low |
| OD-29 | Does Lock on a face-supported dependent constrain anything? | product | HJ-3 | low |
| OD-30 | Defer a checkpoint while a Construction edit is open? | product | HJ-1 | low |
| OD-31 | Derived-name policy at the 96-byte cap | product | OPP-29 | low |
| OD-32 | Confirm the model for a sketch on another body's face | product | FC-R1 §5 Q3 | low |
| OD-33 | `surfaceDestroyed`: bounded wait or unbounded | rule | CC-13, H-11 | low |
| OD-34 | Sculpt status-line layout against `uilr120` | layout | F-06, CC-49 | low |
| OD-35 | May first-entry hints use the status line? | UX rule | F-12 | low |
| OD-36 | A Sculpt-kind file whose active body is hidden | product | D-04 / E-09 (recorded debt) | low |
| OD-37 | `closeProject` vs load guard asymmetry (D7) | policy | E-10, D-8 | low |

---

## OD-27 — Physical-device review of the `CAD-R2-C2` APK and the pending checklists *(new)*

- **Context.** `PROJECT_STATUS.md` Next Stage (`:4851-4864`) names exactly one next step: the
  OWNER physical-device review of the `CAD-R2-C2` APK.
  - APK source: the `ci-fast-evidence` artifact `12860436405` of `CI FAST` `40787465884`.
  - APK SHA-256: `262076926e8cc3f90dc70425de4363d3ba3300adfc1998092d581cdffc40be9e`.
  - Test packs: `artifacts/cad-r2-c2-picking-seam/SUMMARY.md` (the orthographic seam tap) and
    `artifacts/cad-r2-c1-quality-gate/SUMMARY.md` §"OWNER physical test pack" (which includes
    the CAD-R2 retained-sketch checklist).
  - Still-pending checklists: `MODELING-R1-OWNER-CORRECTION`, `MODELING-FOUNDATIONS-R1`, the
    drafting toolkit and Revolve.
  - Pending verdicts: the `IMPORT-01B` / `UI-OWNER-45` Delete verdict and the UI-OWNER-32
    aesthetic verdict (seat B decision 4).
  - Physical-only items: stylus hover and pressure (MOD-115, UI-033, UI-081), a hardware GPU,
    16 KB pages, ARM64 timing of 3969-cell edits, and large font on a real screen (seat C
    decision 4, seat F decision 7).
- **Why only the OWNER.** `CLAUDE.md`: "Emulator CI closes no physical-device gate … a green
  `CI DEVICE` run is never cited as evidence for any of them". Every device result in this
  audit is the CI emulator (see `TESTS.md`).
- **Options.**
  - (a) Pass, with or without notes.
  - (b) Fail with named items. Each failure becomes a bounded correction stage.
  - (c) Pass part of it. Name the items that still block.
- **Consequences.** Until there is a verdict, nothing merges to `feature/cad-v6-sketch-face-r1`
  or `main` (`PROJECT_STATUS.md:4862-4864`). Engineering work on a task branch is still possible
  (see `PRIORITISED_BACKLOG.md` §6), but it cannot land.
- **Also relevant.** The audit found a defect class that a physical session would see at once:
  - F-01 / OPP-07: `sketch_navigator_flip` is 36.2 dp wide, 4 dp from two 48 dp arrows.
  - C-01 / OPP-06: Flip and ±90° do not move the camera.

  These are known before the review. The OWNER may choose to review after they are fixed.
- **Evidence.** `PROJECT_STATUS.md:4851-4870`; `SEAT_C.md` and `SEAT_F.md` "Owner decisions
  needed"; `SEAT_D.md` MOD-115.
- **Blocked until decided.** Any merge of the v6 line, and the Tier-5 timing (OD-03).

## OD-01 — A2: world-plane retained sketch

- **Context.** A retained sketch may stand only on a face of the body's own features. A second
  ROOT / world-plane sketch is refused (`SketchSupportInvalid`; CLAUDE.md `CAD-V6-S1` /
  `CAD-R2`). The state is `BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH`.
  - Proof of today's refusal: host check `R2_SK_06` and device D18. Keep Sketch is not drawn,
    and native refuses it.
  - Options are in `artifacts/cad-r2-c1-quality-gate/SUMMARY.md` §A2.
- **Options.**

  | Option | Data / format | Consequence |
  | --- | --- | --- |
  | **0. Keep the hold** | none | **Current behaviour, not a decision.** A construction sketch on XY/XZ/YZ inside a body still needs a face of the body. Recording "0" only confirms the hold. |
  | 1. Additional ROOT sketches (a world-workplane record owned by the body, with no feature) | `CadSketchRecord` gains a world placement for non-base records; `validateCadSketchTable` accepts more than one root; **new `CADB` v9**, written only when such a sketch exists | v1..v8 unchanged; an older build must REFUSE v9. UX risk: the body's transform carries a "world" sketch, which is really body-local. The C1 summary calls this the smallest format delta. |
  | 2. Body-local construction planes (an authored plane entity; a sketch stands on it by id) | a new entity type, **new `CADB` version**, a reference rule (delete refused while the plane is used) | as option 1, plus a new dependency edge to validate on load. Custom workplanes were "not this stage" before. |
  | 3. Project-level sketches (owned by no body) | a new project section beside `CADB` | Breaks "a project is its bodies". Home, History, the fingerprint, Objects, delete and duplicate all change. Largest option. |

- **Consequences.**
  - Options 1–3 each need `CADB` v9 or a new section. That means `DATA_PACKAGE_SPEC.md`
    layout, `scripts/build-forge-corpus.ps1` parity, new corpus fixtures (valid and refused)
    and the release guard.
  - `CADB` v9 is **not authorised** (`OPPORTUNITIES.md` Class 3).
  - This audit recommends no option.
- **Evidence.** `artifacts/cad-r2-c1-quality-gate/SUMMARY.md:52-70`; `SEAT_C.md` decision 1;
  `PROJECT_STATUS.md:4860-4862`; IDEA-01.
- **Blocked until decided.** IDEA-01, and any world-plane retained sketch. The refusal stays as
  it is.

## OD-03 — Tier-5 FullSharded aggregate: when, and with what

- **Context.**
  - `PROJECT_STATUS.md:4863-4864`: "The v6 branch merges to `main` only after the completed v6
    migration passes the Tier-5 aggregate once."
  - `CLAUDE.md` defines `-FullSharded` as the authoritative exhaustive path and
    `FULL_SHARDED_SUITE_PASS` as its only aggregate marker.
  - This audit ran three focused CI DEVICE dispatches (`TESTS.md`). None is an aggregate, and
    none may be cited as one.
  - Seat I (I-02, CC-16): the 25 suites re-run on every Activity start, about 9.5 s each, which
    was 304.4 s of 667 s in `40797031704`. That projects toward the 120-minute TEST-OWNER-02
    hard stop.
  - Harness changes (OPP-33) are Tier-5 triggers.
  - C-06 (survived 2/3): core sketch device classes were last run on older trees. Run
    `40800569636` re-ran `SketchUxTest`, `CadVerticalSliceTest` and `CadExtrudeExtentTest`.
    `SpatialSketchTest`, `SketchExtrudeTest`, `CadCanvasExtrudeTest` and the `CadPlanar*`
    classes were not re-run on this tree. (`CadMultiFaceOwnerTest` did run, in C2 on the
    identical product tree; `TESTS.md` §2.)
- **Options.**
  - (a) Run Tier-5 once, after the OD-27 verdict and after the OD-26 change. This is the
    cheapest attempt and the one most likely to fit the budget.
  - (b) Run it now on `70a6c14`, accepting the per-start cost and the
    `-OwnerOverrideAttemptLimit` rules.
  - (c) Defer it until a stage needs an aggregate claim.

  Also decide whether the diagnostic preview classes stay in the aggregate (seat I decision 4).
- **Consequences.** (a) orders the work: OD-26 → (optionally OPP-33) → Tier-5. Any source edit
  between those steps invalidates the earlier PASS (`TEST-RUNTIME-R1`). (b) risks a
  budget-stopped run.
- **Evidence.** `SEAT_I.md` I-02, I-06, I-08 and decision 4; `CONTRACT_CONFLICTS.md` CC-16;
  `work/VERIFICATION.md` C-06.
- **Blocked until decided.** The merge to `main`, and any claim worded "full suite". The stage
  recommended in the backlog uses no aggregate.

## OD-04 — Desktop priority and gate

- **Context.**
  - A desktop client is H2 and unauthorised (`ARCHITECTURE.md:195-201`, `ARCH-OWNER-01`).
  - `DESKTOP_POC_GATE.md` lists the preconditions:
    - G0-1: authorise a research spike and name its OS (Linux is cheapest).
    - G0-2: choose the shell family. S1 is native, S2 is JVM, and S3 is library-assisted;
      every S3 library needs its own gate.
    - G0-3: the FP policy (= OD-17).
    - G0-4: the allocator question (= OD-13).
  - G1-1 to G1-3 are already met on Linux x86-64 without FMA (E1).
  - Related: whether Android desktop windowing, ChromeOS, mouse and hardware keyboard are
    targets (F-10 = G-09, UI-079/080; seat F decision 4, seat G decision 5).
- **Options.**
  - (a) No desktop work now. This is the current state.
  - (b) Authorise only the bounded spike (G1 + G2 + G3 + A1–A8). No product claim, no release,
    and no Android byte may change (A7).
  - (c) Authorise Android desktop-mode input first (IDEA-03). It needs a new input vocabulary
    and OD-20.
  - (d) Both (b) and (c).
- **Consequences.** (b) needs a renderer surface-seam split (one explicit seam today, which
  CLAUDE.md keeps) and probably the OPP-31 CI job. (c) changes `forgeshape_input.h`, which is
  the platform-neutral boundary. Neither may start from this audit.
- **Evidence.** `DESKTOP_POC_GATE.md`, `DESKTOP_PORT_STRATEGIES.md`, `PORTABILITY_MATRIX.md`,
  `SEAT_G.md`; IDEA-02, IDEA-03, IDEA-04.
- **Blocked until decided.** IDEA-02, IDEA-03 and IDEA-04.

## OD-05 — Recover semantics (A-02, P1) *(Seat A decision 2)*

- **Context.**
  - `onRecoverRequested` reloads the checkpoint, **deletes** it, and sets both persisted
    baselines (`EditorWorkspaceView.java:5652-5687`: clear at `:5675`,
    `autosave.noteProjectPersisted` at `:5676`, `noteProjectPersisted` at `:5679`).
  - Autosave's skip rule then writes no new checkpoint until the next edit.
  - The recovered work is therefore in neither the slot nor a checkpoint. A second crash or
    kill before an edit or a Save loses it.
  - The panel upheld this 3/3.
  - CLAUDE.md says "Autosave protects work". But `PRODUCT.md:193-194` ("A project you just
    saved, opened or recovered is not asked about") and the pinned test FSR1B-05
    (`ProjectAutosaveRecoveryTest.java:321-322` asserts `!ProjectCheckpoint.exists` after
    Recover) describe the current code. The code comment says the behaviour is deliberate.
- **Options.**
  - (a) Keep the current semantics, and record the accepted risk in `PRODUCT.md` and
    `PROJECT_STATUS.md` Known Issues.
  - (b) Recovered work stays **unsaved** until Save or Discard. Keep the checkpoint, do not set
    the persisted baseline, and leaving asks Save / Discard / Cancel. This inverts FSR1B-05 and
    changes `PRODUCT.md:193-194`.
  - (c) Recovered work is **protected but not asked about**. Keep or re-write the checkpoint (do
    not mark the autosave baseline as persisted), but keep the "not asked about" UX. FSR1B-05's
    checkpoint assertion changes; `PRODUCT.md:193-194` holds.
- **Consequences.** (b) and (c) change a pinned device assertion, so they need a focused rerun
  of `ProjectAutosaveRecoveryTest` and `ProjectProcessDeathTest`. (a) leaves the only P1
  without a fix.
- **Evidence.** `SEAT_A.md` A-02 and Round 2 #2; `CONTRACT_CONFLICTS.md` CC-04;
  `COUNCIL_DELIBERATION.md` D-2; `RISK_REGISTER.md` R-01; `work/VERIFICATION.md` A-02.
- **Blocked until decided.** OPP-G1.

## OD-06 — "Save and continue" before Open Saved Project (A-01, P2) *(Seat A decision 3)*

- **Context.**
  - On a dirty project, Open Saved Project asks Save / Discard / Cancel.
  - "Save and continue" writes the single slot (`saveProjectToSlot`,
    `EditorWorkspaceView.java:5458-5471`) and then opens that same slot (`:2656-2657`). The
    "open" becomes a reload of what was just saved, and the previously saved project is
    destroyed.
  - CLAUDE.md APP-H1 lists Open Saved Project among the leaves that ask Save / Discard /
    Cancel, so the code follows the letter of the rule. The outcome contradicts
    `unsaved_prompt` ("nothing is replaced until you decide", `strings.xml:62`).
  - Upheld 3/3, and re-verified independently by seat J (X-A01).
- **Options.**
  - (a) Keep it. Single-slot overwrite is an accepted Known Issue
    (`PROJECT_STATUS.md:4381-4388`).
  - (b) Before Open Saved Project, offer only **Discard and open** or **Cancel**. Save is
    meaningless when the destination is the slot being opened. This needs a CLAUDE.md APP-H1
    wording change and a `PRODUCT.md` change.
  - (c) Keep Save, but reword it so the user sees that saving replaces the project about to be
    opened.
  - (d) Defer until a multi-slot or project library exists (out of scope).
- **Consequences.** (b) is the smallest product change, but it amends an APP-H1 sentence.
  `HomeFlowTest` and `EditorWorkspaceProjectActionsTest` need cases for it.
- **Evidence.** `SEAT_A.md` A-01; CC-20; R-02; `work/VERIFICATION.md` A-01, X-A01.
- **Blocked until decided.** OPP-G2.

## OD-07 — Saved slot reachable from Home (A-03) *(Seat A decision 1)*

- **Context.**
  - Home offers exactly New Project and Open File… (`PRODUCT.md:130-134`; APP-H1).
  - The internal slot (`getFilesDir()`, `ProjectSlot.java:63`) is reachable only from the
    project drawer, so only with a project open.
  - After a cold launch, the user's saved project cannot be reopened without first creating
    another project.
  - `home_prompt` ("Open a saved project file, or start a new one.") suggests a route that does
    not exist (CC-11).
  - Upheld 2/3; regraded to P2 BLOCKED-OWNER.
- **Options.**
  - (a) Keep two Home actions, and correct `home_prompt` (wording only).
  - (b) Add **Open Saved Project** to Home, shown only when `ProjectSlot.exists`.
  - (c) Make the slot one choice inside Open File….
- **Consequences.** (b) and (c) amend APP-H1's "exactly two" and `PRODUCT.md:130-134`, and
  change `HomeFlowTest` E2E-APPH1 cases. (b) is IDEA-05 (S).
- **Evidence.** `SEAT_A.md` A-03; CC-11; R-25; `work/VERIFICATION.md` A-03.
- **Blocked until decided.** IDEA-05, and the CC-11 string fix if (a) is chosen.

## OD-08 — Back in the CAD bootstrap while a panel is open (F-03, P2)

- **Context.**
  - `dismissTopmostSurface` tests the bootstrap (`EditorWorkspaceView.java:1177-1184`) before
    `topmostOpenSurface()` (`:1201`).
  - So System Back with a palette or editor open discards the whole drawn bootstrap sketch and
    returns to Home.
  - `PRODUCT.md:196-198` ("takes a New-CAD sketch back to Home") describes this.
    `ForgeShapeActivity.java:68-72` ("the innermost thing the user opened closes first")
    contradicts it.
  - Upheld 3/3. Disagreement D-5.
- **Options.**
  - (a) Keep the current order and amend the Activity comment.
  - (b) Innermost-first: Back closes the panel or palette, and the next Back goes Home.
  - (c) (b), plus a confirmation before discarding a bootstrap sketch that has drawn geometry
    (seat F decision 1).
- **Consequences.** (b) is the OPP-37 derivation applied to the bootstrap too. (c) adds a
  question that APP-H1's "costs nothing" wording does not mention. Either way, add a
  `KEYCODE_BACK` device case.
- **Evidence.** `SEAT_F.md` F-03 and decision 1; CC-06(a); CC-24 (stale comment); R-16.
- **Blocked until decided.** OPP-G3, and the bootstrap half of OPP-37. OPP-08 is not blocked:
  it is scoped to in-project phases.

## OD-09 — Navigator view after Back to Sketch / Edit Sketch, once Flip works (OPP-06)

- **Context.**
  - When the C-01 fix frames `viewFrame()`, view state is reset only in `begin()` / `cancel()`
    (`forgeshape_sketch_session.cpp:225,270`).
  - So Back to Sketch and Edit Sketch would reinstall the flipped or rolled view.
  - CLAUDE.md `CAD-UX-S1-C1` says those paths reinstall "the exact aligned view".
  - Disagreement D-6.
- **Options.**
  - (a) Reset view state on those paths. This keeps the CAD-UX-S1-C1 sentence and needs no
    decision.
  - (b) Keep the user's flip and roll across them. This amends CAD-UX-S1-C1.
  - (c) Withdraw the two controls until decided.
- **Consequences.** Choosing (a) lets OPP-06 proceed now. (b) changes an owner rule.
- **Evidence.** CC-01; `SEAT_C.md` C-01; `SEAT_F.md` F-02; R-15.
- **Blocked until decided.** Only option (b).

## OD-10 — Is an open sketch or staged edit unsaved work when leaving? (A-07) *(Seat A decision 4)*

- **Context.**
  - The dirty test is fingerprint-only (`EditorWorkspaceView.java:2597-2602`).
  - `closeProject` and `loadProject` cancel the sketch (`forgeshape_jni.cpp:8977-8980`,
    `:8478-8482`). Leaving with a drawn sketch discards it without a question.
  - This matches the sketch-volatility rule ("process death loses the sketch and nothing
    else").
  - Upheld 2/3; regraded BLOCKED-OWNER, P3/INFO.
- **Options.**
  - (a) Keep it. The sketch is volatile by rule.
  - (b) Treat a sketch with at least one entity, or a staged edit, as unsaved. Leaving asks;
    the fingerprint is untouched.
- **Consequences.** (b) adds a Java dirty term that is not part of the fingerprint. It must not
  reach autosave or the `.forge` bytes.
- **Evidence.** `SEAT_A.md` A-07; `work/VERIFICATION.md` A-07.
- **Blocked until decided.** Any change to the leave question.

## OD-11 — Protection after Discard, then a cancelled or refused Open (E-08)

- **Context.**
  - Discard clears the checkpoint before the picker opens (`EditorWorkspaceView.java:2680-2690`).
  - If the picker is cancelled or the file is refused, the old project stays live (by design).
  - Autosave's `checkpointedFingerprint` still equals the live project, so nothing is
    checkpointed until the next edit.
  - CLAUDE.md's "Discard retires the checkpoint" is literally met.
- **Options.**
  - (a) Keep it.
  - (b) Retire the checkpoint only once the replacement project is live.
  - (c) On cancel or refusal, re-arm autosave so that the next background re-checkpoints.
- **Consequences.** (b) or (c) is a Java-only change, with a device case in
  `ProjectAutosaveRecoveryTest`.
- **Evidence.** `SEAT_E.md` E-08 and Round 2 #7; `COUNCIL_DEBT_RECHECK.md` §4.
- **Blocked until decided.** The E-08 fix.

## OD-12 — Hidden-body editing, and what Lock holds *(= OD-J1, with B decision 2 and D-OD3)*

- **Context.**
  - A hidden CAD or Surface body can still be edited through History, Edit Sketch, Keep
    Sketch, Add/Cut, and the Surface value and Thicken controls (J-01). No solid candidate is
    drawn for it: `applyCadOperationPreviewLocked` substitutes only items already in the
    snapshot.
  - Freeform, Sculpt and Dimensions refuse a hidden body.
  - The gizmo and placement editor stay over a hidden active body (B-06).
  - Separately, Freeform refuses **every** cage edit on a locked body
    (`forgeshape_freeform_session.cpp:278`, `BodyLocked`). `PRODUCT.md:537-542` says lock
    refuses only moving ("Everything else still works"), and so does CLAUDE.md Stage 018A
    ("two named guards"). See CC-10.
- **Options (hidden).**
  - (a) Refuse CAD and Surface edits over a hidden body, below JNI and withdrawn above it, as
    Freeform does.
  - (b) Allow them, and draw the candidate for the hidden target.
  - (c) Keep the current behaviour and document it.
- **Options (lock).**
  - (a) Lock holds FORM as well as placement for every representation. Extend it, and amend
    CLAUDE.md, `PRODUCT.md` and the Stage 018A wording.
  - (b) Lock holds placement only. Narrow Freeform to match.
  - (c) Freeform stays the documented exception.
- **Consequences.** Any choice except "keep" touches several sessions plus their availability
  presentation. CC-10 needs a text change whichever option is picked.
- **Evidence.** `SEAT_J.md` J-01, OD-J1, C-3 and C-4; `SEAT_B.md` B-06; `SEAT_D.md` D-09; RC-17;
  `END_TO_END_JOURNEYS.md` N22.
- **Blocked until decided.** The J-01 refusal question, B-06, and the CC-10 wording.

## OD-13 — Is an allocator-only difference a change? *(= OD-J3 = G-06; G0-4)*

- **Context.**
  - `projectSemanticFingerprint` mixes `nextObjectId` (`forgeshape_project_state.cpp:753`).
    So Add Primitive followed by Undo leaves the project "dirty" (J-03).
  - Re-encoded bytes after Open depend on what the process loaded earlier (G-06).
  - CLAUDE.md says "the `ObjectId` allocator is never rolled back", which makes this
    consistent with the rules today.
  - Regraded BLOCKED-OWNER / FUTURE-NOT-DUE.
- **Options.**
  - (a) Keep it: the high-water mark is part of the document.
  - (b) Exclude an allocator-only difference from the dirty test, but keep it in the bytes.
  - (c) A per-document allocator inside a future document context (IDEA-04, desktop-driven).
- **Consequences.** (b) changes the fingerprint's meaning (the autosave coalescing rule). (c)
  is an architecture change across singletons.
- **Evidence.** `SEAT_J.md` J-03 and C-15; `SEAT_G.md` G-06; RC-19; R-49.
- **Blocked until decided.** J-03, IDEA-04 and G0-4.

## OD-14 — Should kept sketches share the 16-slot table? (C-03, D-10)

- **Context.**
  - Kept sketches and feature sketches share `kMaxCadSketches` (16).
  - Add and Cut are offered on a full table, and the preview then reports `SketchNotFound`
    (OPP-20 fixes the offer and the name inside the current policy).
- **Options.**
  - (a) Keep the shared table (OPP-20 only).
  - (b) Give kept sketches an independent bound.
- **Consequences.** (b) changes a `CADB` bound and needs a format review (spec, builder,
  fixtures).
- **Evidence.** `SEAT_C.md` C-03 and decision 2; R-18.
- **Blocked until decided.** Only option (b). OPP-20 proceeds under (a).

## OD-16 — Decoder policy for a non-identity stored transform on a face-supported body

- **Context.** Because of B-01, a `.forge` can carry a non-identity transform on a
  face-supported body. `validateProjectDocument` checks only `transformValid`
  (`forgeshape_project_document.cpp:739`). The value is inert for drawing, but it is
  fingerprinted.
- **Options.**
  - (a) Accept and ignore it. This is today's behaviour, and needs no change after OPP-02.
  - (b) Refuse it by name. This fails closed, but files written by the buggy build stop
    opening.
  - (c) Normalise it on load. This changes the bytes on re-save, so a re-save would not be
    byte-identical.
- **Consequences.** (b) and (c) are format-behaviour changes: spec text, possibly a refusal
  fixture through the PowerShell builder, and corpus parity.
- **Evidence.** CC-02 sub-question; `SEAT_B.md` decision 1.
- **Blocked until decided.** Nothing in the recommended stage, which takes (a) implicitly.

## OD-17 — FP-contraction policy *(G0-3)*

- **Context.** Two self-test oracles fail under FP contraction, which is the arm64 NDK default
  (G-01; host `-mfma` gives 2 failed, `-ffp-contract=off` gives 0). `PROJECT_STATUS.md:2447-2449`
  argues against pinning.
- **Options.**
  - (a) Tolerant or same-expression oracles, with no pin (OPP-11 (a); needs no decision).
  - (b) Pin `-ffp-contract=off` everywhere.
  - (c) Either, plus an FMA host job in CI (needs OD-18).
- **Evidence.** `SEAT_G.md` G-01 and decision 2; R-33.
- **Blocked until decided.** Only (b) and (c).

## OD-18 — CI policy

- **Context.**
  - (1) Workflows trigger on `main`, one infra branch, PRs to `main` and dispatch. A
    task-branch push is not tested, although CLAUDE.md `CI-CLOUD-R1` and
    `docs/CI_CLOUD.md:12` say it is (CC-12, I-03).
  - (2) No workflow runs the host suites or the corpus verdict script. Platform neutrality "must
    stay asserted" (`ARCHITECTURE.md:208`, CC-15, G-02 upheld 2/3).
  - (3) No release-variant runtime smoke, and no signing for one (I-10).
  - (4) CI DEVICE is x86_64 only; there is no arm64 run (PORT-032).
- **Options.** Decide each of (1)–(4) yes or no. For (1), either add task-branch globs, or
  change the CLAUDE.md and `CI_CLOUD.md` texts to say "dispatch".
- **Consequences.** (2) is OPP-31 (S, low risk, minutes of runner time) and is also desktop gate
  G1-1/G1-2. (1) changes runner spend. (3) and (4) are IDEA-08.
- **Evidence.** `SEAT_I.md` I-03, I-05, I-10 and decisions 1, 2, 5; `SEAT_G.md` decision 6;
  R-35.
- **Blocked until decided.** OPP-31, IDEA-08, and the CC-12 text in OPP-41.

## OD-19 — Performance budgets (physical device)

- **Context.**
  - No repo record states a budget.
  - Host measurements:
    - At-cap Finish takes 275–322 ms, synchronous under `g_stateMutex` (H-05, upheld 2/3).
    - A stroke at about 100k vertices triggers a whole-mesh refresh and upload per sample,
      about 20 ms on the host (H-06, upheld 2/3).
    - Rendering is continuous (H-07).
    - Triangles of 1e-8 m² or smaller are unpickable (H-10).
- **Options.**
  - (a) State numbers: the maximum UI-thread block at the arrangement cap, the stroke frame time
    at the 100k tier, the frame-pacing policy (continuous, or draw-on-change with explicit
    evidence frames), and the minimum pickable feature size.
  - (b) Measure release-ARM on a physical device first, then state numbers.
  - (c) No budget (keep these as risks).
- **Consequences.** Until there are numbers, OPP-39 and OPP-40 (L, high-risk threading or
  renderer work) cannot be judged done. IDEA-07 changes evidence captures that count presented
  frames.
- **Evidence.** `SEAT_H.md` H-05, H-06, H-07, H-10 and decisions 1, 2, 4; D-7; R-26..R-30, R-20.
- **Blocked until decided.** OPP-39, OPP-40, IDEA-07, and the H-10 part of OPP-44.

## OD-20 — Unit policy for interaction thresholds (G-05, CC-41)

- **Context.** Freeform's tap slop scales by density (`forgeshape_freeform_session.cpp:640`).
  The sketch's slop is a raw 24 px (`forgeshape_sketch_session.h:208-210`). They are 2.75×
  apart at density 2.75. No rule states the intended unit.
- **Options.**
  - (a) Density-independent units everywhere.
  - (b) Raw px everywhere.
  - (c) Per instrument, stated.
- **Consequences.** (a) changes the feel of sketch taps on dense screens, and the
  `FORGESHAPE_SKETCH_TAP` evidence. IDEA-03 and IDEA-02 need this answer.
- **Evidence.** `SEAT_G.md` G-05; CC-41; R-22.
- **Blocked until decided.** CC-41 wording; IDEA-03.

## OD-21 — Accessibility specification

- **Context.**
  - No rule requires TalkBack selected state or live regions (F-07, UI-073/074).
  - No rule names which form factors (tablet, foldable, split-screen) need runtime evidence
    (UI-047).
  - The Light Charcoal focus ring measures 2.48:1, below WCAG 1.4.11's 3:1, on an
    owner-approved palette (F-08).
- **Options.**
  - (a) Write a specification, then implement it (IDEA-09).
  - (b) Allow OPP-45 as hygiene now (`setSelected` plus a polite live region; low risk).
  - (c) No accessibility scope.
- **Evidence.** `SEAT_F.md` F-07, F-08 and decisions 3, 5, 6; R-24.
- **Blocked until decided.** OPP-45, IDEA-09, and the F-08 palette change.

## OD-22 — NotRetained stroke policy *(= D-OD1)*

- **Context.** OPP-04's default fix (a) clears Undo on a NotRetained stroke, so Undo cannot
  cross it, just as it cannot cross a Freeze. This needs no decision.
- **Options.**
  - (a) Clear Undo (default).
  - (b) Refuse the stroke. This contradicts CLAUDE.md "a stroke too large to retain still
    APPLIES", so it needs a rule change.
- **Blocked until decided.** Only (b).

## OD-23 — Clear Mask over the per-entry cap *(= D-OD2)*

- **Context.** Clear Mask refuses an entry over the cap (`EntryTooLarge`; CLAUDE.md
  `SCULPT-FCM-R1`). `canClearMask` omits the size test, so the control is drawn and then
  refused for a mask over about 87,371 vertices (D-05, CC-08).
- **Options.**
  - (a) Withdraw the control over the cap. This is the rule as written; OPP-21 does it.
  - (b) Allow a Clear Mask without Undo, after a confirmation.
  - (c) An erase mode. Invert, Grow, Shrink and Blur stay H2.
- **Consequences.** Under (a) the user has no way to remove such a mask except Undo of the
  strokes. (b) amends the SCULPT-FCM-R1 sentence.
- **Blocked until decided.** Only (b) and (c). OPP-21 proceeds under (a).

## OD-24 — Write-side `.forge` size limit *(= seat E OD-4, H-E2)*

- **Context.** The reader refuses files over 256 MiB. The writer has no limit, so Save could
  write a file this build cannot reopen.
- **Options.**
  - (a) Refuse at Save with a named status.
  - (b) Keep it as is.
- **Evidence.** `SEAT_E.md` decision 4; R-07; IDEA-10.
- **Blocked until decided.** IDEA-10.

## OD-26 — Run the self-tests once per process (I-02, CC-16 cost half)

- **Context.**
  - `start()` re-runs all 25 suites whenever the render thread is not joinable
    (`forgeshape_jni.cpp:2083-2126`). That was 32 starts and 32 runs in `40797031704`.
  - CLAUDE.md is met (never per frame), but the Activity comment "Started once per PROCESS" is
    false.
  - This is a debug-path product change.
- **Options.**
  - (a) A process-static "suites ran" flag. Optionally move the CAD_FEATURE performance blocks
    to host-only runs.
  - (b) Keep it, and correct the comment only (part of OPP-41).
- **Consequences.** (a) changes what each device test's startup proves: once per process, not
  once per test. The startup-token capture must still see all 25 tokens on the first start. It
  is the precondition for an affordable OD-03 run.
- **Evidence.** `SEAT_I.md` I-02 and decision 3; CC-16.
- **Blocked until decided.** OPP-32.

## OD-28 — System Back inside in-project modes (A-10) *(new; Seat A decision 5)*

- **Context.**
  - CLAUDE.md APP-H1: "Back is one step in every phase and the platform's own at Home".
    `PRODUCT.md:196` agrees.
  - Inside an in-project sketch with nothing open, in a Freeform cage edit, and in Dimensions,
    Back is not one step back in the mode.
  - `uilr106` asserts that the resting Construction workspace hands Back to the platform
    (UI-083, a DOC-DIVERGENCE).
  - The recovery-question half is not an OWNER question: `PRODUCT.md:196` already answers it.
- **Options.**
  - (a) Back leaves each mode one step (sketch → Cancel or "Back to …", Freeform → end the
    session, Dimensions → exit). Resting Construction stays the platform's.
  - (b) Keep it, and amend APP-H1 and `PRODUCT.md:196` to say which modes Back does not step
    out of.
- **Consequences.** (a) needs one ordered dismissal list (OPP-37) and `KEYCODE_BACK` device cases
  per phase (OPP-33).
- **Evidence.** `SEAT_A.md` A-10; `SEAT_F.md` decision 1; CC-06(c); CC-07; R-16.
- **Blocked until decided.** The A-10 mode half and the UI-083 wording.

## OD-29 — Lock on a face-supported dependent *(new; = OD-J2, HJ-3)*

- **Context.** A face-supported body's placement is derived from its producer. When the
  producer moves, a locked dependent moves too. It is unspecified whether that is intended.
- **Options.**
  - (a) Lock says nothing about derived placement. Document this.
  - (b) Refuse moving a producer that has a locked dependent.
- **Consequences.** (b) is a new guard on the gizmo and on the transform path.
- **Evidence.** `SEAT_J.md` OD-J2 (`:492`).
- **Blocked until decided.** HJ-3.

## OD-30 — Defer a checkpoint while a Construction edit is open *(new; = OD-J4, HJ-1)*

- **Context.** `encodeProject` can be read by the autosave worker while a
  `ScopedConstructionEdit` is open (HJ-1, E5).
- **Options.**
  - (a) Defer the checkpoint until the edit closes.
  - (b) Keep it as is. A checkpoint of mid-edit state is still a valid document.
- **Consequences.** (a) interacts with OPP-19 (RC-05, one native call for fingerprint and
  bytes). Decide this before OPP-19 is scheduled.
- **Evidence.** `SEAT_J.md` OD-J4 (`:495`).
- **Blocked until decided.** The HJ-1 part of OPP-19.

## OD-31 — Derived-name policy at the 96-byte cap *(new; Seat B decision 3)*

- **Context.** "copy" and "Mirror" names at the cap are truncated mid-suffix or dropped (B-05).
  A comment cites a non-existent `LengthFilter` (CC-45).
- **Options.**
  - (a) Truncate the base and keep the suffix.
  - (b) Refuse by name.
  - (c) Keep the current behaviour.
- **Blocked until decided.** OPP-29 (low stakes).

## OD-32 — A sketch on another body's face *(new; Seat C decision 3, FC-R1 §5 Q3)*

- **Context.** A sketch on body P's face offers New Body (a TopoRef dependent) and also Add and
  Cut into P (`operationAvailable`, `forgeshape_sketch_session.cpp:2927-2933`). CLAUDE.md states
  "A sketch on a planar face of a CAD body targets that body". The OWNER has not confirmed this
  model since FC-R1.
- **Options.**
  - (a) Confirm it as is.
  - (b) Restrict it.
- **Blocked until decided.** Nothing scheduled. A confirmation closes FC-R1 Q3.

## OD-33 — `surfaceDestroyed`: bounded wait *(new; CC-13, H-11)*

- **Context.** CLAUDE.md: `surfaceDestroyed` "must block until native code has released the
  `ANativeWindow`". The code waits at most 5 s (`forgeshape_jni.cpp:9487`) and logs `acked=0`
  on a timeout. Safety rests on the renderer's own window reference (E2 + E5).
- **Options.**
  - (a) State the bound and the refcount argument in CLAUDE.md.
  - (b) Wait without a bound. This carries an ANR risk.
- **Blocked until decided.** The CC-13 wording.

## OD-34 — Sculpt status-line layout against `uilr120` *(new; F-06, CC-49)*

- **Context.** In Sculpt, the brush column and the Tool Rail overlap the status line (F-06),
  against CLAUDE.md "a surface … may never partially cover another live control". The obvious
  column-offset fix collides with `uilr120`, which pins the column top to the rail.
- **Options.**
  - (a) Move the status line.
  - (b) Offset the column, and amend `uilr120`.
  - (c) Accept it as is.
- **Blocked until decided.** F-06.

## OD-35 — First-entry hints on the status line *(new; F-12)*

- **Context.** CLAUDE.md "Chrome reports, it does not instruct" forbids always-true messages.
  Some status-line text is instruction (F-12).
- **Options.**
  - (a) No hints. Remove the instructional text.
  - (b) Allow one-time first-entry hints, under a named rule.
- **Blocked until decided.** F-12.

## OD-36 — A Sculpt-kind file whose active body is hidden *(new; D-OD4; D-04 / E-09 recorded debt)*

- **Context.** The load path enters Sculpt without a visibility check
  (`forgeshape_project_state.cpp:421-425`). Stage027 GUARD-2 covers only Start / Resume
  (`PROJECT_STATUS.md:4263-4269` records this as debt).
- **Options.**
  - (a) Open such a file in Construction.
  - (b) Refuse the load by name.
- **Blocked until decided.** The D-04 debt.

## OD-37 — `closeProject` vs load guard asymmetry *(new; D7 / E-10, disagreement D-8)*

- **Context.** Load refuses an open Construction edit; `closeProject` clears it. All five callers
  are shell paths after the dirty guard, so no wrong outcome was reached.
- **Options.**
  - (a) Add the guard defensively.
  - (b) Record it as intended.
- **Blocked until decided.** Whether to keep it on the debt list (`PROJECT_STATUS.md:4249-4250`).

---

## Not owner decisions (recorded so they are not re-asked)

| Item | Why no decision is needed |
| --- | --- |
| Face-supported body in export (seat E decision 1) | CLAUDE.md "exports EVERY body" plus CAD-A3 derived placement answer it; OPP-01 (a) |
| Import during a sketch (seat E decision 3) | CLAUDE.md CAD-R0-A1A2 already refuses creation during a sketch; OPP-03 (a) |
| Recovery offer once per process, or only at Home (seat E decision 2) | `PRODUCT.md:2027` "asks — once" is the intent; the choice between (a) and (b) is implementation (OPP-09) |
| RestartRequired surfaced mid-session (seat H decision 3) | The CLAUDE.md GPU rule ("rather than presenting a black viewport") answers it; OPP-10 |
| Face-supported body must not be transformable (seat B decision 1, first half) | CLAUDE.md CAD-A3 is explicit; OPP-02 (a). Only the decoder half is OD-16 |
| OQ-01 (Directional Scale), OQ-02 (Sculpt dimensions) | Pre-existing open owner questions, unchanged by this audit, not renumbered |
| D-OD5 (Freeform/Surface → Sculpt stale-source rule) | H2, recorded; nothing offers it today |
