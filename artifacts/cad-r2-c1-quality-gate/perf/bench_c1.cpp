// CAD-R2-C1 shared performance harness (evidence only; never part of the APK).
//
// Compiled UNCHANGED against two product trees (the R2 predecessor 636797e and
// the R2 head), with the flags scripts/host-native-selftests.sh uses, and linked
// against each tree's own product objects. It repeats the EXACT workloads of the
// three product metrics in question, many times in one process, so a sample is
// one operation rather than one whole-suite run:
//
//   eval_all          forgeshape_cad_multiface_selftest.cpp: every face of the
//                     63 x 63 grid selected, then SketchSession::evaluateCandidate
//   trim100           forgeshape_sketch_drafting_selftest.cpp: planSketchTrim on
//                     the 50 + 50 line grid at (2.0, 0.1), tolerance 0.02
//   revolve_square    forgeshape_cad_revolve_selftest.cpp: regenerateCadBody of
//                     the canonical revolved square (the metric is the MAX of 20)
//
// For attribution, eval_all is also split into its parts: candidateState(),
// regenerateCadBody(candidate) and -- on a tree that has it -- the R2 judge
// validateCadStateBeyondRegeneration(candidate). Define FS_HAS_R2_JUDGE for that.
//
// Usage: bench_c1 <samples> <warmup>   (prints one TSV row per sample)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_camera.h"
#include "forgeshape_input.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_drafting.h"
#include "forgeshape_sketch_session.h"

using namespace forgeshape;

namespace {

double us(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

SketchEntityId add(CadSketch* s, SketchEntity::Payload p) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(s, std::move(p), &id);
    return id;
}

SketchLine line(double u0, double v0, double u1, double v1) {
    SketchLine l;
    l.start = SketchPoint{u0, v0};
    l.end = SketchPoint{u1, v1};
    return l;
}

// --- eval_all: the multiface Driver, verbatim in behaviour --------------------
struct Driver {
    static constexpr int kW = 1000;
    static constexpr int kH = 1000;
    SketchSession sketch;
    CameraController camera;
    bool begin() {
        if (sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
        return true;
    }
    bool at(TouchAction action, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool place(SketchTool tool, SketchEntity::Payload exact) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.13, 0.27}, kW, kH, &x0, &y0)
            || !sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.61, 0.83}, kW, kH, &x1, &y1)) {
            return false;
        }
        const size_t before = sketch.sketch().entities.size();
        bool ok = at(TouchAction::Down, x0, y0);
        for (int step = 1; step <= 4; ++step) {
            const float t = step / 4.0f;
            ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        ok &= at(TouchAction::Up, x1, y1);
        if (!ok || sketch.sketch().entities.size() != before + 1u) return false;
        return sketch.replaceEntity(sketch.sketch().entities.back().id(), std::move(exact))
               == CadStatus::Ok;
    }
    bool finishOn(const CadSketch& source) {
        if (!begin()) return false;
        for (const SketchEntity& entity : source.entities) {
            SketchTool tool = SketchTool::Line;
            if (entity.rectangle() != nullptr) tool = SketchTool::Rectangle;
            if (entity.circle() != nullptr) tool = SketchTool::Circle;
            if (!place(tool, entity.payload())) return false;
        }
        return sketch.finish() == CadStatus::Ok;
    }
};

CadSketch gridSketch(int cols, int rows) {
    CadSketch s;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = cols;
    r.height = rows;
    add(&s, r);
    const double u0 = -0.5 * cols;
    const double v0 = -0.5 * rows;
    for (int k = 1; k < cols; ++k) add(&s, line(u0 + k, v0, u0 + k, -v0));
    for (int k = 1; k < rows; ++k) add(&s, line(u0, v0 + k, -u0, v0 + k));
    return s;
}

// --- trim100: the drafting grid ------------------------------------------------
CadSketch trimGrid() {
    CadSketch grid;
    for (int i = 0; i < 50; ++i) {
        add(&grid, line(0.0, 0.1 * i, 6.0, 0.1 * i));
        add(&grid, line(0.1 * i + 0.05, -0.5, 0.1 * i + 0.05, 5.5));
    }
    return grid;
}

// --- revolve_square: the canonical revolved square -------------------------------
CadBodyState revolveSquare() {
    CadSketch s;
    SketchRectangle r;
    r.center = SketchPoint{1.5, 0.5};
    r.width = 1.0;
    r.height = 1.0;
    add(&s, r);
    add(&s, line(0.0, -1.0, 0.0, 2.0));
    RevolveFeature rev;
    rev.profileEntityId = 1;
    rev.axis = CadSketchEdgeRef{2, 0};
    rev.angleDegrees = 360.0;
    rev.direction = RevolveDirection::Positive;
    return makeCadRevolveBodyState(s, rev);
}

}  // namespace

int main(int argc, char** argv) {
    const int samples = argc > 1 ? std::atoi(argv[1]) : 30;
    const int warmup = argc > 2 ? std::atoi(argv[2]) : 3;

    Driver d;
    if (!d.finishOn(gridSketch(63, 63)) || d.sketch.planarFaceCount() != 3969u) {
        std::fprintf(stderr, "eval_all setup failed\n");
        return 2;
    }
    for (size_t k = 0; k < 3969u; ++k) {
        if (d.sketch.togglePlanarFace(k) != CadStatus::Ok) return 3;
    }
    const CadSketch grid = trimGrid();
    const CadBodyState square = revolveSquare();

    std::printf("sample\teval_all_us\teval_state_us\teval_regen_us\teval_judge_us\ttrim100_us\trevolve_square_us\n");
    for (int i = -warmup; i < samples; ++i) {
        // A new candidate revision with the SAME selection: off and on again,
        // so evaluateCandidate cannot return its cached answer.
        d.sketch.togglePlanarFace(0);
        d.sketch.togglePlanarFace(0);
        auto t0 = std::chrono::steady_clock::now();
        const CadCandidateEvaluation& e = d.sketch.evaluateCandidate();
        const double evalAll = us(t0);
        if (e.status != CadStatus::Ok || e.mesh == nullptr || e.mesh->components != 1u) return 4;

        t0 = std::chrono::steady_clock::now();
        const CadBodyState candidate = d.sketch.candidateState();
        const double stateUs = us(t0);
        CadBodyMesh mesh;
        t0 = std::chrono::steady_clock::now();
        if (regenerateCadBody(candidate, &mesh) != CadStatus::Ok) return 5;
        const double regenUs = us(t0);
        double judgeUs = 0.0;
#ifdef FS_HAS_R2_JUDGE
        t0 = std::chrono::steady_clock::now();
        if (validateCadStateBeyondRegeneration(candidate) != CadStatus::Ok) return 6;
        judgeUs = us(t0);
#endif
        t0 = std::chrono::steady_clock::now();
        SketchTrimPlan trim;
        planSketchTrim(grid, SketchPoint{2.0, 0.1}, 0.02, &trim);
        const double trimUs = us(t0);

        CadBodyMesh rev;
        t0 = std::chrono::steady_clock::now();
        if (regenerateCadBody(square, &rev) != CadStatus::Ok) return 7;
        const double revUs = us(t0);

        if (i >= 0) {
            std::printf("%d\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\n", i, evalAll, stateUs, regenUs,
                        judgeUs, trimUs, revUs);
        }
    }
    d.sketch.cancel();
    return 0;
}
