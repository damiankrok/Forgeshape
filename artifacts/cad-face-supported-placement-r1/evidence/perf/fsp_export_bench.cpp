// CAD-FACE-SUPPORTED-PLACEMENT-R1 host benchmark (evidence only, never built
// into the product). Median wall time of exportSceneAsGlb over two 12-body
// scenes: twelve independent CAD bodies, and one producer carrying an 11-deep
// chain of face-supported dependents (each on the previous body's far cap, the
// worst case for resolveWorldModel). Linked once against the exporter at the
// base tree (7c2fc91) and once against this stage's, same objects otherwise.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_scene.h"

namespace {
using namespace forgeshape;

CadBodyState rect(double w, double h, double d) {
    CadBodyState state;
    cadBaseSketch(state).plane = Workplane::XY;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = w;
    r.height = h;
    addSketchEntity(&cadBaseSketch(state), r);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = d;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

SceneObject* add(ConstructionScene& scene, const CadBodyState& state) {
    CadStatus why = CadStatus::Ok;
    SceneObject* body = scene.addCadBody(state, &why);
    if (body != nullptr) publishSceneObject(*body);
    return body;
}

SceneObject* onFarCap(ConstructionScene& scene, const SceneObject* producer, double s) {
    const CadBodyState& ps = producer->cadOrNull()->state();
    std::vector<CadFace> faces;
    enumerateCadFaces(ps, &faces);
    for (const CadFace& face : faces) {
        if (face.token.kind != CadFaceKind::CapFar || !face.eligible) continue;
        CadBodyState child = rect(s, s, 0.1);
        cadBaseSketch(child).hasFaceSupport = true;
        cadBaseSketch(child).faceSupport.producerObjectId = producer->objectId();
        cadBaseSketch(child).faceSupport.producerLocalFeatureId = kCadFeatureId;
        cadBaseSketch(child).faceSupport.face = face.token;
        cadBaseSketch(child).faceSupport.lineageToken = cadTopologySignature(ps);
        return add(scene, child);
    }
    return nullptr;
}

double medianMicros(const ConstructionScene& scene, int runs, size_t* bytesOut) {
    std::vector<double> t;
    for (int i = 0; i < runs; ++i) {
        GlbExportStatus why = GlbExportStatus::NothingToExport;
        const auto a = std::chrono::steady_clock::now();
        const std::vector<uint8_t> bytes = exportSceneAsGlb(scene, ProjectKind::Construction, &why);
        const auto b = std::chrono::steady_clock::now();
        if (why != GlbExportStatus::Ok) return -1.0;
        *bytesOut = bytes.size();
        t.push_back(std::chrono::duration<double, std::micro>(b - a).count());
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}
}  // namespace

int main() {
    const int runs = 401;
    ConstructionScene independent{NoProjectTag{}};
    for (int i = 0; i < 12; ++i) add(independent, rect(1.0 + 0.1 * i, 1.0, 0.5));
    ConstructionScene chain{NoProjectTag{}};
    SceneObject* last = add(chain, rect(2.0, 2.0, 0.5));
    int dependents = 0;
    for (int i = 0; i < 11 && last != nullptr; ++i) {
        last = onFarCap(chain, last, 1.9 - 0.15 * i);
        if (last != nullptr && last->isFaceSupportedCad()) ++dependents;
    }
    size_t bi = 0, bc = 0;
    const double mi = medianMicros(independent, runs, &bi);
    const double mc = medianMicros(chain, runs, &bc);
    std::printf("FSP_EXPORT_BENCH runs=%d independent_bodies=12 median_us=%.1f bytes=%zu\n", runs, mi, bi);
    std::printf("FSP_EXPORT_BENCH runs=%d face_supported_chain_bodies=%d(+1 producer) median_us=%.1f bytes=%zu\n",
                runs, dependents, mc, bc);
    return mi < 0 || mc < 0 || dependents != 11;
}
