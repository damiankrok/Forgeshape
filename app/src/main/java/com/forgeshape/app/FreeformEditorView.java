package com.forgeshape.app;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The Freeform cage context surface ({@code MODELING-FOUNDATIONS-R1} B): the
 * body of the precision surface while Shape is held over a Freeform body.
 *
 * <p>What a tap on the cage selects (Vertex, Edge, Face, and whether taps add
 * to the selection), what the cage gizmo does (Move, Rotate, Scale), and the
 * typed tools -- Push/Pull and Extrude by an exact distance, Insert Loop at an
 * exact ratio, Crease at an exact weight, Delete Face -- plus the body-wide
 * Symmetry planes and Subdivision level. Every tool is ONE native transaction
 * (one Undo); nothing here holds a cage value, and every control that cannot
 * succeed for the current selection is ABSENT ({@link FreeformPresentation}).
 *
 * <p>It is a context surface and not a bar: it lives in the on-demand precision
 * surface, opened from the toggle, and is absent otherwise.
 */
final class FreeformEditorView extends LinearLayout {

    private final InspectorHost host;
    private final double[] slots = new double[NativeViewport.FREEFORM_STATE_SIZE];

    private final TextView counts;
    private final TextView[] elementChips = new TextView[3];
    private final TextView multiChip;
    private final LinearLayout modeRow;
    private final TextView[] modeChips = new TextView[3];
    private final LinearLayout faceGroup;
    private final NumericPropertyRow distanceRow;
    private final TextView pushPull;
    private final TextView extrude;
    private final TextView deleteFaces;
    private final LinearLayout edgeGroup;
    private final LinearLayout loopRow;
    private final NumericPropertyRow ratioRow;
    private final TextView insertLoop;
    private final NumericPropertyRow creaseRow;
    private final TextView setCrease;
    private final TextView[] symmetryChips = new TextView[3];
    private final TextView[] levelChips = new TextView[NativeViewport.FREEFORM_MAX_LEVEL + 1];

    private static final int[] ELEMENT_IDS = {R.id.freeform_element_vertex,
            R.id.freeform_element_edge, R.id.freeform_element_face};
    private static final int[] ELEMENT_LABELS = {R.string.freeform_element_vertex,
            R.string.freeform_element_edge, R.string.freeform_element_face};
    private static final int[] MODE_IDS = {R.id.freeform_mode_move, R.id.freeform_mode_rotate,
            R.id.freeform_mode_scale};
    private static final int[] MODE_LABELS = {R.string.freeform_mode_move,
            R.string.freeform_mode_rotate, R.string.freeform_mode_scale};
    private static final int[] SYMMETRY_IDS = {R.id.freeform_symmetry_x,
            R.id.freeform_symmetry_y, R.id.freeform_symmetry_z};
    private static final int[] SYMMETRY_PLANES = {NativeViewport.FREEFORM_SYMMETRY_X,
            NativeViewport.FREEFORM_SYMMETRY_Y, NativeViewport.FREEFORM_SYMMETRY_Z};
    private static final int[] SYMMETRY_LABELS = {R.string.freeform_symmetry_x,
            R.string.freeform_symmetry_y, R.string.freeform_symmetry_z};
    private static final int[] LEVEL_IDS = {R.id.freeform_level_0, R.id.freeform_level_1,
            R.id.freeform_level_2, R.id.freeform_level_3, R.id.freeform_level_4};

    FreeformEditorView(Context context, InspectorHost host) {
        super(context);
        this.host = host;
        setId(R.id.freeform_editor);
        setOrientation(VERTICAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        final int smallGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        counts = EditorControlStyles.captionText(context, R.id.freeform_counts, "");
        addView(counts, EditorControlStyles.rowParams(0));

        // Select: what a tap on the cage picks.
        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.freeform_section_select)),
                EditorControlStyles.rowParams(gap));
        final LinearLayout elements = row(context);
        for (int i = 0; i < 3; i++) {
            final int element = i;
            elementChips[i] = EditorControlStyles.chip(context, ELEMENT_IDS[i], context.getString(ELEMENT_LABELS[i]));
            elementChips[i].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    report(NativeViewport.freeformSetElement(element), false);
                }
            });
            elements.addView(elementChips[i], EditorControlStyles.evenShare(i == 0 ? 0 : smallGap));
        }
        addView(elements, EditorControlStyles.rowParams(smallGap));
        multiChip = EditorControlStyles.chip(context, R.id.freeform_multi_select,
                context.getString(R.string.freeform_multi_select));
        multiChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                read();
                NativeViewport.freeformSetMultiSelect(!(slots[NativeViewport.FREEFORM_STATE_MULTI] != 0.0));
                refreshFromNative();
            }
        });
        addView(multiChip, EditorControlStyles.rowParams(smallGap));

        // The gizmo's meaning on the selection.
        modeRow = row(context);
        for (int i = 0; i < 3; i++) {
            final int mode = i;
            modeChips[i] = EditorControlStyles.chip(context, MODE_IDS[i], context.getString(MODE_LABELS[i]));
            modeChips[i].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    report(NativeViewport.freeformSetTransformMode(mode), false);
                }
            });
            modeRow.addView(modeChips[i], EditorControlStyles.evenShare(i == 0 ? 0 : smallGap));
        }
        addView(modeRow, EditorControlStyles.rowParams(gap));

        // Face tools: one exact distance, two acts.
        faceGroup = new LinearLayout(context);
        faceGroup.setOrientation(VERTICAL);
        distanceRow = new NumericPropertyRow(context, R.id.field_freeform_distance,
                context.getString(R.string.freeform_distance), true);
        distanceRow.setText("0.25");
        faceGroup.addView(distanceRow, EditorControlStyles.rowParams(0));
        final LinearLayout faceActions = row(context);
        pushPull = EditorControlStyles.actionChip(context, R.id.freeform_push_pull,
                context.getString(R.string.freeform_push_pull));
        pushPull.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final BigDecimal meters = readLength(distanceRow);
                if (meters != null) report(NativeViewport.freeformPushPull(meters.doubleValue()), true);
            }
        });
        extrude = EditorControlStyles.actionChip(context, R.id.freeform_extrude,
                context.getString(R.string.freeform_extrude));
        extrude.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final BigDecimal meters = readLength(distanceRow);
                if (meters != null) report(NativeViewport.freeformExtrude(meters.doubleValue()), true);
            }
        });
        faceActions.addView(pushPull, EditorControlStyles.evenShare(0));
        faceActions.addView(extrude, EditorControlStyles.evenShare(smallGap));
        faceGroup.addView(faceActions, EditorControlStyles.rowParams(smallGap));
        deleteFaces = EditorControlStyles.secondaryActionChip(context, R.id.freeform_delete_faces,
                context.getString(R.string.freeform_delete_faces));
        deleteFaces.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                report(NativeViewport.freeformDeleteFaces(), true);
            }
        });
        faceGroup.addView(deleteFaces, EditorControlStyles.rowParams(smallGap));
        addView(faceGroup, EditorControlStyles.rowParams(gap));

        // Edge tools: an exact ratio for the loop, an exact weight for the crease.
        edgeGroup = new LinearLayout(context);
        edgeGroup.setOrientation(VERTICAL);
        loopRow = new LinearLayout(context);
        loopRow.setOrientation(VERTICAL);
        ratioRow = new NumericPropertyRow(context, R.id.field_freeform_ratio,
                context.getString(R.string.freeform_ratio), true);
        ratioRow.setText("0.5");
        loopRow.addView(ratioRow, EditorControlStyles.rowParams(0));
        insertLoop = EditorControlStyles.actionChip(context, R.id.freeform_insert_loop,
                context.getString(R.string.freeform_insert_loop));
        insertLoop.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final BigDecimal ratio = readNumber(ratioRow);
                if (ratio != null) report(NativeViewport.freeformInsertLoop(ratio.doubleValue()), true);
            }
        });
        loopRow.addView(insertLoop, EditorControlStyles.rowParams(smallGap));
        edgeGroup.addView(loopRow, EditorControlStyles.rowParams(0));
        creaseRow = new NumericPropertyRow(context, R.id.field_freeform_crease,
                context.getString(R.string.freeform_crease_weight), true);
        creaseRow.setText("1");
        edgeGroup.addView(creaseRow, EditorControlStyles.rowParams(gap));
        setCrease = EditorControlStyles.actionChip(context, R.id.freeform_set_crease,
                context.getString(R.string.freeform_set_crease));
        setCrease.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final BigDecimal weight = readNumber(creaseRow);
                if (weight != null) report(NativeViewport.freeformSetCrease(weight.doubleValue()), true);
            }
        });
        edgeGroup.addView(setCrease, EditorControlStyles.rowParams(smallGap));
        addView(edgeGroup, EditorControlStyles.rowParams(gap));

        // Body-wide: the symmetry planes and the subdivision level.
        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.freeform_section_symmetry)),
                EditorControlStyles.rowParams(gap));
        final LinearLayout symmetryRow = row(context);
        for (int i = 0; i < 3; i++) {
            final int plane = SYMMETRY_PLANES[i];
            symmetryChips[i] = EditorControlStyles.chip(context, SYMMETRY_IDS[i],
                    context.getString(SYMMETRY_LABELS[i]));
            symmetryChips[i].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    read();
                    final FreeformPresentation.State state = new FreeformPresentation.State(slots);
                    report(NativeViewport.freeformSetSymmetry(
                            FreeformPresentation.toggledSymmetry(state, plane)), true);
                }
            });
            symmetryRow.addView(symmetryChips[i], EditorControlStyles.evenShare(i == 0 ? 0 : smallGap));
        }
        addView(symmetryRow, EditorControlStyles.rowParams(smallGap));
        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.freeform_section_level)),
                EditorControlStyles.rowParams(gap));
        final LinearLayout levelRow = row(context);
        for (int i = 0; i < levelChips.length; i++) {
            final int level = i;
            levelChips[i] = EditorControlStyles.chip(context, LEVEL_IDS[i], Integer.toString(i));
            levelChips[i].setContentDescription(context.getString(R.string.freeform_level_description, i));
            levelChips[i].setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    report(NativeViewport.freeformSetLevel(level), true);
                }
            });
            levelRow.addView(levelChips[i], EditorControlStyles.evenShare(i == 0 ? 0 : smallGap));
        }
        addView(levelRow, EditorControlStyles.rowParams(smallGap));
    }

    private static LinearLayout row(Context context) {
        final LinearLayout row = new LinearLayout(context);
        row.setOrientation(HORIZONTAL);
        return row;
    }

    private void read() {
        NativeViewport.freeformState(slots);
    }

    /** Re-reads the session and the cage, and draws only what can succeed. */
    void refreshFromNative() {
        read();
        final FreeformPresentation.State s = new FreeformPresentation.State(slots);
        final Context context = getContext();
        counts.setText(context.getString(R.string.freeform_counts, s.vertices, s.edges, s.faces));
        for (int i = 0; i < 3; i++) {
            EditorControlStyles.setChipActive(elementChips[i], s.element == i);
            EditorControlStyles.setChipActive(modeChips[i], s.transformMode == i);
            EditorControlStyles.setChipActive(symmetryChips[i],
                    FreeformPresentation.symmetryOn(s, SYMMETRY_PLANES[i]));
        }
        EditorControlStyles.setChipActive(multiChip, s.multiSelect);
        for (int i = 0; i < levelChips.length; i++) {
            EditorControlStyles.setChipActive(levelChips[i], s.level == i);
        }
        modeRow.setVisibility(FreeformPresentation.showTransformModes(s) ? VISIBLE : GONE);
        faceGroup.setVisibility(FreeformPresentation.showPushPull(s) ? VISIBLE : GONE);
        deleteFaces.setVisibility(FreeformPresentation.showDeleteFaces(s) ? VISIBLE : GONE);
        edgeGroup.setVisibility(FreeformPresentation.showCrease(s) ? VISIBLE : GONE);
        loopRow.setVisibility(FreeformPresentation.showInsertLoop(s) ? VISIBLE : GONE);
    }

    /** A status for a native answer; a change re-reads the whole workspace. */
    private void report(int code, boolean edit) {
        final Context context = getContext();
        if (code == NativeViewport.FREEFORM_OK) {
            if (edit) {
                host.onNativeStateChanged();
                host.finishEditing();
            }
        } else {
            host.showStatus(context.getString(FreeformPresentation.refusalMessage(code),
                    NativeViewport.freeformStatusToken(code)), R.attr.fsTextError);
        }
        refreshFromNative();
    }

    private BigDecimal readLength(NumericPropertyRow row) {
        final BigDecimal display = readNumber(row);
        return display == null ? null : host.uiState().displayUnit().toMeters(display);
    }

    private BigDecimal readNumber(NumericPropertyRow row) {
        final String raw = row.text();
        try {
            return LengthUnit.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                            ? getContext().getString(R.string.field_empty, row.label())
                            : getContext().getString(R.string.field_not_a_number, row.label(), raw.trim()),
                    R.attr.fsTextError);
            row.focusForCorrection();
            return null;
        }
    }

    /** The row that owns a field id, or null. For verification. */
    NumericPropertyRow rowFor(int fieldId) {
        if (fieldId == R.id.field_freeform_distance) return distanceRow;
        if (fieldId == R.id.field_freeform_ratio) return ratioRow;
        if (fieldId == R.id.field_freeform_crease) return creaseRow;
        return null;
    }
}
