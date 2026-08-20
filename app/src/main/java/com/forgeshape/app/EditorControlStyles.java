package com.forgeshape.app;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The one place the Editor Workspace's controls get their look.
 *
 * <p>It exists because the two provisional panels each carried a private copy
 * of the same background, padding and colour helpers, and the copies had
 * already started to differ. Every metric and colour now comes from resources,
 * so a control's size is arguable in {@code dimens.xml} rather than buried in a
 * view constructor.
 *
 * <p>Styling only. Nothing here reads or writes product state, makes a native
 * call, or knows what any control means.
 */
final class EditorControlStyles {

    private EditorControlStyles() {
    }

    static int dp(Context context, int value) {
        return Math.round(context.getResources().getDisplayMetrics().density * value);
    }

    static int dimen(Context context, int dimenRes) {
        return context.getResources().getDimensionPixelSize(dimenRes);
    }

    /** Converts a raw window pixel extent to density-independent pixels. */
    static int toDp(Context context, int pixels) {
        final float density = context.getResources().getDisplayMetrics().density;
        return density > 0.0f ? Math.round(pixels / density) : pixels;
    }

    // -----------------------------------------------------------------------
    // Backgrounds
    // -----------------------------------------------------------------------

    /** Opaque chrome: text sits on it, so it may not be translucent. */
    static GradientDrawable chromeSurface(Context context) {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(context.getColor(R.color.chrome_surface));
        shape.setStroke(dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.chrome_hairline));
        return shape;
    }

    /**
     * Chrome that stands on the viewport.
     *
     * <p>Translucent on purpose and only here: the model behind a rail is
     * informative, and a rail is narrow enough that its labels stay legible
     * over it. Sheets and docks carry paragraphs and stay opaque.
     */
    static GradientDrawable chromeOverlay(Context context) {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(context.getColor(R.color.chrome_overlay));
        shape.setCornerRadius(dimen(context, R.dimen.control_corner));
        shape.setStroke(dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.chrome_hairline));
        return shape;
    }

    /**
     * A control's background in its idle, active or disabled state.
     *
     * <p>Active is never colour alone: a filled accent <i>and</i> a thicker
     * accent border <i>and</i> a brightened label, so the current tool is
     * unmistakable to someone who cannot separate the two blues.
     */
    static GradientDrawable controlBackground(Context context, boolean active) {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(context.getColor(active ? R.color.accent_fill : R.color.control_surface));
        shape.setCornerRadius(dimen(context, R.dimen.control_corner));
        shape.setStroke(
                dimen(context, active ? R.dimen.control_border_active_width
                                      : R.dimen.control_border_width),
                context.getColor(active ? R.color.accent_border : R.color.control_border));
        return shape;
    }

    /** The accent background for a primary commit (Apply, Freeze, Resume). */
    static StateListDrawable primaryBackground(Context context) {
        final GradientDrawable idle = new GradientDrawable();
        idle.setColor(context.getColor(R.color.accent_fill));
        idle.setCornerRadius(dimen(context, R.dimen.control_corner));
        idle.setStroke(dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.accent_border));

        final GradientDrawable pressed = new GradientDrawable();
        pressed.setColor(context.getColor(R.color.accent));
        pressed.setCornerRadius(dimen(context, R.dimen.control_corner));
        pressed.setStroke(dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.accent));

        final StateListDrawable states = new StateListDrawable();
        states.addState(new int[]{android.R.attr.state_pressed}, pressed);
        states.addState(new int[]{}, idle);
        return states;
    }

    static GradientDrawable fieldBackground(Context context) {
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(context.getColor(R.color.control_surface));
        shape.setCornerRadius(dimen(context, R.dimen.control_corner));
        shape.setStroke(dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.control_border));
        return shape;
    }

    // -----------------------------------------------------------------------
    // Text controls
    // -----------------------------------------------------------------------

    static TextView sectionLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, R.dimen.text_label));
        label.setTextColor(context.getColor(R.color.text_secondary));
        label.setAllCaps(true);
        label.setLetterSpacing(0.08f);
        return label;
    }

    static TextView fieldLabel(Context context, CharSequence text) {
        final TextView label = new TextView(context);
        label.setText(text);
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, R.dimen.text_label));
        label.setTextColor(context.getColor(R.color.text_secondary));
        return label;
    }

    /**
     * A tappable pill: a unit, a primitive, a reserved entry.
     *
     * <p>{@code TextView} rather than {@code Button} so the height is exactly
     * the touch target requested and not the platform button's own minimum plus
     * insets, which is what made the previous rows wider than they looked.
     */
    static TextView chip(Context context, int id, CharSequence text) {
        final TextView chip = new TextView(context);
        chip.setId(id);
        chip.setText(text);
        chip.setContentDescription(text);
        chip.setGravity(Gravity.CENTER);
        chip.setSingleLine(true);
        chip.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, R.dimen.text_body));
        chip.setTextColor(context.getColor(R.color.text_primary));
        final int padding = dimen(context, R.dimen.chip_padding_horizontal);
        chip.setPadding(padding, 0, padding, 0);
        chip.setMinimumHeight(dimen(context, R.dimen.control_height));
        chip.setBackground(controlBackground(context, false));
        chip.setClickable(true);
        chip.setFocusable(true);
        return chip;
    }

    /** Repaints a chip for its selected state, background and label together. */
    static void setChipActive(TextView chip, boolean active) {
        final Context context = chip.getContext();
        chip.setBackground(controlBackground(context, active));
        chip.setTextColor(context.getColor(active ? R.color.text_primary : R.color.text_secondary));
        chip.setActivated(active);
    }

    /** A chip that names something the product does not have yet. */
    static void setChipReserved(TextView chip, CharSequence reason) {
        final Context context = chip.getContext();
        chip.setEnabled(false);
        chip.setTextColor(context.getColor(R.color.text_disabled));
        final GradientDrawable shape = new GradientDrawable();
        shape.setColor(context.getColor(R.color.control_surface));
        shape.setCornerRadius(dimen(context, R.dimen.control_corner));
        shape.setStroke(dimen(context, R.dimen.control_border_width),
                context.getColor(R.color.text_disabled));
        chip.setBackground(shape);
        chip.setContentDescription(reason);
    }

    /** A primary commit button. */
    static TextView primaryButton(Context context, int id, CharSequence text) {
        final TextView button = chip(context, id, text);
        button.setBackground(primaryBackground(context));
        button.setTextColor(context.getColor(R.color.text_primary));
        final int padding = dimen(context, R.dimen.control_padding_horizontal);
        button.setPadding(padding, 0, padding, 0);
        return button;
    }

    static TextView statusText(Context context, int id) {
        final TextView status = new TextView(context);
        status.setId(id);
        status.setTextSize(TypedValue.COMPLEX_UNIT_PX, dimen(context, R.dimen.text_status));
        status.setTextColor(context.getColor(R.color.text_secondary));
        return status;
    }

    // -----------------------------------------------------------------------
    // Layout params
    // -----------------------------------------------------------------------

    static LinearLayout.LayoutParams rowParams(int topMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        return params;
    }

    /** An equal share of a horizontal row, with a gap before all but the first. */
    static LinearLayout.LayoutParams evenShare(int leftMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        params.leftMargin = leftMargin;
        return params;
    }

    static LinearLayout.LayoutParams wrap(int leftMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.leftMargin = leftMargin;
        return params;
    }

    /** A flexible gap that pushes what follows to the far end of a row. */
    static View spacer(Context context) {
        final View spacer = new View(context);
        spacer.setLayoutParams(new LinearLayout.LayoutParams(0, 1, 1.0f));
        return spacer;
    }
}
