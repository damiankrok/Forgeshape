package com.forgeshape.app;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

import java.util.Arrays;
import java.util.Comparator;

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
 */
final class WorkbenchViewCubeView extends View {

    interface OnViewChosen {
        void onStandardViewChosen(int view);
    }

    /** Diameter of the cube's disc. */
    static final int SIZE_DP = 112;

    private static final int[] AXIS_COLOURS = {0xFFE5534B, 0xFF57AB5A, 0xFF539BF5};
    private static final String[] LETTERS = {"X", "Y", "Z"};
    private static final int[][] VIEWS = {
            {NativeViewport.VIEW_RIGHT, NativeViewport.VIEW_LEFT},
            {NativeViewport.VIEW_TOP, NativeViewport.VIEW_BOTTOM},
            {NativeViewport.VIEW_FRONT, NativeViewport.VIEW_BACK}};

    private final OnViewChosen listener;
    private final float[] axes = new float[9];
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint letter = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final End[] ends = new End[6];

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
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeCap(Paint.Cap.ROUND);
        stroke.setStrokeWidth(EditorControlStyles.dp(context, 3));
        letter.setTextAlign(Paint.Align.CENTER);
        letter.setFakeBoldText(true);
        letter.setTextSize(EditorControlStyles.dp(context, 12));
        letter.setColor(0xFF12110F);
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
            Arrays.fill(axes, 0.0f);
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
        Arrays.sort(ends, new Comparator<End>() {
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
        fill.setColor(EditorControlStyles.themeColor(context, R.attr.fsSurfaceFloating));
        fill.setAlpha(150);
        canvas.drawCircle(cx, cy, getWidth() * 0.5f, fill);
        final float big = EditorControlStyles.dp(context, 11);
        final float small = EditorControlStyles.dp(context, 7);
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
                fill.setColor(colour);
                fill.setAlpha(115);
                canvas.drawCircle(end.x, end.y, small, fill);
            }
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            return true;
        }
        if (event.getActionMasked() != MotionEvent.ACTION_UP) {
            return true;
        }
        computeEnds();
        End best = null;
        float bestDistance = Float.MAX_VALUE;
        // Near first, so of two ends on the same pixel the one in front wins.
        for (int i = ends.length - 1; i >= 0; i--) {
            final End end = ends[i];
            final float dx = event.getX() - end.x;
            final float dy = event.getY() - end.y;
            final float d = dx * dx + dy * dy;
            if (d < bestDistance - 1.0f) {
                bestDistance = d;
                best = end;
            }
        }
        if (best != null) {
            performClick();
            listener.onStandardViewChosen(VIEWS[best.axis][best.positive ? 0 : 1]);
        }
        return true;
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
}
