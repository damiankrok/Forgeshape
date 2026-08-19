// Debug-only deterministic checks for the runtime mesh layer.
//
// No third-party test framework: the checks are plain functions that fill a
// caller-provided array, exactly like the camera and picking self-tests. They
// run ONCE at startup and never per frame.
#pragma once

namespace forgeshape {

struct MeshSelfTestResult {
    const char* name;
    bool passed;
};

// Runs every check, writing up to `max` results. Returns how many ran.
//
// Side effect, deliberately: the coherence checks publish revisions into the
// process-wide MeshStore, because that is the object whose behaviour is being
// asserted. The caller publishes the baseline fixture afterwards, so the store
// is left holding a known-good current revision.
int runMeshSelfTests(MeshSelfTestResult* out, int max);

}  // namespace forgeshape
