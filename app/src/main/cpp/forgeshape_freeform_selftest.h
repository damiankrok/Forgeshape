// Debug-only self-tests for Freeform/SubD (`MODELING-FOUNDATIONS-R1` B): the
// control cage and its rules, Catmull-Clark with boundaries and creases, every
// cage tool, symmetry, the Construction history over a cage, and the `FRFM`
// codec.
//
// Platform-neutral, like every suite: each case builds the cage, the scene and
// the history it needs, so a result never depends on what a live session or an
// earlier suite left behind. What only a device can show -- real touches on the
// cage, the context surface, captured pixels -- lives in the instrumented
// `FreeformOwnerTest`.
#pragma once

namespace forgeshape {

struct FreeformSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runFreeformSelfTests(FreeformSelfTestResult* out, int maxOut);

// The bounded timings the suite measured, each the median of three runs in
// microseconds: subdivision of a 96-face closed cage and a 500-face open cage
// at levels 1..4, and one interactive cage-drag sample (transform + validate +
// subdivide at the cage's level) on each. Printed as
// `FORGESHAPE_FREEFORM_PERFORMANCE` beside the suite's result. Valid after
// runFreeformSelfTests; storage owned by the implementation.
const char* freeformPerformanceReport();

// The SHA-256 of the three `FRFM` corpus documents as this build encodes them
// (`freeform_box_v1`, `freeform_crease_symmetry_v1`, `freeform_bad_topology_v1`),
// space-separated `name=hex` pairs, printed as
// `FORGESHAPE_FREEFORM_GOLDEN_SHA256`. Valid after runFreeformSelfTests.
const char* freeformFixtureDigests();

}  // namespace forgeshape
