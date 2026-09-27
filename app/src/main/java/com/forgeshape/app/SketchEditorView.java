package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The precision surface of a sketch in progress (CAD-R0-A1A2).
 *
 * <p>Two bodies in one view, chosen by the native session's state and never by
 * anything remembered here:
 *
 * <ul>
 *   <li><b>Editing</b> — the selected entity's exact values (a rectangle's
 *       width and height, a circle's radius, a line's two endpoints), one Apply
 *       that submits them as typed, and Delete entity. A polyline's points are
 *       shown but not editable in this version, and it says so.</li>
 *   <li><b>Ready</b> — the closed profiles the sketch offers, the extrusion
 *       depth and its direction, and Extrude, which is the same act as the
 *       toolbar's Extrude and commits the sketch as one new CAD Body.</li>
 * </ul>
 *
 * <p><b>Presentation and input only.</b> It holds field text; it holds no
 * coordinate, no profile and no depth. Every number shown is read back from the
 * native session, every edit is submitted to it whole, and a typed value is
 * never snapped: the grid is a drawing aid and this panel is the way around it.
 *
 * <p>The pinned commit is ONE container whose content follows the state, so
 * the panel that pinned it once keeps showing the right act when the sketch
 * moves from Editing to Ready.
 */
final class SketchEditorView extends LinearLayout implements PropertyInspectorView.PinnedCommit {

    private final InspectorHost host;
    private final double[] nativeSketch = new double[NativeViewport.SKETCH_STATE_SIZE];
    private final double[] nativeEntity = new double[NativeViewport.SKETCH_ENTITY_SIZE];
    private final double[] profileInfo = new double[NativeViewport.SKETCH_REGION_INFO_SIZE];

    // --- Editing ---
    private final LinearLayout editingSection;
    private final TextView entitySummary;
    private final LinearLayout rectangleRow;
    private final LinearLayout circleRow;
    private final LinearLayout lineRow;
    private final NumericPropertyRow[] rectangleFields = new NumericPropertyRow[2];
    private final NumericPropertyRow[] circleFields = new NumericPropertyRow[1];
    private final NumericPropertyRow[] lineFields = new NumericPropertyRow[4];
    private final TextView deleteEntity;
    private final TextView applyEntity;

    // --- Ready ---
    private final LinearLayout readySection;
    private final LinearLayout profileChooser;
    /**
     * New Body, Add and Cut (`CAD-VERTICAL-SLICE-R1`), with their section
     * label: the group is withdrawn whole when it holds one choice. Only the operations
     * native says can be chosen now are drawn: a sketch on a world plane has
     * no body to add to or cut, so there the row carries New Body alone.
     */
    private final LinearLayout operationRow;
    private final TextView operationNewBody;
    private final TextView operationAdd;
    private final TextView operationCut;
    private final NumericPropertyRow depthField;
    private final TextView directionAlong;
    private final TextView directionAgainst;
    /**
     * The side chips' row, WITHDRAWN when the extent names both sides
     * (`CAD-EXT-R1`). The extent itself is authored ON THE CANVAS, where both
     * arrows are visible; this panel edits the primary distance of whatever
     * mode the session has and does not offer a side there is none of.
     */
    private final LinearLayout directionRow;
    private final TextView extrude;
    /** The manipulator state, re-read on every refresh; nothing is kept. */
    private final double[] nativeExtrude = new double[NativeViewport.CAD_EXTRUDE_SIZE];

    private final UnitChipsView unitChips;

    /** The one pinned commit; its content is swapped with the state. */
    private final FrameLayout commit;

    // There is deliberately NO draft direction here (`CAD-UX-S1` 4.8).
    //
    // A direction chip submits at once, so the field that used to mirror it was
    // a second copy of a value the session already owned — harmless at one
    // field, and exactly the wrong seed for an operation, a target and a second
    // distance later. The chips now show what native says on every refresh and
    // the depth submission reads the direction back from the same state, so the
    // canvas Flip and the panel chips cannot become two answers.

    /** Told to act; the workspace owns what the act means. */
    interface OnSketchAction {
        void onExtrudeRequested();

        /**
         * New Body, Add or Cut (`CAD-VERTICAL-SLICE-R1`) — the same act the
         * canvas operation badge sends, so the two cannot become two answers.
         */
        void onExtrudeOperationRequested(int operation);
    }

    private final OnSketchAction actions;

    SketchEditorView(Context context, final InspectorHost host, OnSketchAction actions) {
        super(context);
        this.host = host;
        this.actions = actions;
        setId(R.id.sketch_editor);
        setOrientation(VERTICAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        final int smallGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        final int sectionGap = EditorControlStyles.dimen(context, R.dimen.section_gap);

        // ----- Editing -----
        editingSection = new LinearLayout(context);
        editingSection.setOrientation(VERTICAL);
        editingSection.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.sketch_selected_section)),
                EditorControlStyles.rowParams(0));
        entitySummary = EditorControlStyles.titleText(context, R.id.sketch_entity_summary, "");
        editingSection.addView(entitySummary, EditorControlStyles.rowParams(smallGap));

        rectangleRow = buildRow(context, R.id.sketch_entity_row_rectangle, rectangleFields,
                new int[]{R.id.field_sketch_rect_width, R.id.field_sketch_rect_height},
                new int[]{R.string.label_width, R.string.label_height});
        circleRow = buildRow(context, R.id.sketch_entity_row_circle, circleFields,
                new int[]{R.id.field_sketch_circle_radius}, new int[]{R.string.label_radius});
        lineRow = new LinearLayout(context);
        lineRow.setId(R.id.sketch_entity_row_line);
        lineRow.setOrientation(VERTICAL);
        final NumericPropertyRow[] lineStart = new NumericPropertyRow[2];
        final NumericPropertyRow[] lineEnd = new NumericPropertyRow[2];
        lineRow.addView(buildRow(context, View.NO_ID, lineStart,
                new int[]{R.id.field_sketch_line_x0, R.id.field_sketch_line_y0},
                new int[]{R.string.label_start_x, R.string.label_start_y}),
                EditorControlStyles.rowParams(0));
        lineRow.addView(buildRow(context, View.NO_ID, lineEnd,
                new int[]{R.id.field_sketch_line_x1, R.id.field_sketch_line_y1},
                new int[]{R.string.label_end_x, R.string.label_end_y}),
                EditorControlStyles.rowParams(gap));
        lineFields[0] = lineStart[0];
        lineFields[1] = lineStart[1];
        lineFields[2] = lineEnd[0];
        lineFields[3] = lineEnd[1];
        editingSection.addView(rectangleRow, EditorControlStyles.rowParams(gap));
        editingSection.addView(circleRow, EditorControlStyles.rowParams(gap));
        editingSection.addView(lineRow, EditorControlStyles.rowParams(gap));

        deleteEntity = EditorControlStyles.secondaryActionChip(context, R.id.delete_sketch_entity,
                context.getString(R.string.delete_sketch_entity));
        deleteEntity.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
        deleteEntity.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onDeleteEntity();
            }
        });
        final LinearLayout.LayoutParams deleteParams = EditorControlStyles.rowParams(gap);
        deleteParams.width = LayoutParams.MATCH_PARENT;
        editingSection.addView(deleteEntity, deleteParams);
        addView(editingSection, EditorControlStyles.rowParams(0));

        applyEntity = EditorControlStyles.primaryButton(context, R.id.apply_sketch_entity,
                context.getString(R.string.apply_sketch_entity));
        applyEntity.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onApplyEntity();
            }
        });

        // ----- Ready -----
        readySection = new LinearLayout(context);
        readySection.setOrientation(VERTICAL);
        readySection.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.sketch_profile_section)),
                EditorControlStyles.rowParams(0));
        profileChooser = new LinearLayout(context);
        profileChooser.setId(R.id.sketch_profile_chooser);
        profileChooser.setOrientation(VERTICAL);
        readySection.addView(profileChooser, EditorControlStyles.rowParams(smallGap));
        operationRow = new LinearLayout(context);
        operationRow.setId(R.id.sketch_operation_row);
        operationRow.setOrientation(VERTICAL);
        operationRow.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.sketch_operation_section)),
                EditorControlStyles.rowParams(0));
        final LinearLayout operationChips = new LinearLayout(context);
        operationChips.setOrientation(HORIZONTAL);
        operationNewBody = operationChip(context, R.id.sketch_operation_new_body,
                R.string.sketch_operation_new_body, NativeViewport.OPERATION_NEW_BODY);
        operationAdd = operationChip(context, R.id.sketch_operation_add,
                R.string.sketch_operation_add, NativeViewport.OPERATION_ADD);
        operationCut = operationChip(context, R.id.sketch_operation_cut,
                R.string.sketch_operation_cut, NativeViewport.OPERATION_CUT);
        operationChips.addView(operationNewBody, EditorControlStyles.evenShare(0));
        operationChips.addView(operationAdd, EditorControlStyles.evenShare(smallGap));
        operationChips.addView(operationCut, EditorControlStyles.evenShare(smallGap));
        operationRow.addView(operationChips, EditorControlStyles.rowParams(smallGap));
        readySection.addView(operationRow, EditorControlStyles.rowParams(sectionGap));
        readySection.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.sketch_extrude_section)),
                EditorControlStyles.rowParams(sectionGap));
        depthField = new NumericPropertyRow(context, R.id.field_extrude_depth,
                context.getString(R.string.label_depth), true);
        readySection.addView(depthField, EditorControlStyles.rowParams(smallGap));
        final LinearLayout directions = new LinearLayout(context);
        directions.setOrientation(HORIZONTAL);
        directionAlong = EditorControlStyles.chip(context, R.id.extrude_direction_along,
                context.getString(R.string.extrude_direction_along));
        directionAgainst = EditorControlStyles.chip(context, R.id.extrude_direction_against,
                context.getString(R.string.extrude_direction_against));
        directionAlong.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onDirectionChosen(NativeViewport.EXTRUDE_ALONG_NORMAL);
            }
        });
        directionAgainst.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                onDirectionChosen(NativeViewport.EXTRUDE_AGAINST_NORMAL);
            }
        });
        directions.addView(directionAlong, EditorControlStyles.evenShare(0));
        directions.addView(directionAgainst, EditorControlStyles.evenShare(smallGap));
        directionRow = directions;
        readySection.addView(directions, EditorControlStyles.rowParams(gap));
        addView(readySection, EditorControlStyles.rowParams(0));

        extrude = EditorControlStyles.primaryButton(context, R.id.sketch_extrude_commit,
                context.getString(R.string.extrude));
        extrude.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                // The depth is submitted FIRST, so the commit extrudes what the
                // field says rather than what the session last held.
                if (submitDepth()) {
                    SketchEditorView.this.actions.onExtrudeRequested();
                }
            }
        });

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.unit_selector)),
                EditorControlStyles.rowParams(sectionGap));
        unitChips = new UnitChipsView(context, new UnitChipsView.OnUnitSelected() {
            @Override
            public void onUnitSelected(LengthUnit unit) {
                host.onDisplayUnitRequested(unit);
            }
        });
        addView(unitChips, EditorControlStyles.rowParams(smallGap));

        commit = new FrameLayout(context);
        refreshFromNative();
    }

    private TextView operationChip(Context context, int id, int labelRes, final int operation) {
        final TextView chip = EditorControlStyles.chip(context, id, context.getString(labelRes));
        chip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                actions.onExtrudeOperationRequested(operation);
            }
        });
        return chip;
    }

    private LinearLayout buildRow(Context context, int rowId, NumericPropertyRow[] out,
                                  int[] fieldIds, int[] labelRes) {
        final LinearLayout row = new LinearLayout(context);
        if (rowId != View.NO_ID) {
            row.setId(rowId);
        }
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
        return commit;
    }

    /** Whether the session is past editing, for the workspace's title and toggle. */
    boolean ready() {
        NativeViewport.sketchState(nativeSketch);
        return nativeSketch[NativeViewport.SKETCH_STATE] == NativeViewport.SKETCH_READY;
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    void refreshFromNative() {
        NativeViewport.sketchState(nativeSketch);
        final boolean ready =
                nativeSketch[NativeViewport.SKETCH_STATE] == NativeViewport.SKETCH_READY;
        final LengthUnit unit = host.uiState().displayUnit();
        editingSection.setVisibility(ready ? GONE : VISIBLE);
        readySection.setVisibility(ready ? VISIBLE : GONE);
        unitChips.showSelected(unit);

        // The pinned commit follows the state.
        final View wanted = ready ? extrude : applyEntity;
        if (commit.getChildCount() != 1 || commit.getChildAt(0) != wanted) {
            commit.removeAllViews();
            commit.addView(wanted, new FrameLayout.LayoutParams(
                    LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        }

        if (!ready) {
            refreshSelectedEntity(unit);
            return;
        }
        refreshProfiles();
        depthField.setText(unit.format(nativeSketch[NativeViewport.SKETCH_EXTRUDE_DEPTH]));
        NativeViewport.cadExtrudeToolState(nativeExtrude);
        refreshOperations();
        // The pinned Extrude follows the preview's verdict, on the toolbar's
        // terms: disabled while the candidate is not one a commit may make.
        final boolean committable = nativeExtrude[NativeViewport.CAD_EXTRUDE_CANDIDATE_STATUS]
                == NativeViewport.CAD_OK;
        extrude.setEnabled(committable);
        extrude.setAlpha(committable ? 1f : 0.45f);
        directionRow.setVisibility(
                nativeExtrude[NativeViewport.CAD_EXTRUDE_EXTENT] == NativeViewport.EXTENT_ONE_SIDE
                        ? VISIBLE
                        : GONE);
        showDirection();
    }

    private void refreshSelectedEntity(LengthUnit unit) {
        final Context context = getContext();
        final boolean selected = NativeViewport.sketchSelectedEntity(nativeEntity);
        rectangleRow.setVisibility(GONE);
        circleRow.setVisibility(GONE);
        lineRow.setVisibility(GONE);
        deleteEntity.setVisibility(selected ? VISIBLE : GONE);
        applyEntity.setEnabled(false);
        if (!selected) {
            entitySummary.setText(context.getString(R.string.sketch_no_selection));
            return;
        }
        final int kind = (int) nativeEntity[NativeViewport.SKETCH_ENTITY_KIND];
        final int v = NativeViewport.SKETCH_ENTITY_VALUES;
        switch (kind) {
            case NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE:
                entitySummary.setText(context.getString(R.string.tool_rectangle));
                rectangleRow.setVisibility(VISIBLE);
                rectangleFields[0].setText(unit.format(nativeEntity[v + 2]));
                rectangleFields[1].setText(unit.format(nativeEntity[v + 3]));
                applyEntity.setEnabled(true);
                break;
            case NativeViewport.SKETCH_ENTITY_KIND_CIRCLE:
                entitySummary.setText(context.getString(R.string.tool_circle));
                circleRow.setVisibility(VISIBLE);
                circleFields[0].setText(unit.format(nativeEntity[v + 2]));
                applyEntity.setEnabled(true);
                break;
            case NativeViewport.SKETCH_ENTITY_KIND_LINE:
                entitySummary.setText(context.getString(R.string.tool_line));
                lineRow.setVisibility(VISIBLE);
                for (int i = 0; i < 4; i++) {
                    lineFields[i].setText(unit.format(nativeEntity[v + i]));
                }
                applyEntity.setEnabled(true);
                break;
            default:
                entitySummary.setText(context.getString(R.string.sketch_polyline_summary,
                        (int) nativeEntity[v],
                        context.getString(nativeEntity[v + 1] != 0.0
                                ? R.string.sketch_polyline_closed
                                : R.string.sketch_polyline_open)));
                break;
        }
    }

    /**
     * The REGIONS the sketch offers (`CAD-VERTICAL-SLICE-R1`): an outer loop
     * minus the loops cleanly inside it, so a rectangle around a circle lists
     * the rectangle-with-a-hole and the disk as two rows. A row is ACTIVE when
     * native says its region is part of the extrusion — every selected region,
     * not one "chosen" id — and tapping a row makes it the one region, which is
     * what a list of choices means. Adding a second region to the extrusion is
     * a tap on it in the viewport, where the regions are seen.
     */
    private void refreshProfiles() {
        final Context context = getContext();
        profileChooser.removeAllViews();
        final int count = NativeViewport.sketchProfiles(null);
        final long[] anchors = new long[Math.max(count, 1)];
        final int written = NativeViewport.sketchProfiles(anchors);
        final long chosen = (long) nativeSketch[NativeViewport.SKETCH_CHOSEN_PROFILE];
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        for (int i = 0; i < written; i++) {
            final long anchor = anchors[i];
            String label = context.getString(R.string.tool_polyline);
            boolean selected = anchor == chosen;
            if (NativeViewport.sketchProfileInfo(anchor, profileInfo)) {
                selected = profileInfo[NativeViewport.SKETCH_REGION_SELECTED] != 0.0;
                final String area = LengthUnit.formatArea(
                        host.uiState().displayUnit(), profileInfo[2]);
                switch ((int) profileInfo[0]) {
                    case NativeViewport.SKETCH_PROFILE_KIND_RECTANGLE:
                        label = context.getString(R.string.sketch_profile_rectangle, area);
                        break;
                    case NativeViewport.SKETCH_PROFILE_KIND_CIRCLE:
                        label = context.getString(R.string.sketch_profile_circle, area);
                        break;
                    default:
                        label = context.getString(R.string.sketch_profile_polygon,
                                (int) profileInfo[1], area);
                        break;
                }
                final int holes = (int) profileInfo[NativeViewport.SKETCH_REGION_HOLES];
                if (holes == 1) {
                    label = context.getString(R.string.sketch_region_one_hole, label);
                } else if (holes > 1) {
                    label = context.getString(R.string.sketch_region_holes, label, holes);
                }
            }
            final TextView chip = EditorControlStyles.listRow(context,
                    R.id.sketch_profile_option, label);
            chip.setTag(Long.valueOf(anchor));
            chip.setContentDescription(context.getString(selected
                    ? R.string.sketch_region_selected_description
                    : R.string.sketch_region_description, label));
            EditorControlStyles.setListRowActive(chip, selected);
            chip.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    onProfileChosen(anchor);
                }
            });
            final LinearLayout.LayoutParams params = EditorControlStyles.rowParams(i == 0 ? 0 : gap);
            params.width = LayoutParams.MATCH_PARENT;
            profileChooser.addView(chip, params);
        }
    }

    /** Shows the operations native offers and marks the one it holds. */
    private void refreshOperations() {
        final int available = (int) nativeExtrude[NativeViewport.CAD_EXTRUDE_OPERATIONS_AVAILABLE];
        final int operation = (int) nativeExtrude[NativeViewport.CAD_EXTRUDE_OPERATION];
        operationNewBody.setVisibility(
                (available & NativeViewport.OPERATION_BIT_NEW_BODY) != 0 ? VISIBLE : GONE);
        operationAdd.setVisibility(
                (available & NativeViewport.OPERATION_BIT_ADD) != 0 ? VISIBLE : GONE);
        operationCut.setVisibility(
                (available & NativeViewport.OPERATION_BIT_CUT) != 0 ? VISIBLE : GONE);
        // A row with one choice is not a choice; the canvas badge still says
        // what the extrusion does.
        final int offered = Integer.bitCount(available & 7);
        operationRow.setVisibility(offered > 1 ? VISIBLE : GONE);
        EditorControlStyles.setChipActive(operationNewBody,
                operation == NativeViewport.OPERATION_NEW_BODY);
        EditorControlStyles.setChipActive(operationAdd, operation == NativeViewport.OPERATION_ADD);
        EditorControlStyles.setChipActive(operationCut, operation == NativeViewport.OPERATION_CUT);
    }

    /** Shows the direction the SESSION holds. The panel remembers none. */
    private void showDirection() {
        final int direction = currentDirection();
        EditorControlStyles.setChipActive(directionAlong,
                direction == NativeViewport.EXTRUDE_ALONG_NORMAL);
        EditorControlStyles.setChipActive(directionAgainst,
                direction == NativeViewport.EXTRUDE_AGAINST_NORMAL);
    }

    /** The direction native currently holds, read on every use rather than kept. */
    private int currentDirection() {
        return (int) nativeSketch[NativeViewport.SKETCH_EXTRUDE_DIRECTION];
    }

    // -----------------------------------------------------------------------
    // Acts
    // -----------------------------------------------------------------------

    private void onProfileChosen(long anchor) {
        final int status = NativeViewport.sketchSelectProfile(anchor);
        if (status != NativeViewport.CAD_OK) {
            host.showStatus(CadStatusMessages.describe(getContext(), status), R.attr.fsTextError);
            return;
        }
        refreshFromNative();
    }

    private void onDirectionChosen(int direction) {
        // Submitted at once, so the extrude preview and the canvas arrow turn
        // with the chip rather than only when Extrude is pressed. The chosen
        // value goes STRAIGHT to native and the chips are then redrawn from what
        // native accepted, so a refused direction leaves the panel showing the
        // truth rather than the tap.
        submitDepth(direction);
        refreshFromNative();
    }

    /** Submits the depth field with the session's own direction. */
    private boolean submitDepth() {
        return submitDepth(currentDirection());
    }

    /** Submits the depth field and one direction. False on a refusal. */
    private boolean submitDepth(int direction) {
        final BigDecimal depth = readField(depthField);
        if (depth == null) {
            return false;
        }
        final int status = NativeViewport.sketchSetExtrude(
                host.uiState().displayUnit().toMeters(depth).doubleValue(), direction);
        if (status != NativeViewport.CAD_OK) {
            host.showStatus(CadStatusMessages.describe(getContext(), status), R.attr.fsTextError);
            depthField.focusForCorrection();
            return false;
        }
        return true;
    }

    private void onApplyEntity() {
        final Context context = getContext();
        if (!NativeViewport.sketchSelectedEntity(nativeEntity)) {
            return;
        }
        final long id = (long) nativeEntity[NativeViewport.SKETCH_ENTITY_ID];
        final LengthUnit unit = host.uiState().displayUnit();
        int status;
        switch ((int) nativeEntity[NativeViewport.SKETCH_ENTITY_KIND]) {
            case NativeViewport.SKETCH_ENTITY_KIND_RECTANGLE: {
                final BigDecimal width = readField(rectangleFields[0]);
                if (width == null) return;
                final BigDecimal height = readField(rectangleFields[1]);
                if (height == null) return;
                status = NativeViewport.sketchApplyRectangle(id,
                        unit.toMeters(width).doubleValue(), unit.toMeters(height).doubleValue());
                break;
            }
            case NativeViewport.SKETCH_ENTITY_KIND_CIRCLE: {
                final BigDecimal radius = readField(circleFields[0]);
                if (radius == null) return;
                status = NativeViewport.sketchApplyCircle(id, unit.toMeters(radius).doubleValue());
                break;
            }
            case NativeViewport.SKETCH_ENTITY_KIND_LINE: {
                final double[] values = new double[4];
                for (int i = 0; i < 4; i++) {
                    final BigDecimal value = readField(lineFields[i]);
                    if (value == null) return;
                    values[i] = unit.toMeters(value).doubleValue();
                }
                status = NativeViewport.sketchApplyLine(id, values[0], values[1], values[2],
                        values[3]);
                break;
            }
            default:
                return;
        }
        if (status != NativeViewport.CAD_OK) {
            host.showStatus(CadStatusMessages.describe(context, status), R.attr.fsTextError);
            return;
        }
        refreshFromNative();
        host.showStatus(context.getString(R.string.status_sketch_entity_applied),
                R.attr.fsTextSuccess);
        host.finishEditing();
    }

    private void onDeleteEntity() {
        final int status = NativeViewport.sketchDeleteSelected();
        if (status != NativeViewport.CAD_OK) {
            host.showStatus(CadStatusMessages.describe(getContext(), status), R.attr.fsTextError);
            return;
        }
        refreshFromNative();
        host.showStatus(getContext().getString(R.string.status_sketch_entity_deleted),
                R.attr.fsTextSecondary);
    }

    /** Parses one field, reporting and focusing it on failure. Signed values
     *  are allowed: a coordinate may be negative; a size is refused below JNI. */
    private BigDecimal readField(NumericPropertyRow row) {
        final String raw = row.text();
        try {
            return LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                    ? getContext().getString(R.string.field_empty, row.label())
                    : getContext().getString(R.string.field_not_a_number, row.label(),
                            raw.trim()), R.attr.fsTextError);
            row.focusForCorrection();
            return null;
        }
    }

    boolean convertDisplayUnit(LengthUnit from, LengthUnit to) {
        boolean all = true;
        for (NumericPropertyRow field : rectangleFields) all &= field.convertUnit(from, to);
        for (NumericPropertyRow field : circleFields) all &= field.convertUnit(from, to);
        for (NumericPropertyRow field : lineFields) all &= field.convertUnit(from, to);
        all &= depthField.convertUnit(from, to);
        unitChips.showSelected(to);
        return all;
    }

    void clearEditFocus() {
        for (NumericPropertyRow field : rectangleFields) field.clearEditFocus();
        for (NumericPropertyRow field : circleFields) field.clearEditFocus();
        for (NumericPropertyRow field : lineFields) field.clearEditFocus();
        depthField.clearEditFocus();
    }
}
