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
 * <p>One container, five bodies: the Construction shape editor, the placement
 * editor, the Sculpt context, the sketch editor and the CAD feature editor.
 * Which body it holds is a function of the active mode and the active Tool
 * Rail entry, and swapping it changes the inspector's content and never its
 * structure.
 *
 * <p><b>Closed means absent, not collapsed.</b> A collapsed detent is still a
 * full-width strip anchored to the bottom of the window — a permanent
 * structural claim made by a surface nobody asked for — so the panel is either
 * open, carrying its whole body, or not in the window at all. It <b>grows out
 * of the control that opened it</b>, the rail's precision toggle, so the
 * relation between the held tool and its numbers is spatial.
 *
 * <p><b>Its body scrolls and is capped as a bottom sheet</b>, and the cap ends
 * on a row, not through one: a control sliced across its middle reads as a
 * rendering fault, not as "there is more below". {@link PrecisionScrollView}
 * rounds the visible body DOWN to the last row that fits whole.
 *
 * <p><b>Owns no value.</b> The bodies read and submit; this is a frame with a
 * title, a close control and a scroll container.
 */
final class PropertyInspectorView extends AnchoredSurfaceView {

    /**
     * A body with a commit the panel pins rather than scrolls.
     *
     * <p>Apply used to be the last row of the body, and in compact portrait that
     * put it <b>three swipes below the fold</b> — the panel showed Position and
     * Rotation, ended flush at a row boundary, and said nothing about what was
     * under it. A commit the user cannot see is a commit they do not know they
     * have to make, and an exact-value panel whose whole point is that the
     * numbers are exact cannot hide the control that makes them true.
     *
     * <p>So the body scrolls and the commit does not. The editor still owns the
     * control and what pressing it means; the panel owns where it is drawn,
     * because only the panel knows how much of the body is on screen. A body
     * with nothing to commit — the Sculpt context — simply does not implement
     * this, and the footer is absent.
     */
    interface PinnedCommit {
        /** The commit control, or null when this body has nothing to commit. */
        View commitControl();
    }

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

    /** Holds the body's commit control, pinned below the scroll. See
     *  {@link PinnedCommit}. */
    private final FrameLayout footer;
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
        // A body that continues says so. The panel ends on a whole row, which
        // fixed a mid-row cut and cost the one cue a cut accidentally gave: a
        // clean edge reads as the end of the content. A fading bottom edge is
        // the platform's own answer, it appears only while there is more below,
        // and it costs no layout — where a peek row would cost a row.
        scroll.setVerticalFadingEdgeEnabled(true);
        scroll.setFadingEdgeLength(
                EditorControlStyles.dimen(context, R.dimen.precision_scroll_fade));
        addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        body = new FrameLayout(context);
        body.setId(R.id.inspector_body);
        scroll.addView(body, new ScrollView.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // The pinned commit, below the scrolling body and inside the panel. It
        // is empty and GONE for a body with nothing to commit, so a panel that
        // has no Apply has no footer either. See PinnedCommit.
        footer = new FrameLayout(context);
        footer.setId(R.id.inspector_footer);
        footer.setPadding(pad, pad, pad, pad);
        footer.setVisibility(GONE);
        addView(footer, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
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
        footer.removeAllViews();
        footer.setVisibility(GONE);
        if (content != null) {
            final ViewGroup previousParent = (ViewGroup) content.getParent();
            if (previousParent != null) {
                previousParent.removeView(content);
            }
            body.addView(content, new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            pinCommit(content);
        }
        // A body swap starts at the top: the panel is showing something else
        // now, and a carried-over scroll offset would open it part-way down.
        scroll.scrollTo(0, 0);
    }

    /**
     * Moves this body's commit control into the pinned footer.
     *
     * <p>Reparented rather than duplicated: there is one Apply, with one id and
     * one listener, and it is the editor's. What changes is only which container
     * draws it — and therefore whether it can scroll away.
     */
    private void pinCommit(View content) {
        if (!(content instanceof PinnedCommit)) {
            return;
        }
        final View commit = ((PinnedCommit) content).commitControl();
        if (commit == null) {
            return;
        }
        final ViewGroup previousParent = (ViewGroup) commit.getParent();
        if (previousParent != null) {
            previousParent.removeView(commit);
        }
        footer.addView(commit, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        footer.setVisibility(VISIBLE);
    }

    /** Whether a commit is currently pinned. For verification. */
    boolean hasPinnedCommit() {
        return footer.getVisibility() == VISIBLE && footer.getChildCount() == 1;
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
        reserveHeightForTitleAndCommit(widthMeasureSpec, spec);
        super.onMeasure(widthMeasureSpec, spec);
    }

    /**
     * Gives the title bar and the pinned commit their full height, and the
     * scrolling body whatever is left.
     *
     * <p>Without this the panel would repeat the defect the trailing cluster
     * had: a vertical {@code LinearLayout} measures in order against what is
     * left, so a body taller than the panel would take everything and squeeze
     * the commit under it to nothing — which is the exact control this stage
     * exists to keep reachable. The body is the one child that can give height
     * back without losing anything, because what it cannot show, it scrolls to.
     */
    private void reserveHeightForTitleAndCommit(int widthMeasureSpec, int heightMeasureSpec) {
        if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.UNSPECIFIED) {
            scroll.setMaxHeightPx(0);
            return;
        }
        final int available = MeasureSpec.getSize(heightMeasureSpec)
                - getPaddingTop() - getPaddingBottom();
        final int natural = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        int reserved = 0;
        for (int i = 0; i < getChildCount(); i++) {
            final View child = getChildAt(i);
            if (child.getVisibility() == GONE) {
                continue;
            }
            final MarginLayoutParams params = (MarginLayoutParams) child.getLayoutParams();
            if (child != scroll) {
                measureChildWithMargins(child, widthMeasureSpec, 0, natural, 0);
                reserved += child.getMeasuredHeight();
            }
            reserved += params.topMargin + params.bottomMargin;
        }
        // Never below one row, so a window short enough to leave nothing over
        // still shows a value rather than a sliver of one.
        scroll.setMaxHeightPx(Math.max(available - reserved,
                EditorControlStyles.dimen(getContext(), R.dimen.control_height)));
    }
}
