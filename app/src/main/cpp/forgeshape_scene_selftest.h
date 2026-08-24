// Debug-only self-tests for the multi-object scene (S17-01..20).
#pragma once

namespace forgeshape {

struct SceneSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runSceneSelfTests(SceneSelfTestResult* out, int maxOut);

}  // namespace forgeshape
