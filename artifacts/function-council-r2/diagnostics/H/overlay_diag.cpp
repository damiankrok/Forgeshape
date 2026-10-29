// FC2 seat H diagnostic: sketch overlay size & build cost at the arrangement cap.
#include "forgeshape_cad_multiface_selftest.cpp"
#include <cstdio>
using namespace forgeshape;
int main() {
    for (int n : {8, 24, 40, 63}) {
        Driver d;
        auto t0 = std::chrono::steady_clock::now();
        bool ready = d.finishOn(gridSketch(n, n));
        double finishUs = micros(t0);
        size_t faces = ready ? d.sketch.planarFaceCount() : 0;
        SketchOverlayPtr o0 = d.sketch.overlay(0.01f);
        size_t v0 = o0 ? o0->vertices.size() : 0;
        for (size_t k = 0; k < faces; ++k) d.sketch.togglePlanarFace(k);
        auto t1 = std::chrono::steady_clock::now();
        SketchOverlayPtr o1 = d.sketch.overlay(0.01f);
        double buildUs = micros(t1);
        size_t v1 = o1 ? o1->vertices.size() : 0;
        // camera-only rebuild: change worldPerUnit, measure 20 rebuilds
        double maxUs = 0, tot = 0;
        for (int i = 0; i < 20; ++i) {
            auto t2 = std::chrono::steady_clock::now();
            SketchOverlayPtr o2 = d.sketch.overlay(0.01f + 0.0001f * (i + 1));
            double us = micros(t2); tot += us; if (us > maxUs) maxUs = us;
        }
        auto t3 = std::chrono::steady_clock::now();
        const CadCandidateEvaluation& e = d.sketch.evaluateCandidate();
        double evalUs = micros(t3);
        std::printf("grid=%dx%d ready=%d faces=%zu finish_us=%.0f overlay_vertices_unselected=%zu overlay_vertices_all_selected=%zu ranges=%zu cap=%u build_all_selected_us=%.0f camera_rebuild_us_mean=%.0f max=%.0f eval_us=%.0f eval_status=%s\n",
            n, n, ready, faces, finishUs, v0, v1, o1 ? o1->ranges.size() : 0, kMaxSketchOverlayVertices, buildUs, tot/20, maxUs, evalUs, cadStatusName(e.status));
        d.sketch.cancel();
    }
}
