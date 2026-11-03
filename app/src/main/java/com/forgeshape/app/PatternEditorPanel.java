package com.forgeshape.app;

import android.content.Context;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The canvas editor of one Linear, Circular or Mirror pattern
 * (`CAD-PATTERN-R1`): the kind, the axis and its direction, how many copies,
 * the spacing / angle / plane, and for a Linear pattern an optional second
 * direction — then Apply, Delete (an existing pattern) and Cancel.
 *
 * <p>It holds no truth. What it shows comes from native (the seeds of a new
 * pattern, the values of a committed one), and Apply hands every value to
 * native, which judges the whole body and makes the act ONE Undo. Rows a kind
 * does not use are ABSENT rather than disabled: a Mirror has no count and no
 * direction to reverse, and only a Linear pattern has a second direction.
 */
final class PatternEditorPanel extends LinearLayout {

    /** What the panel asks of its host. */
    interface Listener {
        /** The display unit lengths are typed in. */
        LengthUnit patternUnit();

        /** Reports a field that does not parse, as a verdict. */
        void patternFieldRefused(String message);

        /** Applies the request; true closes the panel. */
        boolean patternApply(Request request);

        /** Deletes the pattern being edited; true closes the panel. */
        boolean patternDelete();

        /** The panel closed without applying anything. */
        void patternCancelled();
    }

    /** Every value one Apply sends, in native's units (metres, degrees). */
    static final class Request {
        int kind;
        int axis;
        boolean reversed;
        int count;
        double value;
        int axis2 = -1;
        int count2 = 1;
        double spacing2;
        final double[] center = new double[3];
    }

    private final Listener listener;
    private final TextView[] kindChips = new TextView[3];
    private final TextView axisLabel;
    private final TextView[] axisChips = new TextView[3];
    private final TextView reverseChip;
    private final LinearLayout countRow;
    private final EditText countField;
    private final TextView valueLabel;
    private final EditText valueField;
    private final LinearLayout secondRow;
    private final TextView[] secondChips = new TextView[4];
    private final LinearLayout secondValues;
    private final EditText count2Field;
    private final EditText spacing2Field;
    private final TextView deleteButton;

    private int kind = NativeViewport.PATTERN_LINEAR;
    private int axis = NativeViewport.AXIS_X;
    private boolean reversed;
    private int axis2 = -1;
    private double[] seeds = new double[NativeViewport.PATTERN_SEEDS_SIZE];
    private final double[] center = new double[3];
    private boolean editing;

    PatternEditorPanel(Context context, Listener listener) {
        super(context);
        this.listener = listener;
        setId(R.id.cad_canvas_pattern_editor);
        setOrientation(VERTICAL);
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.cad_hud_editor_padding);
        setPadding(pad, pad, pad, pad);
        setClickable(true);
        setVisibility(GONE);
        final int gap = EditorControlStyles.dimen(context, R.dimen.toolbar_gap);

        final LinearLayout kindRow = row(context, 0);
        final int[] kindIds = {R.id.pattern_kind_linear, R.id.pattern_kind_circular,
                R.id.pattern_kind_mirror};
        for (int k = 0; k < 3; k++) {
            final int chosen = k;
            kindChips[k] = EditorControlStyles.chip(context, kindIds[k],
                    context.getString(PatternPresentation.kindName(k)));
            kindChips[k].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    chooseKind(chosen);
                }
            });
            kindRow.addView(kindChips[k], EditorControlStyles.wrap(k == 0 ? 0 : gap));
        }

        final LinearLayout axisRow = row(context, gap);
        axisLabel = label(context);
        axisRow.addView(axisLabel, EditorControlStyles.wrap(0));
        final int[] axisIds = {R.id.pattern_axis_x, R.id.pattern_axis_y, R.id.pattern_axis_z};
        for (int a = 0; a < 3; a++) {
            final int chosen = a;
            axisChips[a] = EditorControlStyles.chip(context, axisIds[a],
                    context.getString(PatternPresentation.axisName(a)));
            axisChips[a].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    chooseAxis(chosen);
                }
            });
            axisRow.addView(axisChips[a], EditorControlStyles.wrap(gap));
        }
        reverseChip = EditorControlStyles.chip(context, R.id.pattern_reverse,
                context.getString(R.string.pattern_reverse));
        reverseChip.setContentDescription(context.getString(R.string.pattern_reverse_description));
        reverseChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                reversed = !reversed;
                refreshChips();
            }
        });
        axisRow.addView(reverseChip, EditorControlStyles.wrap(gap));

        final LinearLayout valueRow = row(context, gap);
        countRow = new LinearLayout(context);
        countRow.setOrientation(HORIZONTAL);
        countRow.setGravity(Gravity.CENTER_VERTICAL);
        final TextView countLabel = label(context);
        countLabel.setText(R.string.pattern_label_count);
        countRow.addView(countLabel, EditorControlStyles.wrap(0));
        countField = field(context, R.id.field_pattern_count, false);
        countRow.addView(countField, EditorControlStyles.wrap(gap));
        valueRow.addView(countRow, EditorControlStyles.wrap(0));
        valueLabel = label(context);
        valueRow.addView(valueLabel, EditorControlStyles.wrap(gap * 2));
        valueField = field(context, R.id.field_pattern_value, true);
        valueRow.addView(valueField, EditorControlStyles.wrap(gap));

        secondRow = row(context, gap);
        final TextView secondLabel = label(context);
        secondLabel.setText(R.string.pattern_label_second);
        secondRow.addView(secondLabel, EditorControlStyles.wrap(0));
        final int[] secondIds = {R.id.pattern_second_off, R.id.pattern_second_x,
                R.id.pattern_second_y, R.id.pattern_second_z};
        for (int s = 0; s < 4; s++) {
            final int chosen = s - 1;
            secondChips[s] = EditorControlStyles.chip(context, secondIds[s],
                    context.getString(s == 0 ? R.string.pattern_second_off
                            : PatternPresentation.axisName(s - 1)));
            secondChips[s].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    chooseSecondAxis(chosen);
                }
            });
            secondRow.addView(secondChips[s], EditorControlStyles.wrap(gap));
        }
        secondValues = row(context, gap);
        final TextView count2Label = label(context);
        count2Label.setText(R.string.pattern_label_count2);
        secondValues.addView(count2Label, EditorControlStyles.wrap(0));
        count2Field = field(context, R.id.field_pattern_count2, false);
        secondValues.addView(count2Field, EditorControlStyles.wrap(gap));
        final TextView spacing2Label = label(context);
        spacing2Label.setText(R.string.pattern_label_spacing2);
        secondValues.addView(spacing2Label, EditorControlStyles.wrap(gap * 2));
        spacing2Field = field(context, R.id.field_pattern_spacing2, true);
        secondValues.addView(spacing2Field, EditorControlStyles.wrap(gap));

        final LinearLayout actions = row(context, gap);
        final TextView apply = EditorControlStyles.primaryButton(context, R.id.apply_pattern,
                context.getString(R.string.apply));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                submit();
            }
        });
        actions.addView(apply, EditorControlStyles.wrap(0));
        deleteButton = EditorControlStyles.actionChip(context, R.id.delete_pattern,
                context.getString(R.string.pattern_delete));
        deleteButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (PatternEditorPanel.this.listener.patternDelete()) {
                    close();
                }
            }
        });
        actions.addView(deleteButton, EditorControlStyles.wrap(gap));
        final TextView cancel = EditorControlStyles.actionChip(context, R.id.cancel_pattern,
                context.getString(R.string.pattern_cancel));
        cancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                close();
                PatternEditorPanel.this.listener.patternCancelled();
            }
        });
        actions.addView(cancel, EditorControlStyles.wrap(gap));

        addView(kindRow);
        addView(axisRow);
        addView(valueRow);
        addView(secondRow);
        addView(secondValues);
        addView(actions);
    }

    /** Opens the panel for a NEW pattern of {@code kind}, seeded by native. */
    void openNew(int kind, double[] seeds) {
        this.seeds = seeds.clone();
        editing = false;
        center[0] = seeds[NativeViewport.PATTERN_SEED_CENTER_X];
        center[1] = seeds[NativeViewport.PATTERN_SEED_CENTER_X + 1];
        center[2] = seeds[NativeViewport.PATTERN_SEED_CENTER_X + 2];
        chooseKind(kind);
        deleteButton.setVisibility(GONE);
        setVisibility(VISIBLE);
    }

    /** Opens the panel over a COMMITTED pattern's values; Apply edits it. */
    void openExisting(double[] info, double[] seeds) {
        this.seeds = seeds.clone();
        editing = true;
        kind = (int) info[NativeViewport.PATTERN_INFO_KIND];
        axis = PatternPresentation.clampAxis((int) info[NativeViewport.PATTERN_INFO_AXIS]);
        reversed = info[NativeViewport.PATTERN_INFO_REVERSED] != 0.0;
        final int count2 = (int) info[NativeViewport.PATTERN_INFO_COUNT2];
        axis2 = kind == NativeViewport.PATTERN_LINEAR && count2 > 1
                ? (int) info[NativeViewport.PATTERN_INFO_AXIS2] : -1;
        for (int k = 0; k < 3; k++) {
            center[k] = info[NativeViewport.PATTERN_INFO_CENTER_X + k];
        }
        final LengthUnit unit = listener.patternUnit();
        countField.setText(String.valueOf((int) info[NativeViewport.PATTERN_INFO_COUNT]));
        switch (kind) {
            case NativeViewport.PATTERN_CIRCULAR:
                valueField.setText(CadRevolvePresentation.formatDegrees(
                        info[NativeViewport.PATTERN_INFO_ANGLE]));
                break;
            case NativeViewport.PATTERN_MIRROR:
                valueField.setText(unit.format(info[NativeViewport.PATTERN_INFO_OFFSET]));
                break;
            default:
                valueField.setText(unit.format(info[NativeViewport.PATTERN_INFO_SPACING]));
                break;
        }
        count2Field.setText(String.valueOf(Math.max(2, count2)));
        spacing2Field.setText(unit.format(count2 > 1 ? info[NativeViewport.PATTERN_INFO_SPACING2]
                : PatternPresentation.defaultValue(NativeViewport.PATTERN_LINEAR,
                        PatternPresentation.nextAxis(axis), this.seeds)));
        deleteButton.setVisibility(VISIBLE);
        refreshChips();
        setVisibility(VISIBLE);
    }

    boolean isOpen() {
        return getVisibility() == VISIBLE;
    }

    boolean isEditing() {
        return editing;
    }

    int kind() {
        return kind;
    }

    void close() {
        setVisibility(GONE);
        countField.clearFocus();
        valueField.clearFocus();
        count2Field.clearFocus();
        spacing2Field.clearFocus();
    }

    // -------------------------------------------------------------------------

    private void chooseKind(int chosen) {
        kind = chosen;
        axis = PatternPresentation.defaultAxis(kind, seeds);
        reversed = PatternPresentation.defaultReversed(kind, axis, seeds);
        axis2 = -1;
        countField.setText(String.valueOf(PatternPresentation.defaultCount(kind, axis, seeds)));
        writeValue(PatternPresentation.defaultValue(kind, axis, seeds));
        count2Field.setText("2");
        writeSpacing2();
        refreshChips();
    }

    private void chooseAxis(int chosen) {
        final boolean wasDefault = !editing;
        axis = chosen;
        if (axis2 == axis) {
            axis2 = PatternPresentation.nextAxis(axis);
            writeSpacing2();
        }
        // A NEW pattern follows its seeds per axis, so changing the direction
        // keeps the copies clear of the original along the new one.
        if (wasDefault && kind != NativeViewport.PATTERN_CIRCULAR) {
            writeValue(PatternPresentation.defaultValue(kind, axis, seeds));
            if (kind == NativeViewport.PATTERN_LINEAR) {
                reversed = PatternPresentation.defaultReversed(kind, axis, seeds);
                countField.setText(String.valueOf(
                        PatternPresentation.defaultCount(kind, axis, seeds)));
            }
        }
        refreshChips();
    }

    private void chooseSecondAxis(int chosen) {
        axis2 = chosen == axis ? PatternPresentation.nextAxis(axis) : chosen;
        if (axis2 >= 0) {
            writeSpacing2();
        }
        refreshChips();
    }

    private void writeValue(double value) {
        valueField.setText(PatternPresentation.valueIsLength(kind)
                ? listener.patternUnit().format(value)
                : CadRevolvePresentation.formatDegrees(value));
    }

    private void writeSpacing2() {
        final int along = axis2 >= 0 ? axis2 : PatternPresentation.nextAxis(axis);
        spacing2Field.setText(listener.patternUnit().format(PatternPresentation.defaultValue(
                NativeViewport.PATTERN_LINEAR, along, seeds)));
    }

    private void refreshChips() {
        final Context context = getContext();
        for (int k = 0; k < 3; k++) {
            EditorControlStyles.setChipActive(kindChips[k], k == kind);
        }
        axisLabel.setText(PatternPresentation.axisLabel(kind));
        for (int a = 0; a < 3; a++) {
            EditorControlStyles.setChipActive(axisChips[a], a == axis);
            axisChips[a].setContentDescription(context.getString(R.string.pattern_axis_description,
                    context.getString(PatternPresentation.axisLabel(kind)),
                    context.getString(PatternPresentation.axisName(a))));
        }
        reverseChip.setVisibility(PatternPresentation.reverseShown(kind) ? VISIBLE : GONE);
        EditorControlStyles.setChipActive(reverseChip, reversed);
        countRow.setVisibility(PatternPresentation.countShown(kind) ? VISIBLE : GONE);
        valueLabel.setText(PatternPresentation.valueLabel(kind));
        final boolean second = PatternPresentation.secondDirectionShown(kind);
        secondRow.setVisibility(second ? VISIBLE : GONE);
        for (int s = 0; s < 4; s++) {
            EditorControlStyles.setChipActive(secondChips[s], s - 1 == axis2);
            // The direction the first row already walks is not a second one.
            secondChips[s].setVisibility(s - 1 == axis ? GONE : VISIBLE);
        }
        secondValues.setVisibility(second && axis2 >= 0 ? VISIBLE : GONE);
        valueField.setContentDescription(context.getString(R.string.pattern_field_description,
                context.getString(PatternPresentation.valueLabel(kind)),
                PatternPresentation.valueIsLength(kind) ? listener.patternUnit().label()
                        : context.getString(R.string.pattern_degrees)));
    }

    private void submit() {
        final Context context = getContext();
        final Request request = new Request();
        request.kind = kind;
        request.axis = axis;
        request.reversed = reversed && PatternPresentation.reverseShown(kind);
        request.count = PatternPresentation.countShown(kind)
                ? PatternPresentation.parseCount(countField.getText().toString()) : 2;
        if (request.count < 0) {
            refuse(R.string.pattern_label_count, countField);
            return;
        }
        final Double value = parseValue(valueField, PatternPresentation.valueIsLength(kind));
        if (value == null) {
            refuse(PatternPresentation.valueLabel(kind), valueField);
            return;
        }
        request.value = value;
        if (PatternPresentation.secondDirectionShown(kind) && axis2 >= 0) {
            request.axis2 = axis2;
            request.count2 = PatternPresentation.parseCount(count2Field.getText().toString());
            if (request.count2 < 0) {
                refuse(R.string.pattern_label_count2, count2Field);
                return;
            }
            final Double spacing2 = parseValue(spacing2Field, true);
            if (spacing2 == null) {
                refuse(R.string.pattern_label_spacing2, spacing2Field);
                return;
            }
            request.spacing2 = spacing2;
        }
        System.arraycopy(center, 0, request.center, 0, 3);
        if (listener.patternApply(request)) {
            close();
        }
    }

    private Double parseValue(EditText field, boolean length) {
        try {
            final BigDecimal typed = LengthUnit.parse(field.getText().toString());
            return length ? listener.patternUnit().toMeters(typed).doubleValue()
                    : typed.doubleValue();
        } catch (NumberFormatException notANumber) {
            return null;
        }
    }

    private void refuse(int labelRes, EditText field) {
        final Context context = getContext();
        final String raw = field.getText().toString().trim();
        listener.patternFieldRefused(raw.isEmpty()
                ? context.getString(R.string.field_empty, context.getString(labelRes))
                : context.getString(R.string.field_not_a_number, context.getString(labelRes), raw));
        field.requestFocus();
    }

    private static LinearLayout row(Context context, int topMargin) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        final LayoutParams params = new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = topMargin;
        row.setLayoutParams(params);
        return row;
    }

    private static TextView label(Context context) {
        final TextView label = new TextView(context);
        label.setSingleLine(true);
        label.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextSecondary));
        label.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_label));
        EditorControlStyles.applyMediumWeight(label);
        return label;
    }

    private static EditText field(Context context, int id, boolean decimal) {
        final EditText made = new EditText(context);
        made.setId(id);
        made.setSingleLine(true);
        made.setBackgroundResource(R.drawable.bg_field);
        final int padH = EditorControlStyles.dimen(context, R.dimen.field_padding_horizontal);
        final int padV = EditorControlStyles.dimen(context, R.dimen.field_padding_vertical);
        made.setPadding(padH, padV, padH, padV);
        if (decimal) {
            made.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL
                    | InputType.TYPE_NUMBER_FLAG_SIGNED);
            made.setKeyListener(DigitsKeyListener.getInstance("0123456789.-"));
        } else {
            made.setInputType(InputType.TYPE_CLASS_NUMBER);
        }
        made.setImeOptions(EditorInfo.IME_ACTION_DONE);
        made.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        made.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        made.setMinimumWidth(EditorControlStyles.dimen(context, decimal
                ? R.dimen.cad_canvas_field : R.dimen.control_height));
        made.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        return made;
    }
}
