package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.util.List;

/**
 * The Parametric History surface ({@code MODELING-FOUNDATIONS-R1} A): how the
 * active body was built, one row per sketch and per feature in construction
 * order, and one tap to edit any of them.
 *
 * <p><b>A VIEW of a chain that already existed.</b> A CAD Body's (or a Surface
 * body's) feature chain is its truth below JNI; this surface draws a fresh read
 * of it and remembers nothing, so it can never describe a chain that has moved.
 * There is no reorder and no suppression: both would need durable semantics
 * this stage does not add.
 *
 * <p><b>Three segments over one CAD body</b> ({@code CAD-R2}). <i>Steps</i> is
 * the timeline as it always was — a tap edits the row. <i>Sketches</i> is the
 * Sketch Browser: every retained sketch with how it is used and where it
 * stands. <i>Features</i> lists the chain's features. In those two a tap
 * SELECTS a row (by durable id) and an inline action strip beneath the list
 * offers exactly the acts native accepts for it — Edit Sketch, Edit Feature,
 * Use for Add, Use for Cut, Delete Sketch — with the dependencies and the
 * reason any absent act is absent. One surface, never a second editor: every
 * act lands on the one existing native path. A Surface body keeps Steps alone.
 *
 * <p><b>A small floating surface, not a timeline bar.</b> It grows out of the
 * History control in the history capsule, shows five rows and scrolls beyond
 * them — the Sculpt History navigator's shape, because a phone has no room for
 * a permanent desktop strip. Every row and every act is a 48 dp target, and
 * the strip WRAPS rather than shrinks a target.
 *
 * <p><b>States are marked by SHAPE.</b> A filled dot is regenerated, a cross is
 * the first feature that cannot be rebuilt (and says why in words), a dotted
 * ring is a feature after it that was never evaluated, a hollow ring a sketch
 * nothing builds from, a pencil marks what a staged edit changes. Colour is the
 * second carrier, never the only one.
 *
 * <p>Owns no truth and decides nothing: the segment and the selected id are
 * presentation; a tap reports the row or the act; the workspace runs it.
 */
final class FeatureHistoryView extends AnchoredSurfaceView {

    /** Rows visible before the list scrolls; a viewport target, not a bound. */
    static final int VISIBLE_ROWS = 5;

    /** Told which row the user tapped; the caller owns what it means. */
    interface OnHistoryRowChosen {
        void onFeatureHistoryRowChosen(FeatureHistoryPresentation.Row row);
    }

    /** Told which act the user chose on which durable target ({@code CAD-R2}). */
    interface OnHistoryAct {
        void onHistoryAct(int act, long sketchId, long featureId);
    }

    private final OnHistoryRowChosen listener;
    private OnHistoryAct actListener;
    private final LinearLayout segments;
    private final TextView tabSteps;
    private final TextView tabSketches;
    private final TextView tabFeatures;
    private final BoundedScrollView scroller;
    private final LinearLayout rows;
    private final TextView detail;
    private final LinearLayout actions;
    private final TextView caption;
    private final int rowGap;

    private int segment = SketchBrowserPresentation.SEGMENT_STEPS;
    private long selectedSketch;
    private long selectedFeature;
    private boolean cadBody;
    private FeatureHistoryPresentation.Model lastModel = FeatureHistoryPresentation.Model.empty();
    private SketchBrowserPresentation.Inventory lastInventory =
            SketchBrowserPresentation.Inventory.empty();
    private LengthUnit lastUnit = LengthUnit.METERS;

    FeatureHistoryView(Context context, OnHistoryRowChosen listener) {
        super(context);
        this.listener = listener;
        setId(R.id.feature_history);
        setGrowsUpward(true);
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        rowGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        setPadding(pad, pad, pad, pad);

        addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.feature_history_title)), EditorControlStyles.rowParams(0));

        segments = new LinearLayout(context);
        segments.setId(R.id.feature_history_segments);
        segments.setOrientation(HORIZONTAL);
        tabSteps = segmentChip(context, R.id.feature_history_tab_steps,
                R.string.feature_history_tab_steps, R.string.feature_history_tab_steps_description,
                SketchBrowserPresentation.SEGMENT_STEPS);
        tabSketches = segmentChip(context, R.id.feature_history_tab_sketches,
                R.string.feature_history_tab_sketches,
                R.string.feature_history_tab_sketches_description,
                SketchBrowserPresentation.SEGMENT_SKETCHES);
        tabFeatures = segmentChip(context, R.id.feature_history_tab_features,
                R.string.feature_history_tab_features,
                R.string.feature_history_tab_features_description,
                SketchBrowserPresentation.SEGMENT_FEATURES);
        segments.addView(tabSteps, segmentParams(0));
        segments.addView(tabSketches, segmentParams(rowGap));
        segments.addView(tabFeatures, segmentParams(rowGap));
        segments.setVisibility(GONE);
        addView(segments, EditorControlStyles.rowParams(rowGap));

        // The selected row's acts and its dependencies stand ABOVE the list:
        // in a short window it is the scrolling list that gives up height,
        // never an act -- a squeezed act would fall under the 48 dp floor
        // (found on the landscape device run, D12).
        actions = new LinearLayout(context);
        actions.setId(R.id.feature_history_actions);
        actions.setOrientation(VERTICAL);
        actions.setVisibility(GONE);
        addView(actions, EditorControlStyles.rowParams(rowGap));

        detail = EditorControlStyles.captionText(context, R.id.feature_history_detail, "");
        detail.setVisibility(GONE);
        addView(detail, EditorControlStyles.rowParams(rowGap));

        scroller = new BoundedScrollView(context);
        scroller.setId(R.id.feature_history_list);
        scroller.setFillViewport(false);
        scroller.setVerticalScrollBarEnabled(true);
        addView(scroller, EditorControlStyles.rowParams(rowGap));

        rows = new LinearLayout(context);
        rows.setOrientation(VERTICAL);
        scroller.addView(rows, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        caption = EditorControlStyles.captionText(context, View.NO_ID,
                context.getString(R.string.feature_history_caption));
        addView(caption, EditorControlStyles.rowParams(rowGap));
    }

    void setActListener(OnHistoryAct actListener) {
        this.actListener = actListener;
    }

    private TextView segmentChip(Context context, int id, int label, int description,
                                 final int which) {
        final TextView chip = EditorControlStyles.chip(context, id, context.getString(label));
        chip.setContentDescription(context.getString(description));
        chip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                showSegment(which);
            }
        });
        return chip;
    }

    private static LinearLayout.LayoutParams segmentParams(int leftMargin) {
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        params.leftMargin = leftMargin;
        return params;
    }

    /** Back to Steps with nothing selected: what every fresh open shows. */
    void resetSegment() {
        segment = SketchBrowserPresentation.SEGMENT_STEPS;
        selectedSketch = 0L;
        selectedFeature = 0L;
    }

    /** Switches the visible segment and repaints from the last read. */
    void showSegment(int which) {
        segment = SketchBrowserPresentation.segmentFor(which, cadBody);
        render();
    }

    int segment() {
        return segment;
    }

    long selectedSketch() {
        return selectedSketch;
    }

    long selectedFeature() {
        return selectedFeature;
    }

    /** Repaints the whole list from one freshly read model. Rebuilt, never diffed. */
    void showModel(FeatureHistoryPresentation.Model model, LengthUnit unit) {
        showModel(model, SketchBrowserPresentation.Inventory.empty(), false, unit);
    }

    /**
     * Repaints from one timeline read and, for a CAD body, one inventory read.
     * A selected id the new read no longer carries is dropped, never moved to
     * a neighbour.
     */
    void showModel(FeatureHistoryPresentation.Model model,
                   SketchBrowserPresentation.Inventory inventory, boolean cad, LengthUnit unit) {
        lastModel = model;
        lastInventory = inventory;
        lastUnit = unit;
        cadBody = cad;
        segment = SketchBrowserPresentation.segmentFor(segment, cad);
        selectedSketch = SketchBrowserPresentation.reconcileSketch(inventory, selectedSketch);
        selectedFeature = SketchBrowserPresentation.reconcileFeature(model, selectedFeature);
        render();
    }

    private void render() {
        final Context context = getContext();
        segments.setVisibility(SketchBrowserPresentation.segmentsShown(cadBody) ? VISIBLE : GONE);
        EditorControlStyles.setChipActive(tabSteps,
                segment == SketchBrowserPresentation.SEGMENT_STEPS);
        EditorControlStyles.setChipActive(tabSketches,
                segment == SketchBrowserPresentation.SEGMENT_SKETCHES);
        EditorControlStyles.setChipActive(tabFeatures,
                segment == SketchBrowserPresentation.SEGMENT_FEATURES);
        rows.removeAllViews();
        actions.removeAllViews();
        detail.setVisibility(GONE);
        actions.setVisibility(GONE);
        int index = 0;
        int focus = -1;
        if (segment == SketchBrowserPresentation.SEGMENT_SKETCHES) {
            caption.setText(lastInventory.rows.isEmpty()
                    ? R.string.sketch_browser_empty : R.string.sketch_browser_caption);
            for (final SketchBrowserPresentation.SketchRow row : lastInventory.rows) {
                final boolean selected = row.id == selectedSketch;
                rows.addView(buildSketchRow(context, row, selected),
                        EditorControlStyles.rowParams(index == 0 ? 0 : rowGap));
                if (selected) {
                    focus = index;
                }
                index++;
            }
            final SketchBrowserPresentation.SketchRow chosen = lastInventory.find(selectedSketch);
            if (chosen != null) {
                showDetail(SketchBrowserText.detail(context, chosen, lastInventory, lastModel));
                showActs(SketchBrowserPresentation.sketchActs(chosen), chosen.id, 0L,
                        SketchBrowserText.name(context, chosen));
            }
        } else if (segment == SketchBrowserPresentation.SEGMENT_FEATURES) {
            caption.setText(R.string.feature_browser_caption);
            for (final FeatureHistoryPresentation.Row row
                    : SketchBrowserPresentation.features(lastModel)) {
                final boolean selected = row.id == selectedFeature;
                rows.addView(buildSelectableFeatureRow(context, row, selected),
                        EditorControlStyles.rowParams(index == 0 ? 0 : rowGap));
                if (selected) {
                    focus = index;
                }
                index++;
            }
            final FeatureHistoryPresentation.Row chosen =
                    SketchBrowserPresentation.featureRow(lastModel, selectedFeature);
            if (chosen != null) {
                showDetail(SketchBrowserText.featureDetail(context, lastUnit, chosen,
                        lastInventory, lastModel));
                showActs(SketchBrowserPresentation.featureActs(chosen), chosen.sketchId, chosen.id,
                        FeatureHistoryText.name(context, chosen));
            }
        } else {
            caption.setText(R.string.feature_history_caption);
            for (final FeatureHistoryPresentation.Row row : lastModel.rows) {
                rows.addView(buildRow(context, lastUnit, row, listener),
                        EditorControlStyles.rowParams(index == 0 ? 0 : rowGap));
                index++;
            }
            focus = lastModel.failedRowIndex();
        }
        final int rowHeight = EditorControlStyles.dimen(context, R.dimen.control_height) * 2;
        scroller.setMaxHeightPx(VISIBLE_ROWS * rowHeight + (VISIBLE_ROWS - 1) * rowGap);
        if (focus >= 0) {
            scrollRowIntoView(focus);
        }
    }

    private void showDetail(String text) {
        detail.setText(text);
        detail.setContentDescription(text);
        detail.setVisibility(VISIBLE);
    }

    /**
     * The action strip: one chip per act native accepts, wrapped so every
     * target keeps its full width and 48 dp height — a line never squeezes a
     * fourth chip in.
     */
    private void showActs(List<Integer> acts, final long sketchId, final long featureId,
                          String target) {
        if (acts.isEmpty()) {
            return;
        }
        final Context context = getContext();
        final int width = EditorControlStyles.dimen(context, R.dimen.feature_history_width)
                - getPaddingLeft() - getPaddingRight();
        final int minTarget = EditorControlStyles.dimen(context, R.dimen.control_height) * 2;
        final int columns = SketchBrowserPresentation.actionColumns(width, minTarget, rowGap,
                acts.size());
        LinearLayout line = null;
        for (int i = 0; i < acts.size(); i++) {
            if (i % columns == 0) {
                line = new LinearLayout(context);
                line.setOrientation(HORIZONTAL);
                actions.addView(line, EditorControlStyles.rowParams(i == 0 ? 0 : rowGap));
            }
            final int act = acts.get(i);
            final TextView chip = EditorControlStyles.actionChip(context,
                    SketchBrowserText.actId(act), context.getString(SketchBrowserText.actLabel(act)));
            chip.setContentDescription(SketchBrowserText.actDescription(context, act, target));
            chip.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (actListener != null) {
                        actListener.onHistoryAct(act, sketchId, featureId);
                    }
                }
            });
            line.addView(chip, segmentParams(i % columns == 0 ? 0 : rowGap));
        }
        actions.setVisibility(VISIBLE);
    }

    private TextView buildSketchRow(Context context, final SketchBrowserPresentation.SketchRow row,
                                    boolean selected) {
        final String text = SketchBrowserText.rowText(context, row, lastModel);
        final TextView view = EditorControlStyles.listRow(context, R.id.sketch_browser_row, text);
        view.setSingleLine(false);
        view.setMaxLines(3);
        view.setTag(row);
        if (row.state == NativeViewport.SKETCH_ROW_UNUSED
                || row.state == NativeViewport.SKETCH_ROW_NOT_REGENERATED) {
            view.setAlpha(0.75f);
        } else if (row.state == NativeViewport.SKETCH_ROW_FAILED
                || row.state == NativeViewport.SKETCH_ROW_SUPPORT_LOST) {
            view.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
        }
        EditorControlStyles.setListRowActive(view, selected);
        view.setSelected(selected);
        view.setContentDescription(text.replace('\n', ' '));
        view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                selectedSketch = selectedSketch == row.id ? 0L : row.id;
                render();
            }
        });
        return view;
    }

    private TextView buildSelectableFeatureRow(Context context,
                                               final FeatureHistoryPresentation.Row row,
                                               boolean selected) {
        final TextView view = buildRow(context, lastUnit, row, null);
        EditorControlStyles.setListRowActive(view, selected || row.editing);
        view.setSelected(selected);
        view.setClickable(true);
        view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                selectedFeature = selectedFeature == row.id ? 0L : row.id;
                render();
            }
        });
        return view;
    }

    /**
     * One row: the state glyph and the name on the first line, the detail on
     * the second. Shared with the regeneration issue card so the two always
     * read a row the same way; {@code chosen} null makes the row read-only.
     */
    static TextView buildRow(Context context, LengthUnit unit,
                             final FeatureHistoryPresentation.Row row,
                             final OnHistoryRowChosen chosen) {
        final String name = FeatureHistoryText.name(context, row);
        final String detail = FeatureHistoryText.detail(context, unit, row);
        final TextView view = EditorControlStyles.listRow(context, R.id.feature_history_row,
                row.mark() + "  " + name + "\n" + detail);
        view.setSingleLine(false);
        view.setMaxLines(3);
        view.setTag(row);
        if (row.isFailed()) {
            view.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
            EditorControlStyles.applyMediumWeight(view);
        } else if (row.state == NativeViewport.TIMELINE_STATE_NOT_REGENERATED
                || row.state == NativeViewport.TIMELINE_STATE_PENDING
                || row.state == NativeViewport.TIMELINE_STATE_UNUSED) {
            view.setAlpha(0.6f);
        }
        EditorControlStyles.setListRowActive(view, row.editing);
        final boolean live = chosen != null && row.editable();
        view.setContentDescription(context.getString(live ? R.string.history_row_description
                : R.string.history_row_description_static, name, detail));
        if (live) {
            view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    chosen.onFeatureHistoryRowChosen(row);
                }
            });
        } else {
            view.setClickable(false);
        }
        return view;
    }

    private void scrollRowIntoView(final int index) {
        post(new Runnable() {
            @Override
            public void run() {
                if (index < 0 || index >= rows.getChildCount()) {
                    return;
                }
                final View row = rows.getChildAt(index);
                scroller.scrollTo(0, Math.max(0, row.getTop()
                        - (scroller.getHeight() - row.getHeight()) / 2));
            }
        });
    }

    /** For verification: the row container, so a test reads real rows. */
    LinearLayout rowsContainer() {
        return rows;
    }

    /** For verification: the action strip and the detail line. */
    LinearLayout actionsContainer() {
        return actions;
    }

    TextView detailText() {
        return detail;
    }

    static ViewGroup.LayoutParams anchoredParams(Context context, int bottomOffsetPx) {
        final android.widget.FrameLayout.LayoutParams params =
                new android.widget.FrameLayout.LayoutParams(
                        EditorControlStyles.dimen(context, R.dimen.feature_history_width),
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.BOTTOM | Gravity.END;
        params.bottomMargin = bottomOffsetPx;
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }
}
