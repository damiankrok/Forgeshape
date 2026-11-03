// CR-07 (2): why the two FMA failures happen, measured on the product's own
// functions. Built twice (with and without -mfma contraction); compare outputs.
#include <cmath>
#include <cstdio>
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_history.h"
using namespace forgeshape;

// History oracle: the selftest feeds 0.5 + 0.01*i in a loop (i a runtime
// size_t) and compares, with ==, against 0.5 + 0.01*(overflow-capacity-1)
// computed at a different site.
__attribute__((noinline)) double loopValue(size_t i) { return 0.5 + 0.01 * static_cast<double>(i); }

int main() {
    const size_t overflow = kConstructionHistoryCapacity + 8;
    const size_t k = overflow - kConstructionHistoryCapacity - 1;
    const double fed = loopValue(k);
    const double oracle = 0.5 + 0.01 * static_cast<double>(overflow - kConstructionHistoryCapacity - 1);
    std::printf("HISTORY capacity=%zu k=%zu fed=%.17g oracle=%.17g equal=%d ulps=%.0f fma(0.01,k,0.5)=%.17g\n",
                (size_t)kConstructionHistoryCapacity, k, fed, oracle, fed == oracle,
                (fed - oracle) / (std::nextafter(oracle, 1e9) - oracle),
                std::fma(0.01, (double)k, 0.5));

    // Lens oracle: two r=1 circles at (-0.4,0) and (+0.4,0).
    CadSketch sketch;
    SketchCircle ca; ca.center = SketchPoint{-0.4, 0.0}; ca.radius = 1.0;
    SketchCircle cb; cb.center = SketchPoint{0.4, 0.0}; cb.radius = 1.0;
    SketchEntityId a = kNoSketchEntity, b = kNoSketchEntity;
    addSketchEntity(&sketch, ca, &a);
    addSketchEntity(&sketch, cb, &b);
    const SketchRegionExtraction x = extractSketchRegions(sketch);
    for (const ClosedProfile& p : x.loops.profiles) {
        std::printf("LENS loop anchor=%llu area=%.17g\n", (unsigned long long)p.anchorEntityId, (double)p.area);
    }
    SketchEntityId hit = kNoSketchEntity;
    sketchRegionAt(x, SketchPoint{0.0, 0.0}, &hit);
    std::printf("LENS a=%llu b=%llu tap(0,0)->%llu (oracle expects a)\n",
                (unsigned long long)a, (unsigned long long)b, (unsigned long long)hit);
    return 0;
}
