// Debug-only deterministic checks for the exact Construction sphere and for the
// typed primitive parameter boundary.
//
// Not a test framework and not shipped behaviour: a fixed list of assertions the
// debug build runs once at startup, exactly like the camera, picking, mesh,
// Construction-box, transform and primitive suites. No third-party test library
// is used.
#pragma once

namespace forgeshape {

struct SphereSelfTestResult {
    const char* name = "";
    bool passed = false;
};

// Runs every check, writing at most `max` results. Returns how many ran.
int runSphereSelfTests(SphereSelfTestResult* out, int max);

}  // namespace forgeshape
