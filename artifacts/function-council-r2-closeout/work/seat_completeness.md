# Seat completeness re-assessment (FUNCTION-COUNCIL-R2-CLOSEOUT-R1)

Inputs (read-only): `artifacts/function-council-r2/SEAT_{A..J}.md` (Completeness lines,
Round 2 sections, "repair"/"superseded at repair" notes), `SUMMARY.md:47-58`, `TESTS.md:25-28`,
`work/VERIFICATION.md`, and `function-council-r2-closeout/BEFORE.md` (canonical
`FORGESHAPE_MASTER_PLAN_v4.1.61.md` and full OWNER decisions are ABSENT; the 16-file OWNER
archive is historical/partial and is not to be stitched into a plan).

Categories: **(a)** closable by host/source work; **(b)** closable by focused CI DEVICE
(emulator, never physical); **(c)** needs physical device / TalkBack / OWNER;
**(d)** needs the missing canonical plan. Rule applied: a seat with any open (c) or (d)
reason stays PARTIAL; more code reading cannot close it.

The coordinator's focused CI DEVICE run (GlbExportTest, SpatialSketchTest, SketchExtrudeTest,
CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest, CadPlanarFaceOwnerCorrectionTest) is NOT
visible to this seat; nothing below claims its result.

COMPLETE seats unchanged: **E** (document/lifecycle), **G** (desktop portability, design-only
scope), **I** (test/CI). Not re-assessed for downgrade beyond confirming their lines
(`SEAT_E.md:683`, `SEAT_G.md:209`, `SEAT_I.md:398`).

## Seat A — shell and cross-feature journeys (original: PARTIAL, `SEAT_A.md:293`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| Canonical plan sources absent (PARTIAL-PLAN-SOURCE-GAP) | (d) | No | Archive is partial; reconstruction prohibited |
| A-01, A-02, A-05, A-06 Java-layer defects source-proven only (E2) | (b) for A-01/A-02 (a new device case would be needed; no existing test reproduces them, and FSR1B-05 pins A-02's behaviour); (c) for A-02 fix direction (OWNER decision 2) | No | No existing androidTest reproduces them; writing tests is out of scope for an audit-only closeout |
| A-05/A-06 (Activity recreation / process death while picker open) | (b) at best, realistically (c) — needs a recreate/kill-while-picker harness | No | |
| Device journeys partly closed by repair (`SEAT_A.md:281`, CI DEVICE 40805804385, emulator) | (b) — already closed for the classes that run covered | Done (emulator only) | |

**Final: PARTIAL** (reasons (c) and (d) remain).

## Seat B — objects, transform, placement (original: PARTIAL, `SEAT_B.md:355`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| No device run of the domain's androidTest classes on `70a6c14` | (b) | Partly closed by repair: ObjectsDeleteTest, ObjectCommandsTest, MirrorSmokeTest, BodyDimensionsSmokeTest passed in CI DEVICE 40805804385 (TESTS.md:28, emulator). Still stale (E3): EditorWorkspaceGizmoTest, EditorWorkspaceHistoryTest, EditorWorkspaceObjectsTest, SelectionOutlineTest — not in any coordinator run, including the current focused one | Closable only by another focused CI DEVICE dispatch |
| B-01/B-02 (face-supported transform write; GLB export placement) not reproduced on a device | (a) already host-reproduced (E1); device confirmation (b) — GlbExportTest is in the coordinator's focused run but asserts current behaviour, not the defect | No (result not visible here) | |
| B-06 hidden-active-body gizmo behaviour unspecified | (c) OWNER | No | |
| Canonical plan / OWNER decisions absent | (d) | No | |

**Final: PARTIAL.**

## Seat C — CAD / sketch (original: PARTIAL, `SEAT_C.md:426`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| Five core sketch classes stale on this tree (C-06): SpatialSketchTest, SketchExtrudeTest, CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest, CadPlanarFaceOwnerCorrectionTest | (b) | Pending — exactly the coordinator's focused run; result not visible to this seat | Would close this reason only, emulator grade |
| Other Android/UI parts rest on historic E3 runs | (b) | No | |
| Physical-device gates (stylus, hardware GPU, real perf) | (c) | No | Emulator closes no physical gate |
| Canonical plan / OWNER decisions absent | (d) | No | |

**Final: PARTIAL** (even a green focused run leaves (c) and (d)).

## Seat D — Sculpt / Freeform / Surface modelling (original: PARTIAL, `SEAT_D.md:425`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| No device run of Sculpt/Freeform/Surface classes | (b) | Closed by repair: CI DEVICE 40800569636 77/77 (`SEAT_D.md:466-478`, emulator) | |
| D-02 (Import GLB under open sketch) device repro | (a) domain already E1 (`diag_d6.out`); device repro (b) needs a new device case | No | No existing test drives it |
| D-03 (Freeform session survives load) device repro | (a) domain already E1 (`diag_d2.out`, re-run in `SEAT_A.md` Round 2); device (b) needs a new case | No | |
| EditorWorkspaceSculptRetentionTest, EditorWorkspaceHistoryTest historic only | (b) | No | Not in the focused run |
| Physical stylus, mask visuals, real-device perf | (c) | No | OWNER-PHYSICAL-UNVERIFIED |
| Canonical plan absent | (d) | No | |

**Final: PARTIAL.**

## Seat F — Android UI/UX (original: PARTIAL, `SEAT_F.md:462`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| F-03 (bootstrap Back discards sketch with panel open), F-04 (API 36 Back during Revolve), F-05 (comma decimal dropped) source-only (E2) | (b) for F-03/F-04 (need a new platform-Back device case; only `uilr106` drives platform Back); F-05 (a) partially by JVM/source, IME behaviour (c) | No | No existing test reproduces them |
| TalkBack, tablet, foldable, mouse, physical stylus | (c) | No | |
| Round 2 item 15 (guard rerun provenance) UNRESOLVED | (a) coordinator provenance question | No (coordinator-owned) | |
| Canonical plan / OWNER decisions absent | (d) | No | |

**Final: PARTIAL.**

## Seat H — renderer, picking, performance (original: PARTIAL, `SEAT_H.md:401`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| Physical-device, release-ARM64, thermal timings | (c) | No | Host numbers are x86 `-O1` relative indicators only |
| Vulkan validation-layer session since SEL-OUT-R1 (HYP-H3) | (c) — session-scoped adb-only tool on a real/isolated device; CI emulator not authorised for it here | No | |
| Device CAD-R2 parametric timings blocked by H-03 (1023-char perf line truncation) | (a) diagnosis done (E1); evidence itself blocked until a product fix | No | Fixing product is out of scope |
| Performance budgets, frame pacing, RestartRequired surfacing, min pickable size | (c) OWNER decisions | No | |
| Canonical plan absent | (d) | No | |

**Final: PARTIAL.**

## Seat J — cross-cutting / adversarial (original: PARTIAL, `SEAT_J.md:536`)

| Missing element | Category | Can close now? | Note |
| --- | --- | --- | --- |
| Device-level journeys | (b) | Partly closed by repair: CI DEVICE 40805804385 90/90 (persistence, recovery, process-death, object-command, JNI-hardening; emulator) | |
| Between-stage process kill not evidenced | (b) needs a new device harness | No | |
| Filesystem-failure path (no run) | (a) host fault-injection is possible in principle for the platform-neutral codec; the Android write path is (b) | No | Not run in this closeout |
| Surface hidden-body edits (J-01) | (a) domain already E1 (`j1_hidden_edit`); refusal semantics (c) OWNER OD-J1 | No | |
| Canonical plan / OWNER decisions absent | (d) | No | |

**Final: PARTIAL.**

## Summary

| Seat | Original | Reasons (category) | Can close now? | Final |
| --- | --- | --- | --- | --- |
| A | PARTIAL | plan gap (d); A-01/02/05/06 Java defects source-only (b/c); A-02 fix needs OWNER (c) | No | PARTIAL |
| B | PARTIAL | 4 domain androidTest classes still stale (b); B-01/B-02 device confirmation (b); B-06 OWNER (c); plan gap (d) | No (4 classes closed by repair, emulator) | PARTIAL |
| C | PARTIAL | 5 stale sketch classes (b, pending coordinator focused run, not visible); physical gates (c); plan gap (d) | No | PARTIAL |
| D | PARTIAL | device classes closed by repair (b, done); D-02/D-03 device repro (b); physical stylus/perf (c); plan gap (d) | No | PARTIAL |
| E | COMPLETE | — | — | COMPLETE (unchanged) |
| F | PARTIAL | F-03/F-04 runtime repro (b); F-05 IME (c); TalkBack/tablet/stylus (c); guard provenance (a, coordinator); plan gap (d) | No | PARTIAL |
| G | COMPLETE | — (design-only scope) | — | COMPLETE (unchanged) |
| H | PARTIAL | ARM64/physical/thermal (c); validation layer (c); H-03 blocks device perf (a, needs product fix); budgets OWNER (c); plan gap (d) | No | PARTIAL |
| I | COMPLETE | — | — | COMPLETE (unchanged) |
| J | PARTIAL | device journeys partly closed (b); process-kill harness (b); filesystem failure (a/b); J-01 OWNER OD-J1 (c); plan gap (d) | No | PARTIAL |

Result: 3 COMPLETE, 7 PARTIAL — unchanged. Every PARTIAL seat carries the (d) plan-source gap,
and each also carries at least one (c) reason, so none can be closed by host/source work or by the
pending focused CI DEVICE run. Evidence grade of this re-assessment: E2/E4 (source and dossier
reads); no new run was made for it.
