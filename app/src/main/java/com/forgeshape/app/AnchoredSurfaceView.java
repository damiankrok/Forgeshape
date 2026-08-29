package com.forgeshape.app;

import android.content.Context;
import android.widget.LinearLayout;

/**
 * A context surface that <b>grows out of the control that opened it</b>.
 *
 * <p>Four surfaces in the Editor Workspace are this: the Objects panel, the Add
 * Primitive palette, the precision surface and the Display popover. They are
 * different content in different places, but they are one <i>motion</i> — a
 * panel leaving a control — and before UI-R4B each of them owned a private copy
 * of it. The copies had drifted: three different pairs of duration constants for
 * one family, no interpolator anywhere (so the platform's ease-in-out ran, which
 * is the wrong curve for a response to a tap), non-uniform X/Y scale in two of
 * them, and — the visible one — a first open that grew from the wrong corner.
 *
 * <p><b>The first-open defect, and why it needed a class rather than a fix.</b>
 * The pivot is the corner the surface grows from, and it is the invoker's
 * corner: the surface's own bottom-left when it unfolds upward, its top-left
 * when it unfolds down, its top-right for the popover under the toolbar. All
 * three are expressed in the surface's own size — and on the first open the
 * surface has never been laid out, so {@code getHeight()} is 0 and the pivot
 * lands at the top-left of a zero-sized box. The very first time a user opened
 * the scene list, the palette or the exact values, it grew from nowhere in
 * particular; every later open was correct, which is exactly why it survived
 * review. The Display popover had already been patched with an
 * {@code onSizeChanged} pivot; the other three had not, and a patch per surface
 * is how the next one is forgotten.
 *
 * <p>Here the growth simply <b>waits for a size</b>. An open with no size yet
 * puts the surface in its start state and makes it visible so the traversal
 * measures it, and the animation starts from {@link #onSizeChanged} with a real
 * pivot. That costs one frame on the first open of each surface in a process and
 * nothing ever after.
 *
 * <p><b>What every subclass inherits, and may not re-decide:</b>
 *
 * <ul>
 *   <li>{@link ChromeMotion#ANCHORED_ENTER_MS} / {@link
 *       ChromeMotion#ANCHORED_EXIT_MS} and {@link ChromeMotion#anchoredEase()}.
 *   <li>Uniform scale on both axes, from {@link
 *       ChromeMotion#ANCHORED_START_SCALE} — a growth, never a stretch.
 *   <li>Cancel-first. A second tap reverses a transition; it never queues
 *       behind it.
 *   <li>Reduced motion lands on the final state <b>now</b>, with no transient
 *       scale or alpha and no animator posted at all.
 *   <li>{@link #isOpen()} is the state the user ASKED FOR, not this view's
 *       visibility: a closing surface is still {@code VISIBLE} for the length of
 *       its exit, and the control that draws itself active while the surface is
 *       up must not stay lit for it.
 *   <li>Nothing here touches the viewport, its size or its surface, and no
 *       transition runs while a viewport gesture is in flight — see
 *       {@link #setMotionAllowed}.
 * </ul>
 *
 * <p>What a subclass still owns is its content, its own material tier, its own
 * measurement rules and which edge it is anchored to.
 */
abstract class AnchoredSurfaceView extends LinearLayout {

    /**
     * Told whenever a surface opens or closes.
     *
     * <p>One choke point rather than a notification at each of the workspace's
     * open/close call sites, because the thing that has to know — whether
     * System Back has a surface to dismiss before it may leave the app — is
     * wrong the moment one site forgets to report.
     */
    interface OnOpenStateChanged {
        void onSurfaceOpenStateChanged(AnchoredSurfaceView surface, boolean open);

        /** Called after the presentation is fully visible or fully absent. */
        void onSurfacePresentationSettled(AnchoredSurfaceView surface, boolean visible);
    }

    /** The state the user asked for; see {@link #isOpen()}. */
    private boolean open;

    /** See {@link OnOpenStateChanged}. Null until the workspace attaches one. */
    private OnOpenStateChanged openStateListener;

    /** Which way the surface unfolds, set by the workspace's anchor from where
     *  the invoking control sits in the window. */
    private boolean growsUpward = true;

    /** False while a viewport gesture is in flight; see {@link #setMotionAllowed}. */
    private boolean motionAllowed = true;

    /**
     * Set when an open had to be deferred because this surface has never been
     * laid out, so {@link #onSizeChanged} knows to start the growth it staged.
     */
    private boolean growthPending;

    AnchoredSurfaceView(Context context) {
        super(context);
        setOrientation(VERTICAL);
        // Absent until asked for. Nothing about the resting workspace is a
        // context surface's to occupy.
        setVisibility(GONE);
    }

    /**
     * Which corner this surface grows from, in its own coordinates.
     *
     * <p>Answered by the subclass because it is a fact about where the surface
     * is anchored, not about how it moves: three of the four hang off a leading
     * edge and the Display popover hangs off the trailing one.
     *
     * @return true when the growth origin is the surface's own trailing edge
     */
    boolean anchoredToTrailingEdge() {
        return false;
    }

    /**
     * Whether this surface unfolds upward — set by the workspace's anchor.
     *
     * <p>The Objects capsule is low in the window on a phone and its {@code +}
     * sits under a column on a tablet, so the same surface has to grow both
     * ways and neither is a property of the surface itself.
     */
    final void setGrowsUpward(boolean upward) {
        growsUpward = upward;
        applyAnchorPivot();
    }

    final boolean growsUpward() {
        return growsUpward;
    }

    /**
     * Whether this surface may spend time on a transition.
     *
     * <p>Set false by the workspace while a viewport gesture — a camera orbit
     * or, in Sculpt Mode, a real stroke — is in flight. A chrome transition must
     * never compete with pointer samples for the main thread: input
     * responsiveness outranks motion, so the surface simply appears.
     */
    final void setMotionAllowed(boolean allowed) {
        motionAllowed = allowed;
    }

    /**
     * Whether the surface is open.
     *
     * <p>The <b>target</b> state, not this view's visibility. A closing surface
     * is still {@code VISIBLE} for the length of its exit, and a caller asking
     * "is this open" — the control that lights while it is, the workspace
     * deciding whether a tap opens or closes — means what the user asked for,
     * not whether an animation has finished running.
     */
    final boolean isOpen() {
        return open;
    }

    final void setOnOpenStateChanged(OnOpenStateChanged listener) {
        openStateListener = listener;
    }

    private void reportOpenState() {
        if (openStateListener != null) {
            openStateListener.onSurfaceOpenStateChanged(this, open);
        }
    }

    /** Opens or closes the surface, growing it out of the control that owns it. */
    final void setOpen(boolean open) {
        if (open == this.open) {
            return;
        }
        this.open = open;
        reportOpenState();
        // Cancel-first, always: a second tap while a transition is running must
        // reverse it rather than queue behind it, and a queued pair settles on
        // whichever animator happened to finish last.
        ChromeMotion.begin(this);
        growthPending = false;

        final long durationMs = ChromeMotion.duration(
                open ? ChromeMotion.ANCHORED_ENTER_MS : ChromeMotion.ANCHORED_EXIT_MS,
                motionAllowed ? ChromeMotion.animatorScale(getContext()) : 0.0f);
        if (durationMs <= 0L) {
            // Reduced motion, or a gesture in flight. Land on the final state
            // NOW: a zero-duration animator still posts a frame and still ends
            // asynchronously, which would leave a transient scale on screen for
            // exactly the user who asked for none.
            settle(open);
            return;
        }
        if (!open) {
            animateOut(durationMs);
            return;
        }
        stageEntry();
        if (needsSize()) {
            // Never laid out: the anchor corner is not knowable yet, because it
            // is expressed in this surface's own height. Visible and staged, so
            // the traversal measures it; the growth starts from onSizeChanged.
            growthPending = true;
            return;
        }
        applyAnchorPivot();
        animateIn(durationMs);
    }

    /**
     * Closes with no animation.
     *
     * <p>For the case where the chrome around the surface is disappearing in the
     * same frame — animating one panel out of a workspace that is already fading
     * only draws the eye to it.
     */
    final void closeImmediately() {
        final boolean wasOpen = open;
        open = false;
        if (wasOpen) {
            reportOpenState();
        }
        growthPending = false;
        ChromeMotion.begin(this);
        settle(false);
    }

    /**
     * Keeps the growth origin on the anchor corner once this surface has a size,
     * and starts a growth that was waiting for one.
     */
    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        applyAnchorPivot();
        if (!growthPending) {
            return;
        }
        growthPending = false;
        final long durationMs = ChromeMotion.duration(ChromeMotion.ANCHORED_ENTER_MS,
                motionAllowed ? ChromeMotion.animatorScale(getContext()) : 0.0f);
        if (durationMs <= 0L) {
            settle(true);
            return;
        }
        animateIn(durationMs);
    }

    /** Puts the growth origin on the corner the invoking control sits at. */
    private void applyAnchorPivot() {
        setPivotX(anchoredToTrailingEdge() ? getWidth() : 0.0f);
        setPivotY(growsUpward ? getHeight() : 0.0f);
    }

    /** Whether the anchor corner is still unknowable. */
    private boolean needsSize() {
        return (growsUpward && getHeight() <= 0)
                || (anchoredToTrailingEdge() && getWidth() <= 0);
    }

    /** The state a growth starts from, applied before the surface is measured. */
    private void stageEntry() {
        setAlpha(0.0f);
        setScaleX(ChromeMotion.ANCHORED_START_SCALE);
        setScaleY(ChromeMotion.ANCHORED_START_SCALE);
        setVisibility(VISIBLE);
    }

    private void animateIn(long durationMs) {
        animate().alpha(1.0f).scaleX(1.0f).scaleY(1.0f)
                .setDuration(durationMs)
                .setInterpolator(ChromeMotion.anchoredEase())
                .start();
    }

    private void animateOut(long durationMs) {
        applyAnchorPivot();
        animate().alpha(0.0f)
                .scaleX(ChromeMotion.ANCHORED_START_SCALE)
                .scaleY(ChromeMotion.ANCHORED_START_SCALE)
                .setDuration(durationMs)
                .setInterpolator(ChromeMotion.anchoredEase())
                .withEndAction(new Runnable() {
                    @Override
                    public void run() {
                        // Reached only when the exit finished; a reversal
                        // cancels it, and the reversal owns the state.
                        settle(false);
                    }
                }).start();
    }

    /**
     * Lands on a resting state, so the next open starts from a known transform
     * rather than from wherever a cancelled animation stopped.
     */
    private void settle(boolean visible) {
        ChromeMotion.settle(this, visible);
        if (openStateListener != null) {
            openStateListener.onSurfacePresentationSettled(this, visible);
        }
    }

    /**
     * The resting transform, for verification.
     *
     * <p>What a motion case may assert is that a surface always <b>ends</b>
     * somewhere legitimate — fully there or fully gone, at alpha 1 and scale 1 —
     * however the user interrupted it. Sampling a frame of a transition would be
     * a test of the device's frame timing.
     */
    final boolean isSettled() {
        return getAlpha() == 1.0f && getScaleX() == 1.0f && getScaleY() == 1.0f
                && getVisibility() == (open ? VISIBLE : GONE);
    }

    /** Where this surface currently grows from, for verification. */
    final float anchorPivotX() {
        return getPivotX();
    }

    /** Where this surface currently grows from, for verification. */
    final float anchorPivotY() {
        return getPivotY();
    }

    /** Only for verification of the first-open path; see {@link #onSizeChanged}. */
    final boolean growthWaitingForSize() {
        return growthPending;
    }

    /**
     * Consumes every touch this surface's own controls did not take.
     *
     * <p>The rule every chrome surface follows: a missed tap between two rows
     * must never reach the {@code SurfaceView} beneath, where it would orbit the
     * camera or, in Sculpt Mode, deform the model.
     */
    @Override
    public boolean onTouchEvent(android.view.MotionEvent event) {
        super.onTouchEvent(event);
        return true;
    }
}
