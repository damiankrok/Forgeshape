// Seat D diagnostic 5: GLB export of a Freeform and a Surface body (no suite asserts it). Scratch only.
#include <cstdio>
#include "forgeshape_freeform.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"
#include "forgeshape_surface.h"
#include "forgeshape_sketch.h"
using namespace forgeshape;
int main() {
    ConstructionScene scene(NoProjectTag{});
    ConstructionHistory history{scene};
    ObjectId ff = kNoObject;
    createFreeformBody(scene, history, 1, &ff);  // Plane: open cage -> double sided expected
    SurfaceBodyState s;
    CadSketch sk; sk.plane = Workplane::XY;
    SketchLine l; l.start = SketchPoint{0, 0}; l.end = SketchPoint{1, 0};
    SketchEntityId e = kNoSketchEntity; addSketchEntity(&sk, l, &e);
    const uint32_t id = appendSurfaceSketch(&s, sk, 0.0);
    SurfaceFeature f; f.kind = SurfaceFeatureKind::ExtrudedSurface; f.section.sketchId = id; f.section.curves = {e}; f.distance = 0.5;
    appendSurfaceFeature(&s, f);
    SurfaceStatus why = SurfaceStatus::Ok;
    scene.addSurfaceBody(s, &why);
    GlbExportScene captured;
    const GlbExportStatus st = captureGlbExportScene(scene, ProjectKind::Construction, &captured);
    GlbExportStatus enc = GlbExportStatus::Ok;
    const std::vector<uint8_t> glb = exportSceneAsGlb(scene, ProjectKind::Construction, &enc);
    std::printf("INFO bodies=%zu capture=%s exported=%zu bytes encode=%s",
                scene.bodyCount(), glbExportStatusName(st), glb.size(), glbExportStatusName(enc));
    for (const GlbExportBody& b : captured.bodies)
        std::printf(" [doubleSided=%d fromSculpt=%d]", (int)b.doubleSided, (int)b.fromSculpt);
    std::printf("\n");
    return 0;
}
