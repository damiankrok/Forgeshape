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
template <typename R, typename F>
static void run1(const char* name, F run) {
    static std::vector<R> res; res.assign(8192, R{});
    int n = run(res.data(), 8192); int f = 0;
    for (int i = 0; i < n; ++i) if (!res[i].passed) { ++f; std::printf("  FAIL %s %s\n", name, res[i].name); }
    std::printf("%s %d checks %d failed\n", name, n, f);
}
int main() {  // REVERSE of the production emission order, and CAD_FEATURE twice
    run1<SurfaceSelfTestResult>("SURFACE", runSurfaceSelfTests);
    run1<FreeformSelfTestResult>("FREEFORM", runFreeformSelfTests);
    run1<CadFeatureSelfTestResult>("CAD_FEATURE", runCadFeatureSelfTests);
    run1<MirrorSelfTestResult>("MIRROR", runMirrorSelfTests);
    run1<BodyDimensionsSelfTestResult>("BODY_DIMENSIONS", runBodyDimensionsSelfTests);
    run1<SketchUxSelfTestResult>("SKETCH_UX", runSketchUxSelfTests);
    run1<CadA3SelfTestResult>("CAD_A3", runCadA3SelfTests);
    run1<CadSelfTestResult>("CAD", runCadSelfTests);
    run1<GltfImportSelfTestResult>("GLTF_IMPORT", runGltfImportSelfTests);
    run1<GltfExportSelfTestResult>("GLTF_EXPORT", runGltfExportSelfTests);
    run1<RenderRecoverySelfTestResult>("RENDER_RECOVERY", runRenderRecoverySelfTests);
    run1<ProjectSelfTestResult>("PROJECT", runProjectSelfTests);
    run1<GizmoSelfTestResult>("GIZMO", runGizmoSelfTests);
    run1<HistorySelfTestResult>("CONSTRUCTION_HISTORY", runHistorySelfTests);
    run1<SceneSelfTestResult>("SCENE", runSceneSelfTests);
    run1<RenderMeshSelfTestResult>("RENDER_SHADING", runRenderMeshSelfTests);
    run1<SculptSelfTestResult>("SCULPT_BRUSH_KERNEL", runSculptSelfTests);
    run1<ConeCapsuleSelfTestResult>("CONE_CAPSULE", runConeCapsuleSelfTests);
    run1<SphereSelfTestResult>("CONSTRUCTION_SPHERE", runSphereSelfTests);
    run1<PrimitiveSelfTestResult>("CONSTRUCTION_PRIMITIVE", runPrimitiveSelfTests);
    run1<TransformSelfTestResult>("CONSTRUCTION_TRANSFORM", runTransformSelfTests);
    run1<ConstructionSelfTestResult>("CONSTRUCTION_BOX", runConstructionSelfTests);
    run1<MeshSelfTestResult>("DYNAMIC_MESH", runMeshSelfTests);
    run1<PickingSelfTestResult>("PICKING", runPickingSelfTests);
    run1<CameraSelfTestResult>("CAMERA", runCameraSelfTests);
    run1<CadFeatureSelfTestResult>("CAD_FEATURE(2nd)", runCadFeatureSelfTests);
    run1<ProjectSelfTestResult>("PROJECT(2nd)", runProjectSelfTests);
}
