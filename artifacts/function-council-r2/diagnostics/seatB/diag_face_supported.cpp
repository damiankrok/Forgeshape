// FC2 Seat B diagnostic: face-supported CAD body vs transform write, gizmo pivot, GLB export.
#include <cstdio>
#include <cmath>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_project_state.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_transform.h"
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
static void pm(const char* n, const Mat4& m) { std::printf("%s translation=(%.4f,%.4f,%.4f)\n", n, m.m[12], m.m[13], m.m[14]); }

int main() {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history(scene);
    CadStatus why;
    const CadBodyState producerState = rectBody(2.0, 1.0, 3.0);
    SceneObject* producer = scene.addCadBody(producerState, &why);
    std::printf("producer add=%s id=%llu\n", producer ? "ok" : "fail", (unsigned long long)(producer?producer->objectId():0));
    publishSceneObject(*producer);
    CadBodyState child = rectBody(0.5, 0.5, 0.5);
    cadBaseSketch(child).hasFaceSupport = true;
    cadBaseSketch(child).faceSupport.producerObjectId = producer->objectId();
    cadBaseSketch(child).faceSupport.producerLocalFeatureId = kCadFeatureId;
    cadBaseSketch(child).faceSupport.face = tokenFor(producerState, CadFaceKind::CapFar);
    cadBaseSketch(child).faceSupport.lineageToken = cadTopologySignature(producerState);
    SceneObject* dep = scene.addCadBody(child, &why);
    std::printf("dependent add=%s faceSupported=%d\n", dep ? "ok" : "fail", dep ? (int)dep->isFaceSupportedCad() : -1);
    if (!dep) return 1;
    publishSceneObject(*dep);
    const ObjectId depId = dep->objectId();

    Mat4 resolved; scene.resolveWorldModel(depId, &resolved);
    pm("A resolved world model (drawn/picked)", resolved);
    TransformValues tv = dep->transform().values();
    std::printf("A stored transform pos=(%.4f,%.4f,%.4f)\n", tv.positionX, tv.positionY, tv.positionZ);

    // 1. GLB export capture
    GlbExportScene ex; GlbExportStatus es = captureGlbExportScene(scene, ProjectKind::Construction, &ex);
    std::printf("B export status=%s\n", glbExportStatusName(es));
    for (auto& b : ex.bodies) std::printf("B export body %llu translation=(%.4f,%.4f,%.4f)\n", (unsigned long long)b.objectId, b.translation[0], b.translation[1], b.translation[2]);

    // 2. Gizmo pivot for the face-supported active body
    GizmoSession gizmo(scene, history);
    gizmo.setActive(true);
    CameraSnapshot cam{};
    Vec3 eye{4,5,9}, tgt{0,0,1.5};
    cam.view = mat4LookAt(eye, tgt, Vec3{0,1,0});
    cam.proj = mat4Perspective(60.0f*3.14159265f/180.0f, 1080.0f/2000.0f, 0.05f, 200.0f);
    cam.eye = eye; cam.target = tgt; cam.projection = ProjectionMode::Perspective; cam.orthoHalfHeightMeters = 1.0f;
    GizmoSnapshot gs = gizmo.snapshot(cam, 1080, 2000);
    std::printf("C gizmo visible=%d pivot=(%.4f,%.4f,%.4f)\n", gs.visible, gs.pivot.x, gs.pivot.y, gs.pivot.z);

    // 3. Exact-value transform write (as applyBoxTransform does: ScopedConstructionEdit + applyTransformValues on active body)
    const uint64_t fpBefore = projectSemanticFingerprint(scene, ProjectKind::Construction);
    const size_t undoBefore = history.undoDepth();
    TransformValues moved = tv; moved.positionX = 5.0;
    TransformApplyResult r;
    { ScopedConstructionEdit edit(history); r = applyTransformValues(scene.activeBody().transform(), moved); }
    Mat4 resolved2; scene.resolveWorldModel(depId, &resolved2);
    const uint64_t fpAfter = projectSemanticFingerprint(scene, ProjectKind::Construction);
    std::printf("D active=%llu applyStatus=%d undoDepth %zu->%zu fingerprintChanged=%d\n",
                (unsigned long long)scene.activeBodyId(), (int)r.status, undoBefore, history.undoDepth(), (int)(fpBefore != fpAfter));
    pm("D resolved world model after write", resolved2);
    SceneSnapshot snap = scene.snapshot();
    for (auto& it : snap) if (it.objectId == depId) pm("D snapshot model after write", it.model);
    gs = gizmo.snapshot(cam, 1080, 2000);
    std::printf("D gizmo pivot after write=(%.4f,%.4f,%.4f)\n", gs.pivot.x, gs.pivot.y, gs.pivot.z);
    ex = GlbExportScene{}; captureGlbExportScene(scene, ProjectKind::Construction, &ex);
    for (auto& b : ex.bodies) std::printf("D export body %llu translation=(%.4f,%.4f,%.4f)\n", (unsigned long long)b.objectId, b.translation[0], b.translation[1], b.translation[2]);
    return 0;
}
