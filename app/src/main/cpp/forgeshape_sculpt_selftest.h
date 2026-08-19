// Debug-only deterministic checks for the Freeze-to-Sculpt representation
// contract and for the Grab brush.
//
// Not a test framework and not shipped behaviour: a fixed list of assertions the
// debug build runs once at startup, exactly like the camera, picking, mesh,
// Construction-box, transform, primitive and sphere suites. No third-party test
// library is used.
//
// Every check runs against LOCAL SculptSession / ConstructionObject / MeshStore
// instances, so the suite cannot disturb the process-scoped product state it
// runs beside.
#pragma once

namespace forgeshape {

struct SculptSelfTestResult {
    const char* name = "";
    bool passed = false;
};

// Runs every check, writing at most `max` results. Returns how many ran.
int runSculptSelfTests(SculptSelfTestResult* out, int max);

}  // namespace forgeshape
