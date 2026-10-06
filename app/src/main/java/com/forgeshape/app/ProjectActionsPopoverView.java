package com.forgeshape.app;

import android.content.Context;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The project DRAWER: what the ForgeShape mark at the top-left grows into
 * (`MODELING-R1-OWNER-CORRECTION`).
 *
 * <p>It hangs from the mark at the window's LEADING edge, under the toolbar,
 * and unfolds from the mark's own corner -- the anchored-surface growth every
 * panel in the workspace uses, pivoted where the mark is, so the drawer is
 * visibly the mark opening rather than a page arriving from nowhere. It stands
 * on the model and blocks nothing else: the viewport, the Tool Rail and the
 * utility group stay live beside it, and the mark or System Back closes it.
 *
 * <p><b>Every row works, and there are only as many as there are working
 * things</b>, grouped by the question each answers (`ProjectDrawerPolicy`):
 * CREATE -- New Sketch in this project, or a New Project; PROJECT -- Save
 * Project and Open Saved Project (the app's one slot); TRANSFER -- Save Copy…,
 * Open File… and Share Diagnostics… through the system's own document UI;
 * IMPORT -- Import GLB…; APPLICATION -- Settings.
 *
 * <p><b>Transfer is not interchange.</b> What Save Copy… writes is a ForgeShape
 * project, readable by another ForgeShape installation; Export is a different
 * act with its own home in the Global Toolbar. No Save As and no recent list.
 *
 * <p><b>Import GLB… creates real objects</b> (`IMPORT-01A`), one Undo step for
 * the whole import; it is the ONE user-facing GLB route.
 *
 * <p><b>Owns no state and no act.</b> Each row reports a request through
 * {@link ProjectDrawerPolicy#perform}, which closes the drawer and calls the
 * ONE handler the workspace already had -- New Sketch is the very path Add
 * Primitive's tile takes into the spatial support chooser. This view is told
 * only whether there is a saved project to open, whether import and New Sketch
 * can succeed now, and nothing else.
 */
final class ProjectActionsPopoverView extends AnchoredSurfaceView {

    /** Told which project action was asked for; the caller owns what it means. */
    interface OnProjectAction {
        /** `MODELING-R1-OWNER-CORRECTION`: New Sketch in this project -- the
         *  spatial support chooser, exactly as Add Primitive's tile enters it. */
        void onNewSketchRequested();

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

        /** `UI-PREF-R1`: open the Settings page over the project. */
        void onSettingsRequested();
    }

    private final TextView newSketchRow;
    private final TextView newRow;
    private final TextView saveRow;
    private final TextView openRow;
    private final TextView saveCopyRow;
    private final TextView openFileRow;
    private final TextView diagnosticsRow;
    private final TextView importSectionLabel;
    private final TextView importRow;
    private final TextView settingsRow;
    private final View header;

    ProjectActionsPopoverView(Context context, final OnProjectAction listener,
                              final ProjectDrawerPolicy.Closer closer) {
        super(context);
        setId(R.id.project_actions_popover);
        setGrowsUpward(false);
        // TIER 2, like the display popover: prose-named actions to be read
        // rather than a capsule to be glanced at, so it is opaque.
        EditorControlStyles.applyContextSurface(this);
        setMinimumWidth(EditorControlStyles.dimen(context, R.dimen.project_drawer_min_width));

        final int pad = EditorControlStyles.dimen(context, R.dimen.inspector_padding);
        setPadding(pad, pad, pad, pad);

        // The mark again, at the drawer's head and beside the product's name:
        // the drawer is what the mark opened into. Decorative to a screen
        // reader -- the name says it.
        final LinearLayout head = new LinearLayout(context);
        head.setId(R.id.project_drawer_header);
        head.setOrientation(HORIZONTAL);
        head.setGravity(Gravity.CENTER_VERTICAL);
        final View mark = EditorControlStyles.icon(context, R.drawable.ic_forgeshape_mark,
                R.dimen.icon_size);
        mark.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
        head.addView(mark, mark.getLayoutParams());
        final TextView name = EditorControlStyles.titleText(context, View.NO_ID,
                context.getString(R.string.app_name));
        final LinearLayout.LayoutParams nameParams = EditorControlStyles.rowParams(0);
        nameParams.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        head.addView(name, nameParams);
        head.setContentDescription(context.getString(R.string.app_name));
        header = head;
        addView(head, EditorControlStyles.rowParams(0));

        // The rows scroll inside the drawer, so a short landscape window can
        // reach Settings at the bottom: the drawer is bounded by the window it
        // hangs in, never pushed past its edge.
        final ScrollView scroll = new ScrollView(context);
        scroll.setId(R.id.project_drawer_scroll);
        scroll.setVerticalScrollBarEnabled(true);
        final LinearLayout rows = new LinearLayout(context);
        rows.setOrientation(VERTICAL);
        scroll.addView(rows, new ScrollView.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(scroll, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        final TextView[] built = new TextView[ProjectDrawerPolicy.ROWS.length];
        TextView importLabel = null;
        ProjectDrawerPolicy.Group group = null;
        for (int i = 0; i < ProjectDrawerPolicy.ROWS.length; i++) {
            final ProjectDrawerPolicy.Row row = ProjectDrawerPolicy.ROWS[i];
            if (row.group != group) {
                group = row.group;
                final TextView label = EditorControlStyles.sectionLabel(context,
                        context.getString(ProjectDrawerPolicy.groupLabel(group)));
                if (group == ProjectDrawerPolicy.Group.IMPORT) {
                    importLabel = label;
                }
                rows.addView(label, EditorControlStyles.rowParams(
                        EditorControlStyles.dimen(context, R.dimen.row_gap)));
            }
            final TextView view = EditorControlStyles.listRow(context, row.id,
                    context.getString(row.label));
            view.setGravity(Gravity.CENTER_VERTICAL);
            // Every row carries the interactive floor as HIT AREA, reached
            // through the row's own box while the label keeps the size it
            // reads at -- the glyph is never grown to make a target.
            view.setMinimumHeight(EditorControlStyles.dimen(context, R.dimen.control_height));
            view.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    ProjectDrawerPolicy.perform(row.action, closer, listener);
                }
            });
            rows.addView(view, EditorControlStyles.rowParams(
                    EditorControlStyles.dimen(context, R.dimen.row_gap_small)));
            built[i] = view;
        }
        newSketchRow = built[0];
        newRow = built[1];
        saveRow = built[2];
        openRow = built[3];
        saveCopyRow = built[4];
        openFileRow = built[5];
        diagnosticsRow = built[6];
        importRow = built[7];
        settingsRow = built[8];
        importSectionLabel = importLabel;
        settingsRow.setContentDescription(context.getString(R.string.settings));
    }

    /**
     * Draws or withdraws New Sketch: offered exactly when it can succeed
     * ({@link ProjectDrawerPolicy#newSketchShown}), absent otherwise.
     */
    void showNewSketchAvailable(boolean available) {
        newSketchRow.setVisibility(available ? VISIBLE : GONE);
    }

    /** New Sketch, for verification. */
    TextView newSketchRow() {
        return newSketchRow;
    }

    /** The drawer's head (the mark and the product's name), for verification. */
    View header() {
        return header;
    }

    /**
     * Draws or withdraws the import group.
     *
     * <p>Import is not a Sculpt act: it creates bodies and makes the first one
     * active, and in Sculpt the active body is the sculpt target. So the row
     * and the label that exists only to introduce it are ABSENT there, not
     * drawn and then refused. The workspace passes native's answer on every
     * refresh; this view remembers nothing, and the guards below it stay.
     */
    void showImportAvailable(boolean available) {
        final int visibility = available ? VISIBLE : GONE;
        importSectionLabel.setVisibility(visibility);
        importRow.setVisibility(visibility);
    }

    /** The import section label, so a layout test can find it by reference. */
    View importSectionLabel() {
        return importSectionLabel;
    }

    /**
     * It hangs from the ForgeShape mark, the toolbar's LEADING control, so it
     * grows from its own leading top corner -- where the mark is.
     */
    @Override
    boolean anchoredToTrailingEdge() {
        return !ProjectDrawerPolicy.anchoredToLeadingEdge();
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

    TextView settingsRow() {
        return settingsRow;
    }

    static ViewGroup.LayoutParams anchoredParams(Context context, int topOffsetPx) {
        final android.widget.FrameLayout.LayoutParams params =
                new android.widget.FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.TOP | Gravity.START;
        params.topMargin = topOffsetPx;
        // Under the mark: the toolbar's own leading inset, so the drawer's
        // edge and the mark's capsule line up.
        params.leftMargin = EditorControlStyles.dimen(context, R.dimen.toolbar_padding_horizontal);
        return params;
    }
}
