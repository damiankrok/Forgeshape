// Debug-only self-tests for Construction Mirror (`MIRROR-01`,
// MIRROR01-01..12).
//
// Like every other suite in this product it builds its OWN ConstructionScene
// and its OWN ConstructionHistory, so a result never depends on what a live
// session or an earlier suite left behind.
#pragma once

namespace forgeshape {

struct MirrorSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runMirrorSelfTests(MirrorSelfTestResult* out, int maxOut);

}  // namespace forgeshape
