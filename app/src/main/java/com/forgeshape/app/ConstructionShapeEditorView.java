package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The exact-value editor for what the Construction Body <i>is</i>.
 *
 * <p>Which primitive, that primitive's own parameters, the display unit they
 * are written in, and one Apply that submits the lot. It is one half of the
 * Construction inspector; where the object <i>sits</i> is the other half and
 * has its own editor and its own Apply, because the two have different
 * consequences — a shape change republishes the mesh and a placement change
 * cannot.
 *
 * <p><b>Presentation and input only.</b> It holds field text and a draft
 * primitive kind; it holds no dimension. The authoritative values are native
 * doubles in meters, and the only way this class changes anything is to submit
 * a complete primitive at once through {@link NativeViewport} and accept the
 * verdict. Moving the primitive chooser changes nothing but which fields are on
 * screen.
 */
final class ConstructionShapeEditorView extends LinearLayout {

    /** Two-parameter primitive field order: slot 0 and slot 1 are always the
     *  same two fields in the same order for one primitive, whichever it is —
     *  so a row's slots never change meaning, only their labels do. For
     *  cylinder, cone and capsule that pair is a diameter and an axial length;
     *  for a plane it is a width and a depth, which is exactly why these two
     *  are named by POSITION rather than by "diameter"/"axial". */
    private static final int FIELD_0 = 0;
    private static final int FIELD_1 = 1;

    private static final int[] CHOOSER_IDS = {
            R.id.primitive_option_box, R.id.primitive_option_cylinder,
            R.id.primitive_option_sphere, R.id.primitive_option_cone,
            R.id.primitive_option_capsule, R.id.primitive_option_plane
    };
    private static final int[] CHOOSER_LABELS = {
            R.string.primitive_box, R.string.primitive_cylinder, R.string.primitive_sphere,
            R.string.primitive_cone, R.string.primitive_capsule, R.string.primitive_plane
    };
    /** Three per row rather than five or six: five (or six) chips across a
     *  compact window would each be narrower than a fingertip, and the last
     *  one clipped. */
    private static final int CHOOSER_COLUMNS = 3;

    private final InspectorHost host;
    private final ObjectsSectionView objects;

    private final TextView[] chooserChips = new TextView[CHOOSER_IDS.length];
    private final View[] parameterRows = new View[CHOOSER_IDS.length];
    private final NumericPropertyRow[] boxFields = new NumericPropertyRow[3];
    private final NumericPropertyRow[] cylinderFields = new NumericPropertyRow[2];
    private final NumericPropertyRow[] sphereFields = new NumericPropertyRow[1];
    private final NumericPropertyRow[] coneFields = new NumericPropertyRow[2];
    private final NumericPropertyRow[] capsuleFields = new NumericPropertyRow[2];
    private final NumericPropertyRow[] planeFields = new NumericPropertyRow[2];
    private final UnitChipsView unitChips;

    /** Reused across reads; native fills this with authoritative values. */
    private final double[] nativePrimitive = new double[NativeViewport.PRIMITIVE_STATE_SIZE];

    ConstructionShapeEditorView(Context context, final InspectorHost host) {
        super(context);
        this.host = host;
        setOrientation(VERTICAL);

        // The Objects section sits above the shape controls because it decides
        // WHICH body everything below it edits. Reading top to bottom the panel
        // now says: this body, this shape, these dimensions, apply.
        objects = new ObjectsSectionView(context, host);
        addView(objects, EditorControlStyles.rowParams(0));

        addView(buildChooser(context), EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.section_gap)));

        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        parameterRows[NativeViewport.PRIMITIVE_BOX] = buildRow(context, R.id.primitive_row_box,
                boxFields,
                new int[]{R.id.field_box_width, R.id.field_box_height, R.id.field_box_depth},
                new int[]{R.string.label_width, R.string.label_height, R.string.label_depth});
        parameterRows[NativeViewport.PRIMITIVE_CYLINDER] = buildRow(context,
                R.id.primitive_row_cylinder, cylinderFields,
                new int[]{R.id.field_cylinder_diameter, R.id.field_cylinder_height},
                new int[]{R.string.label_diameter, R.string.label_height});
        parameterRows[NativeViewport.PRIMITIVE_SPHERE] = buildRow(context,
                R.id.primitive_row_sphere, sphereFields,
                new int[]{R.id.field_sphere_diameter},
                new int[]{R.string.label_diameter});
        parameterRows[NativeViewport.PRIMITIVE_CONE] = buildRow(context, R.id.primitive_row_cone,
                coneFields,
                new int[]{R.id.field_cone_diameter, R.id.field_cone_height},
                new int[]{R.string.label_diameter, R.string.label_height});
        parameterRows[NativeViewport.PRIMITIVE_CAPSULE] = buildRow(context,
                R.id.primitive_row_capsule, capsuleFields,
                new int[]{R.id.field_capsule_diameter, R.id.field_capsule_total_height},
                new int[]{R.string.label_diameter, R.string.label_total_height});
        parameterRows[NativeViewport.PRIMITIVE_PLANE] = buildRow(context,
                R.id.primitive_row_plane, planeFields,
                new int[]{R.id.field_plane_width, R.id.field_plane_depth},
                new int[]{R.string.label_width, R.string.label_depth});
        for (View row : parameterRows) {
            addView(row, EditorControlStyles.rowParams(gap));
        }

        addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.unit_selector)), EditorControlStyles.rowParams(gap));
        unitChips = new UnitChipsView(context, new UnitChipsView.OnUnitSelected() {
            @Override
            public void onUnitSelected(LengthUnit unit) {
                host.onDisplayUnitRequested(unit);
            }
        });
        addView(unitChips, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        final TextView apply = EditorControlStyles.primaryButton(context, R.id.apply_shape,
                context.getString(R.string.apply_shape));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApplyShape();
            }
        });
        final LinearLayout.LayoutParams applyParams = EditorControlStyles.rowParams(gap);
        applyParams.width = ViewGroup.LayoutParams.MATCH_PARENT;
        addView(apply, applyParams);

        refreshFromNative();
    }

    // -----------------------------------------------------------------------
    // View tree
    // -----------------------------------------------------------------------

    private View buildChooser(Context context) {
        final LinearLayout chooser = new LinearLayout(context);
        chooser.setId(R.id.primitive_chooser);
        chooser.setOrientation(VERTICAL);

        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        LinearLayout row = null;
        for (int i = 0; i < CHOOSER_IDS.length; i++) {
            if (i % CHOOSER_COLUMNS == 0) {
                row = new LinearLayout(context);
                row.setOrientation(HORIZONTAL);
                chooser.addView(row, EditorControlStyles.rowParams(i == 0 ? 0 : gap));
            }
            final int kind = i;
            final TextView chip = EditorControlStyles.chip(context, CHOOSER_IDS[i],
                    context.getString(CHOOSER_LABELS[i]));
            chip.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    onDraftKindSelected(kind);
                }
            });
            chooserChips[i] = chip;
            row.addView(chip, EditorControlStyles.evenShare(i % CHOOSER_COLUMNS == 0 ? 0 : gap));
        }
        // The last row is short; a trailing filler keeps the chips in it the
        // same width as the ones above rather than stretched across the panel.
        if (CHOOSER_IDS.length % CHOOSER_COLUMNS != 0 && row != null) {
            for (int i = CHOOSER_IDS.length % CHOOSER_COLUMNS; i < CHOOSER_COLUMNS; i++) {
                row.addView(new View(context), EditorControlStyles.evenShare(gap));
            }
        }
        return chooser;
    }

    private View buildRow(Context context, int rowId, NumericPropertyRow[] out,
                          int[] fieldIds, int[] labelRes) {
        final LinearLayout row = new LinearLayout(context);
        row.setId(rowId);
        row.setOrientation(HORIZONTAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        for (int i = 0; i < fieldIds.length; i++) {
            out[i] = new NumericPropertyRow(context, fieldIds[i],
                    context.getString(labelRes[i]), i == fieldIds.length - 1);
            row.addView(out[i], EditorControlStyles.evenShare(i == 0 ? 0 : gap));
        }
        return row;
    }

    // -----------------------------------------------------------------------
    // Draft primitive kind
    // -----------------------------------------------------------------------

    /**
     * Swaps which primitive's fields are on screen.
     *
     * <p>Pure presentation: no native call is made, so the object's kind, its
     * parameters, its transform, the mesh revision and the GPU upload count are
     * all untouched. The object becomes a cylinder only when Apply Shape
     * submits one and native code agrees.
     */
    private void onDraftKindSelected(int kind) {
        if (kind == host.uiState().draftPrimitiveKind()) {
            return;
        }
        host.uiState().setDraftPrimitiveKind(kind);
        showDraftKind();
        host.showStatus(getContext().getString(R.string.status_draft_kind, describeDraftKind()),
                R.color.text_secondary);
    }

    private void showDraftKind() {
        final int draft = host.uiState().draftPrimitiveKind();
        for (int i = 0; i < parameterRows.length; i++) {
            // Exactly one parameter row is on screen at a time, and it is
            // always the drafted kind's, so no field on screen can be read as
            // another primitive's parameter.
            parameterRows[i].setVisibility(i == draft ? VISIBLE : GONE);
            EditorControlStyles.setChipActive(chooserChips[i], i == draft);
        }
    }

    private String describeDraftKind() {
        return getContext().getString(CHOOSER_LABELS[host.uiState().draftPrimitiveKind()]);
    }

    // -----------------------------------------------------------------------
    // Display unit
    // -----------------------------------------------------------------------

    /**
     * Re-expresses every length in this editor in another unit.
     *
     * <p>Pure presentation: no native call, so the authoritative parameters,
     * the mesh revision and the GPU upload count are untouched. The conversion
     * is an exact decimal point shift, so switching back and forth reproduces
     * the original digits. Rotation is not this editor's concern and is never
     * converted anywhere — degrees are degrees in every length unit.
     *
     * @return whether every field was a number and was converted
     */
    boolean convertDisplayUnit(LengthUnit from, LengthUnit to) {
        boolean allConverted = true;
        for (NumericPropertyRow[] group : allFieldGroups()) {
            for (NumericPropertyRow field : group) {
                // Both the visible row and the hidden ones, so a draft that is
                // off screen does not silently change meaning while it is away.
                allConverted &= field.convertUnit(from, to);
            }
        }
        unitChips.showSelected(to);
        return allConverted;
    }

    private NumericPropertyRow[][] allFieldGroups() {
        return new NumericPropertyRow[][]{
                boxFields, cylinderFields, sphereFields, coneFields, capsuleFields, planeFields};
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    /**
     * Rewrites every field and the chooser from authoritative native state, in
     * the currently selected display unit.
     *
     * <p>This is the only source of the numbers and the kind shown at startup
     * and after a resume; nothing about the object's shape or size is stored or
     * defaulted here. It also resets the draft kind to what the object actually
     * is, so the chooser can never be left claiming a shape the object is not.
     */
    /** The Objects section, for tests that select a body by its ObjectId. */
    ObjectsSectionView objectsSection() { return objects; }

    void refreshFromNative() {
        // The Objects list first: it decides which body the fields below
        // describe, and it must never lag behind a viewport pick.
        objects.refreshFromNative();
        NativeViewport.constructionPrimitive(nativePrimitive);
        final LengthUnit unit = host.uiState().displayUnit();
        host.uiState().setDraftPrimitiveKind((int) nativePrimitive[0]);
        showDraftKind();

        for (int i = 0; i < boxFields.length; i++) {
            boxFields[i].setText(unit.format(
                    nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH + i]));
        }
        writeAxialPair(cylinderFields, NativeViewport.PRIMITIVE_CYLINDER_DIAMETER, unit);
        sphereFields[FIELD_0].setText(
                unit.format(nativePrimitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER]));
        writeAxialPair(coneFields, NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER, unit);
        writeAxialPair(capsuleFields, NativeViewport.PRIMITIVE_CAPSULE_DIAMETER, unit);
        writeAxialPair(planeFields, NativeViewport.PRIMITIVE_PLANE_WIDTH, unit);
        unitChips.showSelected(unit);
    }

    /** Writes two sequential native slots into a row's two fields, in order.
     *  Named for its main use — a diameter followed by an axial length, for
     *  cylinder, cone and capsule — but equally correct for any two sequential
     *  lengths in the same order, which is how the plane's width/depth pair
     *  reuses it below. */
    private void writeAxialPair(NumericPropertyRow[] fields, int firstSlot, LengthUnit unit) {
        fields[FIELD_0].setText(unit.format(nativePrimitive[firstSlot]));
        fields[FIELD_1].setText(unit.format(nativePrimitive[firstSlot + 1]));
    }

    // -----------------------------------------------------------------------
    // Apply Shape
    // -----------------------------------------------------------------------

    /**
     * Parses the drafted primitive's parameters, converts them to meters and
     * submits them together with the kind as one atomic native request.
     *
     * <p>Nothing is submitted unless every relevant field parses and is
     * positive: a request native code would refuse for a reason the UI already
     * knows about is not worth making, and a partial submission does not exist.
     * Native validation remains the final authority — invalid values are never
     * clamped or repaired here.
     *
     * <p>The transform is not part of this request and is never disturbed by it.
     */
    private void onApplyShape() {
        final Integer result = submitDraftShape();
        if (result == null) {
            return;  // a field was reported; nothing was submitted
        }
        switch (result) {
            case NativeViewport.APPLY_APPLIED:
                host.onNativeStateChanged();
                host.showStatus(getContext().getString(R.string.status_shape_applied,
                        describeShape()), R.color.text_success);
                host.finishEditing();
                break;
            case NativeViewport.APPLY_UNCHANGED:
                host.onNativeStateChanged();
                host.showStatus(getContext().getString(R.string.status_shape_unchanged,
                        describeShape()), R.color.text_secondary);
                host.finishEditing();
                break;
            case NativeViewport.APPLY_REJECTED_NOT_POSITIVE:
                reject(R.string.reject_not_positive);
                break;
            case NativeViewport.APPLY_REJECTED_NOT_FINITE:
                reject(R.string.reject_not_finite);
                break;
            case NativeViewport.APPLY_REJECTED_NOT_REPRESENTABLE:
                reject(R.string.reject_not_representable);
                break;
            case NativeViewport.APPLY_REJECTED_RELATION:
                // Today the only relation in the product is the capsule's, and
                // naming it is far more useful than "invalid dimensions".
                reject(R.string.reject_relation);
                break;
            default:
                reject(R.string.reject_shape_other);
                break;
        }
    }

    private void reject(int messageRes) {
        host.showStatus(getContext().getString(messageRes), R.color.text_error);
    }

    /**
     * Reads the drafted primitive's own fields and submits them through that
     * primitive's own native method.
     *
     * <p>Each branch reads exactly the fields that primitive has and calls the
     * method that takes exactly those parameters, so there is no point at which
     * a value is carried in a slot whose meaning depends on a separate kind.
     *
     * @return the {@code APPLY_*} status, or {@code null} when a field was
     *         reported and nothing was submitted
     */
    private Integer submitDraftShape() {
        final LengthUnit unit = host.uiState().displayUnit();
        switch (host.uiState().draftPrimitiveKind()) {
            case NativeViewport.PRIMITIVE_SPHERE: {
                final BigDecimal diameter = readField(sphereFields[FIELD_0], true);
                if (diameter == null) {
                    return null;
                }
                return NativeViewport.applyConstructionSphere(
                        unit.toMeters(diameter).doubleValue());
            }
            case NativeViewport.PRIMITIVE_CYLINDER: {
                final BigDecimal[] values = readAxialPair(cylinderFields);
                if (values == null) {
                    return null;
                }
                return NativeViewport.applyConstructionCylinder(
                        unit.toMeters(values[FIELD_0]).doubleValue(),
                        unit.toMeters(values[FIELD_1]).doubleValue());
            }
            case NativeViewport.PRIMITIVE_CONE: {
                final BigDecimal[] values = readAxialPair(coneFields);
                if (values == null) {
                    return null;
                }
                return NativeViewport.applyConstructionCone(
                        unit.toMeters(values[FIELD_0]).doubleValue(),
                        unit.toMeters(values[FIELD_1]).doubleValue());
            }
            case NativeViewport.PRIMITIVE_CAPSULE: {
                final BigDecimal[] values = readAxialPair(capsuleFields);
                if (values == null) {
                    return null;
                }
                // The relation totalHeight >= diameter is deliberately NOT
                // checked here. Native validation owns it, exactly as it owns
                // positivity for the values this editor can already name a
                // problem with: it refuses only what it can describe from the
                // text alone.
                return NativeViewport.applyConstructionCapsule(
                        unit.toMeters(values[FIELD_0]).doubleValue(),
                        unit.toMeters(values[FIELD_1]).doubleValue());
            }
            case NativeViewport.PRIMITIVE_PLANE: {
                final BigDecimal[] values = readAxialPair(planeFields);
                if (values == null) {
                    return null;
                }
                return NativeViewport.applyConstructionPlane(
                        unit.toMeters(values[FIELD_0]).doubleValue(),
                        unit.toMeters(values[FIELD_1]).doubleValue());
            }
            default: {
                final BigDecimal[] displayed = new BigDecimal[boxFields.length];
                for (int i = 0; i < boxFields.length; i++) {
                    displayed[i] = readField(boxFields[i], true);
                    if (displayed[i] == null) {
                        return null;
                    }
                }
                return NativeViewport.applyConstructionBox(
                        unit.toMeters(displayed[0]).doubleValue(),
                        unit.toMeters(displayed[1]).doubleValue(),
                        unit.toMeters(displayed[2]).doubleValue());
            }
        }
    }

    /**
     * Reads two sequential fields from one primitive's own row, in order.
     *
     * <p>Shared by the cylinder, the cone and the capsule, whose two fields are
     * a diameter and an axial length, and by the plane, whose two fields are a
     * width and a depth — all four present exactly two lengths in exactly the
     * same two slots. It reads fields and reports on them; it decides nothing
     * about the shape, and the caller still hands the values to that
     * primitive's own native method, so the typed boundary is untouched.
     */
    private BigDecimal[] readAxialPair(NumericPropertyRow[] fields) {
        final BigDecimal diameter = readField(fields[FIELD_0], true);
        if (diameter == null) {
            return null;
        }
        final BigDecimal axial = readField(fields[FIELD_1], true);
        if (axial == null) {
            return null;
        }
        return new BigDecimal[]{diameter, axial};
    }

    /**
     * Parses one field, reporting and focusing it on failure.
     *
     * @return the parsed value, or {@code null} when the field was reported
     */
    private BigDecimal readField(NumericPropertyRow row, boolean positive) {
        final String raw = row.text();
        final BigDecimal value;
        try {
            value = LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            fail(row, raw.trim().isEmpty()
                    ? getContext().getString(R.string.field_empty, row.label())
                    : getContext().getString(R.string.field_not_a_number, row.label(),
                            raw.trim()));
            return null;
        }
        if (positive && value.signum() <= 0) {
            fail(row, getContext().getString(R.string.field_not_positive, row.label(),
                    host.uiState().displayUnit().label()));
            return null;
        }
        return value;
    }

    private void fail(NumericPropertyRow row, String message) {
        host.showStatus(message, R.color.text_error);
        row.focusForCorrection();
    }

    /** Describes the authoritative shape in the selected display unit. */
    private String describeShape() {
        NativeViewport.constructionPrimitive(nativePrimitive);
        final LengthUnit unit = host.uiState().displayUnit();
        final int kind = (int) nativePrimitive[0];
        switch (kind) {
            case NativeViewport.PRIMITIVE_SPHERE:
                return "sphere " + unit.format(
                        nativePrimitive[NativeViewport.PRIMITIVE_SPHERE_DIAMETER])
                        + " " + unit.label() + " dia";
            case NativeViewport.PRIMITIVE_CYLINDER:
                return describeAxialPair("cylinder",
                        NativeViewport.PRIMITIVE_CYLINDER_DIAMETER, "");
            case NativeViewport.PRIMITIVE_CONE:
                return describeAxialPair("cone",
                        NativeViewport.PRIMITIVE_CONE_BOTTOM_DIAMETER, "");
            case NativeViewport.PRIMITIVE_CAPSULE:
                return describeAxialPair("capsule",
                        NativeViewport.PRIMITIVE_CAPSULE_DIAMETER, " total");
            case NativeViewport.PRIMITIVE_PLANE:
                // Not describeAxialPair: "dia" is right for a diameter, wrong
                // for a width, so the plane gets its own "W x D" phrasing.
                return "plane " + unit.format(
                        nativePrimitive[NativeViewport.PRIMITIVE_PLANE_WIDTH])
                        + " x " + unit.format(
                                nativePrimitive[NativeViewport.PRIMITIVE_PLANE_WIDTH + 1])
                        + " " + unit.label();
            default:
                return "box " + unit.format(nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH])
                        + " x " + unit.format(
                                nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 1])
                        + " x " + unit.format(
                                nativePrimitive[NativeViewport.PRIMITIVE_BOX_WIDTH + 2])
                        + " " + unit.label();
        }
    }

    /**
     * Describes a diameter-plus-axial-length primitive from the array native
     * code just filled. {@code axialSuffix} is what makes a capsule's height
     * read as its <i>total</i> height rather than its middle.
     */
    private String describeAxialPair(String name, int diameterSlot, String axialSuffix) {
        final LengthUnit unit = host.uiState().displayUnit();
        return name + " " + unit.format(nativePrimitive[diameterSlot]) + " dia x "
                + unit.format(nativePrimitive[diameterSlot + 1]) + " " + unit.label()
                + axialSuffix;
    }

    /** Describes the object's current kind, for a message about the object
     *  rather than about the draft. */
    String describeNativeKind() {
        NativeViewport.constructionPrimitive(nativePrimitive);
        final int kind = (int) nativePrimitive[0];
        final int index = (kind >= 0 && kind < CHOOSER_LABELS.length)
                ? kind : NativeViewport.PRIMITIVE_BOX;
        return getContext().getString(CHOOSER_LABELS[index]).toLowerCase(java.util.Locale.US);
    }

    void clearEditFocus() {
        for (NumericPropertyRow[] group : allFieldGroups()) {
            for (NumericPropertyRow field : group) {
                field.clearEditFocus();
            }
        }
    }
}
