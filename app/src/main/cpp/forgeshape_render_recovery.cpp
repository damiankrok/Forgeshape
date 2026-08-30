#include "forgeshape_render_recovery.h"

namespace forgeshape {

const char* renderFailureKindName(RenderFailureKind kind) {
    switch (kind) {
        case RenderFailureKind::None: return "None";
        case RenderFailureKind::SwapchainOutOfDate: return "SwapchainOutOfDate";
        case RenderFailureKind::SurfaceLost: return "SurfaceLost";
        case RenderFailureKind::DeviceLost: return "DeviceLost";
        case RenderFailureKind::Unrecoverable: return "Unrecoverable";
    }
    return "unknown";
}

const char* rendererLifecycleName(RendererLifecycle state) {
    switch (state) {
        case RendererLifecycle::Healthy: return "Healthy";
        case RendererLifecycle::Recovering: return "Recovering";
        case RendererLifecycle::RestartRequired: return "RestartRequired";
    }
    return "unknown";
}

const char* renderRecoveryActionName(RenderRecoveryAction action) {
    switch (action) {
        case RenderRecoveryAction::Continue: return "Continue";
        case RenderRecoveryAction::RebuildSwapchain: return "RebuildSwapchain";
        case RenderRecoveryAction::RebuildDevice: return "RebuildDevice";
        case RenderRecoveryAction::StopRestartRequired: return "StopRestartRequired";
    }
    return "unknown";
}

RenderFailureKind classifyRenderResult(int vkResultCode, bool suboptimalIsExpected) {
    if (vkResultCode == kVkSuccessCode) {
        return RenderFailureKind::None;
    }
    if (vkResultCode == kVkSuboptimalCode) {
        // The convention makes this the STEADY STATE on a rotated display, not
        // a fault. Rebuilding for it would rebuild every frame for as long as
        // the device is turned.
        return suboptimalIsExpected ? RenderFailureKind::None
                                    : RenderFailureKind::SwapchainOutOfDate;
    }
    if (vkResultCode == kVkErrorOutOfDateCode) {
        return RenderFailureKind::SwapchainOutOfDate;
    }
    if (vkResultCode == kVkErrorSurfaceLostCode) {
        return RenderFailureKind::SurfaceLost;
    }
    if (vkResultCode == kVkErrorDeviceLostCode) {
        return RenderFailureKind::DeviceLost;
    }
    return RenderFailureKind::Unrecoverable;
}

RenderRecoveryAction RenderRecoveryPolicy::onFailure(RenderFailureKind kind) {
    // Terminal means terminal. The frame loop has stopped; a policy that
    // answered anything else here would start it again on the next stray call.
    if (state_ == RendererLifecycle::RestartRequired) {
        return RenderRecoveryAction::StopRestartRequired;
    }

    switch (kind) {
        case RenderFailureKind::None:
            return RenderRecoveryAction::Continue;

        case RenderFailureKind::SwapchainOutOfDate:
            // The device is fine, so this is not a lifecycle event at all: the
            // existing rebuild path already handles it and the renderer stays
            // Healthy throughout.
            return RenderRecoveryAction::RebuildSwapchain;

        case RenderFailureKind::SurfaceLost:
            // Owned by attach/detach, not here. The window went away; the
            // Android layer will hand a new one over, and the device is
            // untouched.
            return RenderRecoveryAction::Continue;

        case RenderFailureKind::DeviceLost:
            if (deviceRebuildAttempts_ >= kMaxDeviceRebuildAttempts) {
                state_ = RendererLifecycle::RestartRequired;
                return RenderRecoveryAction::StopRestartRequired;
            }
            ++deviceRebuildAttempts_;
            state_ = RendererLifecycle::Recovering;
            return RenderRecoveryAction::RebuildDevice;

        case RenderFailureKind::Unrecoverable:
            state_ = RendererLifecycle::RestartRequired;
            return RenderRecoveryAction::StopRestartRequired;
    }
    state_ = RendererLifecycle::RestartRequired;
    return RenderRecoveryAction::StopRestartRequired;
}

void RenderRecoveryPolicy::onDeviceRebuildFinished(bool succeeded) {
    if (state_ == RendererLifecycle::RestartRequired) {
        return;
    }
    if (succeeded) {
        ++deviceRebuildsCompleted_;
        state_ = RendererLifecycle::Healthy;
        return;
    }
    // A rebuild that could not complete has told us the answer already. Spending
    // the remaining attempts here rather than letting the next lost frame spend
    // them one at a time is what stops a doomed device from costing several full
    // teardown-and-rebuild cycles before the product admits it.
    deviceRebuildAttempts_ = kMaxDeviceRebuildAttempts;
    state_ = RendererLifecycle::RestartRequired;
}

void RenderRecoveryPolicy::forceRestartRequired() {
    state_ = RendererLifecycle::RestartRequired;
    deviceRebuildAttempts_ = kMaxDeviceRebuildAttempts;
}

}  // namespace forgeshape
