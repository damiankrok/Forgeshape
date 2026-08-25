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
 *
 * <p><b>Each column is one variable, read top to bottom.</b> The caption names
 * it, the value directly under the caption says what it is, and the track under
 * both is how it is changed. Before UI-R4B the value sat above the track and the
 * caption below it, so the two halves of one fact were separated by 200 dp of
 * slider and the eye had to pair "Radius" with a number at the other end of the
 * column — which is why the readouts looked like two unlabelled gauges with
 * captions underneath rather than like two named values.
 *
 * <p><b>The live value is here and nowhere else.</b> Dragging a slider used to
 * also write "Brush: radius 120 px, strength 0.45." into the workspace's status
 * line, so the same two numbers were on screen twice, one of them at the top of
 * the window where the user is not looking, and the last drag's numbers then sat
 * there for the rest of the session. A value being dragged belongs beside the
 * thing dragging it.
 */
final class BrushEdgeControlsView extends LinearLayout {

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

    /** Set while this class drives a slider, so it does not react to itself. */
    private boolean writingBack;

    BrushEdgeControlsView(Context context) {
        super(context);
        setId(R.id.brush_edge_controls);
        setOrientation(HORIZONTAL);
        // TIER 1, exactly like the Tool Rail it sits opposite and the toolbar's
        // control groups above it. That is what stops these two reading as a
        // debug overlay bolted onto the viewport: they are the same material,
        // the same radius and the same depth as every other floating control
        // group, so they belong to the workspace rather than to a diagnostic.
        EditorControlStyles.applyFloatingSurface(this);
        final int padding = EditorControlStyles.dimen(context, R.dimen.brush_gap);
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

    /**
     * One column: a caption, the value it names directly beneath it, then the
     * track that changes it.
     *
     * <p>The order is the whole correction. Caption and value are <b>one
     * header</b>, adjacent and tight, so "Radius" and "120 px" are read as a
     * single fact; the track follows because it is how that fact is changed, not
     * part of what it says. The previous arrangement — value, track, caption —
     * put 200 dp of slider between the two halves of one variable, and the eye
     * paired each number with the caption of the other column as often as not.
     *
     * <p>Both columns are built by this one method, so their captions sit on one
     * baseline and their values on another by construction rather than by two
     * sets of margins agreeing.
     */
    private LinearLayout column(Context context, TextView value, VerticalSliderView slider,
                                LinearLayout.LayoutParams trackParams, String caption) {
        final LinearLayout column = new LinearLayout(context);
        column.setOrientation(VERTICAL);
        column.setGravity(Gravity.CENTER_HORIZONTAL);

        final TextView label = new TextView(context);
        // The caption in full — "Radius", not "RAD".
        //
        // It was truncated to three upper-case letters on the grounds that the
        // column is as narrow as a thumb, but both words fit the column at this
        // size, and the abbreviations read as register names rather than as the
        // two things a sculptor adjusts most. Cutting a localized string with
        // substring(0, 3) was also a hazard of its own: it is not a translation
        // rule in any language, and it throws outright on a caption shorter than
        // three characters.
        label.setText(caption);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.rail_label_size));
        label.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextSecondary));
        label.setGravity(Gravity.CENTER);
        label.setSingleLine(true);
        // WRAP_CONTENT and NOT ellipsised. The caption and the live value are
        // what decide this column's width, and both have to be readable in full:
        // a truncated "Stren…" over a truncated "120 p…" is the register-name
        // look UI-R3 already removed once.
        final LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        labelParams.gravity = Gravity.CENTER_HORIZONTAL;
        column.addView(label, labelParams);

        column.addView(value);
        column.addView(slider, trackParams);
        return column;
    }

    /**
     * The live value, directly under the caption that names it.
     *
     * <p>Body-sized and in the primary text colour rather than a tiny amber
     * readout. Amber is the MEASUREMENT role and belongs to a typed dimension in
     * the Property Inspector; spending it on a slider's current position made
     * the two columns read as an instrument panel taped to the viewport. Here
     * the number is simply the loudest thing in its own column, which is what a
     * value being dragged should be — and the caption above it is quiet and
     * small, so the pair reads as one heading rather than as two labels.
     *
     * <p>It tracks the pointer 1:1 and is never animated or smoothed: a brush
     * radius that eased into place would be lying about what the next stroke
     * will do.
     */
    private TextView valueLabel(Context context, int id) {
        final TextView label = new TextView(context);
        label.setId(id);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        label.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        EditorControlStyles.applyMediumWeight(label);
        label.setGravity(Gravity.CENTER);
        label.setSingleLine(true);
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER_HORIZONTAL;
        // Tight to the caption above, clear of the track below: the gap is what
        // says which two of the three things in this column are one fact.
        params.topMargin = 0;
        params.bottomMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        label.setLayoutParams(params);
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
        // No listener, and nothing above this is told. What changed is on screen
        // in this control, 1:1 with the pointer; the workspace's status line has
        // nothing to add and used to hold the last drag's numbers indefinitely.
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

    /**
     * The radius as it is written under its caption.
     *
     * <p>Still pixels, deliberately. The brush radius IS a screen radius in this
     * domain — it is the size of the region a finger affects, which is a
     * property of the gesture rather than of the model, and it does not change
     * when the camera moves. Re-expressing it in metres would be a change to the
     * domain contract, and this stage does not redesign brush units.
     */
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
