#include <cstdio>
#include <cfloat>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
using namespace forgeshape;
int main(int argc, char** argv) {
  for (int a = 1; a < argc; ++a) {
    std::ifstream in(argv[a], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ProjectDocument d; decodeProject(b.data(), b.size(), &d);
    ConstructionScene scene(NoProjectTag{}); SculptSession s; ConstructionHistory h(scene);
    const ProjectCodecStatus ls = loadProjectDocument(d, scene, s, h);
    GlbExportScene ex; GlbExportStatus st = captureGlbExportScene(scene, d.kind, &ex);
    std::printf("== %s load=%s capture=%s\n", argv[a], projectCodecStatusName(ls), glbExportStatusName(st));
    for (size_t i = 0; i < ex.bodies.size(); ++i) {
        const GlbExportBody& e = ex.bodies[i];
        const SceneObject* body = scene.findBody(e.objectId);
        Mat4 world; const bool ok = scene.resolveWorldModel(e.objectId, &world);
        // world-space AABB of the exported geometry (local-baked + translation) vs drawn (world * mesh)
        float emin[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, emax[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (const auto& v : e.render.vertices) for (int k = 0; k < 3; ++k) { float x = v.position[k] + e.translation[k]; emin[k] = x < emin[k] ? x : emin[k]; emax[k] = x > emax[k] ? x : emax[k]; }
        std::printf("body %llu faceSupported=%d export_translation=(%.4f,%.4f,%.4f) world_model_translation=(%.4f,%.4f,%.4f) resolved=%d export_aabb_z=[%.4f,%.4f] export_aabb_y=[%.4f,%.4f]\n",
            (unsigned long long)e.objectId, body && body->cadFaceSupportOrNull() ? 1 : 0,
            e.translation[0], e.translation[1], e.translation[2], world.m[12], world.m[13], world.m[14], ok ? 1 : 0, emin[2], emax[2], emin[1], emax[1]);
    }
  }
}
