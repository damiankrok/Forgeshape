// What the renderer does when the GPU goes away, expressed without Vulkan.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan header, no renderer
// type. That is deliberate and it is the whole point of the file. A device-loss
// policy that lived inside the renderer could only be tested by destroying a
// real device on a real GPU, which is exactly the thing this project must not do
// to its authoritative emulator; expressed here it is an ordinary state machine
// with an ordinary self-test, and the renderer is left holding only the Vulkan
// calls.
//
// GPU resources are NOT project truth
// -----------------------------------
// Nothing here touches, reads or preserves project state, because nothing here
// has to: the scene, every body, every published `RuntimeMesh` and every Frozen
// Sculpt Mesh live in CPU domain code that a lost device cannot reach. Losing
// the device costs the GPU's *copy* of derived data, and the recovery is to
// build that copy again from the CPU truth that never moved. This module only
// decides whether to try, how many times, and when to stop trying and say so.
#pragma once

#include <cstdint>

namespace forgeshape {

// How bad a frame failure was.
//
// The renderer classifies a Vulkan result into one of these and then stops
// thinking about severity; everything downstream reasons about the kind.
enum class RenderFailureKind {
    None,
    // Expected and self-healing: the swapchain no longer matches the window.
    // Not a failure at all in the ordinary case — see the identity-preTransform
    // convention, which makes SUBOPTIMAL the steady state on a rotated display.
    SwapchainOutOfDate,
    // The surface is gone but the device is fine: the window was taken away.
    // Handled by the existing attach/detach ownership, not by this policy.
    SurfaceLost,
    // The device is gone. Every handle created from it is now invalid, and the
    // only lawful thing left to do with them is destroy them.
    DeviceLost,
    // Anything else a frame can return: out of memory, a driver error, a
    // programming mistake. Not retried, because retrying an unknown fault in a
    // loop is how a diagnostic becomes a battery drain.
    Unrecoverable,
};

const char* renderFailureKindName(RenderFailureKind kind);

// Where the renderer stands, as the Java layer is allowed to see it.
//
// Deliberately three states and not a bitfield: the product has exactly one
// question to answer for the user — "is the viewport coming back?" — and three
// answers is what it takes to answer it honestly.
enum class RendererLifecycle {
    // Presenting, or ready to.
    Healthy,
    // The device was lost and a rebuild is in progress or about to be attempted.
    // The project is intact; the viewport is briefly not.
    Recovering,
    // The renderer has stopped and will not start again in this process. The
    // project is still intact and has been checkpointed; what the user needs is
    // a restart, and saying so is better than presenting a black viewport
    // forever or, worse, drawing through a corrupt device.
    RestartRequired,
};

const char* rendererLifecycleName(RendererLifecycle state);

// What the renderer should DO about a failure.
enum class RenderRecoveryAction {
    // Nothing; carry on with the next frame.
    Continue,
    // Rebuild the swapchain and its dependents. The device survives.
    RebuildSwapchain,
    // Destroy every device-scoped object and build them again, then re-upload
    // derived data from CPU truth.
    RebuildDevice,
    // Stop. Tear down what can be torn down, checkpoint the project, and report
    // that a restart is required.
    StopRestartRequired,
};

const char* renderRecoveryActionName(RenderRecoveryAction action);

// How many device rebuilds are attempted before the renderer gives up.
//
// Two, and the number is a judgement rather than a measurement. One is too few:
// a device can be lost once for a reason that has already passed — the driver
// was updated, the GPU was reset under memory pressure — and a single failure
// would strand a user whose next attempt would have worked. Many is worse than
// two: a device that will not come back does not come back on the fifth try
// either, and each attempt costs a full teardown and rebuild of every pipeline
// and buffer. Beyond this the honest answer is that a restart is required.
constexpr int kMaxDeviceRebuildAttempts = 2;

// Stable diagnostic tokens, named here so the renderer, the self-test and the
// diagnostic log cannot spell them differently.
constexpr const char* kRenderDeviceLostToken = "RENDER_DEVICE_LOST";
constexpr const char* kRenderDeviceRebuiltToken = "RENDER_DEVICE_REBUILT";
constexpr const char* kRenderRestartRequiredToken = "RENDER_RESTART_REQUIRED";

// Turns a Vulkan result code into a failure kind.
//
// Takes a plain `int` rather than a `VkResult` so this file needs no Vulkan
// header. The three codes it recognises are named as constants below and are
// `static_assert`ed against the real `VkResult` enumerators inside the renderer,
// so the two can never drift apart while still living in separate worlds.
constexpr int kVkSuccessCode = 0;
constexpr int kVkSuboptimalCode = 1000001003;
constexpr int kVkErrorOutOfDateCode = -1000001004;
constexpr int kVkErrorSurfaceLostCode = -1000000000;
constexpr int kVkErrorDeviceLostCode = -4;

// `suboptimalIsExpected` carries the identity-preTransform convention: on a
// rotated display SUBOPTIMAL is the steady state, and rebuilding the swapchain
// for it would rebuild it every single frame. The renderer already knows this;
// the classification takes it as an argument rather than guessing.
RenderFailureKind classifyRenderResult(int vkResultCode, bool suboptimalIsExpected);

// The device-loss state machine.
//
// Not internally synchronised: it is owned by the renderer and touched only on
// the render thread, exactly like every other renderer member.
class RenderRecoveryPolicy {
public:
    RendererLifecycle state() const { return state_; }

    // How many device rebuilds have been STARTED, for logging and for the
    // bound. Never reset by a successful rebuild: a device that is lost twice
    // in one session has spent two of its attempts, and pretending otherwise
    // would turn the bound into no bound at all whenever a rebuild briefly
    // succeeded.
    int deviceRebuildAttempts() const { return deviceRebuildAttempts_; }

    // How many rebuilds actually completed. Introspection only.
    int deviceRebuildsCompleted() const { return deviceRebuildsCompleted_; }

    // Decides what to do about one frame failure, and moves the state machine.
    //
    // Once the renderer is RestartRequired every later failure is answered with
    // StopRestartRequired and nothing is attempted again: the frame loop has
    // already stopped, and a policy that changed its mind would restart it.
    RenderRecoveryAction onFailure(RenderFailureKind kind);

    // Told after an attempted rebuild finished. `succeeded` returns the
    // renderer to Healthy; a failure spends the remaining attempts immediately,
    // because a rebuild that cannot complete is not going to complete on a
    // retry that changes nothing.
    void onDeviceRebuildFinished(bool succeeded);

    // Forces the terminal state for a reason this policy did not decide — the
    // surface could not be re-attached, say. There is no way back out of it.
    void forceRestartRequired();

private:
    RendererLifecycle state_ = RendererLifecycle::Healthy;
    int deviceRebuildAttempts_ = 0;
    int deviceRebuildsCompleted_ = 0;
};

}  // namespace forgeshape
