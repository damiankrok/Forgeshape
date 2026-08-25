package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ScrollView;

/**
 * The precision surface's body, which <b>ends on a row rather than through
 * one</b>.
 *
 * <p>The defect this fixes is the one the owner review named first about the
 * exact-value panel: at rest, the bottom of the surface regularly cut a chip, a
 * field or a caption across its middle. The bottom inset was there and the
 * scrolling worked — what was wrong was where the surface was allowed to stop.
 * A bottom sheet is capped at a fraction of the window, that cap is a number of
 * pixels, and the body under it is a stack of rows of unrelated heights, so the
 * two lined up only by luck.
 *
 * <p>A control sliced across its middle is not read as "there is more below".
 * It is read as a rendering fault: the user cannot tell a clipped surface from a
 * broken one, and half a glyph is the single most expensive thing a precision
 * panel can show, because the panel's whole claim is that its numbers are exact.
 *
 * <p><b>The rule.</b> When the content is taller than the space, the visible
 * body is rounded DOWN to the bottom of the last row that fits whole. The user
 * then sees a complete row at the boundary and scrolls for the rest — and
 * scrolling is unchanged, because the content is untouched and only the viewport
 * it is seen through moved.
 *
 * <p><b>What this does not do.</b> It does not cap anything: the cap is still
 * {@code PropertyInspectorView}'s and still comes from the window. It does not
 * change the content, its order, its measurement or its scroll range. And it
 * never rounds when everything fits, so the panel is exactly as tall as its body
 * whenever its body is short — the content-sized philosophy is intact.
 *
 * <p>Rows are found one level at a time rather than by a tree walk: the body is
 * a {@code FrameLayout} holding one editor, and that editor is a vertical
 * {@code LinearLayout} whose children are the rows. Anything else — a body that
 * is not that shape — falls through and behaves exactly like a plain
 * {@code ScrollView}, which is the correct answer for content that has no rows
 * to end on.
 */
final class PrecisionScrollView extends ScrollView {

    /** Set during measure; see {@link #contentOverflows()}. */
    private boolean overflowing;

    /** Set during measure; see {@link #visibleContentHeight()}. */
    private int visibleContentHeight;

    PrecisionScrollView(Context context) {
        super(context);
    }

    /** Whether the body currently holds more than this container shows. */
    boolean contentOverflows() {
        return overflowing;
    }

    /** How much of the body is visible, in this container's own coordinates. */
    int visibleContentHeight() {
        return visibleContentHeight;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        final View content = getChildCount() > 0 ? getChildAt(0) : null;
        if (content == null) {
            overflowing = false;
            visibleContentHeight = 0;
            return;
        }
        final int inset = getPaddingTop() + getPaddingBottom();
        final int available = Math.max(0, getMeasuredHeight() - inset);
        final int contentHeight = content.getMeasuredHeight();
        visibleContentHeight = Math.min(available, contentHeight);
        overflowing = contentHeight > available;
        if (!overflowing || available <= 0) {
            // Everything fits. The panel is exactly as tall as its body, which
            // is what it is supposed to be whenever it can be.
            return;
        }
        final int boundary = lastWholeRowBottom(content, available);
        if (boundary <= 0 || boundary >= available) {
            // Either no row boundary could be found, or the first row is taller
            // than the whole space. Rounding down to nothing would hide the
            // body outright, which is worse than a cut, so this is left as the
            // platform measured it and the row simply scrolls.
            return;
        }
        visibleContentHeight = boundary;
        setMeasuredDimension(getMeasuredWidth(), boundary + inset);
    }

    /**
     * The bottom of the last row that fits entirely within {@code available}.
     *
     * <p>Computed from measured heights and margins rather than from
     * {@link View#getBottom()}, which is 0 until the layout pass this is running
     * ahead of.
     *
     * @return 0 when the content is not a stack of rows, or when no row fits
     */
    private static int lastWholeRowBottom(View content, int available) {
        final ViewGroup rows = rowsOf(content);
        if (rows == null) {
            return 0;
        }
        // Where the row stack itself begins inside the scrolled content, which
        // is not always 0: the editor sits in a FrameLayout body, and either can
        // carry padding of its own.
        int offset = rows.getPaddingTop();
        if (rows != content && content instanceof ViewGroup) {
            offset += ((ViewGroup) content).getPaddingTop();
            final ViewGroup.LayoutParams params = rows.getLayoutParams();
            if (params instanceof ViewGroup.MarginLayoutParams) {
                offset += ((ViewGroup.MarginLayoutParams) params).topMargin;
            }
        }

        int cursor = offset;
        int lastFittingBottom = 0;
        for (int i = 0; i < rows.getChildCount(); i++) {
            final View row = rows.getChildAt(i);
            if (row.getVisibility() == GONE) {
                continue;
            }
            int top = cursor;
            int bottom;
            final ViewGroup.LayoutParams params = row.getLayoutParams();
            if (params instanceof ViewGroup.MarginLayoutParams) {
                final ViewGroup.MarginLayoutParams margins =
                        (ViewGroup.MarginLayoutParams) params;
                top += margins.topMargin;
                bottom = top + row.getMeasuredHeight();
                cursor = bottom + margins.bottomMargin;
            } else {
                bottom = top + row.getMeasuredHeight();
                cursor = bottom;
            }
            if (bottom > available) {
                break;
            }
            lastFittingBottom = bottom;
        }
        return lastFittingBottom;
    }

    /**
     * The vertical stack of rows inside the scrolled content, or {@code null}.
     *
     * <p>Descends at most one wrapper, because that is the shape the precision
     * surface actually has: the scroll holds the inspector's body frame, and the
     * body frame holds exactly one editor. Anything deeper is not a row stack
     * this class can reason about, and guessing would be worse than declining.
     */
    private static ViewGroup rowsOf(View content) {
        if (isVerticalStack(content)) {
            return (ViewGroup) content;
        }
        if (content instanceof ViewGroup && ((ViewGroup) content).getChildCount() == 1) {
            final View only = ((ViewGroup) content).getChildAt(0);
            if (isVerticalStack(only)) {
                return (ViewGroup) only;
            }
        }
        return null;
    }

    private static boolean isVerticalStack(View view) {
        return view instanceof LinearLayout
                && ((LinearLayout) view).getOrientation() == LinearLayout.VERTICAL
                && ((LinearLayout) view).getChildCount() > 0;
    }
}
