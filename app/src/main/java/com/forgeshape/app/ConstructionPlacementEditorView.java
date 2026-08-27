package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The exact-value editor for where the Construction Body <i>sits</i> and how
 * large it is drawn.
 *
 * <p>Position in the shared display unit, rotation always in degrees, scale
 * always unitless, and one Apply that submits all nine as a single atomic
 * request.
 *
 * <p>It is a separate editor from the shape editor and has a separate Apply on
 * purpose, because the two commits have different consequences: applying a
 * shape republishes the mesh, applying a placement <b>publishes no revision and
 * uploads nothing</b>. Merging them into one button would hide a domain rule
 * behind a convenience. Scale belongs on this side of that split for exactly
 * that reason: it is a multiplier on a derived matrix, never a dimension, and a
 * 2 m box at scale 2 still has a Construction width of 2 m.
 *
 * <p><b>Presentation and input only.</b> It holds field text and no transform;
 * the authoritative position is double meters, the authoritative rotation is
 * double degrees and the authoritative scale is a unitless double, all native.
 */
final class ConstructionPlacementEditorView extends LinearLayout
        implements PropertyInspectorView.PinnedCommit {

    private static final int[] POSITION_IDS = {R.id.field_pos_x, R.id.field_pos_y,
            R.id.field_pos_z};
    private static final int[] POSITION_LABELS = {R.string.label_pos_x, R.string.label_pos_y,
            R.string.label_pos_z};
    private static final int[] ROTATION_IDS = {R.id.field_rot_x, R.id.field_rot_y,
            R.id.field_rot_z};
    private static final int[] ROTATION_LABELS = {R.string.label_rot_x, R.string.label_rot_y,
            R.string.label_rot_z};
    private static final int[] SCALE_IDS = {R.id.field_scale_x, R.id.field_scale_y,
            R.id.field_scale_z};
    private static final int[] SCALE_LABELS = {R.string.label_scale_x, R.string.label_scale_y,
            R.string.label_scale_z};

    private final InspectorHost host;
    private final TextView positionSectionLabel;
    private final NumericPropertyRow[] positionFields = new NumericPropertyRow[3];
    private final NumericPropertyRow[] rotationFields = new NumericPropertyRow[3];
    private final NumericPropertyRow[] scaleFields = new NumericPropertyRow[3];
    private final UnitChipsView unitChips;

    /** Apply. Lives in the panel footer rather than in this stack; see
     *  {@link #commitControl()}. */
    private final TextView apply;

    /** Reused across reads; native fills this with authoritative values:
     *  position X/Y/Z in meters, rotation X/Y/Z in degrees, scale X/Y/Z. */
    private final double[] nativeTransform = new double[NativeViewport.TRANSFORM_SIZE];

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

        // The unit chips belong to POSITION and sit inside its group, under the
        // fields they convert.
        //
        // They used to sit after Scale, under a heading reading "Display unit",
        // which put a millimetre / centimetre / metre choice directly beneath
        // the one group in this panel that is unitless by a hard product rule. A
        // reader has no way to know from the layout that the chips skip the two
        // groups between them and the fields they govern — and a Scale that
        // appears to offer units contradicts what a scale IS. Rotation is
        // untouched by them for the same reason: an angle is not a length.
        unitChips = new UnitChipsView(context, new UnitChipsView.OnUnitSelected() {
            @Override
            public void onUnitSelected(LengthUnit unit) {
                host.onDisplayUnitRequested(unit);
            }
        });
        addView(EditorControlStyles.sectionLabel(context,
                        context.getString(R.string.unit_selector_position)),
                EditorControlStyles.rowParams(smallGap));
        addView(unitChips, EditorControlStyles.rowParams(smallGap));

        addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.section_rotation)),
                EditorControlStyles.rowParams(sectionGap));
        addView(buildRow(context, rotationFields, ROTATION_IDS, ROTATION_LABELS),
                EditorControlStyles.rowParams(smallGap));

        // Scale is LAST, in the order the transform composes: place, turn, size.
        // It deliberately carries NO unit in its heading and nothing below it
        // offers one either — a multiplier is not a length, and a unit control
        // seated under this row would say it was.
        addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.section_scale)),
                EditorControlStyles.rowParams(sectionGap));
        addView(buildRow(context, scaleFields, SCALE_IDS, SCALE_LABELS),
                EditorControlStyles.rowParams(smallGap));

        // Apply is NOT added to this stack. It is handed to the panel, which
        // pins it below the scrolling body — see commitControl(). In compact
        // portrait it was three swipes below the fold, and a commit the user
        // cannot see is a commit they do not know they have to make.
        apply = EditorControlStyles.primaryButton(context, R.id.apply_transform,
                context.getString(R.string.apply_transform));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApplyTransform();
            }
        });

        refreshFromNative();
    }

    /**
     * The row that owns a field id, or null.
     *
     * <p>For verification. The COMPLETE value lives on the row; the
     * {@code EditText} under it may be drawing a shortened form of it, and a
     * case about long values has to be able to read both. See
     * {@link NumericPropertyRow}.
     */
    NumericPropertyRow rowFor(int fieldId) {
        for (NumericPropertyRow[] group :
                new NumericPropertyRow[][]{positionFields, rotationFields, scaleFields}) {
            for (NumericPropertyRow row : group) {
                if (row.field().getId() == fieldId) {
                    return row;
                }
            }
        }
        return null;
    }

    /**
     * Apply, for the panel to pin below its scrolling body.
     *
     * <p>The commit belongs to this editor — it is the act, and the editor owns
     * what it means — but WHERE it is drawn is the panel's, because only the
     * panel knows how much of the body it is showing. See
     * {@link PropertyInspectorView.PinnedCommit}.
     */
    @Override
    public View commitControl() {
        return apply;
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
     * <p>Rotation and scale are deliberately untouched: an angle is not a
     * length and a multiplier is not a length either, so degrees stay degrees
     * and a scale of 2 stays 2 whichever unit the lengths are written in.
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
     * Rewrites all nine fields from authoritative native state: position in the
     * selected display unit, rotation in degrees, scale as a bare multiplier.
     *
     * <p>This is also what a gizmo drag comes back through: the handles write
     * the same authoritative transform these fields read, so the two can never
     * disagree about where the body is.
     */
    void refreshFromNative() {
        NativeViewport.boxTransform(nativeTransform);
        final LengthUnit unit = host.uiState().displayUnit();
        for (int i = 0; i < 3; i++) {
            positionFields[i].setText(
                    unit.format(nativeTransform[NativeViewport.TRANSFORM_POSITION + i]));
            // Degrees are not a length: they are presented as-is, in every unit.
            rotationFields[i].setText(LengthUnit.present(
                    BigDecimal.valueOf(nativeTransform[NativeViewport.TRANSFORM_ROTATION + i])));
            // And a multiplier is not a length either.
            scaleFields[i].setText(LengthUnit.present(
                    BigDecimal.valueOf(nativeTransform[NativeViewport.TRANSFORM_SCALE + i])));
        }
        positionSectionLabel.setText(positionSectionTitle());
        unitChips.showSelected(unit);
    }

    /**
     * Parses all nine placement values and submits them as ONE atomic native
     * request.
     *
     * <p>Zero and negative are valid for every position and every rotation — a
     * coordinate is a place and an angle is a direction, neither is a size — so
     * for those six the only thing refused here is text that is not a number at
     * all. A SCALE is the exception, and the refusal is native: zero would make
     * the transform singular and negative would be a Mirror, which this product
     * does not have. This layer does not re-implement that rule; it submits the
     * value and reports what native code decided.
     *
     * <p>One Apply is one history step and one atomic write, so a bad scale
     * cannot leave a good position half applied.
     *
     * <p>The shape is not part of this request and is never disturbed by it,
     * and a placement that lands publishes no mesh revision.
     */
    private void onApplyTransform() {
        final BigDecimal[] position = new BigDecimal[3];
        final BigDecimal[] rotation = new BigDecimal[3];
        final BigDecimal[] scale = new BigDecimal[3];
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
        for (int i = 0; i < 3; i++) {
            scale[i] = readField(scaleFields[i]);
            if (scale[i] == null) {
                return;
            }
        }

        final LengthUnit unit = host.uiState().displayUnit();
        switch (NativeViewport.applyBoxTransform(
                unit.toMeters(position[0]).doubleValue(),
                unit.toMeters(position[1]).doubleValue(),
                unit.toMeters(position[2]).doubleValue(),
                rotation[0].doubleValue(), rotation[1].doubleValue(),
                rotation[2].doubleValue(),
                scale[0].doubleValue(), scale[1].doubleValue(), scale[2].doubleValue())) {
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
            case NativeViewport.APPLY_REJECTED_NOT_POSITIVE:
                host.showStatus(getContext().getString(
                        R.string.reject_transform_scale_not_positive), R.attr.fsTextError);
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
     * <p>The only check here is that the text is a number. Positivity is a
     * DOMAIN rule and it stays below JNI: duplicating it on this side would make
     * two places able to disagree about what a valid scale is.
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
        final int p = NativeViewport.TRANSFORM_POSITION;
        final int r = NativeViewport.TRANSFORM_ROTATION;
        final int s = NativeViewport.TRANSFORM_SCALE;
        return unit.format(nativeTransform[p]) + ", " + unit.format(nativeTransform[p + 1]) + ", "
                + unit.format(nativeTransform[p + 2]) + " " + unit.label() + " @ "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[r])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[r + 1])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[r + 2])) + " deg"
                // No unit after the scale, deliberately: a multiplier has none.
                + " x " + LengthUnit.present(BigDecimal.valueOf(nativeTransform[s])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[s + 1])) + ", "
                + LengthUnit.present(BigDecimal.valueOf(nativeTransform[s + 2]));
    }

    void clearEditFocus() {
        for (NumericPropertyRow field : positionFields) {
            field.clearEditFocus();
        }
        for (NumericPropertyRow field : rotationFields) {
            field.clearEditFocus();
        }
        for (NumericPropertyRow field : scaleFields) {
            field.clearEditFocus();
        }
    }
}
