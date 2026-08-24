#include "forgeshape_selection_pulse.h"

namespace forgeshape {
namespace {

// Smoothstep, used as the decay curve.
//
// Chosen over a linear ramp or an ease-OUT because of what each one looks like
// at the two ends. A linear decay leaves the peak the instant it is reached, so
// the acknowledgement is gone before the finger is; an ease-out leaves even
// faster. Smoothstep holds the peak for a few frames and then settles gently
// into the resting value, which is what makes a 220 ms decay read as one
// deliberate acknowledgement rather than a flicker.
//
// It is monotone on [0, 1] and never leaves [0, 1], so the returned alpha
// cannot overshoot either end — no bounce, which is exactly what this stage was
// told not to add.
float smoothstepUnit(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

}  // namespace

float advanceSelectionPulse(SelectionPulseState& state, bool selectedNow,
                            double deltaSeconds, bool motionEnabled) {
    if (!selectedNow) {
        // Deselection clears the visual state outright. Leaving a half-decayed
        // pulse behind would make a body that is selected AGAIN later start its
        // acknowledgement part-way through.
        state.selected = false;
        state.pulseElapsedSeconds = -1.0;
        return 0.0f;
    }

    if (!state.selected) {
        // The false -> true edge, and the only thing that ever starts a pulse.
        // A body that was already selected does not pulse again, however many
        // times the user taps it: the pulse acknowledges a CHANGE in selection
        // truth, and repeating it for a tap that changed nothing would be noise.
        state.selected = true;
        state.pulseElapsedSeconds = motionEnabled ? 0.0 : -1.0;
    } else if (state.pulseElapsedSeconds >= 0.0) {
        double step = deltaSeconds;
        if (!(step > 0.0)) {
            step = 0.0;  // written inverted so a NaN delta advances nothing
        } else if (step > kMaxSelectionFrameSeconds) {
            step = kMaxSelectionFrameSeconds;
        }
        state.pulseElapsedSeconds += step;
        // Reduced motion can be turned on WHILE a pulse is running; honouring it
        // here means the pulse ends at the next frame rather than finishing.
        if (!motionEnabled || state.pulseElapsedSeconds >= kSelectionPulseSeconds) {
            state.pulseElapsedSeconds = -1.0;
        }
    }

    if (state.pulseElapsedSeconds < 0.0) {
        return kSelectionRestingAlpha;
    }

    const float t = static_cast<float>(state.pulseElapsedSeconds / kSelectionPulseSeconds);
    const float decayed = smoothstepUnit(t);
    return kSelectionPulseAlpha + (kSelectionRestingAlpha - kSelectionPulseAlpha) * decayed;
}

}  // namespace forgeshape
