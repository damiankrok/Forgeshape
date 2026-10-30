# FUNCTION-COUNCIL-R2-CLOSEOUT-R1: Council challenge closeout

This is an audit-only closeout. No product, test, script, fixture, workflow, Gradle or format file was changed. The previous dossier, `artifacts/function-council-r2/`, was used read-only.

| | |
| --- | --- |
| Product tree | `70a6c14` (= `f3db953`) |
| Audit branch | `audit/function-council-r2-closeout-r1`, cut from `2e5d8fc` |
| Inputs | `work/seat_completeness.md`, `work/positive_claims.md`, `work/alias_counts.md`, `ALIAS_TO_ROOT_CAUSE.tsv`, `CRITICAL_REPRO_MATRIX.tsv` and `CRITICAL_REPROS.md` (all in this closeout); `artifacts/function-council-r2/work/VERIFICATION.md` |
| Classification words | REPRODUCED, SOURCE-CONFIRMED-NOT-REPRODUCED, REFUTED, BLOCKED, OWNER-SEMANTICS-REQUIRED |

**Confidence scale**

| Grade | Meaning |
| --- | --- |
| E1 | Reproduced in this closeout, with output |
| E2 | Source-derived proof (`file:line`) |
| E3 | Historic recorded run |
| E4 | Documentation only |
| E5 | Inference |
| E0 | No evidence |

**What this document does not rely on**

- **The coordinator's focused CI DEVICE run is not cited.** That run covers GlbExportTest, SpatialSketchTest, SketchExtrudeTest, CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest and CadPlanarFaceOwnerCorrectionTest. Its result was not visible when this was written.
- **No emulator result is treated as physical-device evidence.** Every E3 device figure below comes from an x86_64 API 36 emulator in CI DEVICE.
- **The canonical plan is absent.** The full `FORGESHAPE_MASTER_PLAN_v4.1.61.md` is not in the 16-file OWNER archive, and nothing here reconstructs it.

---

## 1. Seat completeness

The source is `work/seat_completeness.md`. It is an E2/E4 re-assessment from source and dossier reads, and no new run was made for it.

Each open reason is put in one of four categories:

- **(a)** closable by host or source work;
- **(b)** closable by a focused CI DEVICE run (emulator, never physical);
- **(c)** needs a physical device, TalkBack or the OWNER;
- **(d)** needs the missing canonical plan.

A seat with any open (c) or (d) reason stays PARTIAL. More code reading cannot close one.

| Seat | Original | Open reasons (category) | Closable now? | Final |
| --- | --- | --- | --- | --- |
| A, shell and journeys | PARTIAL | Plan gap (d). A-01, A-02, A-05 and A-06 are Java defects proven from source only (b/c). The A-02 fix needs OD-05 (c). | No | **PARTIAL** |
| B, objects and transform | PARTIAL | Four androidTest classes are still stale: Gizmo, History, Objects and SelectionOutline (b). B-01 and B-02 need device confirmation (b). B-06 needs the OWNER (c). Plan gap (d). | No. Four classes were closed by the repair run 40805804385 (emulator). | **PARTIAL** |
| C, CAD and sketch | PARTIAL | Five stale sketch classes (b). These are pending in the coordinator's focused run, which is not visible here. Physical gates (c). Plan gap (d). | No | **PARTIAL** |
| D, Sculpt, Freeform and Surface | PARTIAL | D-02 and D-03 need device repros (b). Physical stylus and performance (c). Plan gap (d). The device classes were closed by 40800569636 (emulator). | No | **PARTIAL** |
| E, document and lifecycle | COMPLETE | — | — | **COMPLETE** (unchanged) |
| F, Android UI/UX | PARTIAL | F-03 and F-04 need a runtime repro (b). F-05 depends on the IME (c). TalkBack, tablet and stylus (c). Guard-rerun provenance is a coordinator question (a). Plan gap (d). | No | **PARTIAL** |
| G, desktop portability (design-only scope) | COMPLETE | — | — | **COMPLETE** (unchanged) |
| H, renderer and performance | PARTIAL | ARM64, physical and thermal measurements (c). Validation-layer session (c). H-03 log truncation blocks device performance figures until the product is fixed (a). Budgets need the OWNER (c). Plan gap (d). | No | **PARTIAL** |
| I, test and CI | COMPLETE | — | — | **COMPLETE** (unchanged) |
| J, cross-cutting | PARTIAL | Device journeys are partly closed (b). A process-kill harness (b). The filesystem-failure path (a/b). J-01 refusal semantics need OD-J1 (c). Plan gap (d). | No | **PARTIAL** |

**Result: 3 COMPLETE (E, G, I) and 7 PARTIAL.** This is unchanged from the dossier.

- Every PARTIAL seat carries the (d) plan-source gap and at least one (c) reason.
- So none can be closed by host or source work, or by the pending focused CI DEVICE run.
- Even an all-green focused run would close only Seat C's stale-class reason, and only at emulator grade.

---

## 2. The eight P2 findings the dossier never panel-verified

In the dossier, `work/VERIFICATION.md` lines 63-76 lists H-07, H-08, I-01, I-02, I-03, I-05, I-06 and I-08 as "seat grade only".

In this closeout, each one got **two independent checks**. The verdict rule is:

- **UPHELD** only if both checks upheld it.
- **SPLIT** if one check upheld it and the other did not.
- **REFUTED** if neither check upheld it.
- **NOT_PANEL_VERIFIED** if fewer than two checks returned.

**Panel outcome: 8 of 8 UPHELD. There are no SPLIT, REFUTED or NOT_PANEL_VERIFIED findings.**

Category and severity follow these rules:

- Where the two votes agree on a category, it is adopted.
- Where they disagree, both categories are recorded. The mechanism-level category in §5 is then fixed by the stated rule there.
- Where the two votes give different severities, both are recorded as a range. The panel did not settle it, and this document does not pick one.

| ID | Title | Verdict | Category (vote 1 / vote 2) | Corrected severity (vote 1 / vote 2) | Best evidence the panel returned | Mechanism |
| --- | --- | --- | --- | --- | --- | --- |
| H-07 | The render loop never idles while a surface is attached | **UPHELD** (2/2) | PERFORMANCE_RISK / PERFORMANCE_RISK | P2 risk / P3 → **P2–P3** | E2 from source. In `forgeshape_jni.cpp:1822-1826`, the wait predicate includes `g_viewport.running`, which is true from attach to detach. Each pass retakes `g_stateMutex` and rebuilds the snapshot. This is SOURCE-CONFIRMED-NOT-REPRODUCED: there was no Vulkan device, so nothing was measured. FIFO/vsync paces the loop, so it is not a CPU spin. Battery and thermal cost are OWNER-PHYSICAL-UNVERIFIED. | RC-15c |
| H-08 | Face-supported bodies re-derive their face frame every frame (F-07) | **UPHELD** (2/2) | PERFORMANCE_RISK / PERFORMANCE_RISK | P3 / P2 risk (recorded debt) → **P2–P3** | **E1 (host x86_64, -O1)**, from `diagnostics/PANEL-H08-1/h08_diag.out`. The snapshot median for a CHAIN of n=8 is 166.7 µs, n=32 is 2551.4 µs and n=64 is 10427.1 µs. PLAIN bodies at the same counts take 3.6, 12.9 and 25.9 µs. **Cost grows quadratically with chain depth**, which the seat text did not state. Source: `forgeshape_scene.cpp:281-326, 359` and `forgeshape_cad_face.cpp:88-117`. **Citation fix:** PROJECT_STATUS.md:4697-4700, not :4679. No device timing exists. | RC-15d |
| I-01 | A device suite silently drops checks past its result cap; PROJECT is at 256 | **UPHELD** (2/2) | TEST_HARNESS_DEFECT / TEST_HARNESS_DEFECT | P2 latent / P3 → **P2–P3** | **E1**, from `diagnostics/I01-panel1/cap_probe.out`: `PROJECT cap=256 recorded=256 failed=0 device_line=SELFTEST_OK` and `cap=255 recorded=255`. The host harness still runs 256 checks at cap 8192. The probe copies the verdict logic in `forgeshape_jni.cpp:821-874`. The `if (n < max)` recorder pattern appears in 23 suites. Freeform and Surface clamp the returned count with `std::min`. No check is lost on this tree. CR-07-S1 agrees. | RC-13e |
| I-02 | The 25 suites re-run on every Activity start | **UPHELD** (2/2) | PERFORMANCE_RISK / PERFORMANCE_RISK | P2 / P2 → **P2** (debug and test cost only) | E2. In `forgeshape_jni.cpp:2083-2126`, the only guard is `thread.joinable()`. `NativeViewport_stop` (≈:10322-10335) joins the thread, and `ForgeShapeActivity.onDestroy` (≈:581-583) calls stop on every non-configuration destroy. Release builds compile the suites out (`#ifndef NDEBUG`). The literal CLAUDE.md contract ("run once from start(), never per frame") is met. Vote 1 adds a secondary P3 DOC_DIVERGENCE for the comment at `ForgeShapeActivity.java:34-36`. | RC-13f |
| I-03 | A task-branch push runs no CI, contrary to CLAUDE.md | **UPHELD** (2/2) | DOC_DIVERGENCE / DOC_DIVERGENCE | P2 / P2 → **P2** | **E1** (vote 1): the trigger blocks were parsed with PyYAML. `ci-fast.yml` and `ci-device.yml` fire on push only for `[main, infra/ci-cloud-r1]`, plus `pull_request: [main]` and `workflow_dispatch`. A push of `audit/function-council-r2-closeout-r1` returns `False`. `ci-full-sharded.yml` is dispatch-only. The code and the CI_CLOUD.md table agree. CLAUDE.md "push the task branch and let GitHub Actions test it" and `docs/CI_CLOUD.md:12` overstate. Which side changes is an OWNER choice. | RC-NEW-28 |
| I-05 | CI FAST does not run the native suites or the corpus-verdict diff | **UPHELD** (2/2) | **TEST_HARNESS_DEFECT / FUTURE_OPPORTUNITY** (categories disagree) | P3 / P3 → **P3** (down from P2) | E2. No `.github/workflows/*` step calls `host-native-selftests.sh` or `host-forge-corpus-verdicts.sh`. **Seat impact corrected:** the 25 suites do reach CI on every PR and push to main through CI DEVICE's ordered 25-token check, not "only through a dispatched CI DEVICE". The scripts call themselves developer evidence. What is missing is cheap device-free coverage and a committed expected-verdict baseline. | RC-13b (shared with G-02) |
| I-06 | No exhaustive aggregate covers the current tree | **UPHELD** (2/2) | DESIGN_HOLD / DESIGN_HOLD | P3 / P2 → **P2–P3** | E1 (vote 1) for the counts from git: 57 @Test classes at `fa0b6b4` and 70 now; 13 have never been in any aggregate. `git diff --stat fa0b6b4 f3db953 -- app/src/main` gives 153 files, +49358/−2066. E3 for the last aggregate, 36607747079 on `fa0b6b4` (PROJECT_STATUS.md:438/444/483). It is a deliberate Tier-5 hold: Tier 5 is required before the v6→main merge, and that merge is OWNER-gated. | RC-14a (shared with C-06) |
| I-08 | Most device touches cannot detect chrome standing over the target | **UPHELD** (2/2) | TEST_HARNESS_DEFECT / TEST_HARNESS_DEFECT | P3 / P3 → **P3** (down from P2) | E2. There are 482 `performClick()` call sites in 67 androidTest files. `SketchTestSupport.send()` dispatches straight to `viewport_surface` (vote 1 counts 140 call expressions; vote 2 counts 176 call lines in 26 files; the seat said 170). **Correction:** decor-root or hit-owner dispatch exists in about 10–12 classes, not 8. Coverage of chrome against chrome exists through `Rect.intersects`. | RC-14c |

**Net effect of the panel**

- **Verdicts.** All eight move from "seat grade only" to panel-verified UPHELD. None became a BUG.
- **Severity.**
  - Two are firm P3 (I-05, I-08).
  - Two stay firm P2 (I-02, I-03).
  - Four are recorded as P2–P3 because the votes disagree (H-07, H-08, I-01, I-06).
  - **No finding is P2 by both votes and also a product-behaviour defect.**
- **Category changes against `ALIAS_TO_ROOT_CAUSE.tsv`.** The map tagged I-02 TEST_HARNESS_DEFECT and I-06 TEST_HARNESS_DEFECT. Both I-02 votes say PERFORMANCE_RISK, and both I-06 votes say DESIGN_HOLD. §5 applies both changes. I-05's category disagreement does not move its mechanism, because RC-13b also carries G-02, which the dossier panel verified as TEST_HARNESS_DEFECT (2/3).
- **Evidence raised by the panel.**
  - H-08 and I-01 are now E1 on the host.
  - I-03 is E1 for the trigger parse.
  - The I-06 counts are E1 from git.
  - H-07, I-02, I-05 and I-08 stay E2.

---

## 3. The ten downgraded positive claims

The source is `work/positive_claims.md`. The dossier's skeptics judged 10 of the 12 strongest positive claims overstated (`work/VERIFICATION.md:44-61`). The two that stood were D's sculpt-source exclusion and D's exact Freeform Symmetry.

**The rule in this closeout:** a claim moves only where an existing test asserts it through the real path *and* that test ran on this product tree. No grade is raised by a diagnostic alone.

| # | Seat | Claim (short) | Full claim, final | Narrower sub-claim that ran on this tree | Refuted part (not re-run unless noted) |
| --- | --- | --- | --- | --- | --- |
| 1 | A | Shell phase re-derived from native after recreation | **E2**, SOURCE-CONFIRMED-NOT-REPRODUCED | Home and the CAD bootstrap are derived from native after `recreate()`, with an explicit `syncFromNative()`: **E3** (C2, `HomeFlowTest.e2eAppH1_12`) | Counter-case: E-02, recovery question after a recreation |
| 2 | A | Failed open is all-or-nothing | **E2** | At `loadProjectDocument`, the domain scene and history are untouched (FSR1A_07/13, IMP01A_20): **E1** host. A decode-failed Open File changes no native value: **E3** (D-90) | **REFUTED at the JNI entry point** (E-12: the sketch is cancelled before a post-decode refusal), E2 |
| 3 | B | Hidden excluded from draw and pick by one predicate | **E2** | Hidden ⇒ absent from the snapshot (obj018a_03, s027_06/07): **E1** host | — ("not picked" and "single predicate" are E2) |
| 4 | B | Gizmo drag is one pointer, one step; cancel or second pointer restores | **E2** | The GizmoSession single step and the cancel that restores nine values: **E1** host | The second-pointer arbitration is JNI-only. EditorWorkspaceGizmoTest has no run on this tree (E3 on an older tree only). |
| 5 | C | All CAD suites pass (155/66/118/638; 17 R2_OR; 17 PICK_C2) | **E2** | They pass on the host x86_64 `-O1` build: **E1**. **Count fix:** only PICK_C2_12..17 are in CAD_FEATURE; 01..11 are in PICKING. | **REFUTED under FP contraction** for `CADFC2_PF_03b…` (CR-07, E1 host emulation). Running on arm64 itself is BLOCKED. |
| 6 | C | All 39 CAD fixtures decode to recorded verdicts, refused by name | **E2** | All **52** CAD fixtures decode to their recorded verdicts (31 Ok, 20 InvalidSemanticValue, 1 UnresolvedReference): **E1** | Count "39" is **REFUTED** (it is 52). "By specific name" is E2: 20 of the 21 refusals carry only the coarse codec status. |
| 7 | E | Decoder and load fail-closed; no crash; refused load never mutates | **E2** | Codec plus `loadProjectDocument` on the named corrupt cases: **E1**. "No crash": **E3** (FC2 fuzz diagnostics, not committed tests) | **REFUTED at the JNI entry point** (E-12), E2 |
| 8 | E | Encoder canonical, matches committed corpus | **E2** | The writer is deterministic, and the in-code fixtures equal the committed digests: **E1**. On-disk match: **E3** (CI FAST parity 72/72), with E1 corroboration from REENC (43/44; `unknown_optional` is by design) | "Canonical" (a unique encoding) is **REFUTED**, E3 (E-06: `faceEdgeLocalIndex > 0xFFFF` is masked) |
| 9 | F | Home and New Project opaque, full-window; XY bootstrap | **E2** | Full-window pages with no editor chrome; New Project replaces Home; XY bootstrap with no chooser and no project: **E3** (D-77) | "Opaque" is asserted by no test (E2) |
| 10 | F | System Back closes the topmost surface via the platform key on API 36 | **E2** | A real `KEYCODE_BACK` closes Add Primitive in Construction and the app stays: **E3** (D-32, `uilr106`) | **REFUTED as a whole-product claim** (F-03 and F-04 counter-cases, E2) |

**Net result**

- **No full claim is raised.** All 10 stay at E2.
- **Narrower sub-claims at E1** (fresh host run, `HOST_SELFTESTS_OK (4392 checks, 0 failed)`): #2, #3, #4, #5, #6, #7 and #8.
- **Narrower sub-claims at E3** (FC2 or C2 emulator runs on the same product tree): #1, #2, #9 and #10.
- **Wording corrections:** #5 (where the PICK_C2 checks live) and #6 (52 fixtures, not 39).

---

## 4. Counting units, and the single number

The sources are `work/alias_counts.md` and `ALIAS_TO_ROOT_CAUSE.tsv`, which has 251 alias rows. All figures come from the recorded `diagnostics/ALIAS/count_aliases.out`.

The dossier's three numbers count **different units**, and none can be derived from another.

| Unit | One item is | Count |
| --- | --- | --- |
| A. DEFECT-CONFIRMED requirement rows | One expected behaviour graded broken | **37** (P1 2, P2 17, P3 18) → 33 mechanisms, 20 parent RCs |
| B. P0–P2 seat findings | One seat heading at its pre-Round-2 grade | **38** (30 panel-verified by the dossier + 8 verified in §2 here) → 29 mechanisms, 14 parent RCs |
| B′. The dossier's panel-verified root causes | Unit B's 30 panel rows after the dossier's own duplicate list | **23** (1:1 with 23 distinct mechanisms) |
| C. Mechanism (this closeout) | One faulty piece of logic at one code site, needing its own fix | 94 ids in use in `ALIAS_TO_ROOT_CAUSE.tsv`, of which 78 are live and 16 are hypothesis-only. **Pre-closeout findings only**: the five closeout-new findings in §5a carry no id there |

**Why 37 ≠ 38 ≠ 23**

- Unit A excludes P2 findings that were never graded DEFECT-CONFIRMED: OWNER holds, PARTIAL coverage gaps, risks and doc divergences.
- Unit A includes 18 P3 rows, which lie outside the P0–P2 band.
- Unit A has already folded cross-seat duplicates together.
- One mechanism can break several requirements. For example, RC-20 appears as 4 rows (UI-011..014) and RC-08a as 2 rows (UI-009, UI-024).
- Unit B′ counts only the panel-verified P0–P2 findings. It includes non-defects, such as A-03 and A-07 (OWNER holds), B-03 (a doc divergence), C-06, G-01 and G-02 (test or CI), and H-05 and H-06 (performance risks).

### The single number

> **33 unique current defect mechanisms.** These are mechanisms in category BUG whose status is current or narrowed, under 22 parent RC numbers.

**Scope of the 33:** it counts only the mechanisms in `ALIAS_TO_ROOT_CAUSE.tsv`, which maps the dossier's pre-closeout findings. It does **not** include the closeout-new findings that `DECISION_GATES.md` Part 1 marks "new in closeout". Three of those are provisionally BUG (UX-2, UX-5, UX-9; see §5a). Counting them gives **36**, but none has a mechanism id in the alias map, and UX-9's cause is only an E5 inference, so its identity as one mechanism is not proven.

- The panel results in §2 do not change it, because none of the eight was graded BUG.
- **Strict reading** (status current only, which excludes the 6 narrowed-only mechanisms RC-06e, RC-10a, RC-10c, RC-17a, RC-NEW-02 and RC-NEW-08): **27**.
- **Sensitivity:**
  - **+1** if the OWNER rules that "Autosave protects work" governs Recover (RC-06b, A-02).
  - **+1** if the OWNER rules F-03 a defect (RC-08a).
  - **−2** if latent defensive-guard gaps with no UI path are not counted (RC-17a, RC-NEW-08).
- **Confidence of the 33**, graded by the best alias of each, as in the map before the repair round: E1 9, E2 12, E3 12 (the repaired map gives E1 6, E2 15, E3 12 — see the coordinator note in §5).
  - Under the stricter reading that an E1 mechanism must have been reproduced end to end on the host (and not as one leg or a component transcription), RC-06a, RC-04a and RC-09 fall to E2. That gives **E1 6, E2 15, E3 12**.
  - The matrix itself grades CR-05-S1, CR-05-S4 and CR-06-S1/S2 as SOURCE-CONFIRMED-NOT-REPRODUCED end to end. See §6.
- **Severity of the 33** (the dossier's post-Round-2 grade, highest alias):
  - P1: 1 (RC-01a, GLB export of a face-supported body).
  - P2: 11.
  - P3: 21.

---

## 5. Final split by category (mechanism level, live, each mechanism counted once)

### Starting point

The figures come from `count_aliases.out` §5: BUG 33, DESIGN_HOLD 14, TEST_HARNESS_DEFECT 16, DOC_DIVERGENCE 6, PERFORMANCE_RISK 6 and FUTURE_OPPORTUNITY 5.

*Coordinator note (re-run after the repair round):* `count_aliases.py` was re-run on the repaired `ALIAS_TO_ROOT_CAUSE.tsv` (`python3 -I diagnostics/ALIAS/count_aliases.py /home/user/Forgeshape`, exit 0; `count_aliases.out` replaced). The repaired map already carries adjustments 1–3 below, so it now prints DESIGN_HOLD 15, TEST_HARNESS_DEFECT 14 and FUTURE_OPPORTUNITY 4 directly, BUG still 33, and best confidence per BUG mechanism **E1 6, E2 15, E3 12** (the stricter figure of §4 is now the map's own). The final split below is unchanged.

- Those add up to 80 entries.
- Two mechanisms were listed twice: RC-13f under both TEST_HARNESS_DEFECT and PERFORMANCE_RISK, and RC-19 under both DESIGN_HOLD and FUTURE_OPPORTUNITY.
- So the 80 entries cover **78 live unique mechanisms**.

### Adjustments, each stated

1. **RC-13f becomes PERFORMANCE_RISK only.** Both I-02 panel votes classify it as PERFORMANCE_RISK, and its other alias, H-15, already was. TEST_HARNESS_DEFECT goes from 16 to 15.
2. **RC-14a becomes DESIGN_HOLD.** Both I-06 panel votes say it is a deliberate Tier-5 hold behind the OWNER merge gate. Its other alias, C-06 (narrowed, stale classes), has the same cause. DESIGN_HOLD goes from 14 to 15 and TEST_HARNESS_DEFECT from 15 to 14. *Sensitivity:* if C-06's stale classes are instead counted as a test-currency defect, the mechanism moves back, giving TEST_HARNESS_DEFECT 15 and DESIGN_HOLD 14.
3. **RC-19 counts once, as DESIGN_HOLD.** Through J-03 it needs OD-J3 (the per-document allocator), and G-06's FUTURE_OPPORTUNITY tag (IDEA-04) is the form a fix would take. FUTURE_OPPORTUNITY goes from 5 to 4.
4. **RC-13b stays TEST_HARNESS_DEFECT,** despite I-05's split votes. The dossier panel verified its G-02 alias in that category.

### Final split

| Category | Live mechanisms | Hypothesis-only (not counted) |
| --- | --- | --- |
| **BUG** | **33** | 9 |
| **DESIGN_HOLD** | **15** | 1 |
| **TEST_HARNESS_DEFECT** | **14** | 2 |
| **DOC_DIVERGENCE** | **6** | 0 |
| **PERFORMANCE_RISK** | **6** | 3 |
| **FUTURE_OPPORTUNITY** | **4** | 1 |
| **Total** | **78** | 16 |

This split, like the 33 in §4, covers **pre-closeout findings only**: the mechanisms in `ALIAS_TO_ROOT_CAUSE.tsv`. The five closeout-new findings are listed separately in §5a.

**Where the eight findings from §2 land:**

- H-07 → RC-15c, PERFORMANCE_RISK.
- H-08 → RC-15d, PERFORMANCE_RISK.
- I-01 → RC-13e, TEST_HARNESS_DEFECT.
- I-02 → RC-13f, PERFORMANCE_RISK.
- I-03 → RC-NEW-28, DOC_DIVERGENCE.
- I-05 → RC-13b, TEST_HARNESS_DEFECT.
- I-06 → RC-14a, DESIGN_HOLD.
- I-08 → RC-14c, TEST_HARNESS_DEFECT.

`ALIAS_TO_ROOT_CAUSE.tsv` itself is **not** rewritten here. It still carries the pre-panel tags for I-02 and I-06, and this section records the correction.

### 5a. Closeout-new findings (not in `ALIAS_TO_ROOT_CAUSE.tsv`)

`DECISION_GATES.md` Part 1 marks five items "new in closeout". None of them appears in `ALIAS_TO_ROOT_CAUSE.tsv`, which this document does not rewrite, so none has a mechanism id there. The labels `CL-1`..`CL-5` below are local to this document. They are not alias-map ids. The categories are provisional, assigned here by the same rule as §5 (a rule breach the source shows is BUG; a question only the OWNER can settle is DESIGN_HOLD; a stale test is TEST_HARNESS_DEFECT).

| Local label | `DECISION_GATES.md` item | Finding | Provisional category | Severity (as `DECISION_GATES.md`) | Grade in the primary source |
| --- | --- | --- | --- | --- | --- |
| CL-1 | 18 | UX-2: the selected Line's dimension label and length editor stand partly inside the orientation navigator and over the Tool Rail's x-span | BUG | P2 (UX) | `ANDROID_UX_REVIEW.md` UX-2: SOURCE-CONFIRMED-NOT-REPRODUCED. E3 historic capture (run `40797031704`), E2 z-order, E1 only for the overlap arithmetic over the recorded rects |
| CL-2 | 19 | `EditorWorkspaceMobileTest#uir4a04` asserts 8 Add-palette tiles; the product draws 12 | TEST_HARNESS_DEFECT | P2 (coverage) | `evidence/device-run-40983965648/TRIAGE.md`: E1 (emulator run, `expected:<8> but was:<12>`) + E2 (commit dates). Not a product regression |
| CL-3 | 22 | UX-9: a ghost "Body #0" Objects capsule with the IME up in the CAD bootstrap | BUG | P3 (UX) | `ANDROID_UX_REVIEW.md` UX-9: observed in a historic run, E3 (run `40797031704`); cause (`applyPrimarySurfaceChromePolicy` ignoring `projectOpen`) E5, not reproduced |
| CL-4 | 23 | UX-5: a failed staged Surface edit names its verdict and Fix only outside the visible sheet | BUG | P3 (UX) | `ANDROID_UX_REVIEW.md` UX-5: observed in a historic run, E3 (run `40800569636`); not reproduced and not source-derived |
| CL-5 | 28 | UX-6: Transform over a locked body gives no cue | DESIGN_HOLD (OWNER-SEMANTICS-REQUIRED) | P3 (UX) | `ANDROID_UX_REVIEW.md` UX-6: SOURCE-CONFIRMED-NOT-REPRODUCED for the missing cue, E2 + E3 (run `40797031704`); whether a cue is required is the unnumbered OWNER question in `DECISION_GATES.md` |

**Grade disagreement, recorded rather than resolved.** `DECISION_GATES.md` items 18, 22, 23 and 28 grade UX-2, UX-9, UX-5 and UX-6 as "REPRODUCED ... E1". `ANDROID_UX_REVIEW.md`, which owns those findings, grades them as above: historic E3 captures that were not re-captured in this closeout, with E1 only for UX-2's overlap arithmetic. This document follows `ANDROID_UX_REVIEW.md`. *Coordinator:* `DECISION_GATES.md` was afterwards aligned to the same grades (see its addendum), so the disagreement is resolved in favour of the owning file.

**Totals if the closeout-new findings are added** (provisional, with no alias-map ids):

| Category | Pre-closeout (§5) | Closeout-new | Combined |
| --- | --- | --- | --- |
| BUG | 33 | 3 (CL-1, CL-3, CL-4) | 36 |
| DESIGN_HOLD | 15 | 1 (CL-5) | 16 |
| TEST_HARNESS_DEFECT | 14 | 1 (CL-2) | 15 |
| DOC_DIVERGENCE | 6 | 0 | 6 |
| PERFORMANCE_RISK | 6 | 0 | 6 |
| FUTURE_OPPORTUNITY | 4 | 0 | 4 |
| **Total** | **78** | **5** | **83** |

*Caveats:* each closeout-new finding is counted as one new mechanism. That is not proven for CL-3, whose cause is E5. CL-1's label width below the 48 dp floor is a separate symptom already carried by F-01 and is not counted again. The rail truncation seen with the IME up in UX-2 and UX-9 (capture 08) is the same capture and is not counted as a sixth item.

---

## 6. How the critical repros (CR-01..CR-07) change earlier grades

The source is `CRITICAL_REPRO_MATRIX.tsv`: 48 symptom rows, all built against product tree `70a6c14`.

**Rules applied**

- A CR result can confirm, narrow, refute or re-grade a finding **only at host grade**.
- **An E1 or E2 result never becomes runtime E3 automatically.**
- No host result stands in for a device result, and no emulator result stands in for a physical one.

### CR-01: GLB export of a face-supported body (B-02 = E-01, RC-01a)

- **Symptoms S1–S6 REPRODUCED, E1 host.** `cr01_export_face.out` has 11 MISMATCH lines covering all the face-supported bodies, and `glb_sha256.out` shows byte-identical cap and side exports.
- **S7 control REFUTED, E1.** Independent bodies are exported correctly: 12 MATCH.
- **S8 SOURCE-CONFIRMED-NOT-REPRODUCED, E2.** The expected behaviour is contractual and needs no new owner semantics.
- **Change to earlier grades:**
  - The finding is re-reproduced on this tree at E1 host, and its **scope narrows**: only face-supported bodies are misplaced, never independent ones.
  - Severity stays P1.
  - GlbExportTest asserts current behaviour, not this defect. The coordinator's run of it is not cited, so there is **no device grade**.

### CR-02: Transform of a face-supported body (B-01 → RC-01b; B-03 → RC-01c)

- **S4–S6 REPRODUCED, E1 host.** A typed write or a gizmo drag records `undoDelta=1`, moves the fingerprint and the stored transform, and leaves `worldMoved=0`. The invisible edit reaches `.forge` bytes.
- **S1–S3 SOURCE-CONFIRMED-NOT-REPRODUCED, E2.** The JNI guards are absent, and the UI reachability is **not** upgraded to runtime.
- **S7 (B-03) E2.** It stays DOC_DIVERGENCE.
- **S6 is an instance of the existing OD-16, not a new question.** It is OWNER-SEMANTICS-REQUIRED for already-saved files: should the decoder refuse, ignore or canonicalise a stored transform that is not the identity? `DECISION_GATES.md` item 3 and its OD-16 row, and `PROPOSED_NEXT_SINGLE_STAGE.md`, map it to OD-16. CR-02-S6 makes OD-16's premise E1: such a transform is written and decodes today.
- **Change to earlier grades:** B-01's domain half is E1 host. Its UI half stays E2.

### CR-03: Import GLB while sketching (A-04 = C-02 = D-02 = E-03, RC-02)

- **S1, S4 and S6 REPRODUCED, E1 host.**
- **E-03 was graded E2 by seat E.** It is now **E1 host**, the same as its duplicates.
- **S3 REFUTED, E1.** Importing does not corrupt the sketch, the encoding or the later commit. This bounds the severity at P2 and gives no reason to raise it.
- **S8 REFUTED, E2.** The bootstrap import is not reachable from the UI, so S7 (the direct-call bootstrap import) is a **latent** defensive gap only.
- **S5 OWNER-SEMANTICS-REQUIRED.** It asks whether the support chooser is a creation-in-progress state.
- **Change to earlier grades:** E-03 rises from E2 to E1 host. The finding's scope is unchanged.

### CR-04: Sculpt NotRetained undo (D-01, RC-11)

- **S1–S4 REPRODUCED, E1 host.** `cr04.out` gives `RESULT failures=9` in the subject scenarios, while the controls pass.
- **S5 REFUTED, E1.** It is a control claim: the oversized stroke does apply and is named NotRetained.
- **Side note:** `strings.xml:613` says "cannot be undone" while Undo stays enabled.
- **Change to earlier grades:** D-01 is re-confirmed at E1 host. The fix form needs OWNER decision D-OD1.

### CR-05: Persistence lifecycle (A-01, A-02, A-05 = E-04, E-02, A-06, A-03/A-07)

The matrix is **more conservative than the map's per-alias E1** in two places.

| Symptom | Finding | Matrix grade | Note |
| --- | --- | --- | --- |
| S1 | A-01 | SOURCE-CONFIRMED-NOT-REPRODUCED + OD-06; E2, with E1 file effects in the host transcript | The workspace flow is a line-cited transcription with Android shims, not the View or Activity runtime. **The end-to-end grade is E2.** |
| S4 | A-05 / E-04 | E2, with E1 support | The same caveat applies. **The end-to-end grade is E2.** |
| S5, S6 | E-02 | REPRODUCED (component), E1 | Stands as reproduced at component level. |
| S2, S3 | A-02 | REPRODUCED or SOURCE-CONFIRMED-NOT-REPRODUCED, each + OD-05 | Remains **DESIGN_HOLD**: the written texts conflict, so it is not classified a defect. |
| S7 | E-02c | **BLOCKED**, E5 | The worker race was not run. |
| S8 | A-06 | SOURCE-CONFIRMED-NOT-REPRODUCED, E2; the empty file is E5 | **Correction:** a palette change cannot trigger A-06. Only a system configuration change or process death can. |
| S9 | A-03 / A-07 | OWNER-SEMANTICS-REQUIRED | Needs OD-07 and OD-10. |

**Change to earlier grades:** none of these is promoted to runtime. §4 gives the stricter E1 count that follows from this.

### CR-06: Android chrome (C-01 = F-02, F-05, F-04, F-03, F-01)

| Symptom | Finding | Result | Grade |
| --- | --- | --- | --- |
| S3 | C-01 / F-02 | REPRODUCED: `camera_view_identical_to_B0=1` | E1 host native |
| S1, S2 | F-05 | SOURCE-CONFIRMED-NOT-REPRODUCED end to end; the parser and model legs are E1 | **E2 end to end** (the map's E1 for RC-09 is a leg grade) |
| S4 | F-04 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 for API 36; E5 for API 33–35 |
| S5 | F-03 | OWNER-SEMANTICS-REQUIRED (OD-08) | Stays DESIGN_HOLD |
| S6 | F-01 | SOURCE-CONFIRMED-NOT-REPRODUCED | E3 emulator measurement (40797031704) + E2. **Never physical.** |

**Change to earlier grades:** C-01 is re-confirmed at E1 host. F-05 is held at E2 end to end. F-01 stays an emulator E3.

### CR-07: Test harness (I-01, G-01, I-05 = G-02)

- **S1 REPRODUCED, E1.** PROJECT is at 256/256, and a simulated 257th check is dropped while the token still says OK. This agrees with the §2 panel.
- **S2 and S3 REPRODUCED, E1 host** under FMA builds (`HOST_SELFTESTS_FAIL (4392 checks, 2 failed)`).
  - They are classified as a **false FAIL from an over-tight oracle**, not a product bug.
  - The arm64 device result is E5 for S3 and E2 (asm) for S2. Running on arm64 itself is BLOCKED.
- **S4 REPRODUCED, E1.** The NDK clang emits `fmadd` for aarch64 at `-O0` and `-O2`, and the code has no `fp-contract` policy.
- **S5 SOURCE-CONFIRMED-NOT-REPRODUCED, E2.**
- **Change to earlier grades:**
  - G-01 stays TEST_HARNESS_DEFECT (RC-13a) at E1 host emulation.
  - Positive claim #5 is refuted for the shipping-arm64 reading, as in §3.

### Summary of grade movements caused by the CRs

- **Raised to E1 host:** E-03 (from E2), and H-08 and I-01 through their panel probes.
- **Re-confirmed at E1 host:** B-02/E-01, B-01 (domain), A-04 family, D-01, C-01/F-02, E-02 (component), G-01, I-01.
- **Held at E2 end to end, although some legs are E1:** A-01, A-05/E-04, F-05, F-04, and the UI half of B-01. For A-01, A-05/E-04 and F-05, the per-alias E1 in `ALIAS_TO_ROOT_CAUSE.tsv` should be read as a leg or component grade.
- **Narrowed or bounded by REFUTED controls:** CR-01-S7 limits the export bug to face-supported bodies. CR-03-S3 shows no corruption, and CR-03-S8 shows the bootstrap import is not reachable from the UI. CR-04-S5 confirms the oversized stroke applies and is named.
- **OWNER questions:** CR-02-S6 (saved non-identity transforms) is an instance of the existing **OD-16**, not a new question. CR-03-S5 (whether the support chooser is a creation-in-progress state) is new and has no OD id; it is listed in `DECISION_GATES.md` under "OWNER questions raised outside the OD list" (CR-03 Q2).
- **BLOCKED:** CR-05-S7 (the worker race) and the arm64 run of CR-07-S2/S3.
- **No CR result was promoted to device E3, and no emulator measurement was promoted to physical.**
