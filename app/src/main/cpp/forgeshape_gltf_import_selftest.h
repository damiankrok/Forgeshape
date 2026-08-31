// GLB-IMPORT-R0 self-tests: the independent parser, and the roundtrip.
//
// Debug-only, run once from NativeViewport.start(), never per frame. Each case
// builds the domain objects it needs, so no result depends on what a live
// session left behind.
#pragma once

namespace forgeshape {

struct GltfImportSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runGltfImportSelfTests(GltfImportSelfTestResult* out, int maxOut);

}  // namespace forgeshape
