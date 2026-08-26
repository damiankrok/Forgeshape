// Debug-only self-tests for the Construction transaction boundary and the
// Undo/Redo history (S019-N01..).
#pragma once

namespace forgeshape {

struct HistorySelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runHistorySelfTests(HistorySelfTestResult* out, int maxOut);

}  // namespace forgeshape
