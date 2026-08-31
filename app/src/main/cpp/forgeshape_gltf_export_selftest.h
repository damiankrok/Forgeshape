// Debug-only self-tests for the early GLB export (FSR1C-01..12).
//
// Every case reads the exported bytes back with its OWN little-endian decoder
// rather than with the writer's helpers. Validating a file through the code that
// wrote it proves the two halves agree with each other and nothing about whether
// either agrees with glTF, so the envelope, the offsets and the geometry are all
// decoded here from first principles.
//
// The fuller independent check lives in the instrumentation, where a real JSON
// parser in another language reads the same bytes.
#pragma once

namespace forgeshape {

struct GltfExportSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runGltfExportSelfTests(GltfExportSelfTestResult* out, int maxOut);

}  // namespace forgeshape
