package com.forgeshape.app;

import android.content.Context;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;

import java.math.BigDecimal;

/**
 * One labelled exact-value field.
 *
 * <p>The smallest reusable piece of the Property Inspector: a caption over an
 * {@link EditText} that accepts a signed decimal. Every exact value in the
 * product — a box width, a cone height, a position coordinate, a rotation angle
 * — is one of these, which is why the keyboard configuration lives here once
 * instead of being repeated per field.
 *
 * <p><b>Holds no value.</b> The text is a draft; the authoritative number is a
 * native double in meters or degrees. This row parses and reports, and the
 * caller decides what to do about it.
 */
final class NumericPropertyRow extends LinearLayout {

    private final EditText field;
    private final String label;

    /**
     * @param fieldId  the stable semantic id verification names this field by
     * @param label    the caption, also the field's content description and the
     *                 name any rejection message will use, so the user is told
     *                 about the field they can see
     * @param lastInRow whether the IME's action key should commit rather than
     *                 advance — the last field of a section has nowhere to go
     */
    NumericPropertyRow(Context context, int fieldId, String label, boolean lastInRow) {
        super(context);
        this.label = label;
        setOrientation(VERTICAL);

        addView(EditorControlStyles.fieldLabel(context, label));

        field = new EditText(context);
        field.setId(fieldId);
        // A numeric keyboard, but with a key listener that also accepts a comma
        // decimal separator and a minus sign. setRawInputType is what keeps
        // both: it tells the IME what to show without replacing the key
        // listener the way setInputType would. The minus sign matters twice
        // over — a negative coordinate or angle is an ordinary value that must
        // be typeable, and a negative dimension must be typeable so it can be
        // visibly refused rather than unreachable.
        field.setKeyListener(DigitsKeyListener.getInstance("0123456789.,-"));
        field.setRawInputType(InputType.TYPE_CLASS_NUMBER
                | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | InputType.TYPE_NUMBER_FLAG_SIGNED);
        field.setSingleLine(true);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_value));
        field.setTextColor(context.getColor(R.color.text_primary));
        // The background carries a focused state, so which field the keyboard
        // is about to type into is readable from its border rather than from
        // the caret alone.
        field.setBackgroundResource(R.drawable.bg_field);
        final int paddingX =
                EditorControlStyles.dimen(context, R.dimen.field_padding_horizontal);
        final int paddingY =
                EditorControlStyles.dimen(context, R.dimen.field_padding_vertical);
        field.setPadding(paddingX, paddingY, paddingX, paddingY);
        field.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        field.setImeOptions(lastInRow ? EditorInfo.IME_ACTION_DONE : EditorInfo.IME_ACTION_NEXT);
        field.setContentDescription(label);

        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        addView(field, params);
    }

    EditText field() {
        return field;
    }

    String label() {
        return label;
    }

    String text() {
        return field.getText().toString();
    }

    void setText(CharSequence text) {
        field.setText(text);
    }

    /** Puts the caret in this field and selects it, so a correction is one type. */
    void focusForCorrection() {
        field.requestFocus();
        field.selectAll();
    }

    void clearEditFocus() {
        field.clearFocus();
    }

    /**
     * Re-expresses this field's text in another unit, exactly.
     *
     * <p>Text that is not a number is left exactly as the user typed it:
     * silently rewriting or discarding it would be worse than showing it
     * unconverted, and Apply will report it anyway.
     *
     * @return whether the text was a number and was converted
     */
    boolean convertUnit(LengthUnit from, LengthUnit to) {
        try {
            final BigDecimal value = LengthUnit.parse(text());
            setText(LengthUnit.present(to.convertFrom(from, value)));
            return true;
        } catch (NumberFormatException notANumber) {
            return false;
        }
    }
}
