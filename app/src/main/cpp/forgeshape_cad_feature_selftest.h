// Debug-only self-tests for the CAD vertical slice (`CAD-VERTICAL-SLICE-R1`):
// the boolean-kernel gate, sketch regions with holes, the retained feature
// chain and its New Body / Add / Cut operations, and the `CADB` v5 codec.
//
// Platform-neutral, like every suite: each case builds the sketch, the body,
// the scene and the history it needs, so a result never depends on what a live
// session or an earlier suite left behind. What only a device can show -- the
// canvas HUD, real MotionEvents, captured pixels -- lives in the instrumented
// `CadVerticalSliceTest`.
#pragma once

namespace forgeshape {

struct CadFeatureSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runCadFeatureSelfTests(CadFeatureSelfTestResult* out, int maxOut);

// The bounded timings the suite took for the owner's acceptance model (a
// rectangle-with-hole base, one Add, one Cut), each the median of five runs in
// microseconds: the base regeneration, the base+Add chain, the base+Add+Cut
// chain and a v5 encode+decode round trip, plus the full chain's triangle
// count -- `region_base_us=.. add_us=.. add_cut_us=.. codec_roundtrip_us=..
// triangles=..`. Printed as `FORGESHAPE_CAD_FEATURE_PERFORMANCE` beside the
// suite's own result. Valid after runCadFeatureSelfTests; storage owned by the
// implementation.
const char* cadFeaturePerformanceReport();

// The SHA-256 of every `CADB` v5 document the suite encodes, as this build
// encodes it, space-separated `name=hex` pairs: the four valid fixtures
// (`region_hole`, `feature_add`, `feature_cut`, `feature_chain`) and then the
// byte-patched corrupt ones. Printed so drift from the independently built
// PowerShell corpus is a value that can be read rather than only an assertion
// that failed. Valid after runCadFeatureSelfTests; storage owned by the
// implementation.
const char* cadFeatureFixtureDigests();

}  // namespace forgeshape
