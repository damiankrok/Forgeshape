# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — Decision gates

Audit only. **Nothing here is decided, started, fixed or authorised.** This file
orders what the closeout and the previous dossier found, and names the gate in
front of each item. It does not choose an option for any OWNER decision.

Baseline: product tree `70a6c14` (`app/src/main` = `f3db953`), audited on branch
`audit/function-council-r2-closeout-r1` from `2e5d8fc`. `git diff 70a6c14 HEAD -- app/`
is empty (`BEFORE.md`).

**Inputs.** `CRITICAL_REPRO_MATRIX.tsv` and `CRITICAL_REPROS.md` (CR-01..CR-07, 49 rows),
`ALIAS_TO_ROOT_CAUSE.tsv` (251 aliases → `RC-nn`), `ANDROID_UX_REVIEW.md`,
`PLAN_COVERAGE_VERDICT.md`, `SOURCES_RECONCILED.md`, and the read-only previous dossier
`artifacts/function-council-r2/` (`PRIORITISED_BACKLOG.md`, `OPPORTUNITIES.md`,
`OWNER_DECISIONS_REQUIRED.md`).

**Evidence words.** Classification: REPRODUCED, SOURCE-CONFIRMED-NOT-REPRODUCED, REFUTED,
BLOCKED, OWNER-SEMANTICS-REQUIRED. Confidence: E1 reproduced in this closeout with output,
E2 source proof (file:line), E3 historic recorded run, E4 doc only, E5 inference, E0 none.
Every runtime result in this file is host or emulator. **No result here is physical-device
evidence**, and no emulator result closes a physical gate.

**Device runs in this closeout tree.**

- `evidence/device-run-40979514270/` is the coordinator's focused CI DEVICE run
  (`GlbExportTest`, `SpatialSketchTest`, `SketchExtrudeTest`, `CadCanvasExtrudeTest`,
  `CadPlanarFaceRuntimeTest`, `CadPlanarFaceOwnerCorrectionTest`). **Its result is not
  interpreted or claimed here**, per the task. Nothing below depends on it.
- `evidence/device-run-40983965648/` (`EditorWorkspaceGizmoTest`,
  `Ui3dStateCorrectionTest`, `EditorWorkspaceMobileTest`; emulator `ForgeShape_CI_API36`,
  x86_64, SwiftShader) records `run=73 failed=1`, 25/25 startup tokens in order, and one
  failure triaged in its `TRIAGE.md` as a stale count assertion (item 19 below). It is cited
  only as emulator evidence.

---

## Part 1 — Existing problems, one ranked list

Ranked by: (1) what the user loses (wrong data leaving the product or work lost, above
silent wrong input, above a guard inconsistency, above cosmetic); (2) strength of the
closeout evidence; (3) whether anything stands in front of it. Duplicates are merged by
root cause (`ALIAS_TO_ROOT_CAUSE.tsv`). "Prev." is the previous dossier's backlog rank
(`PRIORITISED_BACKLOG.md` §1, Class 1).

Gate vocabulary:

- **none** — the written contract already decides the expected behaviour; only stage
  authorisation is needed.
- **OD-xx** — an OWNER decision from `OWNER_DECISIONS_REQUIRED.md` must be made first.
- **unnumbered OWNER question** — raised by this closeout; no OD id is assigned here.
- **CI policy** — needs an OD-18 answer or a workflow change.
- **physical review** — only an OWNER physical-device session (OD-27) can settle it.

Every item additionally needs the OWNER to authorise the stage that fixes it, and nothing
merges to the v6 line or `main` before the OD-27 verdict (`PROJECT_STATUS.md` Next Stage).

### 1A. Defects and contract violations

| Rank | Problem | Root cause / aliases | Closeout evidence | Sev. | Prev. | Gate |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | GLB export writes a face-supported CAD Body at its stored transform (the origin) instead of where it is drawn; a cap and a side dependent export byte-identical files; producer moves do not reach the dependent | RC-01a; OPP-01, B-02 = E-01 | CR-01-S1..S6 **REPRODUCED E1**: 11 MISMATCH (every face-supported body); of the 13 independent bodies, 12 MATCH and 1 (the Imported Mesh in `F_mixed_cad_face_v2`, body 2, `kind=other`) is compared by translation only (`verdict=NO_SOURCE_REDERIVED (translation equal=1)`), so CR-01-S7 is REFUTED for the 12 compared by AABB; MISMATCH deltas 1.00–5.66 m (`max_abs_aabb_delta` 1.0000 for `S3_chain` body 2 up to 5.6561) on synthetic scenes and on four committed `CADB` v2 fixtures; S1/S2 GLB SHA-256 both `9934da55…` | P1 | 1 | **none** (CLAUDE.md "exports EVERY body", CAD-A3 derived placement, ARCH-OWNER-07; CR-01-S8 E2) |
| 2 | After Recover the recovered work is in neither the slot nor a checkpoint; a second process death leaves nothing; Recover → New Project asks nothing | RC-06b; OPP-G1, A-02 | CR-05-S2 **REPRODUCED E1** (component: real `AutosaveController` + `ProjectCheckpoint`; workspace calls `:5675-5679` transcribed, E2); CR-05-S3 E2 | P1 | 2 | **OD-05** (written texts conflict: `PRODUCT.md:193-194` and pinned FSR1B-05 vs CLAUDE.md "Autosave protects work") |
| 3 | A face-supported CAD Body accepts a typed transform write and a gizmo drag: +1 Undo, fingerprint moves, stored transform moves, drawn body does not; the gizmo pivot is drawn 3 m from the body; the junk transform reaches `.forge` and decodes; ARCHITECTURE.md describes a guard that does not exist | RC-01b, RC-01c; OPP-02, B-01, B-03 | CR-02-S4/S5/S6 **REPRODUCED E1** (`C1/C2 VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=0`); JNI and UI reachability S1–S3 and S7 E2 | P2 | 3 | **none** for the guard (CAD-A3 "NOT independently transformable"); **OD-16** only for what the decoder does with already-saved non-identity transforms |
| 4 | Sculpt Undo/Redo after a NotRetained (over-cap) stroke lands on mesh states that never existed; `hasEdits` reads false while the deformation stands; navigator row 0 is unreachable truth | RC-11; OPP-04, D-01 | CR-04-S1..S4 **REPRODUCED E1** (K3–K7 FAIL, nearest-state `maxAbs` 0.011–0.024, `match=-1`); S5 (the stroke applies and is named) REFUTED as a defect | P2 | 5 | **none** for the defect (quoted contract); the remedy (clear Undo vs refuse the stroke) is **OWNER-SEMANTICS-REQUIRED: D-OD1 / OD-22**. The previous dossier proposed clearing Undo as a default; that proposal is not an OWNER decision and the closeout takes none |
| 5 | Import GLB accepted while a sketch is drawn in an open project (body created, made active, Construction step recorded, fingerprint moved); the step cannot be undone until the sketch closes; the drawer row is shown | RC-02; OPP-03, A-04 = C-02 = D-02 = E-03 | CR-03-S1, S4 **REPRODUCED E1**; S2 E2; S3 (corruption) REFUTED E1; S6 control holds | P2 | 4 | **none** for the sketch half (CLAUDE.md CAD-R0-A1A2 refuses creation while a sketch is drawn; previous dossier "Not owner decisions"); **unnumbered OWNER question** for the support-chooser half (CR-03-S5: import and Construction Undo both run while the chooser is up) and for an optional `!hasProject()` refusal (CR-03-S7, direct call only; UI reach REFUTED, S8) |
| 6 | An Activity recreation re-offers recovery over the live project, and offers a saved, unchanged project as "Unsaved work found" | RC-04c; OPP-09, E-02 | CR-05-S5, S6 **REPRODUCED E1** (component); S7 (worker race) BLOCKED E5 | P2 | 6 | **none** |
| 7 | A recreation forgets that the project was saved: `projectDirty()` flips false → true with an unchanged fingerprint, and New Project asks the unsaved question | RC-04a; OPP-14, A-05 = E-04 | CR-05-S4 SOURCE-CONFIRMED-NOT-REPRODUCED, E2 (+E1 support) | P2 (fail-safe direction) | 6 | **none** |
| 8 | A Save Copy / Export destination chosen across a recreation writes nothing, shows no status and is logged as a cancel; an empty document is likely | RC-04b; OPP-15, A-06 | CR-05-S8 SOURCE-CONFIRMED-NOT-REPRODUCED, E2 (empty file E5). Correction: a palette change cannot trigger it | P2/P3 | 6 | **none** |
| 9 | Orientation navigator Flip and ±90° change the state and label but not the camera | RC-03; OPP-06, C-01 = F-02, UX-7 | CR-06-S3 **REPRODUCED E1** (`camera_view_identical_to_B0=1` for Flip, +90, −90) | P2 | 8 | **none** for the reset-on-reentry form; **OD-09** only if the user's flip/roll should survive Back to Sketch / Edit Sketch |
| 10 | Three canvas editors drop the decimal comma: "2,5" commits 25, "1 000,5" commits 10005 | RC-09; OPP-05, F-05, UX-4 | CR-06-S1/S2 SOURCE-CONFIRMED-NOT-REPRODUCED: filter and model legs E1 (`setExtrudeSide(true,25)=Ok`, `applyBodyDimension` scale 1 → 12.5), end-to-end E2 | P2 | 7 | **none** |
| 11 | System Back during an in-project Revolve with nothing open backgrounds the app on API 36 | RC-08b; OPP-08, F-04, UX-3 | CR-06-S4 SOURCE-CONFIRMED-NOT-REPRODUCED, E2 (API 33–35 E5) | P2 | 9 | **none** (in-project scope only) |
| 12 | "Save and continue" before Open Saved Project overwrites the only saved project, then reopens what was just saved | RC-06a; OPP-G2, A-01 = X-A01 | CR-05-S1 SOURCE-CONFIRMED-NOT-REPRODUCED + OWNER-SEMANTICS; file effects E1 in the host transcript (`A01_A_EXISTS_IN_NO_FILE=OBSERVED`) | P2 | 14 | **OD-06** |
| 13 | A renderer that stops mid-session is not reported until the next resume | RC-12; OPP-10, H-01 | Not re-run here; previous dossier E2, panel 3/3 | P2 | 10 | **none** |
| 14 | A device self-test suite silently drops checks past its cap; PROJECT is at 256/256, so a 257th failing check still prints `_SELFTEST_OK` | RC-13e; OPP-12, I-01 = H-13 | CR-07-S1 **REPRODUCED E1** (`SIM 257 checks (257th fails) … recorded=256 failed=0 device_token=FORGESHAPE_PROJECT_SELFTEST_OK`) | P2 latent | 11 | **none**; must precede any stage that adds PROJECT checks |
| 15 | Two self-test oracles fail under FP contraction (the arm64 NDK fuses even at −O0) | RC-13a; OPP-11, G-01 | CR-07-S2/S3/S4 **REPRODUCED E1** on host (`HOST_SELFTESTS_FAIL … 2 failed` under FMA; `fmadd` in arm64 −O0 and −O2 asm); arm64 device outcome E2 / E5, physical unverified | P2 (evidence) | 12 | **none** for tolerant/same-expression oracles; **OD-17** for a `-ffp-contract` pin |
| 16 | No CI workflow runs the host native suites or the corpus-verdict diff; CI DEVICE is x86_64 only | RC-13b; OPP-31, G-02 = I-05 | CR-07-S5 SOURCE-CONFIRMED-NOT-REPRODUCED, E2 (0 references in `ci-fast`, `ci-device`, `ci-full-sharded`) | P2 | 16 | **CI policy (OD-18)** |
| 17 | Chrome hit areas below 48 dp wide: `sketch_navigator_flip` 36.19 dp, `view_selection_outline_on` 36.95, `_off` 39.24, `sketch_dimension_label` 43.43 | RC-20; OPP-07, F-01, UX-8 | CR-06-S6 SOURCE-CONFIRMED-NOT-REPRODUCED: rects E3 (emulator, historic run 40797031704), cause E2 (`EditorControlStyles.chip` sets no minimum width) | P2 | 13 | **none** for the floor; **physical review** for reach and comfort |
| 18 | Selected Line's dimension label and its length field stand partly inside the orientation navigator (label 44.4 %, field 75.2 % of their area), and the label and editor overlap the Tool Rail's x-span (18 px, 147 px) | new in closeout (UX-2) | `ANDROID_UX_REVIEW.md` UX-2 SOURCE-CONFIRMED-NOT-REPRODUCED: rects from historic capture E3, overlap arithmetic E1, z-order E2 | P2 (UX) | — | **none** (CLAUDE.md: a surface may never partially cover another live control; the navigator may never stand on the Tool Rail); **physical review** to confirm on a real screen |
| 19 | `EditorWorkspaceMobileTest#uir4a04` asserts 8 Add-palette tiles; the product draws 12 since the Freeform/Surface tiles (2026-10-06) — a stale assertion no run caught for about 3.5 weeks | new in closeout (test harness) | `evidence/device-run-40983965648/TRIAGE.md`: E1 (emulator, `expected:<8> but was:<12>`) + E2 (commit dates); the other 72 tests of the three classes passed | P2 (coverage) | — | **none** for the test fix; any Tier-5 aggregate (**OD-03**) would fail on it until fixed |
| 20 | Core sketch device classes not re-verified on the audited tree | RC-14a; C-06 | Previous dossier partly closed it (`40800569636`). Coordinator: the focused CI DEVICE `40979514270` (head `f749a58`, product tree = `70a6c14`, emulator) ran the remaining classes `SpatialSketchTest` 8/8, `SketchExtrudeTest` 9/9, `CadCanvasExtrudeTest` 11/11, `CadPlanarFaceRuntimeTest` 6/6, `CadPlanarFaceOwnerCorrectionTest` 5/5 — coverage gap closed on the emulator (see `TESTS.md`) | P2 (coverage) | 17 | **none** (focused CI DEVICE); **OD-03** for any aggregate claim |
| 21 | CAD bootstrap Back with a panel open discards the drawn sketch and returns Home with no question | RC-08a; OPP-G3, F-03, UX-1 | CR-06-S5 OWNER-SEMANTICS-REQUIRED, behaviour E2 | P2 | 15 | **OD-08** |
| 22 | With the IME up in the CAD bootstrap a ghost "Body #0" Objects capsule appears and the rail is cut short | new in closeout (UX-9) | `ANDROID_UX_REVIEW.md` UX-9 observed in historic capture, E3 (not re-captured in this closeout; grade per `ANDROID_UX_REVIEW.md`); cause (`applyPrimarySurfaceChromePolicy` ignores `projectOpen`) E5 | P3 (UX) | — | **none** (APP-H1: nothing is drawn behind Home that fabricates a body) |
| 23 | A failed staged Surface edit names its verdict and Fix only outside the visible sheet | new in closeout (UX-5) | `ANDROID_UX_REVIEW.md` UX-5 observed in historic capture, E3 (grade per `ANDROID_UX_REVIEW.md`) | P3 (UX) | — | **none** |
| 24 | The saved project cannot be reopened from Home | RC-06c; A-03 → IDEA-05 | CR-05-S9 OWNER-SEMANTICS, E2 | P2 BLOCKED-OWNER | 18 | **OD-07** |
| 25 | Leaving discards an open sketch or staged edit without a question | RC-06d; A-07 | CR-05-S9 OWNER-SEMANTICS, E2 | P3/INFO | 19 | **OD-10** |
| 26 | Discard followed by a cancelled or refused Open leaves the live dirty project without a checkpoint until the next edit | RC-06f; E-08 | Not re-run; previous E2 | P3 | — | **OD-11** |
| 27 | At-cap Finish 275–322 ms synchronous under `g_stateMutex`; whole-mesh refresh and upload per stroke sample | RC-15a, RC-15b; OPP-40, OPP-39 | Not re-run; previous E1 host timing + E2 | P2 risk | 20, 21 | **OD-19** + **physical review** (release-ARM measurement) |
| 28 | Transform over a locked body is silent: rail highlights, no gizmo, no status, no lock cue on the capsule | new in closeout (UX-6) | `ANDROID_UX_REVIEW.md` UX-6 SOURCE-CONFIRMED-NOT-REPRODUCED for the missing cue, E2 + E3 (grade per `ANDROID_UX_REVIEW.md`) | P3 (UX) | — | **unnumbered OWNER question** (whether a cue is required under "Chrome reports, it does not instruct") |
| 29 | Sculpt status pill partly covered by the brush column | RC (F-06), UX-10 | `ANDROID_UX_REVIEW.md` UX-10 observed in historic capture, E3 (grade per `ANDROID_UX_REVIEW.md`) | P3 | — | **OD-34** |
| 30 | Freeform session and selection survive opening another project; the JNI load wrapper cancels the sketch and moves the camera before a refusal | RC-07a, RC-07b; OPP-13, OPP-16 | Not re-run; previous E1 (`diag_d2`) / E2 | P3 | 22, 23 | **none** (host tests need item 14 first: PROJECT is at cap) |
| 31 | A PlanarFaces chain that does not regenerate passes validation but fails load; the codec accepts two encodings of one face token | RC-16a, RC-16b; OPP-17, OPP-18 | Not re-run; previous E1 | P3 | 24, 25 | **none**; OPP-18 also touches `DATA_PACKAGE_SPEC.md` wording (format review, lineage formula unchanged) |
| 32 | Autosave reads fingerprint and bytes under two locks; Save clears the checkpoint with no ordering against a worker; `.pending` fallback never read | RC-05a..c; OPP-19 | Not re-run; previous E2, timing E5 | P3 | 26 | **none**; the HJ-1 part waits on **OD-30** |
| 33 | Controls drawn and then refused: Add/Cut on a full 16-record sketch table; Clear Mask over the per-entry cap; Freeform Symmetry / level over budget | RC-10a..c; OPP-20, OPP-21 | Not re-run; previous E2 | P3 | 27, 28 | **none** for withdrawal; **OD-14** only to change the table policy, **OD-23** only for a Clear Mask alternative |
| 34 | Load enters Sculpt over a hidden active body | RC-07d; D-04 / E-09 | Not re-run; previous E3 / E2 | P3 (recorded debt) | — | **OD-36** |
| 35 | Hidden CAD/Surface bodies stay editable; Lock means different things per representation; a locked face-supported dependent moves with its producer | RC-17b..e; J-01, B-06, CC-10, HJ-3 | Not re-run; previous E2 / E3 | P3 | — | **OD-12**, **OD-29** |
| 36 | Small native and UI hygiene: out-of-range Line length named `ZeroLengthLine`; Freeform/Surface rows seeded unitless; device performance line truncated at 1023 chars; uninitialised floats in two self-tests; Resume Sculpt has no native sketch guard; a test leaks `animator_duration_scale 0`; wrong picker-failure string; raw refusal token on New Freeform | OPP-22..28, OPP-30; RC-18a, RC-13c, RC-13d | Not re-run; previous E1 / E2 | P3 | 29–36 | **none** each |
| 37 | Derived names at the 96-byte cap truncated mid-suffix | OPP-29, B-05 | Not re-run; previous E2 | P3 | 37 | **OD-31** |
| 38 | `closeProject` does not refuse an open Construction edit while load does | RC-07c; D7 / E-10 | Not re-run; previous E2; no wrong outcome reached | INFO | — | **OD-37** |
| 39 | Allocator-only difference leaves a project "dirty" (Add Primitive + Undo) | RC-19; J-03, G-06 | Not re-run; previous E3 | INFO (consistent with today's rules) | — | **OD-13** |

### 1B. Quality inside current contracts (not defects)

From `PRIORITISED_BACKLOG.md` §2. None adds a feature, a format version or a library.

| Id | Item | Gate |
| --- | --- | --- |
| OPP-34 | Per-suite expected-count table so a suite that loses checks fails | **CI policy** (script only); after item 14 |
| OPP-35 | One session reset for load (after success) and close; absorbs item 30 | none |
| OPP-36 | Availability asks the same predicate the refusal does; absorbs item 33 | none |
| OPP-37 | Derive `hasDismissibleSurface` from the dismissal list | **OD-08** (bootstrap half), **OD-28** (mode half) |
| OPP-33 | Device taps through hit testing; 48 dp sweep helper; `KEYCODE_BACK` per phase | **OD-03** (it is a Tier-5 trigger) |
| OPP-41 | One documentation batch for stale counts and texts (CC-21..CC-49) | CC-12 text waits on **OD-18** |
| OPP-42 | Remove orphan natives and dead helpers; probe stubbed `debug*` exports | none |
| OPP-38 | Cache face-frame resolution by producer revision | none; after items 1 and 3 (same resolver) |
| OPP-44 | Shader/C++ constant parity; relative pick threshold; skip equal-extent rebuild | **OD-19** for the threshold only |
| OPP-43 | Condition waits instead of `sleep(250)` | none |
| OPP-45 | TalkBack selected state and a polite live region | **OD-21** |
| OPP-32 | Self-tests once per process | **OD-26** (precedes an affordable OD-03 run) |

---

## Part 2 — Future features (IDEA only, not defects, not authorised)

From `OPPORTUNITIES.md` Class 3 and `PRIORITISED_BACKLOG.md` §3. These are **not**
problems, are **not** proposed as the next stage, and are kept apart from Part 1 on
purpose. The closeout adds no evidence that makes any of them due.

| Id | Idea | Gate | Size | Closeout note |
| --- | --- | --- | --- | --- |
| IDEA-01 | World-plane retained sketch, A2 options 1–3 | **OD-01**; options 1–3 need `CADB` v9 or a new section (not authorised) | L–XL | No new evidence; today's refusal stays proved by `R2_SK_06` and D18 (`SOURCES_RECONCILED.md` §7 refutes the "D18 pointer looks wrong" note) |
| IDEA-02 | Desktop research spike (Linux first, viewport-only PoC) | **OD-04**, **OD-17**; licence gate for a library-assisted shell | L / XL | `DESKTOP_SCOPE_REVIEW.md`: neutral-domain compile REPRODUCED E1; every Windows step BLOCKED E0; variant choice OWNER-SEMANTICS |
| IDEA-03 | Android desktop-mode input (wheel, secondary button, hover, keyboard) | **OD-04**, **OD-20** | M–L | — |
| IDEA-04 | Multi-document context (per-document allocator) | **OD-13** | L | — |
| IDEA-05 | Open Saved Project from Home | **OD-07** (also Part 1 item 24) | S | — |
| IDEA-06 | Real-stylus hover preview in Sculpt | needs an autosave-suspend concept; **physical review** for hover | M | — |
| IDEA-07 | Draw-on-change frame pacing | **OD-19** | M | — |
| IDEA-08 | Release-variant runtime smoke; arm64 automated run | **CI policy (OD-18)** | S–M | CR-07-S4 shows arm64 fuses FP ops that x86_64 CI never exercises (E1 asm) |
| IDEA-09 | Accessibility specification | **OD-21** | M | — |
| IDEA-10 | Write-side `.forge` size limit | **OD-24** | S | — |
| IDEA-11 | Revolve Add/Cut, later Revolve, curved-surface trim, T-Spline refinement, general NURBS | each its own stage; most need a format version | XL | — |

Historical plan items the reconciliation found "required by an old plan and not done"
(UV MVP, OBJ export, Sculpt X symmetry and pressure, subdivide/remesh, snaps, sample
projects, cloud/account, …) are **not** added here as ideas or as defects: whether any is
due depends on `OQ-B` and the absent v4.1.61 (`PLAN_COVERAGE_VERDICT.md` §5–§6).

---

## Part 3 — OWNER decision register (status only; nothing decided)

Every OD below is **OPEN**. "Adds" is what this closeout's evidence contributes; "Still
blocked" is what cannot proceed until the OWNER answers. Ids, numbering gaps (OD-02, OD-15,
OD-25 never assigned) and meanings are kept exactly as in `OWNER_DECISIONS_REQUIRED.md`.

### The three named first

| OD | Decision | Closeout adds | Still blocked |
| --- | --- | --- | --- |
| **OD-27** | Physical-device review of the `CAD-R2-C2` APK (SHA-256 `262076926e8c…be9e`, `ci-fast-evidence` artifact of CI FAST `40787465884`) and the pending checklists (MODELING-R1 correction, MODELING-FOUNDATIONS-R1, drafting, Revolve; IMPORT-01B / UI-OWNER-45 Delete verdict; UI-OWNER-32 aesthetic verdict) | (1) The OWNER APK artifact **expires 2026-11-11T23:31:32Z** (`PLAN_COVERAGE_VERDICT.md` DLT-E016); after that it must be re-produced from the same tree. (2) Defects a physical session would meet at once, now with closeout evidence: navigator Flip/±90 (item 9, E1), sub-48 dp chips (item 17, E3), dimension label inside the navigator (item 18, E1 visual), ghost "Body #0" with the IME (item 22, E1 visual). The OWNER may choose to review before or after they are fixed. (3) Further pending verdicts the reconciliation found never recorded: Stage 018A, 020M and MIRROR-01 UX verdicts and the UI-PREF aesthetic verdict (DLT-E012..E015). Nothing in the closeout is physical evidence | Every merge of the v6 line to `feature/cad-v6-sketch-face-r1` or `main`; OD-03 timing; every physical-only item (stylus hover/pressure, hardware GPU, 16 KB pages, ARM64 timing, large font on a real screen) |
| **OD-01** | A2 world-plane retained sketch: option 0 (keep the hold), 1, 2 or 3 | No new behaviour evidence. Corrects one closeout extract note: D18 does cover the world-plane Keep Sketch refusal (`CadRetainedSketchOwnerTest.java:471-472`, E2), so `CAD-089`'s test citation stands. The decision bodies that might already answer A2 (current `OWNER_DECISIONS_*`, v4.1.61) are absent | IDEA-01; options 1–3 also need an authorised `CADB` v9 (or a new section), spec, builder parity and fixtures. The hold stays in force until decided |
| **OD-05** | Recover semantics: (a) keep, (b) unsaved until Save/Discard, (c) protected but not asked | Upgrades the mechanism from E2 to **E1 (component)**: after Recover the real `AutosaveController` skips the checkpoint (`writes=0 skippedUnchanged=1`), and a second death leaves `slot=absent ckpt=absent recoveryPrompt=false` (`A02_RECOVERED_WORK_LOST_ON_SECOND_DEATH=OBSERVED`). Control: Open File leaves the document checkpointed (`CONTROL_OPEN_FILE_IS_CHECKPOINTED`), an internal inconsistency with Recover. Pinned tests asserting the current behaviour: `ProjectAutosaveRecoveryTest.fsr1b05` `:322-323`, `ProjectProcessDeathTest.stage6` `:349` | OPP-G1 (Part 1 item 2, the only P1 behind an OWNER gate) |

### All other ODs

| OD | Decision | Closeout adds | Still blocked |
| --- | --- | --- | --- |
| OD-03 | When to run the Tier-5 FullSharded aggregate, and with which classes | A stale assertion (`uir4a04`, Part 1 item 19) would fail any aggregate today (`40983965648`, E1 emulator). The coordinator's focused run is not claimed. No aggregate was run | Merge to `main`; any "full suite" claim |
| OD-04 | Desktop priority and gate (G0-1, G0-2) | `DESKTOP_SCOPE_REVIEW.md`: Linux neutral-domain compile and headless reuse REPRODUCED E1; 275 JNI exports = 275 natives E1; every Windows/MSVC step BLOCKED E0; no variant judged easy | IDEA-02, IDEA-03, IDEA-04 |
| OD-06 | "Save and continue" before Open Saved Project | File effects E1 in the host transcript (`A01_SLOT_NO_LONGER_HOLDS_A`, `A01_A_EXISTS_IN_NO_FILE=OBSERVED`); UI path stays E2. `HomeFlowTest.e2eAppH1_10` covers Save and continue only for Open File | OPP-G2 (item 12) |
| OD-07 | Saved slot reachable from Home | No new evidence (E2) | IDEA-05, item 24, the CC-11 string fix under option (a) |
| OD-08 | Bootstrap Back order with a panel open | `ANDROID_UX_REVIEW.md` ranks it UX-1, the largest friction; behaviour E2 | OPP-G3 (item 21); bootstrap half of OPP-37 |
| OD-09 | Navigator view after Back to Sketch / Edit Sketch once Flip works | Item 9 is now **E1**; the control probe shows `viewFrame()` framing does move the camera, so a fix is detectable | Only option (b); option (a) proceeds without it |
| OD-10 | Is an open sketch or staged edit unsaved work on leave? | No new evidence (E2) | Item 25 |
| OD-11 | Protection after Discard + cancelled/refused Open | Not re-run | Item 26 |
| OD-12 | Hidden-body editing; what Lock holds | Not re-run. Closeout adds a related UX item (item 28: silent Transform over a locked body) as a separate, unnumbered question | J-01, B-06, CC-10 wording |
| OD-13 | Is an allocator-only difference a change? | CR-03 notes a post-Undo fingerprint difference consistent with the never-rolled-back `ObjectId` mark (E5, not claimed as a defect) | J-03, IDEA-04, G0-4 |
| OD-14 | Should kept sketches share the 16-slot table? | Not re-run | Only option (b); OPP-20 proceeds under (a) |
| OD-16 | Decoder policy for a non-identity stored transform on a face-supported body | **Now E1**: such a transform is written today (C1 encode `fnv=0xed440e2232e2d767`, `equalsPristine=0`) and decodes (`decode status=0 dependent stored pos=(5,0,0) rotZ=30`). It is inert for drawing (`worldMoved=0`) and, after item 1's fix, would be inert for export too | Only a decoder change (refuse / ignore / canonicalise); the recommended stage keeps today's decoder and does not pre-empt it |
| OD-17 | FP-contraction policy | **E1**: `-mfma` host runs give 2 failed; `-ffp-contract=off` control 0; arm64 NDK asm shows `fmadd` at −O0 and −O2; x86_64 CI never fuses | Only options (b) pin and (c) FMA CI job |
| OD-18 | CI policy (task-branch triggers, host suites in CI FAST, release smoke, arm64) | CR-07-S5 E2: 0 references to `host-native-selftests` or the corpus-verdict script in any workflow; CI DEVICE image is android-36 x86_64 only | OPP-31 (item 16), OPP-34, IDEA-08, CC-12 text |
| OD-19 | Performance budgets | Not re-run | OPP-39, OPP-40, IDEA-07, H-10 part of OPP-44 |
| OD-20 | Unit policy for interaction thresholds | Not re-run | CC-41 wording; IDEA-03 |
| OD-21 | Accessibility specification | `ANDROID_UX_REVIEW.md` states screenshots cannot show TalkBack; F-07 stays E2 | OPP-45, IDEA-09, F-08 palette change |
| OD-22 | NotRetained stroke: clear Undo (previous dossier's proposed default) or refuse | Item 4 is **E1** (K3–K7 fail; Redo also lands on a non-existent state, new beyond D-01). The closeout lists the remedy as OWNER-SEMANTICS-REQUIRED (D-OD1); the previous dossier's proposed default (a) is a proposal, not a decision | The remedy for item 4, under either option |
| OD-23 | Clear Mask over the per-entry cap | Not re-run | Only options (b) and (c) |
| OD-24 | Write-side `.forge` size limit | Not re-run | IDEA-10 |
| OD-26 | Run the self-tests once per process | `40983965648` reports 25/25 startup tokens in order on its first start (emulator); cost per start not re-measured | OPP-32; an affordable OD-03 run |
| OD-28 | System Back inside in-project modes | Not re-run (E2) | A-10 mode half; UI-083 wording |
| OD-29 | Lock on a face-supported dependent | CR-02 control D2 shows a dependent's world placement moves with its producer (`D2 dependent VERDICT … storedMoved=0 worldMoved=1`, E1) — the mechanism behind the question, not an answer | HJ-3 |
| OD-30 | Defer a checkpoint while a Construction edit is open | Not re-run | HJ-1 part of OPP-19 |
| OD-31 | Derived-name policy at the 96-byte cap | Not re-run | OPP-29 (item 37) |
| OD-32 | A sketch on another body's face (Add/Cut into the producer) | Not re-run | Nothing scheduled; closes FC-R1 Q3 |
| OD-33 | `surfaceDestroyed` bounded wait | Not re-run | CC-13 wording |
| OD-34 | Sculpt status-line layout vs `uilr120` | Item 29 observed in historic capture, E3 | F-06 |
| OD-35 | First-entry hints on the status line | Not re-run | F-12 |
| OD-36 | A Sculpt-kind file whose active body is hidden | Not re-run | D-04 / E-09 debt (item 34) |
| OD-37 | `closeProject` vs load guard asymmetry | Not re-run | Debt-list entry (item 38) |

### OWNER questions raised outside the OD list (no id assigned here)

These are recorded so they are not lost. This closeout assigns no OD number to them.

| Source | Question |
| --- | --- |
| CR-03 Q2 | Is the support chooser a creation-in-progress state (as Freeform treats it), so that Import and Construction Undo are refused while it is up? |
| CR-03 Q3 | Should `importGlbDurable` refuse when `!hasProject()` as defence in depth (not UI-reachable today)? |
| UX-6 | Is a cue required when Transform is chosen over a locked body? |
| DLT-E003/E004 | OBJ export: historical approved MVP (v4.1.0:189) vs CLAUDE.md "absent in both directions" |
| DLT-E008 | One-way Create Sculpt Copy (UI-OWNER-24) vs the shipped reversible workflow |
| DLT-E009 | Conditional Revolve-cap supports (v4.1.58 §B) vs "every face of a Revolve is ineligible" |
| DLT-E017 | UI-OWNER-25: should Import converge on Objects + (today: the project drawer)? |
| DLT-E018 | TEST-OWNER-02 budget stated twice with different numbers (CLAUDE.md 90/120 min aggregate vs `PROJECT_STATUS.md:2543-2550` 30/45 min closeout) |
| Plan | `OQ-B` (full MVP or tiering — decides H0), `OQ-C` / GATE-VIABILITY, `OQ-A` (UV half), `OQ-01`, `OQ-02`, `OQ-03/04/06/07/E`, release readiness (`PLAN_COVERAGE_VERDICT.md` §5) |

---

## Part 4 — Documents the OWNER must still supply

From `SOURCES_RECONCILED.md` §5–§6. None of these was reconstructed; the sixteen archive
files were not stitched together.

1. **The full canonical `FORGESHAPE_MASTER_PLAN_v4.1.61.md`, or an authoritative
   successor**, stating the current MVP (H0) and roadmap. Without it no full
   plan-compliance claim is possible (`PLAN_COVERAGE_VERDICT.md`: PARTIAL-PLAN-SOURCE-GAP;
   318 of 1,273 statements have no current authority, 172 of them depend on v4.1.61).
2. **The current `FORGESHAPE_OWNER_DECISIONS_*`, with decision bodies** (the archive carries
   72 titles only). Needed at least for OQ-B, OQ-C, OQ-A (UV half), OQ-03/04/06/07/E,
   UI-OWNER-24, -25, -26, -31, -34, -36. It may already answer some ODs above; if so, that
   record wins and the OD row closes with a citation.
3. **The full current `FORGESHAPE_MASTER_PLAN_HISTORY_*`** (2026-09-07 to 2026-10-29 is
   covered only by partial v4.1.58 and the addenda).
4. **`FORGESHAPE_PROJECT_STATE_*` between v4.1.0 and v4.1.66** (at least v4.1.0 and
   v4.1.58–v4.1.63; v4.1.64/65 survive only as archived blocks inside v4.1.66).
5. The other referenced-but-missing files: the `OQ-B` "proposed release tiering" text;
   `PROMPT_FORGESHAPE_FUNCTION_COUNCIL_R2_FULL_PRODUCT_PLATFORM_AUDIT.md`;
   `PROMPT_CAD_R2_ULTRACODE_RETAINED_SKETCH_PARAMETRIC_QUALITY.md` (a copy exists among
   earlier session uploads but is not in the archive and was not read);
   `CAD-R2-C2-OWNER-PHYSICAL-REVIEW-RESULT-GATE-R1` prompt;
   `FORGESHAPE_CAD_R0_DATA_DECISION_v1/v2/v3.md`, `FORGESHAPE_CAD_R0_UX_DECISION_v1.md`;
   `PROMPT_D_FORGESHAPE.md`, the SCULPT-H1 prompt v2, `FORGESHAPE_PROMPT_AUTHORING_RULES.md`;
   the UI-SPEC-R0 accepted mocks; and the OWNER's physical evidence (screenshots, videos,
   `1 lowpoly.glb`, the C1/C2 OWNER APK copies).
6. **The OD-27 physical-review result itself**, before the C2 OWNER APK artifact expires
   (2026-11-11T23:31:32Z) or with a re-produced APK after that.


## Coordinator addendum (after the focused CI DEVICE runs)

- Item 1 (CR-01): `GlbExportTest` ran **24/24 green** in CI DEVICE `40979514270`
  on this product tree, yet the defect is reproduced at E1 on the host — the
  class has no CAD Body and no face-supported case
  (`grep -niE "cad|topo|support" GlbExportTest.java` finds none), which is why
  existing tests miss it.
- Item 3 (CR-02): `EditorWorkspaceGizmoTest` ran **46/46 green** in CI DEVICE
  `40983965648`; none of its cases uses a face-supported CAD body.
- Item 19: classified TEST_HARNESS_DEFECT in
  `evidence/device-run-40983965648/TRIAGE.md` (stale count; authorised
  Freeform/Surface tiles), not a product regression.
- Items 18, 22, 23, 28, 29: grades aligned to `ANDROID_UX_REVIEW.md` (historic
  E3 captures, not re-captured), per the critic's finding.
