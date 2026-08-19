// Debug-only deterministic checks for the Construction domain (the exact box).
//
// No third-party test framework: the checks are plain functions that fill a
// caller-provided array, exactly like the camera, picking and mesh self-tests.
// They run ONCE at startup and never per frame.
#pragma once

namespace forgeshape {

struct ConstructionSelfTestResult {
    const char* name;
    bool passed;
};

// Runs every check, writing up to `max` results. Returns how many ran.
//
// Side effect, deliberately: the picking coherence checks publish revisions into
// the process-wide MeshStore, because "picking sees the generated box" is
// exactly what is being asserted. They never touch the process-wide
// ConstructionBox, and the caller republishes the product box afterwards, so the
// store is left holding the real Construction geometry.
int runConstructionSelfTests(ConstructionSelfTestResult* out, int max);

}  // namespace forgeshape
