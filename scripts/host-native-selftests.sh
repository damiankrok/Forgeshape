#!/usr/bin/env bash
# Builds ForgeShape's platform-neutral native domain and every self-test suite
# for the HOST (Linux, g++ or clang++) and runs them -- the same suites a debug
# launch runs from `NativeViewport.start()`, without an emulator.
#
# Developer evidence only. It proves the domain arithmetic, the codec and the
# kernel on the host ABI; it does NOT replace the on-device startup tokens
# (`CI DEVICE`), which are the gate, and it compiles nothing Android-specific:
# `forgeshape_jni.cpp` and `forgeshape_renderer.cpp` are the only two
# translation units that are not platform-neutral, and both are left out.
#
# Usage: scripts/host-native-selftests.sh [suite-filter]
#   suite-filter  optional substring; only suites whose name contains it run.
# Env:   CXX (default g++), HOST_SELFTEST_OUT (default build/host-selftests)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP="$ROOT/app/src/main/cpp"
OUT="${HOST_SELFTEST_OUT:-$ROOT/build/host-selftests}"
CXX="${CXX:-g++}"
FILTER="${1:-}"
mkdir -p "$OUT/obj" "$OUT/kernel"

COMMON=(-std=c++17 -O1 -g0 -I"$CPP" -I"$CPP/third_party/manifold/include"
        -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM)

# The vendored kernel, built once and cached: it never changes under a task.
KLIB="$OUT/kernel/libmanifold.a"
if [ ! -f "$KLIB" ]; then
    pids=()
    for src in "$CPP"/third_party/manifold/src/*.cpp; do
        "$CXX" "${COMMON[@]}" -O2 -w -c "$src" -o "$OUT/kernel/$(basename "$src" .cpp).o" &
        pids+=($!)
    done
    for p in "${pids[@]}"; do wait "$p"; done
    ar rcs "$KLIB" "$OUT"/kernel/*.o
fi

# Every product and self-test source except the two platform-bound ones.
SOURCES=()
for src in "$CPP"/forgeshape_*.cpp; do
    case "$(basename "$src")" in
        forgeshape_jni.cpp|forgeshape_renderer.cpp) continue ;;
    esac
    SOURCES+=("$src")
done

# Incremental: an object is rebuilt when its source or any header is newer.
NEWEST_HEADER="$(ls -t "$CPP"/forgeshape_*.h | head -1)"
pids=()
OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj="$OUT/obj/$(basename "$src" .cpp).o"
    OBJECTS+=("$obj")
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ "$NEWEST_HEADER" -nt "$obj" ]; then
        "$CXX" "${COMMON[@]}" -Wall -Wextra -Wno-unused-parameter -c "$src" -o "$obj" &
        pids+=($!)
        if [ "${#pids[@]}" -ge "$(nproc)" ]; then
            wait "${pids[0]}"; pids=("${pids[@]:1}")
        fi
    fi
done
for p in "${pids[@]}"; do wait "$p"; done

MAIN="$OUT/host_selftest_main.cpp"
cat > "$MAIN" <<'EOF'
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
static const char* gFilter = nullptr;
static int gFailures = 0;
static int gChecks = 0;
template <typename R, typename F>
static void suite(const char* name, F run) {
    if (gFilter && !std::strstr(name, gFilter)) return;
    static std::vector<R> results;
    results.assign(8192, R{});
    const auto t0 = std::chrono::steady_clock::now();
    const int count = run(results.data(), static_cast<int>(results.size()));
    const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
    int failed = 0;
    for (int i = 0; i < count; ++i) {
        if (!results[i].passed) {
            ++failed;
            std::printf("  FAIL %s: %s\n", name, results[i].name ? results[i].name : "?");
        }
    }
    gChecks += count;
    gFailures += failed;
    std::printf("%s %s (%d checks, %d failed, %.0f ms)\n", failed ? "HOST_SUITE_FAIL" : "HOST_SUITE_OK",
                name, count, failed, ms);
}
int main(int argc, char** argv) {
    if (argc > 1 && argv[1][0]) gFilter = argv[1];
    suite<CameraSelfTestResult>("CAMERA", runCameraSelfTests);
    suite<PickingSelfTestResult>("PICKING", runPickingSelfTests);
    suite<MeshSelfTestResult>("DYNAMIC_MESH", runMeshSelfTests);
    suite<ConstructionSelfTestResult>("CONSTRUCTION_BOX", runConstructionSelfTests);
    suite<TransformSelfTestResult>("CONSTRUCTION_TRANSFORM", runTransformSelfTests);
    suite<PrimitiveSelfTestResult>("CONSTRUCTION_PRIMITIVE", runPrimitiveSelfTests);
    suite<SphereSelfTestResult>("CONSTRUCTION_SPHERE", runSphereSelfTests);
    suite<ConeCapsuleSelfTestResult>("CONE_CAPSULE", runConeCapsuleSelfTests);
    suite<SculptSelfTestResult>("SCULPT_BRUSH_KERNEL", runSculptSelfTests);
    suite<RenderMeshSelfTestResult>("RENDER_SHADING", runRenderMeshSelfTests);
    suite<SceneSelfTestResult>("SCENE", runSceneSelfTests);
    suite<HistorySelfTestResult>("CONSTRUCTION_HISTORY", runHistorySelfTests);
    suite<GizmoSelfTestResult>("GIZMO", runGizmoSelfTests);
    suite<ProjectSelfTestResult>("PROJECT", runProjectSelfTests);
    suite<RenderRecoverySelfTestResult>("RENDER_RECOVERY", runRenderRecoverySelfTests);
    suite<GltfExportSelfTestResult>("GLTF_EXPORT", runGltfExportSelfTests);
    suite<GltfImportSelfTestResult>("GLTF_IMPORT", runGltfImportSelfTests);
    suite<CadSelfTestResult>("CAD", runCadSelfTests);
    suite<CadA3SelfTestResult>("CAD_A3", runCadA3SelfTests);
    suite<SketchUxSelfTestResult>("SKETCH_UX", runSketchUxSelfTests);
    suite<BodyDimensionsSelfTestResult>("BODY_DIMENSIONS", runBodyDimensionsSelfTests);
    suite<MirrorSelfTestResult>("MIRROR", runMirrorSelfTests);
    suite<CadFeatureSelfTestResult>("CAD_FEATURE", runCadFeatureSelfTests);
    if (!gFilter || std::strstr("CAD_FEATURE", gFilter)) {
        std::printf("FORGESHAPE_CAD_FEATURE_PERFORMANCE %s\n", cadFeaturePerformanceReport());
    }
    suite<FreeformSelfTestResult>("FREEFORM", runFreeformSelfTests);
    if (!gFilter || std::strstr("FREEFORM", gFilter)) {
        std::printf("FORGESHAPE_FREEFORM_PERFORMANCE %s\n", freeformPerformanceReport());
        std::printf("FORGESHAPE_FREEFORM_GOLDEN_SHA256 %s\n", freeformFixtureDigests());
    }
    suite<SurfaceSelfTestResult>("SURFACE", runSurfaceSelfTests);
    if (!gFilter || std::strstr("SURFACE", gFilter)) {
        std::printf("FORGESHAPE_SURFACE_PERFORMANCE %s\n", surfacePerformanceReport());
        std::printf("FORGESHAPE_SURFACE_GOLDEN_SHA256 %s\n", surfaceFixtureDigests());
    }
    std::printf("%s (%d checks, %d failed)\n", gFailures ? "HOST_SELFTESTS_FAIL" : "HOST_SELFTESTS_OK",
                gChecks, gFailures);
    return gFailures ? 1 : 0;
}
EOF
"$CXX" "${COMMON[@]}" -c "$MAIN" -o "$OUT/host_selftest_main.o"
"$CXX" -o "$OUT/host-selftests" "$OUT/host_selftest_main.o" "${OBJECTS[@]}" "$KLIB"
cd "$ROOT"
"$OUT/host-selftests" "$FILTER"
