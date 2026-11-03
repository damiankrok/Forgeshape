// Seat D diagnostic 6 (round 2, for seat G's D-02 demand): with a SketchSession
// open over a live scene, does the domain import commit (the one importGlbDurable
// calls when not in Sculpt) create bodies, switch the active body and record a
// Construction step? Scratch only; never part of the build.
#include <cstdio>
#include "forgeshape_gltf_import.h"
#include "forgeshape_history.h"
#include "forgeshape_import_commit.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_workplane.h"
using namespace forgeshape;
int main() {
    ConstructionScene scene;              // default: one Construction body, as the import selftest Fixture
    ConstructionHistory history{scene};
    SculptSession sculpt;
    publishConstructionObject(scene.activeBody().construction(), scene.activeBody().meshStore());
    SketchSession sketch;
    const CadStatus began = sketch.begin(Workplane::XY);
    const size_t bodiesBefore = scene.bodyCount();
    const ObjectId activeBefore = scene.activeBodyId();
    const size_t undoBefore = history.undoDepth();
    std::printf("INFO before: sketchBegin=%d sketchActive=%d inSculpt=%d bodies=%zu active=%llu undo=%zu editInProgress=%d\n",
                (int)began, (int)sketch.active(), (int)sculpt.inSculptMode(), bodiesBefore,
                (unsigned long long)activeBefore, undoBefore, (int)history.editInProgress());

    // One triangle, one batch: a minimal valid parsed GLB mesh.
    ParsedGlbScene parsed;
    ParsedGlbMesh m;
    m.name = "tri";
    m.positions = {0.f,0.f,0.f, 1.f,0.f,0.f, 0.f,1.f,0.f};
    m.normals = {0.f,0.f,1.f, 0.f,0.f,1.f, 0.f,0.f,1.f};
    m.indices = {0u,1u,2u};
    ParsedGlbBatch b; b.firstIndex = 0; b.indexCount = 3; b.doubleSided = false;
    m.batches = {b};
    parsed.meshes.push_back(m);

    // Mirror importGlbDurable's guard exactly: refuse only in Sculpt.
    ImportCommitReport report;
    ImportCommitStatus st = sculpt.inSculptMode() ? ImportCommitStatus::RefusedInSculpt
                                                  : commitImportedGlbScene(parsed, scene, history, &report);
    std::printf("INFO import: status=%s objects=%d firstObjectId=%llu\n", importCommitStatusName(st),
                report.objects, (unsigned long long)report.firstObjectId);
    std::printf("INFO after: sketchActive=%d bodies=%zu active=%llu undo=%zu\n", (int)sketch.active(),
                scene.bodyCount(), (unsigned long long)scene.activeBodyId(), history.undoDepth());
    const bool harm = st == ImportCommitStatus::Ok && sketch.active()
                      && scene.bodyCount() == bodiesBefore + 1
                      && scene.activeBodyId() != activeBefore
                      && history.undoDepth() == undoBefore + 1;
    std::printf("RESULT import_accepted_under_open_sketch_creates_body_switches_active_records_step=%d\n", (int)harm);
    return 0;
}
