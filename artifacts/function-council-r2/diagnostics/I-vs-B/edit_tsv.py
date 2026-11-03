import sys
p='/home/user/Forgeshape/artifacts/function-council-r2/work/req_I.tsv'
rows=[l.rstrip('\n').split('\t') for l in open(p,encoding='utf-8')]
idx={r[0]:r for r in rows}
def s(i,c,v): idx[i][c]=v
def a(i,c,v): idx[i][c]=idx[i][c]+v
# 1
s('TST-005',15,'P2 (latent): PROJECT 256/256 = 0 headroom; no check is lost on this tree (host cap 8192 also records 256); an overflow would be silent on device. Other headroom: CAMERA 119/128, CONSTRUCTION_BOX 100/128 (jni:923), DYNAMIC_MESH 91/128 (jni:902), MIRROR 62/128 (jni:567)')
# 2
s('TST-007',15,'P2: re-runs on every non-config Activity destroy/create (every instrumented test); ~304 s of 667 s test time. Multiplier pre-dates this tree (same stop() at fa0b6b4:581-582); cost growth is suite time (CAD_FEATURE 7.5 s of ~8.4 s on device). Plus DOC-DIVERGENCE: ForgeShapeActivity.java:33-36 comment says once per PROCESS; CLAUDE.md requires only once from start(), never per frame (met)')
# 3
a('TST-027',12,'; docs/CI_CLOUD.md is self-inconsistent: :12 "Push it; GitHub Actions tests it" vs its own table :22-23 "every push to main, every PR to main" (correct); a task branch with an open PR to main IS tested')
s('TST-027',15,'P2: a task-branch push is untested unless dispatched or a PR to main is open')
# 13 order
a('TST-006',15,' Scope: order-independent with an EMPTY process scene (host). Untested: forgeshape_primitive_selftest.cpp:124-128 process_transform_answers_for_no_project_without_reading_a_body short-circuits true when constructionScene().hasProject(), so on a warm Activity restart with a project open (I-02) it passes vacuously (E2)')
# 16
s('TST-036',14,'UNDETERMINED-SOURCE-GAP')
s('TST-036',15,'P3: an assumption skip may still read OK (n tests); kept as hypothesis H-I-1')
# 6
s('TST-040',1,'PROJECT_STATUS.md Next Stage (:4851-4865, "v6 branch merges to main only after ... Tier-5 aggregate once") and Tier-5 policy (:2545)')
s('TST-040',2,'H1')
s('TST-040',3,'The Tier-5 gate before merging to main has device evidence for every androidTest class on the product tree being merged')
a('TST-040',12,'; B-domain classes with no run on f3db953: EditorWorkspaceGizmoTest (46), EditorWorkspaceHistoryTest (20), EditorWorkspaceObjectsTest (10), ObjectsDeleteTest (8), ObjectCommandsTest (1), MirrorSmokeTest (1), BodyDimensionsSmokeTest (1)')
# 8
s('TST-044',12,'E2: 482 performClick() sites in 67 files and 170 viewport-direct dispatch sites (SketchTestSupport.tapViewport/dragSketch/tapSketch) in 26 classes bypass window hit-testing; EditorWorkspaceGizmoTest dispatches drags to the viewport view (:2126, :2138); decor-root, hit-checked gestures exist in 8 test classes: 4 via ModelingOwnerRig touchView/touchId/realGesture/realDrag (CadParametricHistoryOwnerTest, CadRetainedSketchOwnerTest, FreeformOwnerTest, SurfaceOwnerTest; rig press() is performClick and unchecked) and 4 with their own decor-root realGesture (CadFillPickR2Test, CadMultiFaceOwnerTest, CadRevolveOwnerTest, CadSketchDraftingOwnerTest); ~106 call sites by re-count')
s('TST-044',15,'P2: includes the gizmo: no gizmo test can detect chrome standing over a handle')
# 19
s('TST-060',15,'P3: device classes older tree only. Bake rule asserted for INDEPENDENT bodies only: no GLTF_EXPORT check covers a face-supported dependent; forgeshape_gltf_export.cpp:178-180 reads body.transform() not resolveWorldModel; SEAT_B B-02 (P1) exports the dependent at (0,0,0) while drawn at (0,0,3) (re-run E1 by seat I in round 2)')
a('TST-060',17,'Add a GLTF_EXPORT check: face-supported dependent exports at its resolved world placement')
if idx['TST-060'][17].startswith('-Add'): idx['TST-060'][17]=idx['TST-060'][17][1:]
# 18
s('TST-065',15,'P3: device classes older tree only. Gap: no test asserts that a face-supported CAD body refuses transform/gizmo (CAD-A3); see TST-076. No test covers the gizmo after sceneSelectBody onto a locked body, after Undo of Unlock, or over a hidden active body (SEAT_B B-04, B-06; graded there)')
# 20
s('TST-066',11,'SCENE 175 (obj018a=76) for Delete/Rename/Hide/Lock/Duplicate; RefusedFaceSupportedCad asserted in CAD_A3 66 (forgeshape_cad_a3_selftest.cpp:883), not SCENE; ObjectsDeleteTest 8, ObjectCommandsTest 1, EditorWorkspaceObjectsTest 10')
# 14
s('TST-072',7,'suites build local stores/scenes: forgeshape_mesh_selftest.cpp:147,370; forgeshape_construction_selftest.cpp:265,334,384,473; scene/history suites own ConstructionScene; only process-scoped read is forgeshape_primitive_selftest.cpp:124-128 (read-only)')
s('TST-072',15,'INFO: the only process-scoped read is the read-only check in TST-006; the forgeshape_jni.cpp:2120-2125 comment ("mesh and construction self-tests publish revisions of their own into the store") is stale (DOC-DIVERGENCE, INFO)')
# 11
a('TST-014',12,'; local rerun command and exit code unrecorded (provenance incomplete) -> claim rests on CI FAST 40796992679')
hdr=rows[0]
def new(vals): 
    assert len(vals)==19; rows.append(vals)
new(['TST-076','CLAUDE.md Hard rules CAD-A3 ("such a body is NOT independently transformable (SceneObject::isFaceSupportedCad)")','H0','A face-supported CAD body refuses transform writes and the gizmo','not implemented as stated','gizmo / placement editor','transform JNI','isFaceSupportedCad read only at forgeshape_body_commands.cpp:205 (Duplicate) and forgeshape_jni.cpp:7679 (verification seam); no transform/gizmo guard reads it','-','write records an Undo step','none','none: 0 hits for FaceSupported in EditorWorkspaceGizmoTest, ObjectCommandsTest, GlbExportTest; no host check','SEAT_B B-01 diag_face_supported, re-run by seat I round 2 (/tmp/claude-0/fc2-scratch/seatB/diag_face_supported): gizmo visible=1, applyStatus=0, undoDepth 0->1, fingerprintChanged=1, resolved model unchanged (0,0,3)','E1','DEFECT-CONFIRMED','P2 (graded by SEAT_B B-01); test gap: no test','SEAT_B B-01, B-02','Guard transform writes and gizmo activation on isFaceSupportedCad; add host + device check','-'])
new(['TST-077','.github/workflows/ci-device.yml:2 header comment','H0','Workflow comments state the current token count','stale','-','-','-','-','-','-','-','ci-device.yml:2 "(23 self-test tokens...)" vs 25 enforced','E2','DOC-DIVERGENCE','P3','I-04 (sixth place, raised by seat B)','Correct to 25','-'])
with open(p,'w',encoding='utf-8') as f:
    for r in rows: f.write('\t'.join(r)+'\n')
