package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The Sculpt History navigator ({@code SCULPT-H1}): the active body's retained
 * sculpt states, and one tap to stand on any of them.
 *
 * <p><b>It is a VIEW of a history that already existed.</b> {@code
 * SculptHistory} has been a bounded, volatile, per-body branch of completed
 * strokes since {@code ARCH-OWNER-12}, and the two chrome controls have been
 * stepping along it one entry at a time. This surface only draws that branch
 * and names a position on it. It adds no storage, no capacity, no budget and no
 * second stack — which is why nothing here is cached: {@link #showHistory} is
 * handed a freshly read model on every refresh, exactly as Undo and Redo re-read
 * their enabled state, because a remembered copy would be the second answer to
 * "what can be undone" that one history exists to prevent.
 *
 * <p><b>A row is a STATE, not an entry.</b> Two strokes make three rows: where
 * the mesh started and where each stroke left it. That is the difference
 * between this and a list of things that happened — the user is choosing
 * somewhere to stand, not something to replay — and it is why row zero has a
 * name of its own rather than a stroke number.
 *
 * <p><b>Five rows visible, and the branch is not five long.</b> {@link
 * #VISIBLE_ROWS} is a viewport target: it is what fits above the bottom row on
 * the shortest supported window without this becoming a panel. The list scrolls
 * over every state {@code SculptHistory} kept, up to its own thirty-two entry
 * cap, and the sixth row is one scroll away rather than absent.
 *
 * <p><b>The current state is marked by SHAPE, not by colour alone.</b> Each row
 * carries a leading glyph — filled for the state on screen, hollow for one
 * behind it, ring-with-a-gap for one ahead — and the accessible label says the
 * same thing in words. The accent fill and the dimmed future are the third and
 * fourth signals, not the only ones.
 *
 * <p><b>Owns no state and decides nothing.</b> A tap reports an ordinal; native
 * code performs the jump and the workspace repaints from what native code
 * reports afterwards. This class never calls {@code NativeViewport}.
 */
final class SculptHistoryNavigatorView extends AnchoredSurfaceView {

    /**
     * How many rows are visible before the list scrolls.
     *
     * <p>A VIEWPORT target and not a history bound — see the class comment. The
     * older {@code UI-OWNER-41} "five actions" experiment is this number and
     * nothing more: the history itself was never shortened to five.
     */
    static final int VISIBLE_ROWS = 5;

    /** Told which state the user tapped; the caller owns what it means. */
    interface OnHistoryStateChosen {
        void onSculptHistoryStateChosen(int ordinal);
    }

    /** Filled: the state the mesh shows right now. */
    private static final String MARK_CURRENT = "●";
    /** Hollow: a state behind the cursor, reachable by going back. */
    private static final String MARK_PAST = "○";
    /** Dotted: a state ahead of the cursor, reachable by going forward. */
    private static final String MARK_FUTURE = "◌";

    private final OnHistoryStateChosen listener;
    private final TextView title;
    private final BoundedScrollView scroller;
    private final LinearLayout rows;
    private final TextView droppedNotice;
    private final int rowGap;

    SculptHistoryNavigatorView(Context context, OnHistoryStateChosen listener) {
        super(context);
        this.listener = listener;
        setId(R.id.history_navigator);
        // It grows out of the bottom row's history capsule, so it always
        // unfolds UPWARD. Stated once here rather than pushed in by the
        // workspace, because unlike the surfaces that hang off the Objects
        // capsule this anchor cannot move between window shapes.
        setGrowsUpward(true);
        // TIER 2. It is a body of rows to be read and chosen from rather than a
        // capsule to be glanced at, so it is opaque like the Objects panel and
        // the Display popover.
        EditorControlStyles.applyContextSurface(this);

        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        rowGap = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        setPadding(pad, pad, pad, pad);

        title = EditorControlStyles.sectionLabel(context,
                context.getString(R.string.sculpt_history));
        addView(title, EditorControlStyles.rowParams(0));

        // The rows go inside a bounded scroller rather than straight into this
        // column. A vertical LinearLayout hands its height to children in
        // order, so a long branch would push the dropped notice below it off
        // the surface; a scroller can honestly give height back because what it
        // cannot show is still reachable.
        scroller = new BoundedScrollView(context);
        scroller.setId(R.id.history_navigator_list);
        scroller.setFillViewport(false);
        // The list is what the user flicks through. Without this the surface
        // itself would keep the drag and the rows would never move.
        scroller.setVerticalScrollBarEnabled(true);
        addView(scroller, EditorControlStyles.rowParams(rowGap));

        rows = new LinearLayout(context);
        rows.setOrientation(VERTICAL);
        scroller.addView(rows, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        droppedNotice = EditorControlStyles.captionText(context, View.NO_ID,
                context.getString(R.string.sculpt_history_dropped));
        droppedNotice.setVisibility(GONE);
        addView(droppedNotice, EditorControlStyles.rowParams(rowGap));
    }

    /**
     * Repaints the whole list from one freshly read model.
     *
     * <p>Rebuilt rather than diffed. The branch is at most thirty-three rows of
     * one {@code TextView} each, and rebuilding is what makes the drawn list
     * incapable of describing a branch that has moved — a diff would have to
     * decide which rows survived a truncation and an eviction in the same
     * refresh, which is exactly the bookkeeping a re-read exists to avoid.
     *
     * @param stateCount how many states the retained branch holds; never zero
     * @param cursor which of them the mesh currently shows
     * @param anyDropped whether eviction has taken states off the oldest end,
     *     which is what makes row zero "Start" rather than the original mesh
     */
    void showHistory(int stateCount, int cursor, boolean anyDropped) {
        rows.removeAllViews();
        final Context context = getContext();
        final int total = Math.max(1, stateCount);
        for (int ordinal = 0; ordinal < total; ordinal++) {
            rows.addView(buildRow(context, ordinal, cursor),
                    EditorControlStyles.rowParams(ordinal == 0 ? 0 : rowGap));
        }
        droppedNotice.setVisibility(anyDropped ? VISIBLE : GONE);
        applyRowCap();
        // Bring the cursor into view. On open the user is almost always at the
        // newest state, which is the bottom of an oldest-first list, so without
        // this the surface would open showing the beginning of a long session
        // and hide the part they are working in.
        scrollCursorIntoView(cursor, total);
    }

    /**
     * Builds one row: a marker, a name, and what a screen reader is told.
     *
     * <p>Ordinal zero is "Start" — the state before the oldest stroke still
     * kept — and every other row is the stroke that produced it. The numbering
     * is over the RETAINED branch, so after an eviction "Stroke 1" is the
     * oldest kept stroke rather than the first one ever made; {@link
     * #droppedNotice} is what says so, rather than a row label that would
     * quietly become untrue.
     */
    private TextView buildRow(Context context, int ordinal, int cursor) {
        final String name = ordinal == 0
                ? context.getString(R.string.sculpt_history_start)
                : context.getString(R.string.sculpt_history_stroke, ordinal);
        final String mark = ordinal == cursor ? MARK_CURRENT
                : (ordinal < cursor ? MARK_PAST : MARK_FUTURE);
        final TextView row = EditorControlStyles.listRow(context, R.id.history_navigator_row,
                mark + "  " + name);
        // Every row is a 48 dp target: listRow inherits the chip minimum
        // height, and the padding is the chip's. Nothing here shrinks a drawn
        // box to fit more rows -- the list scrolls instead.
        EditorControlStyles.setListRowActive(row, ordinal == cursor);
        if (ordinal == cursor) {
            EditorControlStyles.applyMediumWeight(row);
            row.setContentDescription(
                    context.getString(R.string.sculpt_history_row_current, name));
        } else if (ordinal > cursor) {
            // Dimmed, but still fully operable: a future state is exactly as
            // reachable as a past one, so this is emphasis and never a disabled
            // control drawn as a promise.
            row.setAlpha(0.6f);
            row.setContentDescription(
                    context.getString(R.string.sculpt_history_row_undone, name));
        } else {
            row.setContentDescription(name);
        }
        final int chosen = ordinal;
        row.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onSculptHistoryStateChosen(chosen);
            }
        });
        return row;
    }

    /**
     * Caps the scroller at {@link #VISIBLE_ROWS} rows.
     *
     * <p>Measured from the row height the rows actually use rather than from a
     * dimension repeated here, so a larger system font grows the rows and the
     * cap together instead of clipping the fifth one.
     */
    private void applyRowCap() {
        final int rowHeight = EditorControlStyles.dimen(getContext(), R.dimen.control_height);
        scroller.setMaxHeightPx(VISIBLE_ROWS * rowHeight + (VISIBLE_ROWS - 1) * rowGap);
    }

    /**
     * Scrolls so the cursor row is visible, once the list has a size.
     *
     * <p>Posted rather than called straight: the rows were added this frame and
     * have not been laid out, so their heights are still zero and a scroll
     * computed now would land at the top whatever the cursor is.
     */
    private void scrollCursorIntoView(final int cursor, final int total) {
        post(new Runnable() {
            @Override
            public void run() {
                if (cursor < 0 || cursor >= rows.getChildCount()) {
                    return;
                }
                final View row = rows.getChildAt(cursor);
                final int centred = row.getTop()
                        - (scroller.getHeight() - row.getHeight()) / 2;
                scroller.scrollTo(0, Math.max(0, centred));
            }
        });
    }

    /** For verification: the row container, so a test reads real rows. */
    LinearLayout rowsContainer() {
        return rows;
    }

    /** For verification: what the parent last allowed the list. */
    BoundedScrollView listScroller() {
        return scroller;
    }

    /**
     * Where the surface hangs: the bottom trailing corner, over the history
     * capsule it grew out of.
     *
     * <p>Bottom-anchored rather than top-anchored because its invoker is in the
     * bottom row, and a surface that grows out of a control has to start AT
     * that control — see {@link AnchoredSurfaceView}.
     */
    static ViewGroup.LayoutParams anchoredParams(Context context, int bottomOffsetPx) {
        final android.widget.FrameLayout.LayoutParams params =
                new android.widget.FrameLayout.LayoutParams(
                        EditorControlStyles.dimen(context, R.dimen.history_navigator_width),
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.BOTTOM | Gravity.END;
        params.bottomMargin = bottomOffsetPx;
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }
}
