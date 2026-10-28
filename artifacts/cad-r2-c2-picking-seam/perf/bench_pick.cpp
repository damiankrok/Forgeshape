// CAD-R2-C2 picking hot-path harness (evidence only; never part of the APK).
//
// Compiled unchanged against two product trees (C1 `08406cf` and the C2
// candidate) and linked against PRODUCT objects only (no *_selftest.o), the
// layout C1 proved comparable. Per sample, each workload repeats a fixed
// number of picks; the sample is the mean microseconds per pick.
//
//   interior   pickTriangleMesh, a 100 x 100 quad grid (20,000 triangles),
//              ray through a cell interior
//   seam       the same mesh, ray exactly through a shared diagonal
//   miss       the same mesh, ray beside it (every triangle tested, none hit)
//   far_seam   the 250 m orthographic eye's ray onto the seam (C1's input)
//   scene      pickSceneSnapshot over 8 CAD bodies stacked along the ray
//
// Usage: bench_pick <samples> <warmup>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_camera.h"
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_scene.h"
#include "forgeshape_selection.h"
#include "forgeshape_sketch.h"

using namespace forgeshape;

namespace {

double perPick(int reps, const std::chrono::steady_clock::time_point& t0) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count()
           / reps;
}

}  // namespace

int main(int argc, char** argv) {
    const int samples = argc > 1 ? std::atoi(argv[1]) : 30;
    const int warmup = argc > 2 ? std::atoi(argv[2]) : 3;

    const int n = 100;
    std::vector<float> positions;
    std::vector<uint32_t> indices;
    for (int j = 0; j <= n; ++j) {
        for (int i = 0; i <= n; ++i) {
            positions.push_back(-1.0f + 2.0f * i / n);
            positions.push_back(-1.0f + 2.0f * j / n);
            positions.push_back(0.0f);
        }
    }
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            const uint32_t a = j * (n + 1) + i, b = a + 1, c = a + (n + 1), d = c + 1;
            indices.insert(indices.end(), {a, b, c, c, b, d});
        }
    }
    TriangleMeshView mesh;
    mesh.positions = positions.data();
    mesh.positionStride = sizeof(float) * 3;
    mesh.vertexCount = static_cast<uint32_t>(positions.size() / 3);
    mesh.indices = indices.data();
    mesh.indexCount = static_cast<uint32_t>(indices.size());

    const Vec3 down{0.0f, 0.0f, -1.0f};
    const Ray interior{{0.013f, 0.027f, 3.0f}, down};
    const Ray seam{{0.01f, 0.01f, 3.0f}, down};  // the diagonal of cell (50, 50)
    const Ray miss{{1.5f, 0.0f, 3.0f}, down};
    const Ray farSeam{{0.01f, 0.01f, 250.0f}, down};

    ConstructionScene scene{NoProjectTag{}};
    for (int k = 0; k < 8; ++k) {
        CadSketch s;
        SketchRectangle r;
        r.center = SketchPoint{0.0, 0.0};
        r.width = 0.5;
        r.height = 0.5;
        SketchEntityId id = kNoSketchEntity;
        addSketchEntity(&s, r, &id);
        ExtrudeFeature e;
        e.profileEntityId = id;
        e.depth = 0.2;
        SceneObject* body = scene.addCadBody(makeCadBodyState(s, e));
        if (body == nullptr) return 2;
        TransformValues placed = body->transform().values();
        placed.positionZ = -1.0 * k;
        body->transform().setValues(placed);
        publishSceneObject(*body);
    }
    const SceneSnapshot snapshot = scene.snapshot();
    CameraController camera;
    camera.setViewport(1000, 1000);
    camera.frameSketchView(Vec3{0.05f, 0.03f, 0.0f}, Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1});
    const CameraSnapshot cam = camera.snapshot();

    // Sanity: the workloads mean what they say on this tree.
    const bool sane = pickTriangleMesh(interior, mesh, true).hit && pickTriangleMesh(seam, mesh, true).hit
                      && !pickTriangleMesh(miss, mesh, true).hit
                      && pickSceneSnapshot(cam, 500.0f, 500.0f, 1000, 1000, snapshot).hit;
    std::printf("# sane=%d far_seam_hit=%d\n", sane, pickTriangleMesh(farSeam, mesh, true).hit);
    std::printf("sample\tinterior_us\tseam_us\tmiss_us\tfar_seam_us\tscene8_us\n");
    volatile int sink = 0;
    for (int i = -warmup; i < samples; ++i) {
        const int reps = 20;
        auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < reps; ++k) sink += pickTriangleMesh(interior, mesh, true).triangleIndex;
        const double a = perPick(reps, t0);
        t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < reps; ++k) sink += pickTriangleMesh(seam, mesh, true).triangleIndex;
        const double b = perPick(reps, t0);
        t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < reps; ++k) sink += pickTriangleMesh(miss, mesh, true).triangleIndex;
        const double c = perPick(reps, t0);
        t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < reps; ++k) sink += pickTriangleMesh(farSeam, mesh, true).triangleIndex;
        const double d = perPick(reps, t0);
        t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 200; ++k) sink += pickSceneSnapshot(cam, 500.0f, 500.0f, 1000, 1000, snapshot).triangleIndex;
        const double e = perPick(200, t0);
        if (i >= 0) std::printf("%d\t%.2f\t%.2f\t%.2f\t%.2f\t%.3f\n", i, a, b, c, d, e);
    }
    return sink == 12345 ? 1 : 0;
}
