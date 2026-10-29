import sys
p='/home/user/Forgeshape/artifacts/function-council-r2/work/req_E.tsv'
rows=[l.rstrip('\n').split('\t') for l in open(p)]
idx={r[0]:i for i,r in enumerate(rows)}
def S(i,**kw):
    r=rows[idx[i]]
    m={'source':1,'horizon':2,'expected':3,'impl':4,'android':5,'jni':6,'cpp':7,'fmt':8,'undo':9,'neg':10,'tests':11,'run':12,'grade':13,'verdict':14,'sev':15,'deps':16,'improve':17,'owner':18}
    for k,v in kw.items(): r[m[k]]=v
S('DOC-010',expected='Each of 72 fixtures decodes to the verdict the self-tests and builder expect (44 Ok, 28 refused) and every Ok CAD body regenerates',
  sev='R2 (seat H obj 15): 22 of 28 refusals are the coarse InvalidSemanticValue; the PLANTED cause is not asserted; no expected-file was passed to the script',
  grade='E1 (refusal) / E2-E5 (specific cause)')
S('DOC-011',expected='A refused load changes nothing: scene, fingerprint, body count, mode, history untouched (domain level, loadProjectDocument)',
  verdict='PARTIAL',grade='E1 (domain) / E2 (JNI)',
  sev='R2 (seat H obj 8): holds at loadProjectDocument; the JNI wrapper cancels the sketch, ends the sketch view and drops g_grabbing/g_strokePending BEFORE a post-decode refusal - see DOC-087',
  improve='move the sketch/gesture reset after validate+runtimeCanEvaluate+editInProgress, or into the commit branch')
S('DOC-028',sev='P3 (crafted/corrupt-but-CRC-valid files only; refused at load, but the refusal at JNI still drops an open sketch - DOC-087)')
S('DOC-038',verdict='PARTIAL',grade='E3 (single writer) / E2 (mechanism of concurrent-writer loss)',
  sev='R2 (seat H obj 9): single-writer only. Two writers sharing recovery.forge.pending (old+new AutosaveController across a recreation) can delete the valid checkpoint via the delete+rename fallback (ProjectCheckpoint.java:104,112) - HYP-E6, reachability E5. No parent-dir fsync (H-E3)')
S('DOC-039',horizon='H0',verdict='PARTIAL',
  sev='P3: kept .pending copy is never offered back, and a concurrent writer can delete or overwrite it (HYP-E6). R2: relabelled H2->H0 (seat H obj 14)')
S('DOC-041',verdict='PARTIAL',grade='E2',
  sev='R2 (seat H obj 9): off UI thread holds, but ProjectCheckpoint.write/read/clear are static and unsynchronized; two controllers coexist across a recreation (HYP-E6), read() length/open race (HYP-E7). Save encodes+fsyncs on the UI thread and holds g_stateMutex during encode, which the render loop also takes per frame (jni:1870-1872) - H-E4')
S('DOC-045',sev='R2: fail-closed at loadProjectDocument; the JNI wrapper side effects of DOC-087 apply to Recover only if a sketch is open, which a launch-time offer does not have')
S('DOC-048',verdict='PARTIAL',sev='P3: live dirty project unprotected until its next edit. R2 (seat H obj 7): CLAUDE.md literally says Discard retires the checkpoint, so whether a cancelled/refused Open restores protection is an owner policy choice',
  owner='Should a cancelled or refused Open after Discard restore checkpoint protection?')
S('DOC-051',verdict='PARTIAL',sev='P3/INFO: missing defensive guard; no reachable wrong outcome (closeProject only from shell paths after the dirty guard, EditorWorkspaceView.java:2420/2446/2580/2641/2721). R2 regrade from DEFECT-CONFIRMED (seat H obj 13)')
S('DOC-052',sev='P3 (native; UI reachability UNDETERMINED). R2 (seat H obj 17): loadProject also does not cancel supportChooser() (closeProject does, jni:8976), nor end Dimensions or the gizmo; those parts UNDETERMINED-SOURCE-GAP/E5 until traced through dismissPrimarySurfacesExcept/onNativeStateChanged')
S('DOC-062',verdict='PARTIAL',grade='E2',
  sev='R2 (seat H obj 8): the cancel also happens when the load is then REFUSED (DOC-087), and openSavedProjectSlot/onOpenProjectDocumentChosen skip the refresh on refusal (EditorWorkspaceView.java:5490-5496, 6095-6103)')
S('DOC-084',grade='E2',verdict='DEFECT-CONFIRMED',impl='not implemented',
  sev='P3 (seat H obj 12; same grade as H-02): Save clears the checkpoint on the UI thread (EditorWorkspaceView.java:5465) while a worker write may be in flight; noteProjectPersisted is posted behind it; next cold launch offers a candidate that may be OLDER than the slot. Reachability timing E5')
# split DOC-077
r=rows[idx['DOC-077']]
a=list(r); b=list(r)
a[0]='DOC-077a'; a[3]='A lost device costs the project nothing (project bytes unchanged across a loss)'; a[13]='E3 (+E1 policy)'; a[14]='PASS-SOURCE-AND-RECORDED-RUNTIME'; a[15]='device path historic only'
b[0]='DOC-077b'; b[3]='RestartRequired triggers an immediate checkpoint and the notice'; b[13]='E2'; b[14]='PARTIAL'
b[7]='EditorWorkspaceView.java:6147-6154 onRendererRestartRequired; sole production caller ForgeShapeActivity.checkRendererLifecycle from onResume (335-336, 371-382)'
b[11]='DiagnosticsAndRendererLossTest.java:226-229 calls onRendererRestartRequired() by hand; host RENDER_RECOVERY 24/0 tests the pure policy only'
b[15]='R2 (seat H obj 10, cross-ref SEAT_H H-01): checkpoint+notice deferred until the next onResume; not triggered at the moment RestartRequired is entered'
rows[idx['DOC-077']]=a
rows.insert(idx['DOC-077']+1,b)
blank=['-']*19
n1=list(blank); n1[:]=['DOC-086','CLAUDE.md autosave rule; SEAT_H H-02','H0','A checkpoint records the fingerprint of the state it actually encoded (a changed project is always checkpointed as it is)','not implemented','AutosaveController','projectFingerprint + encodeProject (two separate g_stateMutex acquisitions)','AutosaveController.java:157 (fingerprint), :173 (encode), :189 (records earlier fingerprint); jni:8936-8947, 8387-8410','recovery.forge','-','-','NONE','source','E2','DEFECT-CONFIRMED','P3: edit during encode + Undo within the 900 ms debounce leaves recovery.forge holding the undone edit, never rewritten. Reachability timing E5','SEAT_H H-02','take fingerprint and encode under one lock (return both from one JNI call)','-']
n2=['DOC-087','CLAUDE.md "a failed load changes nothing at all"; seat H obj 8','H0','The product load entry point changes nothing when the load is refused after decode','partial','Open Saved Project / Open File while a sketch or Edit Sketch is open (mark shown while sketching, ProjectDrawerPolicy.java:109-111)','loadProject','forgeshape_jni.cpp:8463-8478 clears g_grabbing/g_strokePending, sketchSession().cancel(), endSketchView() before loadProjectDocument, which can refuse RefusedEditInProgress (project_state.cpp:250), validate failure, MissingRequiredSection or InvalidSemanticValue (E-05 class)','-','-','post-decode refusal still drops the staged sketch and moves the camera','NONE','source','E2','DEFECT-CONFIRMED','P3: staged sketch / Edit Sketch discarded and view returned by a load that fails; Java does not refresh after a refusal (EditorWorkspaceView.java:5490-5496, 6095-6103)','DOC-011, DOC-062, E-05','run validate + runtimeCanEvaluate + editInProgress before the cancels; cancel only on the commit path','-']
rows.insert(idx['DOC-084']+2,n1)
rows.insert(idx['DOC-084']+3,n2)
open(p,'w').write('\n'.join('\t'.join(r) for r in rows)+'\n')
print(len(rows))
