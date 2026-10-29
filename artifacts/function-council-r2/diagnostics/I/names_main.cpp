#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include "forgeshape_body_dimensions_selftest.h"
#include "forgeshape_cad_a3_selftest.h"
#include "forgeshape_cad_feature_selftest.h"
#include "forgeshape_cad_selftest.h"
#include "forgeshape_camera_selftest.h"
#include "forgeshape_cone_capsule_selftest.h"
#include "forgeshape_construction_selftest.h"
#include "forgeshape_freeform_selftest.h"
#include "forgeshape_surface_selftest.h"
#include "forgeshape_gizmo_selftest.h"
#include "forgeshape_gltf_export_selftest.h"
#include "forgeshape_gltf_import_selftest.h"
#include "forgeshape_history_selftest.h"
#include "forgeshape_mesh_selftest.h"
#include "forgeshape_mirror_selftest.h"
#include "forgeshape_picking_selftest.h"
#include "forgeshape_primitive_selftest.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_render_mesh_selftest.h"
#include "forgeshape_render_recovery_selftest.h"
#include "forgeshape_scene_selftest.h"
#include "forgeshape_sculpt_selftest.h"
#include "forgeshape_sketch_ux_selftest.h"
#include "forgeshape_sphere_selftest.h"
#include "forgeshape_transform_selftest.h"
using namespace forgeshape;
#include <map>
#include <string>
template <typename R, typename F>
static void dump(const char* name, F run) {
    static std::vector<R> res; res.assign(8192, R{});
    int n = run(res.data(), 8192);
    std::map<std::string,int> pre;
    for (int i = 0; i < n; ++i) {
        std::string s = res[i].name ? res[i].name : "?";
        // prefix = up to the first '_' followed by a digit, or first 2 underscore-separated tokens
        size_t cut = std::string::npos;
        for (size_t k = 0; k + 1 < s.size(); ++k) if ((s[k]=='_'||s[k]=='-') && isdigit((unsigned char)s[k+1])) { cut = k; break; }
        if (cut == std::string::npos) { size_t u = s.find('_'); cut = u == std::string::npos ? s.size() : u; }
        pre[s.substr(0, cut)]++;
    }
    std::printf("%s %d :", name, n);
    for (auto& kv : pre) std::printf(" %s=%d", kv.first.c_str(), kv.second);
    std::printf("\n");
}
int main() {
    dump<CameraSelfTestResult>("CAMERA", runCameraSelfTests);
    dump<PickingSelfTestResult>("PICKING", runPickingSelfTests);
    dump<MeshSelfTestResult>("DYNAMIC_MESH", runMeshSelfTests);
    dump<ConstructionSelfTestResult>("CONSTRUCTION_BOX", runConstructionSelfTests);
    dump<TransformSelfTestResult>("CONSTRUCTION_TRANSFORM", runTransformSelfTests);
    dump<PrimitiveSelfTestResult>("CONSTRUCTION_PRIMITIVE", runPrimitiveSelfTests);
    dump<SphereSelfTestResult>("CONSTRUCTION_SPHERE", runSphereSelfTests);
    dump<ConeCapsuleSelfTestResult>("CONE_CAPSULE", runConeCapsuleSelfTests);
    dump<SculptSelfTestResult>("SCULPT_BRUSH_KERNEL", runSculptSelfTests);
    dump<RenderMeshSelfTestResult>("RENDER_SHADING", runRenderMeshSelfTests);
    dump<SceneSelfTestResult>("SCENE", runSceneSelfTests);
    dump<HistorySelfTestResult>("CONSTRUCTION_HISTORY", runHistorySelfTests);
    dump<GizmoSelfTestResult>("GIZMO", runGizmoSelfTests);
    dump<ProjectSelfTestResult>("PROJECT", runProjectSelfTests);
    dump<RenderRecoverySelfTestResult>("RENDER_RECOVERY", runRenderRecoverySelfTests);
    dump<GltfExportSelfTestResult>("GLTF_EXPORT", runGltfExportSelfTests);
    dump<GltfImportSelfTestResult>("GLTF_IMPORT", runGltfImportSelfTests);
    dump<CadSelfTestResult>("CAD", runCadSelfTests);
    dump<CadA3SelfTestResult>("CAD_A3", runCadA3SelfTests);
    dump<SketchUxSelfTestResult>("SKETCH_UX", runSketchUxSelfTests);
    dump<BodyDimensionsSelfTestResult>("BODY_DIMENSIONS", runBodyDimensionsSelfTests);
    dump<MirrorSelfTestResult>("MIRROR", runMirrorSelfTests);
    dump<CadFeatureSelfTestResult>("CAD_FEATURE", runCadFeatureSelfTests);
    dump<FreeformSelfTestResult>("FREEFORM", runFreeformSelfTests);
    dump<SurfaceSelfTestResult>("SURFACE", runSurfaceSelfTests);
}
