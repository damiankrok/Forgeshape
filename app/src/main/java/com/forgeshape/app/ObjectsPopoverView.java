package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
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
 * <p><b>Why it is its own surface.</b> Scene-level content nested inside the
 * active-object value panel is a role confusion (and a list scrolling inside
 * another scroll), so the two concerns are separate surfaces: what the scene
 * HOLDS, and what the selected body's numbers ARE.
 *
 * <p>It scrolls, because a scene can grow without limit while a window cannot,
 * and it is capped so it can never become the window. Opened from the Objects
 * capsule and anchored to it, so reaching the scene costs one tap in every mode
 * and every window. The growth itself is {@link AnchoredSurfaceView}'s.
 */
final class ObjectsPopoverView extends AnchoredSurfaceView {

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
     * The panel's layout in the overlay, at its own fixed width.
     *
     * <p>Where it actually hangs is decided per open by the workspace's anchor,
     * from the bounds of the control that opened it. The width is fixed rather
     * than measured because the anchor has to place the surface before it has
     * ever been laid out — see {@code EditorWorkspaceView.anchorOverlayTo}.
     */
    static ViewGroup.LayoutParams anchoredParams(Context context) {
        final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.objects_popover_width),
                ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.BOTTOM | Gravity.START;
        return params;
    }
}
