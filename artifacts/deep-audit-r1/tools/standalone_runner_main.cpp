// Standalone runner for the platform-neutral ForgeShape self-test suites.
// Built with the NDK clang and executed on the emulator; not part of the product.
#include <cstdio>
#include <cstring>

#include "forgeshape_cad_a3_selftest.h"
#include "forgeshape_cad_selftest.h"
#include "forgeshape_camera_selftest.h"
#include "forgeshape_cone_capsule_selftest.h"
#include "forgeshape_construction_selftest.h"
#include "forgeshape_gizmo_selftest.h"
#include "forgeshape_gltf_export_selftest.h"
#include "forgeshape_gltf_import_selftest.h"
#include "forgeshape_history_selftest.h"
#include "forgeshape_mesh_selftest.h"
#include "forgeshape_picking_selftest.h"
#include "forgeshape_primitive_selftest.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_scene_selftest.h"
#include "forgeshape_sculpt_selftest.h"
#include "forgeshape_sketch_ux_selftest.h"
#include "forgeshape_sphere_selftest.h"
#include "forgeshape_transform_selftest.h"

namespace {
int g_totalChecks = 0;
int g_totalFailures = 0;
int g_suites = 0;

template <typename Result, typename Fn>
void runSuite(const char* name, Fn fn) {
    static Result results[2048];
    const int n = fn(results, 2048);
    int failures = 0;
    for (int i = 0; i < n; ++i) {
        if (!results[i].passed) {
            ++failures;
            std::printf("%s_CASE_FAIL:%s\n", name, results[i].name ? results[i].name : "?");
        }
    }
    std::printf("%s_%s (%d checks, %d failures)\n", name, failures == 0 ? "OK" : "FAIL", n, failures);
    g_totalChecks += n;
    g_totalFailures += failures;
    ++g_suites;
}
}  // namespace

int main(int argc, char** argv) {
    const char* only = argc > 1 ? argv[1] : nullptr;
    auto want = [&](const char* tag) { return only == nullptr || std::strstr(tag, only) != nullptr; };
    using namespace forgeshape;
    if (want("CAMERA")) runSuite<CameraSelfTestResult>("FORGESHAPE_CAMERA_SELFTEST", runCameraSelfTests);
    if (want("PICKING")) runSuite<PickingSelfTestResult>("FORGESHAPE_PICKING_SELFTEST", runPickingSelfTests);
    if (want("DYNAMIC_MESH")) runSuite<MeshSelfTestResult>("FORGESHAPE_DYNAMIC_MESH_SELFTEST", runMeshSelfTests);
    if (want("CONSTRUCTION_BOX")) runSuite<ConstructionSelfTestResult>("FORGESHAPE_CONSTRUCTION_BOX_SELFTEST", runConstructionSelfTests);
    if (want("CONSTRUCTION_TRANSFORM")) runSuite<TransformSelfTestResult>("FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST", runTransformSelfTests);
    if (want("CONSTRUCTION_PRIMITIVE")) runSuite<PrimitiveSelfTestResult>("FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST", runPrimitiveSelfTests);
    if (want("CONSTRUCTION_SPHERE")) runSuite<SphereSelfTestResult>("FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST", runSphereSelfTests);
    if (want("CONE_CAPSULE")) runSuite<ConeCapsuleSelfTestResult>("FORGESHAPE_CONE_CAPSULE_SELFTEST", runConeCapsuleSelfTests);
    if (want("SCULPT_BRUSH")) runSuite<SculptSelfTestResult>("FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST", runSculptSelfTests);
    if (want("SCENE")) runSuite<SceneSelfTestResult>("FORGESHAPE_SCENE_SELFTEST", runSceneSelfTests);
    if (want("CONSTRUCTION_HISTORY")) runSuite<HistorySelfTestResult>("FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST", runHistorySelfTests);
    if (want("GIZMO")) runSuite<GizmoSelfTestResult>("FORGESHAPE_GIZMO_SELFTEST", runGizmoSelfTests);
    if (want("PROJECT")) runSuite<ProjectSelfTestResult>("FORGESHAPE_PROJECT_SELFTEST", runProjectSelfTests);
    if (want("GLTF_EXPORT")) runSuite<GltfExportSelfTestResult>("FORGESHAPE_GLTF_EXPORT_SELFTEST", runGltfExportSelfTests);
    if (want("GLTF_IMPORT")) runSuite<GltfImportSelfTestResult>("FORGESHAPE_GLTF_IMPORT_SELFTEST", runGltfImportSelfTests);
    if (want("CAD_SELFTEST")) runSuite<CadSelfTestResult>("FORGESHAPE_CAD_SELFTEST", runCadSelfTests);
    if (want("CAD_A3")) runSuite<CadA3SelfTestResult>("FORGESHAPE_CAD_A3_SELFTEST", runCadA3SelfTests);
    if (want("SKETCH_UX")) runSuite<SketchUxSelfTestResult>("FORGESHAPE_SKETCH_UX_SELFTEST", runSketchUxSelfTests);
    std::printf("RUNNER_SUMMARY suites=%d checks=%d failures=%d %s\n", g_suites, g_totalChecks,
                g_totalFailures, g_totalFailures == 0 ? "PASS" : "FAIL");
    return g_totalFailures == 0 ? 0 : 1;
}
