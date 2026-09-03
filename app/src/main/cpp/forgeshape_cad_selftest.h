// Debug-only self-tests for the CAD domain (`CADR0-01..40`, the native half).
//
// Platform-neutral, exactly like every other suite: each case builds the
// sketch, the scene, the history and the camera it needs, so a result never
// depends on what a live session or an earlier suite left behind. What can
// only be true on a device -- rows, controls, the gizmo on a CAD body, real
// MotionEvents -- lives in the instrumented `SketchExtrudeTest`.
#pragma once

namespace forgeshape {

struct CadSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runCadSelfTests(CadSelfTestResult* out, int maxOut);

// The bounded performance measurements the suite took, formatted for a log
// line: profile extraction, triangulation and regeneration for a rectangle,
// a circle, a 32-edge and a 128-edge polyline, in microseconds. Valid after
// runCadSelfTests; storage owned by the implementation.
const char* cadPerformanceReport();

}  // namespace forgeshape
