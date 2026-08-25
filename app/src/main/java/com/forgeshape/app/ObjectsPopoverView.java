package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

/**
 * The Objects panel for every window that has not earned a column.
 *
 * <p><b>A host, not a second Objects implementation.</b> Exactly like
 * {@code objectsDock}, this panel is handed the one
 * {@link ObjectsSectionView} the workspace owns; it builds no rows, holds no
 * {@code ObjectId} and does not remember which body is active. There is one
 * scene list in the product and it simply moves between three hosts.
 *
 * <p><b>Why it exists.</b> The scene list used to live inside the Construction
 * shape editor, which is the Property Inspector's body — so a panel titled
 * "Shape" opened with the list of bodies first and pushed the width/height/depth
 * fields, the thing it is named after, below the fold of a sheet capped at 30 %
 * of the window. Scene-level content nested inside the active-object value panel
 * is a role confusion, and on a compact window it was also a list scrolling
 * inside another scroll. Here the two concerns are separate surfaces: what the
 * scene HOLDS, and what the selected body's numbers ARE.
 *
 * <p>It scrolls, because a scene can grow without limit while a window cannot,
 * and it is capped so it can never become the window the way the old sheet did.
 *
 * <p>Opened from the Global Toolbar, so reaching the scene costs one tap in
 * every mode and every window rather than "expand the inspector, then scroll".
 */
final class ObjectsPopoverView extends LinearLayout {

    /** Matches the Display popover, so the two panels feel like one system. */
    private static final long OPEN_DURATION_MS = 140L;
    private static final long CLOSE_DURATION_MS = 100L;

    /**
     * Tallest the panel may be, as a fraction of the window.
     *
     * <p>Capped for the same reason the bottom sheet is: an uncapped
     * wrap-content panel grows to whatever its content wants, and a scene of
     * twenty bodies would cover the model completely. Beyond this the list
     * scrolls.
     */
    private static final float MAX_HEIGHT_FRACTION = 0.55f;

    private final ScrollView scroll;
    private final FrameLayout body;

    ObjectsPopoverView(Context context) {
        super(context);
        setId(R.id.objects_popover);
        setOrientation(VERTICAL);
        setContentDescription(context.getString(R.string.objects));
        // TIER 2 — an expanded context surface, like the Display popover. It
        // carries a named list to be read rather than a capsule to be glanced
        // at, so it is opaque.
        EditorControlStyles.applyContextSurface(this);

        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        setPadding(pad, pad, pad, pad);

        scroll = new ScrollView(context);
        body = new FrameLayout(context);
        scroll.addView(body, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(scroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        setVisibility(GONE);
    }

    /**
     * Takes the one Objects section in, or reports that it is not here.
     *
     * <p>Idempotent, and called from the layout decision, which runs on every
     * measure pass.
     */
    void host(ObjectsSectionView objects) {
        if (objects.getParent() == body) {
            return;
        }
        if (objects.getParent() instanceof ViewGroup) {
            ((ViewGroup) objects.getParent()).removeView(objects);
        }
        body.addView(objects, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /** Whether the one Objects section is currently parented here. */
    boolean hosts(ObjectsSectionView objects) {
        return objects.getParent() == body;
    }

    boolean isOpen() {
        return getVisibility() == VISIBLE;
    }

    /** Opens or closes the panel, growing from the control that opened it. */
    void setOpen(boolean open) {
        if (open == isOpen()) {
            return;
        }
        // Always interruptible: a second tap while the open animation is still
        // running must close it, not queue behind it.
        ChromeMotion.begin(this);

        // The panel grows out of the toolbar control at the top-right.
        setPivotX(getWidth());
        setPivotY(0.0f);

        if (!ChromeMotion.animationsEnabled(getContext())) {
            setVisibility(open ? VISIBLE : GONE);
            setAlpha(1.0f);
            setScaleX(1.0f);
            setScaleY(1.0f);
            return;
        }
        if (open) {
            setAlpha(0.0f);
            setScaleX(0.96f);
            setScaleY(0.96f);
            setVisibility(VISIBLE);
            animate().alpha(1.0f).scaleX(1.0f).scaleY(1.0f)
                    .setDuration(OPEN_DURATION_MS).start();
        } else {
            animate().alpha(0.0f).scaleX(0.96f).scaleY(0.96f)
                    .setDuration(CLOSE_DURATION_MS)
                    .withEndAction(new Runnable() {
                        @Override
                        public void run() {
                            setVisibility(GONE);
                            // Left in the resting state so the next open starts
                            // from a known transform rather than from wherever a
                            // cancelled animation stopped.
                            setAlpha(1.0f);
                            setScaleX(1.0f);
                            setScaleY(1.0f);
                        }
                    }).start();
        }
    }

    /**
     * Closes with no animation.
     *
     * <p>For the case where the chrome around the panel is disappearing in the
     * same frame — animating one panel out of a workspace that is already fading
     * would only draw the eye to it.
     */
    void closeImmediately() {
        ChromeMotion.begin(this);
        setVisibility(GONE);
        setAlpha(1.0f);
        setScaleX(1.0f);
        setScaleY(1.0f);
    }

    /**
     * Consumes every touch that lands on the panel.
     *
     * <p>The same rule every chrome surface follows: a tap here must never
     * reach the {@code SurfaceView} beneath, where it would orbit the camera.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);
        return true;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        final View parent = (View) getParent();
        if (parent == null || parent.getHeight() <= 0) {
            return;
        }
        final int cap = Math.round(parent.getHeight() * MAX_HEIGHT_FRACTION);
        if (getMeasuredHeight() > cap) {
            setMeasuredDimension(getMeasuredWidth(), cap);
        }
    }

    /**
     * Where the panel hangs: under the toolbar, on the trailing edge, beneath
     * the control that opens it.
     */
    static ViewGroup.LayoutParams anchoredParams(Context context, int topOffsetPx) {
        final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.objects_popover_width),
                ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP | Gravity.END;
        params.topMargin = topOffsetPx;
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }
}
