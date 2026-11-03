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
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The sketch actions entry and its palette (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`).
 *
 * <p>One compact <b>Modify</b> control under the orientation navigator, in the
 * upper trailing corner, while a sketch is DRAWN. It grows the sketch actions
 * palette out of itself — Select multiple, Dimension, Make Construction / Make
 * Regular, Trim, Extend, Offset, Mirror, Delete and the Dimensions visibility —
 * and, while a modify mode is active, becomes that mode's compact capsule: its
 * name, what the mode needs (the dimension kinds, the exact offset, the mirror
 * Confirm) and the way out. The Tool Rail keeps its seven drawing tools; there
 * is no bottom toolbar and the viewport stays the workspace.
 *
 * <p><b>Holds no drafting state.</b> The selection, the mode, the dimension
 * target, the offset distance and the mirror axis are re-read from native on
 * every refresh ({@link NativeViewport#sketchDraftingState}). The only thing
 * kept here is whether the palette is open and the Driving/Reference choice a
 * new dimension will take — both presentation, both volatile.
 */
final class SketchModifyView extends LinearLayout {

    /** Told what was asked; the workspace owns what each act means. */
    interface OnModifyAction {
        void onSketchModifyAction(int action);

        void onSketchModifyModeEnd(boolean confirm);

        void onSketchDimensionKindChosen(int kind, int mode);

        void onSketchDimensionAngleRequested();

        void onSketchOffsetDistanceEntered(double meters);

        void onSketchDimensionVisibilityChosen(int visibility);

        void onSketchMultiSelectToggled(boolean on);
    }

    private static final int[] ACTION_IDS = {
            R.id.sketch_action_select_multiple, R.id.sketch_action_dimension,
            R.id.sketch_action_construction, R.id.sketch_action_trim, R.id.sketch_action_extend,
            R.id.sketch_action_offset, R.id.sketch_action_mirror, R.id.sketch_action_delete};
    private static final int[] ACTION_LABELS = {
            R.string.sketch_action_select_multiple, R.string.sketch_action_dimension,
            R.string.sketch_make_construction, R.string.sketch_action_trim,
            R.string.sketch_action_extend, R.string.sketch_action_offset,
            R.string.sketch_action_mirror, R.string.sketch_action_delete};
    private static final int[] KIND_IDS = {
            R.id.sketch_dimension_kind_length, R.id.sketch_dimension_kind_angle,
            R.id.sketch_dimension_kind_horizontal, R.id.sketch_dimension_kind_vertical,
            R.id.sketch_dimension_kind_width, R.id.sketch_dimension_kind_height,
            R.id.sketch_dimension_kind_radius, R.id.sketch_dimension_kind_diameter,
            R.id.sketch_dimension_kind_edge_length, R.id.sketch_dimension_kind_arc_radius,
            R.id.sketch_dimension_kind_sweep, R.id.sketch_dimension_kind_edge_angle};

    private final InspectorHost host;
    private final OnModifyAction actions;
    private final double[] sketch = new double[NativeViewport.SKETCH_STATE_SIZE];
    private final double[] draft = new double[NativeViewport.SKETCH_DRAFT_SIZE];
    private final int[] kinds = new int[NativeViewport.DIM_KIND_COUNT];

    private final TextView entry;
    private final LinearLayout palette;
    private final TextView[] actionChips = new TextView[SketchDraftingPresentation.ACTION_COUNT];
    /** The palette's action rows: two chips each, so the palette stays a corner control. */
    private final LinearLayout actionRows;
    /** Which actions the rows currently hold, so they are rebuilt only on a change. */
    private boolean[] laidOutActions = new boolean[0];
    private final TextView[] visibilityChips = new TextView[3];

    private final LinearLayout modeCapsule;
    private final TextView modeTitle;
    private final LinearLayout kindRow;
    private final TextView[] kindChips = new TextView[NativeViewport.DIM_KIND_COUNT];
    private final TextView drivingChip;
    private final LinearLayout offsetRow;
    private final EditText offsetField;
    private final TextView confirm;
    private final TextView done;

    private boolean paletteOpen;
    /** The mode a NEW dimension takes; volatile presentation, Driving first. */
    private boolean drivingChosen = true;
    private int shownMode = NativeViewport.MODIFY_NONE;

    SketchModifyView(Context context, InspectorHost host, OnModifyAction actions) {
        super(context);
        this.host = host;
        this.actions = actions;
        setId(R.id.sketch_modify);
        setOrientation(VERTICAL);
        setGravity(Gravity.END);
        setVisibility(GONE);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        entry = EditorControlStyles.chip(context, R.id.sketch_modify_toggle,
                context.getString(R.string.sketch_modify));
        entry.setContentDescription(context.getString(R.string.sketch_modify_description));
        entry.setOnClickListener(v -> {
            paletteOpen = !paletteOpen;
            refreshFromNative();
        });
        addView(entry, EditorControlStyles.rowParams(gap));

        palette = new LinearLayout(context);
        palette.setId(R.id.sketch_actions_palette);
        palette.setOrientation(VERTICAL);
        EditorControlStyles.applyContextSurface(palette);
        palette.setPadding(gap, gap, gap, gap);
        for (int a = 0; a < SketchDraftingPresentation.ACTION_COUNT; a++) {
            final int action = a;
            final TextView chip = EditorControlStyles.chip(context, ACTION_IDS[a],
                    context.getString(ACTION_LABELS[a]));
            chip.setOnClickListener(v -> {
                if (action == SketchDraftingPresentation.ACTION_SELECT_MULTIPLE) {
                    actions.onSketchMultiSelectToggled(
                            draft[NativeViewport.SKETCH_DRAFT_MULTI_SELECT] == 0.0);
                    return;
                }
                paletteOpen = false;
                actions.onSketchModifyAction(action);
            });
            actionChips[a] = chip;
        }
        actionRows = new LinearLayout(context);
        actionRows.setOrientation(VERTICAL);
        palette.addView(actionRows, EditorControlStyles.rowParams(0));
        final TextView visibilityLabel = EditorControlStyles.fieldLabel(context,
                context.getString(R.string.sketch_dimensions_visibility));
        palette.addView(visibilityLabel, EditorControlStyles.rowParams(gap));
        final LinearLayout visibilityRow = new LinearLayout(context);
        visibilityRow.setOrientation(HORIZONTAL);
        final int[] visibilityIds = {R.id.sketch_dimensions_selected, R.id.sketch_dimensions_all,
                R.id.sketch_dimensions_off};
        final int[] visibilityLabels = {R.string.sketch_dimensions_selected,
                R.string.sketch_dimensions_all, R.string.sketch_dimensions_off};
        for (int i = 0; i < 3; i++) {
            final int visibility = i;
            final TextView chip = EditorControlStyles.chip(context, visibilityIds[i],
                    context.getString(visibilityLabels[i]));
            chip.setOnClickListener(v -> actions.onSketchDimensionVisibilityChosen(visibility));
            visibilityChips[i] = chip;
            visibilityRow.addView(chip, EditorControlStyles.wrap(i == 0 ? 0 : gap));
        }
        palette.addView(visibilityRow, EditorControlStyles.rowParams(gap));
        palette.setVisibility(GONE);
        addView(palette, EditorControlStyles.rowParams(gap));

        modeCapsule = new LinearLayout(context);
        modeCapsule.setId(R.id.sketch_modify_mode);
        modeCapsule.setOrientation(VERTICAL);
        EditorControlStyles.applyContextSurface(modeCapsule);
        modeCapsule.setPadding(gap, gap, gap, gap);
        modeTitle = EditorControlStyles.titleText(context, R.id.sketch_modify_mode_title, "");
        modeCapsule.addView(modeTitle, EditorControlStyles.rowParams(0));

        kindRow = new LinearLayout(context);
        kindRow.setOrientation(VERTICAL);
        for (int k = 0; k < NativeViewport.DIM_KIND_COUNT; k++) {
            final int kind = k;
            final TextView chip = EditorControlStyles.chip(context, KIND_IDS[k],
                    context.getString(SketchDraftingPresentation.kindName(k)));
            chip.setOnClickListener(v -> {
                if (kind == NativeViewport.DIM_EDGE_ANGLE) {
                    actions.onSketchDimensionAngleRequested();
                    return;
                }
                final boolean driving = drivingChosen && SketchDraftingPresentation.drivingAllowed(kind);
                actions.onSketchDimensionKindChosen(kind,
                        driving ? NativeViewport.DIM_MODE_DRIVING : NativeViewport.DIM_MODE_REFERENCE);
            });
            kindChips[k] = chip;
            kindRow.addView(chip, EditorControlStyles.rowParams(gap));
        }
        drivingChip = EditorControlStyles.chip(context, R.id.sketch_dimension_driving,
                context.getString(R.string.sketch_dimension_driving));
        drivingChip.setOnClickListener(v -> {
            drivingChosen = !drivingChosen;
            refreshFromNative();
        });
        kindRow.addView(drivingChip, EditorControlStyles.rowParams(gap));
        modeCapsule.addView(kindRow, EditorControlStyles.rowParams(gap));

        offsetRow = new LinearLayout(context);
        offsetRow.setOrientation(HORIZONTAL);
        offsetRow.setGravity(Gravity.CENTER_VERTICAL);
        offsetField = new EditText(context);
        offsetField.setId(R.id.field_sketch_offset_distance);
        offsetField.setSingleLine(true);
        offsetField.setBackgroundResource(R.drawable.bg_field);
        offsetField.setInputType(InputType.TYPE_CLASS_TEXT);
        offsetField.setKeyListener(DigitsKeyListener.getInstance("0123456789.,-+*/()"));
        offsetField.setImeOptions(EditorInfo.IME_ACTION_DONE);
        offsetField.setTextSize(TypedValue.COMPLEX_UNIT_PX,
                EditorControlStyles.dimen(context, R.dimen.text_body));
        offsetField.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextPrimary));
        offsetField.setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.sketch_dimension_field));
        offsetField.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
        offsetField.setContentDescription(context.getString(R.string.sketch_offset_distance));
        offsetField.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE) {
                    submitOffset();
                    return true;
                }
                return false;
            }
        });
        offsetRow.addView(offsetField, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        modeCapsule.addView(offsetRow, EditorControlStyles.rowParams(gap));

        final LinearLayout buttons = new LinearLayout(context);
        buttons.setOrientation(HORIZONTAL);
        confirm = EditorControlStyles.primaryButton(context, R.id.sketch_modify_confirm,
                context.getString(R.string.sketch_modify_confirm));
        confirm.setOnClickListener(v -> {
            if (shownMode == NativeViewport.MODIFY_OFFSET && offsetField.hasFocus()) {
                // A typed value not yet submitted is the one meant.
                if (!submitOffset()) {
                    return;
                }
            }
            actions.onSketchModifyModeEnd(true);
        });
        buttons.addView(confirm, EditorControlStyles.wrap(0));
        done = EditorControlStyles.chip(context, R.id.sketch_modify_done,
                context.getString(R.string.sketch_modify_done));
        done.setOnClickListener(v -> actions.onSketchModifyModeEnd(false));
        buttons.addView(done, EditorControlStyles.wrap(gap));
        modeCapsule.addView(buttons, EditorControlStyles.rowParams(gap));
        modeCapsule.setVisibility(GONE);
        addView(modeCapsule, EditorControlStyles.rowParams(gap));
    }

    /** Whether the palette is open, for verification. */
    boolean paletteOpen() {
        return palette.getVisibility() == VISIBLE;
    }

    void closePalette() {
        paletteOpen = false;
        palette.setVisibility(GONE);
    }

    /**
     * Re-reads the session and shows the entry, the palette or the mode
     * capsule — or nothing, outside a drawn sketch.
     */
    void refreshFromNative() {
        NativeViewport.sketchState(sketch);
        final int state = (int) sketch[NativeViewport.SKETCH_STATE];
        if (!SketchDraftingPresentation.modifyEntryShown(state)) {
            paletteOpen = false;
            setVisibility(GONE);
            return;
        }
        NativeViewport.sketchDraftingState(draft);
        setVisibility(VISIBLE);
        final Context context = getContext();
        final int mode = (int) draft[NativeViewport.SKETCH_DRAFT_MODE];
        final boolean modeActive = mode != NativeViewport.MODIFY_NONE;
        if (mode != shownMode && mode == NativeViewport.MODIFY_OFFSET) {
            offsetField.setText(host.uiState().displayUnit().format(
                    draft[NativeViewport.SKETCH_DRAFT_OFFSET_DISTANCE]));
        }
        shownMode = mode;
        entry.setVisibility(modeActive ? GONE : VISIBLE);
        EditorControlStyles.setChipActive(entry, paletteOpen && !modeActive);
        palette.setVisibility(paletteOpen && !modeActive ? VISIBLE : GONE);
        modeCapsule.setVisibility(modeActive ? VISIBLE : GONE);

        final int selection = (int) draft[NativeViewport.SKETCH_DRAFT_SELECTION_COUNT];
        final int singleKind = (int) draft[NativeViewport.SKETCH_DRAFT_SINGLE_KIND];
        final boolean[] shown = SketchDraftingPresentation.paletteActions(state, selection, singleKind);
        layOutActions(shown);
        actionChips[SketchDraftingPresentation.ACTION_CONSTRUCTION].setText(context.getString(
                SketchDraftingPresentation.constructionLabel(
                        draft[NativeViewport.SKETCH_DRAFT_CONSTRUCTION_TARGET] != 0.0)));
        EditorControlStyles.setChipActive(actionChips[SketchDraftingPresentation.ACTION_SELECT_MULTIPLE],
                draft[NativeViewport.SKETCH_DRAFT_MULTI_SELECT] != 0.0);
        final int visibility = (int) draft[NativeViewport.SKETCH_DRAFT_DIM_VISIBILITY];
        for (int i = 0; i < 3; i++) {
            EditorControlStyles.setChipActive(visibilityChips[i], i == visibility);
        }

        if (!modeActive) {
            return;
        }
        modeTitle.setText(context.getString(modeTitle(mode)));
        kindRow.setVisibility(mode == NativeViewport.MODIFY_DIMENSION ? VISIBLE : GONE);
        offsetRow.setVisibility(mode == NativeViewport.MODIFY_OFFSET ? VISIBLE : GONE);
        boolean anyDriving = false;
        final int count = mode == NativeViewport.MODIFY_DIMENSION
                ? NativeViewport.sketchDimensionTargetKinds(kinds) : 0;
        for (int k = 0; k < NativeViewport.DIM_KIND_COUNT; k++) {
            boolean offered = false;
            for (int i = 0; i < count && i < kinds.length; i++) {
                offered = offered || kinds[i] == k;
            }
            kindChips[k].setVisibility(offered ? VISIBLE : GONE);
            anyDriving = anyDriving || (offered && SketchDraftingPresentation.drivingAllowed(k));
        }
        drivingChip.setVisibility(anyDriving ? VISIBLE : GONE);
        EditorControlStyles.setChipActive(drivingChip, drivingChosen);
        drivingChip.setText(context.getString(drivingChosen ? R.string.sketch_dimension_driving
                : R.string.sketch_dimension_reference));
        // Confirm exists only where a mode creates something, and only when it
        // would succeed: Offset with a valid preview, Mirror with an axis.
        final boolean confirmable = (mode == NativeViewport.MODIFY_OFFSET
                && (int) draft[NativeViewport.SKETCH_DRAFT_OFFSET_STATUS] == NativeViewport.CAD_OK)
                || (mode == NativeViewport.MODIFY_MIRROR
                && draft[NativeViewport.SKETCH_DRAFT_MIRROR_AXIS_SET] != 0.0);
        confirm.setVisibility(confirmable ? VISIBLE : GONE);
        done.setText(context.getString(mode == NativeViewport.MODIFY_OFFSET
                || mode == NativeViewport.MODIFY_MIRROR ? R.string.cancel : R.string.sketch_modify_done));
    }

    /**
     * Puts the shown actions into rows of two, in reading order. A control
     * that cannot succeed is not drawn, so a hidden action takes no cell.
     */
    private void layOutActions(boolean[] shown) {
        if (java.util.Arrays.equals(shown, laidOutActions)) {
            return;
        }
        laidOutActions = shown.clone();
        final int gap = EditorControlStyles.dimen(getContext(), R.dimen.row_gap_small);
        for (TextView chip : actionChips) {
            if (chip.getParent() instanceof ViewGroup) {
                ((ViewGroup) chip.getParent()).removeView(chip);
            }
        }
        actionRows.removeAllViews();
        LinearLayout row = null;
        int inRow = 0;
        for (int a = 0; a < SketchDraftingPresentation.ACTION_COUNT; a++) {
            if (!shown[a]) {
                continue;
            }
            if (row == null || inRow == 2) {
                row = new LinearLayout(getContext());
                row.setOrientation(HORIZONTAL);
                actionRows.addView(row, EditorControlStyles.rowParams(actionRows.getChildCount() == 0 ? 0 : gap));
                inRow = 0;
            }
            row.addView(actionChips[a], EditorControlStyles.evenShare(inRow == 0 ? 0 : gap));
            inRow++;
        }
    }

    private int modeTitle(int mode) {
        switch (mode) {
            case NativeViewport.MODIFY_DIMENSION:
                return draft[NativeViewport.SKETCH_DRAFT_DIM_AWAIT_SECOND] != 0.0
                        ? R.string.sketch_mode_dimension_angle : R.string.sketch_action_dimension;
            case NativeViewport.MODIFY_TRIM:
                return R.string.sketch_action_trim;
            case NativeViewport.MODIFY_EXTEND:
                return R.string.sketch_action_extend;
            case NativeViewport.MODIFY_OFFSET:
                return R.string.sketch_action_offset;
            case NativeViewport.MODIFY_MIRROR:
                return draft[NativeViewport.SKETCH_DRAFT_MIRROR_AXIS_SET] != 0.0
                        ? R.string.sketch_action_mirror : R.string.sketch_mode_mirror_axis;
            default:
                return R.string.sketch_modify;
        }
    }

    /** Parses the offset field (plain numbers or a safe expression) and submits METRES. */
    private boolean submitOffset() {
        final Context context = getContext();
        final String raw = offsetField.getText().toString();
        final BigDecimal typed;
        try {
            typed = ExactExpression.parse(raw);
        } catch (NumberFormatException notANumber) {
            host.showStatus(raw.trim().isEmpty()
                            ? context.getString(R.string.field_empty,
                                    context.getString(R.string.sketch_offset_distance))
                            : context.getString(R.string.field_not_a_number,
                                    context.getString(R.string.sketch_offset_distance), raw.trim()),
                    R.attr.fsTextError);
            return false;
        }
        actions.onSketchOffsetDistanceEntered(host.uiState().displayUnit().toMeters(typed).doubleValue());
        return true;
    }
}
