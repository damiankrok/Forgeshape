// PANEL-H08-1: measure what ConstructionScene::snapshot() (called once per
// presented frame from the render loop under g_stateMutex,
// forgeshape_jni.cpp viewSceneSnapshot) costs for face-supported CAD bodies.
// Builds scenes the same way forgeshape_cad_a3_selftest.cpp CADA3-35 does.
// Measures steady-state (warm) timings, median of K runs, for:
//   chainN: base + N bodies each on the previous body's far cap (depth 1..N)
//   fanN:   base + N bodies all on the base's far cap (depth 1 each)
//   plainN: base + N world-plane CAD bodies (no face support) as a control
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_math.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"

using namespace forgeshape;

static CadBodyState rectBody(double w, double h, double depth) {
    CadBodyState state;
    cadBaseSketch(state).plane = Workplane::XY;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = w;
    r.height = h;
    addSketchEntity(&cadBaseSketch(state), r);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}
static CadFaceToken farCap(const CadBodyState& s) {
    std::vector<CadFace> faces;
    enumerateCadFaces(s, &faces);
    for (const CadFace& f : faces) if (f.token.kind == CadFaceKind::CapFar) return f.token;
    return CadFaceToken{};
}
static CadBodyState childOn(const CadBodyState& producer, ObjectId pid) {
    CadBodyState s = rectBody(1.0, 1.0, 1.0);
    cadBaseSketch(s).hasFaceSupport = true;
    cadBaseSketch(s).faceSupport.producerObjectId = pid;
    cadBaseSketch(s).faceSupport.producerLocalFeatureId = kCadFeatureId;
    cadBaseSketch(s).faceSupport.face = farCap(producer);
    cadBaseSketch(s).faceSupport.lineageToken = cadTopologySignature(producer);
    return s;
}
static double medianSnapshotUs(const ConstructionScene& scene, int k, size_t* items) {
    std::vector<double> t;
    for (int i = 0; i < k; ++i) {
        auto a = std::chrono::steady_clock::now();
        SceneSnapshot s = scene.snapshot();
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - a).count();
        *items = s.size();
        t.push_back(us);
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}
static double medianSigUs(const CadBodyState& st, int k) {
    std::vector<double> t;
    volatile uint64_t sink = 0;
    for (int i = 0; i < k; ++i) {
        auto a = std::chrono::steady_clock::now();
        sink ^= cadTopologySignature(st);
        CadFace f;
        resolveCadFace(st, farCap(st), &f);
        t.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - a).count());
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

int main() {
    const int K = 21;
    {
        CadBodyState s = rectBody(4, 4, 1);
        std::printf("PER_LEVEL signature+resolveFace(+farCap enum) median_us=%.1f\n", medianSigUs(s, 201));
    }
    for (int n : {0, 1, 8, 16, 32, 64}) {
        // chain
        ConstructionScene chain;
        SceneObject* prev = chain.addCadBody(rectBody(4, 4, 1));
        publishSceneObject(*prev);
        bool ok = true;
        for (int i = 0; i < n; ++i) {
            CadStatus why = CadStatus::Ok;
            SceneObject* next = chain.addCadBody(childOn(prev->cadOrNull()->state(), prev->objectId()), &why);
            if (!next) { ok = false; break; }
            publishSceneObject(*next);
            prev = next;
        }
        size_t items = 0;
        double chainUs = medianSnapshotUs(chain, K, &items);
        Mat4 m;
        bool res = chain.resolveWorldModel(prev->objectId(), &m);
        std::printf("CHAIN n=%d built=%d items=%zu snapshot_median_us=%.1f last_z=%.3f resolved=%d\n",
                    n, ok, items, chainUs, res ? mat4TransformPoint(m, Vec3{0, 0, 0}).z : -1.0f, res);
        // fan
        ConstructionScene fan;
        SceneObject* base = fan.addCadBody(rectBody(4, 4, 1));
        publishSceneObject(*base);
        ObjectId bid = base->objectId();
        CadBodyState baseState = base->cadOrNull()->state();
        ok = true;
        for (int i = 0; i < n; ++i) {
            SceneObject* c = fan.addCadBody(childOn(baseState, bid));
            if (!c) { ok = false; break; }
            publishSceneObject(*c);
        }
        double fanUs = medianSnapshotUs(fan, K, &items);
        std::printf("FAN   n=%d built=%d items=%zu snapshot_median_us=%.1f\n", n, ok, items, fanUs);
        // plain control
        ConstructionScene plain;
        for (int i = 0; i <= n; ++i) {
            SceneObject* c = plain.addCadBody(rectBody(4, 4, 1));
            publishSceneObject(*c);
        }
        double plainUs = medianSnapshotUs(plain, K, &items);
        std::printf("PLAIN n=%d items=%zu snapshot_median_us=%.1f\n", n, items, plainUs);
    }
    return 0;
}
