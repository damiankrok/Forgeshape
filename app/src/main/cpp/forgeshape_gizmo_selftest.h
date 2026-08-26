// Debug-only self-tests for the Construction Move / Rotate gizmo (S020-N01..).
//
// Covers the solvers and their degenerate cases, the screen-constant scale, the
// hit-region arithmetic and the transaction boundary around one drag. Every
// numeric check uses a tolerance; none of it looks at a pixel of a screenshot.
#pragma once

namespace forgeshape {

struct GizmoSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runGizmoSelfTests(GizmoSelfTestResult* out, int maxOut);

}  // namespace forgeshape
