package com.forgeshape.app;

import android.content.Context;
import android.provider.Settings;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.view.animation.Interpolator;
import android.view.animation.PathInterpolator;

/**
 * The rules every chrome transition in ForgeShape follows, and the one growth
 * every anchored surface performs.
 *
 * <p><b>This is not an animation framework and must not become one.</b> There
 * is no transition type, no registry, no builder, no interpolator catalogue and
 * no way to describe motion as data. What is here is the set of decisions that
 * were already being made in more than one place, and nothing else: the
 * durations, the reduced-motion question, the cancel-first rule, one alpha
 * helper, and the anchored-surface growth.
 *
 * <p><b>The anchored growth was four copies before UI-R4B</b> — the Objects
 * panel, the Add Primitive palette, the precision surface and the Display
 * popover each carried private {@code OPEN_DURATION_MS} / {@code
 * CLOSE_DURATION_MS} constants for the same motion, three of them had drifted
 * to different numbers from {@link #ENTER_MS} / {@link #EXIT_MS}, none of them
 * set an interpolator at all (so the platform's default ease-in-out ran, which
 * starts a surface slowly at the moment the user is waiting for it), and two
 * scaled X and Y by different amounts, which stretches a panel rather than
 * growing it. One family of motion now has one owner. See
 * {@link AnchoredSurfaceView}, which is where a surface takes it.
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

    /**
     * A surface that GROWS OUT OF a control arriving.
     *
     * <p>Longer than {@link #ENTER_MS}, and deliberately. A fade is over as soon
     * as it is legible; a growth has to be seen travelling from the control that
     * opened it, because that travel is the entire message — this panel belongs
     * to that button. At 140 ms the three anchored surfaces read as appearing
     * near their invoker rather than out of it; below about 180 ms the eye
     * cannot resolve the origin at all.
     */
    static final long ANCHORED_ENTER_MS = 190L;

    /**
     * The same surface leaving, quicker than it arrived.
     *
     * <p>Dismissal has already been decided about, and the ratio matters more
     * than either number: an exit that takes as long as the entrance reads as
     * the surface being reluctant.
     */
    static final long ANCHORED_EXIT_MS = 150L;

    /**
     * How small an anchored surface starts, applied to BOTH axes.
     *
     * <p>Uniform, which the copies this replaced were not: scaling X by 0.98 and
     * Y by 0.96 stretches a panel into place instead of growing it, and the
     * distortion is most visible on the widest surface, which is the palette.
     * 0.96 is enough travel to be seen at 190 ms and small enough that no text
     * inside is legibly the wrong size on the way.
     */
    static final float ANCHORED_START_SCALE = 0.96f;

    /**
     * The one easing curve in the product, as its four control points.
     *
     * <p>{@code cubic-bezier(0.23, 1, 0.32, 1)} — an EASE-OUT: nearly all of the
     * travel happens in the first third, and the rest is the surface settling.
     * Every transition here answers a tap the user has already made, so the
     * surface has to leave the control immediately and spend its remaining time
     * arriving. The platform's default is an ease-in-out, which does the
     * opposite — it starts slowly, at exactly the moment the user is waiting —
     * and it was what ran, because not one of the four surfaces set an
     * interpolator at all.
     *
     * <p>Named as numbers rather than only as a built object so the SHAPE can be
     * argued about without a device: the first control point's y at 1.0 with its
     * x at 0.23 is what makes it an ease-out, and a JVM case can hold that
     * without loading an Android class.
     */
    static final float ANCHORED_EASE_X1 = 0.23f;
    static final float ANCHORED_EASE_Y1 = 1.0f;
    static final float ANCHORED_EASE_X2 = 0.32f;
    static final float ANCHORED_EASE_Y2 = 1.0f;

    /**
     * The built curve, created on first use.
     *
     * <p>Lazily rather than in a static initializer, and the reason is a real
     * one: {@code PathInterpolator} is an Android class, and the JVM test source
     * set runs against the stub android.jar. A static field would make merely
     * LOADING this class throw on the JVM, which would take every off-device
     * case about the durations and the reduced-motion rule down with it.
     *
     * <p>Unsynchronized, because every caller is on the main thread — a chrome
     * transition is started from a click listener or from a layout pass and
     * nowhere else. Worst case two threads would build two identical stateless
     * interpolators.
     */
    private static Interpolator anchoredEase;

    private ChromeMotion() {
    }

    /** The one easing curve, for the surfaces that grow out of a control. */
    static Interpolator anchoredEase() {
        if (anchoredEase == null) {
            anchoredEase = new PathInterpolator(ANCHORED_EASE_X1, ANCHORED_EASE_Y1,
                    ANCHORED_EASE_X2, ANCHORED_EASE_Y2);
        }
        return anchoredEase;
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
