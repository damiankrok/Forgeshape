// FC2 seat A diagnostic: does the durable import commit consult the sketch
// session? (CLAUDE.md: while a sketch is drawn, creation and body switching
// are refused below JNI.) Read-only use of product sources; never in the build.
#include <cstdio>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_construction.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_gltf_import.h"
#include "forgeshape_import_commit.h"
#include "forgeshape_sketch_session.h"

using namespace forgeshape;
int main() {
    ConstructionScene scene;                     // one default body, as the product builds
    ConstructionHistory history{scene};
    publishConstructionObject(scene.activeBody().construction(), scene.activeBody().meshStore());
    const ObjectId activeBefore = scene.activeBodyId();
    const size_t bodiesBefore = scene.bodyCount();
    GlbExportStatus ew = GlbExportStatus::Ok;
    const std::vector<uint8_t> glb = exportSceneAsGlb(scene, ProjectKind::Construction, &ew);
    ParsedGlbScene parsed;
    const GlbImportStatus pw = importGlb(glb.data(), glb.size(), &parsed);
    // Open the process sketch session exactly as the JNI does for New Sketch.
    const CadStatus begun = sketchSession().begin(Workplane::XY);
    ImportCommitReport report;
    const ImportCommitStatus cw = commitImportedGlbScene(parsed, scene, history, &report);
    std::printf("glb_bytes=%zu export=%d parse=%d sketch_begin=%d sketch_active=%d\n",
                glb.size(), (int)ew, (int)pw, (int)begun, (int)sketchSession().active());
    std::printf("commit=%s bodies_before=%zu bodies_after=%zu active_before=%llu active_after=%llu undo_depth=%zu\n",
                importCommitStatusName(cw), bodiesBefore, scene.bodyCount(),
                (unsigned long long)activeBefore, (unsigned long long)scene.activeBodyId(),
                (size_t)history.undoDepth());
    const bool importedDuringSketch = cw == ImportCommitStatus::Ok && sketchSession().active()
            && scene.bodyCount() == bodiesBefore + 1 && scene.activeBodyId() != activeBefore;
    std::printf("SEATA_IMPORT_DURING_SKETCH=%s\n", importedDuringSketch ? "ACCEPTED" : "REFUSED");
    return 0;
}
