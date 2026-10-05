package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The exact-value editor for what a CAD Body <i>is</i> (CAD-R0-A1A2).
 *
 * <p>A CAD Body is a sketch extruded along its workplane's normal, so its
 * editable truth is the profile's sizes — a rectangle's width and height, or a
 * circle's radius — the extrusion depth and its direction. One Apply submits
 * them together and is one history step; the body's mesh is regenerated from
 * the new truth and the placement is untouched, exactly as a primitive edit
 * leaves it.
 *
 * <p>A polygon profile (a closed polyline or a loop of lines) has no size to
 * type in this version; the panel says so and offers the depth alone.
 *
 * <p><b>Presentation and input only.</b> It holds field text; it holds no size
 * and no depth. Every number is read back from the active body, and the only
 * way this class changes anything is to submit a complete request through
 * {@link NativeViewport} and accept the verdict.
 */
final class CadFeatureEditorView extends LinearLayout
        implements PropertyInspectorView.PinnedCommit {

    private final InspectorHost host;
    private final double[] nativeCad = new double[NativeViewport.CAD_STATE_SIZE];

    private final TextView planeSummary;
    private final TextView profileSummary;
    private final LinearLayout rectangleRow;
    private final LinearLayout circleRow;
    private final NumericPropertyRow[] rectangleFields = new NumericPropertyRow[2];
    private final NumericPropertyRow[] circleFields = new NumericPropertyRow[1];
    private final NumericPropertyRow depthField;
    /** The extrusion's section label, withdrawn with its rows over a revolved body. */
    private final TextView extrudeLabel;
    private final TextView directionAlong;
    private final TextView directionAgainst;
    /**
     * The side chips' row, WITHDRAWN for a body whose extent names both sides
     * (`CAD-EXT-R1`). Symmetric already reaches both and Two Sides states both
     * explicitly, so there is no side left to choose; native refuses a
     * direction change there too, and this is the control not being drawn where
     * it could not succeed.
     */
    private final LinearLayout directionRow;
    private final UnitChipsView unitChips;
    private final TextView apply;
    /** Reopens the body's sketch for editing (`SKETCH-UX-R1` F1). */
    private final TextView editSketch;
    /**
     * The body's features, in chain order (`CAD-VERTICAL-SLICE-R1`): the first
     * sketch and extrusion, then each Add and Cut. A row is the way back into
     * that feature's own sketch and extrusion; the list holds no feature of its
     * own and is rebuilt from native on every refresh.
     */
    private final LinearLayout featureList;
    private final double[] featureInfo = new double[NativeViewport.CAD_FEATURE_INFO_SIZE];

    private int draftDirection = NativeViewport.EXTRUDE_ALONG_NORMAL;
    private int profileKind = NativeViewport.CAD_PROFILE_NONE;

    CadFeatureEditorView(Context context, final InspectorHost host) {
        super(context);
        this.host = host;
        setId(R.id.cad_editor);
        setOrientation(VERTICAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        final int smallGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        final int sectionGap = EditorControlStyles.dimen(context, R.dimen.section_gap);

        planeSummary = EditorControlStyles.titleText(context, R.id.cad_plane_summary, "");
        addView(planeSummary, EditorControlStyles.rowParams(0));
        profileSummary = EditorControlStyles.titleText(context, R.id.cad_profile_summary, "");
        addView(profileSummary, EditorControlStyles.rowParams(smallGap));

        rectangleRow = buildRow(context, R.id.cad_row_rectangle, rectangleFields,
                new int[]{R.id.field_cad_rect_width, R.id.field_cad_rect_height},
                new int[]{R.string.label_width, R.string.label_height});
        circleRow = buildRow(context, R.id.cad_row_circle, circleFields,
                new int[]{R.id.field_cad_circle_radius}, new int[]{R.string.label_radius});
        addView(rectangleRow, EditorControlStyles.rowParams(gap));
        addView(circleRow, EditorControlStyles.rowParams(gap));

        extrudeLabel = EditorControlStyles.sectionLabel(context,
                context.getString(R.string.sketch_extrude_section));
        addView(extrudeLabel, EditorControlStyles.rowParams(sectionGap));
        depthField = new NumericPropertyRow(context, R.id.field_cad_depth,
                context.getString(R.string.label_depth), true);
        addView(depthField, EditorControlStyles.rowParams(smallGap));
        final LinearLayout directions = new LinearLayout(context);
        directions.setOrientation(HORIZONTAL);
        directionAlong = EditorControlStyles.chip(context, R.id.cad_direction_along,
                context.getString(R.string.extrude_direction_along));
        directionAgainst = EditorControlStyles.chip(context, R.id.cad_direction_against,
                context.getString(R.string.extrude_direction_against));
        directionAlong.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                draftDirection = NativeViewport.EXTRUDE_ALONG_NORMAL;
                showDirection();
            }
        });
        directionAgainst.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                draftDirection = NativeViewport.EXTRUDE_AGAINST_NORMAL;
                showDirection();
            }
        });
        directions.addView(directionAlong, EditorControlStyles.evenShare(0));
        directions.addView(directionAgainst, EditorControlStyles.evenShare(smallGap));
        directionRow = directions;
        addView(directions, EditorControlStyles.rowParams(gap));

        // Edit Sketch (`SKETCH-UX-R1` F1). The panel above types the sizes this
        // build can express as numbers; this reopens the SKETCH itself, which is
        // where a profile's shape — a curve, an extra edge, a line's exact
        // length — is changed. One control, because a CAD Body has exactly one
        // sketch and needs no feature tree to say which.
        editSketch = EditorControlStyles.secondaryActionChip(context, R.id.edit_cad_sketch,
                context.getString(R.string.edit_sketch));
        editSketch.setContentDescription(context.getString(R.string.edit_sketch) + ". "
                + context.getString(R.string.edit_sketch_description));
        editSketch.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                host.onEditCadSketchRequested();
            }
        });
        final LinearLayout.LayoutParams editParams = EditorControlStyles.rowParams(sectionGap);
        editParams.width = LayoutParams.MATCH_PARENT;
        addView(editSketch, editParams);

        featureList = new LinearLayout(context);
        featureList.setId(R.id.cad_feature_list);
        featureList.setOrientation(VERTICAL);
        addView(featureList, EditorControlStyles.rowParams(sectionGap));

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.unit_selector)),
                EditorControlStyles.rowParams(sectionGap));
        unitChips = new UnitChipsView(context, new UnitChipsView.OnUnitSelected() {
            @Override
            public void onUnitSelected(LengthUnit unit) {
                host.onDisplayUnitRequested(unit);
            }
        });
        addView(unitChips, EditorControlStyles.rowParams(smallGap));

        apply = EditorControlStyles.primaryButton(context, R.id.apply_cad,
                context.getString(R.string.apply_cad));
        apply.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApply();
            }
        });

        refreshFromNative();
    }

    private LinearLayout buildRow(Context context, int rowId, NumericPropertyRow[] out,
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

    @Override
    public View commitControl() {
        return apply;
    }

    /** Rewrites every field from the active CAD Body. A no-op for any other body. */
    void refreshFromNative() {
        if (!NativeViewport.cadState(nativeCad)) {
            return;
        }
        final Context context = getContext();
        final LengthUnit unit = host.uiState().displayUnit();
        final int plane = (int) nativeCad[NativeViewport.CAD_PLANE];
        planeSummary.setText(context.getString(R.string.cad_plane_summary,
                context.getString(planeName(plane))));
        profileKind = (int) nativeCad[NativeViewport.CAD_PROFILE_KIND];
        rectangleRow.setVisibility(GONE);
        circleRow.setVisibility(GONE);
        switch (profileKind) {
            case NativeViewport.CAD_PROFILE_RECTANGLE:
                profileSummary.setText(context.getString(R.string.cad_profile_rectangle));
                rectangleRow.setVisibility(VISIBLE);
                rectangleFields[0].setText(unit.format(nativeCad[NativeViewport.CAD_PRIMARY_SIZE]));
                rectangleFields[1].setText(
                        unit.format(nativeCad[NativeViewport.CAD_SECONDARY_SIZE]));
                break;
            case NativeViewport.CAD_PROFILE_CIRCLE:
                profileSummary.setText(context.getString(R.string.cad_profile_circle));
                circleRow.setVisibility(VISIBLE);
                circleFields[0].setText(unit.format(nativeCad[NativeViewport.CAD_PRIMARY_SIZE]));
                break;
            default:
                profileSummary.setText(context.getString(R.string.cad_profile_polygon,
                        (int) nativeCad[NativeViewport.CAD_PROFILE_VERTICES]));
                break;
        }
        // A REVOLVED body (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`) has no extrusion and
        // no size this panel can type: its truth is the revolve feature, which
        // its row below reopens. The extrude rows and Apply are withdrawn
        // rather than drawn and then refused.
        final boolean revolved =
                (int) nativeCad[NativeViewport.CAD_STATE_KIND] == NativeViewport.FEATURE_KIND_REVOLVE;
        extrudeLabel.setVisibility(revolved ? GONE : VISIBLE);
        depthField.setVisibility(revolved ? GONE : VISIBLE);
        apply.setVisibility(revolved ? GONE : VISIBLE);
        if (revolved) {
            rectangleRow.setVisibility(GONE);
            circleRow.setVisibility(GONE);
            directionRow.setVisibility(GONE);
            profileSummary.setText(context.getString(R.string.cad_profile_revolve,
                    CadRevolvePresentation.label(nativeCad[NativeViewport.CAD_STATE_REVOLVE_ANGLE])));
            unitChips.showSelected(unit);
            refreshFeatures(context);
            return;
        }
        // The extent is authored ON THE CANVAS, where both sides are visible;
        // this panel edits the PRIMARY distance of whatever mode the body has
        // and withdraws the side chips where there is no side to choose.
        final int extent = (int) nativeCad[NativeViewport.CAD_STATE_EXTENT];
        depthField.setText(unit.format(nativeCad[NativeViewport.CAD_DEPTH]));
        draftDirection = (int) nativeCad[NativeViewport.CAD_DIRECTION];
        directionRow.setVisibility(extent == NativeViewport.EXTENT_ONE_SIDE ? VISIBLE : GONE);
        showDirection();
        unitChips.showSelected(unit);
        refreshFeatures(context);
    }

    /**
     * Rebuilds the feature rows. Withdrawn for a body with ONE feature: its
     * sketch is what Edit Sketch above already reopens, and a list of one is
     * not a choice.
     */
    private void refreshFeatures(Context context) {
        featureList.removeAllViews();
        final long body = NativeViewport.sceneActiveBodyId();
        final int count = NativeViewport.cadFeatureCount(body);
        // One feature is not a choice -- unless it is a Revolve, whose row is
        // the one way back into its axis, angle and direction.
        final boolean revolved =
                (int) nativeCad[NativeViewport.CAD_STATE_KIND] == NativeViewport.FEATURE_KIND_REVOLVE;
        if (count <= 1 && !revolved) {
            featureList.setVisibility(GONE);
            return;
        }
        featureList.setVisibility(VISIBLE);
        featureList.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.cad_features_section)),
                EditorControlStyles.rowParams(0));
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        for (int i = 0; i < count; i++) {
            if (!NativeViewport.cadFeatureInfo(body, i, featureInfo)) {
                break;
            }
            final long featureId = (long) featureInfo[NativeViewport.CAD_FEATURE_ID];
            final int operation = (int) featureInfo[NativeViewport.CAD_FEATURE_OPERATION];
            final boolean revolveRow = (int) featureInfo[NativeViewport.CAD_FEATURE_KIND]
                    == NativeViewport.FEATURE_KIND_REVOLVE;
            final String label = revolveRow
                    ? context.getString(R.string.cad_feature_row_revolve, i + 1,
                            CadRevolvePresentation.label(featureInfo[NativeViewport.CAD_FEATURE_ANGLE]))
                    : context.getString(R.string.cad_feature_row, i + 1,
                            context.getString(operationName(operation)));
            final TextView row = EditorControlStyles.listRow(context, R.id.cad_feature_row,
                    label);
            row.setTag(Long.valueOf(featureId));
            row.setContentDescription(context.getString(R.string.cad_feature_row_description,
                    label));
            row.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    host.onEditCadFeatureRequested(featureId);
                }
            });
            final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(gap);
            params.width = LayoutParams.MATCH_PARENT;
            featureList.addView(row, params);
        }
    }

    static int operationName(int operation) {
        switch (operation) {
            case NativeViewport.OPERATION_ADD: return R.string.sketch_operation_add;
            case NativeViewport.OPERATION_CUT: return R.string.sketch_operation_cut;
            default: return R.string.sketch_operation_new_body;
        }
    }

    static int planeName(int plane) {
        switch (plane) {
            case NativeViewport.WORKPLANE_XZ: return R.string.workplane_xz;
            case NativeViewport.WORKPLANE_YZ: return R.string.workplane_yz;
            default: return R.string.workplane_xy;
        }
    }

    private void showDirection() {
        EditorControlStyles.setChipActive(directionAlong,
                draftDirection == NativeViewport.EXTRUDE_ALONG_NORMAL);
        EditorControlStyles.setChipActive(directionAgainst,
                draftDirection == NativeViewport.EXTRUDE_AGAINST_NORMAL);
    }

    /**
     * Submits the profile's sizes, the depth and the direction as ONE request.
     *
     * <p>Nothing is submitted unless every relevant field parses and is
     * positive; native validation is the final authority and repairs nothing.
     */
    private void onApply() {
        final Context context = getContext();
        final LengthUnit unit = host.uiState().displayUnit();
        final BigDecimal depth = readField(depthField);
        if (depth == null) {
            return;
        }
        final double depthMeters = unit.toMeters(depth).doubleValue();
        int result;
        switch (profileKind) {
            case NativeViewport.CAD_PROFILE_RECTANGLE: {
                final BigDecimal width = readField(rectangleFields[0]);
                if (width == null) return;
                final BigDecimal height = readField(rectangleFields[1]);
                if (height == null) return;
                result = NativeViewport.cadApplyRectangle(unit.toMeters(width).doubleValue(),
                        unit.toMeters(height).doubleValue(), depthMeters, draftDirection);
                break;
            }
            case NativeViewport.CAD_PROFILE_CIRCLE: {
                final BigDecimal radius = readField(circleFields[0]);
                if (radius == null) return;
                result = NativeViewport.cadApplyCircle(unit.toMeters(radius).doubleValue(),
                        depthMeters, draftDirection);
                break;
            }
            default:
                result = NativeViewport.cadApplyExtrude(depthMeters, draftDirection);
                break;
        }
        switch (result) {
            case NativeViewport.APPLY_APPLIED:
                host.onNativeStateChanged();
                host.showStatus(context.getString(R.string.status_cad_applied),
                        R.attr.fsTextSuccess);
                host.finishEditing();
                break;
            case NativeViewport.APPLY_UNCHANGED:
                host.onNativeStateChanged();
                host.showStatus(context.getString(R.string.status_cad_unchanged),
                        R.attr.fsTextSecondary);
                host.finishEditing();
                break;
            case NativeViewport.APPLY_REJECTED_NOT_POSITIVE:
                host.showStatus(context.getString(R.string.reject_not_positive),
                        R.attr.fsTextError);
                break;
            case NativeViewport.APPLY_REJECTED_NOT_FINITE:
                host.showStatus(context.getString(R.string.reject_not_finite),
                        R.attr.fsTextError);
                break;
            default:
                host.showStatus(CadStatusMessages.describe(context,
                        NativeViewport.cadLastStatus()), R.attr.fsTextError);
                break;
        }
    }

    private BigDecimal readField(NumericPropertyRow row) {
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
        if (value.signum() <= 0) {
            fail(row, getContext().getString(R.string.field_not_positive, row.label(),
                    host.uiState().displayUnit().label()));
            return null;
        }
        return value;
    }

    private void fail(NumericPropertyRow row, String message) {
        host.showStatus(message, R.attr.fsTextError);
        row.focusForCorrection();
    }

    boolean convertDisplayUnit(LengthUnit from, LengthUnit to) {
        boolean all = true;
        for (NumericPropertyRow field : rectangleFields) all &= field.convertUnit(from, to);
        for (NumericPropertyRow field : circleFields) all &= field.convertUnit(from, to);
        all &= depthField.convertUnit(from, to);
        unitChips.showSelected(to);
        return all;
    }

    void clearEditFocus() {
        for (NumericPropertyRow field : rectangleFields) field.clearEditFocus();
        for (NumericPropertyRow field : circleFields) field.clearEditFocus();
        depthField.clearEditFocus();
    }
}
