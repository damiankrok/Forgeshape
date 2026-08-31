package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.View;
import android.widget.TextView;

/**
 * The project actions: keep the work, get it back, and move it off the device.
 *
 * <p><b>Every entry works, and there are only as many as there are working
 * things.</b> Two groups, because there are two questions. The first is the
 * app's own storage — one project slot, so exactly Save Project and Open Saved
 * Project. The second is the device's, through the system's own document UI:
 * Save Copy… writes the same canonical `.forge` bytes wherever the user says,
 * Open File… reads one back, and Share Diagnostics… writes the local report.
 *
 * <p><b>Transfer is not interchange.</b> What Save Copy… writes is a ForgeShape
 * project, readable by another ForgeShape installation. The wording never says
 * export, because Export is a different act with its own home in the Global
 * Toolbar. No New, no Save As and no recent list either.
 *
 * <p><b>The third group is a DIAGNOSTIC, not import.</b> It opens a `.glb` —
 * in practice one ForgeShape just wrote — reads it with a parser that shares
 * nothing with the writer, and shows the result in place of the model so the
 * two can be compared. What it produces is not an object: it cannot be
 * selected, edited, sculpted, saved, autosaved or exported, and it is gone when
 * the app restarts. Production import is post-MVP, and no row here suggests
 * otherwise — which is why every one of them says <i>check</i> or
 * <i>preview</i> and none says "import" on its own. OBJ and FBX remain absent
 * in both directions.
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

        void onSaveCopyRequested();

        void onOpenFileRequested();

        void onShareDiagnosticsRequested();

        /** GLB-IMPORT-R0: pick a `.glb` and read it back as a preview. */
        void onImportGlbPreviewRequested();

        /** Swap the viewport between the model and the imported preview. */
        void onToggleImportedPreviewRequested();

        /** Forget the preview and go back to the model. */
        void onClearImportedPreviewRequested();
    }

    private final TextView saveRow;
    private final TextView openRow;
    private final TextView saveCopyRow;
    private final TextView openFileRow;
    private final TextView diagnosticsRow;
    private final TextView previewSectionLabel;
    private final TextView importPreviewRow;
    private final TextView togglePreviewRow;
    private final TextView clearPreviewRow;

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

        // The second group: the same project, moved through the device's own
        // storage rather than the app's. Under its own label because "where
        // this file lives" is a different question from "save my work", and a
        // flat list of four would make Save Copy look like a second Save.
        addView(EditorControlStyles.sectionLabel(context,
                context.getString(R.string.project_transfer)),
                EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));

        saveCopyRow = EditorControlStyles.listRow(context, R.id.project_save_copy,
                context.getString(R.string.project_save_copy));
        saveCopyRow.setGravity(Gravity.CENTER_VERTICAL);
        saveCopyRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onSaveCopyRequested();
            }
        });
        addView(saveCopyRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        openFileRow = EditorControlStyles.listRow(context, R.id.project_open_file,
                context.getString(R.string.project_open_file));
        openFileRow.setGravity(Gravity.CENTER_VERTICAL);
        openFileRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onOpenFileRequested();
            }
        });
        addView(openFileRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // The local diagnostic report. It sits here because it is the third
        // thing that leaves the app through the system's own document UI, and
        // giving it a surface of its own would be a settings screen this product
        // does not have. Nothing is sent anywhere: the user picks a file.
        diagnosticsRow = EditorControlStyles.listRow(context, R.id.project_share_diagnostics,
                context.getString(R.string.project_share_diagnostics));
        diagnosticsRow.setGravity(Gravity.CENTER_VERTICAL);
        diagnosticsRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onShareDiagnosticsRequested();
            }
        });
        addView(diagnosticsRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap)));

        // GLB-IMPORT-R0. A third group, because it answers a third question:
        // not "keep my work" and not "move my work", but "is the file I just
        // exported right?". It is a DIAGNOSTIC and is worded as one — nothing
        // here creates an object, and nothing it shows can be edited or saved.
        previewSectionLabel = EditorControlStyles.sectionLabel(context,
                context.getString(R.string.preview_section));
        addView(previewSectionLabel, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap)));

        importPreviewRow = EditorControlStyles.listRow(context, R.id.import_glb_preview,
                context.getString(R.string.import_glb_preview));
        importPreviewRow.setGravity(Gravity.CENTER_VERTICAL);
        importPreviewRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onImportGlbPreviewRequested();
            }
        });
        addView(importPreviewRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // The next two are ABSENT until there is a preview, rather than drawn
        // and disabled: neither can succeed with nothing loaded, and a control
        // that cannot succeed is not drawn.
        togglePreviewRow = EditorControlStyles.listRow(context, R.id.toggle_imported_preview,
                context.getString(R.string.show_imported_preview));
        togglePreviewRow.setGravity(Gravity.CENTER_VERTICAL);
        togglePreviewRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onToggleImportedPreviewRequested();
            }
        });
        addView(togglePreviewRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        clearPreviewRow = EditorControlStyles.listRow(context, R.id.clear_imported_preview,
                context.getString(R.string.clear_imported_preview));
        clearPreviewRow.setGravity(Gravity.CENTER_VERTICAL);
        clearPreviewRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onClearImportedPreviewRequested();
            }
        });
        addView(clearPreviewRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // Every row carries the interactive floor as HIT AREA, reached with
        // padding rather than by growing the drawn row: the text stays the size
        // it reads at.
        applyTouchFloor(context, saveRow);
        applyTouchFloor(context, openRow);
        applyTouchFloor(context, saveCopyRow);
        applyTouchFloor(context, openFileRow);
        applyTouchFloor(context, diagnosticsRow);
        applyTouchFloor(context, importPreviewRow);
        applyTouchFloor(context, togglePreviewRow);
        applyTouchFloor(context, clearPreviewRow);

        showPreviewState(false, false);
    }

    /**
     * Draws only the preview controls that can currently do something.
     *
     * <p>With nothing loaded there is nothing to show and nothing to clear, so
     * both rows are GONE rather than disabled — they take no space and offer no
     * target. The toggle names what pressing it will DO, not what is currently
     * on screen, because a row that reads "Showing my model" would be a status
     * line pretending to be a control.
     */
    void showPreviewState(boolean loaded, boolean previewVisible) {
        final int visibility = loaded ? View.VISIBLE : View.GONE;
        togglePreviewRow.setVisibility(visibility);
        clearPreviewRow.setVisibility(visibility);
        if (!loaded) {
            return;
        }
        final int label = previewVisible ? R.string.show_source_instead
                : R.string.show_imported_preview;
        togglePreviewRow.setText(getContext().getString(label));
        togglePreviewRow.setContentDescription(getContext().getString(label));
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

    TextView saveCopyRow() {
        return saveCopyRow;
    }

    TextView openFileRow() {
        return openFileRow;
    }

    TextView diagnosticsRow() {
        return diagnosticsRow;
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
