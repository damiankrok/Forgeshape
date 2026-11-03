// Debug-only self-tests for Surface (`MODELING-FOUNDATIONS-R1` C): planar,
// extruded, revolved and lofted patches, the exact planar Trim, Stitch with its
// named refusals, Thicken where the offset is defined, the staged first-failure
// regeneration, the Construction history over a feature list, and the `SURF`
// codec.
//
// Platform-neutral, like every suite: each case builds the sketches, the body,
// the scene and the history it needs. What only a device can show lives in the
// instrumented `SurfaceOwnerTest`.
#pragma once

namespace forgeshape {

struct SurfaceSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runSurfaceSelfTests(SurfaceSelfTestResult* out, int maxOut);

// Bounded timings (microseconds, one run each): a representative planar patch,
// a loft, a stitch and a thicken. Printed as `FORGESHAPE_SURFACE_PERFORMANCE`.
const char* surfacePerformanceReport();

// The SHA-256 of the three `SURF` corpus documents as this build encodes them,
// printed as `FORGESHAPE_SURFACE_GOLDEN_SHA256`.
const char* surfaceFixtureDigests();

}  // namespace forgeshape
