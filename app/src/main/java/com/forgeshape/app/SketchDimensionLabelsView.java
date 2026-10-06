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
import java.util.ArrayList;
import java.util.List;

/**
 * The numeric labels of a sketch's PERSISTENT dimensions
 * (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`).
 *
 * <p>The annotation — extension lines, dimension line, arrowheads, the radial
 * leader, the angle arc — is drawn by the renderer from the sketch overlay's
 * {@code Dimension} (Driving) and {@code DimensionReference} ranges. What is not
 * geometry-shaped is the number, so each is a chip placed at the anchor native
 * reports, on {@link SketchDimensionLabelView}'s own terms.
 *
 * <p><b>Driving and Reference read apart by more than colour</b>: a Reference
 * is written in parentheses and drawn at reduced emphasis, and only a Driving
 * label opens a value field — a Reference opens Delete alone, because its value
 * is measured and has nothing to drive.
 *
 * <p><b>Overlapping labels are HIDDEN, never moved</b>: the selection's own
 * dimensions claim their box first, then Driving, then the older id
 * ({@link SketchDraftingPresentation#resolveVisible}). A label that does not
 * project is not drawn at a guessed point.
 *
 * <p>The container is not clickable and claims no touch of its own; each chip
 * is a 48 dp target and nothing else, so a tap beside a label still reaches the
 * sketch.
 *
 * <p><b>Holds no dimension.</b> Values, modes and anchors are re-read from the
 * session on every refresh.
 */
final class SketchDimensionLabelsView extends FrameLayout {

    /** Told what was asked; the workspace owns what it means. */
    interface OnDimensionLabelAction {
        void onSketchDimensionValueEntered(long dimensionId, int kind, double value);

        void onSketchDimensionDeleteRequested(long dimensionId);
    }

    private final InspectorHost host;
    private final ViewportAnchorSpace anchorSpace;
    private final OnDimensionLabelAction actions;

    private final double[] labels =
            new double[NativeViewport.SKETCH_LABEL_MAX * NativeViewport.SKETCH_LABEL_STRIDE];
    private final long[] selection = new long[NativeViewport.SKETCH_LABEL_MAX];
    private final List<TextView> pool = new ArrayList<>();

    private final LinearLayout editor;
    private final EditText field;
    private final TextView apply;

    /** The dimension the editor is open on, or 0. */
    private long editingId;
    private int editingKind;
    private boolean editingReference;
    private float editingX;
    private float editingY;

    SketchDimensionLabelsView(Context context, InspectorHost host, ViewportAnchorSpace anchorSpace,
                              OnDimensionLabelAction actions) {
        super(context);
        this.host = host;
        this.anchorSpace = anchorSpace;
        this.actions = actions;
        setId(R.id.sketch_dimension_labels);
        setClickable(false);
        setFocusable(false);
        setVisibility(GONE);

        editor = new LinearLayout(context);
        editor.setId(R.id.sketch_dimension_value_editor);
        editor.setOrientation(LinearLayout.HORIZONTAL);
        editor.setGravity(Gravity.CENTER_VERTICAL);
        editor.setBackgroundResource(R.drawable.bg_surface_context);
        final int pad = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        editor.setPadding(pad, pad, pad, pad);
        editor.setVisibility(GONE);

        field = new EditText(context);
        field.setId(R.id.field_sketch_dimension_value);
        field.setSingleLine(true);
        field.setBackgroundResource(R.drawable.bg_field);
        // Text, not the numeric keyboard: the field reads the safe arithmetic
        // ExactExpression accepts (12+3, 50/2), and a sign stays typable so a
        // mistake is REPORTED by name rather than silently impossible to make.
        field.setInputType(InputType.TYPE_CLASS_TEXT);
        field.setKeyListener(DigitsKeyListener.getInstance("0123456789.,-+*/()"));
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

        apply = EditorControlStyles.primaryButton(context, R.id.apply_sketch_dimension_value,
                context.getString(R.string.apply));
        apply.setOnClickListener(v -> submit());
        editor.addView(apply, EditorControlStyles.wrap(pad));

        final TextView delete = EditorControlStyles.chip(context, R.id.sketch_dimension_delete,
                context.getString(R.string.sketch_dimension_delete));
        delete.setOnClickListener(v -> {
            final long id = editingId;
            closeEditor();
            if (id != 0) {
                actions.onSketchDimensionDeleteRequested(id);
            }
        });
        editor.addView(delete, EditorControlStyles.wrap(pad));

        addView(editor, new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /** Layout params for the container: the whole viewport overlay. */
    static FrameLayout.LayoutParams anchoredParams() {
        return new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
    }

    private TextView labelAt(int index) {
        while (pool.size() <= index) {
            final TextView chip = EditorControlStyles.chip(getContext(),
                    R.id.sketch_dimension_label_item, "");
            chip.setOnClickListener(v -> {
                final Object tag = v.getTag(R.id.sketch_dimension_label_item);
                if (tag instanceof double[]) {
                    openEditor((double[]) tag);
                }
            });
            pool.add(chip);
            // Below the editor, so an open field is never covered by a label.
            addView(chip, getChildCount() - 1, new LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        return pool.get(index);
    }

    /** Re-reads the session and places every visible label, or withdraws them all. */
    void refreshFromNative() {
        final int count = Math.min(NativeViewport.sketchDimensionLabels(labels),
                NativeViewport.SKETCH_LABEL_MAX);
        if (count <= 0) {
            close();
            return;
        }
        setVisibility(VISIBLE);
        final int selected = Math.min(NativeViewport.sketchSelection(selection), selection.length);
        final LengthUnit unit = host.uiState().displayUnit();
        final Context context = getContext();
        final int floor = EditorControlStyles.dimen(context, R.dimen.control_height);
        final float[] cx = new float[count];
        final float[] cy = new float[count];
        final float[] w = new float[count];
        final float[] h = new float[count];
        final int[] priority = new int[count];
        final boolean[] projects = new boolean[count];
        boolean editingStillShown = false;
        for (int i = 0; i < count; i++) {
            final int o = i * NativeViewport.SKETCH_LABEL_STRIDE;
            final long id = (long) labels[o + NativeViewport.SKETCH_LABEL_ID];
            final int kind = (int) labels[o + NativeViewport.SKETCH_LABEL_KIND];
            final boolean reference =
                    (int) labels[o + NativeViewport.SKETCH_LABEL_MODE] == NativeViewport.DIM_MODE_REFERENCE;
            final double value = labels[o + NativeViewport.SKETCH_LABEL_VALUE];
            final long entity = (long) labels[o + NativeViewport.SKETCH_LABEL_ENTITY];
            final TextView chip = labelAt(i);
            final String text = SketchDraftingPresentation.label(kind, reference, value, unit);
            chip.setText(text);
            chip.setContentDescription(context.getString(reference
                            ? R.string.sketch_dimension_reference_description
                            : R.string.sketch_dimension_driving_description,
                    context.getString(SketchDraftingPresentation.kindName(kind)), text));
            chip.setAlpha(reference ? 0.72f : 1.0f);
            chip.setTag(R.id.sketch_dimension_label_item, new double[] {
                    id, kind, reference ? 1.0 : 0.0, value,
                    labels[o + NativeViewport.SKETCH_LABEL_X], labels[o + NativeViewport.SKETCH_LABEL_Y]});
            chip.setTag("dimension:" + id);
            projects[i] = labels[o + NativeViewport.SKETCH_LABEL_PROJECTS] != 0.0;
            cx[i] = (float) labels[o + NativeViewport.SKETCH_LABEL_X];
            cy[i] = (float) labels[o + NativeViewport.SKETCH_LABEL_Y];
            anchorSpace.measureUnderParent(chip);
            w[i] = SketchDraftingPresentation.touchExtent(chip.getMeasuredWidth(), floor);
            h[i] = SketchDraftingPresentation.touchExtent(chip.getMeasuredHeight(), floor);
            boolean ofSelection = false;
            for (int s = 0; s < selected; s++) {
                ofSelection = ofSelection || selection[s] == entity;
            }
            priority[i] = SketchDraftingPresentation.priority(ofSelection, !reference, id);
            if (!projects[i]) {
                // A label with nowhere honest to stand claims no box either.
                priority[i] = Integer.MIN_VALUE;
                w[i] = 0.0f;
                h[i] = 0.0f;
            }
            if (id == editingId) {
                editingStillShown = projects[i];
                editingX = cx[i];
                editingY = cy[i];
            }
        }
        final boolean[] visible = SketchDraftingPresentation.resolveVisible(cx, cy, w, h, priority);
        for (int i = 0; i < pool.size(); i++) {
            final TextView chip = pool.get(i);
            final boolean show = i < count && projects[i] && visible[i];
            final long id = i < count
                    ? (long) labels[i * NativeViewport.SKETCH_LABEL_STRIDE + NativeViewport.SKETCH_LABEL_ID]
                    : 0L;
            chip.setVisibility(show && id != editingId ? VISIBLE : GONE);
            if (show) {
                anchorSpace.measureAndPlace(chip, cx[i], cy[i], 1.0f);
            }
        }
        if (editingId != 0) {
            if (!editingStillShown) {
                closeEditor();
            } else {
                anchorSpace.measureAndPlace(editor, editingX, editingY, 1.0f);
            }
        }
    }

    /** The number of labels currently drawn, for verification. */
    int visibleLabelCount() {
        int n = 0;
        for (TextView chip : pool) {
            n += chip.getVisibility() == VISIBLE ? 1 : 0;
        }
        return n;
    }

    /** The dimension the editor is open on, or 0, for verification. */
    long editingDimensionId() {
        return editingId;
    }

    /** The drawn label of a dimension id, or null, for verification. */
    TextView labelFor(long dimensionId) {
        for (TextView chip : pool) {
            if (("dimension:" + dimensionId).equals(chip.getTag()) && chip.getVisibility() == VISIBLE) {
                return chip;
            }
        }
        return null;
    }

    private void openEditor(double[] label) {
        editingId = (long) label[0];
        editingKind = (int) label[1];
        editingReference = label[2] != 0.0;
        editingX = (float) label[4];
        editingY = (float) label[5];
        final LengthUnit unit = host.uiState().displayUnit();
        final double value = label[3];
        // Seeded with the CURRENT value — every digit, as every editor is —
        // and selected whole, so the first keystroke replaces rather than
        // appends.
        field.setText(SketchDraftingPresentation.isAngle(editingKind)
                ? LengthUnit.present(BigDecimal.valueOf(value))
                : unit.format(value));
        field.setVisibility(editingReference ? GONE : VISIBLE);
        apply.setVisibility(editingReference ? GONE : VISIBLE);
        editor.setVisibility(VISIBLE);
        refreshFromNative();
        if (!editingReference) {
            field.selectAll();
            field.requestFocus();
            final InputMethodManager ime = (InputMethodManager)
                    getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (ime != null) {
                ime.showSoftInput(field, InputMethodManager.SHOW_IMPLICIT);
            }
        }
    }

    /** Closes the editor without submitting; the authored value is unchanged. */
    void closeEditor() {
        if (editingId == 0 && editor.getVisibility() != VISIBLE) {
            return;
        }
        editingId = 0;
        editor.setVisibility(GONE);
        field.clearFocus();
        final InputMethodManager ime = (InputMethodManager)
                getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
    }

    private void close() {
        closeEditor();
        for (TextView chip : pool) {
            chip.setVisibility(GONE);
        }
        setVisibility(GONE);
    }

    /**
     * Parses the field and submits it: METRES for a length, DEGREES for an
     * angle. An unparseable value is reported by name and the field keeps focus.
     */
    private void submit() {
        if (editingId == 0 || editingReference) {
            return;
        }
        final Context context = getContext();
        final String raw = field.getText().toString();
        final String name = context.getString(SketchDraftingPresentation.kindName(editingKind));
        final BigDecimal typed;
        try {
            typed = ExactExpression.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                            ? context.getString(R.string.field_empty, name)
                            : context.getString(R.string.field_not_a_number, name, raw.trim()),
                    R.attr.fsTextError);
            field.requestFocus();
            return;
        }
        final double value = SketchDraftingPresentation.isAngle(editingKind)
                ? typed.doubleValue()
                : host.uiState().displayUnit().toMeters(typed).doubleValue();
        actions.onSketchDimensionValueEntered(editingId, editingKind, value);
    }
}
