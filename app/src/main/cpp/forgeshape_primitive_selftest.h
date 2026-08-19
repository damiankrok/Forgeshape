// Debug-only deterministic checks for the active Construction object and the
// exact cylinder primitive.
//
// Not a test framework and not shipped behaviour: a fixed list of assertions the
// debug build runs once at startup, exactly like the camera, picking, mesh,
// Construction-box and transform suites. No third-party test library is used.
#pragma once

namespace forgeshape {

struct PrimitiveSelfTestResult {
    const char* name = "";
    bool passed = false;
};

// Runs every check, writing at most `max` results. Returns how many ran.
int runPrimitiveSelfTests(PrimitiveSelfTestResult* out, int max);

}  // namespace forgeshape
