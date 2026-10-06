package com.forgeshape.app;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.math.BigDecimal;

/**
 * The Surface context surface ({@code MODELING-FOUNDATIONS-R1} C): the body
 * of the precision surface over a Surface body, and over a sketch drawn for
 * one.
 *
 * <p><b>Over a Surface sketch</b> it is what Finish means: Patch, Extrude,
 * Revolve, Loft, Trim or Keep as Section, each drawn only while its native
 * candidate — the body's whole feature chain regenerated with this sketch
 * added — would be committed. The values they carry (a distance, an angle,
 * which side of a trim stays) are typed here.
 *
 * <p><b>Over a Surface body</b> it starts the next sketch (at a typed offset
 * along XY's normal, which is how a Loft's second section is placed), and it
 * offers Stitch and Thicken where their candidates succeed. A History row opens
 * a STAGED value edit here: the typed value is evaluated with the whole chain
 * on every keystroke, a failure downstream names its row, Apply is drawn only
 * when the staged chain regenerates, Fix keeps the edit open and Cancel drops
 * it. Nothing is written until Apply, and Apply is one Undo.
 *
 * <p>It holds no feature and no candidate. The only state it keeps is which
 * row a staged edit is about and the text being typed, both presentation.
 */
final class SurfaceEditorView extends LinearLayout {

    /** What this surface asks of the workspace beyond a value edit. */
    interface Host extends InspectorHost {
        /** Opens a sketch for the active Surface body at {@code offsetMeters}. */
        void onSurfaceSketchRequested(double offsetMeters);

        /**
         * A Finish landed: the sketch is over and the body exists;
         * {@code firstProject} when that Finish created the project itself.
         */
        void onSurfaceSketchCommitted(boolean firstProject);
    }

    private final Host host;
    private final double[] sketchState = new double[NativeViewport.SURFACE_SKETCH_STATE_SIZE];
    private final double[] bodyState = new double[NativeViewport.SURFACE_STATE_SIZE];
    private final double[] timelineHeader = new double[NativeViewport.TIMELINE_HEADER_SIZE];
    private final double[] timelineRows =
            new double[NativeViewport.TIMELINE_ROW_SIZE * NativeViewport.TIMELINE_MAX_ROWS * 2];
    private final long[] liveFeatures = new long[NativeViewport.SURFACE_MAX_FEATURES];

    private final TextView summary;

    private final LinearLayout finishGroup;
    private final NumericPropertyRow distanceRow;
    private final NumericPropertyRow angleRow;
    private final TextView keepInsideChip;
    private final TextView[] createButtons = new TextView[NativeViewport.SURFACE_CREATE_KIND_COUNT + 1];
    private final TextView blocked;
    private boolean keepInside;

    private final LinearLayout bodyGroup;
    private final NumericPropertyRow offsetRow;
    private final TextView stitchButton;
    private final NumericPropertyRow thicknessRow;
    private final LinearLayout thickenList;

    private final LinearLayout editGroup;
    private final TextView editTitle;
    private final NumericPropertyRow valueRow;
    private final TextView verdict;
    private final LinearLayout failureRow;
    private final TextView applyButton;
    private final TextView fixButton;
    private FeatureHistoryPresentation.Row editing;
    private boolean refreshing;

    SurfaceEditorView(Context context, Host host) {
        super(context);
        this.host = host;
        setId(R.id.surface_editor);
        setOrientation(VERTICAL);
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        final int smallGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);

        summary = EditorControlStyles.captionText(context, R.id.surface_summary, "");
        addView(summary, EditorControlStyles.rowParams(0));

        // --- Finish: what the open sketch becomes -------------------------
        finishGroup = column(context);
        distanceRow = new NumericPropertyRow(context, R.id.field_surface_distance,
                context.getString(R.string.surface_distance), true);
        distanceRow.setText("0.5");
        finishGroup.addView(distanceRow, EditorControlStyles.rowParams(0));
        angleRow = new NumericPropertyRow(context, R.id.field_surface_angle,
                context.getString(R.string.surface_angle), true);
        angleRow.setText("360");
        finishGroup.addView(angleRow, EditorControlStyles.rowParams(smallGap));
        keepInsideChip = EditorControlStyles.chip(context, R.id.surface_trim_keep_inside,
                context.getString(R.string.surface_trim_keep_inside));
        keepInsideChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                keepInside = !keepInside;
                refreshFromNative();
            }
        });
        finishGroup.addView(keepInsideChip, EditorControlStyles.rowParams(smallGap));
        for (final int kind : SurfacePresentation.CREATE_ORDER) {
            final TextView button = EditorControlStyles.actionChip(context, SurfacePresentation.createId(kind),
                    context.getString(SurfacePresentation.createLabel(kind)));
            button.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    commitSketch(kind);
                }
            });
            createButtons[kind] = button;
            finishGroup.addView(button, EditorControlStyles.rowParams(smallGap));
        }
        blocked = EditorControlStyles.captionText(context, R.id.surface_create_blocked, "");
        finishGroup.addView(blocked, EditorControlStyles.rowParams(smallGap));
        addView(finishGroup, EditorControlStyles.rowParams(gap));

        // --- The body: the next sketch, Stitch, Thicken, a staged edit ----
        bodyGroup = column(context);
        bodyGroup.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.surface_section_create)), EditorControlStyles.rowParams(0));
        offsetRow = new NumericPropertyRow(context, R.id.field_surface_offset,
                context.getString(R.string.surface_offset), true);
        offsetRow.setText("0");
        bodyGroup.addView(offsetRow, EditorControlStyles.rowParams(smallGap));
        final TextView newSketch = EditorControlStyles.actionChip(context, R.id.surface_new_sketch,
                context.getString(R.string.surface_new_sketch));
        newSketch.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final BigDecimal offset = readLength(offsetRow);
                if (offset != null) {
                    SurfaceEditorView.this.host.onSurfaceSketchRequested(offset.doubleValue());
                }
            }
        });
        bodyGroup.addView(newSketch, EditorControlStyles.rowParams(smallGap));

        bodyGroup.addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.surface_section_modify)), EditorControlStyles.rowParams(gap));
        stitchButton = EditorControlStyles.actionChip(context, R.id.surface_stitch,
                context.getString(R.string.surface_stitch));
        stitchButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                report(NativeViewport.surfaceStitch(NativeViewport.sceneActiveBodyId(), true),
                        R.string.status_surface_stitched);
            }
        });
        bodyGroup.addView(stitchButton, EditorControlStyles.rowParams(smallGap));
        thicknessRow = new NumericPropertyRow(context, R.id.field_surface_thickness,
                context.getString(R.string.surface_thickness), true);
        thicknessRow.setText("0.1");
        bodyGroup.addView(thicknessRow, EditorControlStyles.rowParams(smallGap));
        thickenList = column(context);
        bodyGroup.addView(thickenList, EditorControlStyles.rowParams(smallGap));

        editGroup = column(context);
        editTitle = EditorControlStyles.sectionLabel(context, "");
        editTitle.setId(R.id.surface_edit_title);
        editGroup.addView(editTitle, EditorControlStyles.rowParams(0));
        valueRow = new NumericPropertyRow(context, R.id.field_surface_edit_value,
                context.getString(R.string.surface_value), true);
        editGroup.addView(valueRow, EditorControlStyles.rowParams(smallGap));
        verdict = EditorControlStyles.captionText(context, R.id.surface_edit_verdict, "");
        editGroup.addView(verdict, EditorControlStyles.rowParams(smallGap));
        failureRow = column(context);
        editGroup.addView(failureRow, EditorControlStyles.rowParams(smallGap));
        final LinearLayout editActions = new LinearLayout(context);
        editActions.setOrientation(HORIZONTAL);
        applyButton = EditorControlStyles.actionChip(context, R.id.surface_edit_apply,
                context.getString(R.string.apply));
        applyButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                applyEdit();
            }
        });
        fixButton = EditorControlStyles.secondaryActionChip(context, R.id.surface_edit_fix,
                context.getString(R.string.regeneration_issue_fix));
        fixButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                valueRow.focusForCorrection();
            }
        });
        final TextView cancel = EditorControlStyles.secondaryActionChip(context, R.id.surface_edit_cancel,
                context.getString(R.string.cancel));
        cancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                endEdit();
            }
        });
        editActions.addView(applyButton, EditorControlStyles.evenShare(0));
        editActions.addView(fixButton, EditorControlStyles.evenShare(smallGap));
        editActions.addView(cancel, EditorControlStyles.evenShare(smallGap));
        editGroup.addView(editActions, EditorControlStyles.rowParams(smallGap));
        bodyGroup.addView(editGroup, EditorControlStyles.rowParams(gap));
        addView(bodyGroup, EditorControlStyles.rowParams(gap));

        final TextWatcher restage = new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                if (!refreshing) {
                    refreshFromNative();
                }
            }
        };
        valueRow.field().addTextChangedListener(restage);
        distanceRow.field().addTextChangedListener(restage);
        angleRow.field().addTextChangedListener(restage);
        thicknessRow.field().addTextChangedListener(restage);
    }

    private static LinearLayout column(Context context) {
        final LinearLayout column = new LinearLayout(context);
        column.setOrientation(VERTICAL);
        return column;
    }

    /** Opens a staged value edit of a History row (a feature's value or a sketch's offset). */
    void beginEdit(FeatureHistoryPresentation.Row row) {
        editing = row;
        final double value = NativeViewport.surfaceValue(NativeViewport.sceneActiveBodyId(),
                SurfacePresentation.valueTarget(row), row.id);
        refreshing = true;
        valueRow.setText(Double.isNaN(value) ? "" : formatValue(row, value));
        refreshing = false;
        refreshFromNative();
    }

    /** Whether a staged edit is open, for the workspace and for verification. */
    boolean editOpen() {
        return editing != null;
    }

    void endEdit() {
        editing = null;
        refreshFromNative();
    }

    /** Re-reads native state and draws only what can succeed. */
    void refreshFromNative() {
        refreshing = true;
        try {
            final Context context = getContext();
            final Double distance = quietLength(distanceRow);
            final Double angle = quietNumber(angleRow);
            NativeViewport.surfaceSketchState(distance == null ? Double.NaN : distance,
                    angle == null ? Double.NaN : angle, keepInside, sketchState);
            final boolean sketchOpen = SurfacePresentation.sketchOpen(sketchState);
            finishGroup.setVisibility(sketchOpen ? VISIBLE : GONE);
            bodyGroup.setVisibility(sketchOpen ? GONE : VISIBLE);
            if (sketchOpen) {
                refreshFinish(context);
            } else {
                refreshBody(context);
            }
        } finally {
            refreshing = false;
        }
    }

    private void refreshFinish(Context context) {
        summary.setText(context.getString(R.string.surface_finish_summary));
        for (int kind : SurfacePresentation.CREATE_ORDER) {
            createButtons[kind].setVisibility(SurfacePresentation.createShown(sketchState, kind) ? VISIBLE : GONE);
        }
        EditorControlStyles.setChipActive(keepInsideChip, keepInside);
        keepInsideChip.setVisibility(sketchState[NativeViewport.SURFACE_SKETCH_BODY] != 0.0 ? VISIBLE : GONE);
        if (SurfacePresentation.anyCreateShown(sketchState)) {
            blocked.setVisibility(GONE);
        } else {
            blocked.setVisibility(VISIBLE);
            blocked.setText(refusal(context, SurfacePresentation.blockingVerdict(sketchState)));
        }
    }

    private void refreshBody(Context context) {
        final long body = NativeViewport.sceneActiveBodyId();
        if (!NativeViewport.surfaceState(body, bodyState)) {
            summary.setText("");
            return;
        }
        summary.setText(context.getString(R.string.surface_body_summary,
                (int) bodyState[NativeViewport.SURFACE_STATE_FEATURES],
                (int) bodyState[NativeViewport.SURFACE_STATE_PATCHES],
                (int) bodyState[NativeViewport.SURFACE_STATE_OPEN_EDGES]));
        stitchButton.setVisibility(SurfacePresentation.stitchShown(NativeViewport.surfaceStitch(body, false))
                ? VISIBLE : GONE);

        // Thicken: one button per live feature whose candidate succeeds, named
        // as its History row is.
        thickenList.removeAllViews();
        final Double thickness = quietLength(thicknessRow);
        final int live = NativeViewport.surfaceLiveFeatures(body, liveFeatures);
        final int count = NativeViewport.surfaceTimeline(body, -1, 0L, 0.0, timelineHeader, timelineRows);
        final FeatureHistoryPresentation.Model model = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_SURFACE, timelineHeader, timelineRows, count);
        for (int i = 0; thickness != null && i < Math.min(live, liveFeatures.length); i++) {
            final long featureId = liveFeatures[i];
            if (!SurfacePresentation.thickenShown(
                    NativeViewport.surfaceThicken(body, featureId, thickness, false))) {
                continue;
            }
            final FeatureHistoryPresentation.Row row = featureRow(model, featureId);
            final TextView button = EditorControlStyles.actionChip(context, R.id.surface_thicken,
                    context.getString(R.string.surface_thicken_feature,
                            row == null ? Long.toString(featureId) : FeatureHistoryText.name(context, row)));
            button.setTag(featureId);
            button.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    final Double t = quietLength(thicknessRow);
                    if (t != null) {
                        report(NativeViewport.surfaceThicken(NativeViewport.sceneActiveBodyId(), featureId, t, true),
                                R.string.status_surface_thickened);
                    }
                }
            });
            thickenList.addView(button, EditorControlStyles.rowParams(thickenList.getChildCount() == 0 ? 0
                    : EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
        }
        refreshEdit(context, body);
    }

    private void refreshEdit(Context context, long body) {
        if (editing == null) {
            editGroup.setVisibility(GONE);
            return;
        }
        editGroup.setVisibility(VISIBLE);
        editTitle.setText(context.getString(R.string.surface_edit_title, FeatureHistoryText.name(context, editing)));
        final int target = SurfacePresentation.valueTarget(editing);
        final double committed = NativeViewport.surfaceValue(body, target, editing.id);
        final Double typed = typedValue(editing);
        failureRow.removeAllViews();
        if (typed == null || Double.isNaN(committed)) {
            verdict.setText(context.getString(R.string.surface_edit_type_value));
            applyButton.setVisibility(GONE);
            fixButton.setVisibility(GONE);
            return;
        }
        final int count = NativeViewport.surfaceTimeline(body, target, editing.id, typed, timelineHeader,
                timelineRows);
        final FeatureHistoryPresentation.Model staged = FeatureHistoryPresentation.fromNative(
                FeatureHistoryPresentation.DOMAIN_SURFACE, timelineHeader, timelineRows, count);
        final FeatureHistoryPresentation.Row failure = SurfacePresentation.stagedFailure(staged);
        if (failure != null) {
            verdict.setText(context.getString(R.string.surface_edit_fails, FeatureHistoryText.name(context, failure)));
            verdict.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
            failureRow.addView(FeatureHistoryView.buildRow(context, host.uiState().displayUnit(), failure, null),
                    EditorControlStyles.rowParams(0));
        } else if (staged.status != NativeViewport.SURFACE_OK) {
            verdict.setText(refusal(context, staged.status));
            verdict.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
        } else {
            verdict.setText(context.getString(R.string.surface_edit_regenerates));
            verdict.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextSecondary));
        }
        applyButton.setVisibility(SurfacePresentation.applyShown(staged, committed, typed) ? VISIBLE : GONE);
        fixButton.setVisibility(staged.status != NativeViewport.SURFACE_OK ? VISIBLE : GONE);
    }

    private static FeatureHistoryPresentation.Row featureRow(FeatureHistoryPresentation.Model model, long id) {
        for (FeatureHistoryPresentation.Row row : model.rows) {
            if (!row.isSketch() && row.id == id) {
                return row;
            }
        }
        return null;
    }

    private void commitSketch(int kind) {
        final int valueKind = SurfacePresentation.createValue(kind);
        double value = 0.0;
        if (valueKind == SurfacePresentation.VALUE_LENGTH) {
            final BigDecimal meters = readLength(distanceRow);
            if (meters == null) return;
            value = meters.doubleValue();
        } else if (valueKind == SurfacePresentation.VALUE_ANGLE) {
            final BigDecimal degrees = readNumber(angleRow);
            if (degrees == null) return;
            value = degrees.doubleValue();
        }
        final boolean firstProject = !NativeViewport.projectOpen();
        final int code = NativeViewport.surfaceCommitSketch(kind, value, keepInside);
        if (code == NativeViewport.SURFACE_OK) {
            keepInside = false;
            host.onSurfaceSketchCommitted(firstProject);
            host.showStatus(getContext().getString(R.string.status_surface_created,
                    getContext().getString(SurfacePresentation.createLabel(kind))), R.attr.fsTextSuccess);
        } else {
            host.showStatus(refusal(getContext(), code), R.attr.fsTextError);
        }
        refreshFromNative();
    }

    private void applyEdit() {
        if (editing == null) return;
        final Double typed = typedValue(editing);
        if (typed == null) return;
        final int code = NativeViewport.surfaceApplyValue(NativeViewport.sceneActiveBodyId(),
                SurfacePresentation.valueTarget(editing), editing.id, typed);
        if (code == NativeViewport.SURFACE_OK) {
            final String name = FeatureHistoryText.name(getContext(), editing);
            editing = null;
            host.onNativeStateChanged();
            host.finishEditing();
            host.showStatus(getContext().getString(R.string.status_surface_edited, name), R.attr.fsTextSuccess);
        } else {
            host.showStatus(refusal(getContext(), code), R.attr.fsTextError);
        }
        refreshFromNative();
    }

    private void report(int code, int successMessage) {
        if (code == NativeViewport.SURFACE_OK) {
            host.onNativeStateChanged();
            host.finishEditing();
            host.showStatus(getContext().getString(successMessage), R.attr.fsTextSuccess);
        } else {
            host.showStatus(refusal(getContext(), code), R.attr.fsTextError);
        }
        refreshFromNative();
    }

    static String refusal(Context context, int code) {
        final String token = NativeViewport.surfaceStatusToken(code);
        return context.getString(SurfacePresentation.refusalMessage(token), token);
    }

    private String formatValue(FeatureHistoryPresentation.Row row, double value) {
        if (SurfacePresentation.rowValue(row) == SurfacePresentation.VALUE_ANGLE) {
            return LengthUnit.present(BigDecimal.valueOf(value));
        }
        return host.uiState().displayUnit().format(value);
    }

    private Double typedValue(FeatureHistoryPresentation.Row row) {
        return SurfacePresentation.rowValue(row) == SurfacePresentation.VALUE_ANGLE
                ? quietNumber(valueRow) : quietLength(valueRow);
    }

    /** The field as metres, or null while it does not hold a number; never a status. */
    private Double quietLength(NumericPropertyRow row) {
        final Double display = quietNumber(row);
        return display == null ? null
                : host.uiState().displayUnit().toMeters(BigDecimal.valueOf(display)).doubleValue();
    }

    private static Double quietNumber(NumericPropertyRow row) {
        try {
            return LengthUnit.parse(row.text()).doubleValue();
        } catch (NumberFormatException notANumber) {
            return null;
        }
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
        if (fieldId == R.id.field_surface_distance) return distanceRow;
        if (fieldId == R.id.field_surface_angle) return angleRow;
        if (fieldId == R.id.field_surface_offset) return offsetRow;
        if (fieldId == R.id.field_surface_thickness) return thicknessRow;
        if (fieldId == R.id.field_surface_edit_value) return valueRow;
        return null;
    }
}
