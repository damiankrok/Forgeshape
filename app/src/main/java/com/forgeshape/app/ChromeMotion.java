package com.forgeshape.app;

import android.content.Context;
import android.provider.Settings;
import android.view.View;
import android.view.ViewPropertyAnimator;

/**
 * The four rules every chrome transition in ForgeShape follows.
 *
 * <p><b>This is not an animation framework and must not become one.</b> There
 * is no transition type, no registry, no builder, no interpolator catalogue and
 * no way to describe motion as data. What is here is the small set of decisions
 * that were already being made identically in more than one place, and nothing
 * else: the two durations, the reduced-motion question, the cancel-first rule,
 * and one alpha helper. A view that needs a movement of its own writes it
 * itself, against these constants — that is what the Display popover does with
 * its anchor-pivot scale.
 *
 * <p>The rules:
 *
 * <ul>
 *   <li><b>Short.</b> A chrome transition is feedback on an act the user has
 *       already committed to. Past about an eighth of a second it stops reading
 *       as feedback and starts being something waited out.
 *   <li><b>Interruptible.</b> Every transition cancels whatever was running on
 *       that view first. A second tap must reverse a transition, never queue
 *       behind it.
 *   <li><b>Reduced motion is obeyed exactly.</b> A zero animator duration scale
 *       — set by the "Remove animations" accessibility option and by developer
 *       options — means land on the final state now, not run a shortened
 *       animation. {@link #duration} returns 0 for that case and every caller
 *       treats 0 as "no animation at all".
 *   <li><b>Never on the path of a pointer sample.</b> Nothing here touches the
 *       viewport, its size or its surface, and no caller may start a transition
 *       that lays out chrome while a viewport gesture is in flight. Input
 *       responsiveness and viewport stability both outrank motion.
 * </ul>
 */
final class ChromeMotion {

    /**
     * A surface arriving. Short enough to feel immediate rather than animated:
     * a panel that takes longer than this to appear is one the user has already
     * looked away from.
     */
    static final long ENTER_MS = 120L;

    /**
     * A surface leaving, deliberately quicker than it arrived. What is going
     * away has already been decided about, so lingering over it only delays the
     * thing the user asked to see.
     */
    static final long EXIT_MS = 90L;

    private ChromeMotion() {
    }

    /**
     * The platform's animator duration scale.
     *
     * <p>Read per use rather than cached: the setting can change while the
     * application is running, and a cached copy would keep animating for a user
     * who has just turned animation off.
     */
    static float animatorScale(Context context) {
        return Settings.Global.getFloat(context.getContentResolver(),
                Settings.Global.ANIMATOR_DURATION_SCALE, 1.0f);
    }

    /**
     * Whether the system wants animations at all, from a scale value.
     *
     * <p>Pure, so the contract can be argued about without a device — which is
     * the whole reason the scale is passed in rather than read here.
     */
    static boolean animationsEnabled(float animatorScale) {
        // Written as a positive test so a NaN scale (a value no platform should
        // report, but nothing prevents) reduces motion rather than animating.
        return animatorScale > 0.0f;
    }

    /** Whether the system wants animations at all, right now. */
    static boolean animationsEnabled(Context context) {
        return animationsEnabled(animatorScale(context));
    }

    /**
     * How long a transition may take, given the system's scale.
     *
     * <p><b>Zero means do not animate</b> — jump to the final state — rather
     * than "animate instantly". The distinction matters: a zero-duration
     * animator still posts frames and still ends asynchronously, so a caller
     * that ran one would leave the view in a transient state for a frame, which
     * is exactly what a user asking for no motion is trying to avoid.
     *
     * <p>The scale is NOT multiplied in. The platform already applies it to
     * every {@code ViewPropertyAnimator}, so doing it here would apply it
     * twice; all this decides is whether the animation runs at all.
     */
    static long duration(long baseMs, float animatorScale) {
        return animationsEnabled(animatorScale) ? baseMs : 0L;
    }

    /**
     * Starts a transition on a view, cancelling whatever was running on it.
     *
     * <p>The cancel is the point. Without it a rapid toggle queues animations
     * and the view settles on whichever one happened to finish last, which is
     * how a panel ends up half visible with no gesture left to fix it.
     */
    static ViewPropertyAnimator begin(View view) {
        view.animate().cancel();
        return view.animate();
    }

    /**
     * Fades a chrome surface in or out, and lands it on the correct visibility.
     *
     * <p>Alpha only, deliberately. Chrome hide/restore must not move, resize or
     * re-lay-out anything: the Vulkan viewport is full-bleed and already
     * occupies the whole window, so there is no size for this to change — and a
     * transition that did change one would rebuild the swapchain for a question
     * about where buttons are drawn.
     *
     * <p>Appearing is made visible at once and faded up, so it is touchable for
     * the whole transition. Disappearing goes {@code GONE} only at the end, and
     * the alpha is restored there so the next appearance starts from a known
     * state rather than from wherever a cancelled fade stopped.
     *
     * @param durationMs from {@link #duration}; 0 lands on the final state now
     */
    static void fade(final View view, final boolean show, long durationMs) {
        begin(view);
        if (durationMs <= 0L) {
            view.setAlpha(1.0f);
            view.setVisibility(show ? View.VISIBLE : View.GONE);
            return;
        }
        if (show) {
            // A view still fading out is part-way down; starting from wherever
            // it actually is makes a reversal continuous instead of a jump.
            if (view.getVisibility() != View.VISIBLE) {
                view.setAlpha(0.0f);
                view.setVisibility(View.VISIBLE);
            }
            view.animate().alpha(1.0f).setDuration(durationMs).start();
        } else {
            view.animate().alpha(0.0f).setDuration(durationMs)
                    .withEndAction(new Runnable() {
                        @Override
                        public void run() {
                            view.setVisibility(View.GONE);
                            view.setAlpha(1.0f);
                        }
                    }).start();
        }
    }

    /** Puts a view in its resting state with no animation running on it. */
    static void settle(View view, boolean visible) {
        view.animate().cancel();
        view.setAlpha(1.0f);
        view.setTranslationY(0.0f);
        view.setScaleX(1.0f);
        view.setScaleY(1.0f);
        view.setVisibility(visible ? View.VISIBLE : View.GONE);
    }
}
