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
 * The contextual exact-value panel.
 *
 * <p>One container, three different bodies: the Construction shape editor, the
 * Construction placement editor, or the Sculpt context. Which body it holds is
 * a function of the active mode and the active Tool Rail entry, and swapping it
 * changes the inspector's content and never its structure.
 *
 * <p>Two things about it are load-bearing rather than cosmetic:
 *
 * <ul>
 *   <li><b>Its body scrolls.</b> The absence of any scroll container in the
 *       Android layer is the direct cause of the landscape defect this stage
 *       exists to fix; a body that does not fit is reachable, not lost.</li>
 *   <li><b>It collapses.</b> Collapsed it costs one header strip, which is what
 *       makes the viewport floor achievable on a compact window without
 *       removing a control from anywhere.</li>
 * </ul>
 *
 * <p><b>Owns no value.</b> The bodies read and submit; this is a frame with a
 * title, a toggle and a scroll container.
 */
final class PropertyInspectorView extends LinearLayout {

    /** Told the user collapsed or expanded the panel. */
    interface OnExpandedChanged {
        /**
         * The user's decision, reported the moment it is made — before any
         * transition finishes. What the UI REMEMBERS must be the target, or a
         * rotation part-way through a collapse would come back expanded.
         */
        void onInspectorExpandedChanged(boolean expanded);

        /**
         * The panel has finished changing size and is at its resting layout.
         *
         * <p>Separate from the decision because a side-placed panel gives back
         * WIDTH when it collapses, and narrowing the column while the body is
         * still on screen would clip the very content that is leaving.
         */
        void onInspectorLayoutSettled();
    }

    private final TextView title;
    private final ImageView toggle;
    private final ScrollView scroll;
    private final FrameLayout body;
    private final OnExpandedChanged listener;

    private boolean expanded = true;

    /** See {@link #setMotionAllowed}. */
    private boolean motionAllowed = true;

    /** Zero disables the cap; a bottom sheet sets it so it cannot grow to fill
     *  the window, which is exactly what the previous panel did. */
    private int maxHeightPx;

    PropertyInspectorView(Context context, OnExpandedChanged listener) {
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
        // The whole header toggles, not only the chip: it is the largest target
        // in the panel and the one a thumb reaches for first.
        header.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleExpanded();
            }
        });
        addView(header, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                EditorControlStyles.dimen(context, R.dimen.inspector_handle_height)));

        title = EditorControlStyles.titleText(context, R.id.inspector_title, "");
        title.setSingleLine(true);
        header.addView(title, new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        toggle = EditorControlStyles.iconButton(context, R.id.inspector_toggle,
                R.drawable.ic_chevron_down,
                context.getString(R.string.inspector_collapse));
        toggle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleExpanded();
            }
        });
        header.addView(toggle, EditorControlStyles.iconButtonParams(context, 0));

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

        showExpanded(true);
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

    boolean isExpanded() {
        return expanded;
    }

    /**
     * Puts the panel in a detent with no transition.
     *
     * <p>Idempotent, and deliberately instant: this is what the measure pass
     * and every state refresh call, and a layout traversal is no place to start
     * an animation. Motion belongs to the one path that is a user ACT — see
     * {@link #toggleExpanded}.
     */
    void showExpanded(boolean value) {
        if (value == expanded && scroll.getVisibility() == (value ? VISIBLE : GONE)
                && scroll.getAlpha() == 1.0f) {
            return;
        }
        expanded = value;
        ChromeMotion.settle(scroll, value);
        showToggleGlyph(value);
    }

    /**
     * The chevron, its content description, and nothing else.
     *
     * <p>Set from the TARGET detent at the start of a transition rather than at
     * its end. An interrupted collapse must never leave a control claiming the
     * panel will do the opposite of what it is doing, and the glyph is the only
     * thing in the panel that could say so.
     */
    private void showToggleGlyph(boolean value) {
        // The chevron points the way the panel will go, not the way it is.
        toggle.setImageResource(
                value ? R.drawable.ic_chevron_down : R.drawable.ic_chevron_up);
        toggle.setContentDescription(getContext().getString(
                value ? R.string.inspector_collapse : R.string.inspector_expand));
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
     * The user changed the detent: the one path that animates.
     *
     * <p><b>The panel's size changes exactly once per toggle</b>, and the body
     * fades and slides the short distance either side of that. Animating the
     * HEIGHT would mean a {@code requestLayout} on every frame of the
     * transition, re-running the workspace's whole adaptive layout decision
     * — which lives in {@code onMeasure} — dozens of times for a panel that is
     * going to end up exactly where it always did. Alpha and translation are
     * drawing properties: they cost no traversal at all.
     *
     * <p>So the two directions are deliberately sequenced rather than
     * symmetric. Expanding opens the space first and lets the body arrive into
     * it; collapsing lets the body leave first and then closes the space. Both
     * end at the identical resting layout the instant path produces.
     *
     * <p>Not animated at all while a viewport gesture is in flight, or when the
     * platform asks for reduced motion. In both cases this is the instant path.
     */
    private void toggleExpanded() {
        final boolean value = !expanded;
        expanded = value;
        showToggleGlyph(value);
        if (listener != null) {
            listener.onInspectorExpandedChanged(value);
        }

        final long durationMs = ChromeMotion.duration(
                value ? ChromeMotion.ENTER_MS : ChromeMotion.EXIT_MS,
                ChromeMotion.animatorScale(getContext()));
        if (durationMs == 0L || !motionAllowed) {
            ChromeMotion.settle(scroll, value);
            notifyLayoutSettled();
            return;
        }

        ChromeMotion.begin(scroll);
        final float offset = EditorControlStyles.dimen(getContext(), R.dimen.row_gap);
        if (value) {
            scroll.setVisibility(VISIBLE);
            scroll.setAlpha(0.0f);
            scroll.setTranslationY(-offset);
            // The space is already open, so this is the resting layout from the
            // first frame; only the body's own drawing is still arriving.
            notifyLayoutSettled();
            scroll.animate().alpha(1.0f).translationY(0.0f).setDuration(durationMs).start();
        } else {
            scroll.animate().alpha(0.0f).translationY(-offset).setDuration(durationMs)
                    .withEndAction(new Runnable() {
                        @Override
                        public void run() {
                            // Reached only when the fade finished; a reversal
                            // cancels it, and the reversal owns the state.
                            ChromeMotion.settle(scroll, false);
                            notifyLayoutSettled();
                        }
                    }).start();
        }
    }

    private void notifyLayoutSettled() {
        if (listener != null) {
            listener.onInspectorLayoutSettled();
        }
    }

    /**
     * Whether this panel may spend time on a detent change.
     *
     * <p>Set false by the workspace while a viewport gesture — a camera orbit
     * or, in Sculpt Mode, a real stroke — is in flight. A chrome transition
     * must never compete with pointer samples for the main thread: input
     * responsiveness outranks motion, so the panel simply snaps instead.
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
