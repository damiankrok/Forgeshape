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
 * things.</b> Three groups, because there are three questions. The first is the
 * app's own storage — New Project…, Save Project and Open Saved Project (one
 * slot). The second is the device's, through the system's own document UI:
 * Save Copy… writes the same canonical `.forge` bytes wherever the user says,
 * Open File… reads one back, and Share Diagnostics… writes the local report.
 * The third is somebody else's mesh — Import GLB…, and nothing beside it.
 *
 * <p><b>Transfer is not interchange.</b> What Save Copy… writes is a ForgeShape
 * project, readable by another ForgeShape installation; Export is a different
 * act with its own home in the Global Toolbar. No Save As and no recent list.
 *
 * <p><b>Import GLB… creates real objects</b>: one per supported mesh node, each
 * a row in the Objects list with the ordinary gizmo, one Undo step for the
 * whole import, and geometry saved into `.forge` without the source file. An
 * imported object is non-parametric, so <i>Shape</i> is withdrawn for one; it
 * can be sculpted (`IMPORT-01B`). OBJ and FBX remain absent in both
 * directions. This is the ONE user-facing GLB route: the session-only
 * diagnostic preview below JNI is reached only from the verification suites,
 * because two visible ways to open a `.glb` that did different things to the
 * project is exactly the confusion to avoid.
 *
 * <p>It is an {@link AnchoredSurfaceView} that hangs UNDER the toolbar, where
 * its anchor is in every window; that is stated here rather than pushed in by
 * the workspace, exactly as the display popover states it.
 *
 * <p><b>Owns no state.</b> Each row reports a request; the workspace performs it
 * and writes the outcome to the one status line. The only thing this view is
 * told is whether there is a saved project to open at all, which decides whether
 * Open is a working control or the recessed, inert row it honestly is.
 */
final class ProjectActionsPopoverView extends AnchoredSurfaceView {

    /** Told which project action was asked for; the caller owns what it means. */
    interface OnProjectAction {
        /** `APP-H1`: leave this project for a new one. Guarded by the
         *  unsaved-changes question when the project is dirty. */
        void onNewProjectRequested();

        void onSaveProjectRequested();

        void onOpenProjectRequested();

        void onSaveCopyRequested();

        void onOpenFileRequested();

        void onShareDiagnosticsRequested();

        /** `IMPORT-01A`: pick a `.glb` and turn what it describes into objects. */
        void onImportGlbRequested();
    }

    private final TextView newRow;
    private final TextView saveRow;
    private final TextView openRow;
    private final TextView saveCopyRow;
    private final TextView openFileRow;
    private final TextView diagnosticsRow;
    private final TextView importSectionLabel;
    private final TextView importRow;

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

        // New Project (APP-H1): the way from this project to another, through
        // the New Project chooser and -- when there are unsaved changes -- the
        // one question that guards them. First, because it is the act that
        // leaves; Save and Open are acts on the project the user is in.
        newRow = EditorControlStyles.listRow(context, R.id.project_new,
                context.getString(R.string.project_new));
        newRow.setGravity(Gravity.CENTER_VERTICAL);
        newRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onNewProjectRequested();
            }
        });
        addView(newRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

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

        // A third group, because it answers a third question: not "keep my
        // work" and not "move my work", but "bring somebody else's mesh in".
        // Since `IMPORT-01A` that is a real act — it creates objects the user
        // can select, move, undo and save — so the group is named for the
        // objects it makes and nothing here says preview any more.
        importSectionLabel = EditorControlStyles.sectionLabel(context,
                context.getString(R.string.import_section));
        addView(importSectionLabel, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap)));

        importRow = EditorControlStyles.listRow(context, R.id.import_glb,
                context.getString(R.string.import_glb));
        importRow.setGravity(Gravity.CENTER_VERTICAL);
        importRow.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                listener.onImportGlbRequested();
            }
        });
        addView(importRow, EditorControlStyles.rowParams(
                EditorControlStyles.dimen(context, R.dimen.row_gap_small)));

        // Every row carries the interactive floor as HIT AREA, reached with
        // padding rather than by growing the drawn row: the text stays the size
        // it reads at.
        applyTouchFloor(context, newRow);
        applyTouchFloor(context, saveRow);
        applyTouchFloor(context, openRow);
        applyTouchFloor(context, saveCopyRow);
        applyTouchFloor(context, openFileRow);
        applyTouchFloor(context, diagnosticsRow);
        applyTouchFloor(context, importRow);
    }

    /** The import section label, so a layout test can find it by reference. */
    View importSectionLabel() {
        return importSectionLabel;
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
    TextView newRow() {
        return newRow;
    }

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
