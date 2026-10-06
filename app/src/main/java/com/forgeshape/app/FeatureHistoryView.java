package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

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
 * <p><b>A small floating surface, not a timeline bar.</b> It grows out of the
 * History control in the history capsule, shows five rows and scrolls beyond
 * them — the Sculpt History navigator's shape, because a phone has no room for
 * a permanent desktop strip. Every row is a 48 dp target.
 *
 * <p><b>States are marked by SHAPE.</b> A filled dot is regenerated, a cross is
 * the first feature that cannot be rebuilt (and says why in words), a dotted
 * ring is a feature after it that was never evaluated, a pencil marks what a
 * staged edit changes. Colour is the second carrier, never the only one.
 *
 * <p>Owns no state and decides nothing: a tap reports the row; the workspace
 * opens that row's existing editor.
 */
final class FeatureHistoryView extends AnchoredSurfaceView {

    /** Rows visible before the list scrolls; a viewport target, not a bound. */
    static final int VISIBLE_ROWS = 5;

    /** Told which row the user tapped; the caller owns what it means. */
    interface OnHistoryRowChosen {
        void onFeatureHistoryRowChosen(FeatureHistoryPresentation.Row row);
    }

    private final OnHistoryRowChosen listener;
    private final BoundedScrollView scroller;
    private final LinearLayout rows;
    private final TextView caption;
    private final int rowGap;

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

    /** Repaints the whole list from one freshly read model. Rebuilt, never diffed. */
    void showModel(FeatureHistoryPresentation.Model model, LengthUnit unit) {
        rows.removeAllViews();
        final Context context = getContext();
        int index = 0;
        for (final FeatureHistoryPresentation.Row row : model.rows) {
            rows.addView(buildRow(context, unit, row, listener),
                    EditorControlStyles.rowParams(index == 0 ? 0 : rowGap));
            index++;
        }
        final int rowHeight = EditorControlStyles.dimen(context, R.dimen.control_height) * 2;
        scroller.setMaxHeightPx(VISIBLE_ROWS * rowHeight + (VISIBLE_ROWS - 1) * rowGap);
        final int failed = model.failedRowIndex();
        if (failed >= 0) {
            scrollRowIntoView(failed);
        }
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
