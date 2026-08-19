// Debug-only deterministic checks for the Construction transform.
//
// Not a test framework and not shipped behaviour: a fixed list of assertions the
// debug build runs once at startup, exactly like the camera, picking, mesh and
// Construction-box suites. No third-party test library is used.
#pragma once

namespace forgeshape {

struct TransformSelfTestResult {
    const char* name = "";
    bool passed = false;
};

// Runs every check, writing at most `max` results. Returns how many ran.
int runTransformSelfTests(TransformSelfTestResult* out, int max);

}  // namespace forgeshape
