import sys
p=sys.argv[1]
lines=open(p,encoding='utf-8').read().rstrip('\n').split('\n')
H=lines[0].split('\t'); idx={h:i for i,h in enumerate(H)}
rows=[l.split('\t') for l in lines[1:]]
byid={r[0]:r for r in rows}
DIAG="seat-D host diag (fc2-scratch/D) @70a6c14"
def s(i,**kw):
    r=byid[i]
    for k,v in kw.items(): r[idx[k]]=v
s("MOD-006",dependencies="G-05 (seat G): brush radius reaches native as raw physical px (BrushEdgeControlsView.java:232-235 -> setSculptBrush jni:9152), min 24 px = ~8.7 dp at density 2.75; unit policy UNDETERMINED-SOURCE-GAP (owner/UX), matters for a density-1.0 desktop shell")
s("MOD-007",source="CLAUDE.md SCULPT-FCM-R1 'Not this stage' (pressure); UI-OWNER-06",expected_behaviour="Stylus pressure/tilt/hover not used for deformation (H2 half only; the physical stylus gate is MOD-115)",dependencies="-",owner_decision="-")
s("MOD-014",source="CLAUDE.md 'A control that cannot succeed is not drawn'; ARCHITECTURE.md:3670-3679; CLAUDE.md SCULPT-FCM-R1 (Clear Mask refuses EntryTooLarge); function-council-r1 D6",
  expected_behaviour="Clear Mask refuses EntryTooLarge over the per-entry cap (deliberate, CLAUDE.md SCULPT-FCM-R1); ARCHITECTURE.md:3678 discloses canClearMask() omits the size test; the general CLAUDE.md rule still asks that a control that cannot succeed is not drawn",
  implementation_status="partial: canClearMask omits the size test (disclosed by ARCHITECTURE.md:3678), so a mask over ~87,371 vertices shows the control and the tap is refused with the generic strings.xml:751 message",
  verdict="PARTIAL",severity_gap="P3",
  owner_decision="OWNER: how a user removes a mask too large to retain when it cannot be undone (D-05; precondition: it came from a NotRetained mask stroke or its entries were evicted)")
s("MOD-015",severity_gap="P3 (with MOD-014: a mask built by a NotRetained mask stroke, or whose entries were evicted, and over ~87,371 vertices can be neither undone nor cleared in-session; a large mask from retained strokes is still undoable entry by entry)")
s("MOD-020",tests="host SCUNDO_23_* (note: check name SCUNDO_23_an_oversized_stroke_is_refused_by_name is misleading; the stroke is APPLIED and NotRetained, not refused)")
s("MOD-042",source="PROJECT_STATUS.md:4263-4269 Technical Debt (Stage027 GUARD-2 load path, recorded as 'not in Stage027's scope'); CLAUDE.md Stage027 GUARD-2 scopes the guard to Start / Resume Sculpt only",
  expected_behaviour="Recorded debt: the load path may enter Sculpt over a hidden active body (no H1 rule requires the load path to refuse; GUARD-2 covers Start/Resume only)",
  implementation_status="recorded technical debt, reproduced: loadProjectDocument enters Sculpt for a Sculpt-kind document without asking visibility",
  verdict="PARTIAL",severity_gap="P3 (recorded debt, not a new defect)")
s("MOD-049",tests="NONE in suites (no test imports during a sketch); seat-D diag_d6 (round 2): SketchSession.begin(XY) active, commitImportedGlbScene under the JNI's Sculpt-only guard -> Ok, bodies 1->2, active 1->2, undo 0->1, sketch still active",
  evidence_run_sha=DIAG,evidence_grade="E1 (domain, host) + E2 (JNI/Java guards)",
  dependencies="seat A (shell), seat C (sketch); harm shown = rule violation plus a Construction step and active-body switch under an open sketch; no data loss demonstrated")
s("MOD-057",tests="host obj018a_10_the_copy_has_an_empty_sculpt_history, obj018a_10_the_sources_sculpt_history_is_untouched (forgeshape_scene_selftest.cpp:~907-910) for history; mask half NONE (cloneFrozenSculpt body_commands.cpp:58-80 -> freezeFrom zeroes mask sculpt.cpp:487-497, unasserted)",
  evidence_grade="E1 (history) + E2 (mask)",verdict="PASS-REPRODUCED (history half); IMPLEMENTED-UNTESTED (mask half)")
s("MOD-064",evidence_grade="E1 (CAD curved wall, host C1_CH_*) + E2 (Freeform/Surface occlusion: representation-neutral pickSceneSnapshot, support_chooser.cpp:181-199; no check puts a Freeform/Surface body in front of a plane)",
  verdict="PASS-REPRODUCED (CAD curved-wall case); IMPLEMENTED-UNTESTED (Freeform/Surface case, journey J-D6)")
for i in ("MOD-072","MOD-073"):
    s(i,dependencies="G-05 (seat G): kFreeformTapSlopUnits 24 is reference units x density (freeform_session.h:45) while selection/sketch slops are 24 physical px (selection.h:37, sketch_session.h:210) -> ~3x tolerance at density 2.75; unit policy UNDETERMINED-SOURCE-GAP (owner/UX)")
s("MOD-079",expected_behaviour="Turning a Symmetry plane ON over an asymmetric cage, and a level whose faces x 4^level exceeds kMaxFreeformDerivedQuads (131072, freeform.h:178), are not drawn as available (turning a plane OFF always succeeds; the seeded Box never reaches the budget)",
  implementation_status="partial: chips always drawn; refreshFromNative sets only the active state (FreeformEditorView.java:245-266); symmetry ON over an asymmetric cage and an over-budget level are drawn and then refused by name",
  owner_decision="OWNER/UX: chips are also state indicators in a selector group; whether to hide or disable them is a UX decision, not purely a rule fix")
s("MOD-086",expected_behaviour="Export writes a Freeform body's derived smooth surface (open bodies double-sided; stored-level and closed-single-sided clauses source-only)",
  tests="NONE in suites; seat-D diag_d5 exercised an OPEN Freeform Plane only: capture Ok, doubleSided=1, fromSculpt=0; level and closed-body clauses E2 (gltf_export.cpp:253-267)",
  evidence_grade="E1 (open body, double-sided) + E2 (level, closed single-sided)",verdict="PASS-REPRODUCED (open bodies export, double-sided); IMPLEMENTED-UNTESTED (stored level, closed body)")
s("MOD-109",expected_behaviour="Export writes a Surface body's patches and solid (open bodies double-sided; closed/thickened single-sided clause source-only)",
  tests="NONE in suites; seat-D diag_d5 exercised an OPEN extruded Surface only (Ok, doubleSided=1); closed/thickened body E2 (gltf_export.cpp:268-280)",
  evidence_grade="E1 (open body, double-sided) + E2 (closed)",verdict="PASS-REPRODUCED (open bodies export, double-sided); IMPLEMENTED-UNTESTED (closed/thickened)")
s("MOD-087",expected_behaviour="No user-facing string says T-Spline or SubD (CLAUDE.md MODELING-FOUNDATIONS-R1 B and UI vocabulary; NURBS is not a banned word, only H2 scope)",
  possible_improvement="extend the vocabulary JVM test to T-Spline/SubD")
s("MOD-091",expected_behaviour="Bounded host timings of Freeform level-4 subdivision and drag are measured, not gated",implementation_status="measured on x86_64 host only",
  verdict="PASS-REPRODUCED (bounded host timings measured; not gated)",severity_gap="- (open500_l4 49.6 ms on host already exceeds a 16.7 ms frame; device interactivity OWNER-PHYSICAL-UNVERIFIED)")
s("MOD-111",expected_behaviour="Bounded host timings of Surface patch/loft/stitch/thicken regeneration are measured, not gated",implementation_status="measured on x86_64 host only",
  verdict="PASS-REPRODUCED (bounded host timings measured; not gated)",severity_gap="- (device interactivity OWNER-PHYSICAL-UNVERIFIED)")
s("MOD-114",verdict="PARTIAL",severity_gap="P3 (evidence age: current-tree device result UNDETERMINED, no run)")
new=["MOD-115","PROJECT_STATUS.md:4113-4115 Known Issues (STYLUS-G1 BLOCKED-ENV-STYLUS-G1); UI-OWNER-06","H1","Physical stylus gate (real stylus events through the live input path) closed on physical hardware","blocked: attempted at 6c09c7c, BLOCKED-ENV-STYLUS-G1","-","-","forgeshape_input.h semantic samples","-","-","-","NONE (needs physical device + stylus)","-","E4","BLOCKED-OWNER","-","physical S Pen device","-","OWNER-PHYSICAL-UNVERIFIED"]
assert len(new)==len(H)
if "MOD-115" not in byid: rows.append(new)
for r in rows: assert len(r)==len(H),r[0]
open(p,'w',encoding='utf-8').write('\t'.join(H)+'\n'+'\n'.join('\t'.join(r) for r in rows)+'\n')
print(len(rows))
