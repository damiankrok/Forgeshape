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
 * The three numeric halves of a Construction Body's dimension annotation
 * (Stage 020M, {@code UI-OWNER-33B}).
 *
 * <p>The annotation itself — extension lines, dimension line and end ticks — is
 * drawn by the <b>renderer</b>, from the body's own local oriented bounds
 * carried through its transform, because it is geometry-shaped and belongs in
 * the same space as the body it measures. What is <b>not</b> geometry-shaped is
 * the number: an overall size has to be legible at any zoom, upright whatever
 * the camera is doing, and — the whole point — <b>editable by typing</b>. So
 * each label is a small chrome view positioned over the viewport at the anchor
 * native reports for that axis, which is the midpoint of that axis's real
 * dimension line and never a guessed offset off a world bounding box.
 *
 * <p>This is the same split, and the same grammar, {@link
 * SketchDimensionLabelView} uses for a selected sketch Line. It is a separate
 * view because it answers a different question — the overall size of a BODY,
 * not the length of one authored entity — and because three labels with one
 * open editor between them is not the one-label case.
 *
 * <p><b>An interaction overlay, not project geometry.</b> Nothing here is
 * exported to GLB, reaches a {@code .forge} byte, mints a revision or appears
 * in a snapshot.
 *
 * <p><b>Holds no dimension.</b> Every number shown is read back from native on
 * every refresh, and a typed value is submitted whole, in metres, through the
 * one domain entry point.
 */
final class BodyDimensionLabelsView extends FrameLayout {

    /** Told that an exact overall dimension was typed for one body axis. */
    interface OnDimensionAction {
        void onBodyDimensionEntered(int axis, double meters);
    }

    private static final int[] LABEL_IDS = {R.id.body_dimension_label_x,
            R.id.body_dimension_label_y, R.id.body_dimension_label_z};
    private static final int[] AXIS_NAMES = {R.string.body_dimension_x, R.string.body_dimension_y,
            R.string.body_dimension_z};

    private final InspectorHost host;
    private final OnDimensionAction actions;
    /** The ONE viewport-anchor conversion; see {@link ViewportAnchorSpace}. */
    private final ViewportAnchorSpace anchorSpace;
    private final double[] state = new double[NativeViewport.BODY_DIM_SIZE];
    private final float[] screen = new float[2];

    /**
     * The body these three numbers describe, or {@link NativeViewport#NO_OBJECT}.
     *
     * <p>Held so a change of owner is NOTICED rather than survived
     * ({@code UI3D-F-007}): an editor left open over the previous body would
     * submit a size to the wrong one, and a label placed from the previous
     * body's anchor is ghost UI whatever the number on it says. Cleared
     * whenever the surface withdraws, so no later refresh can resurrect it.
     */
    private long ownerBodyId = NativeViewport.NO_OBJECT;

    private final TextView[] readings = new TextView[3];
    private final LinearLayout editor;
    private final EditText field;

    /** The axis the editor is open on, or {@link NativeViewport#BODY_DIM_AXIS_NONE}. */
    private int editingAxis = NativeViewport.BODY_DIM_AXIS_NONE;

    BodyDimensionLabelsView(Context context, InspectorHost host, ViewportAnchorSpace anchorSpace,
                            OnDimensionAction actions) {
        super(context);
        this.host = host;
        this.anchorSpace = anchorSpace;
        this.actions = actions;
        setId(R.id.body_dimension_labels);
        setVisibility(GONE);

        for (int axis = 0; axis < 3; axis++) {
            final int which = axis;
            final TextView reading = EditorControlStyles.chip(context, LABEL_IDS[axis], "");
            reading.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    openEditor(which);
                }
            });
            readings[axis] = reading;
            addView(reading, new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT));
        }

        editor = new LinearLayout(context);
        editor.setId(R.id.body_dimension_editor);
        editor.setOrientation(LinearLayout.HORIZONTAL);
        editor.setGravity(Gravity.CENTER_VERTICAL);
        editor.setBackgroundResource(R.drawable.bg_surface_context);
        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        editor.setPadding(pad, pad, pad, pad);
        editor.setVisibility(GONE);

        field = new EditText(context);
        field.setId(R.id.field_body_dimension);
        field.setSingleLine(true);
        field.setBackgroundResource(R.drawable.bg_field);
        // The same keyboard every exact-value field in the product opens. A
        // dimension is unsigned — a negative size is refused below JNI rather
        // than clamped — but the sign stays typable so a mistake is REPORTED by
        // name instead of being silently impossible to make.
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
                R.id.apply_body_dimension, context.getString(R.string.apply_body_dimension));
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

        addView(editor, new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /**
     * Re-reads native state and places the three labels, or withdraws them all.
     *
     * <p>Withdrawn whenever Dimensions mode is closed or the active body cannot
     * be measured. One label is withdrawn on its own when its anchor does not
     * project to a point on screen — a value with nowhere honest to stand is not
     * drawn at a guessed position.
     */
    void refreshFromNative() {
        NativeViewport.bodyDimensionsState(state);
        final boolean open = state[NativeViewport.BODY_DIM_MODE_ACTIVE] != 0.0
                && state[NativeViewport.BODY_DIM_MEASURABLE] != 0.0;
        if (!open) {
            close();
            return;
        }
        // The owner is read back rather than assumed. Native closes the mode
        // itself the moment its body stops being measurable, so reaching here
        // means SOME body is being measured; which one is the question, and a
        // different answer than last time invalidates the open editor before
        // anything is placed from the new body's anchors.
        final long body = NativeViewport.sceneActiveBodyId();
        if (body != ownerBodyId) {
            closeEditor();
            ownerBodyId = body;
        }
        setVisibility(VISIBLE);
        final LengthUnit unit = host.uiState().displayUnit();
        for (int axis = 0; axis < 3; axis++) {
            final TextView reading = readings[axis];
            if (!NativeViewport.bodyDimensionLabelPoint(axis, screen)) {
                reading.setVisibility(GONE);
                continue;
            }
            final double meters = state[NativeViewport.BODY_DIM_X + axis];
            final String value = unit.formatWithUnit(meters);
            reading.setText(value);
            reading.setContentDescription(getContext().getString(
                    R.string.body_dimension_description, getContext().getString(AXIS_NAMES[axis]),
                    value));
            reading.setVisibility(axis == editingAxis ? GONE : VISIBLE);
            placeAt(reading, screen[0], screen[1]);
            if (axis == editingAxis) {
                placeAt(editor, screen[0], screen[1]);
            }
        }
    }

    /** Which axis the editor is open on, or -1. For verification. */
    int editingAxis() {
        return editingAxis;
    }

    /**
     * Centres a chip on its anchor, kept inside the VIEWPORT.
     *
     * <p>The anchor native reports is in viewport-content pixels and this view
     * stands in the inset-padded overlay, so the conversion between the two is
     * {@link ViewportAnchorSpace}'s and never arithmetic written here: one
     * contract for every anchored surface rather than three that can drift.
     */
    private void placeAt(View shown, float x, float y) {
        anchorSpace.measureAndPlace(shown, x, y, 1.0f);
    }

    /**
     * Opens the compact editor on one axis.
     *
     * <p>One axis at a time: the field is seeded with that axis's CURRENT
     * dimension and selected whole, so the first keystroke replaces rather than
     * appends — the rule every exact-value field in this product follows.
     */
    private void openEditor(int axis) {
        if (axis < 0 || axis >= 3 || getVisibility() != VISIBLE) {
            return;
        }
        editingAxis = axis;
        // Native owns which axis is active, because it is what the renderer
        // draws in the highlight weight. Java asks; it holds no answer.
        NativeViewport.setBodyDimensionAxis(axis);
        final LengthUnit unit = host.uiState().displayUnit();
        field.setText(unit.format(state[NativeViewport.BODY_DIM_X + axis]));
        field.selectAll();
        readings[axis].setVisibility(GONE);
        editor.setVisibility(VISIBLE);
        refreshFromNative();
        field.requestFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
        }
    }

    /** Closes the editor without submitting. The body's size is unchanged. */
    void closeEditor() {
        if (editor.getVisibility() != VISIBLE) {
            return;
        }
        editor.setVisibility(GONE);
        if (editingAxis >= 0 && editingAxis < 3) {
            readings[editingAxis].setVisibility(VISIBLE);
        }
        editingAxis = NativeViewport.BODY_DIM_AXIS_NONE;
        NativeViewport.setBodyDimensionAxis(NativeViewport.BODY_DIM_AXIS_NONE);
        field.clearFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    private void close() {
        closeEditor();
        // The cached owner goes with the surface. Kept, it would let a later
        // unrelated refresh place three numbers from a body that is no longer
        // the one being measured, which is exactly the ghost UI `UI-OWNER-50`
        // forbids.
        ownerBodyId = NativeViewport.NO_OBJECT;
        setVisibility(GONE);
    }

    /**
     * Parses the field and submits it as METRES for the axis being edited.
     *
     * <p>An unparseable value is reported by name and the field keeps focus so
     * it can be corrected. Whether the value is a size the domain accepts is
     * decided below JNI, and neither case moves one coordinate here.
     */
    private void submit() {
        if (editingAxis < 0 || editingAxis >= 3) {
            return;
        }
        final Context context = getContext();
        final String raw = field.getText().toString();
        final BigDecimal typed;
        try {
            typed = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                            ? context.getString(R.string.field_empty,
                                    context.getString(AXIS_NAMES[editingAxis]))
                            : context.getString(R.string.field_not_a_number,
                                    context.getString(AXIS_NAMES[editingAxis]), raw.trim()),
                    R.attr.fsTextError);
            field.requestFocus();
            return;
        }
        final LengthUnit unit = host.uiState().displayUnit();
        actions.onBodyDimensionEntered(editingAxis,
                unit.toMeters(typed).doubleValue());
    }
}
