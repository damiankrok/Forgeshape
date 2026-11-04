package com.forgeshape.app;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;

/**
 * The workbench's view cube ({@code WORKBENCH-RIBBON-R1}): the world's three
 * axes drawn from the camera's own orientation, and each of their six ends a
 * standard view — +X Right, +Y Top, +Z Front and their opposites.
 *
 * <p>A pure view of native truth: the directions are read from the camera on
 * every draw ({@link NativeViewport#cameraViewAxes}), nothing is stored, and a
 * tap asks native for the view ({@link NativeViewport#cameraStandardView}),
 * which refuses inside a sketch's exact view. The whole cube is one touch
 * target, comfortably over the 48 dp floor; a tap goes to the NEAREST end,
 * the nearer of two that overlap on screen winning, so ends drawn close
 * together never make a dead zone.
 *
 * <p>Since {@code WORKBENCH-UX-R1} every end is reachable by touch: tapping
 * the end that already faces the viewer (within {@link #FACING_COS}) turns to
 * the OPPOSITE view, so Bottom, Back and Left are one tap from Top, Front and
 * Right; a press that travels past {@link #DRAG_SLOP_DP} orbits the camera
 * (the same {@link NativeViewport#cameraPointerOrbit} a mouse drag takes) and
 * snaps nothing; the negative ends are labelled rings rather than dim dots;
 * and TalkBack reads six virtual nodes, one per view.
 */
final class WorkbenchViewCubeView extends View {

    interface OnViewChosen {
        void onStandardViewChosen(int view);

        /** The cube was dragged: the camera turned, and what follows it must too. */
        void onViewCubeOrbited();
    }

    /** Diameter of the cube's disc. */
    static final int SIZE_DP = 112;
    /** A press travelling further than this orbits instead of choosing. */
    static final float DRAG_SLOP_DP = 8.0f;
    /** An end within 10 degrees of pointing at the eye already faces it. */
    static final float FACING_COS = 0.985f;

    private static final int[] AXIS_COLOURS = {0xFFE5534B, 0xFF57AB5A, 0xFF539BF5};
    private static final String[] LETTERS = {"X", "Y", "Z"};
    private static final int[][] VIEWS = {
            {NativeViewport.VIEW_RIGHT, NativeViewport.VIEW_LEFT},
            {NativeViewport.VIEW_TOP, NativeViewport.VIEW_BOTTOM},
            {NativeViewport.VIEW_FRONT, NativeViewport.VIEW_BACK}};
    private static final int[][] VIEW_NAMES = {
            {R.string.view_right, R.string.view_left},
            {R.string.view_top, R.string.view_bottom},
            {R.string.view_front, R.string.view_back}};

    private final OnViewChosen listener;
    private final float[] axes = new float[9];
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ring = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint letter = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint minorLetter = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final End[] ends = new End[6];
    private final float dragSlop;
    private float downX;
    private float downY;
    private float lastX;
    private float lastY;
    private boolean dragging;

    private static final class End {
        int axis;
        boolean positive;
        float depth;
        float x;
        float y;
    }

    WorkbenchViewCubeView(Context context, OnViewChosen listener) {
        super(context);
        this.listener = listener;
        setId(R.id.view_cube);
        setContentDescription(context.getString(R.string.view_cube_description));
        setClickable(true);
        dragSlop = EditorControlStyles.dp(context, (int) DRAG_SLOP_DP);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeCap(Paint.Cap.ROUND);
        stroke.setStrokeWidth(EditorControlStyles.dp(context, 3));
        ring.setStyle(Paint.Style.STROKE);
        ring.setStrokeWidth(Math.max(1.0f, 1.5f * context.getResources().getDisplayMetrics().density));
        final float sp = context.getResources().getDisplayMetrics().scaledDensity;
        letter.setTextAlign(Paint.Align.CENTER);
        letter.setFakeBoldText(true);
        letter.setTextSize(12.0f * sp);
        letter.setColor(0xFF12110F);
        minorLetter.setTextAlign(Paint.Align.CENTER);
        minorLetter.setFakeBoldText(true);
        minorLetter.setTextSize(9.0f * sp);
        for (int i = 0; i < ends.length; i++) {
            ends[i] = new End();
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final int size = EditorControlStyles.dp(getContext(), SIZE_DP);
        setMeasuredDimension(size, size);
    }

    /** The six ends on screen, far first, from the camera as it is now. */
    private void computeEnds() {
        if (!NativeViewport.cameraViewAxes(axes)) {
            java.util.Arrays.fill(axes, 0.0f);
        }
        final float cx = getWidth() * 0.5f;
        final float cy = getHeight() * 0.5f;
        final float reach = getWidth() * 0.5f * 0.62f;
        int i = 0;
        for (int a = 0; a < 3; a++) {
            for (int sign = 0; sign < 2; sign++) {
                final float k = sign == 0 ? 1.0f : -1.0f;
                final End end = ends[i++];
                end.axis = a;
                end.positive = sign == 0;
                // Rows: screen right (0..2), screen up (3..5), toward the eye (6..8).
                end.depth = k * axes[6 + a];
                end.x = cx + k * axes[a] * reach;
                end.y = cy - k * axes[3 + a] * reach;
            }
        }
        java.util.Arrays.sort(ends, new java.util.Comparator<End>() {
            @Override
            public int compare(End l, End r) {
                return Float.compare(l.depth, r.depth);
            }
        });
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        computeEnds();
        final Context context = getContext();
        final float cx = getWidth() * 0.5f;
        final float cy = getHeight() * 0.5f;
        final int ground = EditorControlStyles.themeColor(context, R.attr.fsSurfaceFloating);
        fill.setColor(ground);
        fill.setAlpha(150);
        canvas.drawCircle(cx, cy, getWidth() * 0.5f, fill);
        final float big = EditorControlStyles.dp(context, 11);
        final float small = EditorControlStyles.dp(context, 10);
        for (End end : ends) {
            final int colour = AXIS_COLOURS[end.axis];
            if (end.positive) {
                stroke.setColor(colour);
                canvas.drawLine(cx, cy, end.x, end.y, stroke);
                fill.setColor(colour);
                canvas.drawCircle(end.x, end.y, big, fill);
                canvas.drawText(LETTERS[end.axis], end.x,
                        end.y - (letter.descent() + letter.ascent()) * 0.5f, letter);
            } else {
                // A labelled RING at the axis colour: the opposite end is a
                // view too, and reads as one at rest (`WORKBENCH-UX-R1`).
                fill.setColor(ground);
                fill.setAlpha(235);
                canvas.drawCircle(end.x, end.y, small, fill);
                ring.setColor(colour);
                canvas.drawCircle(end.x, end.y, small, ring);
                minorLetter.setColor(colour);
                canvas.drawText("\u2212" + LETTERS[end.axis], end.x,
                        end.y - (minorLetter.descent() + minorLetter.ascent()) * 0.5f, minorLetter);
            }
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                downX = lastX = event.getX();
                downY = lastY = event.getY();
                dragging = false;
                getParent().requestDisallowInterceptTouchEvent(true);
                return true;
            case MotionEvent.ACTION_MOVE:
                if (!dragging && Math.hypot(event.getX() - downX, event.getY() - downY) > dragSlop) {
                    dragging = true;
                }
                if (dragging) {
                    // The cube is a handle on the view: dragging it turns the
                    // camera exactly as a mouse drag on the model does.
                    NativeViewport.cameraPointerOrbit(event.getX() - lastX, event.getY() - lastY);
                    lastX = event.getX();
                    lastY = event.getY();
                    listener.onViewCubeOrbited();
                }
                return true;
            case MotionEvent.ACTION_UP:
                if (!dragging) {
                    chooseAt(event.getX(), event.getY());
                }
                dragging = false;
                return true;
            case MotionEvent.ACTION_CANCEL:
                dragging = false;
                return true;
            default:
                return true;
        }
    }

    private void chooseAt(float x, float y) {
        computeEnds();
        End best = null;
        float bestDistance = Float.MAX_VALUE;
        // Near first, so of two ends on the same pixel the one in front wins.
        for (int i = ends.length - 1; i >= 0; i--) {
            final End end = ends[i];
            final float dx = x - end.x;
            final float dy = y - end.y;
            final float d = dx * dx + dy * dy;
            if (d < bestDistance - 1.0f) {
                bestDistance = d;
                best = end;
            }
        }
        if (best != null) {
            performClick();
            listener.onStandardViewChosen(viewFor(best.axis, best.positive, best.depth));
        }
    }

    /**
     * The view an end names, or its opposite when it already faces the eye:
     * the second tap on Top is Bottom, so every view is one tap away.
     */
    static int viewFor(int axis, boolean positive, float depth) {
        final boolean facing = depth > FACING_COS;
        return VIEWS[axis][(positive != facing) ? 0 : 1];
    }

    @Override
    public boolean performClick() {
        return super.performClick();
    }

    /** The screen point of an axis end in this view, for a test that taps it. */
    float[] endPoint(int axis, boolean positive) {
        computeEnds();
        for (End end : ends) {
            if (end.axis == axis && end.positive == positive) {
                return new float[] {end.x, end.y};
            }
        }
        return null;
    }

    // --- TalkBack: one virtual node per view ------------------------------
    //
    // A screen reader cannot aim at a 22 dp end drawn on a disc, so the cube
    // exposes its six views as virtual nodes with a click action each; their
    // bounds follow the ends as the camera turns.

    /** Virtual node id for an end: axis * 2 + (positive ? 0 : 1). */
    private static int nodeId(int axis, boolean positive) {
        return axis * 2 + (positive ? 0 : 1);
    }

    private End endForNode(int id) {
        computeEnds();
        for (End end : ends) {
            if (nodeId(end.axis, end.positive) == id) {
                return end;
            }
        }
        return null;
    }

    private final AccessibilityNodeProvider provider = new AccessibilityNodeProvider() {
        @Override
        public AccessibilityNodeInfo createAccessibilityNodeInfo(int virtualViewId) {
            if (virtualViewId == View.NO_ID) {
                final AccessibilityNodeInfo host = AccessibilityNodeInfo.obtain(WorkbenchViewCubeView.this);
                onInitializeAccessibilityNodeInfo(host);
                for (int id = 0; id < 6; id++) {
                    host.addChild(WorkbenchViewCubeView.this, id);
                }
                return host;
            }
            final End end = endForNode(virtualViewId);
            if (end == null) {
                return null;
            }
            final AccessibilityNodeInfo node =
                    AccessibilityNodeInfo.obtain(WorkbenchViewCubeView.this, virtualViewId);
            node.setParent(WorkbenchViewCubeView.this);
            node.setPackageName(getContext().getPackageName());
            node.setClassName("android.widget.Button");
            node.setContentDescription(getContext().getString(
                    VIEW_NAMES[end.axis][end.positive ? 0 : 1]));
            final int half = EditorControlStyles.dp(getContext(), 14);
            final Rect local = new Rect(Math.round(end.x) - half, Math.round(end.y) - half,
                    Math.round(end.x) + half, Math.round(end.y) + half);
            node.setBoundsInParent(local);
            final int[] at = new int[2];
            getLocationOnScreen(at);
            final Rect screen = new Rect(local);
            screen.offset(at[0], at[1]);
            node.setBoundsInScreen(screen);
            node.setVisibleToUser(isShown());
            node.setEnabled(true);
            node.setClickable(true);
            node.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK);
            return node;
        }

        @Override
        public boolean performAction(int virtualViewId, int action, Bundle arguments) {
            if (virtualViewId == View.NO_ID) {
                return performAccessibilityAction(action, arguments);
            }
            if (action != AccessibilityNodeInfo.ACTION_CLICK || virtualViewId < 0 || virtualViewId > 5) {
                return false;
            }
            // The NAMED view, never its opposite: a screen reader names a
            // view and gets exactly that one.
            listener.onStandardViewChosen(VIEWS[virtualViewId / 2][virtualViewId % 2]);
            sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_CLICKED);
            return true;
        }
    };

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        return provider;
    }

    /** The virtual node count TalkBack reads, for verification. */
    int accessibilityViewCount() {
        final AccessibilityNodeInfo host = provider.createAccessibilityNodeInfo(View.NO_ID);
        return host == null ? 0 : host.getChildCount();
    }
}
