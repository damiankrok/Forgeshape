package com.forgeshape.app;

import android.content.Context;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The numeric half of a selected Line's technical dimension
 * (`SKETCH-UX-R1` E1, E3).
 *
 * <p>The annotation itself — extension lines, dimension line, end ticks — is
 * drawn by the renderer from the sketch overlay's own {@code Dimension} range,
 * because it is geometry-shaped and belongs in the same space as the stroke it
 * measures. What is <b>not</b> geometry-shaped is the number: a length has to be
 * legible at any zoom, upright whatever the view rotation, and — the whole point
 * — <b>editable by typing</b>. So the label is a small chrome view positioned
 * over the viewport at the anchor point native reports.
 *
 * <p>It is an <b>interaction overlay</b>, not project geometry. It is never
 * exported to GLB, never reaches a {@code .forge} byte, never mints a revision
 * and never appears in a snapshot. Nothing downstream can read a dimension back
 * out of it because nothing downstream is given it.
 *
 * <p>Two states in one view, and the second is opened only by a deliberate tap:
 *
 * <ul>
 *   <li><b>Reading</b> — the length in the project's display unit, formatted by
 *       the same {@link LengthUnit} rules every other value uses.</li>
 *   <li><b>Editing</b> — a compact field and one Apply. It is deliberately small
 *       and sits where the label was, so it never covers the drawing the user is
 *       measuring; the precision surface remains the place for a value that
 *       needs room.</li>
 * </ul>
 *
 * <p><b>Holds no length.</b> The number shown is read back from the session on
 * every refresh, and a typed value is submitted whole and never snapped — the
 * grid is a drawing aid and this field is the way around it.
 */
final class SketchDimensionLabelView extends FrameLayout {

    /** Told that an exact length was typed; the workspace owns what it means. */
    interface OnDimensionAction {
        void onSketchLineLengthEntered(long entityId, double lengthMeters);
    }

    private final OnDimensionAction actions;
    private final InspectorHost host;
    private final double[] dimension = new double[NativeViewport.SKETCH_DIMENSION_SIZE];
    private final float[] screen = new float[2];

    private final TextView reading;
    private final LinearLayout editor;
    private final EditText field;

    /** The entity the label currently describes, or 0 when it is not shown. */
    private long entityId;

    SketchDimensionLabelView(Context context, InspectorHost host, OnDimensionAction actions) {
        super(context);
        this.host = host;
        this.actions = actions;
        setId(R.id.sketch_dimension_label);
        setVisibility(GONE);

        reading = EditorControlStyles.chip(context, R.id.sketch_dimension_value, "");
        reading.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                openEditor();
            }
        });
        addView(reading, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        editor = new LinearLayout(context);
        editor.setId(R.id.sketch_dimension_editor);
        editor.setOrientation(LinearLayout.HORIZONTAL);
        editor.setGravity(Gravity.CENTER_VERTICAL);
        editor.setBackgroundResource(R.drawable.bg_surface_context);
        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        editor.setPadding(pad, pad, pad, pad);
        editor.setVisibility(GONE);

        field = new EditText(context);
        field.setId(R.id.field_sketch_line_length);
        field.setSingleLine(true);
        field.setBackgroundResource(R.drawable.bg_field);
        // The same keyboard every exact-value field in the product opens. A
        // length is unsigned — a negative length is a different question, and
        // it is refused below JNI rather than clamped — but the sign is left
        // accepted here so a mistyped value is REPORTED by name rather than
        // silently impossible to enter.
        field.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                | InputType.TYPE_NUMBER_FLAG_SIGNED);
        field.setKeyListener(DigitsKeyListener.getInstance("0123456789.-"));
        field.setImeOptions(EditorInfo.IME_ACTION_DONE);
        field.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        field.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        field.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.sketch_dimension_field));
        field.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        field.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    submit();
                    return true;
                }
                return false;
            }
        });
        editor.addView(field, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        final TextView apply = EditorControlStyles.primaryButton(context,
                R.id.apply_sketch_line_length, context.getString(R.string.apply));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submit();
            }
        });
        final LinearLayout.LayoutParams applyParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        applyParams.leftMargin = pad;
        editor.addView(apply, applyParams);

        addView(editor, new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /**
     * Re-reads the session and places the label, or withdraws it.
     *
     * <p>Withdrawn whenever the selection is not a straight Line, or the anchor
     * does not project to a point on screen — a label with nowhere honest to
     * stand is not drawn at a guessed position.
     */
    void refreshFromNative() {
        if (!NativeViewport.sketchLineDimension(dimension)) {
            close();
            return;
        }
        final long id = (long) dimension[NativeViewport.SKETCH_DIMENSION_ENTITY];
        if (!NativeViewport.sketchScreenPoint(dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_U],
                                              dimension[NativeViewport.SKETCH_DIMENSION_ANCHOR_V],
                                              screen)) {
            close();
            return;
        }
        // A different line means a different value: an editor left open over
        // the previous one would submit a length to the wrong entity.
        if (id != entityId) {
            closeEditor();
        }
        entityId = id;
        final LengthUnit unit = host.uiState().displayUnit();
        final double meters = dimension[NativeViewport.SKETCH_DIMENSION_LENGTH];
        reading.setText(unit.formatWithUnit(meters));
        reading.setContentDescription(getContext().getString(
                R.string.sketch_dimension_description, unit.formatWithUnit(meters)));
        setVisibility(VISIBLE);
        placeAt(screen[0], screen[1]);
    }

    /** Centres the label on the annotation's anchor, kept inside the window. */
    private void placeAt(float x, float y) {
        final View shown = editor.getVisibility() == VISIBLE ? editor : reading;
        shown.measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int width = shown.getMeasuredWidth();
        final int height = shown.getMeasuredHeight();
        final ViewGroup parent = (ViewGroup) getParent();
        float left = x - width * 0.5f;
        float top = y - height * 0.5f;
        if (parent != null) {
            // Clamped into the parent rather than allowed off-screen: a label
            // half outside the window is a value the user cannot read or tap.
            left = Math.max(0.0f, Math.min(left, parent.getWidth() - width));
            top = Math.max(0.0f, Math.min(top, parent.getHeight() - height));
        }
        setTranslationX(left);
        setTranslationY(top);
    }

    /** Whether the numeric editor is open, for verification. */
    boolean editorOpen() {
        return editor.getVisibility() == VISIBLE;
    }

    /** The entity the label describes, or 0. */
    long entityId() {
        return entityId;
    }

    private void openEditor() {
        if (entityId == 0) {
            return;
        }
        final LengthUnit unit = host.uiState().displayUnit();
        // Seeded with the CURRENT length so the field opens on the value it is
        // about to replace, and selected whole so the first keystroke replaces
        // rather than appends — the same rule every exact-value field follows.
        field.setText(unit.format(dimension[NativeViewport.SKETCH_DIMENSION_LENGTH]));
        field.selectAll();
        reading.setVisibility(GONE);
        editor.setVisibility(VISIBLE);
        placeAt(screen[0], screen[1]);
        field.requestFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
        }
    }

    /** Closes the editor without submitting. The authored length is unchanged. */
    void closeEditor() {
        if (editor.getVisibility() != VISIBLE) {
            return;
        }
        editor.setVisibility(GONE);
        reading.setVisibility(VISIBLE);
        field.clearFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    private void close() {
        closeEditor();
        entityId = 0;
        setVisibility(GONE);
    }

    /**
     * Parses the field and submits it as METRES.
     *
     * <p>An unparseable value is reported by name and the field keeps focus so
     * it can be corrected; a value the domain refuses is reported the same way,
     * and in neither case is one authored coordinate moved.
     */
    private void submit() {
        final Context context = getContext();
        final String raw = field.getText().toString();
        final BigDecimal typed;
        try {
            typed = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                    ? context.getString(R.string.field_empty,
                                        context.getString(R.string.label_length))
                    : context.getString(R.string.field_not_a_number,
                                        context.getString(R.string.label_length), raw.trim()),
                    R.attr.fsTextError);
            field.requestFocus();
            return;
        }
        actions.onSketchLineLengthEntered(entityId,
                host.uiState().displayUnit().toMeters(typed).doubleValue());
    }

    /** Layout params for the label: absolutely placed inside the viewport overlay. */
    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
    }
}
