package com.forgeshape.app;

import android.content.Context;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Radius and Strength, mounted at the edge of the viewport.
 *
 * <p>These are the two values a sculptor changes constantly and mid-stroke, so
 * they are <b>direct</b>: always on screen, always live, never behind a panel
 * that has to be opened first. That is the one place the Sculpt shell differs
 * most from the Construction shell, and it is why they are not in the Property
 * Inspector with everything else.
 *
 * <p>They sit opposite the Tool Rail so neither hand covers the other's
 * control.
 *
 * <p><b>Holds no brush value.</b> Both sliders submit a request through
 * {@link NativeViewport#setSculptBrush}, which clamps, and both labels are then
 * written from what native code reports — never from the slider's own opinion
 * of what it asked for.
 */
final class BrushEdgeControlsView extends LinearLayout {

    /** Told the brush changed, after native code has been asked and read back. */
    interface OnBrushChanged {
        void onBrushChanged();
    }

    /**
     * The brush ranges, mirroring the native clamp limits so the slider ends
     * line up with what native code will actually accept. Native remains the
     * authority: it clamps whatever arrives, and this view reads the result
     * back rather than assuming its own mapping was honoured.
     */
    private static final double MIN_RADIUS_PIXELS = 24.0;
    private static final double MAX_RADIUS_PIXELS = 600.0;
    private static final double MIN_STRENGTH = 0.05;
    private static final double MAX_STRENGTH = 1.0;

    private final VerticalSliderView radiusSlider;
    private final VerticalSliderView strengthSlider;
    private final TextView radiusValue;
    private final TextView strengthValue;
    private final LinearLayout.LayoutParams radiusTrackParams;
    private final LinearLayout.LayoutParams strengthTrackParams;

    /** Reused across reads; native fills it with the authoritative brush. */
    private final double[] nativeState = new double[NativeViewport.SCULPT_STATE_SIZE];

    private final OnBrushChanged listener;

    /** Set while this class drives a slider, so it does not react to itself. */
    private boolean writingBack;

    BrushEdgeControlsView(Context context, OnBrushChanged listener) {
        super(context);
        this.listener = listener;
        setId(R.id.brush_edge_controls);
        setOrientation(HORIZONTAL);
        EditorControlStyles.applyFloatingSurface(this);
        final int padding = EditorControlStyles.dimen(context, R.dimen.rail_padding);
        setPadding(padding, padding, padding, padding);

        radiusValue = valueLabel(context, R.id.brush_radius_value);
        radiusSlider = new VerticalSliderView(context, R.id.brush_radius_slider,
                context.getString(R.string.brush_radius));
        radiusTrackParams = trackParams(context);
        addView(column(context, radiusValue, radiusSlider, radiusTrackParams,
                context.getString(R.string.brush_radius)), columnParams(context, 0));

        strengthValue = valueLabel(context, R.id.brush_strength_value);
        strengthSlider = new VerticalSliderView(context, R.id.brush_strength_slider,
                context.getString(R.string.brush_strength));
        strengthTrackParams = trackParams(context);
        addView(column(context, strengthValue, strengthSlider, strengthTrackParams,
                        context.getString(R.string.brush_strength)),
                columnParams(context, EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        final VerticalSliderView.OnFractionChanged onMoved =
                new VerticalSliderView.OnFractionChanged() {
                    @Override
                    public void onFractionChanged(float fraction, boolean fromUser) {
                        if (writingBack || !fromUser) {
                            return;
                        }
                        submitBrush();
                    }
                };
        radiusSlider.setOnFractionChanged(onMoved);
        strengthSlider.setOnFractionChanged(onMoved);
    }

    /**
     * The slider's touch column, which is deliberately much wider than the
     * track it draws.
     *
     * <p>Named in {@code dimens.xml} rather than derived from the track width,
     * so restyling the track cannot silently shrink the hit area below what a
     * thumb can find.
     */
    private LinearLayout.LayoutParams trackParams(Context context) {
        return new LinearLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.brush_touch_width),
                EditorControlStyles.dimen(context, R.dimen.brush_slider_height));
    }

    private LinearLayout.LayoutParams columnParams(Context context, int leftMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.leftMargin = leftMargin;
        return params;
    }

    private LinearLayout column(Context context, TextView value, VerticalSliderView slider,
                                LinearLayout.LayoutParams trackParams, String caption) {
        final LinearLayout column = new LinearLayout(context);
        column.setOrientation(VERTICAL);
        column.setGravity(Gravity.CENTER_HORIZONTAL);
        column.addView(value);
        column.addView(slider, trackParams);

        final TextView label = new TextView(context);
        // Three letters, because the column is as narrow as a thumb and the
        // value chip above already says what the number is.
        label.setText(caption.substring(0, 3).toUpperCase(java.util.Locale.US));
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.rail_label_size));
        label.setTextColor(context.getColor(R.color.text_secondary));
        label.setGravity(Gravity.CENTER);
        column.addView(label);
        return column;
    }

    private TextView valueLabel(Context context, int id) {
        final TextView label = new TextView(context);
        label.setId(id);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.rail_label_size));
        label.setTextColor(context.getColor(R.color.text_measure));
        label.setGravity(Gravity.CENTER);
        label.setSingleLine(true);
        return label;
    }

    /**
     * Shortens both tracks so the controls fit a window with little height.
     *
     * <p>A shorter track is coarser, not absent: losing the brush controls in
     * landscape would be the same class of defect this stage exists to fix.
     */
    void setTrackHeightPx(int heightPx) {
        final int minimum = EditorControlStyles.dimen(getContext(),
                R.dimen.brush_slider_min_height);
        final int preferred = EditorControlStyles.dimen(getContext(),
                R.dimen.brush_slider_height);
        final int height = Math.max(minimum, Math.min(preferred, heightPx));
        if (radiusTrackParams.height == height) {
            return;
        }
        radiusTrackParams.height = height;
        strengthTrackParams.height = height;
        requestLayout();
    }

    private double radiusFromSlider() {
        return MIN_RADIUS_PIXELS
                + radiusSlider.fraction() * (MAX_RADIUS_PIXELS - MIN_RADIUS_PIXELS);
    }

    private double strengthFromSlider() {
        return MIN_STRENGTH + strengthSlider.fraction() * (MAX_STRENGTH - MIN_STRENGTH);
    }

    private static float fractionFor(double value, double min, double max) {
        return (float) ((value - min) / (max - min));
    }

    /**
     * Submits both values together and writes back what native code kept.
     *
     * <p>This changes no geometry: the brush takes effect on the next stroke,
     * and a stroke already in progress keeps the radius and the affected set it
     * captured when it started.
     */
    private void submitBrush() {
        NativeViewport.setSculptBrush(radiusFromSlider(), strengthFromSlider());
        refreshFromNative();
        if (listener != null) {
            listener.onBrushChanged();
        }
    }

    /** Rewrites both sliders and both labels from authoritative native state. */
    void refreshFromNative() {
        NativeViewport.sculptState(nativeState);
        writingBack = true;
        radiusSlider.setFraction(fractionFor(nativeState[NativeViewport.SCULPT_RADIUS_PIXELS],
                MIN_RADIUS_PIXELS, MAX_RADIUS_PIXELS));
        strengthSlider.setFraction(fractionFor(nativeState[NativeViewport.SCULPT_STRENGTH],
                MIN_STRENGTH, MAX_STRENGTH));
        writingBack = false;
        radiusValue.setText(describeRadius());
        strengthValue.setText(describeStrength());
    }

    String describeRadius() {
        return Math.round(nativeState[NativeViewport.SCULPT_RADIUS_PIXELS]) + " px";
    }

    String describeStrength() {
        return String.format(java.util.Locale.US, "%.2f",
                nativeState[NativeViewport.SCULPT_STRENGTH]);
    }

    /**
     * Swallows any touch that lands between the two tracks.
     *
     * <p>The sliders own their own gestures; this catches the padding around
     * them, which is over the viewport and would otherwise deform the model.
     */
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return true;
    }
}
