package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The contextual exact-value panel — a precision surface the user <b>asks
 * for</b>.
 *
 * <p>One container, three different bodies: the Construction shape editor, the
 * Construction placement editor, or the Sculpt context. Which body it holds is
 * a function of the active mode and the active Tool Rail entry, and swapping it
 * changes the inspector's content and never its structure.
 *
 * <p><b>Closed means absent, not collapsed.</b> This is the load-bearing change
 * of the mobile workspace: the panel used to have a resting detent, and a
 * collapsed detent is still a full-width strip anchored to the bottom of the
 * window — a permanent structural claim on the workspace made by a surface
 * nobody had asked for. It is now either open, carrying its whole body, or it
 * is not in the window at all, and the viewport reaches the bottom edge. Exact
 * values are ForgeShape's advantage and are one tap away from the tool context
 * that owns them; what changed is which of the two owns the resting layout.
 *
 * <p><b>It grows out of the control that opened it</b> — the Tool Rail's
 * precision toggle — rather than sliding in from an edge, so the relation
 * between the tool being held and the numbers behind it is spatial rather than
 * something to be remembered.
 *
 * <p><b>Its body still scrolls</b>, and is still capped when it is a bottom
 * sheet: a body that does not fit must be reachable, not lost, and an uncapped
 * wrap-content sheet grows to whatever its content wants.
 *
 * <p><b>Owns no value.</b> The bodies read and submit; this is a frame with a
 * title, a close control and a scroll container.
 */
final class PropertyInspectorView extends LinearLayout {

    /** Told the user dismissed the precision surface. */
    interface OnPrecisionSurfaceClosed {
        /**
         * The user closed the panel from its own header.
         *
         * <p>Reported rather than acted on here, because what a closed
         * precision surface means to the workspace — which tool context is
         * still held, which control returns to its resting state — is the
         * workspace's business and not this frame's.
         */
        void onPrecisionCloseRequested();
    }

    /** Matches the Objects panel and the Add Primitive palette, so every
     *  surface the workspace opens on demand behaves the same way. */
    private static final long OPEN_DURATION_MS = 140L;
    private static final long CLOSE_DURATION_MS = 100L;

    private final TextView title;
    private final ScrollView scroll;
    private final FrameLayout body;
    private final OnPrecisionSurfaceClosed listener;

    /** See {@link #setMotionAllowed}. */
    private boolean motionAllowed = true;

    /** Set by the anchor: which way the surface unfolds from its toggle. */
    private boolean growsUpward = true;

    /** The state the user asked for; see {@link #isOpen()}. */
    private boolean open;

    /** Zero disables the cap; a bottom sheet sets it so it cannot grow to fill
     *  the window, which is exactly what the previous panel did. */
    private int maxHeightPx;

    PropertyInspectorView(Context context, OnPrecisionSurfaceClosed listener) {
        super(context);
        this.listener = listener;
        setId(R.id.property_inspector);
        setOrientation(VERTICAL);
        setContentDescription(context.getString(R.string.property_inspector));
        // A placement decides the shape and the depth; see showFloating.
        showFloating(true);

        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);

        final LinearLayout header = new LinearLayout(context);
        header.setOrientation(HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(pad, 0, pad, 0);
        // The whole header dismisses, not only the chip: it is the largest
        // target in the panel and the one a thumb reaches for first.
        header.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                requestClose();
            }
        });
        addView(header, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                EditorControlStyles.dimen(context, R.dimen.inspector_handle_height)));

        title = EditorControlStyles.titleText(context, R.id.inspector_title, "");
        title.setSingleLine(true);
        header.addView(title, new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        final ImageView close = EditorControlStyles.iconButton(context, R.id.inspector_toggle,
                R.drawable.ic_chevron_down, context.getString(R.string.inspector_close));
        close.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                requestClose();
            }
        });
        header.addView(close, EditorControlStyles.iconButtonParams(context, 0));

        scroll = new ScrollView(context);
        scroll.setId(R.id.inspector_scroll);
        scroll.setFillViewport(false);
        scroll.setPadding(pad, 0, pad, pad);
        scroll.setClipToPadding(false);
        addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        body = new FrameLayout(context);
        body.setId(R.id.inspector_body);
        scroll.addView(body, new ScrollView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // Absent until asked for. Nothing about the resting workspace is this
        // panel's to occupy.
        setVisibility(GONE);
    }

    /** Replaces the contextual body. The caller owns which one belongs here. */
    void setBody(View content, CharSequence panelTitle) {
        title.setText(panelTitle);
        title.setContentDescription(panelTitle);
        if (body.getChildCount() == 1 && body.getChildAt(0) == content) {
            return;
        }
        body.removeAllViews();
        if (content != null) {
            final ViewGroup previousParent = (ViewGroup) content.getParent();
            if (previousParent != null) {
                previousParent.removeView(content);
            }
            body.addView(content, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        // A body swap starts at the top: the panel is showing something else
        // now, and a carried-over scroll offset would open it part-way down.
        scroll.scrollTo(0, 0);
    }

    /**
     * Whether the precision surface is open.
     *
     * <p>The <b>target</b> state, not this frame's visibility: a closing panel
     * is still VISIBLE for the length of its fade, and the toggle that draws
     * itself active while it is open must not be lit by an animation that has
     * not finished running.
     */
    boolean isOpen() {
        return open;
    }

    /**
     * Which way the surface unfolds from the control that opened it.
     *
     * <p>A compact window's precision toggle is roughly mid-height beside the
     * rail and the sheet arrives from the bottom; a docked one hangs from the
     * top. Set by the workspace's anchor, for the same reason the Objects panel
     * and the Add Primitive palette take it.
     */
    void setGrowsUpward(boolean upward) {
        growsUpward = upward;
    }

    /**
     * Opens or closes the whole surface.
     *
     * <p>The one path that animates, and it animates the panel's <b>drawing</b>
     * rather than its height: animating a height would mean a
     * {@code requestLayout} on every frame, which re-runs the workspace's whole
     * adaptive layout decision — it lives in {@code onMeasure} — dozens of times
     * for a panel that is going to end up exactly where it always did. Alpha and
     * scale cost no traversal at all.
     *
     * <p>Not animated while a viewport gesture is in flight, or when the
     * platform asks for reduced motion. Both are the instant path.
     */
    void setOpen(boolean open) {
        if (open == this.open) {
            return;
        }
        this.open = open;
        // Always interruptible: a second tap while a transition is running must
        // reverse it, not queue behind it.
        ChromeMotion.begin(this);
        setPivotX(0.0f);
        setPivotY(growsUpward ? getHeight() : 0.0f);

        final long durationMs = ChromeMotion.duration(
                open ? OPEN_DURATION_MS : CLOSE_DURATION_MS,
                motionAllowed ? ChromeMotion.animatorScale(getContext()) : 0.0f);
        if (durationMs <= 0L) {
            settle(open);
            return;
        }
        if (open) {
            setAlpha(0.0f);
            setScaleX(0.98f);
            setScaleY(0.96f);
            setVisibility(VISIBLE);
            animate().alpha(1.0f).scaleX(1.0f).scaleY(1.0f)
                    .setDuration(durationMs).start();
        } else {
            animate().alpha(0.0f).scaleX(0.98f).scaleY(0.96f)
                    .setDuration(durationMs)
                    .withEndAction(new Runnable() {
                        @Override
                        public void run() {
                            // Reached only when the fade finished; a reversal
                            // cancels it, and the reversal owns the state.
                            settle(false);
                        }
                    }).start();
        }
    }

    /** Lands on a resting state with no animation running on the view. */
    private void settle(boolean open) {
        setVisibility(open ? VISIBLE : GONE);
        setAlpha(1.0f);
        setScaleX(1.0f);
        setScaleY(1.0f);
    }

    private void requestClose() {
        if (listener != null) {
            listener.onPrecisionCloseRequested();
        }
    }

    /**
     * Says whether this panel is standing ON the model or sitting BESIDE it.
     *
     * <p>The two are genuinely different surfaces and are drawn differently. A
     * bottom sheet is INSET from the window edges by its host and is therefore
     * rounded on all four corners — it stands over the model with the viewport
     * visible around it. A side overlay floats against the trailing edge and is
     * rounded only on the side that faces the model. A <b>docked</b> panel on a
     * tablet does not float at all: it occupies its own column of the window,
     * and giving it a card's shadow would be a claim about the layout that is
     * simply untrue.
     *
     * @param bottomSheet whether the panel is anchored to the bottom edge; the
     *                    trailing-edge placements round their leading side
     *                    instead
     */
    void showFloating(boolean bottomSheet) {
        setBackgroundResource(bottomSheet
                ? R.drawable.bg_inspector_sheet : R.drawable.bg_inspector_side);
        setElevation(EditorControlStyles.dimen(getContext(), R.dimen.elevation_sheet));
    }

    /** Draws the panel as part of the layout rather than as a surface over it. */
    void showDocked() {
        setBackgroundResource(R.drawable.bg_inspector_side);
        setElevation(0.0f);
    }

    /**
     * Whether this panel may spend time on a transition.
     *
     * <p>Set false by the workspace while a viewport gesture — a camera orbit
     * or, in Sculpt Mode, a real stroke — is in flight. A chrome transition
     * must never compete with pointer samples for the main thread: input
     * responsiveness outranks motion, so the panel simply appears instead.
     */
    void setMotionAllowed(boolean allowed) {
        motionAllowed = allowed;
    }

    /**
     * Caps how tall the panel may measure. Zero means no cap.
     *
     * <p>Only a bottom sheet needs this: a side placement is already bounded by
     * the window's height. Without it a wrap-content panel measures to whatever
     * its content wants, which is how the previous one came to measure to the
     * full window in landscape.
     */
    void setMaxHeightPx(int value) {
        if (maxHeightPx == value) {
            return;
        }
        maxHeightPx = value;
        requestLayout();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int spec = heightMeasureSpec;
        if (maxHeightPx > 0) {
            final int mode = MeasureSpec.getMode(heightMeasureSpec);
            final int size = MeasureSpec.getSize(heightMeasureSpec);
            final int limit = (mode == MeasureSpec.UNSPECIFIED)
                    ? maxHeightPx : Math.min(size, maxHeightPx);
            spec = MeasureSpec.makeMeasureSpec(limit, MeasureSpec.AT_MOST);
        }
        super.onMeasure(widthMeasureSpec, spec);
    }

    /**
     * Swallows every touch the inspector's own controls did not take.
     *
     * <p>Includes the gaps between fields and the padding around them: this
     * panel sits over the viewport, and a missed tap next to a text field must
     * not orbit the model or, in Sculpt Mode, deform it.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
