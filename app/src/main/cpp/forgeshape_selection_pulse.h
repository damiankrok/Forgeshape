// ForgeShape selection feedback — how a SELECTED body is drawn, never which
// body is selected.
//
// Platform-independent: no JNI, no Android, no Vulkan and no UI type appears
// here, and nothing in this file is truth about anything. It holds no ObjectId,
// mints no MeshRevision, moves no vertex, rebuilds no render mesh and cannot be
// observed by picking. Everything it computes is one float that the fragment
// stage mixes a tint at.
//
// The state machine is deliberately expressed as a pure function over an
// explicit elapsed time rather than reading a clock itself. That is what lets
// the self-tests drive a whole pulse deterministically in a few microseconds
// instead of sleeping through it, and it keeps the only clock in the renderer,
// where the frame loop already is.
#pragma once

namespace forgeshape {

// What a body's selection tint settles at while it stays selected: NOTHING.
//
// A Construction Body is selected for the entire time the user is editing it,
// so this is the state the object is normally WORKED in, and any tint at all
// repaints the form the user is judging. UI-R1C1 cut the legacy 0.55 flood to a
// 0.20 resting tint, which was the right direction and still the wrong shape:
// a whole-object wash is a whole-object wash at any strength, and at 0.20 it
// still cost measurable face-to-face contrast on the light grounds.
//
// `SEL-OUT-R1` (UI-OWNER-10) finishes the job. Persistent selection is now the
// Objects capsule plus a true silhouette OUTLINE derived from the body's own
// rendered geometry (forgeshape_selection_outline.h), so the tint has no
// persistent job left and settles at zero. The acknowledgement pulse below is
// unchanged and is the only thing that ever mixes this hue into a surface.
//
// Zero rather than "very small": a tint that had to be kept because nothing
// else said what was selected is exactly the state the outline replaced, and
// leaving a residue would mean the product had two persistent selection
// languages instead of one.
constexpr float kSelectionRestingAlpha = 0.0f;

// The top of the acknowledgement pulse: what the tint reaches the instant a
// body BECOMES selected, before decaying to the resting value. Deliberately the
// legacy alpha, so the moment of selection is exactly as unmistakable as the
// old permanent state was — it just does not stay there.
constexpr float kSelectionPulseAlpha = 0.55f;

// How long the acknowledgement takes to decay. Short enough to read as feedback
// on the tap rather than as an animation the user waits out.
constexpr double kSelectionPulseSeconds = 0.22;

// A frame delta larger than this is not a frame, it is a resume, a stall or a
// debugger. Clamped rather than trusted so a paused process does not come back
// and skip the pulse it was in the middle of.
constexpr double kMaxSelectionFrameSeconds = 0.10;

// One body's selection PRESENTATION state, owned by whoever draws that body.
//
// `selected` is a copy of what the scene snapshot said last frame and exists
// only to detect the false->true edge; it is never read as truth about
// selection. `pulseElapsedSeconds` is negative when no pulse is running.
struct SelectionPulseState {
    bool selected = false;
    double pulseElapsedSeconds = -1.0;
};

// Advances one body's selection presentation by one frame and returns the alpha
// its tint should be mixed at.
//
// Contract:
//   * not selected                  -> 0, and any running pulse is cleared;
//   * not selected -> selected      -> a pulse starts at kSelectionPulseAlpha;
//   * during a pulse                -> a monotone, overshoot-free decay toward
//                                      kSelectionRestingAlpha;
//   * selected, pulse finished      -> kSelectionRestingAlpha;
//   * motionEnabled == false        -> kSelectionRestingAlpha immediately, and
//                                      no pulse ever runs.
//
// The reduced-motion branch lands on the RESTING value rather than on the peak:
// a user who has asked for no animation must still be able to see what is
// selected without waiting for anything, and a permanent peak would simply
// restore the state this whole change exists to remove. Since `SEL-OUT-R1` the
// resting value is zero, so reduced motion means the body is never tinted at
// all — which is the correct answer precisely BECAUSE the outline is not an
// animation: it appears on the frame selection changes and stays, so a user who
// asked for no motion still gets an immediate, permanent answer to "which one".
float advanceSelectionPulse(SelectionPulseState& state, bool selectedNow,
                            double deltaSeconds, bool motionEnabled);

}  // namespace forgeshape
