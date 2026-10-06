package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The regeneration issue card ({@code MODELING-FOUNDATIONS-R1} A): shown while a
 * staged edit of an EARLIER feature makes a LATER one impossible.
 *
 * <p><b>It reports; it decides nothing.</b> The staged chain and its first
 * failing feature are native's answer, read fresh on every refresh. The card
 * names that feature, says why in words, lists the staged history with the
 * failing row marked (a cross, the reason, the error colour) and the rows after
 * it marked as not rebuilt, and offers exactly two ways forward:
 *
 * <ul>
 *   <li><b>Fix</b> — keep the staged edit open and change the value. The card
 *       collapses until the failure changes.</li>
 *   <li><b>Cancel edit</b> — drop the staged edit. The body is exactly what it
 *       was, because nothing was written: the project only changes on a valid
 *       Finish, as one Undo.</li>
 * </ul>
 *
 * <p>Never offered: deleting the later feature, retargeting it, skipping it or
 * turning anything into a mesh.
 */
final class RegenerationIssueView extends LinearLayout {

    /** Rows visible in the card before its list scrolls. */
    static final int VISIBLE_ROWS = 3;

    interface OnIssueAction {
        void onRegenerationIssueFix();

        void onRegenerationIssueCancel();
    }

    private final TextView title;
    private final TextView reason;
    private final LinearLayout rows;
    private final BoundedScrollView scroller;
    private final int rowGap;
    private String shownKey = "";

    RegenerationIssueView(Context context, final OnIssueAction listener) {
        super(context);
        setId(R.id.regeneration_issue);
        setOrientation(VERTICAL);
        EditorControlStyles.applyContextSurface(this);
        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        rowGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        setPadding(pad, pad, pad, pad);
        setElevation(EditorControlStyles.dimen(context, R.dimen.elevation_floating));
        // The card consumes taps on itself so a press between its controls can
        // never fall through to the model behind it.
        setClickable(true);

        title = EditorControlStyles.titleText(context, R.id.regeneration_issue_title, "");
        title.setTextColor(EditorControlStyles.themeColor(context, R.attr.fsTextError));
        addView(title, EditorControlStyles.rowParams(0));
        reason = EditorControlStyles.captionText(context, R.id.regeneration_issue_reason, "");
        addView(reason, EditorControlStyles.rowParams(rowGap));

        scroller = new BoundedScrollView(context);
        scroller.setId(R.id.regeneration_issue_rows);
        scroller.setFillViewport(false);
        addView(scroller, EditorControlStyles.rowParams(rowGap));
        rows = new LinearLayout(context);
        rows.setOrientation(VERTICAL);
        scroller.addView(rows, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        addView(EditorControlStyles.captionText(context, View.NO_ID,
                context.getString(R.string.regeneration_issue_hint)), EditorControlStyles.rowParams(rowGap));

        final LinearLayout actions = new LinearLayout(context);
        actions.setOrientation(HORIZONTAL);
        final TextView fix = EditorControlStyles.primaryButton(context, R.id.regeneration_issue_fix,
                context.getString(R.string.regeneration_issue_fix));
        fix.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onRegenerationIssueFix();
            }
        });
        final TextView cancel = EditorControlStyles.chip(context, R.id.regeneration_issue_cancel,
                context.getString(R.string.regeneration_issue_cancel));
        cancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onRegenerationIssueCancel();
            }
        });
        final int gap = EditorControlStyles.dimen(context, R.dimen.row_gap);
        actions.addView(fix, EditorControlStyles.evenShare(0));
        actions.addView(cancel, EditorControlStyles.evenShare(gap));
        addView(actions, EditorControlStyles.rowParams(gap));
        setVisibility(GONE);
    }

    /** Fills the card from one staged read; the caller decided it is owed. */
    void showIssue(FeatureHistoryPresentation.Model staged, LengthUnit unit) {
        final Context context = getContext();
        final FeatureHistoryPresentation.Row failed = staged.failedRow();
        if (failed == null) {
            hide();
            return;
        }
        final String name = FeatureHistoryText.name(context, failed);
        title.setText(context.getString(R.string.regeneration_issue_title, name));
        reason.setText(FeatureHistoryText.detail(context, unit, failed));
        rows.removeAllViews();
        int index = 0;
        for (FeatureHistoryPresentation.Row row : staged.rows) {
            if (row.isSketch() && !row.editing) {
                // The card lists what was edited and what it broke; the sketches
                // that only carry later features add length, not information.
                continue;
            }
            rows.addView(FeatureHistoryView.buildRow(context, unit, row, null),
                    EditorControlStyles.rowParams(index == 0 ? 0 : rowGap));
            index++;
        }
        final int rowHeight = EditorControlStyles.dimen(context, R.dimen.control_height) * 2;
        scroller.setMaxHeightPx(VISIBLE_ROWS * rowHeight + (VISIBLE_ROWS - 1) * rowGap);
        shownKey = staged.issueKey();
        setVisibility(VISIBLE);
    }

    void hide() {
        shownKey = "";
        setVisibility(GONE);
    }

    /** The issue on screen, or "" when the card is hidden. */
    String shownKey() {
        return shownKey;
    }

    LinearLayout rowsContainer() {
        return rows;
    }

    /** Hangs below the toolbar, centred, where Ready leaves the model clear. */
    static ViewGroup.LayoutParams anchoredParams(Context context, int topOffsetPx) {
        final FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                EditorControlStyles.dimen(context, R.dimen.regeneration_issue_width),
                ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        params.topMargin = topOffsetPx;
        return params;
    }
}
