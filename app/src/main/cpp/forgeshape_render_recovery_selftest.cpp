#include "forgeshape_render_recovery_selftest.h"

#include "forgeshape_render_recovery.h"

#include <string>

namespace forgeshape {
namespace {

struct Recorder {
    RenderRecoverySelfTestResult* out;
    int max;
    int n = 0;

    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

}  // namespace

int runRenderRecoverySelfTests(RenderRecoverySelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    // -----------------------------------------------------------------------
    // Classification
    // -----------------------------------------------------------------------
    //
    // The Vulkan codes are asserted against the real enumerators inside the
    // renderer, where both worlds are visible. What is checked here is the
    // MEANING: which results are routine, which are recoverable, and which end
    // the session.
    {
        r.check("FSR1B_16_success_is_not_a_failure",
                classifyRenderResult(kVkSuccessCode, false) == RenderFailureKind::None);

        // The identity-preTransform convention makes SUBOPTIMAL the steady state
        // on a rotated display. Treating it as a fault would rebuild the
        // swapchain every single frame for as long as the device is turned —
        // which is the exact defect the convention was written to prevent.
        r.check("FSR1B_16_expected_suboptimal_is_not_a_failure",
                classifyRenderResult(kVkSuboptimalCode, true) == RenderFailureKind::None);
        r.check("FSR1B_16_unexpected_suboptimal_rebuilds_the_swapchain",
                classifyRenderResult(kVkSuboptimalCode, false)
                        == RenderFailureKind::SwapchainOutOfDate);

        r.check("FSR1B_16_out_of_date_rebuilds_the_swapchain",
                classifyRenderResult(kVkErrorOutOfDateCode, false)
                        == RenderFailureKind::SwapchainOutOfDate);
        r.check("FSR1B_16_out_of_date_rebuilds_even_when_suboptimal_is_expected",
                classifyRenderResult(kVkErrorOutOfDateCode, true)
                        == RenderFailureKind::SwapchainOutOfDate);
        r.check("FSR1B_16_surface_lost_is_its_own_kind",
                classifyRenderResult(kVkErrorSurfaceLostCode, false)
                        == RenderFailureKind::SurfaceLost);
        r.check("FSR1B_16_device_lost_is_its_own_kind",
                classifyRenderResult(kVkErrorDeviceLostCode, false)
                        == RenderFailureKind::DeviceLost);
        // Anything unrecognised is unrecoverable rather than optimistically
        // retried: retrying an unknown fault in a loop is how a diagnostic
        // becomes a battery drain.
        r.check("FSR1B_16_an_unknown_code_is_unrecoverable",
                classifyRenderResult(-9999, false) == RenderFailureKind::Unrecoverable);
    }

    // -----------------------------------------------------------------------
    // The routine paths never touch the lifecycle
    // -----------------------------------------------------------------------
    {
        RenderRecoveryPolicy policy;
        r.check("FSR1B_16_a_new_policy_is_healthy",
                policy.state() == RendererLifecycle::Healthy
                        && policy.deviceRebuildAttempts() == 0);

        r.check("FSR1B_16_no_failure_continues",
                policy.onFailure(RenderFailureKind::None) == RenderRecoveryAction::Continue);
        r.check("FSR1B_16_an_out_of_date_swapchain_is_not_a_lifecycle_event",
                policy.onFailure(RenderFailureKind::SwapchainOutOfDate)
                                == RenderRecoveryAction::RebuildSwapchain
                        && policy.state() == RendererLifecycle::Healthy);
        // A lost SURFACE is the window going away, which attach/detach already
        // owns. The device is untouched, so the renderer is still healthy.
        r.check("FSR1B_16_a_lost_surface_is_owned_by_attach_detach",
                policy.onFailure(RenderFailureKind::SurfaceLost)
                                == RenderRecoveryAction::Continue
                        && policy.state() == RendererLifecycle::Healthy
                        && policy.deviceRebuildAttempts() == 0);
    }

    // -----------------------------------------------------------------------
    // Device loss: rebuild, then rebuild once more, then stop
    // -----------------------------------------------------------------------
    {
        RenderRecoveryPolicy policy;
        r.check("FSR1B_16_the_first_device_loss_rebuilds",
                policy.onFailure(RenderFailureKind::DeviceLost)
                                == RenderRecoveryAction::RebuildDevice
                        && policy.state() == RendererLifecycle::Recovering
                        && policy.deviceRebuildAttempts() == 1);

        policy.onDeviceRebuildFinished(true);
        r.check("FSR1B_16_a_successful_rebuild_returns_to_healthy",
                policy.state() == RendererLifecycle::Healthy
                        && policy.deviceRebuildsCompleted() == 1);

        // The attempt count is NOT reset by success. A device lost twice in one
        // session has spent two attempts; resetting would make the bound no
        // bound at all whenever a rebuild briefly worked.
        r.check("FSR1B_16_a_second_device_loss_still_rebuilds",
                policy.onFailure(RenderFailureKind::DeviceLost)
                                == RenderRecoveryAction::RebuildDevice
                        && policy.deviceRebuildAttempts() == kMaxDeviceRebuildAttempts);
        policy.onDeviceRebuildFinished(true);

        r.check("FSR1B_16_the_third_device_loss_requires_a_restart",
                policy.onFailure(RenderFailureKind::DeviceLost)
                                == RenderRecoveryAction::StopRestartRequired
                        && policy.state() == RendererLifecycle::RestartRequired);
    }

    // A rebuild that fails spends the remaining attempts at once, rather than
    // letting a doomed device cost several full teardown-and-rebuild cycles
    // before the product admits it.
    {
        RenderRecoveryPolicy policy;
        policy.onFailure(RenderFailureKind::DeviceLost);
        policy.onDeviceRebuildFinished(false);
        r.check("FSR1B_16_a_failed_rebuild_requires_a_restart_immediately",
                policy.state() == RendererLifecycle::RestartRequired
                        && policy.deviceRebuildAttempts() == kMaxDeviceRebuildAttempts
                        && policy.deviceRebuildsCompleted() == 0);
    }

    // -----------------------------------------------------------------------
    // Terminal means terminal
    // -----------------------------------------------------------------------
    {
        RenderRecoveryPolicy policy;
        r.check("FSR1B_16_an_unrecoverable_failure_stops_at_once",
                policy.onFailure(RenderFailureKind::Unrecoverable)
                                == RenderRecoveryAction::StopRestartRequired
                        && policy.state() == RendererLifecycle::RestartRequired);

        // Once the frame loop has stopped, nothing may restart it. A policy that
        // answered Continue or RebuildSwapchain here would do exactly that on
        // the next stray call.
        r.check("FSR1B_16_nothing_restarts_a_stopped_renderer",
                policy.onFailure(RenderFailureKind::None)
                                == RenderRecoveryAction::StopRestartRequired
                        && policy.onFailure(RenderFailureKind::SwapchainOutOfDate)
                                == RenderRecoveryAction::StopRestartRequired
                        && policy.onFailure(RenderFailureKind::DeviceLost)
                                == RenderRecoveryAction::StopRestartRequired);
        policy.onDeviceRebuildFinished(true);
        r.check("FSR1B_16_a_late_rebuild_report_cannot_revive_a_stopped_renderer",
                policy.state() == RendererLifecycle::RestartRequired);
    }

    {
        RenderRecoveryPolicy policy;
        policy.forceRestartRequired();
        r.check("FSR1B_16_a_forced_stop_is_terminal_and_spends_every_attempt",
                policy.state() == RendererLifecycle::RestartRequired
                        && policy.deviceRebuildAttempts() == kMaxDeviceRebuildAttempts);
    }

    // The names are part of the diagnostic contract: a report that said
    // "unknown" would be a report nobody could act on.
    {
        r.check("FSR1B_16_every_state_has_a_name",
                std::string(rendererLifecycleName(RendererLifecycle::Healthy)) == "Healthy"
                        && std::string(rendererLifecycleName(RendererLifecycle::Recovering))
                                == "Recovering"
                        && std::string(rendererLifecycleName(
                                   RendererLifecycle::RestartRequired))
                                == "RestartRequired");
        r.check("FSR1B_16_every_failure_kind_has_a_name",
                std::string(renderFailureKindName(RenderFailureKind::DeviceLost)) == "DeviceLost"
                        && std::string(renderFailureKindName(RenderFailureKind::Unrecoverable))
                                == "Unrecoverable");
        r.check("FSR1B_16_every_action_has_a_name",
                std::string(renderRecoveryActionName(RenderRecoveryAction::RebuildDevice))
                                == "RebuildDevice"
                        && std::string(renderRecoveryActionName(
                                   RenderRecoveryAction::StopRestartRequired))
                                == "StopRestartRequired");
    }

    return r.n;
}

}  // namespace forgeshape
