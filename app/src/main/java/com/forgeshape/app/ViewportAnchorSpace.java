package com.forgeshape.app;

import android.view.View;
import android.view.ViewGroup;

/**
 * The one conversion between a projected VIEWPORT anchor and where an Android
 * view has to be translated to stand on it (`UI-OWNER-50`, `UI-3D-STATE-C1`).
 *
 * <p><b>Why this exists.</b> Native projects a world point into
 * <b>viewport-content pixels</b>: the origin is the rendered surface's own
 * top-left, which is the window's top-left, because the Vulkan viewport is
 * full-bleed and no inset, padding or layout decision may move it. Chrome, by
 * contrast, lives under {@code overlayRoot}, which carries the window-inset
 * padding every other piece of chrome is offset by. A {@code setTranslationY}
 * computed straight from a viewport anchor is therefore drawn one system-bar
 * inset too low.
 *
 * <p><b>The contract, stated once.</b> A projected anchor is viewport-local. A
 * placed view's translation is measured from its own LAYOUT position inside its
 * parent. The bridge between the two is pure runtime geometry:
 *
 * <pre>
 *   translation = anchor + (viewportOriginInWindow
 *                           - parentOriginInWindow
 *                           - placedLayoutPositionInParent)
 * </pre>
 *
 * <p>There is no constant in it. No status-bar height, no navigation-bar
 * height, no density-specific correction and no assumption that the horizontal
 * inset is zero: a landscape navigation bar or a display cutout gives
 * {@code overlayRoot} a left or right padding, and the same arithmetic absorbs
 * it. It is equally correct for a view that translates ITSELF inside the
 * overlay ({@link SketchDimensionLabelView}) and for a container that
 * translates its CHILDREN ({@link BodyDimensionLabelsView}, {@link
 * CadExtrudeCanvasView}), because {@code getLeft()/getTop()} already answer
 * "where does this view's translation start" in both shapes.
 *
 * <p><b>The clamp is in the same space.</b> The rectangle a placed view is kept
 * inside is the real viewport — {@code (0, 0, viewportWidth, viewportHeight)}
 * in viewport pixels — carried through the same offset, never the padded
 * content box of whatever container happens to hold the view. So a control near
 * an edge reaches its clamp exactly where the window requires and not one inset
 * earlier.
 *
 * <p><b>Presentation only.</b> Nothing here reads or writes domain state: it
 * converts a number native already decided into a number the view system
 * understands. The projection itself is untouched, and no anchor is nudged to
 * make an Android placement look right.
 */
final class ViewportAnchorSpace {

    /** The full-bleed render surface every projected anchor is measured from. */
    private final View viewport;

    /** Scratch, so a per-gesture-sample placement allocates nothing. */
    private final int[] here = new int[2];
    private final int[] there = new int[2];

    /**
     * Whether this pass had to read a layout that had not run yet.
     *
     * <p>A container the refresh has just made visible is <b>not laid out at its
     * own position</b> at the instant it is placed into: a {@code GONE} view is
     * skipped by its parent's layout, so it still reports position 0, and an
     * offset derived from it is short by exactly the container's own layout
     * position — one window inset, in this product. Its measured size is stale
     * for the same reason. The pass records that instead of hiding it, and the
     * workspace repeats the refresh once layout has run.
     */
    private boolean layoutPending;

    ViewportAnchorSpace(View viewport) {
        this.viewport = viewport;
    }

    /** Starts one refresh pass. See {@link #layoutWasPending()}. */
    void beginPass() {
        layoutPending = false;
    }

    /**
     * Whether any placement in this pass read a layout that was still pending.
     *
     * <p>True means the placements written are the best answer available now and
     * must be recomputed once the pending layout has run — the first frame of a
     * surface, and nothing else in steady state.
     */
    boolean layoutWasPending() {
        return layoutPending;
    }

    /**
     * Whether the runtime geometry needed for a conversion exists yet.
     *
     * <p>False before the first layout pass, when neither the viewport nor the
     * chrome has a position or a size. A caller that gets false must not guess a
     * placement: it asks again once layout is ready.
     */
    boolean ready() {
        return viewport.getWidth() > 0 && viewport.getHeight() > 0
                && viewport.isAttachedToWindow();
    }

    /** The viewport width in its own pixels, which is what native projects into. */
    int viewportWidth() {
        return viewport.getWidth();
    }

    /** The viewport height in its own pixels. */
    int viewportHeight() {
        return viewport.getHeight();
    }

    /**
     * Places one view centred on a viewport anchor, clamped into the viewport.
     *
     * @param placed  the view whose translation is written: the child a
     *                container moves, or the view itself where it moves itself
     * @param anchorX the projected anchor, in viewport-content pixels
     * @param anchorY the same, vertically
     * @param width   the placed box width in pixels, scale already folded in
     * @param height  the same, vertically
     * @return whether a placement was written; false leaves the view untouched
     *         rather than moving it to a coordinate derived from geometry that
     *         does not exist yet
     */
    boolean place(View placed, float anchorX, float anchorY, float width, float height) {
        if (!ready() || !(placed.getParent() instanceof View)) {
            return false;
        }
        final View parent = (View) placed.getParent();
        if (!parent.isLaidOut() || !placed.isLaidOut()) {
            // The offset below is read from where the parent IS, and a container
            // the refresh has just made visible has not been laid out at its own
            // position yet — a GONE view is skipped by its parent's layout, so it
            // still reports 0. Placed anyway, so a surface never flashes at the
            // window origin, and recorded so the workspace asks once more after
            // the layout runs.
            //
            // Deliberately NOT `isLayoutRequested()`: a chip whose text changed
            // requests layout on every ordinary refresh while its container has
            // not moved at all, and treating that as a pending offset would turn
            // a one-shot correction into a standing one.
            layoutPending = true;
        }
        viewport.getLocationInWindow(here);
        parent.getLocationInWindow(there);
        // The offset that carries a viewport pixel into this view's own
        // translation space. Derived entirely from where things actually ARE at
        // this instant, so an arbitrary inset, a re-parenting or a window change
        // needs no second rule.
        final float offsetX = here[0] - there[0] - placed.getLeft();
        final float offsetY = here[1] - there[1] - placed.getTop();

        float left = anchorX - width * 0.5f + offsetX;
        float top = anchorY - height * 0.5f + offsetY;
        // The clamp bound is the REAL viewport carried into the same space, not
        // the container's padded content box: a value half outside the window is
        // one the user can neither read nor tap, and a value pushed in by a whole
        // inset is one standing further from its geometry than it needs to.
        final float minLeft = offsetX;
        final float minTop = offsetY;
        final float maxLeft = offsetX + viewport.getWidth() - width;
        final float maxTop = offsetY + viewport.getHeight() - height;
        // Guarded both ways: a box wider than the viewport would otherwise clamp
        // to a maximum below its minimum and jump to the far edge.
        left = maxLeft >= minLeft ? Math.max(minLeft, Math.min(left, maxLeft)) : minLeft;
        top = maxTop >= minTop ? Math.max(minTop, Math.min(top, maxTop)) : minTop;

        placed.setTranslationX(left);
        placed.setTranslationY(top);
        return true;
    }

    /**
     * Measures a view at its natural size and places it, scale folded in.
     *
     * <p>The shared shape of all three placing views: measure unconstrained,
     * take the scaled box, centre it on the anchor. The scale is the caller's,
     * and every current caller passes 1.0: the CAD extrude HUD used to scale
     * its whole cluster by the camera-attached multiplier, and since
     * `CAD-VERTICAL-SLICE-R1` it sizes only its GLYPHS by it, because scaling
     * the view scaled its hit area down with it.
     */
    boolean measureAndPlace(View placed, float anchorX, float anchorY, float scale) {
        measureUnderParent(placed);
        return place(placed, anchorX, anchorY, placed.getMeasuredWidth() * scale,
                placed.getMeasuredHeight() * scale);
    }

    /**
     * Measures a view exactly as {@link #measureAndPlace} does, without placing
     * it.
     *
     * <p>For a caller that has to know the measured box BEFORE it chooses the
     * anchor it hands over — the CAD extrude HUD centres its VALUE, not its
     * whole cluster, on the arrow, and hangs a palette from the box the cluster
     * was given. One measuring rule, so the box it reasons about is the box
     * drawn.
     */
    void measureUnderParent(View placed) {
        // Measured under the CONSTRAINT its parent will impose, not
        // unconstrained. A wrap-content child of a full-window container is laid
        // out at most as wide as that container, so an unconstrained measure of a
        // wide cluster answers a size the layout will never produce — and
        // centring that box shifts the control sideways by half the difference.
        // This is the same `AT_MOST` a FrameLayout gives such a child, so the box
        // centred here is the box drawn.
        int widthSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        int heightSpec = widthSpec;
        if (placed.getParent() instanceof View) {
            final View parent = (View) placed.getParent();
            final int availableWidth =
                    parent.getWidth() - parent.getPaddingLeft() - parent.getPaddingRight();
            final int availableHeight =
                    parent.getHeight() - parent.getPaddingTop() - parent.getPaddingBottom();
            if (availableWidth > 0) {
                widthSpec = View.MeasureSpec.makeMeasureSpec(availableWidth,
                        View.MeasureSpec.AT_MOST);
            }
            if (availableHeight > 0) {
                heightSpec = View.MeasureSpec.makeMeasureSpec(availableHeight,
                        View.MeasureSpec.AT_MOST);
            }
        }
        placed.measure(widthSpec, heightSpec);
    }

    /**
     * Runs one action now, or once layout has given the viewport a geometry.
     *
     * <p>{@code UI3D-F-003}'s second half: a surface opened before the first
     * layout pass has nowhere to stand, and the answer is one bounded wait for
     * layout readiness, never a repeating poll. The action runs immediately when
     * geometry already exists, so the common case costs nothing.
     */
    void whenReady(ViewGroup host, Runnable action) {
        if (ready()) {
            action.run();
            return;
        }
        host.post(action);
    }
}
