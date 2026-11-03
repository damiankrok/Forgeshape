# FORGESHAPE — REPORT ARCHIVE AND VERSION HISTORY

**Version:** 4.0.58
**Updated:** 2026-09-01
**Companion to:** `FORGESHAPE_MASTER_PLAN_v4.0.58.md` and `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md`

---

## How to use this file

**Do not read this file during normal stage work.** It exists for one purpose: reconstructing *why* something was decided, when the MASTER PLAN alone does not explain it. Nothing here is current truth. Nothing here authorises work.

Two parts:

- **Part A — Report archive.** One entry per accepted coding-agent or audit report, newest first. A stage cannot be marked `COMPLETE` in MASTER PLAN section 1 without an entry here (MASTER PLAN 2.7).
- **Part B — Version notes.** Every plan version note, verbatim, newest first, from `v4.0.58` back to `v2.3.0`.

Nothing in this file has been compressed, summarized or deleted. That is required by MASTER PLAN 2.5.

**This file is exempt in full from the single-present-tense rule (MASTER PLAN 0.1).** It records statements that were true when written; rewriting them would falsify the archive. A `current` or `running` in a version note or a report entry is history, never a claim about today.

### Known gaps in the version sequence

Two gaps exist and are explained here so that they are not mistaken for further content loss:

- **`v3.9.0` — never published.** No trace of it exists in any surviving version. Most likely a numbering skip.
- **`v3.57.0`, `v3.58.0`, `v3.59.0` — lost content, deliberately not restored.** `v3.57.0` was generated with the large Governance / roadmap / owner-decision middle section already missing. The loss was detected by accident and repaired in `v3.60.0` by restoring from the intact `v3.56.0` body. The three intermediate versions were therefore never canonical and are not reconstructed. This incident is the origin of the no-autonomous-compaction rule (MASTER PLAN 2.5) and of the mechanical integrity checks (MASTER PLAN 10.3).

---

# PART A — REPORT ARCHIVE

*Newest first. Four entries marked **backfilled in v4.0.0** existed only in the top snapshot and in version notes of v3.69.0; its "Latest accepted report delta" section had not been updated since UI-AUDIT2A.*

## MIRROR-01 — accepted technical/repository PASS, full final hash omitted by report

Claude Code returned `PASS-MIRROR-01-OWNER-LATER-READY`.

Baseline `94daaa92494988e6604d8dcf2db6f8a535a9a9f3`, clean.

Final commit was reported only as short hash `3283089`; the tree was reported clean. The coordinator does not fabricate the missing 40-character hash. The next stage must resolve and record it before editing.

Accepted implementation:
- Construction-only mirror across world XY/XZ/YZ;
- positive-scale proper-rotation representation;
- geometry-equivalence proof;
- fresh ObjectId;
- one history step;
- visibility/lock clone policy;
- no source retessellation;
- no `.forge` schema change;
- explicit Imported/Cad/Sculpt refusal;
- compact Objects plane chooser.

Evidence reported green: 62 focused mirror checks, 22 native selftest tokens / 3,335 checks, MirrorSmokeTest, ObjectCommandsTest, builds, release-symbol check and device guards. Automated time about 18 minutes.

No FullSharded was run under TEST-OWNER-03.

`SCULPT-H1` becomes READY. It is strictly the missing history-navigator UX over the already-existing SculptHistory; it must not replace the delta-history implementation.

## Stage 020M — accepted technical/repository PASS

Claude Code returned `PASS-STAGE-020M-OWNER-LATER-READY`.

Baseline `22ec7a204a36cab7a913df53ee10dcbb314ccee3`, clean.
Final HEAD `94daaa92494988e6604d8dcf2db6f8a535a9a9f3`, clean.

Implemented:
- exact Construction Body Dimensions from source parameters;
- body-local X/Y/Z leader labels;
- negative/center/positive anchor resize semantics;
- pure shared resize/anchor solver;
- transient Relative Scale `(1,1,1)` workflow;
- one-step ProjectHistory edits;
- locked/hidden refusal;
- no `.forge` schema change.

Evidence:
- focused dimensions selftest 97 checks PASS;
- native aggregate 21 tokens / 3,273 checks / 0 failures;
- debug/release sanity;
- release self-test symbols absent;
- device guards PASS;
- 30 corpus digests unchanged;
- one BodyDimensionsSmokeTest PASS;
- automated elapsed about 16 minutes.

No FullSharded was run under TEST-OWNER-03.

Coordinator accepts Stage 020M as technical/repository complete. Visual/ergonomic review remains OWNER LATER.

The clean `22ec7a...` baseline also proves the repository had advanced beyond the previously recorded dirty SEL-OUT state. Stage 018A/SEL-OUT are therefore treated as present in live baseline, but their missing direct completion report is not reconstructed or fabricated.

`MIRROR-01` becomes the next READY implementation stage with a bounded world-principal-plane Construction contract.

## OWNER override — skip SEL-OUT closeout tests, continue feature implementation

OWNER explicitly declines the SEL-OUT-R1 closeout/testing prompt and directs the project to implement the next functions.

SEL-OUT-R1 remains a dirty accepted candidate on HEAD `538623c26e0122d97303150ebbb52eb3ea0d32b3`. No final FullSharded PASS is claimed. The pre-existing SpatialSketchTest isolation defect remains deferred test debt.

The next implementation prompt must first commit the existing SEL-OUT candidate without additional testing, then continue directly into Stage 018A.

TEST-OWNER-03 is strengthened:
- ordinary feature verification target <=20 minutes / hard stop 30 minutes;
- automated coverage focuses on high-risk correctness;
- more visual/ergonomic/multi-feature walkthroughs move to OWNER LATER test packs;
- FullSharded is reserved for milestone/release/high-risk cross-cutting work.

Stage 018A is released:
- Rename;
- Visibility;
- Lock;
- Duplicate.

Delete remains untouched under UI-OWNER-45.

## SEL-OUT-R1 — feature green, aggregate blocked by pre-existing SpatialSketchTest isolation

Opus 5 High reported `PASS-SEL-OUT-R1-OWNER-RETEST-READY`, while disclosing that final FullSharded did not PASS.

SEL-OUT-R1 remains uncommitted on baseline `538623c26e0122d97303150ebbb52eb3ea0d32b3`.

Technical implementation is accepted: true renderer mask/composite outline, representation-neutral resources, depth-aware contour, no persistent whole-object glow, View/Overlay toggle, no mesh rebuild/upload on selection, five palettes/handedness, Delete/history untouched.

Shards 1–4 passed. Shard 5 failed SpatialSketchTest; the failure reproduces on the clean pre-feature baseline and is a pre-existing test-isolation defect caused by inherited populated scene state.

Coordinator does not claim aggregate PASS. A bounded closeout will fix test isolation and commit without rerunning FullSharded.

OWNER also changes future test policy: less synthetic/cloud UX coverage, more grouped OWNER manual test packs. TEST-OWNER-03 is active.

## TEST-RUNTIME-R1 — accepted PASS

Opus 5 High returned `PASS-TEST-RUNTIME-R1`.

Baseline `7f2d234f887766390de8b4a0820ee522a5b4a75f`; final clean HEAD `538623c26e0122d97303150ebbb52eb3ea0d32b3`.

The runner now has shard-only, PlanOnly, same-tree resume, versioned checkpoints, tested-byte fingerprints, timing, 90/120-minute budgets, two-attempt limits and explicit failure classification. TESTRUNTIME-01..24 pass, and bounded device integration completed without a forbidden full aggregate.

The stage also corrected the repo-wide log-ignore issue that had dropped cited UI-PREF focused evidence; missing logs were restored.

Remaining tooling debt is accepted: complete aggregate summary/failure guidance awaits the first later real aggregate, resume is per-device, and Ctrl+C does not yet write USER_ABORTED.

TEST-OWNER-02 runner mechanics are now implemented.

`SEL-OUT-R1` becomes the single READY feature stage. The pending IMPORT-01B Delete owner verdict remains parked and independent.

## UI-PREF-R1 OWNER-waiver closeout — accepted

Opus 5 High returned `PASS-UI-PREF-R1-OWNER-WAIVER-CLOSEOUT`.

Baseline `565d4002dc1306161a72bf86bb90cba7144c7316` matched exactly. All 47 dirty candidate paths were preserved.

Final clean HEAD:
`7f2d234f887766390de8b4a0820ee522a5b4a75f`

Commits:
- `23bc652` product/runtime/tests;
- `7f2d234` docs/evidence/waiver closeout.

Focused closeout:
- `uilr106` PASS;
- full legibility class 20 PASS in 8m59s;
- SettingsPreferencesTest 15 PASS in 4m23s;
- AppPreferencesTest PASS in ~5s.

No FullSharded was run, exactly as required by OWNER waiver.

The closeout refined the previous test-helper diagnosis: repeated Back failures correlated with guest/window-system failure, including `INSTRUMENTATION_ABORTED: System has crashed`; the identical APK passed after one clean guest restart. This is now classified as infrastructure evidence, not a demonstrated UI-PREF product regression.

UI-PREF-R1 is repository-complete / OWNER accepted with an explicit technical-evidence waiver. Gizmo handle style remains deferred by renderer contract.

TEST-RUNTIME-R1 is unblocked and becomes the single READY stage.

## UI-PREF-R1 — OWNER accepts product, waives missing final aggregate; repository closeout still required

Opus 5 High stopped with `EVIDENCE-BLOCKED`.

The product implementation is present only in a dirty worktree on baseline HEAD `565d4002dc1306161a72bf86bb90cba7144c7316`.

Focused/device/build/corpus gates are reported green. Three FullSharded attempts did not produce a final aggregate PASS:
- stale three-palette assertion;
- emulator system-server crash;
- late legibility-suite Activity-recreation race after the palette loop expanded to five.

The final deterministic wait fix was applied but its verification was stopped by OWNER after about 154 minutes.

OWNER explicitly reports the feature works and explicitly waives the missing final aggregate for UI-PREF-R1.

Coordinator accepts functional OWNER acceptance but does not call the dirty worktree repository-complete.

`TEST-OWNER-02` is approved:
- bounded aggregate runtime;
- max two full attempts;
- focused/shard reruns after failure;
- hard stop 120 minutes;
- infrastructure failures separated from product failures;
- future shard-resume/per-shard timing tooling.

`UI-PREF-R1-CLOSEOUT` becomes the single READY stage. It must not run FullSharded; it only verifies the last focused helper fix, corrects status/evidence, commits and finishes clean.

## OWNER sequencing override — UI-PREF-R1 while Delete verdict is parked

OWNER cannot perform the pending Delete manual check now and asks for a larger independent implementation prompt so several elements can be tested together later.

Coordinator releases `UI-PREF-R1`, composing the already-approved `UI-PREF1` + `UI-PREF2` work:
- Settings Hub;
- persistent app/UI preferences;
- right/left workspace edge-zone preference;
- five appearance palettes;
- gizmo visual size/thickness/style preferences.

No new OWNER decision is introduced.

Non-collision rule: UI-PREF-R1 may not change SceneObject Delete semantics, Delete ProjectHistory, UI-OWNER-45 or the pending IMPORT-01B verdict.

The old exact-baseline Delete HOLD becomes operationally stale after the new implementation commits; the verdict itself remains open and will be carried into a regenerated combined OWNER retest prompt.

Opus 5 High is the execution model.

## CAD-A3-C2 OWNER PASS repository sync — accepted

Opus 5 High returned `PASS-CAD-A3-C2-OWNER-ACCEPTED-DELETE-PENDING`.

Baseline `5cf981a9c89da8e99366592cb77b0f1cdff1c361` matched exactly; final clean HEAD `565d4002dc1306161a72bf86bb90cba7144c7316`.

The run changed only `PROJECT_STATUS.md`, synchronizing:
- CAD-A3-C2 / SKETCH-UX-R1 COMPLETE / OWNER ACCEPTED;
- UI-OWNER-47 OWNER accepted;
- UI-OWNER-48 OWNER accepted;
- ARCH-OWNER-14 OWNER accepted.

No runtime/test files changed, so no test or FullSharded rerun was required.

One adjacent stale current-truth claim about UI-OWNER-04 having no implementation was corrected and accepted.

Important provenance clarification: the agent observed that UI-OWNER-47/UI-OWNER-48/ARCH-OWNER-14 did not previously exist in repository-local docs. They already existed in the coordinator canonical OWNER decision set; repository status does not create OWNER decisions.

The IMPORT-01B Delete OWNER verdict remains pending and is the only immediate acceptance item.

## OWNER CAD-A3-C2 verdict — PASS

OWNER reports the current CAD-A3-C2 / SKETCH-UX-R1 behavior works on the real device.

Coordinator records:
- `CAD-A3-C2 / SKETCH-UX-R1 = COMPLETE / OWNER ACCEPTED`;
- UI-OWNER-47 = IMPLEMENTED / OWNER ACCEPTED;
- UI-OWNER-48 = IMPLEMENTED / OWNER ACCEPTED;
- ARCH-OWNER-14 = IMPLEMENTED / OWNER ACCEPTED.

The same message does not explicitly confirm the old IMPORT-01B Delete→Undo→Redo check, so that verdict remains pending and is not inferred.

A docs/status-only synchronization prompt is released under Opus 5 High.

## POST-AUDIT-HARDEN-R1 — technical PASS

Opus 5 High returned `PASS-POST-AUDIT-HARDEN-R1-OWNER-RETEST-READY`.

Exact baseline `af6a719a7fad847fc3ad06289cdfd5a077c46822`; final clean HEAD `5cf981a9c89da8e99366592cb77b0f1cdff1c361`.

Closed:
- F-08 JNI Sculpt synchronization;
- F-16 release selftest linkage;
- F-11 JNI exception ordering;
- F-15 accidental external linkage;
- F-18 dead reserved-chip code/resources.

F-08 expanded only within the same defect class: mechanical inspection found 12 unlocked `sculptSession()` calls rather than the seven originally highlighted. Final state reports 54 calls / 0 unlocked.

Release selftest removal significantly reduced release `.so` size while preserving 155 JNI exports and all Debug selftests.

Final verification:
- PAH-R1-01..16 PASS;
- native 20 suites / 2981 checks;
- JVM 70/70;
- both ABIs debug/release;
- device guards;
- 28-fixture corpus;
- FullSharded 40 classes / 524 tests / PASS.

No out-of-scope audit debt changed.

Next work is OWNER CAD-A3-C2 real-device acceptance plus the pending Delete verdict.

## DEEP-AUDIT-R1 — accepted PASS WITH DEBT

Fable 5.1 XHigh returned `PASS-DEEP-AUDIT-R1-WITH-DEBT`.

Baseline resolved to `be5b729c354a9876e24fbef54c630ee21fbe95d3`; reported final HEAD prefix `af6a719`, clean.

The audit found and fixed three P1 defects:
- semantic fingerprint omitted Arc/Spline authored payload;
- in-flight Sculpt stroke could bind to a newly loaded body;
- 4-byte UTF-8 imported names could fail under CheckJNI/debug conversion.

No P0 remained.

Reported broad gates:
- native 20 suites / 2981 checks;
- JVM 70/70;
- both ABIs debug/release;
- device guards;
- 28-fixture independent corpus;
- FullSharded 39 classes / 519 tests / PASS.

The audit also completed the requested comment compression and classified all 113 tracked Markdown files. Current-truth docs were reconciled; historical artifacts were preserved as history.

Coordinator accepts DEEP-AUDIT-R1 and ranks remaining debt.

Immediate bounded hardening:
- F-08 unlocked JNI sculpt-reader rebinding;
- F-16 selftests linked in release;
- F-11 JNI exception-check order;
- F-15 accidental external linkage;
- F-18 dead reserved-chip styling.

Larger P2/P3 performance/architecture items remain deferred to dedicated later stages.

`POST-AUDIT-HARDEN-R1` becomes the single READY stage under the normal Opus 5 High coding preference.

## OWNER sequencing override — DEEP-AUDIT-R1 before further feature work

OWNER declines to run the post-CAD owner-verdict HOLD now and requests a larger holistic application audit using Fable 5.1 XHigh.

The audit must inspect product functionality and implementation from all practical repository angles, including code architecture, concurrency/lifecycle, persistence/data contracts, CAD/Sculpt/import/export, renderer/resources, performance/memory, security/file handling, build/ABI, UI/input/accessibility, tests and all tracked Markdown.

OWNER also explicitly asks to shorten oversized comments in source code.

`AUDIT-OWNER-02` records the durable audit/comment/doc policy.

The audit must classify `.md` files by authority, reconcile current-truth docs with actual code/tests, preserve historical evidence, and audit future-functional assumptions without inventing new scope.

`DEEP-AUDIT-R1` becomes the single READY coding/audit stage. Model override is Fable 5.1 XHigh for this run only; normal ForgeShape coding preference returns to Opus 5 High afterward.

## CAD-A3-C2 / SKETCH-UX-R1 — technical PASS, OWNER retest ready

Agent returned `PASS-CAD-A3-C2-SKETCH-UX-R1-OWNER-RETEST-READY` from clean baseline `c5864c69902be127a12daa5ec81e86b0d07627bb` to clean final HEAD `be5b729c`.

Reported delivered OWNER UX correction:
- full-screen Home instead of modal card;
- navigation-style New Project page;
- immediate first CAD Sketch on XY/+Z;
- top-right orientation navigator with six principal normals and ±90° roll;
- selected-Line technical length annotation with exact numeric edit;
- Arc and Spline authored sketch entities;
- staged Edit Sketch with one-step ProjectHistory commit.

Data contract evolved to CADB v3 for curve authored truth while retaining all 22 older fixture bytes. Six new fixtures reportedly match between independent and production encoders.

Verification accepted: CADUXR1-01..40 PASS, E2E-CADUXR1-01..16 PASS, native 20 suites / 2970 checks, JVM/build/device guards green, FullSharded 39 classes / 519 tests PASS, final tree clean.

Bounded deviations retained: no live length during drag, no tablet visual capture, spline point edits by redraw. Report wording around a curve-specific kill/reopen case is ambiguous despite the all-green E2E matrix; include curve persistence in OWNER retest.

OWNER also records a process preference: use **Opus 5 High** for ForgeShape coding prompts by default from this point forward unless explicitly overridden.

## OWNER CAD UX verdict — Home/first-Sketch redesign + direct line dimensions

After the technical PASS at `c5864c69902be127a12daa5ec81e86b0d07627bb`, OWNER reviewed the real start screen and CAD flow.

OWNER rejects:
- the centered modal-card Home over the viewport;
- picking floating origin planes in an empty 3D world for the first CAD sketch.

OWNER approves:
- full-screen Home page with logo/product identity and integrated Open/New;
- New CAD immediately entering a flat orthographic XY grid;
- upper-right translucent Sketch Orientation Navigator for XY/XZ/YZ, ±normal views and ±90° screen rotation;
- existing physical face picking remains for later sketches on a body;
- technical-drawing-style on-canvas line length with editable numeric value;
- Arc/Curve and Spline sketch tools;
- staged Edit Sketch semantics for existing CAD bodies.

New decisions:
- UI-OWNER-47;
- UI-OWNER-48;
- ARCH-OWNER-14.

`CAD-A3-C2 / SKETCH-UX-R1` becomes the single READY coding stage.

## CAD-A3-C1 — technical PASS / OWNER Home + CAD retest ready

Agent returned `PASS-CAD-A3-APP-H1-OWNER-RETEST-READY` at final clean HEAD `c5864c69902be127a12daa5ec81e86b0d07627bb`.

The continuation closes every material gap from v4.0.42:
- Home is implemented as a no-project state;
- New Project → CAD/Sculpt is real;
- New CAD uses transient pre-project bootstrap and creates its first durable body only on successful Sketch→Extrude commit;
- Open File and dirty Save/Discard/Cancel lifecycle are implemented;
- spatial plane/face selection remains the primary route;
- independent PowerShell CADB-v2 encoder and six v2 fixtures exist;
- independent parity exposed and fixed a mistyped production FNV-1a topology-lineage basis before any shipped v2 file existed;
- all old fixture bytes remain unchanged;
- the required device Home/New/Open/spatial matrix passes;
- stylus hover is verified through injected real generic-motion dispatch without falsely claiming hardware hover;
- screenshot/contact-sheet evidence now exists.

Verification reported: native 19/19 / 2920 checks, JVM 70/70, both ABIs, device guards, 22-fixture corpus, and final FullSharded 37 classes / 504 tests / 504 PASS with integrity counters zero.

Five earlier aggregate failures are retained as evidence; final PASS followed correction/recovery each time. Final post-aggregate commit changes only evidence/docs/capture-script/javadoc and does not alter runtime/test behavior.

Coordinator accepts CAD-A3-C1 as TECHNICAL COMPLETE. CAD-A3 / APP-H1 / UI-OWNER-46 / ARCH-OWNER-13 now await only OWNER real-device interaction acceptance.

## CAD-A3 + APP-H1 — large partial delivery, stage FAIL accepted

Agent returned `FAIL-CAD-A3-APP-H1` at clean HEAD `4556f769fb424c65b717b2fac7a292aaa2b437c0`.

Coordinator accepts the token and classifies the stage as PARTIAL rather than a broken implementation.

Technically green core:
- semantic face topology + TopoRef;
- exact orthographic Sketch camera;
- adaptive grid;
- spatial world-plane/face picking;
- face-supported repeated Sketch/Extrude New Body;
- parent/dependent transform composition;
- cycle-safe dependency graph;
- producer Delete refusal with dependents;
- CADB v2 production codec;
- old CADB v1 compatibility;
- FullSharded 36 classes / 493 tests / 493 PASS.

Material missing acceptance:
1. APP-H1 Home / Open / New Project → CAD/Sculpt not implemented.
2. Independent PowerShell CADB v2 encoder + required v2 corpus fixtures/hashes absent.
3. Device E2E covers only a small subset of the released user-flow matrix.
4. Required screenshot/contact-sheet evidence absent.

The agent attributed APP-H1 deferral to the global non-empty-project invariant. Coordinator chooses a safer continuation direction: prefer a transient pre-project CAD bootstrap session until the first Sketch→Extrude commit, rather than fabricating a body or broadly weakening the ProjectDocument invariant.

`CAD-A3-C1` is the single READY continuation. Do not restart the already-green face-sketch core and do not start booleans.

## OWNER direction — Home + direct world-plane/planar-face CAD sketch entry

OWNER explicitly approves a new mobile-first project/CAD flow before further feature breadth.

Approved interaction:
- no-project Home: Open File / New Project;
- New Project: CAD / Sculpt;
- New CAD: choose XY/XZ/YZ physically in the viewport, not from a list;
- subsequent New Sketch: tap the actual supported planar CAD face with stylus/finger;
- exact orthographic sketch alignment;
- viewport-first contextual controls inspired by Shapr3D/Fusion/Inventor concepts, not copied UI.

Coordinator completed the previously deferred CAD-R0-UX research gate in `FORGESHAPE_CAD_R0_UX_DECISION_v1.md` with adopt/adapt/defer/reject.

Two new binding decisions:
- UI-OWNER-46: mobile Home flow + direct spatial sketch-support selection;
- ARCH-OWNER-13: persistent face-supported Sketch uses semantic TopoRef, never render triangle identity.

`CAD-A3` is pulled forward as the single READY coding stage. APP-H1 implementation is folded into that run. The stage must implement repeated face-supported Sketch/Extrude New Body without booleans and must fail closed rather than fake associativity.

## CAD-R0-A1A2 — technical PASS / OWNER retest ready

Agent returned `PASS-CAD-R0-A1A2-OWNER-RETEST-READY`.

Baseline `2bcfa3fa11fd96fa934887f16e55caefbee496d9` matched clean; final clean HEAD `3a00e0be6c528c9f96ae1f004bdaf70bc67afb82`.

Implemented a third durable `Cad` representation with XY/XZ/YZ workplanes, Line/Polyline/Rectangle/Circle sketch entities, endpoint/grid snapping, closed-profile validation, bounded triangulation, Extrude New Body, parametric edit, ProjectHistory, CADB persistence, autosave/recovery, Objects/gizmo and mesh export. CAD→Sculpt remains explicitly unsupported.

Verification reported: CADR0-01..40 PASS; E2E-CADR0-01..16 PASS; native 18/18 / 2845; JVM 70/70; both ABIs; device guards; corpus VerifyOnly; FullSharded 35 classes / 491 tests / 491 PASS.

Coordinator accepts technical PASS and records OWNER-retetest focus: fixed 0.25 m grid, ~87° sketch view, circle32 faceting, multi-profile selection UX, plus previously identified representation-dispatch architecture debt now that `Cad` is the third body representation.

OWNER also confirmed Sculpt Undo/Redo works; SCULPT-UNDO-R0 / ARCH-OWNER-12 are owner accepted.

## SCULPT-UNDO-R0 — technical PASS / OWNER retest ready

Agent returned `PASS-SCULPT-UNDO-R0-OWNER-RETEST-READY`.

Baseline `04f28df49f5000e863628571cdf5f8f076dac7e0` matched exactly. The cancelled C1 evidence directory was the only dirty path and was safely removed before coding. Final clean HEAD is `2bcfa3fa11fd96fa934887f16e55caefbee496d9`.

Implemented architecture:
- dedicated SculptHistory inside each retained FrozenSculpt;
- project/object history remains separate and never stores Sculpt vertices;
- compact per-stroke deltas: sorted touched vertex indices, before/after positions and before/after hasEdits;
- one completed stroke = one entry;
- 32 entries / 4 MiB per body / 1 MiB per entry;
- deterministic oldest-first eviction;
- Construction and Imported Sculpt share one implementation;
- source truth and SceneObject transform remain immutable;
- revisions advance monotonically;
- Back→Resume retains runtime history;
- history is not serialized; reopened projects preserve current mesh and start with empty stacks.

Verification accepted:
- native 17/17 / 2712 checks;
- SCUNDO-01..24 PASS;
- E2E-SCUNDO-01..10 PASS;
- JVM 70/70;
- both ABIs debug/release;
- device guards;
- unchanged `.forge` digests;
- FullSharded 34 classes / 482 tests / 482 PASS with clean integrity counters.

The test helper was corrected to send real MotionEvents so the Java UI receives the actual stroke-settled event; this exposed and then closed a stale Undo-enabled-state gap that the prior seam-only helper could not show.

Accepted deviations: moved geometry from a cancelled stroke is undoable under standing cancel semantics; hasEdits is explicitly historical rather than derived from revision; Delete→Undo may retain runtime SculptHistory through detached-body ownership.

OWNER real-device retest is the only remaining acceptance step for Sculpt Undo/Redo.

## OWNER override — Sculpt Undo/Redo becomes an approved dedicated stage

OWNER explicitly rejects deferring Sculpt Undo and requests implementation now with a **separate Sculpt history**.

Coordinator records `ARCH-OWNER-12`.

The old rule `Sculpt has no undo` is superseded. The new rule preserves the important architecture boundary: ordinary project/object history still never stores Sculpt vertex data; instead Sculpt receives its own bounded runtime-local per-stroke Undo/Redo history.

Key semantics:
- one completed stroke = one history entry;
- compact touched-vertex deltas preferred;
- hard step and byte bounds required;
- Redo branch clears after a new stroke;
- same implementation for Construction and Imported Sculpt;
- source mesh and SceneObject transform immutable;
- history survives Back→Resume within the same process;
- history is not serialized; reopen preserves current sculpt mesh but starts with empty undo/redo stacks;
- existing Undo/Redo controls become contextual in Sculpt.

The abandoned false-premise C1 artifacts are to be removed in preflight if and only if they are the only dirty paths.

`SCULPT-UNDO-R0` becomes the single READY coding stage while the IMPORT-01B owner Delete check remains OPEN.

## IMPORT-01B-C1 closeout — false premise confirmed, no product change

Agent returned `FAIL-IMPORT-01B-C1 — blocked on a false premise`.

The report independently confirms the coordinator's v4.0.36 correction:
- CLAUDE.md explicitly says Sculpt has no undo;
- ARCHITECTURE.md lists Sculpt undo as a separate future stage;
- PRODUCT.md explains why Undo/Redo controls are intentionally absent in Sculpt;
- EditorWorkspaceView hides the history group while sculpting;
- JNI refuses history steps in Sculpt;
- IMP01B-06 intentionally asserts Undo is unavailable.

No production/test/document file changed. No runtime test or FullSharded rerun is needed.

Only `artifacts/import-01b-c1/` was created and remains untracked. Coordinator decision: do not commit this abandoned correction artifact bundle; remove it and restore a clean worktree.

Product status is unchanged:
- real Import GLB PASS;
- real Imported Mesh Sculpt entry/edit PASS;
- Sculpt Undo intentionally absent;
- only Objects Delete / Undo / Redo owner check remains before full IMPORT-01B acceptance.

## Coordinator correction — Sculpt Undo was an intentional invariant, not an IMPORT-01B defect

While running the v4.0.35 C1 prompt, Claude inspected the live repository and stopped before changing code.

It found that ForgeShape explicitly documents and enforces:
- `Sculpt has no Undo`;
- Sculpt vertex/SculptRevision data never enters a project-history step;
- Undo/Redo controls are deliberately absent in Sculpt.

Coordinator accepts this live-repo finding and retracts the v4.0.35 classification of missing Sculpt Undo as an OWNER regression.

No Sculpt-Undo implementation is authorized.

OWNER real-file evidence still establishes:
- Import GLB PASS;
- Imported Mesh Sculpt entry/edit PASS.

ARCH-OWNER-11 is therefore OWNER accepted.

The only remaining IMPORT-01B owner check is Objects Delete → Undo → Redo.

Sculpt Undo is retained only as a future feature idea requiring a separate owner decision/history design.

## OWNER real-file retest — Imported Sculpt works, Sculpt Undo/Redo fails

OWNER manually tested the real imported low-poly model on the accepted IMPORT-01B build.

Observed:
- Import GLB works.
- Imported Mesh enters Sculpt.
- Sculpt edits work.
- Undo/cofanie akcji during Sculpt is not available or does not function.

Coordinator classification:
- this is a concrete OWNER failure against `IMP01B-06` and the technical report's claim that sculpt stroke Undo/Redo works;
- it is not deferred debt;
- IMPORT-01B remains OPEN for one bounded correction;
- CAD work is held until this regression is fixed and manually rechecked.

No assumption is made yet whether the defect is UI, Java routing/state, JNI/domain history, or a test-seam mismatch.

## IMPORT-01B — technical PASS after FullSharded evidence closure

Agent returned `PASS-IMPORT-01B-OWNER-RETEST-READY`.

Evidence-closure resume HEAD matched `f4d6cf73648cf6de1259b0a7d1cc79c7959d5802`. The existing dirty candidate was preserved exactly; no runtime/test correction was required in the closure run.

The stalled in-flight aggregate was classified as ForgeShape AVD guest/package-manager infrastructure. The launcher restart had resumed a wedged snapshot; a bounded real guest reboot (`adb -s emulator-5580 reboot`) restarted `system_server`, after which APK install and the aggregate succeeded. Global `adb kill-server` was not used and reserved emulator-5554 was not contacted.

Authoritative FullSharded: 33 classes, 472 tests, 5 shards, all 472 executed, all integrity counters zero, every shard PASS, `FULL_SHARDED_SUITE_PASS`.

Final commits:
- `c324910357fdd9f89839ddf63178948833943567` product/tests/corpus;
- `04f28df49f5000e863628571cdf5f8f076dac7e0` evidence/docs.

Final tree clean. Coordinator accepts technical IMPORT-01B PASS. OWNER must now manually retest the real low-poly GLB for Imported Mesh Start Sculpting / Back / Resume / Save-reopen and Objects-list Delete / Undo / Redo.

New non-blocking tooling debt: emulator launch script lacks explicit cold-boot/no-snapshot mode and may restore a wedged system snapshot.

## IMPORT-01B — EVIDENCE-BLOCKED after implementation candidate

Agent returned `EVIDENCE-BLOCKED`, which the coordinator accepts.

Reported candidate implementation covers Imported Mesh reversible Sculpt, `IMPT + SCUL`, real object Delete with Undo/Redo, final-body refusal and targeted renderer pruning.

Strong evidence already reported:
- all IMP01B-01..24 covered;
- E2E-IMP01B-01..11 PASS; OWNER real file still unavailable to the agent;
- native 17 suites / 2628 checks;
- JVM 70/70;
- both ABIs debug/release;
- device guards PASS;
- nine regression classes / 190 tests PASS;
- 12 corpus fixtures with 10 legacy hashes unchanged and two new independent-encoder matches.

FullSharded did not complete:
1. first run found a real GPU fence-wait product defect;
2. second found a real order/process-state test defect;
3. third aborted when Android `system_server` died;
4. fourth stalled in ADB discovery.

Both real defects are reported fixed. Best aggregate coverage on the final candidate tree is 281/472; 191 tests remain unexecuted in the required aggregate.

Coordinator additionally detected that final-tree-clean acceptance is also unmet: HEAD is still the pre-stage `f4d6cf7`, with 29 tracked modified files and 10 untracked files. No IMPORT-01B commit exists.

The refusal to delete during an actively running Sculpt interaction is accepted as a bounded non-blocking deviation: retained-Sculpt bodies remain deletable outside the active brush session, and the owner decision did not require history redesign inside Sculpt mode.

Exactly one next step: preserve the dirty stage worktree, finish one authoritative FullSharded PASS, then commit and return a terminal IMPORT-01B verdict. No new feature stage.

## OWNER acceptance — IMPORT-01A complete; IMPORT-01B + Delete released

OWNER manually tested the real external low-poly GLB on the current build and confirmed:
- durable import succeeds;
- gizmo is available;
- Move / Rotate / Scale work;
- the project can be saved as `.forge`;
- the `.forge` can be reopened later and the imported object remains usable.

Coordinator therefore closes `IMPORT-01A` as COMPLETE / OWNER accepted.

OWNER explicitly releases the next step and additionally asks for object deletion from the Objects list because shapes can currently be added but not removed.

Two binding decisions are recorded:
- `ARCH-OWNER-11`: Imported Mesh may seed the current reversible Sculpt workflow without a fabricated Construction Source; `IMPT + optional SCUL` is the intended project combination.
- `UI-OWNER-45`: generic real SceneObject Delete is pulled forward from Stage 018A into IMPORT-01B, with one-step Undo/Redo and correct renderer/persistence removal. Rename/visibility/lock/duplicate remain later.

`IMPORT-01B` becomes the single READY coding stage.

## ARCH-HEALTH-01 — architecture health review accepted with debt

Result `PASS-ARCH-HEALTH-REVIEW-WITH-DEBT` accepted at final clean HEAD `f4d6cf73648cf6de1259b0a7d1cc79c7959d5802`, baseline `4c296f5415aa6955c2cc77e84d063c9ae5a59d47`.

The review assessed the repository as modular enough to continue feature work and reported aggregate modularity 4.3/5. One concrete synchronization defect was corrected: Construction shape Apply mutated state outside the mutex while the autosave worker read that state under `g_stateMutex`. One redundant Construction tessellation per publish was also removed. Six stale code comments and five stale ARCHITECTURE statements were corrected.

Reported verification: native 17 suites / 2528 checks / zero failures, JVM 70/70, both required ABI/configuration builds, and five focused device suites totaling 71 tests on emulator-5580. No FullSharded aggregate was claimed; coordinator accepts this as proportional evidence for the bounded review corrections. No dedicated race-detector proof was reported, so the mutex fix must not be described as formal proof of global race-freedom.

Non-blocking debt recorded: duplicated interchange-geometry extraction, boolean representation leakage above JNI, EditorWorkspaceView feature concentration, renderer resource pruning, activeBody fallback, bounded O(n²) duplicate-ID scan, and a compatibility-sensitive `.forge` debt where decoded `nextObjectId` is not constrained below the reserved `2^60` preview-key range.

The review does not change OWNER acceptance state for IMPORT-01A. Real-file durable import retest remains the single next action; IMPORT-01B stays blocked until OWNER verdict.

## IMPORT-01A — PASS / durable Imported Mesh project objects

Agent resumed from full baseline `bdf3f59b85a48942cd907a4f24c0158ca821b659` and reported final clean HEAD `4c296f5415aa6955c2cc77e84d063c9ae5a59d47`, with `2610d07` implementation/tests/corpus and `4c296f5` docs/evidence.

Foundation remained valid with three minimum mixed-representation corrections: nullable active Construction access, representation-aware project capture/fingerprint, and shared runtime loadability validation.

Reported product result: GLB import now atomically creates durable Imported Mesh SceneObjects with stable ObjectIds, node/mesh/fallback names, Objects-list selection, existing Move/Rotate/Scale gizmo and one-transaction import history. Linear glTF transform is baked into local geometry; translation remains project placement/pivot; imported objects start at identity rotation and unit scale. Shape and Start Sculpting remain unavailable.

Reported `.forge` evolution: major/minor still 1/0; Imported Mesh truth uses IMPT v1 announced by hasIMPT header flag; CONS is sparse/conditional on actual Construction Source. Legacy fixtures retain identical hashes. Imported truth stores id/name/local positions/effective normals/indices/submesh ranges/doubleSided and excludes source URI/material/UV/GPU/preview state. Mixed Construction/Sculpt/Imported Save/Open, Save Copy, autosave and recovery are self-contained.

Coordinator wording clarification: “IMPT always required in both project kinds” is recorded as required for documents that announce/use Imported Mesh truth through hasIMPT, because legacy documents are simultaneously reported byte-identical. This is a report wording clarification, not a product correction.

Reported verification: native 17 suites / 2528 checks, JVM 70/70, both ABIs, device guards, corpus VerifyOnly, IMP01A-01..25 coverage, device E2E 01..11 PASS, owner exact-file E2E-12 pending, FullSharded 31 classes / 455 tests / 455 PASS with all integrity counters zero.

Accepted bounded deviation: export now includes durable Imported Mesh bodies so visible project bodies are not silently omitted from GLB export. No new format/material scope is inferred.

Coordinator verdict: **PASS-IMPORT-01A-OWNER-RETEST-READY accepted. IMPORT-01A technically COMPLETE. IMPORT-01B remains blocked only on OWNER real-file durable retest.**

## IMPORT-01A — incomplete foundation checkpoint at `bdf3f59`

Agent deliberately returned no terminal result token. Coordinator accepts this as **PARTIAL / FOUNDATION COMPLETE**, not PASS and not FAIL.

Baseline `504f97d6b087bd0103762a53dd4ac515433e95dd` was clean. One enabling-refactor commit `bdf3f59` was created. The report supplied only the short final hash, so the resume run must resolve the full 40-character hash before editing.

Reported accepted foundation:
- SceneObject owns placement independently of Construction Source;
- ConstructionObjectState is shape-only;
- explicit BodyRepresentation and nullable Construction truth;
- ImportedMesh domain type with positions/normals/indices/submesh batches/double-sided validation;
- parser DTO types renamed away from project-truth names;
- history placement generalized across representations;
- SCNE live-repo inspection confirms placement is already project-neutral and separate from CONS.

Reported regression evidence: native 17 suites / 2431 checks / 0 failures, ProjectAutosaveRecoveryTest 14 PASS, EditorWorkspaceHistoryTest 20 PASS, both ABIs build, old `.forge` golden digests unchanged, no reserved-device contact or lingering instrumentation process.

No user-visible 01A behavior exists yet: GLB import remains session preview only.

Remaining stage: atomic ParsedGlbScene→ImportedMesh project objects, Objects/gizmo/history, self-contained `.forge` Imported Mesh truth with backward compatibility and independent corpus implementation, mixed-scene project state, complete focused/device/regression/FullSharded verification.

Coordinator decision: **resume from `bdf3f59`; do not restart the refactor and do not open IMPORT-01B.**

## OWNER real-file acceptance + durable import promotion

OWNER reports that the real external low-poly `.glb` opens normally in ForgeShape under GLB-IMPORT-R1. The observed lack of Objects-list entry, gizmo and Sculpt is classified as the intentional session-only preview boundary, not an R1 import failure.

OWNER explicitly accepts the coordinator's next direction: open `IMPORT-01A` so imported GLB geometry becomes a real durable `Imported Mesh` project object with ObjectId, Objects-list selection, gizmo Move/Rotate/Scale, Undo/Redo and `.forge` persistence. Sculpt is deliberately split into `IMPORT-01B` after 01A.

This is an explicit OWNER timing/scope override of the earlier "durable import post-MVP / after Stage 040" rule only for the bounded functional `IMPORT-01A` slice. The physical-device/high-density readiness gate remains binding for broad import scaling and million-triangle capability claims. GATE-E2E remains OPEN; the owner authorizes 01A before its final verdict rather than declaring the gate passed.

Exactly one next coding step: `IMPORT-01A`.

## GLB-IMPORT-R1 — technical PASS / OWNER exact-file retest ready

Agent report baseline `27ec0bcc377d90fb69daa3b7b1332723ca4fe94b`, final clean HEAD `504f97d6b087bd0103762a53dd4ac515433e95dd`, one stage commit.

Reported R1 implementation widens only the session-only preview: static node matrix/TRS, multi-primitive meshes, uint16/uint32 indices, generated normals when NORMAL is absent, inverse-transpose normal handling when present, determinant/winding handling, validated-but-ignored color/UV attributes, double-sided preview culling, ignored extras, clearer Import GLB UX and bounded failure categories.

Exact OWNER binary was not available and was correctly not fabricated/searched. A deterministic Nomad-like fixture (111,628 B; SHA-256 `b2dcd36d6b2e33cebe78b1b730f0c26cf7baf39ee12e4e4a2cd8cdc454891f51`) reproduces the target feature class at similar scale: one matrix node, one mesh, seven TRIANGLES primitives, shared positions, 1,978 vertices, 3,780 triangles, uint32 indices, no NORMAL, colors/UV, double-sided, extras.

Reported verification: GLBIR1-01..21 PASS; device E2E 01..09 PASS; exact owner-file E2E-10 `OWNER_SAMPLE_RUNTIME_TEST_PENDING`; native 17 suites / 2431 checks; FullSharded 30 classes / 443 tests / 443 PASS; JVM/build/device guards/corpus green; no lingering stage process.

A second FullSharded aggregate is accepted because the evidence screenshot camera-distance change altered the final tree after the first aggregate; only the second final-tree aggregate is treated as authority.

Coordinator verdict: **technical PASS accepted; GLB-IMPORT-R1 remains OPEN only for OWNER exact-file manual acceptance.** No further code is authorized before the owner tries the real `1 lowpoly.glb`.

Exactly one next step: OWNER taps `Import GLB…`, selects the real file, and returns PASS or concrete visible/error-token failure.

## OWNER approval — GLB-IMPORT-R1 external static preview compatibility

OWNER explicitly accepted extending the existing diagnostic importer so a real external low-poly character GLB can be selected and rendered in ForgeShape.

Coordinator inspected the attached `1 lowpoly.glb`: 142,820 bytes, SHA-256 `59fd1bfe8473e9b78849170ef6c02023d45bf769a509b7ab1974127360ff3bd6`, GLB 2.0 generated by Nomad 11, one node with matrix, one mesh with seven TRIANGLES primitives, 1,987 POSITION vertices, 7,908 uint32 indices / 2,636 triangles, no NORMAL accessor, COLOR_0/COLOR_1/TEXCOORD_0, one double-sided material and Nomad extras.

The current R0 parser intentionally rejects matrix/missing-normal external files, so the user's failed attempt is consistent with scope and does not demonstrate a viewport defect.

OWNER approved ARCH-OWNER-09: R1 may support static matrix/TRS, multiple primitives, generated normals, safe ignored attributes and double-sided culling while preserving the existing session-only/non-persistent preview boundary.

Exactly one next step: `GLB-IMPORT-R1`. Broad production import, OBJ and FBX remain outside this stage.

## GLB-IMPORT-R0 — PASS / roundtrip equivalent

Agent report baseline `868995d135b4582693b10cd486abc7418b45c5a9`, final clean HEAD `27ec0bcc377d90fb69daa3b7b1332723ca4fe94b`, commits `949d8a7` product+tests, `ee665ee` evidence, `27ec0bc` docs.

Reported result: session-only Imported Mesh Preview independently parses the current ForgeShape GLB subset from actual bytes without using exporter code. It owns no semantic ObjectId/project/history/persistence state and is excluded from `.forge`, manual save, autosave, recovery and re-export.

Core diagnostic:
- Construction 6→6 bodies/meshes, counts/index order/AABBs identical, max world-position delta exactly `0.0 m`, max normal delta ~`1.20741827e-06°`;
- Sculpt 2 bodies, current Sculpt + Construction companion, max world-position delta `0.0 m`, max normal delta `0°`;
- committed corrected sentinel bytes also roundtrip equivalent.

Reported verification: GLBIR0-01..20 PASS; E2E-GLBIR0-01..08 PASS; native 17 suites / 2379 checks; JVM 70/70; both ABI debug/release; device guards/corpus PASS; FullSharded 29 classes / 429 tests / 429 PASS; no lingering stage process.

Coordinator verdict: **GLB-IMPORT-R0 COMPLETE. Internal ForgeShape source→GLB bytes→independent import mismatch is not demonstrated. GATE-E2E reopens for OWNER external-engine classification.**

Carried bounded debt: renderer preview keys reuse ObjectId storage type from a reserved high range despite no semantic ObjectId; preview shading does not use parsed normals although numeric normal comparison does.

Exactly one next step: OWNER controlled external-engine re-test; no exporter correction/OBJ/FBX before a concrete numeric external mismatch.

## OWNER decision — GLB-IMPORT-R0 diagnostic exception before GATE-E2E

After inspecting the corrected Blender import, OWNER confirmed node Scale is 1/1/1 but still sees a visual arrangement difference compared with ForgeShape. The screenshot does not by itself prove whether the discrepancy belongs to the exporter, Blender's Y-up→Z-up presentation or the camera/view.

OWNER approved one bounded diagnostic exception to the post-MVP import gate: `GLB-IMPORT-R0`.

The stage will independently parse ForgeShape's own static GLB bytes and display them as a session-only Imported Mesh Preview, allowing Source↔Imported comparison and machine world-geometry delta checks. The preview is not project truth, is not saved/autosaved, gets no fake Construction Source and cannot be sculpted/edited.

Coordinator classification: explicit owner scope decision, not an automatic MVP expansion. Broad/durable GLB/OBJ/FBX import remains post-MVP.

Exactly one next step: run `GLB-IMPORT-R0`; GATE-E2E waits for its diagnostic result.

## E2E-R1C-C1 — PASS / baked static-transform policy

Agent report baseline `cdc184b999bae716809f632c11fe771ebbb98c2b`, final clean HEAD `868995d135b4582693b10cd486abc7418b45c5a9`, three commits (`e1fd46f` product+tests, `ae36971` evidence/sentinels, `868995d` docs).

Reported implementation applies ARCH-OWNER-07: `L = Rz·Ry·Rx·S` baked into positions; inverse-transpose normal matrix + renormalization; node Translation retained; no node matrix, Rotation or Scale emitted; no recenter; determinant guards for singular/mirrored invalid transforms; Construction/Sculpt representation selection unchanged; `.forge` authored transforms unchanged.

New current sentinel hashes:
- Construction 59,884 B: `8ac4fb6abb509361b911c9b4bbc2dd8385910668808b40fbd9faeab244f2c3ff`;
- Sculpt 2,588 B: `ae82a0720d9f503f77d0237edf40ab5c1152bae6938df08166e1ea9b31014f2c`.

Old hashes are explicitly PRE_ARCH_OWNER_07 history only.

Reported verification: FSR1C-C1-01..15 PASS; E2ER1C-C1-01..06 PASS; native 16 suites / 2292 checks; GlbExportTest 24/24; JVM 70/70; both ABIs debug/release; device guards and seven-file `.forge` corpus PASS; authoritative FullSharded 28 classes / 406 tests / 406 PASS with all inventory/execution counters zero; no lingering Stage-C1 process.

Coordinator verdict: **PASS-E2E-R1C-C1 accepted. E2E-R1C COMPLETE again. GATE-E2E OPEN / OWNER on the corrected sentinels.** No external-engine result is inferred.

Exactly one next step: OWNER imports the corrected sentinel(s) and returns PASS or concrete observed-vs-expected defect.

## GATE-E2E observation — OWNER changes static GLB transform policy

OWNER compared the ForgeShape sentinel scene with a Blender import. The Blender scene looked visually different enough to make the current node-transform presentation undesirable for interchange, even though the checked Body_1 Blender location/rotation values were consistent with glTF Y-up → Blender Z-up basis conversion and therefore did not prove a structural GLB defect.

OWNER explicitly approved a revised policy: **bake Rotation + Scale into static export geometry; retain Translation/pivot at node level; exported node Scale is 1/1/1 and Rotation identity.**

Coordinator classification: product/export architecture decision, bounded correction to E2E-R1C before GATE-E2E. This is standards-conformant glTF and does not broaden scope to OBJ/FBX.

GATE-E2E remains OPEN and previous sentinel GLBs become historical pre-bake evidence. The previously prepared post-PASS docs sync remains on HOLD and must not be run.

Exactly one next step: focused E2E-R1C-C1 bake-transform correction.

## GATE-E2E — owner handoff support — PASS-GATE-HANDOFF

Claude performed a read-only gate handoff at unchanged clean HEAD `cdc184b999bae716809f632c11fe771ebbb98c2b`.

Confirmed committed files:
- `construction_sentinel.glb`: 60,312 B, SHA-256 `266355a5dc2e58af626c2fb60b66420ad2c98e0d6eb1446925d8c5a3a5eaaf5f`;
- `sculpt_sentinel.glb`: 2,684 B, SHA-256 `16805a054491b3e65c4a0cec86dc3860667bee500b13a9a33d71eb04e1a517f1`.

No file was regenerated or changed; no commit, Gradle, adb/device or external engine was used. Claude correctly did not claim the OWNER gate verdict.

The handoff identified one wording error in the existing gate note: body translations are `(0.5·i, -0.25·i, 1.25·i)` m, so the sequence **descends in Y** while increasing X/Z. Any older “rising line” prose is stale wording; the numeric values are unchanged.

Coordinator verdict: **PASS-GATE-HANDOFF accepted; GATE-E2E remains OPEN / OWNER.**

Exactly one next step: owner imports the sentinel(s) in Godot and returns PASS or a numbered observed-vs-expected rejection.

## E2E-R1C / Stage 023 — reported PASS / owner gate ready

Agent report resolved baseline `678af45d19779ff5af96cc3e5db5850171ba2bdb`, final clean HEAD `cdc184b999bae716809f632c11fe771ebbb98c2b`.

Coordinate authority passed before coding: ForgeShape is documented/implemented as metres, +Y up, right-handed, renderer placement `T·Rz·Ry·Rx·S`, column-major storage, CCW outward winding. The early glTF path therefore uses 1:1 metre scale and zero axis-conversion root.

Reported implementation: hand-written platform-neutral GLB 2.0 exporter; Construction re-evaluation from source truth; Sculpt export from current Sculpt representation; multi-body nodes/meshes; preserved body-local pivot/origin plus node transform; POSITION/NORMAL/indices; SAF Export; independent test-only GLB reader; two deterministic owner sentinel projects/GLBs.

Reported verification: exporter selftest 60 checks; native 16 suites / 2259 checks; JVM 70/70; focused instrumentation suites all green; authoritative FullSharded 28 classes / 399 tests / 399 PASS with all integrity counters zero; both ABIs debug/release; device guards and `.forge` corpus verification PASS; no lingering Stage-023 runner/instrumentation process.

No Godot/Blender/external validator was run and no such claim is accepted. `GATE-E2E` remains an OWNER checkpoint.

Coordinator verdict: **E2E-R1C COMPLETE. `GATE-E2E` OPEN.** Dead reserved-control styling and generic export-refusal messaging are carried non-blocking debt.

Exactly one next step: OWNER imports `construction_sentinel.glb` in Godot using the prepared gate checklist and gives PASS or a concrete rejection symptom.

## E2E-R1B / Stage 022 — reported PASS

Agent report baseline `a52be2464c7b1ca101026e05be9727b16da33627`, final HEAD reported only as `678af452…` / short `678af45`, clean tree, three local commits (`de9937b` product+tests, `c584688` evidence/script, `678af45` docs), no remote/push.

Reported result: separate atomic autosave/recovery checkpoint, explicit Recover/Discard, native `.forge` SAF Save Copy/Open File, lifecycle/background resilience, deterministic injected GPU-device-loss rebuild with CPU document preservation, and bounded local diagnostics/manual system export.

Reported verification:
- FSR1B-01..18 PASS;
- E2ER1B-01..08 PASS;
- native 15 suites / 2199 checks;
- JVM 70/70;
- focused FSR1B 36/36;
- process-death 8/8 stages;
- R1A + UI-LAYOUT-R2 focused regression 94/94;
- FullSharded 27 classes / 382 tests / 382 PASS, shards 76+79+78+75+74, all inventory/execution integrity counters zero;
- arm64-v8a + x86_64 debug/release and device guards PASS.

Accepted deviations:
- FullSharded had to use Claude's background mechanism because the foreground tool timeout is shorter than the suite. Agent reports no Stage-022 runner/instrumentation remained afterwards.
- first aggregate attempt exposed an order-dependent assertion in a newly authored test; it was corrected and the full aggregate rerun from shard 1.

Coordinator verdict: **E2E-R1B COMPLETE on reported evidence.** Remaining limitations are carried debt, not blockers. The report omitted the full final SHA, so the Stage 023 preflight must resolve the exact HEAD before any edit instead of fabricating it.

Exactly one next step: E2E-R1C / Stage 023 early GLB export.

## E2E-R1A — owner operational override / stage closure

After the v4.0.17 cleanup hold, the owner explicitly stated: `Pomijamy te zadania backgroundowe sa juz zakończone idźmy dalej`.

Coordinator interpretation: the owner confirms the two Claude Code FullSharded background task cards are already finished and instructs the project not to spend another run proving their process state. This is an operational override, not new automated evidence.

The previously reported E2E-R1A technical/product evidence remains unchanged at final clean HEAD `a52be2464c7b1ca101026e05be9727b16da33627`: portable `.forge` v1, Construction/Sculpt process-death reopen, corruption/version non-mutation, FSR1A-01..15, E2ER1A-01..06, native 2163 checks, JVM 59/59 and FullSharded 342/342.

Coordinator verdict: **E2E-R1A COMPLETE.** The cleanup-only prompt from v4.0.17 is withdrawn and must not be run. No additional test result is invented. E2E-R1B / Stage 022 becomes READY.

Exactly one next step: run E2E-R1B — autosave, recovery, Scoped Storage `.forge` transfer and offline diagnostics.

## E2E-R1A / Stage 021 — reported PASS with unresolved background-task cleanup

Agent report baseline `14de5ee58f061b36774f22cccc88187f4c0e85bf`, final HEAD `a52be2464c7b1ca101026e05be9727b16da33627`, clean tree, three local commits (`32f3642` product+tests, `19cc0ba` evidence/corpus, `a52be24` docs), no remote/push.

Reported product result: portable `.forge` v1 save/open through one app-private slot, Construction and Sculpt process-death reopen, fail-closed corruption/version handling and a deterministic golden corpus. Reported verification: FSR1A-01..15 120 checks, E2ER1A-01..06 PASS, native 14 suites / 2163 checks, JVM 59/59, project-actions 7/7, R2 regressions 67/67, FullSharded 24 classes / 342 tests / 342 PASS, both Android ABIs debug/release, device guards PASS.

Bounded implementation deviation: one Sculpt `hasEdits` flag is persisted so a reopened edited sculpt cannot silently bypass the destructive Reset guard. Coordinator accepts the boolean safety fact as project truth while keeping SculptRevision/MeshRevision counters non-persistent; CAD-R0-DATA Decision v3 records the clarification.

Owner then supplied a Claude Code Background tasks screenshot showing **two FullSharded Shell jobs still marked Running**: the original aggregate and the `rerun on recovered AVD`. This is consistent with the report's earlier aborted aggregate + rerun sequence, but the report alone cannot prove whether the UI cards are stale wrappers or still-live child process trees.

Coordinator verdict: **reported technical/product PASS, operational closeout HOLD.** Do not mark E2E-R1A COMPLETE and do not open E2E-R1B until a cleanup-only prompt proves no Stage-021-owned background/test process remains. Cleanup must make zero repository changes and does not consume a product correction round unless it reveals a real defect.

Exactly one next step: Stage-021 background-task cleanup closeout.

## UI-LAYOUT-R2 — Final Visual Acceptance Docs Sync — PASS-DOCS-SYNC

Agent report: baseline `71bab57e4e12a91c5f1c7b8cec6a5af9b92569b7`, final HEAD `14de5ee58f061b36774f22cccc88187f4c0e85bf`, exactly one local docs-only commit, final clean tree, no remote/push.

Reported diff is exactly `PROJECT_STATUS.md`; version 0.47.0 → 0.48.0. No Java/C++/XML/resource/test/harness/script/evidence path changed, both R2 evidence packages were reported byte-identical, no Gradle/adb/device/install work ran, and `git diff --check` was clean. The sync attributes final visual acceptance to the coordinator on 2026-08-30 without fabricating an owner screenshot-approval quote, keeps the 2026-08-29 owner acceptance of UI-SPEC-R0 Revision 1 separate, records UI-LAYOUT-R2 COMPLETE, retires correction round 2 unused, activates the UI moratorium and points repository Next Stage to coordinator-owned CAD-R0-DATA.

Coordinator verdict: **accept `PASS-DOCS-SYNC` as reported.** A docs-only current-truth sync does not require product/device test reruns. Repository commands and the post-commit file itself were not independently rerun/inspected by the coordinator; live repo truth wins if later evidence conflicts.

After accepting the sync, the coordinator executed the already-scheduled `CAD-R0-DATA` architecture decision outside the repository. Decision v1 selects a declarative semantic feature graph, separates project-neutral scene / Construction / Sculpt truth, fixes binary64 Construction vs current float32 Sculpt mesh precision, defines fail-closed feature-lineage topology references, defines deterministic `FORGESH1` chunked `.forge` v1 and writes a broader evidence gate/candidate list for `GATE-KERNEL` without answering owner `OQ-A`.

Exactly one next step: run self-contained `E2E-R1A / Stage 021` `.forge` save/load prompt at baseline `14de5ee58f061b36774f22cccc88187f4c0e85bf`.

## UI-LAYOUT-R2 — owner visual review bundle + coordinator verdict — PASS / STAGE COMPLETE

Agent report: baseline `40d1f4490ef874344f0b101da2e290053028f675`, final HEAD `71bab57e4e12a91c5f1c7b8cec6a5af9b92569b7`, exactly one local evidence-only commit, final clean tree, no remote/push. The bundle created only `artifacts/uilayoutr2-owner-review/README.md`, `OWNER_REVIEW.html` and `OWNER_REVIEW_CONTACT_SHEET.png`; no product/resource/test/current-truth docs changed, no adb/Gradle/device work ran, and all 12 source PNG hashes were reported identical before/after packaging.

The six representative pairs are C10 Transform Move World, C10 Exact+IME, L10 Exact+IME, L13 Sculpt Details, E10 Exact+IME and E13 Sculpt Details. Recorded deltas are `Δright=0` for all six, `Δwidth=-4` for all six, and `Δtop` in `[-1,+0.5]`, all PASS against Revision 1. The bundle itself intentionally records no visual verdict.

Coordinator visual review (2026-08-30): **PASS.** Direct inspection of the supplied contact sheet / review images shows the right rail remains a narrow edge-hugging instrument; in the corrected short-landscape and expanded states Exact/Details sits inboard/left of the fixed rail; there is no visible rail translation, overlap, detached control grammar or unnecessary viewport loss. Compact Exact+IME also preserves the same grammar. The known Sculpt Radius/Strength weight remains P2 and is not an R2 blocker.

Coordinator verdict: **`UI-LAYOUT-R2 COMPLETE`.** Correction round 1 is the only consumed round. The unused second round is retired with stage closure. UI moratorium activates. `CAD-R0-DATA` is unblocked and becomes READY; `E2E-R1A` remains blocked by that coordinator decision document.

Exactly one immediate repository handoff: docs-only acceptance sync so repo current-truth documentation records R2 visual completion. No product change and no new stage in that prompt.

## UI-LAYOUT-R2 — correction round 1 — TECHNICAL PASS / OWNER VISUAL REVIEW REQUIRED

Agent report: baseline `40e8b10a0b30da27ba1e60b22ab6f8667135d452`, product+test correction `6d05814ddb89ef4e06d2e78d62af78d8b2262af1`, final evidence+docs HEAD `40d1f4490ef874344f0b101da2e290053028f675`, clean tree, no push/remote.

Scope remained presentation/layout only plus focused tests, evidence and docs. No JNI/native/domain/renderer/picking/mesh/history/serialization/motion/theme/dimens/breakpoint work was reported.

Root cause: side-placed precision/details surface was appended after `WorkspaceTrailingHostView` in the horizontal middle row, so it consumed trailing width and translated the rail inward. Correction seats the side panel inboard of the trailing host and makes trailing collision clamping respect whichever trailing surface begins first.

Reported acceptance evidence:
- 66/66 host frame rows PASS against Revision 1; C10/C13 measured `112.5/8/68`, L10/L13 `88/8/68`, E10/E13 `107/8/68` dp for top/right/width;
- zero intra-cell host drift; Exact-open, Exact+IME and Sculpt Details keep right=8 dp in all cells;
- persistent-resting chrome Δ=0 dp;
- zero intrinsic hit-area violations; ≥48×48 dp;
- original `UILR2-01..18` 18/18 PASS;
- new `UILR2C-01..12` 12/12 PASS;
- JVM 59/59; native 2043/2043 + viewport OK; device guards PASS;
- FullSharded inventory increased to 22 classes / 330 tests and passed 330/330 with zero missing/duplicates/unexpected/execution-missing and `FULL_SHARDED_SUITE_PASS`;
- 36 raw screenshots + 36 mechanical overlays produced in `artifacts/uilayoutr2-correction/`.

Coordinator verdict: **accept TECHNICAL PASS as reported. `UI-LAYOUT-R2` remains OPEN solely for owner/coordinator visual review. Correction round 1 is consumed; one round remains only if the visual evidence reveals a concrete in-scope defect.**

Exactly one next step: owner/coordinator visual verdict on the corrected evidence package. No CAD-R0-DATA or motion/morph before that verdict.

## UI-LAYOUT-R2 — Revision 1 correction handoff — PRECONDITION-NOT-MET / NO CHANGE

The agent reported expected current HEAD `40e8b10a0b30da27ba1e60b22ab6f8667135d452`, clean before and after the run, with no commit and no changed file. The run stopped before build/device/product work because the correction prompt required `D:\TRAVELAPPS\FORGESHAPE_UI_SPEC_R0_REV1_ACCEPTED.html`, but that separately transferred owner artifact was absent. The historical wide R0 file was present and correctly rejected as a substitute.

No adb, Gradle, capture, test or product/resource/test/harness change occurred. `UILR2C-01..12` were not created. Correction round 1 was not consumed.

Coordinator verdict: **correct precondition handling, but the missing-file gate was an orchestration defect.** The owner had already requested a prompt-only handoff. Revision 1 remains OWNER-accepted and complete; the next handoff embeds its exact binding geometry, provenance SHA-256 and acceptance statement directly in the prompt. The coding agent is not allowed to create or infer the owner artifact, and no separate file transfer is required.

Exactly one next step: run the self-contained Revision 1 correction-round-1 prompt.

## UI-LAYOUT-R2 — measured closeout against accepted UI-SPEC-R0 — MEASURED FAIL / EVIDENCE ONLY

The agent reported clean current HEAD `f4416e750b53004df3a7e6cccb038e32b9d8dd29` before the run and clean final HEAD `40e8b10a0b30da27ba1e60b22ab6f8667135d452` after exactly one local evidence commit. The commit adds 86 files, all under `artifacts/uilayoutr2-spec-closeout/`; no product, resource, native, test, harness or live documentation file changed. No push; no remote.

The accepted artifact was present at `D:\TRAVELAPPS\FORGESHAPE_UI_SPEC_R0_ACCEPTED.html`. Observed SHA-256 exactly matched `9f1e866795a2b2169273a7db62807e544747e673758598779a35fd25553dbe64`, and the report quoted the owner acceptance source verbatim. All six requested window/density/font cells were established exactly on isolated `ForgeShape_Stage006` / `emulator-5580`; reserved `emulator-5554` was not contacted.

Measured host result against that artifact: 0/66 absolute anchor rows passed. Runtime width was 68 dp in every cell versus old expected 132/140/144/152; measured resting right inset was 8 dp; measured tops were 112.5 dp compact, 88 dp short landscape and 107 dp expanded. Independently of those old absolute values, the report found a real state-invariance defect: in L10/L13 the host right inset changed 8 → 320 dp and in E10/E13 8 → 360 dp during Exact-open / Exact+IME / Sculpt Details, then returned to 8 dp. Top and width did not drift.

Resting persistent rectangles passed the open/close contract: all binding before/after deltas were exactly 0 dp across 462 rows, including compact/short Objects, Construction history, expanded Construction Objects dock, expanded Sculpt Objects/direct brush controls, and absence of Construction history in Sculpt. Focused `UILR2-01..18` was rerun and passed 18/18. Device guards passed. The evidence package contains 36 raw screenshots and 36 mechanically generated overlays. The report logged 31 clipped on-screen intersections below 48 dp during scrolling but also showed every control's intrinsic geometry reaches ≥48 dp; the coordinator treats this as a specification-wording distinction to resolve, not as 31 independently proven undersized controls.

No repair was attempted, as required. Prior native/JVM/FullSharded evidence remained reusable because no product/test source changed.

Owner visual feedback after seeing the overlays: the old 132–152 dp host direction is too wide; the previous narrow, edge-hugging layout better preserves viewport workspace and is preferred as the next design direction. Coordinator verdict: **accept the report as evidence, reject correcting the product toward the old R0 absolute geometry, reopen UI-SPEC-R0 for owner revision.** The durable UI-OWNER-44 single-host / fixed-frame / no-translation invariant remains binding. No correction round has been consumed because this run made no product correction.

Exactly one next orchestration step: OWNER reviews and explicitly accepts the revised UI-SPEC-R0 geometry. Only then may correction round 1 be released; its first mandatory defect is the landscape/expanded Exact/Details host translation.

## R2 repository documentation current-truth reconciliation — PASS / DOCS ONLY

Baseline HEAD reported as `4a930506fdbbf92aadd03beabb92e26a9873ff3b`, clean before edit. Final HEAD reported as short hash `f4416e7` (the pasted report did not include a full 40-character final hash), one local docs-only commit, final `git status --short` clean, no push and no remote.

Reported scope: exactly `PROJECT_STATUS.md` and `CLAUDE.md` changed (+54 / −18). `ARCHITECTURE.md`, `PRODUCT.md` and `README.md` were read and reported unchanged. No product/test/build/artifact files, no device contact, no adb, no Gradle and no capture.

Reported reconciliations:
- made the missing owner-approved `UI-SPEC-R0` explicit as the blocker for R2 final visual acceptance and made OWNER creation/acceptance of that artifact the executable next step;
- replaced misleading “accepted implementation baseline” wording with “current technical implementation baseline”;
- reconciled stale 44→48 dp, two→three dark appearances, “light theme”, border-led active-state and old flush/docked-material capability rows;
- preserved the three relative/runtime R2 invariants as `VERIFIED` and added a separate `PENDING` row for absolute ≤4 dp placement against an owner-accepted layout;
- updated `CLAUDE.md` selector vocabulary from “two capsules in one selector row” to contextual vertical groups inside the single `WorkspaceTrailingHostView`, with World/Local omitted for Scale.

Reported acceptance evidence: baseline/clean preflight PASS; Markdown-only limited diff PASS; stale-phrase grep zero hits; technical R2 evidence preserved; other three docs absent from `git diff --name-only`; `git diff --check` PASS; final clean tree. One process-only deviation occurred when an initial commit subject contained a literal `@`; the agent amended the local commit message (`733b536` → `f4416e7`) rather than creating a follow-up commit. Content scope was reported unaffected.

Coordinator verdict: **reported PASS.** This closes the bounded documentation-current-truth defect and changes no product/runtime truth, no owner decision and no R2 correction-round accounting. Because the user supplied only the pre-change repo Markdown snapshot in this conversation, the coordinator has not independently inspected the final changed `PROJECT_STATUS.md` and `CLAUDE.md`; if live files later disagree with this report, live repo truth wins.

Exactly one product/orchestration next step remains: OWNER creates and explicitly accepts `UI-SPEC-R0`, then supplies its exact path, SHA-256 or literal `NOT_RECORDED`, and owner-acceptance source to the same-stage R2 evidence closeout.

## UI-LAYOUT-R2 — FUTURE-RUN evidence closeout invocation — PRECONDITION-NOT-MET / NO CHANGE

Baseline and final HEAD both reported as `4a930506fdbbf92aadd03beabb92e26a9873ff3b`; `git status --short` reported clean. The `FUTURE-RUN ONLY` prompt arrived with all three OWNER INPUT values still equal to their placeholder literals: `ACCEPTED_UI_SPEC_R0_PATH`, `ACCEPTED_UI_SPEC_R0_SHA256`, and `OWNER_ACCEPTANCE_SOURCE`.

The agent correctly stopped at preflight before running any command. No Git-history/spec search, adb, Gradle, build, measurement, capture, test, documentation edit or commit occurred. It explicitly withheld mock-derived tables and overlays because their expected side has no owner authority. No new deviation or debt was observed.

Coordinator verdict: **correct precondition handling; no stage advancement; no correction round consumed.** Re-running the evidence closeout before an owner-created and explicitly accepted `UI-SPEC-R0` exists is unauthorized churn.

The same coordinator review also inspected the owner-supplied pre-run repository Markdown snapshot. It found documentation-current-truth drift in `PROJECT_STATUS.md` and `CLAUDE.md` (stale 44-dp/appearance/border/docking rows, missing UI-SPEC blocker in current status, and pre-R2 selector-row vocabulary), while `ARCHITECTURE.md`, `PRODUCT.md`, and `README.md` are materially aligned with the current implementation. These are docs-only defects, not product failures.

Exactly one product/orchestration next step remains: owner creates and explicitly accepts `UI-SPEC-R0`. A bounded docs-only reconciliation may run independently and must not create or infer that artifact.

## UI-LAYOUT-R2 — post-acceptance evidence closeout invocation — PRECONDITION-NOT-MET / NO CHANGE

Baseline and final HEAD both reported as `4a930506fdbbf92aadd03beabb92e26a9873ff3b`. `git status --short` reported clean. The prompt arrived with `ACCEPTED_UI_SPEC_R0_PATH=<path>` still unfilled, so the agent obeyed the prompt's hard gate and stopped immediately.

No accepted `UI-SPEC-R0` path, hash or owner-acceptance source was supplied. Therefore the agent deliberately performed no specification lookup, measurement, screenshot capture, build, device contact, test rerun, documentation edit or commit. It also withheld all mock-derived tables because their expected side has no authority until the owner artifact is supplied.

The report reiterates one useful prior finding without re-deriving it: `UILR2-01`'s absolute baseline is self-referential to implementation resources, while `UILR2-02..18` are relational invariants already covered by the prior focused 18/18 and FullSharded 318/318 evidence. Only independent mock comparison can close the absolute anchor/≤4 dp gate.

Coordinator verdict: **report accepted as correct precondition handling; no advancement of `UI-LAYOUT-R2`; no new debt.** Re-running any closeout before the owner supplies and accepts `UI-SPEC-R0` is process churn and is not an authorized next action.

Exactly one next step: owner creates and explicitly accepts `UI-SPEC-R0`; only after that may the same-stage evidence closeout be launched with the exact artifact path/hash/acceptance source embedded in the prompt.

## UI-LAYOUT-R2 — visual evidence closeout — BLOCKED-MISSING-ACCEPTED-MOCK / NO CHANGE

Baseline and final HEAD both reported as `4a930506fdbbf92aadd03beabb92e26a9873ff3b`. `git status --short` reported clean. No product change, no capture, no device contact, no commit.

The agent obeyed the closeout hard gate and did not fabricate an expected layout. Reported search:
- whole working tree for `UI-SPEC-R0`/variants: no accepted specification;
- whole Git object/history scan for spec/mock/R0 candidates: no qualifying accepted mock;
- deleted-file history: only the already-retired `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md`;
- `docs/ui/wireframes/WF-1..WF-4.svg` explicitly rejected as substitutes because repository documentation identifies them as old, non-production Stage 015A-R wireframes;
- evidence/catalogue artifacts rejected because they do not contain ForgeShape binding rectangles.

Substantive finding: the shipped `UILR2-01..18` verification proves relative/runtime invariants such as fixed host geometry, downward expansion, restore-to-resting bounds and ≥48 dp, but its absolute host geometry is self-referential to implementation dimensions rather than compared to an independent accepted mock. Therefore the `≤4 dp` overlay contract has no expected side and cannot be truthfully evaluated.

Coordinator verdict: **report accepted; gate behaviour PASS; no advancement of `UI-LAYOUT-R2`.** The report exposes a coordinator-plan defect: v4.0.3 marked `UI-SPEC-R0` COMPLETE without evidence. `UI-SPEC-R0` returns to owner-owned READY; R2 remains OPEN. The owner must create and explicitly accept the 4.2 artifact before evidence closeout can resume. The mock must express intended geometry independently rather than copy the current implementation.

Exactly one next step: owner creates and explicitly accepts `UI-SPEC-R0`.

## UI-LAYOUT-R2 — unified right-host static layout — TECHNICAL PASS / VISUAL VERDICT PENDING

Baseline `54c3dfa09c7b0fd9351f6c8967b199c3380e3c1a` → implementation/test HEAD `8d9ebbf0f73e278ed7383ae89c054afa2bcce9f3` → final repo HEAD `4a930506fdbbf92aadd03beabb92e26a9873ff3b`. Reported commits: `27dc28d` product implementation, `8d9ebbf` verification, `4a93050` documentation. Final tree reported clean; no push.

Reported implementation delta:
- `WorkspaceTrailingHostView` is the single external right contextual surface with internal bounded vertical scrolling;
- fixed top/right/width, downward-only contextual expansion;
- Shape and Transform share the surface; Scale hides World/Local without changing the external frame;
- Sculpt uses the same external geometry;
- Exact/Details hide conflicting bottom chrome during entry/open/exit and restore exact resting bounds;
- IME keeps the right host vertical through root-owned animated inset handling;
- no JNI/domain/transform-math/Sculpt-semantic change reported.

Reported verification:
- focused `UILR2-01..18`: 18/18;
- JVM 59/59;
- native 13 suites / 2043 checks / 0 failures; `FORGESHAPE_NATIVE_VIEWPORT_OK`;
- debug/release `arm64-v8a` + `x86_64`: PASS;
- DEV2-01..07 and DEV3-01..06: PASS;
- authoritative FullSharded 318/318 with shard totals `64 + 64 + 66 + 63 + 61`, missing 0, duplicates 0, unexpected 0, execution missing 0, `FULL_SHARDED_SUITE_PASS`.

Evidence reported in local `artifacts/uilayoutr2/`: 16 screenshots, Exact+IME capture, Sculpt Details capture, continuous walkthrough MP4, self-test transcript and validation summary. These local Windows paths were not accessible to the coordinator in the conversation, so the images themselves were not visually adjudicated.

Coordinator assessment:
- **technical result accepted as reported; stage status `OPEN`;** the reported test gate materially exceeds the layout-only minimum in 6.4;
- final visual PASS is withheld until the accepted UI-SPEC-R0 mock and device captures are compared under 4.2/4.4, including overlay deviation ≤4 dp, invariant rail top/right/width across the state/configuration/font-scale matrix, and ≥48 dp targets;
- the pasted report lacks the required `Test ID | PASS/FAIL | short evidence` table and does not explicitly state font-scale 1.3 matrix proof; both are evidence/report gaps, not demonstrated product failures;
- Radius/Strength is retained as separate P2 visual debt and does not expand R2 scope.

Exactly one next step: owner/coordinator visual review after the same-stage evidence package exposes the accepted mock overlays and per-test evidence. No motion/morph or new product stage before that verdict.

## TEST-HARNESS-R1 — authoritative exhaustive-sharded instrumented runner — PASS
*(backfilled in v4.0.0; accepted in plan v3.68.0)*

Baseline `1e3bcd23a5416a9442a00673bd25875e0819dc7d` → final `54c3dfa09c7b0fd9351f6c8967b199c3380e3c1a`. One test-infrastructure commit `54c3dfa`. `PRODUCT_DIFF_COUNT=0`. Final tree clean. No push.

Accepted evidence:
- the official runner supports authoritative exhaustive sharding;
- runtime discovery comes from the current test APK through AndroidJUnitRunner;
- no hand-maintained fixed inventory and no hard-coded `300`;
- deterministic class-atomic partitioning;
- the runner proves complete, non-overlapping assignment;
- any assertion failure, abort, invalid result or missing shard fails the aggregate;
- after infrastructure recovery the whole sharded job restarts from shard 1;
- ordinary `-TestClass` runs cannot emit the authoritative full-suite marker.

Runtime result: 20 instrumented classes; 300 tests; assigned union 300; executed union 300; missing 0; duplicates 0; unexpected 0; execution missing 0; failed shards 0; aborted shards 0; `FULL_SHARDED_SUITE_PASS`; exit code 0. JVM 59/59. Debug, release and androidTest APK builds PASS; native CMake debug/release for `arm64-v8a` and `x86_64` PASS.

Coordinator verdict: **PASS.** The test-process blocker is closed. Owner policy recorded as TEST-OWNER-01.

## UI-ARCH-R1 — bounded structural extraction — PASS
*(backfilled in v4.0.0; accepted in plan v3.65.0/v3.68.0)*

Final HEAD `1e3bcd23a5416a9442a00673bd25875e0819dc7d`.

Accepted: the structural extraction succeeded. `WorkspaceTrailingHostView` now owns right-cluster composition and presentation; root LOC, constructor size, field and accessor counts decreased materially; reported before/after semantic geometry showed zero differences. Native/domain authority and primary-surface ownership boundaries remain healthy.

Held at first review because the JVM test count changed from 60/60 to 59/59 without explanation; resolved in v3.67.0 as the intentional removal of a dead `railDocked`-only test with meaningful device coverage retained.

Coordinator verdict: **COMPLETE / R2-READY.** No remaining architecture or verification blocker prevents `UI-LAYOUT-R2`. Owner policy recorded as ARCH-OWNER-05.

## UI-ARCH-AUDIT1 — read-only UI source architecture audit — PASS
*(backfilled in v4.0.0; accepted in plan v3.64.0)*

Verdict `ARCH-LOCAL-DEBT` at reported HEAD `717cf70`.

Confirmed healthy: native/domain authority, UI state, breakpoint math, primary-surface coordination and leaf components.

Recorded findings: `EditorWorkspaceView` (~2952 LOC, ~467-line constructor) is the local chrome god-object and the primary maintainability risk; stale/dead `railDocked` semantics and distributed IME/right-cluster presentation policy are bounded debt; test coupling to `getChildAt`, exact hierarchy and root accessors contributes to false confidence.

Outcome: authorized `UI-ARCH-R1` as a no-redesign structural refactor.

## UI-LAYOUT-R1 — static workspace stabilization — TECHNICAL PASS / VISUAL REJECT
*(backfilled in v4.0.0; accepted in plan v3.62.0)*

Reported HEAD `717cf70`. Native 2043/2043, JVM 60/60, focused 20/20, final full instrumented 288/288.

Accepted improvements: surface exclusivity, Exact Transform, IME resilience, 48 dp resilience and gizmo legibility.

Final visual acceptance rejected: owner-visible evidence still showed unrelated bottom controls relocating when a sheet opens, and an IME-specific alternate right-control layout.

**The finding that matters most for the whole project:** the existing `UILR1-15` stability test *encoded a weaker contract than the owner intended*. The agent implemented and verified exactly what was written; what was written was not what was meant. This is the evidence on which MASTER PLAN 4.2 (`UI-SPEC-R0`, measurable layout DoD) rests.

Outcome: added UI-OWNER-44 and promoted `UI-LAYOUT-R2`.

## UI-AUDIT2A — Impeccable Native UI Audit — PASS

Reported evidence-only final:
`6007f69`

Hard gate:
PASS — local Impeccable files, hashes, headings, rules and rule→finding mapping reported in `artifacts/uiaudit2a/SKILL-PROOF.md`.

Evidence:
- 49 screenshots;
- 10 measurement records;
- 5 adaptive configurations including font scale 1.3;
- 13/13 native self-tests;
- semantic-id/runtime-bounds harness.

Key retained findings:
- P0 constrained-height collapse of the trailing rail below 48 dp;
- P1 Back/context dismissal failure;
- P1 gizmo hit/spacing weakness;
- P1 text contrast;
- P1 long-number clipping;
- P1 Apply Transform continuation/fold;
- unitless Scale presentation issue.

Corrections to UI-AUDIT1:
- scrim touch leak rejected as false positive;
- display popover auto-close rejected as non-reproducible.

Coordinator verdict:
**PASS as an independent Impeccable audit. Findings remain provisional until all four audits are synthesized.**

## UI-AUDIT1 — Three-Tool Visual/Motion Audit — PARTIAL PASS

Reported baseline:
`bd3f7d5`

Reported evidence-only final:
`573bd45`

Reported scope:
- 23 captures across compact portrait, short landscape and expanded;
- Impeccable 4.1.1, Emil Kowalski design/motion and Remotion 4.0.517 skills read from disk;
- product source/resources/build untouched.

Accepted provisional findings:
- Android Back/context-surface P1;
- short-landscape 48 dp violation P1;
- compact trailing-cluster anchor jump P1;
- long-number clipping P1;
- numeric append/edit behavior P1;
- Apply Transform continuation/fold problem P1;
- unit chips under unitless Scale P1;
- gizmo visual/hit-size weakness P1;
- Stage 020R3 starburst classified as `SCULPT_QUALITY`.

Coordinator rejection of `COMPLETE` depth:
- insufficient lens-by-lens comparison;
- insufficient screenshot/evidence indexing;
- insufficient per-layout/per-control visual composition detail;
- insufficient per-mode gizmo analysis;
- insufficient motion and Remotion prototype detail.

Coordinator verdict:
**PARTIAL PASS. Continue with UI-AUDIT1B before any correction implementation.**

## Stage 020R3 — Scale→Sculpt World/Display-Metric Brush Correction — PASS

Reported baseline:
`76cd4d1`

Reported final:
`bd3f7d5`

Reported commits:
- `df9b26d fix: sculpt in the world metric under a non-uniform Scale`
- `bd3f7d5 test: record Stage 020R3 world-metric brush evidence`

Accepted result:
- world/display brush metric corrected under non-uniform Body Scale;
- all four shipped brushes share the scale-aware affected-region metric;
- Clay/Inflate normal deposition corrected where required;
- no Scale baking/reset;
- transition/history/ObjectId/sculpt retention preserved;
- both-ABI build, native/JVM/device guards and clean full 268-test instrumented rerun reported green;
- runtime evidence verifies the displayed/world radius contract at identity, non-uniform Scale and mixed rotation.

Separate observation:
- supplied combined-stroke screenshots show stronger faceting/starburst on the stretched low-resolution mesh; classify during UI-AUDIT1 as mesh-sampling quality debt vs remaining technical defect, without expanding scope automatically.

Coordinator verdict:
**PASS. Proceed to UI-AUDIT1.**

## DOC-R2 — Current-Truth Reconciliation + Compaction — PASS

Reported baseline:
`a90e7df`

Reported final:
`76cd4d1`

Reported commit:
- `docs: reconcile current ForgeShape truth`

Accepted report delta:
- reconciled stale Undo/Redo wording in PRODUCT;
- removed stale PRODUCT roadmap/proposal text;
- corrected PROJECT_STATUS direct-gizmo, Sketch/Extrude, ABI/test-evidence and current-next wording;
- corrected README both-ABI and Stage 020R2 gizmo/log vocabulary;
- replaced hard raw doc line-count cap with current-truth/navigability policy;
- removed historical/migration contradictions from ARCHITECTURE while preserving current invariants;
- deleted `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md`;
- rescued only two unique durable invariants from that pack into ARCHITECTURE;
- reported no runtime/source/build changes;
- net live documentation reported 701 lines smaller.

Remaining low-priority doc debt:
- orphaned Stage 015A-R SVG wireframes;
- dated Stage 015B framing around a current status decision;
- unreviewed Stage 015C shading comparison artifact.

Coordinator verdict:
**PASS. Proceed to Stage 020R3.**

## Stage 020R2 — Full Transform Gizmo — TECHNICAL CORE PASS / VISUAL HOLD

Baseline:
`f58c7b8`

Reported final:
`a90e7df`

Commits:
- `d10f885` — `feat: expand construction transform gizmo`
- `a90e7df` — `test: track full transform gizmo evidence`

Reported implementation accepted provisionally:
- nine authoritative transform values;
- `T·Rz·Ry·Rx·S`;
- positive Local Scale;
- World/Local Move;
- World/Local Rotate with mixed-orientation correction;
- World/Local planar Move;
- Local axis/plane/uniform Scale;
- Exact Transform Scale;
- Stage 019 history/transaction integration;
- scale-aware normal transform and picking;
- adaptive selector layout;
- Sculpt hides Construction gizmo/selectors.

Reported verification:
- native 13 / 1961 / 0;
- JVM 60/60;
- compact 268/268;
- short 144/144;
- expanded 144/144;
- DEV2/DEV3 PASS;
- S020R2-01..22 PASS;
- final tree clean.

Coordinator screenshot verdict:
**not final visual PASS.**
Primary findings:
- wireframe/debug-like gizmo styling;
- insufficient default stroke/handle legibility;
- Scale gizmo visual clutter;
- weak evidence of active-handle emphasis;
- fragmented trailing chrome composition;
- Exact Transform numeric text visibly clipped;
- broader stable-anchor/coherence audit still required.

Coordinator technical follow-up:
- non-uniform Scale → Sculpt brush anisotropy is user-reachable and must be source/product-adjudicated before final closeout;
- harness-crash rerun method deviated from the preferred full-suite rerun protocol and should be reconciled in documentation/testing policy, not treated as a product regression.

Current classification:
**Stage 020R2 technical core accepted provisionally; final stage closeout on HOLD pending docs/source audit and focused correction decision.**

> **v4.0.0 note.** The classification line above is historical. `Stage 020R2` was closed as `ACCEPTED-AS-IS` at `a90e7df` on 2026-08-29 under MASTER PLAN 9.1 W-4: its technical core was accepted in v3.47.0 and its visual objections are owned by `UI-LAYOUT-R2` and UI-OWNER-44.

---

# PART B — VERSION NOTES

*Newest first, verbatim. See "Known gaps in the version sequence" above.*

### v4.0.58 — 2026-09-07
- accepted `PASS-MIRROR-01-OWNER-LATER-READY`;
- recorded baseline `94daaa92494988e6604d8dcf2db6f8a535a9a9f3` and short final commit `3283089`;
- retained missing full final hash as a reporting debt to resolve in next preflight rather than fabricating it;
- marked MIRROR-01 COMPLETE / OWNER LATER;
- accepted positive-scale Construction world-plane mirror contract and focused evidence;
- kept TEST-OWNER-03 reduced-testing policy;
- released SCULPT-H1 as READY;
- reconciled SCULPT-H1 with existing SCULPT-UNDO-R0: history storage/capacity remains existing delta history, while H1 supplies navigator/jump/hover-preview UX only;
- kept Stage 025 immediately after SCULPT-H1.

### v4.0.57 — 2026-09-07
- accepted `PASS-STAGE-020M-OWNER-LATER-READY`;
- recorded final clean HEAD `94daaa92494988e6604d8dcf2db6f8a535a9a9f3`;
- marked Stage 020M technical/repository COMPLETE and OWNER LATER;
- recorded exact source-derived body dimensions, anchor solver and transient Relative Scale contract;
- preserved OQ-01 and OQ-02 as unanswered;
- reconciled the newer clean Stage 020M baseline with the older dirty SEL-OUT snapshot without fabricating the missing Stage 018A report;
- treated SEL-OUT and Stage 018A as present in the clean baseline, with their user-facing checks still OWNER LATER;
- released MIRROR-01 as READY with R1 limited to current Construction primitives mirrored across XY/XZ/YZ world planes;
- retained reduced ordinary-stage test budgets and no-FullSharded default.

### v4.0.56 — 2026-09-06
- recorded OWNER override to skip SEL-OUT-R1 closeout testing and continue implementing features;
- cancelled the dedicated SEL-OUT test-isolation closeout prompt;
- retained the known pre-existing SpatialSketchTest isolation defect as deferred test debt;
- strengthened TEST-OWNER-03 to <=20 minute target / 30 minute hard stop for ordinary feature verification;
- moved more visual/ergonomic/multi-feature validation into OWNER LATER test packs;
- released Stage 018A Rename/Visibility/Lock/Duplicate as the single READY stage;
- required the Stage 018A prompt to preserve and commit the existing dirty SEL-OUT candidate before new feature coding;
- kept Delete semantics and the pending IMPORT-01B owner verdict untouched.

### v4.0.55 — 2026-09-06
- accepted SEL-OUT-R1 as technical feature PASS / OWNER-retest-ready, not repository-complete;
- recorded renderer outline implementation and green focused/device evidence;
- recorded that shard-5 SpatialSketchTest failure predates the feature and is a test-isolation defect;
- authorized a narrow SpatialSketchTest isolation correction;
- prohibited FullSharded in SEL-OUT-R1 closeout;
- added TEST-OWNER-03: ordinary feature prompts use shorter proportional automation and defer more visual/ergonomic/multi-step UX checks to OWNER LATER test packs;
- kept high-risk data/concurrency/geometry correctness automated;
- released `SEL-OUT-R1-CLOSEOUT` as the single READY stage;
- kept IMPORT-01B Delete OWNER verdict parked.

### v4.0.54 — 2026-09-06
- accepted `PASS-TEST-RUNTIME-R1`;
- recorded final clean HEAD `538623c26e0122d97303150ebbb52eb3ea0d32b3`;
- marked TEST-OWNER-02 runner mechanics implemented;
- accepted shard-only/PlanOnly/same-tree resume/checkpoint/fingerprint/time-budget/attempt-limit/failure-classification tooling;
- preserved the rule that shards from changed APK/test fingerprints cannot combine into an authoritative PASS;
- accepted remaining tooling debt: full aggregate summary path not yet exercised, resume per-device, Ctrl+C USER_ABORTED record absent;
- recorded the `.gitignore` evidence-log correction and restored cited UI-PREF logs;
- released existing approved `SEL-OUT-R1` as the single READY feature stage;
- kept IMPORT-01B Delete owner verdict parked and untouched.

### v4.0.53 — 2026-09-06
- accepted `PASS-UI-PREF-R1-OWNER-WAIVER-CLOSEOUT`;
- recorded clean final HEAD `7f2d234f887766390de8b4a0820ee522a5b4a75f`;
- marked UI-PREF1/UI-PREF2/UI-PREF-R1 and closeout COMPLETE;
- preserved the explicit fact that UI-PREF-R1 has no final FullSharded PASS and was accepted by OWNER waiver;
- accepted focused legibility/settings/JVM verification;
- refined the late `uilr106` failure classification to emulator guest/window-system infrastructure after identical APK PASS on clean restart;
- kept gizmo handle style deferred;
- kept IMPORT-01B Delete owner verdict parked;
- unblocked `TEST-RUNTIME-R1` as the single READY tooling stage;
- added the rule that resumed authoritative shard results may only combine identical runtime/test fingerprints.

### v4.0.52 — 2026-09-06
- recorded UI-PREF-R1 `EVIDENCE-BLOCKED` with a dirty, uncommitted candidate;
- recorded OWNER manual acceptance of UI-PREF-R1 behavior and explicit waiver of its missing final FullSharded aggregate;
- added TEST-OWNER-02 with <=90 minute aggregate target, 120 minute hard stop, max two automatic aggregate attempts, and focused/shard reruns after failures;
- classified guest/system-server/IME/harness failures separately from product failures;
- prohibited a FullSharded rerun in the UI-PREF-R1 closeout;
- released `UI-PREF-R1-CLOSEOUT` as the single READY stage;
- planned `TEST-RUNTIME-R1` after closeout for shard-only/resume and timing support;
- kept IMPORT-01B Delete owner verdict parked and untouched.

### v4.0.51 — 2026-09-05
- recorded OWNER sequencing override to continue implementation while the Delete manual verdict is temporarily unavailable;
- released `UI-PREF-R1` as a combined execution of existing approved UI-PREF1 + UI-PREF2;
- scope includes Settings Hub, app-level preference persistence, handedness/mirrored workspace edge zone, five palettes and gizmo appearance preferences;
- explicitly prohibited changes to Delete/history semantics and kept IMPORT-01B owner verdict pending;
- declared the old exact-baseline Delete HOLD operationally stale after the new stage commits, without cancelling the underlying owner check;
- reconciled CAD-A3 and APP-H1 roadmap rows to COMPLETE after accepted owner flow;
- retained Opus 5 High as the coding model.

### v4.0.50 — 2026-09-05
- accepted `PASS-CAD-A3-C2-OWNER-ACCEPTED-DELETE-PENDING`;
- recorded final clean HEAD `565d4002dc1306161a72bf86bb90cba7144c7316`;
- accepted the docs-only PROJECT_STATUS synchronization with no runtime tests;
- clarified that repository-local absence of UI-OWNER-47/UI-OWNER-48/ARCH-OWNER-14 did not mean those canonical OWNER decisions were absent;
- kept IMPORT-01B Delete→Undo→Redo owner verdict pending;
- released no feature stage;
- prepared one HOLD prompt for Delete owner verdict synchronization/correction only.

### v4.0.49 — 2026-09-05
- recorded OWNER real-device PASS for CAD-A3-C2 / SKETCH-UX-R1;
- marked UI-OWNER-47, UI-OWNER-48 and ARCH-OWNER-14 OWNER accepted;
- did not infer the still-missing IMPORT-01B Delete→Undo→Redo owner verdict;
- released one docs/status-only Opus 5 High synchronization prompt;
- released no new product feature stage.

### v4.0.48 — 2026-09-05
- accepted `PASS-POST-AUDIT-HARDEN-R1-OWNER-RETEST-READY`;
- recorded baseline `af6a719a7fad847fc3ad06289cdfd5a077c46822` and final clean HEAD `5cf981a9c89da8e99366592cb77b0f1cdff1c361`;
- closed F-08/F-16/F-11/F-15/F-18;
- accepted mechanical expansion of F-08 from seven highlighted readers to all twelve unlocked calls of the same defect class;
- recorded release selftest exclusion and release `.so` size reduction;
- recorded FullSharded 40 classes / 524 tests / PASS;
- kept larger audit architecture/performance debt deferred;
- released no new coding stage;
- returned sequencing to OWNER CAD-A3-C2 + Delete verdicts.

### v4.0.47 — 2026-09-05
- accepted `PASS-DEEP-AUDIT-R1-WITH-DEBT`;
- recorded baseline `be5b729c354a9876e24fbef54c630ee21fbe95d3` and final reported prefix `af6a719`;
- accepted three fixed P1 findings F-01/F-02/F-03 with red/green tests and FullSharded 519/519;
- accepted comment compression and all-Markdown truth/future-contract audits;
- restored default coding-agent preference to Opus 5 High after the Fable audit override;
- prioritized F-08/F-16/F-11/F-15/F-18 for one bounded post-audit hardening stage;
- deferred F-06/F-07/F-09/F-10/F-13/F-14 and other broader architecture/performance debts;
- released `POST-AUDIT-HARDEN-R1` as the single READY stage;
- kept owner device verdicts parked until hardening completes.

### v4.0.46 — 2026-09-04
- recorded explicit OWNER sequencing override to run a holistic audit before more feature work;
- added AUDIT-OWNER-02;
- approved Fable 5.1 XHigh specifically for DEEP-AUDIT-R1 while retaining Opus 5 High as the default afterward;
- required audit coverage across product, architecture, native/Java/JNI, concurrency/lifecycle, persistence, geometry/CAD/Sculpt/import/export, renderer/resources, performance/memory, security/file handling, build/ABI, UI/input/accessibility and tests;
- required classification and reconciliation of every tracked Markdown document, including future-functional assumption drift;
- required systematic shortening of oversized/redundant code comments while preserving real invariants/licensing/format/math/concurrency commentary;
- allowed only bounded local fixes for proven high-severity defects; major rewrites remain follow-up findings;
- released DEEP-AUDIT-R1 as the single READY stage and parked owner-verdict work during the audit.

### v4.0.45 — 2026-09-04
- accepted `PASS-CAD-A3-C2-SKETCH-UX-R1-OWNER-RETEST-READY` at clean final HEAD `be5b729c`;
- recorded full-screen Home, immediate first Sketch, orientation navigator, technical line dimension overlay, Arc/Spline and staged Edit Sketch as technically implemented;
- recorded CADB v3 curve persistence with all prior 22 fixtures byte-identical and six new independent fixtures;
- recorded FullSharded 39 classes / 519 tests PASS;
- marked UI-OWNER-47, UI-OWNER-48 and ARCH-OWNER-14 IMPLEMENTED_TECHNICAL / OWNER retest pending;
- recorded bounded deviations: no live drag length, no tablet capture, spline control editing by redraw;
- flagged ambiguous report wording around a curve-specific kill/reopen test for manual OWNER confirmation;
- recorded OWNER preference to use **Opus 5 High** for future ForgeShape coding prompts unless explicitly overridden;
- released no new coding stage before OWNER real-device verdict.

### v4.0.44 — 2026-09-04
- recorded OWNER real-device UX failure of the modal-card Home and empty-3D first-plane chooser;
- approved UI-OWNER-47 full-screen Home;
- approved UI-OWNER-48 immediate flat first Sketch, top-right orientation navigator and on-canvas editable line-length annotation;
- partially superseded UI-OWNER-46 only for first-sketch world-plane selection; physical planar-face selection remains binding for subsequent sketches;
- approved ARCH-OWNER-14 Arc/Spline authored truth and staged Edit Sketch;
- kept semantic face/TopoRef/dependency/CADB-v2 technical core closed and reusable;
- released `CAD-A3-C2 / SKETCH-UX-R1` as the single READY coding stage;
- kept booleans, custom planes, solver breadth and sketch-on-Imported/Sculpt out of scope.

### v4.0.43 — 2026-09-04
- accepted `PASS-CAD-A3-APP-H1-OWNER-RETEST-READY` at clean HEAD `c5864c69902be127a12daa5ec81e86b0d07627bb`;
- completed APP-H1 technically using Home=no-project plus transient New-CAD bootstrap until first Sketch→Extrude commit;
- recorded real Home/Open/New Project→CAD/Sculpt and dirty Save/Discard/Cancel flow;
- completed independent CADB-v2 PowerShell oracle and six new fixtures while preserving all previous fixture bytes;
- recorded independent discovery/fix of a mistyped production FNV-1a topology-lineage basis before any shipped v2 file existed;
- accepted required device Home/New/Open/spatial E2E and injected stylus-hover verification without claiming hardware hover;
- accepted phone screenshot/contact-sheet evidence and recorded tablet capture as non-blocking harness debt;
- recorded FullSharded 37 classes / 504 tests / 504 PASS;
- marked CAD-A3-C1 TECHNICAL COMPLETE and CAD-A3 / APP-H1 / UI-OWNER-46 / ARCH-OWNER-13 OWNER-retest pending;
- corrected stale MASTER owner-decision count text from 64 to 67;
- released no new coding stage before OWNER Home+CAD verdict.

### v4.0.42 — 2026-09-04
- accepted agent token `FAIL-CAD-A3-APP-H1` as an honest incomplete-stage result, not a broken-tree verdict;
- recorded final clean HEAD `4556f769fb424c65b717b2fac7a292aaa2b437c0`;
- accepted semantic face topology, TopoRef, dependency graph, exact sketch camera, adaptive grid, face-supported repeated Extrude and CADB v2 production codec as green partial delivery;
- recorded FullSharded 36 classes / 493 tests / 493 PASS;
- kept UI-OWNER-46 PARTIAL because Home/New Project is not implemented;
- marked ARCH-OWNER-13 technically implemented pending full stage closure;
- identified missing independent CADB-v2 PowerShell corpus as a data-contract blocker;
- identified missing Home/dirty-guard/hover device E2E and screenshot contact sheet as acceptance evidence gaps;
- chose transient pre-project CAD bootstrap as the preferred APP-H1 solution before considering a broad zero-body ProjectDocument refactor;
- released `CAD-A3-C1` as the single READY continuation and prohibited booleans/next CAD feature work.

### v4.0.41 — 2026-09-03
- recorded explicit OWNER approval of UI-OWNER-46 and ARCH-OWNER-13;
- completed CAD-R0-UX with a fresh official-doc interaction review and adopt/adapt/defer/reject record;
- approved no-project Home → Open File / New Project → CAD/Sculpt;
- approved direct spatial XY/XZ/YZ plane selection for the first CAD sketch;
- approved direct stylus/finger selection of supported planar CAD faces for subsequent sketches;
- required exact orthographic Sketch camera alignment instead of the general ~87° pitch-clamped view;
- required semantic feature-owned TopoRef persistence and prohibited render-triangle face identity / nearest-face retargeting;
- pulled CAD-A3 forward as the single READY coding stage;
- folded APP-H1 implementation into CAD-A3 to keep one coherent mobile user journey and one WIP coding stage;
- retained booleans, custom planes, sketch-on-Imported/Sculpt, solver breadth and advanced CAD features out of scope.

### v4.0.40 — 2026-09-03
- accepted `PASS-CAD-R0-A1A2-OWNER-RETEST-READY` at clean HEAD `3a00e0be6c528c9f96ae1f004bdaf70bc67afb82`;
- recorded third durable Cad representation, Sketch domain, profile extraction, triangulation and Extrude New Body;
- recorded project-history, CADB persistence, autosave/recovery, Objects/gizmo/export integration;
- recorded FullSharded 35 classes / 491 tests / 491 PASS;
- recorded CAD→Sculpt as an intentional R0 refusal;
- recorded OWNER-retetest focus on 0.25 m grid, ~87° sketch view, circle32 faceting and profile selection;
- re-surfaced representation-dispatch debt due the new third representation;
- marked SCULPT-UNDO-R0 / ARCH-OWNER-12 OWNER accepted;
- left IMPORT-01B Delete OWNER check pending;
- released no new coding stage before OWNER CAD/Delete verdicts.

### v4.0.39 — 2026-09-03
- accepted `PASS-SCULPT-UNDO-R0-OWNER-RETEST-READY` at final clean HEAD `2bcfa3fa11fd96fa934887f16e55caefbee496d9`;
- recorded dedicated per-body SculptHistory separate from ProjectHistory;
- recorded compact touched-vertex before/after deltas with explicit hasEdits restoration;
- recorded bounds: 32 entries, 4 MiB per body, 1 MiB per entry, deterministic eviction;
- recorded monotonic revision semantics and source/transform immutability;
- recorded same-process Back→Resume history retention and deliberate non-serialization across save/reopen;
- accepted real MotionEvent test-helper correction that closed a stale UI-state coverage gap;
- recorded FullSharded 34 classes / 482 tests / 482 PASS and clean final tree;
- partially retired planned SCULPT-H1: core Undo/Redo is now delivered; only optional history-navigator UX remains later;
- recorded non-blocking future scaling debt for possible aggregate multi-body Sculpt-history memory budgeting;
- left only OWNER manual Sculpt Undo/Redo and Objects Delete/Undo/Redo checks before proceeding to CAD-R0-UX.

### v4.0.38 — 2026-09-03
- recorded explicit OWNER approval of ARCH-OWNER-12;
- superseded the old `Sculpt has no undo` rule;
- preserved the stronger boundary that project/object history never stores Sculpt vertex state;
- defined a separate bounded volatile Sculpt Undo/Redo history with one completed stroke per entry;
- preferred compact touched-vertex before/after deltas over full mesh snapshots;
- required deterministic step/memory bounds and Redo branch invalidation;
- bound the same history to Construction and Imported Sculpt;
- defined Back→Resume in-session retention and no `.forge`/autosave/recovery serialization of history;
- defined contextual reuse of existing Undo/Redo controls in Sculpt;
- released `SCULPT-UNDO-R0` as the single READY coding stage;
- folded removal of abandoned C1 artifacts into the new stage preflight instead of running a standalone cleanup.

### v4.0.37 — 2026-09-03
- accepted the C1 agent report as confirmation of a false-premise task, not a product failure;
- recorded independent live-repo evidence that Sculpt Undo is intentionally absent across docs, UI, JNI and IMP01B-06;
- recorded that C1 made no production/test/document changes and requires no test rerun;
- decided not to commit the untracked `artifacts/import-01b-c1/` bundle from the cancelled correction;
- opened exactly one hygiene action to remove those artifacts and restore a clean accepted HEAD;
- kept only the OWNER Delete → Undo → Redo check pending for IMPORT-01B;
- kept Sculpt Undo as a future unapproved feature idea.

### v4.0.36 — 2026-09-03
- corrected the v4.0.35 premise after Claude inspected live repository rules;
- recorded that Sculpt intentionally has no per-stroke Undo and Sculpt vertex/revision state is excluded from project history;
- cancelled IMPORT-01B-C1 with no product code change required;
- restored IMPORT-01B technical PASS interpretation;
- recorded OWNER real-file PASS for Imported Mesh import + Sculpt entry/edit and marked ARCH-OWNER-11 OWNER accepted;
- kept only UI-OWNER-45 Delete / Undo / Redo manual owner retest pending;
- classified Sculpt Undo as a future unapproved feature idea requiring a dedicated history design;
- removed the Sculpt-Undo blocker from CAD sequencing.

### v4.0.35 — 2026-09-03
- recorded OWNER real-file PASS for Import GLB and Imported Mesh Sculpt entry/edit;
- recorded OWNER failure for Sculpt action Undo/Redo in the actual app;
- classified the failure as a bounded IMPORT-01B correction because IMP01B-06 explicitly required real sculpt stroke Undo/Redo;
- withheld full IMPORT-01B OWNER acceptance;
- kept Delete owner retest pending unless separately confirmed;
- held CAD release until the known Sculpt Undo regression is corrected and manually rechecked;
- opened exactly one targeted correction step, IMPORT-01B-C1.

### v4.0.34 — 2026-09-03
- accepted `PASS-IMPORT-01B-OWNER-RETEST-READY` at final clean HEAD `04f28df49f5000e863628571cdf5f8f076dac7e0`;
- recorded authoritative FullSharded 33 classes / 472 tests / 472 PASS with all integrity counters zero;
- recorded that the closure run changed no runtime/test code and preserved the exact candidate tree fingerprint;
- recorded bounded AVD recovery: a snapshot-resumed wedged system required a real guest reboot; no global ADB reset and no reserved emulator contact;
- marked ARCH-OWNER-11 and UI-OWNER-45 technically implemented, OWNER retest pending;
- recorded Imported Mesh reversible Sculpt, IMPT+SCUL, real Delete/Undo/Redo and renderer pruning as technically complete;
- kept active-Sculpt Delete refusal as accepted bounded deviation;
- added non-blocking tooling debt for missing cold-boot support in the emulator launcher;
- removed coding-stage readiness: the only next action is OWNER real-file Sculpt + Delete retest.

### v4.0.33 — 2026-09-02
- accepted the agent's `EVIDENCE-BLOCKED` result for IMPORT-01B rather than treating partial aggregate coverage as PASS;
- recorded that all focused/native/JVM/build/device/corpus evidence is green but FullSharded is only 281/472 on the final candidate tree;
- recorded two real defects caught by aggregate attempts and their reported targeted fixes, followed by two infrastructure failures;
- corrected the report's acceptance accounting: final git-clean/commit criterion is also outstanding because HEAD remains `f4d6cf7` with a dirty stage worktree;
- accepted refusal to Delete during an active Sculpt interaction as a bounded non-blocking deviation while retaining Delete for bodies with stored Sculpt state;
- corrected the stale IMPORT-01A roadmap status to COMPLETE;
- kept IMPORT-01B as the single READY continuation, strictly for aggregate evidence closure and commit/clean finalization;
- prohibited feature expansion while closing evidence.

### v4.0.32 — 2026-09-01
- recorded OWNER durable real-file PASS for IMPORT-01A: import, gizmo Move/Rotate/Scale and `.forge` reopen all work on the actual low-poly model;
- closed IMPORT-01A as COMPLETE / OWNER accepted;
- approved ARCH-OWNER-11 for Imported Mesh → current reversible Sculpt semantics without fake Construction Source;
- bound Imported Mesh source plus optional SCUL as the intended `.forge` state, subject to live codec proof;
- approved UI-OWNER-45 to pull only generic SceneObject Delete forward from Stage 018A;
- required Delete to be real project mutation with Undo/Redo, deterministic active selection and no ghost render/pick/save/export state;
- kept rename/visibility/lock/duplicate, OBJ/FBX/materials and BRIDGE-R1 out of scope;
- released IMPORT-01B as the single READY coding stage.

### v4.0.31 — 2026-09-01
- accepted `PASS-ARCH-HEALTH-REVIEW-WITH-DEBT` at final clean HEAD `f4d6cf73648cf6de1259b0a7d1cc79c7959d5802`;
- recorded the bounded JNI/autosave synchronization fix and redundant Construction-tessellation removal;
- accepted native/JVM/build + five focused device suites as proportional review evidence; FullSharded was not claimed;
- retained an explicit evidence caveat that no dedicated race-detector/TSAN proof was reported;
- recorded architecture debts without opening cleanup feature work;
- highlighted the `.forge` `nextObjectId < 2^60` namespace check as compatibility-sensitive later debt;
- corrected IMPORT-01A roadmap state to closed-vocabulary `OPEN` while it awaits OWNER verdict;
- kept IMPORT-01B blocked and OWNER durable real-file retest as the single next action.

### v4.0.30 — 2026-09-01
- accepted `PASS-IMPORT-01A-OWNER-RETEST-READY` at final clean HEAD `4c296f5415aa6955c2cc77e84d063c9ae5a59d47`;
- recorded durable Imported Mesh Objects/list/gizmo/history and self-contained `.forge` persistence as reported implemented;
- recorded sparse Construction truth plus IMPT Imported Mesh truth with unchanged legacy corpus hashes;
- clarified report wording so IMPT requiredness is tied to documents announcing/using Imported Mesh truth, consistent with byte-identical legacy files;
- recorded mixed Construction/Sculpt/Imported Save/Open, Save Copy, autosave and recovery PASS;
- recorded native 2528, JVM 70/70 and FullSharded 455/455 reported PASS;
- accepted exporting durable Imported Mesh bodies as a bounded consistency correction preventing silently incomplete exports;
- marked IMPORT-01A technically COMPLETE / OWNER real-file durable retest pending;
- kept IMPORT-01B blocked until that manual OWNER PASS.

### v4.0.29 — 2026-09-01
- recorded IMPORT-01A as incomplete / RUNNING-PARTIAL rather than forcing a false PASS/FAIL token;
- accepted enabling foundation commit `bdf3f59` on reported regression evidence;
- recorded SceneObject placement hoist, explicit representation handling, ImportedMesh domain truth and parser/project-truth separation;
- recorded that current SCNE is already project-neutral and existing `.forge` golden bytes reportedly remain unchanged;
- explicitly recorded that no user-visible durable import capability exists yet;
- bound the resume run to `bdf3f59` and prohibited restarting the accepted refactor without a concrete regression;
- kept IMPORT-01B, OBJ and FBX blocked;
- kept the original IMPORT-01A acceptance target unchanged.

### v4.0.28 — 2026-09-01
- recorded OWNER manual acceptance that the real low-poly external GLB opens normally under GLB-IMPORT-R1;
- closed GLB-IMPORT-R1 OWNER exact-file retest and classified no-list/no-gizmo/no-Sculpt as preview-by-design rather than an R1 defect;
- added `ARCH-OWNER-10`;
- explicitly authorized bounded durable `IMPORT-01A` before the previous post-MVP timing/readiness position, while retaining the high-density/physical-device gate for broad import scale;
- defined one top-level glTF mesh node → one Imported Mesh object and primitives → internal batches;
- required ObjectId, Objects-list selection, gizmo transforms, Undo/Redo and self-contained `.forge` persistence;
- required imported polygon data to remain non-regenerable project truth without fabricated Construction Source;
- split Imported Mesh Sculpt into separate `IMPORT-01B`, blocked by 01A acceptance;
- kept GATE-E2E OPEN and recorded the owner sequencing override instead of fabricating a PASS;
- kept OBJ/FBX/material/texture/high-density expansion out of 01A.

### v4.0.27 — 2026-09-01
- accepted `PASS-GLB-IMPORT-R1-OWNER-RETEST-READY` at final clean HEAD `504f97d6b087bd0103762a53dd4ac515433e95dd`;
- recorded matrix/TRS, multi-primitive, generated-normal, double-sided and safe ignored external-attribute support as reported implemented under ARCH-OWNER-09;
- recorded deterministic Nomad-like structural fixture SHA-256 `b2dcd36...891f51`, 1,978 vertices / 3,780 triangles / seven primitives, explicitly not the owner binary;
- recorded GLBIR1-01..21, E2E-GLBIR1-01..09, native 2431 and FullSharded 443/443 reported PASS;
- recorded exact-owner E2E-10 as `OWNER_SAMPLE_RUNTIME_TEST_PENDING`, not fabricated PASS;
- accepted the second FullSharded aggregate as justified because it alone matches the final tree after an evidence-camera change;
- marked ARCH-OWNER-09 implemented and GLB-IMPORT-R1 technical work complete, but kept OWNER exact-file acceptance open;
- released no next coding stage before the owner manually imports the real `1 lowpoly.glb`.

### v4.0.26 — 2026-08-31
- recorded OWNER approval of `ARCH-OWNER-09` for bounded external static GLB preview compatibility;
- recorded the exact owner Nomad sample fingerprint: 142,820 B, SHA-256 `59fd1bfe...bd6`, 1,987 vertices, 2,636 triangles, node matrix, seven primitives, no NORMAL, colors/UV, double-sided material and Nomad extras;
- clarified that the current R0 failure on this sample is expected by scope, not evidence that the viewport cannot render the model;
- opened `GLB-IMPORT-R1` as the single READY coding stage;
- bound R1 to matrix/TRS, multiple primitives, generated normals, safe ignored attributes, double-sided culling and real Import GLB SAF UX;
- kept preview session-only/non-persistent/non-editable;
- kept production IMPORT-01 post-MVP and OBJ/FBX outside scope;
- blocked GATE-E2E until OWNER can test the real external file after R1.

### v4.0.25 — 2026-08-31
- accepted `PASS-GLB-IMPORT-R0-ROUNDTRIP-EQUIVALENT` at final clean HEAD `27ec0bcc377d90fb69daa3b7b1332723ca4fe94b`;
- recorded exact Construction and Sculpt roundtrip world-position delta `0.0 m`;
- recorded independent actual-byte GLB parsing and session-only preview ownership as reported implemented under ARCH-OWNER-08;
- recorded GLBIR0-01..20, E2E-GLBIR0-01..08, native 2379, JVM 70/70 and FullSharded 429/429 reported PASS;
- marked GLB-IMPORT-R0 COMPLETE and ARCH-OWNER-08 IMPLEMENTED;
- reopened OWNER GATE-E2E for controlled external-engine verification;
- kept production IMPORT-01 post-MVP and OBJ/FBX unopened.

### v4.0.24 — 2026-08-31
- recorded OWNER approval of `ARCH-OWNER-08`, a bounded early diagnostic GLB-import exception before GATE-E2E;
- preserved broad/durable import as post-MVP behind the existing high-density/performance readiness gate;
- opened `GLB-IMPORT-R0` as the single READY coding stage;
- defined the feature as a session-only Imported Mesh Preview, excluded from ObjectIds, history, `.forge`, manual save, autosave and recovery;
- limited parsing to the static embedded-buffer triangle subset ForgeShape currently exports and required fail-closed handling for unsupported features;
- required an independent parser and source-world vs imported-world geometry comparison so the exporter is not validated only by its own writer structures;
- blocked GATE-E2E on this bounded diagnostic result;
- kept OBJ, FBX, broad arbitrary GLB import and imported-mesh editing out of scope.

### v4.0.23 — 2026-08-31
- accepted E2E-R1C-C1 as reported PASS at final clean HEAD `868995d135b4582693b10cd486abc7418b45c5a9`;
- recorded ARCH-OWNER-07 as reported implemented: R/S baked into positions, inverse-transpose normals, node Translation only, no matrix/rotation/scale, no recenter;
- recorded corrected current sentinel hashes and explicitly retained old pre-bake hashes as history only;
- recorded FSR1C-C1-01..15 and E2ER1C-C1-01..06 PASS, native 2292, JVM 70/70 and FullSharded 406/406;
- accepted the agent's correction of two initially weak test designs because final cases now exercise geometry/normal math that can distinguish the required behavior;
- fixed the prior roadmap's off-vocabulary `OPEN-CORRECTION` status by closing E2E-R1C as `COMPLETE` after the reported correction PASS;
- reopened OWNER GATE-E2E on corrected sentinels only;
- kept OBJ/FBX and later product stages blocked until the external-engine verdict.

### v4.0.22 — 2026-08-31
- recorded OWNER approval of `ARCH-OWNER-07`: static interchange export bakes Rotation + Scale into geometry while preserving Translation/pivot at node level;
- recorded target exported node Rotation identity and Scale `(1,1,1)`;
- preserved metres 1:1, +Y-up, right-handed zero-axis-conversion semantics;
- required inverse-transpose normal handling for non-uniform scale and no silent recenter/mirroring;
- explicitly left `.forge` authored transform semantics unchanged;
- reopened E2E-R1C only for one focused transform-policy correction before GATE-E2E;
- kept OBJ/FBX outside this correction;
- kept the prior GATE-E2E docs-sync prompt on HOLD because no OWNER external-engine PASS exists.

### v4.0.21 — 2026-08-31
- accepted read-only `PASS-GATE-HANDOFF` at unchanged HEAD `cdc184b999bae716809f632c11fe771ebbb98c2b`;
- recorded exact sizes and SHA-256 values for both committed owner sentinel GLBs;
- recorded zero repo/device/toolchain/external-engine changes during the handoff;
- kept `GATE-E2E` OPEN because no Godot import or OWNER verdict has occurred;
- corrected coordinator truth for the Construction sentinel trajectory: X/Z increase while Y is negative, so the sequence descends in Y; older “rising” wording is stale prose only;
- released no product coding stage before the owner Godot verdict;
- prepared one HOLD-only post-verdict docs-sync prompt so no coding action can run before OWNER returns `GATE-E2E PASS`.

### v4.0.20 — 2026-08-31
- accepted E2E-R1C / Stage 023 as reported PASS at final clean HEAD `cdc184b999bae716809f632c11fe771ebbb98c2b`;
- recorded repo-proven metres, +Y-up, right-handed, `T·Rz·Ry·Rx·S`, column-major and CCW coordinate authority with zero GLB axis conversion;
- recorded early deterministic GLB 2.0 export for Construction/Sculpt, multi-body nodes, pivots/transforms, POSITION/NORMAL/indices and SAF write path;
- recorded 60 exporter selftest checks, native 2259 checks, JVM 70/70 and authoritative FullSharded 399/399;
- explicitly preserved the boundary that no Godot/Blender/external-validator result exists yet;
- marked E2E-R1C COMPLETE and opened OWNER `GATE-E2E`;
- carried dead reserved-export styling and generic refusal-message wording as non-blocking debt;
- prohibited any next coding stage before the owner Godot import verdict.

### v4.0.19 — 2026-08-31
- accepted E2E-R1B as reported PASS and marked Stage 022 COMPLETE;
- recorded autosave/recovery, native `.forge` SAF transfer, lifecycle persistence resilience, injected renderer device-loss recovery and bounded offline diagnostics as reported implemented;
- recorded reported totals: FullSharded 382/382, native 2199, JVM 70/70, FSR1B 36/36, E2ER1B 8/8 and R1A+R2 regression 94/94;
- accepted the background FullSharded execution as a tooling-timeout deviation because the agent reported terminal completion and zero remaining Stage-022 processes;
- accepted the within-stage fix of an order-dependent new test assertion before the authoritative rerun;
- explicitly recorded that the final 40-character E2E-R1B HEAD was omitted from the report; only prefix `678af45` is known, so the next preflight must resolve the full hash;
- opened E2E-R1C / Stage 023 as the only READY coding stage;
- bound Stage 023 to metres 1:1 and required proof of current right-handed +Y-up world semantics before allowing a zero-axis-conversion GLB implementation; mismatch => stop, not invention.

### v4.0.18 — 2026-08-30
- recorded the owner's explicit operational override that the two previously visible FullSharded background tasks are already finished and should be ignored;
- withdrew the v4.0.17 cleanup-only prompt without running it and without fabricating new process/test evidence;
- marked E2E-R1A COMPLETE based on its existing reported technical/product PASS plus the owner's operational confirmation;
- opened E2E-R1B / Stage 022 as the single READY coding stage;
- kept E2E-R1C behind E2E-R1B in the binding execution order;
- fixed the MASTER quick index count from 57 to the actual 58 owner decisions and ARCH-OWNER range 01..06;
- released one self-contained E2E-R1B prompt covering autosave/recovery, Scoped Storage `.forge` transfer, bounded errors, lifecycle/device-loss persistence resilience and offline diagnostics, while excluding standard interchange formats.

### v4.0.17 — 2026-08-30
- recorded E2E-R1A as reported product/test PASS at final clean HEAD `a52be2464c7b1ca101026e05be9727b16da33627`;
- recorded `.forge` v1 save/open, process-death reopen, corruption/version non-mutation, stable corpus and portability evidence;
- recorded reported test totals: FullSharded 342/342, native 2163 checks, JVM 59/59, project actions 7/7, R2 regressions 67/67, E2E process-death 6/6;
- accepted a bounded `hasEdits` boolean as persistent Sculpt safety truth while keeping numeric Sculpt/Mesh revision counters non-persistent; created CAD-R0-DATA Decision v3;
- recorded the owner's screenshot showing the original and recovered-AVD FullSharded background Shell jobs still marked Running after the final report;
- held E2E-R1A at OPEN for operational cleanup only and kept E2E-R1B blocked;
- released exactly one cleanup-only prompt with zero repository/test rerun scope.

### v4.0.16 — 2026-08-30
- recorded owner approval of `ARCH-OWNER-06`: `.forge` is the canonical self-contained cross-device ForgeShape project document, while standard interchange formats are separate staged import/export concerns;
- allowed semantic compactness in `.forge` only where compatible ForgeShape can deterministically regenerate the same project semantics, while requiring non-regenerable truth such as current Sculpt editable mesh/topology to travel in the file;
- explicitly prohibited device-local paths, Android types/ids, UI/window geometry, ABI/pointer-width/host-endian assumptions, GPU resources and renderer caches from `.forge` truth;
- approved the long-term staged interoperability direction including GLB/glTF, OBJ, FBX and other explicitly selected common formats without adding FBX/OBJ/import to Stage 021 or the first export milestone;
- superseded `FORGESHAPE_CAD_R0_DATA_DECISION_v1.md` with v2 solely to add this portability/interchange boundary; D1–D9 remain unchanged in meaning;
- updated the E2E-R1A prompt to add an explicit portability proof (`FSR1A-15`) and a hard stop against standard-format import/export scope expansion;
- kept E2E-R1A READY at reported clean baseline `14de5ee58f061b36774f22cccc88187f4c0e85bf`.

### v4.0.15 — 2026-08-30
- accepted the final R2 repository documentation sync as reported `PASS-DOCS-SYNC` at clean final HEAD `14de5ee58f061b36774f22cccc88187f4c0e85bf`; only `PROJECT_STATUS.md` changed and no test/device rerun was required for the docs-only delta;
- completed coordinator-owned `CAD-R0-DATA` and created `FORGESHAPE_CAD_R0_DATA_DECISION_v1.md` without changing product code or answering owner `OQ-A`;
- selected a declarative semantic Construction feature graph, project-neutral `SCNE` identity/placement, separate `CONS` and `SCUL` truth, binary64 Construction numerics, current float32 Sculpt mesh truth and fail-closed feature-lineage topological references;
- fixed `.forge` v1 outer envelope to `FORGESH1`, explicit little-endian fields, versioned CRC-32 sections, deterministic writes, temporary decode/validation then atomic live-state replacement, and no fake historical v0 migration fixtures;
- expanded `GATE-KERNEL` from a single box-cylinder check to robustness/determinism/topology-lineage/Android-16KB/portability/license/performance criteria while keeping the kernel choice owner/evidence-gated; candidate families are OCCT, Parasolid, ACIS, Manifold and CGAL;
- moved `CAD-R0-DATA` to COMPLETE and `E2E-R1A` to READY; prepared exactly one self-contained Stage 021 prompt with no external decision-file transfer;
- repaired the stale MASTER footer that still said v4.0.11 and corrected the carried version-note count before publishing this bump;
- integrity: heading-set diff vs v4.0.14 `+0 / -0`; OWNER headings `57→57`; 57 decision Status rows valid; 57 decision Implementation rows valid; HISTORY version notes `90`; raw present-tense detector hits outside §1 were manually classified as existing policy/examples/command literals, with **0 actual competing stage-status claims**.

### v4.0.14 — 2026-08-30
- recorded owner-review bundle run PASS at final evidence-only HEAD `71bab57e4e12a91c5f1c7b8cec6a5af9b92569b7`;
- independently reviewed the supplied contact sheet / representative raw-overlay pairs and issued coordinator visual PASS;
- closed `UI-LAYOUT-R2` as COMPLETE after correction round 1; retired the unused second correction round;
- activated the UI moratorium and froze Revision 1 edge-host geometry as the accepted baseline;
- unblocked `CAD-R0-DATA` to READY while keeping `E2E-R1A` blocked until the coordinator data decision document exists;
- prepared one docs-only coding-agent handoff to reconcile repository current-truth documentation with the final visual acceptance, with no product/test/build/device work.

### v4.0.13 — 2026-08-30
- recorded UI-LAYOUT-R2 correction round 1 as reported TECHNICAL PASS at final HEAD `40d1f4490ef874344f0b101da2e290053028f675`;
- recorded product+test correction commit `6d05814ddb89ef4e06d2e78d62af78d8b2262af1`;
- recorded 66/66 accepted host frames, zero state drift, persistent-resting Δ=0 dp and zero intrinsic hit-area violations;
- recorded new focused `UILR2C-01..12` 12/12 PASS beside the existing 18/18 suite;
- recorded authoritative FullSharded expansion from 318 to 330 tests, all 330 PASS;
- consumed correction round 1 and preserved exactly one reserve round for a concrete visual defect;
- kept UI-LAYOUT-R2 OPEN solely for owner/coordinator static visual review; CAD-R0-DATA and motion/morph remain blocked until that verdict.

### v4.0.12 — 2026-08-30
- accepted the Revision 1 correction invocation as a correct `PRECONDITION-NOT-MET` / no-change stop;
- recorded unchanged clean repository HEAD `40e8b10a0b30da27ba1e60b22ab6f8667135d452` and zero product/build/device/test work;
- confirmed correction round 1 remains unused;
- removed the erroneous requirement that the owner manually transfer the accepted Revision 1 file to the agent machine;
- retained `UI-SPEC-R0` as an OWNER artifact while authorizing the coordinator to transmit its exact accepted geometry, acceptance source and provenance hash verbatim in a self-contained coding-agent prompt;
- kept the correction target unchanged: eliminate landscape/expanded Exact/Details host translation without widening the already-conforming edge rail.

### v4.0.11 — 2026-08-29 — UI-SPEC-R0 REVISION 1 OWNER-DELEGATED ACCEPTANCE
- recorded the owner's explicit delegation to the coordinator to finalize the static dimensions while preserving the narrow edge-hugging layout direction: `Zdam się na ciebie, spróbuj dopasować samemu wymiary, tylko trzymaj się tego layoutu, żeby był przy krawędziach.`;
- replaced the non-binding v0.2 draft with binding `FORGESHAPE_UI_SPEC_R0_REV1_ACCEPTED.html`, SHA-256 `9d032cadc35497a1fb73b185e8889be6031d00b5b0c95fadbcd2642f2a063151`;
- selected a 72 dp design width rather than copying the measured 68 dp runtime width: this preserves the edge-first viewport direction while intentionally leaving 4 dp design breathing room; because overlay tolerance is ≤4 dp, the current 68 dp resting width already conforms and must not be widened merely to hit 72 exactly;
- bound six cell frames to C10/C13 `(112,8,72)`, L10/L13 `(88,8,72)`, E10/E13 `(108,8,72)` dp for top/right/width; height remains content-driven and IME may reduce available height without changing those three anchors;
- retained measured persistent Objects/history/brush/dock resting geometry by reference to the evidence package at `40e8b10`, including Δ=0 open/close restoration, intrinsic ≥48×48 dp targets and no Construction history in Sculpt;
- made the correction scope narrow: only the independently measured L/E Exact-open / Exact+IME / Sculpt-Details translation from right=8 dp to ~320/~360 dp is a required product correction; the side precision/details surface must fit/scroll to the left of the fixed rail instead;
- changed `UI-SPEC-R0` to `COMPLETE`; kept `UI-LAYOUT-R2` `OPEN`; released correction round 1 of maximum 2, with the previous measurement-only closeout still counting as zero correction rounds;
- preserved the old wide R0 artifact and overlays as historical evidence only; no owner decision, roadmap order, native/domain semantics, motion/morph scope or Radius/Strength P2 debt changed.
- integrity check: heading-set diff vs v4.0.10: +0 / −0 | OWNER-IDs 57→57 | 57 decision statuses valid | 57 actual Implementation values valid | roadmap order unchanged | HISTORY notes 86 | stale v4.0.10 companion filename hits in MASTER PLAN: 0.

### v4.0.10 — 2026-08-29 — R2 MEASURED FAIL / OWNER GEOMETRY REVISION
- accepted the `UI-LAYOUT-R2` measured closeout as evidence-only FAIL at HEAD `40e8b10`;
- recorded exact accepted-spec hash match, six exact configuration cells, 18/18 focused rerun, 36 raw screenshots + 36 overlays, and zero product/test/harness changes;
- recorded 0/66 old absolute host-anchor rows passing the old R0 mock, plus the independent L/E right-inset translation defect during Exact/Details;
- preserved the passing Δ=0 resting-bound restoration and intrinsic ≥48 dp target evidence;
- recorded owner visual feedback that the old 132–152 dp host direction is too wide and the previous edge-hugging runtime direction better preserves viewport workspace;
- reopened `UI-SPEC-R0` to `READY` for an owner revision and forbade correcting product geometry to the now-disfavored old R0 numbers;
- retained UI-OWNER-44 unchanged: one authored right host, fixed frame within a state cell, downward/contextual growth, no Exact/Details translation, no alternate IME grammar;
- no R2 correction round consumed yet; correction round 1 is blocked operationally until the revised owner spec is explicitly accepted.

### v4.0.9 — 2026-08-29 — UI-SPEC-R0 OWNER ACCEPTED / R2 CLOSEOUT UNBLOCKED
- recorded the owner's explicit acceptance of the coordinator-designed `UI-SPEC-R0` with the exact statement `Wygląda ok, zatwierdzam`;
- created the independent binding offline HTML mock `FORGESHAPE_UI_SPEC_R0_ACCEPTED.html`, SHA-256 `9f1e866795a2b2169273a7db62807e544747e673758598779a35fd25553dbe64`; the previously generated PNG is retained only as a non-binding visual-direction companion;
- recorded six binding cells: 411×914, 914×411 and 1280×800 dp at font scales 1.0 and 1.3, with accepted cell-specific fixed host top/right/width, resting rectangles, Exact/Details surfaces, ≥48 dp targets, zone/collision rules, state frames and load/no-project/mirrored-zone reservations;
- clarified §4.2 after owner acceptance: fixed host top/right/width are invariant across states within each responsive cell; different configuration/font-scale cells may carry different accepted fixed values;
- preserved existing product truth that Sculpt has no Undo/Redo history; any concept-render appearance of a Sculpt history capsule is explicitly non-binding and is not promoted into the specification;
- changed `UI-SPEC-R0` from `READY` to `COMPLETE`; kept `UI-LAYOUT-R2` `OPEN`, now unblocked for the same-stage measured evidence closeout and subsequent owner/coordinator visual verdict;
- authorized no product correction in the evidence run itself: a measured failure must report deltas and stop, after which the coordinator may issue one focused correction prompt within the existing two-round limit;
- recorded clean external handoff path `D:\TRAVELAPPS\FORGESHAPE_UI_SPEC_R0_ACCEPTED.html` so the owner artifact can be supplied without dirtying the application repository before preflight;
- integrity check: heading-set diff vs v4.0.8: +0 / −0 | OWNER-IDs 57→57 | decision bodies unchanged | roadmap order unchanged.

### v4.0.8 — 2026-08-29 — FOURTH R2 PRECONDITION STOP / FULL HEAD RECORDED
- accepted the fourth `UI-LAYOUT-R2` evidence-closeout report as correct `PRECONDITION-NOT-MET` handling: all three OWNER INPUT values were still placeholder literals, so no mock was opened and no comparison could start;
- upgraded current reported repo HEAD from the prior short form `f4416e7` to the full hash `f4416e750b53004df3a7e6cccb038e32b9d8dd29`; the report states this is the single docs-only reconciliation commit on top of `4a93050`;
- recorded the read-only preflight evidence that `git diff --name-only 4a93050..HEAD` contains exactly `CLAUDE.md` and `PROJECT_STATUS.md`, with clean working tree and no later commit;
- recorded zero adb, Gradle, build, test, capture, evidence-artifact or repository modification in this invocation; reserved `emulator-5554` was untouched;
- kept `UI-SPEC-R0 = READY (OWNER)` and `UI-LAYOUT-R2 = OPEN`; no product/runtime truth, roadmap order, owner decision or R2 correction-round accounting changed;
- added an operational anti-churn rule to current execution: do not launch another R2 evidence-closeout prompt until the owner has created and explicitly accepted `UI-SPEC-R0` and supplied real path, hash/`NOT_RECORDED`, and acceptance-source values.
- integrity check: heading-set diff vs v4.0.7: +0 / −0 | OWNER-IDs 57→57 | decision bodies unchanged | execution state unchanged.

### v4.0.7 — 2026-08-29 — REPO DOC CURRENT-TRUTH RECONCILIATION PASS
- accepted the bounded docs-only reconciliation report from clean baseline `4a93050` to reported short HEAD `f4416e7`; final tree clean, one local commit, no push, no device/adb/Gradle/build/capture work;
- recorded reported diff scope as exactly `PROJECT_STATUS.md` and `CLAUDE.md` (+54 / −18), with `ARCHITECTURE.md`, `PRODUCT.md` and `README.md` read but unchanged;
- recorded correction of the missing `UI-SPEC-R0` blocker/next-step wording, misleading “accepted implementation baseline” wording, six stale capability/current-truth rows, and pre-R2 selector-row/capsule vocabulary;
- preserved relative/runtime R2 rows as VERIFIED while separating the absolute owner-mock ≤4 dp gate as PENDING;
- closed the coordinator-found documentation-current-truth debt as reported, without altering product/runtime truth, roadmap order, owner decisions or R2 correction-round accounting;
- retained `UI-SPEC-R0 = READY (OWNER)` and `UI-LAYOUT-R2 = OPEN`; final visual acceptance still requires the independent owner artifact;
- recorded verification boundary: coordinator accepted the detailed report but has not independently inspected the post-change repo Markdown files in this conversation; live repo truth overrides the report if later evidence conflicts.
- integrity check: heading-set diff vs v4.0.6: +0 / −0 | OWNER-IDs 57→57 | decision Status/Implementation vocabularies valid | live companion/version pointers reconciled | competing-now status claims outside §1: 0 after manual classification of policy/history-example detector hits.

### v4.0.6 — 2026-08-29
- accepted the third `UI-LAYOUT-R2` evidence-closeout preflight stop as correct: all three FUTURE-RUN OWNER INPUT placeholders were still literals, HEAD stayed `4a93050`, tree was reported clean, and no command/device/build/capture/edit/commit occurred;
- kept `UI-SPEC-R0 = READY (OWNER)` and `UI-LAYOUT-R2 = OPEN`; no correction round consumed and no product truth changed;
- audited the owner-supplied current repo Markdown snapshot against the plan and found bounded documentation drift rather than implementation drift;
- recorded stale `PROJECT_STATUS.md` capability rows (`44 dp`, old appearance/light-theme wording, border-led active state, old docked/flush material), stale top-level blocker/next-stage wording, and stale `CLAUDE.md` selector-row/capsule vocabulary;
- confirmed `ARCHITECTURE.md`, `PRODUCT.md` and `README.md` are materially consistent with the R2 architecture/runtime contract;
- authorized only a bounded docs-current-truth reconciliation for those contradictions; it may not create/guess `UI-SPEC-R0` and does not alter the single product/orchestration next step.

### v4.0.5 — 2026-08-29 — PRECONDITION STOP RECORDED
- accepted the second UI-LAYOUT-R2 evidence-closeout report as correct `PRECONDITION-NOT-MET` handling at unchanged HEAD `4a93050`;
- recorded that the post-acceptance prompt was launched with `ACCEPTED_UI_SPEC_R0_PATH=<path>` still unfilled and therefore performed no measurement, capture, build, device contact, tests, documentation edit or commit;
- made no product, roadmap, status or owner-decision change: `UI-SPEC-R0` remains owner-owned `READY`, `UI-LAYOUT-R2` remains `OPEN`, and no agent work is authorized before the owner artifact exists;
- clarified the process consequence: repeated closeout launches without the owner input are churn, not progress, and must stop;
- preserved the prior reported R2 technical evidence unchanged;
- integrity check: heading-set diff vs v4.0.4: +0 / −0 | OWNER-IDs 57→57 | competing-now hits outside §1: 0.

### v4.0.4 — 2026-08-29 — MISSING UI-SPEC-R0 CORRECTION
- accepted the UI-LAYOUT-R2 evidence-closeout report as a correct hard-stop: `BLOCKED-MISSING-ACCEPTED-MOCK`, baseline = final `4a93050`, clean tree, zero product changes, zero captures, zero commits and no device contact;
- the agent reported a whole-working-tree and whole-Git-history search with no accepted `UI-SPEC-R0` artifact; old Stage 015A-R wireframes and audit reference catalogues were correctly rejected as substitutes rather than silently promoted to a specification;
- corrected the coordinator's v4.0.3 error: `UI-SPEC-R0` had been marked `COMPLETE` without evidence. It is now `READY`, executor `OWNER`; no external accepted artifact has been supplied to this coordinator;
- kept `UI-LAYOUT-R2` `OPEN`: its technical implementation/test result at `4a93050` remains recorded, but final acceptance cannot execute the 4.2 overlay contract until an independent owner-approved mock exists;
- explicitly prohibited retrofitting the specification by copying current runtime bounds. The purpose of `UI-SPEC-R0` is to provide an independent intended geometry against which R2 can fail;
- recorded that the R2 start dependency was bypassed without verified satisfaction. This does not erase the implementation; it prevents a false acceptance and keeps new product stages/motion blocked;
- no owner decision changed and no decision body was edited; OWNER_DECISIONS changes only its version/companion header;
- prepared one inactive same-stage evidence-closeout prompt for use only after the owner supplies the accepted mock; it does not authorize product changes.
- integrity check: heading-set diff vs v4.0.3: +0 / −0 | OWNER-IDs 57→57 | decision Status values all legal | Implementation values all in schema | roadmap executor/status cells all legal | actual status-claim hits outside §1: 0 (heuristic produced 6 non-status/policy-history text hits, manually classified).

### v4.0.3 — 2026-08-29 — UI-LAYOUT-R2 TECHNICAL REPORT / OPEN VISUAL GATE
- reviewed the UI-LAYOUT-R2 coding-agent report from baseline `54c3dfa` to final repo HEAD `4a93050`;
- accepted the reported technical verification as sufficient for a technical PASS: focused 18/18, JVM 59/59, native 2043/2043, both ABI debug/release PASS, DEV2/DEV3 PASS, and authoritative FullSharded 318/318 with zero missing/duplicate/unexpected/execution-missing results;
- did **not** promote the stage to COMPLETE because 4.2/4.4 require owner/coordinator visual acceptance against the accepted UI-SPEC-R0 mock;
- recorded the local screenshot/video package as reported evidence only because the `D:\...` artifact paths are not accessible from the coordinator conversation;
- identified two evidence/report gaps before final verdict: no supplied overlay-diff proving ≤4 dp against the accepted mock, and no explicit font-scale 1.3 matrix proof in the pasted report; the required per-test PASS/evidence table from 2.7 is also absent from the pasted report;
- set `UI-SPEC-R0` to COMPLETE, `UI-LAYOUT-R2` to OPEN, RUNNING 0 / OPEN 1, and kept `CAD-R0-DATA` blocked until R2 acceptance;
- updated UI-OWNER-44 metadata to `PARTIAL(UI-LAYOUT-R2)` without changing its decision body;
- recorded Radius/Strength as separate P2 visual debt, explicitly outside the static right-host closeout;
- generated one same-stage evidence-closeout prompt: no product code, only mock overlays, quantitative bounds proof, font-scale/configuration proof, and a contract-compliant evidence table;
- no motion/morph work and no new product stage is authorized before the visual verdict;
- integrity check: heading-set diff vs v4.0.2: +0 / −0 | OWNER-IDs 57→57 | decision Status values: all in vocabulary | Implementation values: all in schema | roadmap status/executor cells: all in vocabulary | competing-now status-claim hits outside §1: 0 (four heuristic matches were ordinary-language references to a running prompt/stage rule, permitted by 0.1).

### v4.0.2 — 2026-08-29 — MODELS APPLIED TO EVERY ROW
- coordinator re-reviewed v4.0.1 against its own twenty findings: thirteen closed, seven fixed in one place but not in every place, and four new inconsistencies introduced by the fixes themselves; all eleven closed here, each verified against the files before being accepted;
- the pattern, narrower than last time: **v4.0.1 introduced three new models — `executor`, the closed roadmap `Status` vocabulary, and the dependency graph — and applied each to the case that motivated it rather than to every row.** A model applied to eighty percent of its rows is a convention with exceptions, and the exceptions are what a reader stops trusting;
- removed the stale vocabulary form of the single-present-tense rule that survived in the 2.8 pre-prompt checklist, where it contradicted the corrected 0.1;
- widened the status-claim detector to the `on hold` and `awaiting` shapes, and stated plainly that it is a heuristic — a clean run means "no known-bad shape", never "rule satisfied";
- fixed the broken decision-counter command where it had been missed, inside `OWNER_DECISIONS`, and wrote its failure mode next to it so it cannot be reintroduced;
- tightened the `Implementation` schema to stage IDs, `legacy` and at most one trailing `accepted YYYY-MM-DD`; moved the commit hash and verdict out of UI-OWNER-30's cell into its notes; fixed check 5, which had been matching its own schema definition row;
- split every grouped roadmap row into one row per work item so that no cell carries `per item — see 7.5`, and **split the Sculpt-dimensions half of `Stage 020M` into its own ID `SCULPT-DIM-01`, `BLOCKED-BY:OQ-02`** — one work ID may not carry two statuses;
- corrected the legacy map, which still said `Stage 021/022/023` are executed as one stage after the decomposition made that false;
- made 7.5's "only place dependencies are recorded" claim literally true by defining what 7.2 may and may not say;
- updated the live filename pointers that still named v4.0.0 files;
- gave every row of the `executor` column one legal value, after row 21 had shipped with `mixed — see 7.2` in the same version that introduced the closed vocabulary;
- added `GATE` to the status vocabulary and reduced every roadmap status cell to a single legal value, moving qualifications into the `What` and `Why here` columns;
- renamed `E2E-R1a/b/c` to `E2E-R1A/B/C` and stated in 0.4 that a decomposition letter attaches to the whole ID, as in `Stage 018A/B/C`;
- **declared this the last correction round on the plan.** R-4 caps corrections at two rounds per defect and W-7 cancels an audit that produces no corrective stage; both now apply to this document. A further defect goes to 8.5 and is fixed in passing at the next version bump. It does not open a third review round and it does not delay `UI-SPEC-R0` or the remote backup by a day;
- no owner decision changed, no decision body was edited, no version note was touched, and section 1 again required no factual correction;
- integrity check: OWNER-IDs 57→57 (heading count) | decision Status values: 5 distinct, all in vocabulary | Implementation values: all in schema | roadmap status cells: all in vocabulary | executor cells: all in vocabulary | status-claim hits outside §1: 0 excluding quoted history in 0.1 and 10.x.

### v4.0.1 — 2026-08-29 — RULE-LAYER CORRECTION
- handed v4.0.0 to a fresh coordinator with an instruction to report contradictions before doing any work; it returned twenty, every one of them verified mechanically against the files, and every one of them fixed in this version;
- the pattern across all twenty: **v4.0.0 declared rules it did not itself obey, and shipped integrity commands that did not implement the rules they claimed to check** — a rule that cannot be checked is a preference;
- restated the single-present-tense rule as a ban on *status claims* rather than on the words `current`/`running`/`next`, which the plan legitimately uses 75 times; scoped it once, exempting `HISTORY` in full and verbatim decision bodies, and said so inside those files rather than only here;
- rewrote the status-claim detector to implement the restated rule, and added a fifth check over the decision status vocabulary;
- fixed both broken integrity commands: the decision counter counted 143 matching lines instead of 57 headings, and the heading-set diff embedded line numbers so that adding a paragraph reported changes that never happened;
- added the **executor** axis (`OWNER` / `COORDINATOR` / `AGENT`) so that ownership of a work item lives in the data instead of in a warning in the handoff prompt; `UI-SPEC-R0` is an owner artifact and the roadmap now says so;
- removed `HELD` in favour of `BLOCKED-BY:UI-SPEC-R0`, so one stage no longer carries two statuses;
- normalized all 57 decision metadata entries to one legal `Status` and one legal `Implementation` value, moved the nuance into a renamed notes row, and tightened both vocabularies so they can be grepped;
- corrected `OQ-05`, which had been recorded as blocking the single next step: handedness is answered *inside* `UI-SPEC-R0`, not before it;
- corrected the group `PLANNED` status on stages that had blocking open questions, and rewrote W-5 with two explicit outcomes so no former `PROVISIONAL` item can sit in an undefined third state;
- broke the circular dependency between `CAD-R0-DATA` and `GATE-KERNEL`: the research gate now writes the gate's pass criteria and candidate list, and the owner answers `OQ-A` at the gate, on evidence;
- **removed `GATE-KERNEL` as a blocker of `CAD-A1`/`CAD-A2`** — UI-OWNER-04 separates Extrude New Body from Add/Cut, and only Add/Cut needs booleans; v4.0.0 would have parked the project's only differentiator behind its largest technical risk for no reason;
- added the `RESEARCH GATE` work type (R-1a), because `CAD-R0-DATA` was labelled a spike while being required to produce a decision document;
- decomposed `E2E-R1` into `E2E-R1a/b/c`, each inside the R-6 one-day/two-round budget, sharing one `GATE-E2E`; `E2E-R1` is a milestone, not a stage;
- placed `DEVICE-M1` in the binding order at position 5 and stripped ordering prose out of the scope-only catalogue;
- recorded `OQ-E` for the missing `GATE-SCOPE` date (proposed 2026-11-30) and marked the gate inert until it is set;
- gave import its own roadmap row, blocked by `Stage 040` and by the 3.6 readiness gate, with the gate outranking the roadmap position;
- added **7.5 dependency graph** as the single machine-checkable home for `Blocked-by` / `Needs` relations, replacing prose dependencies;
- rewrote W-4 as a rule rather than a restatement of `Stage 020R2`'s status, which lives in 1.3 only;
- no owner decision changed, no decision body was edited, no version note was touched, and section 1 required no factual correction — the migration was sound; its rule layer was not;
- integrity check: OWNER-IDs 57→57 (heading count) | decision Status values: 5 distinct, all in vocabulary | status-claim hits outside §1: 0 excluding the quoted history in 0.1, 10.1, 10.4 and 10.5.

### v4.0.0 — 2026-08-29 — CONTROLLED MIGRATION, NOT COMPACTION
- migrated the plan from one 3612-line file into three: `FORGESHAPE_MASTER_PLAN_v4.0.0.md`, `FORGESHAPE_OWNER_DECISIONS_v4.0.0.md`, `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.0.md`, on explicit owner instruction;
- preserved all 57 `*-OWNER-*` decisions with their bodies verbatim, adding only a metadata header per decision (status, decision date where recorded, scope, realizing stage, implementation state, supersession relations, blocking open questions);
- preserved all 74 earlier version notes verbatim and explained the `v3.9.0` and `v3.57.0`–`v3.59.0` gaps for the first time;
- backfilled four missing report-archive entries — `UI-LAYOUT-R1`, `UI-ARCH-AUDIT1`, `UI-ARCH-R1`, `TEST-HARNESS-R1` — which previously existed only in the snapshot and in version notes;
- reorganized the plan into ten hierarchical sections, maximum three levels, eliminating the letter-suffixed sections `§0.1A`, `§19A`, `§21A`, `§22A`, `§24A`;
- introduced the single-present-tense rule: only section 1 may say `current`, `running` or `next`, and added a mechanical detector for violations;
- introduced a closed status vocabulary, replacing eleven prose status variants;
- separated three identifier layers — section number, immutable work/decision ID, explicit relation fields — and prohibited renumbering existing work IDs, which live in prompt filenames, artifact paths, test IDs and commits;
- corrected twenty verified factual defects listed in MASTER PLAN 10.4, including four competing "current stage" statements, a "latest" report section nine versions stale, a completed stage described as a future blocker, and a stage list that silently lost `Stage 020M`;
- deduplicated five policies that had between two and five owners each (selective reading, naming, UI quality, visual acceptance, prompt authoring) into one canonical home apiece with pointers elsewhere;
- collapsed four parallel roadmap copies into one binding order in MASTER PLAN 7.1, with 7.2 describing scope only, without status or order;
- **adopted the owner-approved reordering:** `UI-SPEC-R0` → `UI-LAYOUT-R2` → UI moratorium → `CAD-R0-DATA` → `E2E-R1` (`.forge` + autosave + early GLB) → `GATE-E2E` → `GATE-USER-01` → `SCULPT-H1` → `Stage 018A` → performance/spatial work → `BRIDGE-R1` → CAD → transform-placement extensions → booleans/UV/export → cloud. The previous protected roadmap is preserved verbatim in 7.4;
- no owner decision was cancelled by that reordering; several are executed later, and `UI-OWNER-31`'s word "next" is recorded as historical wording rather than an ordering instruction;
- added a coordinator identity and behaviour section, including the **idea-intake protocol**: an owner idea is never implemented on the turn it arrives; it is played back, classified, given a destination, and written in only after explicit confirmation;
- added `UI-SPEC-R0` and a measurable definition of done for layout (declared rectangles per state, `Δ resting bounds = 0 dp`, identical rail anchors across states and configurations, ≤4 dp overlay deviation, ≥48 dp in every state), plus a two-round limit and a mock load variant for the future screen tenants;
- backfilled twelve roadmap nodes that were required by the MVP scope or by an owner decision but had no owning stage: `GRID-SNAP-01`, `MIRROR-01`, `SEL-OUT-R1`, `GATE-E2E`, `CAD-R0-DATA`, `GATE-PERF-SCULPT`, `STYLUS-G1`, `PERF-BASELINE`, `SPATIAL-R1`, `DEVICE-M1`, and the previously orphaned `Stage 022` and `Stage 023`;
- added six existential gates with genuine FAIL consequences, plus the first stop criteria the plan has ever had;
- added a risk register, an open-questions register replacing the `PROVISIONAL` stage status, a list of untested load-bearing assumptions, and an idea inbox;
- added WIP and closure policy (W-1…W-7) and work-type/budget policy (R-1…R-9), including proportional test gates;
- closed `Stage 020R2` as `ACCEPTED-AS-IS` at `a90e7df` under W-4, removing double bookkeeping of one defect;
- left `OQ-A` (third-party geometry/UV dependencies), `OQ-B` (v1 scope cut) and `OQ-C` (product thesis) explicitly unanswered rather than deciding them on the owner's behalf; answered `OQ-D` provisionally by folding early GLB export into `E2E-R1`;
- integrity check: heading-set diff not comparable (deliberate whole-document reorganization, enumerated in MASTER PLAN 10.4) | OWNER-IDs 57→57 | competing-now hits outside §1: 0 (excluding the quoted historical wording in 0.1, 10.1 and 10.4).

### v3.69.0 — 2026-08-28
- re-audited the active UI-LAYOUT-R2 prompt against MASTER PLAN Governance and documentation ownership rules before execution;
- found a real omission: the prompt mentioned a possible test/docs commit but did not explicitly require selective repo-doc preflight or the stage's current-truth `.md` updates;
- corrected the prompt to require `CLAUDE.md`, current `PROJECT_STATUS.md` summary and heading-routed reads of relevant `ARCHITECTURE.md`/`PRODUCT.md` sections;
- made `PROJECT_STATUS.md` and the relevant `ARCHITECTURE.md` invariant update mandatory on successful R2 implementation;
- constrained `PRODUCT.md` updates to current user-facing truth and prohibited claiming owner visual acceptance before the owner review;
- kept `README.md`/`CLAUDE.md` unchanged unless their owned truth changes;
- added DOC-OWNER-01 code-comment/naming requirements and an explicit documentation-delta report section;
- made no change to R2 product behavior, architecture direction or protected roadmap.

### v3.68.0 — 2026-08-28
- accepted TEST-HARNESS-R1 as PASS at final HEAD `54c3dfa09c7b0fd9351f6c8967b199c3380e3c1a`;
- accepted the new official `-FullSharded` mode as authoritative full-suite verification because discovery, partitioning, missing/duplicate checks, abort/failure semantics and aggregate PASS are runner-owned;
- recorded current authoritative instrumented result as 20 classes / 300 tests / 0 missing / 0 duplicates / `FULL_SHARDED_SUITE_PASS`;
- retained JVM 59/59 as the correct current inventory;
- closed UI-ARCH-R1 verification/process debt and promoted it to COMPLETE / R2-READY;
- activated UI-LAYOUT-R2 on the `WorkspaceTrailingHostView` boundary;
- froze the binding R2 behavior from the post-ARCH-R1 owner video: one right surface, fixed top/right/width, downward-only expansion, bottom hide/restore, no IME alternate grammar;
- kept motion/morph work deferred until owner visual acceptance;
- preserved all prior roadmap and owner decisions.

### v3.67.0 — 2026-08-28
- accepted the JVM `60 → 59` delta as intentional removal of the dead `railDocked`-only test with meaningful device coverage retained;
- accepted 59/59 as the correct current JVM inventory;
- confirmed the five successful shards form an exhaustive non-overlapping union of all 20 classes / 300 tests;
- recorded that current repo wording still treats filtered shard runs as subsets and that a fresh unfiltered rerun again aborted due Android system/service crash;
- rejected an indefinite loop of identical monolithic reruns;
- added TEST-OWNER-01 authorizing an official runner-owned exhaustive-sharded full-suite mode with automatic discovery, partition and aggregate validation;
- promoted TEST-HARNESS-R1 as the only next stage before UI-LAYOUT-R2;
- preserved the post-ARCH-R1 owner-video R2 contract and all roadmap/owner decisions.

### v3.66.0 — 2026-08-28
- reviewed the owner-supplied post-UI-ARCH-R1 interaction video `1000070775.mp4` (~119 s, 1080×2340);
- confirmed that UI-ARCH-R1 preserved visual behavior as intended and did not accidentally redesign the workspace;
- converted the recording into POSTARCH-V01..V07 runtime findings;
- confirmed the three critical R2 invariants: one fixed right contextual surface, bottom-sheet hide/restore instead of upward relocation, and no IME-driven alternate right-control grammar;
- confirmed the same bottom-zone defect in both Exact Transform and Sculpt Details;
- kept startup Project chooser redesign out of R2 scope;
- retained UI-ARCH-R1 closeout as the next execution step before any layout implementation;
- preserved the complete roadmap and all earlier owner decisions.

### v3.65.0 — 2026-08-28
- reviewed UI-ARCH-R1 report at final HEAD `1e3bcd23a5416a9442a00673bd25875e0819dc7d`;
- accepted the structural extraction itself as successful: `WorkspaceTrailingHostView` now owns right-cluster composition/presentation, root LOC/constructor/field/accessor counts materially decreased, and reported before/after semantic geometry has zero differences;
- retained native/domain authority and primary-surface ownership boundaries as healthy;
- did not immediately activate UI-LAYOUT-R2 because JVM test count changed from prior 60/60 to 59/59 without explanation;
- added a closeout check for whether the official five-shard 300/300 instrumented run satisfies current complete-suite crash policy after two rejected monolithic attempts;
- kept product/UI changes blocked during closeout;
- preserved the full roadmap and all prior owner decisions.

### v3.64.0 — 2026-08-28
- accepted UI-ARCH-AUDIT1 as PASS with verdict `ARCH-LOCAL-DEBT`;
- confirmed that native/domain authority, UI state, breakpoint math, primary-surface coordination and leaf components remain viable;
- recorded `EditorWorkspaceView` (~2952 LOC, ~467-line constructor) as the local chrome god-object and primary maintainability risk;
- accepted one `WorkspaceTrailingHostView` ownership boundary before UI-LAYOUT-R2;
- recorded stale/dead `railDocked` semantics and distributed IME/right-cluster presentation policy as bounded debt;
- recorded test coupling to `getChildAt`, exact hierarchy and root accessors as a contributor to false confidence;
- authorized UI-ARCH-R1 as a no-redesign structural refactor;
- preserved all earlier roadmap and owner decisions.

### v3.63.0 — 2026-08-28
- owner deferred UI-LAYOUT-R2 implementation and requested a code-architecture audit first;
- recorded concern that repeated visual corrections may be accumulating on top of an unsuitable UI ownership/layout structure;
- added ARCH-OWNER-05 requiring a simple hierarchical Editor Workspace source structure with explicit composition, surface, adaptive-layout and IME ownership;
- converted UI-LAYOUT-R2 from READY to DEFERRED / NOT ACTIVE while preserving its approved product direction;
- promoted UI-ARCH-AUDIT1 as a read-only source audit at current reported HEAD `717cf70`;
- required the audit to inspect hierarchy, responsibility, state flow, layout mutation, comments/naming, test coupling and refactor risk before further UI implementation;
- defined three possible outcomes: resume R2, bounded structural refactor first, or owner review of a deeper shell correction;
- preserved all prior roadmap/owner decisions and kept motion/morph deferred.

### v3.62.0 — 2026-08-28
- reviewed UI-LAYOUT-R1 report, 14 screenshots, final walkthrough and validation evidence;
- accepted R1 as TECHNICAL PASS at reported HEAD `717cf70` with native 2043/2043, JVM 60/60, focused 20/20 and final full instrumented 288/288;
- rejected final visual acceptance because owner-visible evidence still shows unrelated bottom controls relocating when a sheet opens and an IME-specific alternate right-control layout;
- identified that the existing `UILR1-15` stability test encoded a weaker contract than the owner intended;
- added UI-OWNER-44: one fixed-width right contextual rail surface that expands only downward to reveal additional actions;
- required bottom controls to hide/suspend instead of translating upward around primary bottom sheets;
- prohibited IME-driven horizontal/detached right-control layouts;
- retained R1 improvements to exclusivity, Exact Transform, 48 dp resilience and gizmo legibility;
- promoted UI-LAYOUT-R2 as the only authorized next correction stage;
- kept motion/morph work deferred until static layout acceptance;
- preserved the full pre-existing roadmap and owner-decision set.

### v3.61.0 — 2026-08-27
- reviewed the owner-supplied ~194.7 s portrait interaction recording `1000070747.mp4`;
- confirmed that final UI acceptance remains HOLD despite prior audit evidence;
- added UI-OWNER-43: static layout/anchors/layering must be accepted before new morph/transition polish;
- promoted UI-LAYOUT-R1 Static Workspace Stabilization as the next correction stage;
- recorded video-grounded issues VUI-01..VUI-14 covering surface coexistence, IME collapse, fragmented/right chrome, Exact Transform density, bottom-zone competition, Add/Objects/display composition, Sculpt spatial parity and baseline gizmo legibility;
- explicitly deferred Remotion/morph/transition work until static layout acceptance;
- kept all previously restored roadmap/owner decisions intact.

### v3.60.0 — 2026-08-27 — RESTORED FULL CANONICAL PLAN
- restored the MASTER PLAN from the intact uploaded `v3.56.0` body after detecting that uploaded/generated `v3.57.0` had already lost the large Governance/roadmap/owner-decision middle section;
- preserved all existing owner decisions, exact three-dark-palette values, future five-palette target, directional/relative/dimension Scale plans, snapping/alignment plans, Settings/Project Hub, hierarchy, Sculpt history, portable `.forge`, cross-platform/cloud direction, Shapr3D CAD research gate, CAD roadmap, Sculpt/topology, UV/export/import and historical version notes;
- recorded UI-AUDIT2B Emil as PASS evidence (`87d2612`);
- recorded UI-AUDIT2C Remotion as PASS evidence/prototyping lens;
- recorded UI-AUDIT2D Mobbin plugin-use hard gate as proven at evidence-only `4434356`, but did not accept substantive Mobbin conclusions because the coordinator did not receive them;
- cancelled the previously generated UI-AUDIT2D report-continuation action because the owner explicitly does not want to run it now;
- placed the project on OWNER VIDEO REVIEW HOLD because the owner reports the current app still has visible UI/interaction problems and will provide a usage recording;
- added §0.5 forbidding autonomous MASTER PLAN compaction and requiring heading/owner-decision preservation checks before every future version;
- no implementation prompt is active until the owner video is reviewed.

### v3.56.0 — 2026-08-27
- accepted UI-AUDIT2A as PASS with evidence-only HEAD `6007f69`;
- recorded that the Impeccable hard proof gate was satisfied with path/stat/SHA-256/sections/rules→finding mapping;
- retained the measured P0 trailing-rail collapse and P1 Back/gizmo/contrast/numeric/fold/unit findings as provisional;
- rejected two earlier UI-AUDIT1 false leads: scrim touch leakage and display-popover auto-close;
- preserved positive evidence for anchored popovers, exact numeric round-trip and expanded-layout top anchoring;
- kept all product fixes blocked until Emil, Remotion and Mobbin independent audits complete;
- marked UI-AUDIT2B as the active run.

### v3.55.0 — 2026-08-26
- owner confirmed Mobbin is also available and requested it as an additional independent audit source;
- added `UI-AUDIT2D — Mobbin Comparative Pattern Audit` after Impeccable, Emil and Remotion and before coordinator synthesis;
- defined Mobbin as a comparative pattern/reference source rather than a normative design authority;
- required plugin-use proof through invocation/query logs and returned-reference metadata instead of pretending Mobbin must be a local SKILL.md;
- prohibited pixel-copying/reference-asset reuse and required `adopt / adapt / reject` reasoning;
- moved final cross-audit synthesis until all four independent reports are available.

### v3.54.0 — 2026-08-26
- owner rejected the combined UI-AUDIT1B approach because it could not sufficiently prove that each global Claude skill was actually applied;
- replaced one combined continuation with three independent evidence-backed audits: Impeccable, Emil Kowalski and Remotion;
- added a hard proof-of-skill artifact requirement using absolute path, stat, SHA-256, headings consulted, rules paraphrased and rule→finding mapping;
- defined Remotion correctly as a motion-prototyping/review lens rather than pretending it is an independent generic UI-audit framework;
- prohibited product fixes between the three audits so all lenses inspect the same product state;
- moved comparative synthesis and the next correction prompt until after all three reports are available.

### v3.53.0 — 2026-08-26
- reviewed UI-AUDIT1 and downgraded its self-reported COMPLETE to coordinator `PARTIAL PASS / INSUFFICIENT DEPTH`;
- accepted the measured/source-owned P1 defects as provisional findings but blocked implementation because the comparative visual system was under-described;
- identified missing three-lens comparison, screenshot indexing, per-layout composition analysis, per-mode gizmo analysis, control-family review and concrete motion/Remotion recommendations;
- added `UI-AUDIT1B` as a read-only continuation that reuses the existing 23 captures and gathers only missing evidence;
- prevented premature implementation of the audit's proposed single-constant gizmo size change until the visual target and hit/spacing hierarchy are specified;
- preserved Stage 020R3 starburst as `SCULPT_QUALITY` unless deeper evidence disproves it;
- kept the next product-correction stage gated on owner/coordinator review of the deeper audit.

### v3.52.0 — 2026-08-26
- accepted Stage 020R3 as PASS at reported final HEAD `bd3f7d5`;
- recorded the corrected world/display brush metric, Clay/Inflate normal-space correction and preserved Scale/history/retention ownership;
- accepted the clean full 268-test instrumented rerun after emulator restart as the final runtime verification;
- recorded one process-hygiene note: drain leftover polling waiters before future COMPLETE reports;
- separated a visible scaled-mesh starburst/faceting observation from the proven brush-radius metric so it can be classified without reopening Stage 020R3 or silently adding remeshing scope;
- promoted `UI-AUDIT1` as the only authorized next stage;
- bound UI-AUDIT1 to a read-only three-tool comparison using Impeccable, Emil Kowalski design/motion and official Remotion skills where installed, with no React/Remotion runtime dependency;
- required fresh portrait/landscape/expanded evidence, stable-anchor/jump reproduction and source ownership for each accepted finding;
- kept implementation fixes for one later bounded correction stage after owner/coordinator audit review.

### v3.51.0 — 2026-08-26
- accepted DOC-R2 as PASS from reported baseline `a90e7df` to final `76cd4d1`;
- recorded reconciliation of PRODUCT/PROJECT_STATUS/README/CLAUDE/ARCHITECTURE and deletion of the obsolete Stage 015A-R UX pack;
- preserved three non-blocking documentation debts instead of expanding DOC-R2 after completion;
- promoted Stage 020R3 as the only authorized next coding stage;
- fixed the Stage 020R3 owner contract: a nominally round screen-space Sculpt brush remains round under non-uniform Body Scale; local Sculpt mesh + Body transform ownership stays unchanged and Scale is not baked/reset;
- explicitly separated Stage 020R3 from the later UI/Impeccable audit, Directional Scale, Body Dimensions, snapping and Sculpt history.

### v3.50.0 — 2026-08-26
- owner approved Q1=A: a 120 px round Sculpt brush remains visually round under non-uniform Body Scale; Scale affects object dimensions, not brush-shape semantics;
- owner accepted the coordinator's remaining recommendation, resolving Q2=A: remove the obsolete Stage 015A-R UX decision pack from the live repo during DOC-R2;
- formalized the relationship between Absolute Scale, physical overall dimensions and a new Relative Scale multiplier that resets to `(1,1,1)` on each interaction and multiplies the stored Absolute Scale;
- explicitly rejected the label “World Scale” for that multiplier because World/Local is already coordinate-space terminology and true non-uniform world-axis scaling of a rotated body implies shear outside the current diagonal Scale model;
- approved a separate future Body Dimensions mode with local-oriented X/Y/Z dimension leaders, exact editable values and per-axis `− side / center / + side` anchoring;
- linked Body Dimensions and One-sided Scale to one shared resize-anchor solver;
- approved overall Body Dimensions in Sculpt as bounds-based transform editing, distinct from future CAD feature dimensions;
- expanded future portability target from Android + possible Apple to one ForgeShape product across Android, iPhone/iPad, macOS and Windows PC with a shared platform-neutral C++ core and portable `.forge` format;
- approved local-first cross-device cloud project sync after local persistence, with Google Sign-In desired and Apple-equivalent login requirements rechecked at implementation time;
- added renderer portability, `PORT-R0`, `CLOUD-R0` and `CLOUD-H1` research/implementation gates;
- clarified that “same app” means same product/core/project/account semantics and coherent responsive UI, not necessarily one UI binary/toolkit;
- DOC-R2 is now the only authorized next coding task.

### v3.49.0 — 2026-08-26
- clarified the pending non-uniform Scale → Sculpt issue for owner review without selecting an answer on the owner's behalf;
- performed a reconnaissance pass over current official Shapr3D modeling-space, sketching, constraints, construction-plane, adaptive-UI, Items/History and gizmo documentation/screenshots;
- recorded Shapr3D as a **reference/inspiration source**, never a pixel-copy target and never automatic scope;
- added mandatory `CAD-R0` immediately before real Sketch implementation: repeat fresh internet research at that time, compare against the then-current ForgeShape UI, classify candidate ideas as adopt/adapt/defer/reject, and run an owner questionnaire before writing the `CAD-A1` implementation prompt;
- questionnaire topics now explicitly cover sketch-plane entry, sketch entities, constraints, dimensions, snapping/guides, adaptive contextual actions, construction geometry, feature order, Items/History UI, stylus flow and MVP complexity;
- today's reconnaissance remains non-binding so Shapr3D changes and ForgeShape changes cannot silently become a stale CAD specification.

### v3.48.0 — 2026-08-26
- completed the requested whole-document audit of current `ARCHITECTURE.md`, `CLAUDE.md`, `PROJECT_STATUS.md`, `README.md`, `PRODUCT.md` and `UX_ARCHITECTURE_DECISION_PACK.md` against Stage 020R2 and owner decisions;
- found concrete current-truth contradictions in PRODUCT, PROJECT_STATUS, README and CLAUDE plus historical/stale rationale in ARCHITECTURE;
- separated current three-dark-palette runtime truth from the future five-palette target;
- made MASTER PLAN the sole owner of roadmap/owner decisions and coordinator prompt-authoring policy, while preserving `CLAUDE.md` as the coding agent's repo-local durable safety/rule owner;
- recommended removing the obsolete UX decision pack from the live repo and using Git history as archive;
- placed DOC-R2 documentation reconciliation/compaction before further feature work;
- retained Stage 020R2 visual HOLD and the non-uniform Scale→Sculpt brush anisotropy as separate closeout work;
- asked only two owner decisions before the next prompt: brush metric semantics and delete vs archive the obsolete UX pack.

### v3.47.0 — 2026-08-26
- reviewed the Stage 020R2 report at reported final HEAD `a90e7df` and the supplied `S01..S10` screenshots;
- provisionally accepted the technical core and reported test totals while withholding final Stage 020R2 visual/product acceptance;
- recorded visible gizmo issues: wireframe/debug feel, thin strokes, weak active-handle emphasis, Scale-mode clutter and fragmented trailing Transform chrome;
- recorded a concrete Exact Transform presentation defect: long Scale values visibly clip in numeric fields;
- promoted the report's non-uniform Scale → Sculpt brush anisotropy from passive debt to a required product/architecture adjudication before final transform closeout;
- recorded that three harness crashes were recovered by running unreached suites individually, which is useful evidence but deviates from the previously preferred full-suite rerun protocol;
- checked the coordinator-side ChatGPT plugin registry for `Impeccable`; no installable plugin was returned, so actual Claude/global-skill availability remains to be checked in the coding environment rather than guessed;
- enforced the owner's requested pause: no next implementation prompt until the current repository `.md` documentation pack is uploaded and audited;
- next audit will reconcile repo docs with `a90e7df`, owner decisions, screenshot findings, prompt-policy ownership and stale/duplicated prose before selecting the next coding stage.

### v3.46.0 — 2026-08-26
- marked Stage 020R2 as RUNNING under `AGENT_STAGE_020R2_FULL_TRANSFORM_GIZMO_COMPACT.md`;
- recorded an explicit pause after the R2 report: no next coding prompt until the owner uploads the current repository `.md` documentation set and the coordinator audits contradictions, staleness, duplication and unnecessary prose;
- moved the compact prompt-authoring policy into §0.1A of the MASTER PLAN and deprecated the standalone `FORGESHAPE_PROMPT_AUTHORING_RULES.md` as a second source of truth;
- recorded UI-OWNER-32 gizmo size/thickness/handle-style preferences under future Settings;
- recorded UI-OWNER-33 Symmetric vs One-sided Scale, with six directional handles and open semantics requiring owner questions;
- expanded the old Move-only Surface Snap direction into UI-OWNER-34 Contact/Body Magnet Snap for Move+Scale;
- added UI-OWNER-35 distant Alignment Guides/Magnetic Alignment with Rotate semantics explicitly owner-gated;
- added UI-OWNER-36 stable default anchors + future user-customizable layout;
- added UI-OWNER-37 Settings Hub ownership and kept persistent preferences distinct from transient viewport overlays;
- added UI-OWNER-38 top-left ForgeShape App/Project menu, preserving Export as a global action and prohibiting fake unavailable file actions;
- extended UI-OWNER-08 with UI-OWNER-39 future no-project Home/New/Open flow;
- expanded Stage 018 through UI-OWNER-40 into row commands, multi-select/group transform and parent/child hierarchy substages;
- added UI-OWNER-41 separate Sculpt Undo/Redo history with initial five-action capacity, history navigator, branch invalidation and stylus-hover preview direction;
- superseded UI-OWNER-22 appearance count with UI-OWNER-42: retain the exact three dark palettes and add future Warm Light + Cool Light palettes;
- added AUDIT-OWNER-01 requiring a post-R2 Impeccable/screenshot/source audit of UI coherence and chrome jumping before another shell/preferences stage;
- established the provisional post-R2 sequence: Directional Scale → Contact Snap → Alignment Guides → Settings/UI stability + appearance preferences → Stage 018A/B/C → Sculpt history → persistence/project hub → CAD roadmap, subject to report/docs audit;
- explicitly protected the long-term CAD track from being indefinitely displaced by polish/preferences work.

### v3.45.0 — 2026-08-26
- performed a coordinator “council” review of the unlaunched Stage 020R2 prompt from four lenses: context cost, coding-agent clarity, QA coverage and architecture precision;
- found the original 27,161-character / 3,839-word / 1,222-line prompt materially over-repeated the same contracts across implementation, tests, runtime walkthrough, acceptance and report sections;
- superseded the long Stage 020R2 prompt before launch with `AGENT_STAGE_020R2_FULL_TRANSFORM_GIZMO_COMPACT.md`;
- preserved all binding product/domain contracts: full Move/Rotate/Scale gizmo, planar handles, World/Local, true world rotation, positive Local Scale, Exact Transform Scale, Stage 019 history, pointer safety, scale-aware render/picking and Surface Snap deferral;
- reduced Cartesian test enumeration by using parameterized risk-based test IDs;
- reduced screenshot evidence to ten review-critical views;
- adopted `FORGESHAPE_PROMPT_AUTHORING_RULES.md` as coordinator prompt-writing policy;
- future normal coding prompts target roughly 1,800–2,500 words and avoid duplicating stable repo-wide rules already owned by `CLAUDE.md`/architecture/status.

### v3.44.0 — 2026-08-26
- owner explicitly expanded the transform-gizmo scope before the Stage 020R1 prompt was launched;
- marked `AGENT_STAGE_020R1_WORLD_SPACE_ROTATION_CORRECTION.md` SUPERSEDED and not to be run;
- created Stage 020R2 as one replacement implementation stage based on reported HEAD `f58c7b8`;
- retained the mandatory true world-space rotation correction inside R2;
- expanded the native transform domain from Position+Rotation to Position+Rotation+Scale, with unitless positive local Scale and `T·Rz·Ry·Rx·S` transform order;
- required Stage 019 history/transactions and Exact Transform precision fields to include Scale;
- added UE5-inspired axis/plane transform vocabulary: Move X/Y/Z + XY/XZ/YZ planes, Rotate X/Y/Z, Scale X/Y/Z + planar Scale + uniform Scale;
- approved World/Local(Object) switching for Move/Rotate and Local-only Scale;
- approved X red / Y green / Z blue familiar axis identity with touch-adapted >=48 dp effective hit regions;
- added UI-OWNER-30 for the full gizmo contract;
- added UI-OWNER-31 scheduling Surface Snap directly after Stage 020R2;
- created Stage 020S roadmap item for surface/bounds-aware placement, separate from Grid Snap;
- moved Stage 018 after the full transform + Surface Snap foundation.

### v3.43.0 — 2026-08-26
- reviewed the Stage 020 final report at reported HEAD `f58c7b8`;
- accepted the Stage 019 production startup-history fix, gizmo ownership, Move solver, pointer arbitration, transaction integration, short-layout correction and reported test totals;
- rejected Stage 020 COMPLETE status because the report itself states X/Y rotate rings are not true world-space rotations for already-rotated bodies;
- identified this as a direct violation of the binding world-space-only MVP contract, not an optional documented compromise;
- defined the correct rotation operation as world-axis pre-multiplication `R' = R_worldAxis(Δ) · R_start` under the existing authoritative `Rz·Ry·Rx` representation;
- required branch-continuous Euler decomposition back to the existing transform state, including mixed rotations, >360 continuity and singularity-adjacent stability;
- corrected the test philosophy: world X/Y rotation on a mixed orientation may change multiple Euler components; tests must compare resulting orientation, not assume one-component mutation;
- created Stage 020R1 as one narrow technical correction;
- deferred final Stage 020 screenshot acceptance and Stage 018 until Stage 020R1 passes;
- kept all other accepted Stage 020 systems out of scope for redesign.

### v3.42.0 — 2026-08-26
- accepted Stage 019 core implementation at final HEAD `d8536ab`;
- recorded native Construction history capacity 64, domain-state snapshot model, atomic Exact Shape/Transform/Add operations, stable ObjectId creation restore, multi-object replay and Stage 020-ready begin/update/commit/cancel coalescing;
- accepted reported verification: native 12/1809/0, JVM 60/60, history UI 20/20 in all three layouts, compact/expanded 221/222, short 86/86, DEV2/DEV3 PASS and unchanged `ui11` precondition failure;
- accepted the agent's invalidation and full rerun of the expanded suite after emulator system-server/package-service restart;
- identified one unresolved report-level inconsistency: direct-Sculpt startup reportedly creates a Construction history step, potentially violating the empty-new-session invariant;
- made resolution/proof of that startup path a mandatory Stage 020 preflight gate before gizmo implementation;
- promoted Stage 020 to CURRENT/READY at baseline `d8536ab`;
- fixed first-gizmo scope to Construction-only world-space X/Y/Z Move axes + world-space Rotate rings, pivot at authoritative Construction origin, no Scale/local-world/custom-pivot/free-plane handles;
- kept existing Transform Tool Rail ownership and specified a compact anchored Move/Rotate contextual selector; Exact Transform stays the precision fallback;
- required one gizmo drag to use Stage 019 transaction coalescing as exactly one history entry;
- explicitly deferred user-facing Grid Snap until an increment/unit contract is approved while requiring a clean future quantization hook;
- rejected documentation compaction as a line-count-only gate.

### v3.41.0 — 2026-08-26
- accepted UI-R4C report as COMPLETE at final HEAD `b42076b`;
- accepted the focused UIR4C screenshot set as FINAL VISUAL PASS;
- closed UI-R4 after verifying all four prior screenshot blockers are visibly resolved: full Back navigation, collision-free Add Primitive, single-control Start/Resume Sculpt geometry, and removal of permanent Construction instruction chrome;
- accepted the transient `Now sculpting` message as legitimate operation feedback rather than ambient chrome;
- recorded full UI-R4C regression evidence, including the baseline-reproduced `ui11` soft-keyboard precondition failure;
- promoted Stage 019 to CURRENT/READY;
- defined Stage 019 as a native-owned Construction transaction/history foundation covering current exact primitive edits, exact Position/Rotation edits and atomic Add Primitive creation;
- required a begin/update/commit/cancel transaction boundary suitable for Stage 020 gizmo drag coalescing;
- required stable object identity, multi-object isolation, redo invalidation, no-op/rejection filtering and in-session lifecycle retention;
- explicitly separated Construction Undo/Redo from future Sculpt Undo and from `.forge` history persistence;
- kept Stage 020 blocked until Stage 019 acceptance.

### v3.40.0 — 2026-08-26
- completed owner/coordinator screenshot review of UIR4B-S01..S12;
- retained UI-R4B as TECHNICAL COMPLETE at `19d9213` but rejected final UI-R4 visual acceptance;
- explicitly accepted the major UI-R4B improvements: viewport dominance, Sculpt no-Add truthfulness, Radius/Strength grouping, active Tool Rail state, precision row clipping fix, SI-unit presentation, expanded/tablet composition, fill-led selection, three palettes and `Start Sculpting`;
- recorded four remaining screenshot-grounded blockers only: truncated `Back to Construction`, Add Primitive overlap with the trailing contextual control, nested/overweight Start/Resume Sculpt CTA, and permanent redundant Construction instructional chrome;
- created `UI-R4C Final Visual Composition Cleanup` as one final bounded UI-only correction before Stage 019;
- prohibited reopening accepted UI-R4B motion/adaptive/domain work or introducing any future feature;
- Stage 019 remains blocked until UI-R4C technical verification + focused screenshot review PASS.

### v3.39.0 — 2026-08-26
- accepted UI-R4B implementation report as TECHNICAL COMPLETE at final HEAD `19d9213`;
- recorded two focused commits `bb52b59` and `19d9213`, with final tree reported clean;
- closed the prior UI-R4A AC20 evidence-housekeeping failure because historical UI-R4A evidence and new UI-R4B screenshot evidence were committed according to repo policy;
- recorded successful fixes for Sculpt unavailable Add flow, Tool Rail state restoration, status lifecycle, precision row clipping, anchored motion, expanded/tablet composition, fill-led selection/radius nesting, Radius/Strength hierarchy, SI unit casing, 48 dp hit targets and `Start Sculpting` terminology;
- recorded full reported verification: native 11/1695/0, JVM 60/60, compact+expanded instrumented 190/190, DEV2/DEV3 PASS, zero geometry work from pure UI state and real-stroke upload evidence;
- retained the long-session Activity DESTROYED timeout as environment/harness evidence, not a product regression, because the same APK/tests passed quickly after a fresh boot and native window release evidence was healthy;
- set current stage to `UI-R4B Screenshot Visual Acceptance Review`;
- added a specific screenshot-review check for expanded Sculpt scene/object access because the report says Sculpt gets no Objects column;
- kept Stage 019 blocked until the fresh UIR4B-S01..S12 set receives owner/coordinator visual PASS;
- reiterated that ARCHITECTURE.md line count alone is not a blocker because the project has no raw 2000-line hard cap.

### v3.38.0 — 2026-08-25
- recorded owner approval of a **48 dp** minimum interactive hit area while allowing visually smaller chrome/glyph geometry;
- recorded owner approval of **`Start Sculpting`** as the current reversible Construction→Sculpt transition label;
- added UI-OWNER-28 and UI-OWNER-29 as binding decisions;
- retired the last owner-decision gate for the UI-R4 correction;
- set `UI-R4 Focused Visual/Motion Correction` to READY with active prompt `AGENT_UI_R4_FOCUSED_VISUAL_MOTION_CORRECTION.md`;
- bound the correction to audit-backed defects only: Sculpt unavailable `+`, Tool Rail state restoration, status lifecycle, precision continuation, anchored motion output, expanded/tablet vocabulary, fill-led selection/radius nesting, Radius/Strength + unit hierarchy, non-`Freeze` copy, 48 dp hit areas and AC20 evidence housekeeping;
- explicitly preserved exact approved palette values, the existing typography system, instant reduced-motion behavior and current reversible Back/Resume semantics;
- kept Stage 019 and every future capability outside the correction stage;
- required fresh screenshot-based owner/coordinator review before Stage 019.

### v3.37.0 — 2026-08-25
- accepted `UI-R4A Three-Tool Comparative Visual + Motion Audit R2` as COMPLETE read-only evidence at unchanged HEAD `a5c152c`;
- recorded that Impeccable, Emil Kowalski and official Remotion skills were actually installed globally for Claude Code, read from disk and applied without modifying the ForgeShape repo/build/APK;
- confirmed UI-R4A still fails AC20 because `artifacts/uir4a/` is untracked and intentionally not ignored;
- upgraded the Sculpt `+` issue from suspicion to source-confirmed P0-class flow defect: the user can choose a primitive in Sculpt before native add refuses;
- accepted source-confirmed P1 defects in Tool Rail state restoration, status lifecycle, precision-surface continuation, expanded Sculpt placement and anchored motion output;
- corrected the precision-sheet diagnosis: bottom inset exists; the visible broken cut comes from the capped height/scroll continuation treatment;
- adjudicated Impeccable's generic Material 3/AndroidX conformance requirement as non-binding because ForgeShape has explicit owner/repo architecture rules for the custom native shell;
- resolved the expanded/tablet principle from existing UI-OWNER-17/UI-OWNER-20 rather than escalating a redundant owner question: persistent large-screen context is allowed only when content-aware/inset and consistent with the phone's spatial vocabulary;
- retained Android instant-cut reduced-motion behavior despite Emil's web-oriented preference for gentler residual motion;
- recorded the owner's earlier border/radius, Radius/Strength and `Freeze` observations as independently corroborated by the audit;
- set `UI-R4 Focused Visual/Motion Correction — OWNER DECISION GATE` as current;
- left exactly two owner choices open before the implementation prompt: 44 vs 48 dp touch floor, and the exact transitional non-`Freeze` label (`Sculpt this shape` recommended);
- kept Stage 019 blocked until focused correction + technical verification + screenshot acceptance.

### v3.36.0 — 2026-08-25
- accepted the first UI-R4A visual/motion audit as COMPLETE read-only manual/proxy evidence at unchanged HEAD `a5c152c`;
- resolved UI-R4A AC20 from UNRESOLVED to **FAIL** because raw Git evidence proves `artifacts/uir4a/` is untracked and intentionally not ignored;
- recorded the audit's P1/P2 findings as a comparison baseline rather than final tool consensus because Impeccable, Emil Kowalski skills and Remotion were all unavailable and nothing was installed;
- owner explicitly authorized installing all three audit toolsets globally/user-level for Claude Code and re-running the audit with actual installed skill content;
- kept all installations outside ForgeShape runtime/build dependencies and prohibited adding Remotion/React or audit tooling to the Android repo/APK;
- introduced `UI-R4A Three-Tool Comparative Visual + Motion Audit R2` as the only current task;
- kept Stage 019 blocked until comparative audit → one focused UI-R4 correction → screenshot acceptance;
- retained UI-OWNER-27: user-facing `Freeze` terminology must be retired with semantically honest transitional wording before the future one-way Sculpt-copy model exists.

### v3.35.0 — 2026-08-25
- received the complete UI-R4A screenshot set `UIR4A-S01..S12` plus `WALK-*` transition evidence;
- deliberately deferred coordinator visual acceptance because the owner requested an independent design/motion audit before using either the coordinator's or owner's visual observations as accepted findings;
- inserted a read-only UI-R4A audit gate using Impeccable critique/audit principles and Emil Kowalski design/motion rules, plus a bounded Remotion feasibility assessment for future motion prototypes/review artifacts;
- recorded that Remotion must not become an Android runtime/product dependency merely for motion design; any use is external prototyping/communication unless separately justified later;
- added `UI-OWNER-27`: final user-facing ForgeShape UI must retire `Freeze` / `Freeze to Sculpt` terminology; current legacy data behavior remains until persistence/one-way derivation are safe, so transitional wording must remain semantically honest;
- kept Stage 019 blocked only until the independent audit is reviewed and visual acceptance chooses PASS vs exactly one focused UI correction stage;
- kept AC20 unresolved pending raw Git-status evidence for `artifacts/uir4a/`.

### v3.34.1 — 2026-08-25
- attached and read the original `AGENT_UI_R4A_MOBILE_WORKSPACE_INTERACTION_REDESIGN.md`, removing the previous evidence limitation caused by the missing execution contract;
- cross-checked the coding-agent report against the prompt's required scope, UIR4A-01..18 tests and AC01–AC20;
- accepted the reported functional implementation and verification as consistent with AC01–AC19, with no detected scope creep into gizmo/Undo/import/topology/CAD features;
- corrected the formal classification to **FUNCTIONAL/TEST PASS / CONTRACT PARTIAL ON AC20 / VISUAL REVIEW PENDING** because the report says the final tree is “clean except `artifacts/uir4a/`” while AC20 requires a final clean tree; raw `git status` must determine whether the screenshot directory is ignored or untracked;
- confirmed from the actual UI-R4A prompt that docs may exceed 2000 lines and no raw line-count hard cap applies, so the reported `ARCHITECTURE.md` 2084-line “cap” issue is a documentation-policy inconsistency, not an automatic blocker;
- corrected stale current-state references so the latest accepted implementation HEAD is `a5c152c` rather than the historical UI-R3R2 baseline `9964522`;
- kept screenshot-based visual acceptance as the only product/UI gate before choosing Stage 019 vs one focused UI correction.

### v3.34.0 — 2026-08-25
- accepted the UI-R4A coding-agent report as **TECHNICAL COMPLETE** at reported final HEAD `a5c152c`;
- recorded the new resting mobile workspace: no permanent bottom Inspector, Objects-owned `+` Add Primitive flow, contextual precision Shape/Transform surfaces, and no fake future tools;
- accepted reported regression evidence: both ABI build, native 11/1695, JVM 57/57, compact/expanded 153/154 with only the known `ui11` IME guard, DEV2/DEV3 PASS, zero geometry work from presentation-only interactions, and a real Sculpt stroke retaining edits across Back/Resume;
- kept UI-R4A **VISUAL REVIEW PENDING** because the required `UIR4A-S01`..`S12` images have not yet been supplied to the coordinator; Stage 019 is not launched until that review chooses PASS vs focused correction;
- recorded that the active UI-R4A execution-prompt file was not present in this handoff conversation, so blanket AC-pass declarations were not independently mapped line-by-line;
- recorded uncommitted `artifacts/uir4a/` as next-stage preflight housekeeping rather than calling the repository literally clean;
- recorded the reported `ARCHITECTURE.md` 2084-line/internal-cap discrepancy and reaffirmed that MASTER PLAN has no project-wide hard 2000-line cap; verify/reconcile live repo policy without inventing an automatic docs-only milestone;
- preserved roadmap order after visual acceptance: Stage 019 transactions/Undo → Stage 020 Move/Rotate gizmo → Stage 018 object commands/hierarchy → Stage 021 `.forge` persistence → BRIDGE-R1.

### v3.33.0 — 2026-08-25
- marked UI-R4A as RUNNING after the owner launched `AGENT_UI_R4A_MOBILE_WORKSPACE_INTERACTION_REDESIGN.md`;
- added a top-level `CURRENT ORCHESTRATION SNAPSHOT` so the MASTER PLAN itself carries current coordinator state across conversations;
- deprecated the separate external `FORGESHAPE_PROJECT_STATE*.md` continuity file; it is no longer carried or updated;
- established the two-file coordinator continuity rule: latest MASTER PLAN + active/current coding-agent prompt only;
- added a formal fresh-conversation handoff protocol requiring the new coordinator to read the full MASTER PLAN once, including version notes, plus the active prompt;
- required one active coding-agent prompt at a time and MASTER PLAN update before every next implementation prompt;
- preserved UI-R4A scope and the likely post-R4A transaction → gizmo → object commands → persistence → one-way Sculpt-copy roadmap.

### v3.32.0 — 2026-08-25
- accepted UI-R3R2 technical baseline `9964522` but rejected the result as the final interaction/layout direction after owner screenshot review;
- read and reconciled the full MASTER PLAN including all retained version notes v2.3.0–v3.31.0;
- corrected the stale Gate P0 statement: dedicated real 16 KB runtime evidence was later completed in Gate P1;
- changed the product north star from reversible Construction↔Freeze/Sculpt mode switching to separate saved Construction and derived Sculpt project documents;
- added UI-OWNER-23 mobile direct-manipulation workspace, UI-OWNER-24 one-way CAD→Sculpt copy, UI-OWNER-25 Objects `+` creation flow and UI-OWNER-26 Subdivide-vs-Remesh vocabulary;
- promoted `.forge` save/load ahead of the one-way CAD→Sculpt bridge because persistence is a data-safety prerequisite;
- reprioritized transaction foundation and direct Move/Rotate gizmo before object-command density while retaining historical stage IDs;
- recorded future `Add from file` inside the Objects `+` creation surface while keeping import post-MVP;
- explicitly added Subdivide/multires-style density and voxel-style Remesh as separate future Sculpt topology operations;
- advanced UI-R4A Mobile Workspace Interaction Redesign to CURRENT;
- prohibited UI-R4A from faking gizmo, import, subdivision, mirror or CAD features that are not yet implemented.

### v3.31.0 — 2026-08-25
- owner re-attached the original Claude design references that explicitly show the approved appearance set;
- corrected the prior mistaken assumption that the missing third appearance was an unknown theme beside Dark + Light;
- recovered the exact approved three-palette set as **Warm Graphite**, **Neutral Charcoal / Steel**, and **Light Charcoal (the lightest dark)**;
- recorded exact semantic role → hex mappings for viewport, chrome, floating, inspector, hairline, text primary, text second, accent, selected-rest, warning, error and success;
- recorded recovered style constraints from the same references: three-tier materials, no real backdrop blur, fill-led control states, focused numeric accent ring, and own-control morph relationships;
- re-opened implementation by promoting the next runnable stage to **UI-R3R v2 — Focused Visual Composition Correction + Three Approved Dark Palettes**;
- explicitly marked the already-implemented Warm Light theme as technical foundation / optional internal appearance, not part of the recovered approved three-palette target.

### v3.30.0 — 2026-08-25
- searched available conversation history, uploaded design/report materials, historical MASTER PLAN versions and currently retained image artifacts for the earlier three-palette decision;
- recovered a prior coordinator trace confirming that three color palettes had been reviewed, but not the original exact palette definitions;
- confirmed repository implementation evidence still exposes only Dark + Light and an intentionally extensible semantic theme system;
- marked `UI-OWNER-09` as the implemented two-theme foundation rather than the final appearance-count decision;
- added `UI-OWNER-22`: exactly three owner-approved palettes, with exact third-palette role mapping/source explicitly `SOURCE RECOVERY PENDING`;
- prohibited guessing/reconstructing the missing palette from generic design assumptions;
- superseded the previous runnable UI-R3R prompt until the original Claude Design palette prototype is re-attached/recovered;
- kept repo baseline `5741913` unchanged and made source recovery the only prerequisite before UI-R3R v2.

### v3.29.0 — 2026-08-25
- accepted UI-R3 technical completion at `5741913`;
- rejected UI-R3 visual acceptance after direct review of UIR3-S01..S10;
- recorded concrete screenshot-grounded problems in toolbar, surface hierarchy, compact sheet, expanded docks, Sculpt controls, Tool Rail, start chooser and theme tonal separation;
- approved viewport-first floating/content-aware composition through UI-OWNER-20;
- approved reduced border-led hierarchy through UI-OWNER-21;
- advanced UI-R3R Focused Visual Composition Correction to CURRENT;
- blocked Selection Outline/Stage 018 until a second screenshot review.


### v3.28.0 — 2026-08-25
- accepted DOC-R1 COMPLETE at `006c1e7` and formally closed UI-R2;
- corrected the mistaken project-wide 2000-line hard-cap policy: long docs are allowed when durable truth requires them;
- introduced mandatory selective reading through introduction + heading map + current task-relevant line ranges;
- reconciled stale Stage 014/015 snapshots, Git/device/dependency/risk text with current verified repo state;
- restored the owner-approved CAD path to the active roadmap: Sketch Foundation → Extrude New Body → iterative Sketch/Extrude → persistence → booleans → Add/Cut + Revolve;
- added Sketch/Extrude/Revolve to explicit MVP Construction scope;
- recorded UI-OWNER-19 screenshot-based visual acceptance and DOC-OWNER-02 large-document navigation;
- advanced UI-R3 Visual Quality Polish + Screenshot Review to CURRENT;
- kept Selection Outline separate as UI-R4 rather than mixing renderer work into visual polish.


### v3.27.0 — 2026-08-25
- accepted UI-R2 product implementation at `90a3172` but classified the stage PARTIAL because AC19 failed;
- recorded current-repo proof that start flow, Dark/Light themes, ChromeMotion, Grid and selection pulse are present;
- recorded the single-owner Objects composition and compact/docked presentation;
- accepted two test-only corrections with no product-contract change;
- set DOC-R1 Core Documentation Compaction as CURRENT;
- kept Selection Outline as a later candidate requiring owner scheduling rather than silently starting it.


### v3.26.0 — 2026-08-25
- accepted INPUT-R1V COMPLETE and closed INPUT-R1 as device-verified at `6dd83f1`;
- recorded exact native/JVM/instrumented verification totals and safe emulator evidence;
- recorded that verifier changes corrected two test defects only, not production pointer behavior;
- recorded reusable corrupted-Quickboot diagnosis;
- explicitly marked the orchestration history gap from `feba998` to `bee5564` instead of inventing missing stage completion;
- set UI-R2 Workspace Composition Redesign as the next candidate, with mandatory current-repo preflight.


### v3.25.0 — 2026-08-24
- accepted UI-R1A COMPLETE with zero repo changes;
- recorded that UI architecture is structurally sound but visually prototype-like;
- split UI-R1B into UI-R1B1 visual/start-flow foundation and UI-R1B2 explicit theme/light-mode work;
- moved selection/motion/View/Grid work to UI-R1C based on audit evidence;
- recorded SurfaceView backdrop-blur constraints and deferred real glass;
- recorded stylus pointer-semantics debt before stylus-dependent stages.


### v3.24.0 — 2026-08-24
- accepted Stage 017 COMPLETE at `feba998`;
- recorded the multi-object scene, stable ObjectIds and independent per-body Frozen Sculpt state;
- recorded corrected Sculpt ownership: Frozen mesh per Body, mode/tool/brush/stroke session-wide;
- accepted one instrumented IME failure only because it was reproduced on clean baseline;
- advanced UI-R1A audit to CURRENT instead of Stage 018;
- recorded ARCHITECTURE.md near-cap debt.


### v3.23.0 — 2026-08-24
- formalized the owner premium UI/motion target: CALM + SPATIAL + DYNAMIC + RESPONSIVE + PREMIUM;
- recorded Apple/iOS, Flighty and Nomad Sculpt as quality/workflow references only, never cloning targets;
- defined selective layered translucency/glass rules instead of universal glass styling;
- approved transformation-over-replacement as the primary motion principle where spatial continuity helps;
- added adaptive-workspace and UI-performance quality rules without expanding current phone+tablet support;
- expanded UI-R1 into a staged audit → visual foundation → motion/adaptive/performance modernization program;
- added a final-user-facing UI quality rule while preserving the ability to accept technical/domain stages before polish;
- kept Stage 017 running unchanged and did not authorize implementation in this documentation-only update.


### v3.22.0 — 2026-08-24
- kept the currently running Stage 017 unchanged;
- added UI-R1 immediately after Stage 017 for Editor Workspace visual redesign and Construction-vs-Sculpt start flow;
- approved Light/Dark themes, warm light viewport direction, brief selection pulse + optional outline, and compact View/Grid controls;
- approved scene parent/child hierarchy direction after the flat Stage 017 foundation;
- approved Revolve and exact supported sketch dimensions as bounded CAD direction;
- retained Inventor/Nomad as workflow inspiration only, never cloning targets;
- retained import as later/post-MVP unless a future explicit owner decision changes that boundary.


### v3.21.0 — 2026-08-24
- accepted Pre-017 Correctness Repair COMPLETE at `d04e0b4`;
- recorded active-representation sidedness as the single render/pick/Sculpt truth;
- recorded current-Frozen-mesh edit state as the destructive re-Freeze guard truth;
- recorded hardened device verification including the scalar `.Count` false-PASS fix;
- advanced Stage 017 Multi-object Scene + Hierarchy Foundation to CURRENT;
- kept object commands, Undo, Sketch, booleans, persistence/import and performance refactors outside Stage 017.


### v3.20.0 — 2026-08-24
- accepted Gate P1 COMPLETE at `159af08`;
- recorded real physical ARM64 10k/50k/100k and memory/stability evidence;
- recorded and accepted the ARM64-only barycentric shared-edge picking fix;
- preserved separate dedicated 16 KB runtime evidence while the physical phone uses 4 KB pages;
- advanced Pre-017 correctness repair to CURRENT;
- kept stylus UNVERIFIED as non-blocking;
- kept Undo and whole-mesh performance architecture outside the Pre-017 scope.


### v3.19.0 — 2026-08-23
- physical Galaxy S25 Ultra ARM64 target became available;
- recorded manual ARM64 and package-ABI evidence;
- resumed Gate P1 physical closure using existing 10k/50k/100k stress harness;
- retained 16 KB runtime evidence from the dedicated 16 KB AVD despite the physical phone using 4 KB pages;
- kept Pre-017 audit correctness repair immediately after Gate closure.


### v3.18.0 — 2026-08-23
- recorded the physical Gate P1 closure as BLOCKED exactly because no physical ARM64 serial exists;
- clarified that the existing 16 KB AVD is x86_64 and validates 16 KB runtime/Vulkan but not arm64-v8a execution or physical-device performance;
- stopped rerunning the hardware closure while hardware is absent;
- moved the independent Pre-017 audit correctness repair to CURRENT;
- added an explicit owner checkpoint before Stage 017 if physical ARM64 remains unavailable.


### v3.17.0 — 2026-08-22
- reviewed a full read-only repository audit while physical Gate P1 closure was running;
- queued a single bounded Pre-017 repair for active-representation sidedness, current-mesh re-Freeze guarding, and device-verifier hardening;
- deferred performance optimization until physical Gate metrics exist;
- treated release self-test inclusion as unverified until an actual release artifact is inspected;
- added an owner/architecture checkpoint for Undo/transaction boundaries before editable Sketch/feature history, without changing the current Gate execution.


### v3.16.0 — 2026-08-22
- classified Gate P1 as PARTIAL solely because physical ARM64 evidence is unavailable;
- accepted 16 KB runtime, Vulkan validation, ABI/alignment and x86_64 heavy-mesh harness evidence;
- reduced the only next action to physical ARM64 smoke plus the mandatory 10k/50k/100k ladder;
- explicitly prohibited creating another repair stage while hardware is unavailable;
- Stage 017 remains next immediately after Gate P1 closure.


### v3.15.0 — 2026-08-22
- accepted Stage 016-R2 COMPLETE at `f156ed9`;
- accepted Stage 016 as fully closed;
- recorded explicit-port launch, explicit-serial adb workflow and deterministic self-test fixtures as durable baseline;
- advanced Gate P1 to CURRENT;
- made physical ARM64, real 16 KB runtime, Vulkan validation and measured mesh-density evidence mandatory for Gate P1 COMPLETE;
- allowed stylus evidence to remain explicitly UNVERIFIED when real stylus hardware/input is unavailable;
- prohibited silently installing a large 16 KB runtime image during the gate.


### v3.14.0 — 2026-08-22
- kept Stage 016 product work intact but classified 016-R as PARTIAL;
- rejected auto-assigned emulator port usage after ForgeShape_Stage006 bound prohibited 5554;
- reaffirmed that every adb command must use explicit `-s <serial>`; bare `adb devices` is not an accepted exception;
- inserted 016-R2 to add explicit-port launch isolation and deterministic self-test fixtures;
- Gate P1 remains blocked until remediation itself is clean.


### v3.13.0 — 2026-08-22
- classified Stage 016 as PARTIAL despite technically green Plane work because prohibited emulator-5554 was touched by an unpinned connected test;
- inserted Stage 016-R for device-isolation remediation and safe single-target verification;
- prohibited any cleanup/check against 5554 itself;
- kept Plane implementation commit `6803c73` as the candidate technical baseline;
- Gate P1 remains next after remediation.


### v3.12.0 — 2026-08-21
- accepted Stage 015D COMPLETE at `4718c3d`;
- recorded verified Perspective + implemented Orthographic camera behavior;
- advanced Stage 016 Plane + Primitive Coverage Cleanup to CURRENT;
- fixed Plane contract as finite XZ surface, +Y canonical front, one-sided source topology with two-sided editor usability;
- kept Sketch-plane semantics and later CAD features out of Stage 016.


### v3.11.0 — 2026-08-21
- accepted Stage 015C-R COMPLETE at `171c7ae`;
- closed the inside-out Box defect as a raster front-face convention bug;
- recorded video review: remaining apparent proportion changes are consistent with perspective foreshortening, not proven geometry deformation;
- inserted Stage 015D Orthographic Camera before Plane for CAD-readable inspection;
- kept Perspective as the unchanged default pending later owner preference.


### v3.10.0 — 2026-08-21
- recorded 015C-R as INTERRUPTED by agent quota, not COMPLETE/FAILED;
- required Git-state recovery before further edits;
- prioritized deterministic outward-normal tests before further shader speculation;
- standardized implementation prompts as universal coding-agent prompts, including Antigravity CLI support;
- kept Stage 016 blocked until 015C-R is fully verified.


### v3.8.0 — 2026-08-20
- downgraded Stage 015C from COMPLETE to PARTIAL after owner visual review;
- recorded convex-Box concavity/inside-out appearance as load-bearing shading defect;
- inserted Stage 015C-R to prove winding, render-normal sign and light-space conventions;
- added required outward-normal direction tests before Plane;
- kept Stage 016 Plane blocked until shading/culling conventions are trustworthy.


### v3.7.0 — 2026-08-20
- accepted Platform Fix P2 COMPLETE at `87119bd`;
- recorded the proven Vulkan orientation convention and residual unverified fallback;
- advanced Stage 015C Viewport Shading & Surface Readability to CURRENT;
- kept full PBR out of the immediate stage; default direction is Studio Solid + MatCap + Smooth/Faceted;
- retained Mobbin Pro for relevant static and motion/animation reference.


### v3.6.0 — 2026-08-20
- kept Platform Fix P2 as the current running task;
- inserted Stage 015C Viewport Shading & Surface Readability before Plane;
- selected Studio Solid + MatCap + Smooth/Faceted as the recommended viewport direction;
- deferred full PBR/material preview unless the shading foundation makes a minimal path nearly free;
- added optional cavity/curvature/feature-edge investigation for geometry readability;
- expanded approved Mobbin Pro usage to motion/animation references as well as static UI patterns;
- no current implementation scope was changed while P2 is running.


### v3.5.0 — 2026-08-20
- accepted Stage 015B COMPLETE at commit `cb19617`;
- recorded responsive Editor Workspace and Android UI tests as implemented;
- inserted a bounded renderer orientation fix before Stage 016;
- kept `preTransform` as a hypothesis, not a proven cause;
- retained Stage 016 Plane as the next product feature after renderer correctness is restored.


### v3.4.0 — 2026-08-20
- accepted Documentation Compression Gate as COMPLETE;
- recorded commit `b2fabae` and compact live-doc line counts;
- accepted ARCHITECTURE.md at 1205 lines as a justified target overrun, still below the 2000 hard limit;
- unblocked Stage 015B;
- no product scope or architecture decision changed.


### v3.3.0 — 2026-08-20
- inserted documentation compression gate before Stage 015B;
- hard-capped each core live Markdown file below 2000 lines;
- established lower working targets and Git-history ownership of removed stage detail;
- no product roadmap capability changed.


### v3.2.0 — 2026-08-20
- accepted the owner-decision baseline commit `dd98e01...` as COMPLETE;
- unblocked Stage 015B production UI work;
- changed active terminology to plain-language `Main Editor Interface / Editor Workspace`;
- preserved the approved viewport-first sculpt/CAD UI direction;
- retained stable naming/comment rules and platform-neutral core constraints as load-bearing Stage 015B acceptance criteria.


### v3.1.0 — 2026-08-20
- added owner-approved clear naming and code-comment policy;
- replaced unexplained Forge Shell wording with viewport-first Editor Workspace UI in active owner decisions;
- required responsibility-based production names and canonical domain terminology;
- required concise comments for non-obvious architectural intent/invariants;
- prohibited broad cosmetic renames during unrelated stages;
- required owner-facing choices to be explained in plain language before shorthand.


### v3.0.0 — 2026-08-20
- recorded explicit owner approval of UI-OWNER-01..06;
- added ARCH-OWNER-01 future Apple portability as an architectural constraint;
- added INPUT-OWNER-01 stylus-first ergonomics across Android and future Apple Pencil;
- clarified Construction may begin from primitives or iterative 2D Sketch→Extrude workflows;
- expanded bounded CAD roadmap to multiple planes/planar faces and repeated sketches/features;
- scheduled Extrude New Body first and Add/Cut after boolean infrastructure;
- kept Revolve/Fillet/Chamfer/Shell owner-gated rather than silently expanding MVP;
- preserved explicit Freeze→Sculpt transition and Construction source/history;
- kept Apple UI/backend implementation post-MVP/owner-prioritized while requiring clean portability seams now.


### v2.9.0 — 2026-08-20
- accepted Stage 015B preflight BLOCKED as correct gate behavior;
- recorded zero workspace changes during the blocked run;
- identified ambiguous reuse of D1–D6 between Stage 015A and 015A-R;
- replaced active decision identifiers with UI-OWNER-01..06;
- kept Stage 015B blocked until explicit owner approval;
- no product roadmap or UI recommendation changed.


### v2.8.0 — 2026-08-19
- accepted Stage 015A-R as COMPLETE — DECISION REQUIRED;
- recorded Forge Shell as recommended direction pending owner confirmation;
- recorded WF-1..WF-4 and commit b0561ab;
- defined owner decisions D1–D6;
- blocked Stage 015B until those decisions are resolved;
- retained structured Views + Android UI automation as recommended baseline.


### v2.7.0 — 2026-08-19
- classified Stage 015A design direction as PARTIAL while retaining technical findings;
- made Nomad Sculpt the primary structural sculpt reference for UI re-audit;
- kept Mobbin Pro as supplemental evidence;
- added Shapr3D-like Sketch/Profile→Extrude as CAD interaction reference;
- added bounded 2D Sketch + Extrude to MVP roadmap;
- inserted Stage 015A-R before Stage 015B;
- required real ForgeShape wireframes.


### v2.6.0 — 2026-08-19
- accepted Gate P0 COMPLETE and PROJECT_STATUS v0.15.0;
- recorded local Git baseline and r29 migration commits;
- recorded static/package 16 KB PASS and runtime 16 KB UNVERIFIED;
- split Stage 015 into 015A audit/decision and 015B implementation;
- made Mobbin Pro required in 015A as bounded UX research/audit input;
- added Android UI automation strategy as a 015A/015B concern;
- retained Plane at Stage 016.


### v2.5.0 — 2026-08-19
- accepted Stage 014 COMPLETE and PROJECT_STATUS v0.14.0;
- made Gate P0 current: Git → NDK r29 → 16 KB;
- retained Stage 015 as UI/UX Architecture Foundation;
- moved Plane to Stage 016;
- recorded Stage 014 debt and stale-source regression evidence.


### v2.4.0 — 2026-08-19
- recorded owner approval for local Git after Stage 014;
- recorded owner approval to migrate to stable NDK r29 (`29.0.14206865`);
- made r29 migration + 16 KB proof an immediate post-Stage-014 gate;
- moved UI/UX Architecture Foundation to Stage 015, before Plane and multi-object expansion;
- moved Plane + primitive cleanup to Stage 016;
- approved Mobbin Pro as a bounded UX auditor/designer reference tool;
- recorded owner preference for targeted design questions when several meaningful options exist;
- resolved import as post-MVP;
- added a post-MVP import readiness gate requiring physical heavy-mesh evidence including million-triangle-range tests on capable hardware;
- retained exact supported mesh limits as evidence-based rather than promised.


### v2.3.0 — 2026-08-19
- converted MASTER PLAN to compact current-truth structure;
- removed obsolete active descriptions of old stages;
- corrected update date/current milestone semantics;
- treated Vulkan/JNI/CMake as current technology, not candidates;
- added mandatory 16 KB Gate P0;
- moved physical ARM64 and responsive UI architecture immediately after Stage 015;
- moved Undo transaction model before booleans;
- moved `.forge` save/load/autosave and an early GLB vertical slice before booleans;
- separated Construction Mirror/Symmetry from Sculpt Stroke X Symmetry terminology;
- added explicit owner decision on import;
- kept Stage 014 Cone+Capsule unchanged;
- shortened prompt/report policy;
- retained prior MASTER PLAN versions as historical archive instead of duplicating their history here.

---

*End of history. Current truth lives only in `FORGESHAPE_MASTER_PLAN_v4.0.58.md` section 1.*
