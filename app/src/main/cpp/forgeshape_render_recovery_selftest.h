// Debug-only self-tests for the GPU device-loss policy (FSR1B-16).
//
// The policy is deliberately free of Vulkan, so these cases need no device, no
// driver and no GPU: they are the reason a device-loss contract can be verified
// without destabilising the authoritative emulator's GPU, which the repository
// forbids. What the policy DOES on a real lost device is proved separately by
// the debug injection seam and the instrumented case that drives it.
#pragma once

namespace forgeshape {

struct RenderRecoverySelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runRenderRecoverySelfTests(RenderRecoverySelfTestResult* out, int maxOut);

}  // namespace forgeshape
