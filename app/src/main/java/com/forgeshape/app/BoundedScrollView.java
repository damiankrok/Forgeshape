package com.forgeshape.app;

import android.content.Context;
import android.widget.ScrollView;

/**
 * A {@link ScrollView} whose height its <b>parent</b> is allowed to cap.
 *
 * <p>This exists for one structural reason. A vertical {@code LinearLayout}
 * measures its children in order against the height that is left, so the FIRST
 * child can take everything and the LAST ones are handed what remains — which
 * on a squeezed window is nothing. That is how a shipped 48 dp control came to
 * measure 17 dp at a larger system font and to disappear outright with the
 * keyboard up: the deficit always landed on whichever control happened to be
 * last in the column.
 *
 * <p>The correction is to decide <i>which</i> child absorbs a deficit rather
 * than letting child order decide it. A scrolling container is the only child in
 * the trailing cluster that can honestly give height back — everything it cannot
 * show is still reachable by scrolling — so {@link TrailingClusterColumn}
 * measures the fixed controls first and caps this one with what is left.
 *
 * <p><b>The cap is measure-time state, not layout state.</b> It is written by
 * the parent from inside its own {@code onMeasure}, immediately before the
 * child is measured, so it deliberately does not call {@code requestLayout} —
 * doing so would schedule a second pass to reach the answer this one already
 * has. Zero means uncapped, and an explicitly sized {@code LayoutParams} is
 * still honoured: the cap is an upper bound, never a height of its own.
 */
class BoundedScrollView extends ScrollView {

    /** Zero disables the cap. See the class comment on why this is not a
     *  property that invalidates the layout. */
    private int maxHeightPx;

    BoundedScrollView(Context context) {
        super(context);
    }

    void setMaxHeightPx(int value) {
        maxHeightPx = Math.max(0, value);
    }

    /** For verification: what the parent last allowed this container. */
    int maxHeightPx() {
        return maxHeightPx;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int spec = heightMeasureSpec;
        if (maxHeightPx > 0) {
            final int mode = MeasureSpec.getMode(heightMeasureSpec);
            final int size = MeasureSpec.getSize(heightMeasureSpec);
            if (mode == MeasureSpec.UNSPECIFIED) {
                spec = MeasureSpec.makeMeasureSpec(maxHeightPx, MeasureSpec.AT_MOST);
            } else if (size > maxHeightPx) {
                spec = MeasureSpec.makeMeasureSpec(maxHeightPx, MeasureSpec.AT_MOST);
            }
        }
        super.onMeasure(widthMeasureSpec, spec);
    }
}
