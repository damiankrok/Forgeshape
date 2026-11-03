// FC2 Seat B diagnostic 2: lock vs gizmo after selection change, hidden-body gizmo,
// allocator monotonicity across closeProject, long-name duplicate naming, face-supported undo.
#include <cstdio>
#include <string>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_body_commands.h"
#include "forgeshape_body_delete.h"
#include "forgeshape_construction.h"
#include "forgeshape_transform.h"
using namespace forgeshape;

static CameraSnapshot cam(Vec3 eye, Vec3 tgt) {
    CameraSnapshot c{};
    c.view = mat4LookAt(eye, tgt, Vec3{0,1,0});
    c.proj = mat4Perspective(60.0f*3.14159265f/180.0f, 1080.0f/2000.0f, 0.05f, 200.0f);
    c.eye = eye; c.target = tgt; c.projection = ProjectionMode::Perspective; c.orthoHalfHeightMeters = 1.0f;
    return c;
}

int main() {
    // ---- 1. Lock vs gizmo after a selection change (domain level) ----
    {
        ConstructionScene scene;  // body 1 (box) active
        ConstructionHistory history(scene);
        publishSceneObject(scene.activeBody());
        SceneObject& b = scene.addBody();  // body 2, active
        publishSceneObject(b);
        TransformValues tv = b.transform().values(); tv.positionX = 3.0; b.transform().setValues(tv);
        setSceneBodyLocked(b.objectId(), true, scene, history);
        scene.setActiveBody(1);
        GizmoSession gizmo(scene, history);
        gizmo.setActive(true);               // offered over unlocked body 1 (what setGizmoActive allows)
        scene.setActiveBody(b.objectId());   // what sceneSelectBody / a viewport tap does; no gizmo call
        const CameraSnapshot c = cam(Vec3{6,4,8}, Vec3{3,0,0});
        GizmoSnapshot gs = gizmo.snapshot(c, 1080, 2000);
        std::printf("1 locked active=%llu locked=%d gizmoVisible=%d pivot=(%.2f,%.2f,%.2f)\n",
                    (unsigned long long)scene.activeBodyId(), (int)b.locked(), gs.visible, gs.pivot.x, gs.pivot.y, gs.pivot.z);
        Vec3 grab{}; float x=0,y=0;
        bool ok = gizmoHandleGrabPoint(gs, GizmoHandle::AxisX, &grab) && projectWorldToScreen(c, grab, 1080, 2000, &x, &y);
        bool began = ok && gizmo.beginDrag(7, c, x, y, 1080, 2000);
        bool moved = began && gizmo.updateDrag(7, c, x + 120.0f, y, 1080, 2000);
        bool committed = began && gizmo.commitDrag();
        std::printf("1 beginDrag=%d moved=%d committed=%d lockedBodyPosX=%.4f undoDepth=%zu\n",
                    began, moved, committed, b.transform().values().positionX, history.undoDepth());
    }
    // ---- 2. Hidden active body still gets a gizmo ----
    {
        ConstructionScene scene; ConstructionHistory history(scene);
        publishSceneObject(scene.activeBody());
        setSceneBodyVisible(1, false, scene, history);
        GizmoSession gizmo(scene, history); gizmo.setActive(true);
        GizmoSnapshot gs = gizmo.snapshot(cam(Vec3{4,3,6}, Vec3{0,0,0}), 1080, 2000);
        std::printf("2 hidden active body: inSnapshot=%zu gizmoVisible=%d\n", scene.snapshot().size(), gs.visible);
    }
    // ---- 3. Allocator across closeProject ----
    {
        ConstructionScene scene; scene.addBody(); scene.addBody();
        const ObjectId before = scene.nextObjectId();
        scene.closeProject();
        SceneObject& n = scene.addBody();
        std::printf("3 nextBefore=%llu afterClose newId=%llu hasProject=%d\n",
                    (unsigned long long)before, (unsigned long long)n.objectId(), (int)scene.hasProject());
    }
    // ---- 4. Long name duplicate ----
    {
        ConstructionScene scene; ConstructionHistory history(scene);
        publishSceneObject(scene.activeBody());
        std::string longName(96, 'a');
        BodyCommandStatus rs = renameSceneBody(1, longName, scene, history);
        DuplicateBodyReport rep; BodyCommandStatus ds = duplicateSceneBody(1, scene, history, &rep);
        const SceneObject* copy = scene.findBody(rep.newBodyId);
        std::printf("4 rename=%s dup=%s copyName.len=%zu copyName='%s' sameAsSource=%d\n",
                    bodyCommandStatusName(rs), bodyCommandStatusName(ds),
                    copy ? copy->name().size() : 0, copy ? copy->name().c_str() : "?",
                    copy ? (int)(copy->name() == longName) : -1);
        std::string name95(93, 'b');
        renameSceneBody(1, name95, scene, history);
        DuplicateBodyReport rep2; duplicateSceneBody(1, scene, history, &rep2);
        DuplicateBodyReport rep3; duplicateSceneBody(1, scene, history, &rep3);
        std::printf("4b 93-char source: copy1='%s' copy2='%s'\n",
                    scene.findBody(rep2.newBodyId)->name().c_str(), scene.findBody(rep3.newBodyId)->name().c_str());
    }
    // ---- 5. Delete selection fallback and allocator ----
    {
        ConstructionScene scene; ConstructionHistory history(scene);
        scene.addBody(); scene.addBody();  // ids 1,2,3 active 3
        DeleteBodyReport rep; DeleteBodyStatus s = deleteSceneBody(3, scene, history, &rep);
        SceneObject& n = scene.addBody();
        std::printf("5 delete last active -> status=%s active=%llu nextNew=%llu\n", deleteBodyStatusName(s),
                    (unsigned long long)rep.activeBodyId, (unsigned long long)n.objectId());
        history.undo();
        std::printf("5 after undo of creation bodies=%zu; undo of delete -> ", scene.bodyCount());
        history.undo();
        std::printf("bodies=%zu has3=%d\n", scene.bodyCount(), scene.findBody(3) != nullptr);
    }
    return 0;
}
