package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The exact-value editor for where the Construction Body <i>sits</i>.
 *
 * <p>Position in the shared display unit, rotation always in degrees, and one
 * Apply that submits all six as a single atomic request.
 *
 * <p>It is a separate editor from the shape editor and has a separate Apply on
 * purpose, because the two commits have different consequences: applying a
 * shape republishes the mesh, applying a placement <b>publishes no revision and
 * uploads nothing</b>. Merging them into one button would hide a domain rule
 * behind a convenience.
 *
 * <p><b>Presentation and input only.</b> It holds field text and no transform;
 * the authoritative position is double meters and the authoritative rotation is
 * double degrees, both native.
 */
final class ConstructionPlacementEditorView extends LinearLayout {

    private static final int[] POSITION_IDS = {R.id.field_pos_x, R.id.field_pos_y,
            R.id.field_pos_z};
    private static final int[] POSITION_LABELS = {R.string.label_pos_x, R.string.label_pos_y,
            R.string.label_pos_z};
    private static final int[] ROTATION_IDS = {R.id.field_rot_x, R.id.field_rot_y,
            R.id.field_rot_z};
    private static final int[] ROTATION_LABELS = {R.string.label_rot_x, R.string.label_rot_y,
            R.string.label_rot_z};

    private final InspectorHost host;
    private final TextView positionSectionLabel;
    private final NumericPropertyRow[] positionFields = new NumericPropertyRow[3];
    private final NumericPropertyRow[] rotationFields = new NumericPropertyRow[3];
    private final UnitChipsView unitChips;

    /** Reused across reads; native fills this with authoritative values:
     *  position X/Y/Z in meters then rotation X/Y/Z in degrees. */
    private final double[] nativeTransform = new double[6];

    ConstructionPlacementEditorView(Context context, final InspectorHost host) {
        super(context);
        this.host = host;
        setOrientation(VERTICAL);

        final int smallGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        // Position, Rotation and the unit are three separate questions, so each
        // heading gets the SECTION gap and only a heading's own fields sit at
        // the small gap beneath it. At one shared row gap the three groups ran
        // together and a heading was distinguishable from a field caption by
        // capitalisation alone.
        final int sectionGap = EditorControlStyles.dimen(context, R.dimen.section_gap);

        positionSectionLabel = EditorControlStyles.sectionLabel(context, positionSectionTitle());
        addView(positionSectionLabel, EditorControlStyles.rowParams(0));
        addView(buildRow(context, positionFields, POSITION_IDS, POSITION_LABELS),
                EditorControlStyles.rowParams(smallGap));

        addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.section_rotation)),
                EditorControlStyles.rowParams(sectionGap));
        addView(buildRow(context, rotationFields, ROTATION_IDS, ROTATION_LABELS),
                EditorControlStyles.rowParams(smallGap));

        addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.unit_selector)),
                EditorControlStyles.rowParams(sectionGap));
        unitChips = new UnitChipsView(context, new UnitChipsView.OnUnitSelected() {
            @Override
            public void onUnitSelected(LengthUnit unit) {
                host.onDisplayUnitRequested(unit);
            }
        });
        addView(unitChips, EditorControlStyles.rowParams(smallGap));

        final TextView apply = EditorControlStyles.primaryButton(context, R.id.apply_transform,
                context.getString(R.string.apply_transform));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApplyTransform();
            }
        });
        final LinearLayout.LayoutParams applyParams =
                EditorControlStyles.rowParams(sectionGap);
        applyParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        addView(apply, applyParams);

        refreshFromNative();
    }

    private View buildRow(Context context, NumericPropertyRow[] out, int[] ids, int[] labelRes) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        for (int i = 0; i < ids.length; i++) {
            out[i] = new NumericPropertyRow(context, ids[i], context.getString(labelRes[i]),
                    i == ids.length - 1);
            row.addView(out[i], EditorControlStyles.evenShare(i == 0 ? 0 : gap));
        }
        return row;
    }

    private String positionSectionTitle() {
        return getContext().getString(R.string.section_position,
                host.uiState().displayUnit().label());
    }

    /**
     * Re-expresses the position fields in another unit, exactly.
     *
     * <p>Rotation is deliberately untouched: an angle is not a length, and
     * degrees are degrees whichever unit the lengths are written in.
     *
     * @return whether every position field was a number and was converted
     */
    boolean convertDisplayUnit(LengthUnit from, LengthUnit to) {
        boolean allConverted = true;
        for (NumericPropertyRow field : positionFields) {
            allConverted &= field.convertUnit(from, to);
        }
        positionSectionLabel.setText(getContext().getString(R.string.section_position,
                to.label()));
        unitChips.showSelected(to);
        return allConverted;
    }

    /**
     * Rewrites all six fields from authoritative native state: position in the
     * selected display unit, rotation in degrees.
     */
    void refreshFromNative() {
        NativeViewport.boxTransform(nativeTransform);
        final LengthUnit unit = host.uiState().displayUnit();
        for (int i = 0; i < 3; i++) {
            positionFields[i].setText(unit.format(nativeTransform[i]));
            // Degrees are not a length: they are presented as-is, in every unit.
            rotationFields[i].setText(
                    LengthUnit.present(BigDecimal.valueOf(nativeTransform[3 + i])));
        }
        positionSectionLabel.setText(positionSectionTitle());
        unitChips.showSelected(unit);
    }

    /**
     * Parses all six placement values and submits them as one atomic native
     * request.
     *
     * <p>Zero and negative are valid for all six — a coordinate is a place and
     * an angle is a direction, neither is a size — so the only thing refused
     * here is text that is not a number at all.
     *
     * <p>The shape is not part of this request and is never disturbed by it,
     * and a placement that lands publishes no mesh revision.
     */
    private void onApplyTransform() {
        final BigDecimal[] position = new BigDecimal[3];
        final BigDecimal[] rotation = new BigDecimal[3];
        for (int i = 0; i < 3; i++) {
            position[i] = readField(positionFields[i]);
            if (position[i] == null) {
                return;
            }
        }
        for (int i = 0; i < 3; i++) {
            rotation[i] = readField(rotationFields[i]);
            if (rotation[i] == null) {
                return;
            }
        }

        final LengthUnit unit = host.uiState().displayUnit();
        switch (NativeViewport.applyBoxTransform(
                unit.toMeters(position[0]).doubleValue(),
                unit.toMeters(position[1]).doubleValue(),
                unit.toMeters(position[2]).doubleValue(),
                rotation[0].doubleValue(), rotation[1].doubleValue(),
                rotation[2].doubleValue())) {
            case NativeViewport.APPLY_APPLIED:
                host.onNativeStateChanged();
                host.showStatus(getContext().getString(R.string.status_transform_applied,
                        describeTransform()), R.attr.fsTextSuccess);
                host.finishEditing();
                break;
            case NativeViewport.APPLY_UNCHANGED:
                host.onNativeStateChanged();
                host.showStatus(getContext().getString(R.string.status_transform_unchanged,
                        describeTransform()), R.attr.fsTextSecondary);
                host.finishEditing();
                break;
            case NativeViewport.APPLY_REJECTED_NOT_REPRESENTABLE:
                host.showStatus(getContext().getString(
                        R.string.reject_transform_not_representable), R.attr.fsTextError);
                break;
            default:
                host.showStatus(getContext().getString(R.string.reject_transform_other),
                        R.attr.fsTextError);
                break;
        }
    }

    /**
     * Parses one field, reporting and focusing it on failure.
     *
     * <p>No positivity check anywhere in this editor: every one of these six
     * values is legitimately zero or negative.
     */
    private BigDecimal readField(NumericPropertyRow row) {
        final String raw = row.text();
        try {
            return LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                            ? getContext().getString(R.string.field_empty, row.label())
                            : getContext().getString(R.string.field_not_a_number, row.label(),
                                    raw.trim()),
                    R.attr.fsTextError);
            row.focusForCorrection();
            return null;
        }
    }

    /** Describes the authoritative transform in the selected display unit. */
    private String describeTransform() {
        NativeViewport.boxTransform(nativeTransform);
        final LengthUnit unit = host.uiState().displayUnit();
        return unit.format(nativeTransform[0]) + ", " + unit.format(nativeTransform[1]) + ", "
                + unit.format(nativeTransform[2]) + " " + unit.label() + " @ "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[3])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[4])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[5])) + " deg";
    }

    void clearEditFocus() {
        for (NumericPropertyRow field : positionFields) {
            field.clearEditFocus();
        }
        for (NumericPropertyRow field : rotationFields) {
            field.clearEditFocus();
        }
    }
}
