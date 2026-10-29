// FC2 seat I: is surface_bad_ref_v1 refused for its documented reason (Trim names feature 7)?
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_surface.h"
#include "forgeshape_sketch.h"
using namespace forgeshape;
static std::vector<uint8_t> load(const char* p){ std::ifstream in(p, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
int main(int argc, char** argv) {
    std::vector<uint8_t> b = load(argv[1]);
    ProjectDocument doc; ProjectCodecStatus st = decodeProject(b.data(), b.size(), &doc);
    std::printf("parent decode=%s bodies=%zu\n", projectCodecStatusName(st), doc.surface.bodies.size());
    if (doc.surface.bodies.empty()) return 1;
    const SurfaceBodyState base = doc.surface.bodies[0].state;
    std::printf("parent features:"); for (auto& f : base.features) std::printf(" %u", (unsigned)f.id); std::printf("\n");
    for (uint32_t target : {7u, (uint32_t)base.features.front().id}) {
        SurfaceBodyState s = base;
        CadSketch cutter; cutter.plane = Workplane::XY;
        SketchCircle c; c.center = SketchPoint{0.0, 0.0}; c.radius = 0.5;
        SketchEntityId eid = kNoSketchEntity; addSketchEntity(&cutter, c, &eid);
        const uint32_t cid = appendSurfaceSketch(&s, cutter, 0.0);
        SurfaceFeature f; f.kind = SurfaceFeatureKind::TrimSurface; f.section.sketchId = cid;
        f.regions = surfaceDefaultPatchRegions(cutter); f.target = SurfaceFeatureId{target}; f.keepInside = false;
        appendSurfaceFeature(&s, f);
        SurfaceFeatureId failed = kNoSurfaceFeature;
        SurfaceStatus v = validateSurfaceBodyState(s, &failed);
        SurfaceBodyMesh mesh; SurfaceRegenerationReport rep; SurfaceStatus g = regenerateSurfaceBody(s, &mesh, &rep);
        std::printf("trim target=%u validate=%s failed_feature=%u regenerate=%s\n", target, surfaceStatusName(v),
                    (unsigned)failed, surfaceStatusName(g));
    }
}
