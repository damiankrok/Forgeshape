import csv,sys
p="/home/user/Forgeshape/artifacts/function-council-r2/work/req_F.tsv"
rows=list(csv.reader(open(p),delimiter="\t"))
h=rows[0]; ix={k:i for i,k in enumerate(h)}
R={r[0]:r for r in rows[1:]}
def s(id,col,val): R[id][ix[col]]=val
def a(id,col,val): R[id][ix[col]]=R[id][ix[col]]+val
# obj 3
s("UI-009","source","ForgeShapeActivity.java:68-72 ('\"works\" means the innermost thing the user opened closes first'); EditorWorkspaceLegibilityTest uilr106 test name; CONFLICTS with PRODUCT.md:196-198 ('takes a New-CAD sketch back to Home') and with the deliberate order documented at EditorWorkspaceView.java:1150-1154 ('Then the CAD bootstrap')")
a("UI-009","owner_decision","; owner decision 1 also: panel-open bootstrap Back order (PRODUCT.md:196-198 vs ForgeShapeActivity.java:68-72 disagree) [R2 obj 3]")
# obj 4
s("UI-010","implementation_status","PARTIAL: dismissTopmostSurface handles it (:1166-1175) but hasDismissibleSurface (:1112-1117) omits the revolve state, so where the OnBackInvokedDispatcher is the live route the callback is not registered unless another surface is open. Scope: API 36 with targetSdk 36 (E2 + uilr106 route evidence, EditorWorkspaceLegibilityTest.java:341-347); API 33-35 UNVERIFIED (E5): the manifest has no android:enableOnBackInvokedCallback, so the legacy onBackPressed route may be effective there and would end the revolve")
s("UI-010","negative_refusal_path","app goes to background instead of ending the revolve (API 36/targetSdk 36, in-project, no surface open); API 33-35 UNVERIFIED")
s("UI-010","tests","NONE sends System Back in this state; CadRevolveOwnerTest presses sketch_revolve_back_to_extrude (CadRevolveOwnerTest.java:464) and sends no Back key [R2 obj 4 correction]")
s("UI-010","severity_gap","P2 (F-04; API 36+targetSdk 36; API 33-35 UNVERIFIED)")
# obj 1
s("UI-014","implementation_status","NOT met: the CONTAINER sketch_dimension_label (FrameLayout, SketchDimensionLabelView.java:79) measured 114x126 px = 43.4x48 dp (SketchUxVisualEvidenceTest.java:202 bounds 'dimension_label'); the value chip sketch_dimension_value (:82) is therefore <=114 px wide by inference, not measured. Persistent dimension chips DO set min width (SketchDimensionLabelsView.java:177); SketchDimensionLabelView.java:82-90 does not")
s("UI-014","tests","SketchUxVisualEvidenceTest capture 07 facts (dimension_label = container sketch_dimension_label 114 px; chip not measured)")
s("UI-014","evidence_grade","E1 (container rect) + E2 (chip <= container)")
s("UI-014","severity_gap","P2 (F-01)")
# obj 2
a("UI-018","tests","; test name cadUxR1_11_12_13_rotationTurnsTheViewAndNotTheSketch overclaims: assertions read SKETCH_VIEW_QUARTER_TURNS state bits and entity values only (SketchUxTest.java:217-232), no camera; cadUxR1_08_09_10 Flip likewise reads SKETCH_VIEW_FLIPPED only (:199-205)")
# obj 16
s("UI-021","severity_gap","- (hypothesis H-F1; no severity)")
# obj 6
s("UI-040","source","CLAUDE.md 'Chrome reports, it does not instruct' (a status verdict nobody can read is not reported) [R2 obj 6: the status pill is not a live control and the brush column is not an anchored surface, so 'may never partially cover another live control' does not apply]")
s("UI-040","expected_behaviour","Status-line verdicts are readable, not occluded by other chrome, in every mode including Sculpt")
a("UI-040","tests","; uilr120 (brush top == rail top, EditorWorkspaceLegibilityTest.java:~913-916) and the UI3D-11 VISIBILITY rows PASS over this same state; neither measures occlusion (coverage gap, F-09/I-08)")
s("UI-040","possible_improvement","bound the status pill's width between the brush column and the rail in Sculpt; offsetting the brush column below the pill would break uilr120's asserted top alignment, so that variant is an OWNER layout decision")
# obj 7
for i in ("UI-073","UI-074"):
    a(i,"source"," (no repo rule id; the only repo accessibility rules are content descriptions and SCULPT-H1 'shape + accessible label'; Seat F owner decision 6)")
    s(i,"horizon","H2"); s(i,"verdict","BLOCKED-OWNER"); s(i,"severity_gap","- (recommendation; owner decision 6)")
# obj 8
s("UI-083","source","CLAUDE.md APP-H1 ('Back is one step in every phase and the platform's own at Home'); PRODUCT.md:196 ('System Back always goes one step') vs EditorWorkspaceLegibilityTest uilr106 (:324-328, resting Construction workspace: Back is the platform's)")
s("UI-083","horizon","H1")
s("UI-083","expected_behaviour","What System Back does in an in-project sketch (no surface open): owner records disagree (APP-H1 / PRODUCT.md imply one step away from Home is never the platform's; uilr106 asserts a resting project hands Back to the platform)")
s("UI-083","tests","uilr106 asserts resting Construction hasDismissibleSurface()==false; nothing tests the in-project sketch")
s("UI-083","verdict","DOC-DIVERGENCE"); s("UI-083","severity_gap","P3 (owner decision 1)")
# obj 13
s("UI-084","implementation_status","implemented in part: native FORGESHAPE_SKETCH_TAP: absent from release .so (guard STRING_PATTERN, scripts/ci-release-selftest-guard.sh:46); Java FORGESHAPE_CAD_HUD_TOUCH: gated by FLAG_DEBUGGABLE (CadExtrudeCanvasView.java:249-250, 537-542; not in the guard's scope); Java onKeyDown key hooks REMAIN in release (ForgeShapeActivity.java:402-430) and reach a native debugMeshCommand stub that returns JNI_FALSE under NDEBUG (forgeshape_jni.cpp:2267-2270). Release APK never executed in any tier; debug HUD token not exercised in the fresh device logs (2x SKETCH_TAP:travel, 0x CAD_HUD_TOUCH)")
s("UI-084","evidence_grade","E1 (native string absence; guard provenance incomplete: first invocation guard_exit=2, rerun PASS, rerun command/exit unrecorded) + E2 (Java gate, key-hook stub)")
s("UI-084","verdict","PASS-SOURCE-AND-RECORDED-RUNTIME")
# obj 9
s("UI-087","horizon","- (evidence gap, not a product requirement)")
s("UI-087","verdict","OUT-OF-SCOPE")
s("UI-087","severity_gap","- (moved to TEST_COVERAGE_MATRIX / VALIDATION_PLAN as an evidence gap [R2 obj 9])")
# obj 10 / 11 / 14
a("UI-008","tests","; NOTE: these call workspace.dismissTopmostSurface() directly (HomeFlowTest.java:449-482, SettingsPreferencesTest.java:109) and bypass the API>=33 arming predicate; only uilr106 sends KEYCODE_BACK")
s("UI-017","evidence_grade","E1 (policy, JVM) + E2 (wiring CadExtrudeCanvasView.java:251) + E3")
a("UI-028","evidence_grade"," (recorded measurement, not a gating assertion: Ui3dStateAuditTest.java:40-46)")
a("UI-068","evidence_grade"," (recorded, not asserted)")
s("UI-068","severity_gap","INFO: body labels CLAMP to the edge (CLAMPED rows UI3D-04 after_move 0.44 dp, UI3D-06 65.96 dp and 5.21 dp) while sketch/CAD labels HIDE")
a("UI-039","tests"," (ledger 119/119 is recorded corroboration, not a gate)")
w=csv.writer(open(p,"w",newline=""),delimiter="\t",lineterminator="\n",quoting=csv.QUOTE_NONE,escapechar=None,quotechar='\x00')
for r in rows: w.writerow(r)
