// Debug-only self-tests for SKETCH-UX-R1 (`CADUXR1-*`, the native half).
//
// The curve domain (Arc and Spline), the exact line-length edit, the
// orientation navigator's presentation state, support-plane switching and the
// staged Edit Sketch session, plus the `CADB` v3 round trip that carries a
// curve.
//
// Platform-neutral, like every other suite: each case builds its own sketch,
// scene, history and document, so a result never depends on what a live
// session or an earlier suite left behind. What can only be true on a device --
// the full-screen Home, the navigator's controls, the dimension field, real
// MotionEvents -- is the instrumented `SketchUxTest`.
#pragma once

namespace forgeshape {

struct SketchUxSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runSketchUxSelfTests(SketchUxSelfTestResult* out, int maxOut);

// The bounded curve timings, formatted for a log line: arc and spline
// tessellation, and profile extraction over a curve chain. Valid after
// runSketchUxSelfTests; storage owned by the implementation.
const char* sketchUxPerformanceReport();

}  // namespace forgeshape
