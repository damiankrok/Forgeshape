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
        void onInspectorExpandedChanged(boolean expanded);
    }

    private final TextView title;
    private final ImageView toggle;
    private final ScrollView scroll;
    private final FrameLayout body;
    private final OnExpandedChanged listener;

    private boolean expanded = true;

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

    void showExpanded(boolean value) {
        // Idempotent so it can be called from the measure pass without
        // requesting a fresh layout on every traversal.
        if (value == expanded && scroll.getVisibility() == (value ? VISIBLE : GONE)) {
            return;
        }
        expanded = value;
        scroll.setVisibility(value ? VISIBLE : GONE);
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
     * bottom sheet and a side overlay float, so they are rounded on the edge
     * that faces the model and carry a little depth. A <b>docked</b> panel on a
     * tablet does not float: it occupies its own column of the window, and
     * giving it a card's shadow would be a claim about the layout that is
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

    private void toggleExpanded() {
        showExpanded(!expanded);
        if (listener != null) {
            listener.onInspectorExpandedChanged(expanded);
        }
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
