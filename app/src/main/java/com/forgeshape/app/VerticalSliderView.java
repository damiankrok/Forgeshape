package com.forgeshape.app;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.view.MotionEvent;
import android.view.View;

/**
 * A vertical continuous control for one brush value.
 *
 * <p>Written rather than assembled from a rotated {@code SeekBar} for one
 * reason: a rotated widget's touch coordinates and its drawn track stop
 * agreeing at the edges, and these two controls sit at a screen edge and are
 * dragged with a thumb or a stylus tip. Here the mapping is one line and the
 * hit area is the whole view, which is deliberately much wider than the track.
 *
 * <p><b>Holds no brush.</b> The fraction is a slider position; the brush is a
 * native double that native code clamps. The caller submits the request and
 * writes back whatever native code actually kept.
 *
 * <p>The fraction runs 0 at the bottom to 1 at the top, matching the direction
 * the value grows on screen.
 */
final class VerticalSliderView extends View {

    /** Told the position changed, and whether a finger caused it. */
    interface OnFractionChanged {
        void onFractionChanged(float fraction, boolean fromUser);
    }

    private final Paint trackPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint thumbPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint thumbBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    private final float trackWidth;
    private final float thumbRadius;

    private float fraction;
    private OnFractionChanged listener;

    VerticalSliderView(Context context, int viewId, CharSequence description) {
        super(context);
        setId(viewId);
        setContentDescription(description);
        setClickable(true);
        setFocusable(true);

        trackWidth = EditorControlStyles.dimen(context, R.dimen.brush_track_width);
        thumbRadius = EditorControlStyles.dimen(context, R.dimen.brush_thumb_radius);

        trackPaint.setColor(EditorControlStyles.themeColor(context, R.attr.fsSliderTrack));
        fillPaint.setColor(EditorControlStyles.themeColor(context, R.attr.fsSliderFill));
        // Its own role rather than the body-text colour: a thumb has to stand
        // off ITS TRACK, and on a light theme the track is pale, so the answer
        // is a dark thumb — the opposite of what a text colour would give.
        thumbPaint.setColor(EditorControlStyles.themeColor(context, R.attr.fsSliderThumb));
        thumbBorderPaint.setColor(EditorControlStyles.themeColor(context, R.attr.fsAccent));
        thumbBorderPaint.setStyle(Paint.Style.STROKE);
        thumbBorderPaint.setStrokeWidth(
                EditorControlStyles.dimen(context, R.dimen.control_border_active_width));
    }

    void setOnFractionChanged(OnFractionChanged listener) {
        this.listener = listener;
    }

    float fraction() {
        return fraction;
    }

    /**
     * Moves the slider without telling the listener a user did it.
     *
     * <p>Used when the panel writes back what native code kept, so a clamped
     * value cannot echo back down as a fresh request.
     */
    void setFraction(float value) {
        final float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        if (clamped != fraction) {
            fraction = clamped;
            invalidate();
        }
    }

    private float travel() {
        return Math.max(1.0f, getHeight() - 2.0f * thumbRadius);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        final float centerX = getWidth() * 0.5f;
        final float top = thumbRadius;
        final float bottom = getHeight() - thumbRadius;
        final float half = trackWidth * 0.5f;
        final float radius = half;

        canvas.drawRoundRect(centerX - half, top - half, centerX + half, bottom + half,
                radius, radius, trackPaint);

        final float thumbY = bottom - fraction * travel();
        canvas.drawRoundRect(centerX - half, thumbY - half, centerX + half, bottom + half,
                radius, radius, fillPaint);

        canvas.drawCircle(centerX, thumbY, thumbRadius, thumbPaint);
        canvas.drawCircle(centerX, thumbY, thumbRadius, thumbBorderPaint);
    }

    /**
     * Owns the whole gesture from the moment a pointer lands on it.
     *
     * <p>Returning true on every action is what keeps a brush adjustment from
     * reaching the {@code SurfaceView} underneath and sculpting the model, and
     * the disallow-intercept request keeps an enclosing scroll container from
     * stealing the drag halfway through.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                if (getParent() != null) {
                    getParent().requestDisallowInterceptTouchEvent(true);
                }
                updateFromTouch(event.getY());
                return true;
            case MotionEvent.ACTION_MOVE:
                updateFromTouch(event.getY());
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (getParent() != null) {
                    getParent().requestDisallowInterceptTouchEvent(false);
                }
                return true;
            default:
                return true;
        }
    }

    private void updateFromTouch(float y) {
        final float bottom = getHeight() - thumbRadius;
        float value = (bottom - y) / travel();
        value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
        if (value != fraction) {
            fraction = value;
            invalidate();
        }
        if (listener != null) {
            listener.onFractionChanged(fraction, true);
        }
    }
}
