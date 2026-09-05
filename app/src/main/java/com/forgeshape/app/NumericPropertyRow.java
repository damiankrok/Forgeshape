package com.forgeshape.app;

import android.content.Context;
import android.text.Editable;
import android.text.InputType;
import android.text.TextPaint;
import android.text.TextWatcher;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.View;
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
 *
 * <h2>The complete value and what is drawn are two different strings</h2>
 *
 * <p>A single-line field scrolls to the caret, so a value too long for its
 * column clips at the LEFT: {@code -98765.4321098} in a 117 dp column would
 * read {@code 765.4321098} — a plausible POSITIVE number with nothing saying
 * anything is missing. Losing the sign and the leading digits of a coordinate
 * is the worst place for a truncation to be silent. So this row keeps
 * {@link #value}, the complete text — what the caller set or the user typed,
 * to the last digit — and draws a PRESENTATION of it:
 *
 * <ul>
 *   <li><b>The type shrinks before anything is dropped.</b> A value too long for
 *       its column is set one step smaller, down to the caption size, which is
 *       enough to hold a signed 14-digit coordinate in a 117 dp field. Smaller
 *       type is a presentation; a missing minus sign is a different number.</li>
 *   <li><b>Editing</b> shows the complete value, and a field that gains focus
 *       selects all of it, so the first keystroke replaces rather than appends.
 *       Correcting one digit is still a tap on that digit.</li>
 *   <li><b>Not editing</b>, if it still does not fit, shows as much as does,
 *       shortened from the END with a single ellipsis. The sign and the leading
 *       digits — the magnitude — are what survives, because they are what a
 *       glance is reading.</li>
 * </ul>
 *
 * <p><b>Nothing about the stored precision changes.</b> {@link #text()} returns
 * the complete value in every state, so Apply submits exactly what the user
 * typed: the 15-significant-digit round trip through this field is a property of
 * the value, and the ellipsis is a property of the drawing. A shortened display
 * is never parsed and never submitted.
 */
final class NumericPropertyRow extends LinearLayout {

    /** What a shortened display ends with. One character, so what fits is
     *  measured exactly rather than guessed at a font's ellipsis glyph. */
    private static final String ELLIPSIS = "…";

    private final EditText field;
    private final String label;

    /**
     * The complete text: what the caller set, or what the user typed. The
     * authoritative draft. What is DRAWN may be shorter — see the class comment.
     */
    private String value = "";

    /** True while this class is rewriting the field, so its own writes are not
     *  read back as the user typing. */
    private boolean rendering;

    /** The value role, and the caption role: the band the field type may move in
     *  to keep a long number whole. See applyTextSizeFor. */
    private final float baseTextSizePx;
    private final float minTextSizePx;

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
        baseTextSizePx = EditorControlStyles.dimen(context, R.dimen.text_value);
        minTextSizePx = EditorControlStyles.dimen(context, R.dimen.text_caption);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX, baseTextSizePx);
        field.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
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

        // The first keystroke REPLACES. See the class comment: a caret placed in
        // a populated field is how 0 became 0-98765.4321098.
        field.setSelectAllOnFocus(true);
        field.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (!rendering) {
                    // The user typed, or a caller wrote to the EditText itself.
                    // Either way what is on screen IS the complete value now.
                    value = s.toString();
                    // Re-fit as it is typed, not only when it is set: the field
                    // that has to hold a long coordinate whole is exactly the one
                    // being typed into. Only the SIZE is touched here — rewriting
                    // the text under a caret is what an editor must never do.
                    applyTextSizeFor(value);
                }
            }
        });
        field.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                // Focus gained: the complete value comes back, so editing is
                // always editing the whole number and never an ellipsis. Focus
                // lost: it is shortened again if it does not fit.
                render();
                if (hasFocus) {
                    field.selectAll();
                }
            }
        });
        // What fits depends on the width, which is not known until the row has
        // been laid out — and changes again on a rotation, a font-scale change
        // or a move between placements. Posted rather than applied inline:
        // writing text during a layout pass schedules another one.
        field.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                                       int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (right - left != oldRight - oldLeft) {
                    v.post(new Runnable() {
                        @Override
                        public void run() {
                            render();
                        }
                    });
                }
            }
        });

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

    /** The COMPLETE value, whatever is currently drawn. See the class comment. */
    String text() {
        return value;
    }

    void setText(CharSequence text) {
        value = text == null ? "" : text.toString();
        render();
    }

    /**
     * Whether the drawn text is a shortened form of the complete value.
     *
     * <p>For verification: a case proving a long value keeps its sign has to be
     * able to tell "it fitted" from "it was shortened and still starts with the
     * sign", because only the second is the interesting one.
     */
    boolean displayIsShortened() {
        return !value.contentEquals(field.getText());
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
     * Draws the value: complete while editing, shortened from the end otherwise.
     *
     * <p>Writes nothing when the string it would write is already there, so a
     * caret or a selection the user placed is never reset by a redraw.
     */
    private void render() {
        // Size first, then what to draw: a smaller face may make the whole value
        // fit, and then there is nothing to shorten.
        applyTextSizeFor(value);
        final String shown = field.hasFocus() ? value : fitToField(value);
        if (shown.contentEquals(field.getText())) {
            return;
        }
        rendering = true;
        field.setText(shown);
        rendering = false;
        if (field.hasFocus()) {
            field.setSelection(shown.length());
        }
    }

    /**
     * The largest type size, at or below the value role, that draws this string
     * whole — down to the caption size and no further.
     *
     * <p>Why size before truncation: a coordinate typed into a 117 dp column is
     * long by two or three characters, and one step of type is the difference
     * between reading it and reading a fragment of it. The floor is a real type
     * role rather than a number invented here, and the field's own 48 dp minimum
     * height is unaffected either way, so nothing else in the row moves.
     *
     * <p>Writes only when the size actually changes, because a text size is a
     * layout property.
     */
    private void applyTextSizeFor(String complete) {
        final int available =
                field.getWidth() - field.getPaddingLeft() - field.getPaddingRight();
        float size = baseTextSizePx;
        if (available > 0 && !complete.isEmpty()) {
            final TextPaint scratch = new TextPaint(field.getPaint());
            while (size > minTextSizePx) {
                scratch.setTextSize(size);
                if (scratch.measureText(complete) <= available) {
                    break;
                }
                size -= 1.0f;
            }
        }
        if (Math.abs(field.getTextSize() - size) >= 0.5f) {
            field.setTextSize(TypedValue.COMPLEX_UNIT_PX, size);
        }
    }

    /**
     * As much of the value as the field can draw, shortened from the END.
     *
     * <p>From the end, always: the sign and the leading digits are the
     * magnitude, and a number that has lost its minus sign is not a shortened
     * number, it is a different one. Returns the value untouched when it fits,
     * and when the width is not known yet — one frame of a complete value is
     * better than a shortening based on a guessed width.
     */
    private String fitToField(String complete) {
        final int available =
                field.getWidth() - field.getPaddingLeft() - field.getPaddingRight();
        if (available <= 0 || complete.isEmpty()) {
            return complete;
        }
        final TextPaint paint = field.getPaint();
        if (paint.measureText(complete) <= available) {
            return complete;
        }
        final float ellipsis = paint.measureText(ELLIPSIS);
        int keep = complete.length();
        while (keep > 1 && paint.measureText(complete, 0, keep) + ellipsis > available) {
            keep--;
        }
        return complete.substring(0, keep) + ELLIPSIS;
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
            final BigDecimal parsed = LengthUnit.parse(text());
            setText(LengthUnit.present(to.convertFrom(from, parsed)));
            return true;
        } catch (NumberFormatException notANumber) {
            return false;
        }
    }
}
