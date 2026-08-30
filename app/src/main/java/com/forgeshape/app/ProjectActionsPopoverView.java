package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.View;
import android.widget.TextView;

/**
 * The project actions: save the work, and open it again.
 *
 * <p><b>Two entries, because two things work.</b> There is one app-private
 * project slot, so there is exactly Save Project and Open Saved Project. No New,
 * no Save As, no recent list, no Import and no Export live here — every one of
 * those is either a later stage's feature or, in Export's case, the one approved
 * reserved action that already has its own recessed home in the Global Toolbar.
 * Nothing unimplemented is drawn as a project action.
 *
 * <p>It is an {@link AnchoredSurfaceView} like every other context surface, and
 * it grows out of the control that opened it rather than sliding in from a
 * window edge, so it reads as belonging to that control. It hangs UNDER the
 * toolbar, which is where its anchor is in every window; that is stated here
 * rather than pushed in by the workspace, exactly as the display popover states
 * it, because this surface's anchor cannot move either.
 *
 * <p><b>Owns no state.</b> Each row reports a request; the workspace performs it
 * and writes the outcome to the one status line. The only thing this view is
 * told is whether there is a saved project to open at all, which decides whether
 * Open is offered as a working control or drawn as the recessed, inert row it
 * honestly is — a control that cannot succeed is not offered as though it could.
 */
final class ProjectActionsPopoverView extends AnchoredSurfaceView {

    /** Told which project action was asked for; the caller owns what it means. */
    interface OnProjectAction {
        void onSaveProjectRequested();

        void onOpenProjectRequested();
    }

    private final TextView saveRow;
    private final TextView openRow;

    ProjectActionsPopoverView(Context context, final OnProjectAction listener) {
        super(context);
        setId(R.id.project_actions_popover);
        setGrowsUpward(false);
        // TIER 2, like the display popover: two prose-named actions to be read
        // rather than a capsule to be glanced at, so it is opaque.
        EditorControlStyles.applyContextSurface(this);

        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        setPadding(pad, pad, pad, pad);

        addView(EditorControlStyles.sectionLabel(context, context.getString(R.string.project)),
                EditorControlStyles.rowParams(0));

        saveRow = EditorControlStyles.listRow(context, R.id.project_save,
                context.getString(R.string.project_save));
        saveRow.setGravity(Gravity.CENTER_VERTICAL);
        saveRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onSaveProjectRequested();
            }
        });
        addView(saveRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        openRow = EditorControlStyles.listRow(context, R.id.project_open,
                context.getString(R.string.project_open));
        openRow.setGravity(Gravity.CENTER_VERTICAL);
        openRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onOpenProjectRequested();
            }
        });
        addView(openRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // Both rows carry the interactive floor as HIT AREA, reached with
        // padding rather than by growing the drawn row: the text stays the size
        // it reads at.
        applyTouchFloor(context, saveRow);
        applyTouchFloor(context, openRow);
    }

    /**
     * It hangs under a control in the toolbar's TRAILING group, so it grows
     * from its own trailing top corner — the same corner the display popover
     * grows from, and for the same reason.
     *
     * <p>Without this it grew from its leading edge: the panel unfolded away
     * from the control that opened it, which is precisely the defect the shared
     * anchored contract exists to prevent, and precisely what `UIR4B-09` asks
     * of every surface in the workspace.
     */
    @Override
    boolean anchoredToTrailingEdge() {
        return true;
    }

    private static void applyTouchFloor(Context context, TextView row) {
        // `control_height` IS the 48 dp floor. Applied as a minimum height on a
        // row that is already MATCH_PARENT wide, so the hit area reaches the
        // floor through the row's own box while the label keeps the size it
        // reads at — the glyph is never grown to make a target.
        row.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
    }

    /**
     * Repaints from what is actually on disk.
     *
     * <p>Called every time the surface opens, so the answer is never a cached
     * one: the slot could have been written by this session a moment ago or
     * could have been there since before the process started.
     */
    void showSlotState(boolean hasSavedProject) {
        if (hasSavedProject) {
            openRow.setEnabled(true);
            openRow.setAlpha(1.0f);
            openRow.setText(getContext().getString(R.string.project_open));
            openRow.setContentDescription(getContext().getString(R.string.project_open));
            return;
        }
        // Not a disabled-looking control that would still respond: it is off,
        // and it says why. A row that looks like it works and does not is worse
        // than one that plainly explains there is nothing saved yet.
        openRow.setEnabled(false);
        openRow.setAlpha(0.55f);
        openRow.setText(getContext().getString(R.string.project_open_empty));
        openRow.setContentDescription(getContext().getString(R.string.project_open_empty));
    }

    /** The rows, for verification that names a control by its semantic id. */
    TextView saveRow() {
        return saveRow;
    }

    TextView openRow() {
        return openRow;
    }

    static ViewGroup.LayoutParams anchoredParams(Context context, int topOffsetPx) {
        final android.widget.FrameLayout.LayoutParams params =
                new android.widget.FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP | Gravity.END;
        params.topMargin = topOffsetPx;
        params.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        return params;
    }
}
