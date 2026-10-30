// CR-02 (FUNCTION-COUNCIL-R2-CLOSEOUT-R1, audit only): transform writes on a
// FACE-SUPPORTED CAD body, measured through the PROCESS-SCOPED domain objects the
// JNI uses (constructionScene(), constructionHistory(), gizmoSession(),
// sculptSession(), applyConstructionTransform). forgeshape_jni.cpp cannot be
// built on the host, so `jniApplyBoxTransformDomain` is a line-for-line copy of
// the guard + write sequence in forgeshape_jni.cpp:1349-1385 (applyBoxTransform)
// and `jniSetGizmoActiveDomain` of forgeshape_jni.cpp:3833-3856 (setGizmoActive),
// minus logging. No product code is modified.
#include <cstdio>
#include <cstdint>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_project_state.h"
#include "forgeshape_project_document.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_transform.h"
#include "forgeshape_sculpt.h"
#include <cmath>
#include <string>
using namespace forgeshape;

static CadBodyState rectBody(double w, double h, double depth) {
    CadBodyState state;
    cadBaseSketch(state).plane = Workplane::XY;
    SketchRectangle r; r.center = SketchPoint{0.0, 0.0}; r.width = w; r.height = h;
    addSketchEntity(&cadBaseSketch(state), r);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}
static CadFaceToken tokenFor(const CadBodyState& s, CadFaceKind kind) {
    std::vector<CadFace> faces; enumerateCadFaces(s, &faces);
    for (auto& f : faces) if (f.token.kind == kind) return f.token;
    return CadFaceToken{};
}

// Copy of forgeshape_jni.cpp:1349-1385 (inside the g_stateMutex scope).
static TransformApplyResult jniApplyBoxTransformDomain(const TransformValues& requested, const char** refusal) {
    TransformApplyResult result; *refusal = "none";
    if (!constructionScene().hasProject()) { *refusal = "NoProject"; result.status = TransformUpdateStatus::Rejected; return result; }
    if (constructionScene().activeBody().locked()) { *refusal = "BodyLocked"; result.status = TransformUpdateStatus::Rejected; return result; }
    ScopedConstructionEdit edit(constructionHistory());
    result = applyConstructionTransform(requested);
    return result;
}
// Copy of forgeshape_jni.cpp:3833-3856.
static const char* jniSetGizmoActiveDomain(bool active) {
    const ConstructionScene& scene = constructionScene();
    const bool activeIsLocked = scene.hasProject() && scene.activeBody().locked();
    if (active && sculptSession().inSculptMode()) { gizmoSession().setActive(false); return "REFUSED:in_sculpt_mode"; }
    if (active && activeIsLocked) { gizmoSession().setActive(false); return "REFUSED:body_locked"; }
    gizmoSession().setActive(active);
    return gizmoSession().active() ? "ACTIVE:1" : "ACTIVE:0";
}

static uint64_t fnv(const std::vector<uint8_t>& b) { uint64_t h = 0xCBF29CE484222325ull; for (uint8_t c : b) { h ^= c; h *= 0x100000001B3ull; } return h; }

struct Probe { size_t undo; uint64_t fp; TransformValues stored; Mat4 world; bool resolved; };
static Probe probe(ObjectId id) {
    Probe p{}; ConstructionScene& s = constructionScene();
    p.undo = constructionHistory().undoDepth();
    p.fp = projectSemanticFingerprint(s, ProjectKind::Construction);
    p.stored = s.findBody(id)->transform().values();
    p.resolved = s.resolveWorldModel(id, &p.world);
    return p;
}
static void printProbe(const char* tag, const Probe& p) {
    std::printf("%s undoDepth=%zu fingerprint=0x%016llx stored.pos=(%.4f,%.4f,%.4f) stored.rot=(%.4f,%.4f,%.4f) resolved=%d world.t=(%.4f,%.4f,%.4f)\n",
                tag, p.undo, (unsigned long long)p.fp, p.stored.positionX, p.stored.positionY, p.stored.positionZ,
                p.stored.rotationX, p.stored.rotationY, p.stored.rotationZ, (int)p.resolved, p.world.m[12], p.world.m[13], p.world.m[14]);
}
static void verdict(const char* tag, const Probe& a, const Probe& b) {
    const bool storedMoved = a.stored.positionX != b.stored.positionX || a.stored.positionY != b.stored.positionY || a.stored.positionZ != b.stored.positionZ;
    const bool worldMoved = a.world.m[12] != b.world.m[12] || a.world.m[13] != b.world.m[13] || a.world.m[14] != b.world.m[14];
    std::printf("%s VERDICT undoDelta=%d fingerprintChanged=%d storedMoved=%d worldMoved=%d\n", tag,
                (int)b.undo - (int)a.undo, (int)(a.fp != b.fp), (int)storedMoved, (int)worldMoved);
}

static CameraSnapshot makeCamera() {
    CameraSnapshot cam{};
    Vec3 eye{4, 5, 9}, tgt{0, 0, 1.5};
    cam.view = mat4LookAt(eye, tgt, Vec3{0, 1, 0});
    cam.proj = mat4Perspective(60.0f * 3.14159265f / 180.0f, 1080.0f / 2000.0f, 0.05f, 200.0f);
    cam.eye = eye; cam.target = tgt; cam.projection = ProjectionMode::Perspective; cam.orthoHalfHeightMeters = 1.0f;
    return cam;
}

// Gizmo drag on the active body through the process-scoped session, the way
// forgeshape_jni.cpp:9932-9934 begins it (gizmo.active() && Down && count==1).
static void gizmoDrag(const char* tag, ObjectId id) {
    const int W = 1080, H = 2000;
    CameraSnapshot cam = makeCamera();
    std::printf("%s setGizmoActive(true) -> %s\n", tag, jniSetGizmoActiveDomain(true));
    GizmoSession& g = gizmoSession();
    GizmoSnapshot gs = g.snapshot(cam, W, H);
    Probe a = probe(id);
    std::printf("%s gizmo visible=%d pivot=(%.4f,%.4f,%.4f) vs drawn world.t=(%.4f,%.4f,%.4f)\n", tag, (int)gs.visible,
                gs.pivot.x, gs.pivot.y, gs.pivot.z, a.world.m[12], a.world.m[13], a.world.m[14]);
    Vec3 grab{}; float x = 0, y = 0;
    if (!gizmoHandleGrabPoint(gs, GizmoHandle::AxisX, &grab) || !projectWorldToScreen(cam, grab, W, H, &x, &y)) { std::printf("%s no grab point\n", tag); return; }
    const GizmoHandle hit = g.hitTest(cam, x, y, W, H);
    const bool began = hit == GizmoHandle::AxisX && g.beginDrag(7, cam, x, y, W, H);
    std::printf("%s grab px=(%.1f,%.1f) hit=%d beginDrag=%d\n", tag, x, y, (int)hit, (int)began);
    if (!began) return;
    // Move the finger along the projected X axis direction by ~150 px.
    Vec3 tip{grab.x + 1.0f, grab.y, grab.z}; float tx = 0, ty = 0; projectWorldToScreen(cam, tip, W, H, &tx, &ty);
    float dx = tx - x, dy = ty - y; float len = std::sqrt(dx * dx + dy * dy); dx /= len; dy /= len;
    bool upd = false;
    for (int i = 1; i <= 10; ++i) upd |= g.updateDrag(7, cam, x + dx * 15.0f * i, y + dy * 15.0f * i, W, H);
    const bool recorded = g.commitDrag();
    Probe b = probe(id);
    std::printf("%s updateDragAny=%d commitDrag(recorded)=%d\n", tag, (int)upd, (int)recorded);
    printProbe((std::string(tag) + " before").c_str(), a);
    printProbe((std::string(tag) + " after ").c_str(), b);
    gs = g.snapshot(cam, W, H);
    std::printf("%s gizmo pivot after=(%.4f,%.4f,%.4f)\n", tag, gs.pivot.x, gs.pivot.y, gs.pivot.z);
    verdict(tag, a, b);
    jniSetGizmoActiveDomain(false);
}

int main() {
    ConstructionScene& scene = constructionScene();
    ConstructionHistory& history = constructionHistory();
    std::printf("0 process scene hasProject=%d (empty at start, APP-H1)\n", (int)scene.hasProject());
    CadStatus why;
    const CadBodyState producerState = rectBody(2.0, 1.0, 3.0);
    SceneObject* producer = scene.addCadBody(producerState, &why);
    publishSceneObject(*producer);
    const ObjectId producerId = producer->objectId();
    CadBodyState child = rectBody(0.5, 0.5, 0.5);
    cadBaseSketch(child).hasFaceSupport = true;
    cadBaseSketch(child).faceSupport.producerObjectId = producerId;
    cadBaseSketch(child).faceSupport.producerLocalFeatureId = kCadFeatureId;
    cadBaseSketch(child).faceSupport.face = tokenFor(producerState, CadFaceKind::CapFar);
    cadBaseSketch(child).faceSupport.lineageToken = cadTopologySignature(producerState);
    SceneObject* dep = scene.addCadBody(child, &why);
    if (!dep) { std::printf("dependent add failed\n"); return 1; }
    publishSceneObject(*dep);
    const ObjectId depId = dep->objectId();
    SceneObject* world = scene.addCadBody(rectBody(1.0, 1.0, 1.0), &why);
    publishSceneObject(*world);
    const ObjectId worldId = world->objectId();
    std::printf("1 setup producer=%llu dependent=%llu(faceSupported=%d) control_world_cad=%llu(faceSupported=%d) hasProject=%d undoDepth=%zu\n",
                (unsigned long long)producerId, (unsigned long long)depId, (int)dep->isFaceSupportedCad(),
                (unsigned long long)worldId, (int)world->isFaceSupportedCad(), (int)scene.hasProject(), history.undoDepth());

    // Pristine bytes for the save-identity comparison.
    ProjectCodecStatus cs;
    const std::vector<uint8_t> pristine = encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction), &cs);
    std::printf("1 pristine encode status=%d bytes=%zu fnv=0x%016llx\n", (int)cs, pristine.size(), (unsigned long long)fnv(pristine));

    // ---- (c1) typed write on the FACE-SUPPORTED body (applyBoxTransform domain path)
    scene.setActiveBody(depId);
    Probe a = probe(depId);
    TransformValues req = a.stored; req.positionX += 5.0; req.rotationZ += 30.0;
    const char* refusal = nullptr;
    TransformApplyResult r = jniApplyBoxTransformDomain(req, &refusal);
    Probe b = probe(depId);
    std::printf("C1 active=%llu applyBoxTransform-domain status=%s refusal=%s\n", (unsigned long long)scene.activeBodyId(),
                transformUpdateStatusName(r.status), refusal);
    printProbe("C1 before", a); printProbe("C1 after ", b); verdict("C1", a, b);

    // Saved bytes after the invisible edit
    ProjectDocument doc = captureProjectDocument(scene, ProjectKind::Construction);
    const std::vector<uint8_t> edited = encodeProjectV1(doc, &cs);
    ProjectDocument back; const ProjectCodecStatus ds = decodeProject(edited.data(), edited.size(), &back);
    for (auto& pb : back.scene.bodies) if (pb.objectId == depId)
        std::printf("C1 save: encode status=%d bytes=%zu fnv=0x%016llx equalsPristine=%d; decode status=%d dependent stored pos=(%.4f,%.4f,%.4f) rotZ=%.4f\n",
                    (int)cs, edited.size(), (unsigned long long)fnv(edited), (int)(edited == pristine), (int)ds,
                    pb.transform.positionX, pb.transform.positionY, pb.transform.positionZ, pb.transform.rotationZ);

    // Undo restores
    const bool undone = history.undo();
    Probe u = probe(depId);
    std::printf("C1 undo=%d\n", (int)undone); printProbe("C1 undone", u);
    std::printf("C1 fingerprint after undo equals pristine=%d\n", (int)(u.fp == a.fp));

    // ---- (c2) gizmo drag on the FACE-SUPPORTED body
    scene.setActiveBody(depId);
    gizmoDrag("C2", depId);
    history.undo();

    // ---- (d) controls: independent bodies accept and move
    scene.setActiveBody(worldId);
    Probe ca = probe(worldId);
    TransformValues creq = ca.stored; creq.positionX += 5.0; creq.rotationZ += 30.0;
    TransformApplyResult cr = jniApplyBoxTransformDomain(creq, &refusal);
    Probe cb = probe(worldId);
    std::printf("D1 control world-plane CAD active=%llu status=%s refusal=%s\n", (unsigned long long)scene.activeBodyId(), transformUpdateStatusName(cr.status), refusal);
    printProbe("D1 before", ca); printProbe("D1 after ", cb); verdict("D1", ca, cb);
    history.undo();

    // Producer move: the dependent's DRAWN place follows (proves resolveWorldModel is live, not frozen)
    scene.setActiveBody(producerId);
    Probe pa = probe(producerId), pda = probe(depId);
    TransformValues preq = pa.stored; preq.positionX += 5.0;
    TransformApplyResult pr = jniApplyBoxTransformDomain(preq, &refusal);
    Probe pb2 = probe(producerId), pdb = probe(depId);
    std::printf("D2 control producer status=%s\n", transformUpdateStatusName(pr.status));
    printProbe("D2 producer before", pa); printProbe("D2 producer after ", pb2); verdict("D2 producer", pa, pb2);
    printProbe("D2 dependent before", pda); printProbe("D2 dependent after ", pdb); verdict("D2 dependent", pda, pdb);
    history.undo();

    scene.setActiveBody(worldId);
    gizmoDrag("D3", worldId);
    history.undo();

    // Lock guard control: the only refusal the path has, shown working.
    scene.setActiveBody(depId);
    scene.findBody(depId)->setLocked(true);
    Probe la = probe(depId);
    TransformApplyResult lr = jniApplyBoxTransformDomain(req, &refusal);
    Probe lb = probe(depId);
    std::printf("D4 locked dependent status=%s refusal=%s setGizmoActive(true)->%s\n", transformUpdateStatusName(lr.status), refusal, jniSetGizmoActiveDomain(true));
    verdict("D4", la, lb);
    return 0;
}
