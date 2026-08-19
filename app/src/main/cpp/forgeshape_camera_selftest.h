// Deterministic self-checks for the ForgeShape camera controller.
//
// Debug-only. Runs once when the native viewport starts, never per frame.
// Reports results as plain data so this file stays free of Android/JNI types;
// the JNI layer is responsible for logging them.
#pragma once

namespace forgeshape {

struct CameraSelfTestResult {
    const char* name;
    bool passed;
};

// Fills `out` with at most `maxOut` results and returns how many were written.
int runCameraSelfTests(CameraSelfTestResult* out, int maxOut);

}  // namespace forgeshape
