// Debug-only self-tests for Body Dimensions, the shared resize/anchor solver
// and Relative Scale (Stage 020M, DIM020M-01..16).
//
// Like every other suite in this product it builds its OWN ConstructionScene
// and its OWN ConstructionHistory, so a result never depends on what a live
// session or an earlier suite left behind.
#pragma once

namespace forgeshape {

struct BodyDimensionsSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runBodyDimensionsSelfTests(BodyDimensionsSelfTestResult* out, int maxOut);

}  // namespace forgeshape
