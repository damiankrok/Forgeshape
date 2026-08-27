package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

/**
 * The trailing tool cluster's column, with <b>one</b> child nominated to absorb
 * a height deficit.
 *
 * <p>A plain vertical {@code LinearLayout} answers "the window is too short" by
 * squeezing whichever children happen to be last, because it measures in order
 * against the height that is left. In this column the last children are shipped
 * 48 dp controls — the precision toggle and the transform selectors — so a
 * larger system font, a short landscape window or an open keyboard silently took
 * them below the touch floor or removed them from the tree altogether. Nothing
 * about that was a decision; it was child order.
 *
 * <p>This makes the decision instead. The fixed controls are measured first, at
 * the height they actually want, and the flexible child — the Tool Rail's scroll
 * container — is capped with what is left. The rail is the only child that can
 * give height back honestly: what it cannot show is still reachable by
 * scrolling, which is the overflow strategy this cluster already had and never
 * used. Everything else keeps its full height in every window.
 *
 * <p>The flexible child is never squeezed below {@code minimumFlexibleHeightPx}
 * — which the trailing cluster deliberately leaves at zero, because a floor
 * there would put the remaining deficit back on whatever is last in the column,
 * which is the defect this arrangement exists to remove. What is left over is
 * what the rail gets, and on any real window that is at least one entry.
 */
final class TrailingClusterColumn extends LinearLayout {

    /** The child that gives up height, or null while none has been nominated. */
    private BoundedScrollView flexible;

    /** The floor the flexible child is never capped below. */
    private int minimumFlexibleHeightPx;

    TrailingClusterColumn(Context context) {
        super(context);
        setOrientation(VERTICAL);
    }

    /**
     * Nominates the child that absorbs a deficit, and the height below which it
     * is not squeezed even then.
     */
    void setFlexibleChild(BoundedScrollView child, int minimumHeightPx) {
        flexible = child;
        minimumFlexibleHeightPx = Math.max(0, minimumHeightPx);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        capFlexibleChild(widthMeasureSpec, heightMeasureSpec);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Measures every fixed child at its natural height and hands the remainder
     * to the flexible one.
     *
     * <p>The natural height is taken with an UNSPECIFIED spec deliberately: what
     * is wanted here is what the control needs, not what the current squeeze
     * would allow it, and asking with the available height is exactly the
     * question whose answer was the defect. The children are measured again by
     * {@code super.onMeasure} straight afterwards, which is cheap and is what
     * keeps this a cap rather than a second layout algorithm.
     */
    private void capFlexibleChild(int widthMeasureSpec, int heightMeasureSpec) {
        if (flexible == null || flexible.getVisibility() == GONE) {
            return;
        }
        if (MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.UNSPECIFIED) {
            flexible.setMaxHeightPx(0);
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
            if (child != flexible) {
                measureChildWithMargins(child, widthMeasureSpec, 0, natural, 0);
                reserved += child.getMeasuredHeight();
            }
            reserved += params.topMargin + params.bottomMargin;
        }
        flexible.setMaxHeightPx(Math.max(available - reserved, minimumFlexibleHeightPx));
    }
}
