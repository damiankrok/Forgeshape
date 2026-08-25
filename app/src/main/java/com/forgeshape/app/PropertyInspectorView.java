package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
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
 * <p><b>But the cap now ends on a row, not through one.</b> The cap is a height
 * in pixels and the body is a stack of rows, so the two agreed only by accident:
 * a sheet capped at 30 % of the window landed wherever it landed, which at rest
 * was regularly half-way through a chip or a field caption. A control sliced
 * across its middle by a panel edge reads as a rendering fault, not as "there is
 * more below" — the user cannot tell a clipped surface from a broken one. See
 * {@link PrecisionScrollView}, which rounds the visible body DOWN to the last
 * row that fits whole. Nothing about the cap, the scrolling, the keyboard or
 * exact-value editing changes; what changes is where the surface is allowed to
 * end.
 *
 * <p><b>Owns no value.</b> The bodies read and submit; this is a frame with a
 * title, a close control and a scroll container.
 */
final class PropertyInspectorView extends AnchoredSurfaceView {

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

    private final TextView title;
    private final PrecisionScrollView scroll;
    private final FrameLayout body;
    private final OnPrecisionSurfaceClosed listener;

    /** Zero disables the cap; a bottom sheet sets it so it cannot grow to fill
     *  the window, which is exactly what the previous panel did. */
    private int maxHeightPx;

    PropertyInspectorView(Context context, OnPrecisionSurfaceClosed listener) {
        super(context);
        this.listener = listener;
        setId(R.id.property_inspector);
        setContentDescription(context.getString(R.string.property_inspector));
        // One surface in every placement; see showAsFloatingPanel.
        showAsFloatingPanel();

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

        scroll = new PrecisionScrollView(context);
        scroll.setId(R.id.inspector_scroll);
        scroll.setFillViewport(false);
        scroll.setPadding(pad, 0, pad, pad);
        // CLIPPED to its padding, unlike most containers in the workspace.
        //
        // Nothing in this body casts a shadow, so the usual reason to let a
        // child draw outside the padding does not apply — and letting it meant
        // the bottom row was drawn INTO the sheet's own bottom inset, so the
        // panel's rounded edge cut across it. The padding is a margin here, and
        // it has to behave like one for the boundary to be a boundary.
        scroll.setClipToPadding(true);
        addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        body = new FrameLayout(context);
        body.setId(R.id.inspector_body);
        scroll.addView(body, new ScrollView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
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

    private void requestClose() {
        if (listener != null) {
            listener.onPrecisionCloseRequested();
        }
    }

    /**
     * Draws the panel, wherever its host has placed it.
     *
     * <p><b>One surface in all three placements, and that is the correction.</b>
     * A bottom sheet, a side overlay and a tablet's docked column used to be
     * three different-looking things: rounded on four corners, rounded on one
     * side, and a flat square-edged column with no depth. The user meets all
     * three in one session by rotating a device, and the panel changed
     * character each time — which is what made the expanded window read as a
     * desktop CAD frame rather than as the same workspace with more room.
     *
     * <p>It is a floating panel everywhere now: rounded on every corner, with
     * the same depth, inset from the window edges by its host. That is the same
     * claim the Tool Rail, the Objects column and the toolbar's capsules all
     * make — this is a surface in a workspace, and the model is around it.
     *
     * <p>Called on every placement decision rather than only on a change,
     * because it is idempotent and the caller runs inside a measure pass where
     * a conditional would be one more thing to get wrong.
     */
    void showAsFloatingPanel() {
        setBackgroundResource(R.drawable.bg_inspector_sheet);
        setElevation(EditorControlStyles.dimen(getContext(), R.dimen.elevation_sheet));
    }

    /**
     * Caps how tall the panel may measure. Zero means no cap.
     *
     * <p>Only a bottom sheet needs this: a side placement is already bounded by
     * the window's height. Without it a wrap-content panel measures to whatever
     * its content wants, which is how the previous one came to measure to the
     * full window in landscape.
     *
     * <p>What the cap does <b>not</b> decide is where the visible body ends.
     * That is {@link PrecisionScrollView}'s: a cap is a number of pixels and the
     * body is a stack of rows, so the cap is an upper bound and the last whole
     * row inside it is the boundary.
     */
    void setMaxHeightPx(int value) {
        if (maxHeightPx == value) {
            return;
        }
        maxHeightPx = value;
        requestLayout();
    }

    /**
     * Whether the body currently has more content than the surface shows.
     *
     * <p>For verification: a case proving there is no mid-row cut has to be able
     * to tell "everything fits" from "it was rounded down to a row", because
     * only the second is the interesting one.
     */
    boolean bodyIsScrollable() {
        return scroll.contentOverflows();
    }

    /**
     * Where the visible body ends, in the scroll container's own coordinates.
     *
     * <p>For verification. Compared against each row's bounds, this is what
     * proves no row is crossed by the boundary.
     */
    int visibleBodyBottom() {
        return scroll.visibleContentHeight();
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
}
