// DEBUG-only self-tests for the exact Cone and Capsule primitives.
//
// They cover the two new generators' topology, bounds, winding and degeneracy,
// the typed primitive boundary they arrive through, the apply transaction's
// Applied / Unchanged / Rejected semantics including the capsule's own
// diameter/total-height relation, and picking against both shapes with and
// without a transform.
//
// Like every other ForgeShape self-test suite this is deterministic, allocates
// nothing the caller does not own, and is compiled out of a release build by its
// caller.
#pragma once

namespace forgeshape {

struct ConeCapsuleSelfTestResult {
    const char* name;
    bool passed;
};

// Runs every check, writing at most `max` results. Returns how many ran.
int runConeCapsuleSelfTests(ConeCapsuleSelfTestResult* out, int max);

}  // namespace forgeshape
