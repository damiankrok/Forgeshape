import csv,sys
p='/home/user/Forgeshape/artifacts/function-council-r2/work/req_J.tsv'
rows=list(csv.reader(open(p),delimiter='\t'))
h=rows[0]; ix={k:i for i,k in enumerate(h)}
R={r[0]:r for r in rows[1:]}
def s(id,**kw):
    for k,v in kw.items(): R[id][ix[k]]=v
C2='C2 DEVICE 40782861799 @30b561b (app/src/main+androidTest identical to 70a6c14)'
s('ADV-001',tests='PROJECT host suite FSR1A_07_a_failed_load_leaves_the_scene_untouched/_active_body_untouched/_mode_untouched (project_selftest.cpp:1834-1843), FSR1A_13_a_failed_load_preserves_session_history (:1845); J2 fuzz only for memory safety and decode/load agreement (loadRefused=0)',
  evidence_run_sha='fresh audit host run PROJECT 256/0 (scratchpad/fc2/host_selftests.txt) @70a6c14; J2 ASan/UBSan @70a6c14 (memory safety); FULL_SHARDED 36607747079 @fa0b6b4 (older tree)')
s('ADV-012',evidence_run_sha=C2+' (EditorWorkspaceProjectActionsTest)')
s('ADV-013',possible_improvement='read validated pending when slot missing, or atomic move; impact: previous slot copy lost, newer bytes orphaned in .pending, current work stays in memory and checkpoint')
s('ADV-018',horizon='H2-pending-decision',verdict='BLOCKED-OWNER',severity_gap='P3',implementation_status='consistent with stated rules (allocator is stored truth; APP-H1 asks by fingerprint)')
s('ADV-026',horizon='H0',verdict='DEFECT-CONFIRMED',dependencies='A-02; DOC-DIVERGENCE: PRODUCT.md:193-194 states "recovered is not asked about" as intended, conflicting with CLAUDE.md Autosave rule')
s('ADV-027',expected_behaviour='Save-and-continue before Open Saved Project never overwrites (unannounced) the saved project it is about to open',tests='NONE (HomeFlowTest _10 covers Open File only)')
s('ADV-028',evidence_run_sha=C2+' (HomeFlowTest); FULL_SHARDED 36607747079 @fa0b6b4')
s('ADV-037',evidence_run_sha='fresh audit host run (scratchpad/fc2/host_selftests.txt) @70a6c14; '+C2+' (HomeFlowTest)')
s('ADV-039',tests='SKETCH_UX host CADUXR1_33_edit_sketch_cancel_is_exact_non_mutation (sketch_ux_selftest.cpp:638) for Cancel; J1 (commit, undo depth +1) for Finish',
  evidence_run_sha='fresh audit host run SKETCH_UX 118/0 @70a6c14; seat J j1_hidden_edit @70a6c14')
s('ADV-041',evidence_run_sha='seat J j4_replay @70a6c14; '+C2+' (HomeFlowTest:298)',evidence_grade='E1+E3')
s('ADV-046',horizon='H0',source='CLAUDE.md CAD-R0-A1A2 (creation refused below JNI and withdrawn above it while a sketch is drawn)',evidence_run_sha='seat A diag_import_during_sketch SEATA_IMPORT_DURING_SKETCH=ACCEPTED @70a6c14 (SEAT_A.md:134)',evidence_grade='E1')
s('ADV-051',source='Consistency question (no repo rule requires refusal; Stage027 GUARD-2 is Sculpt-only; PRODUCT.md:530-535)',horizon='H2-pending-decision',verdict='BLOCKED-OWNER',
  implementation_status='edits allowed; no SOLID candidate drawn for a target outside the snapshot (PARTIAL P3 UX observation); sketch overlay and extrude arrow are drawn (jni.cpp:1988-1992)')
s('ADV-053',expected_behaviour='Lock refuses only being moved; everything else works (PRODUCT.md:537-542)',implementation_status='CAD/Surface match PRODUCT.md (PASS-SOURCE E2); Freeform refuses edits with BodyLocked (forgeshape_freeform_session.cpp:278) undocumented',
  evidence_grade='E1+E2',verdict='DOC-DIVERGENCE',dependencies='D-09; Freeform outlier')
s('ADV-061',expected_behaviour='Release carries no self-tests; debug-only JNI seams are inert (no-op bodies) in a release build',implementation_status='self-tests absent (E1, guard rerun 00:57; run_status records first attempt guard_exit=2 at 00:54); debug seams kept as exported no-op bodies (jni.cpp:7893-7911, 7967-7972, 9083) - inert, not compiled out (E2)',
  verdict='PASS-REPRODUCED')
s('ADV-063',tests='CadRevolveOwnerTest devRev01 (sketch_revolve_begin + tapAxis/realGesture MotionEvents), devRev05 (sketch_revolve_change_axis + tap)',evidence_run_sha=C2+' 67/67',evidence_grade='E3',verdict='PASS-SOURCE-AND-RECORDED-RUNTIME',severity_gap='-',dependencies='-',possible_improvement='-',implementation_status='implemented')
s('ADV-064',implementation_status='partial: seams used as setup - sketchSetRevolveAxis (CadParametricHistoryOwnerTest:241, CadRetainedSketchOwnerTest:1262), sketchToggleRegion (CadPlanarFaceRuntimeTest:529; CadFillPickR2Test nativeToggle setup only); DOC-DIVERGENCE: CadPlanarFaceRuntimeTest.toggleFace javadoc names a product row path that does not exist',
  tests='CadPlanarFaceRuntimeTest:521-529; CadParametricHistoryOwnerTest:241; CadRetainedSketchOwnerTest:1262')
s('ADV-066',implementation_status='partial (4 editors convert; Freeform and Surface seeded with unitless literals and not converted; sketch Offset seeded via displayUnit().format but not converted on a mid-mode unit change; no unit caption)')
s('ADV-067',implementation_status='parsers accept comma (ExactExpression.java:36, LengthUnit.parse); three canvas editors key listener DigitsKeyListener("0123456789.-") drops comma (CadExtrudeCanvasView:644, SketchDimensionLabelView:112, BodyDimensionLabelsView:127)',
  tests='source review; J3 shows only the parser works',evidence_grade='E2',verdict='DEFECT-CONFIRMED',dependencies='F-05 (owner)',possible_improvement='add , to the three key listeners')
s('ADV-071',evidence_run_sha=C2+' (HomeFlowTest); FULL_SHARDED 36607747079 @fa0b6b4')
w=csv.writer(open(p,'w',newline=''),delimiter='\t',lineterminator='\n',quoting=csv.QUOTE_NONE,escapechar='\\')
w.writerow(h)
for r in rows[1:]: w.writerow(R[r[0]])
