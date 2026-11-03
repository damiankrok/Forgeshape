// CR-07 (1): device result-array caps vs host check counts, and the Recorder's
// silent-drop semantics. Caps copied from app/src/main/cpp/forgeshape_jni.cpp
// at 70a6c14 (lines quoted in CMD.txt). Read-only: links the unmodified product
// objects built by scripts/host-native-selftests.sh.
#include <cstdio>
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
static void probe(const char* name, int deviceCap, F run) {
    std::vector<R> host(8192);
    const int n = run(host.data(), 8192);
    std::vector<R> dev(deviceCap);
    const int d = run(dev.data(), deviceCap);
    int dfail = 0;
    for (int i = 0; i < d; ++i) if (!dev[i].passed) ++dfail;
    std::printf("%-24s host=%4d device_cap=%4d recorded_at_cap=%4d headroom=%5d %s\n",
                name, n, deviceCap, d, deviceCap - n,
                n > deviceCap ? "DROPPED" : (n == deviceCap ? "AT_CAP" : "ok"));
    (void)dfail;
}

// Verbatim copy of the Recorder shape in forgeshape_project_selftest.cpp:21-33.
struct ProjectLikeRecorder {
    ProjectSelfTestResult* out; int max; int n = 0;
    void check(const char* name, bool ok) {
        if (n < max) { out[n].name = name; out[n].passed = ok; ++n; }
    }
};

int main() {
    probe<CameraSelfTestResult>("CAMERA", 128, runCameraSelfTests);
    probe<PickingSelfTestResult>("PICKING", 256, runPickingSelfTests);
    probe<MeshSelfTestResult>("DYNAMIC_MESH", 128, runMeshSelfTests);
    probe<ConstructionSelfTestResult>("CONSTRUCTION_BOX", 128, runConstructionSelfTests);
    probe<TransformSelfTestResult>("CONSTRUCTION_TRANSFORM", 192, runTransformSelfTests);
    probe<PrimitiveSelfTestResult>("CONSTRUCTION_PRIMITIVE", 256, runPrimitiveSelfTests);
    probe<SphereSelfTestResult>("CONSTRUCTION_SPHERE", 192, runSphereSelfTests);
    probe<ConeCapsuleSelfTestResult>("CONE_CAPSULE", 256, runConeCapsuleSelfTests);
    probe<SculptSelfTestResult>("SCULPT_BRUSH_KERNEL", 768, runSculptSelfTests);
    probe<RenderMeshSelfTestResult>("RENDER_SHADING", 512, runRenderMeshSelfTests);
    probe<SceneSelfTestResult>("SCENE", 256, runSceneSelfTests);
    probe<HistorySelfTestResult>("CONSTRUCTION_HISTORY", 256, runHistorySelfTests);
    probe<GizmoSelfTestResult>("GIZMO", 256, runGizmoSelfTests);
    probe<ProjectSelfTestResult>("PROJECT", 256, runProjectSelfTests);
    probe<RenderRecoverySelfTestResult>("RENDER_RECOVERY", 128, runRenderRecoverySelfTests);
    probe<GltfExportSelfTestResult>("GLTF_EXPORT", 128, runGltfExportSelfTests);
    probe<GltfImportSelfTestResult>("GLTF_IMPORT", 256, runGltfImportSelfTests);
    probe<CadSelfTestResult>("CAD", 256, runCadSelfTests);
    probe<CadA3SelfTestResult>("CAD_A3", 128, runCadA3SelfTests);
    probe<SketchUxSelfTestResult>("SKETCH_UX", 256, runSketchUxSelfTests);
    probe<BodyDimensionsSelfTestResult>("BODY_DIMENSIONS", 256, runBodyDimensionsSelfTests);
    probe<MirrorSelfTestResult>("MIRROR", 128, runMirrorSelfTests);
    probe<CadFeatureSelfTestResult>("CAD_FEATURE", 1024, runCadFeatureSelfTests);
    probe<FreeformSelfTestResult>("FREEFORM", 256, runFreeformSelfTests);
    probe<SurfaceSelfTestResult>("SURFACE", 128, runSurfaceSelfTests);

    // PROJECT at the device cap and one below: what the device sees as the last check.
    {
        std::vector<ProjectSelfTestResult> r(8192);
        const int n = runProjectSelfTests(r.data(), 8192);
        std::printf("PROJECT host_last_check[%d]=%s\n", n - 1, r[n - 1].name);
        std::vector<ProjectSelfTestResult> c(255);
        const int m = runProjectSelfTests(c.data(), 255);
        int f = 0; for (int i = 0; i < m; ++i) if (!c[i].passed) ++f;
        std::printf("PROJECT cap=255 recorded=%d failed=%d last=%s verdict=%s (check '%s' silently absent)\n",
                    m, f, c[m - 1].name, f ? "FAIL" : "OK", r[n - 1].name);
    }
    // A hypothetical 257th check that FAILS, through the identical Recorder shape at cap 256.
    {
        std::vector<ProjectSelfTestResult> r(256);
        ProjectLikeRecorder rec{r.data(), 256};
        for (int i = 0; i < 256; ++i) rec.check("passing_check", true);
        rec.check("hypothetical_257th_FAILING_check", false);
        int f = 0; for (int i = 0; i < rec.n; ++i) if (!r[i].passed) ++f;
        std::printf("SIM 257 checks (257th fails) cap=256 recorded=%d failed=%d device_token=%s\n",
                    rec.n, f, f ? "FORGESHAPE_PROJECT_SELFTEST_FAIL" : "FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)");
    }
    return 0;
}
