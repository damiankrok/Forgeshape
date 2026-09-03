// Debug-only self-tests for CAD-A3 + APP-H1 (`CADA3-*`, the native half).
//
// Platform-neutral. Each case builds its own scenes, histories, camera and
// documents, so a result never depends on process-scoped state. The device
// half -- Home, spatial plane picking, face picking, real MotionEvents -- is
// the instrumented `SpatialSketchTest`.
#pragma once

namespace forgeshape {

struct CadA3SelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runCadA3SelfTests(CadA3SelfTestResult* out, int maxOut);

// The bounded dependency-resolve and regenerate timings, formatted for a log
// line (chain lengths 1, 8, 32). Valid after runCadA3SelfTests.
const char* cadA3PerformanceReport();

}  // namespace forgeshape
