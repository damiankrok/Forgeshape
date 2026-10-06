package com.forgeshape.app;

/**
 * What the project drawer holds, in what order, and what each row DOES
 * (`MODELING-R1-OWNER-CORRECTION`).
 *
 * <p>Pure Java, so the JVM pins it without a device: the drawer view builds its
 * rows from {@link #ROWS} and every row's click goes through
 * {@link #perform}, which is the one place a row is bound to a handler. There
 * is no second save, open or sketch path here — every action is a method the
 * workspace already implemented for the old project popover or for Add
 * Primitive's New Sketch, called exactly as those called it.
 *
 * <p><b>Close first, then act.</b> The drawer is dismissed before the handler
 * runs, so an act that puts the viewport into a mode — New Sketch entering the
 * spatial support chooser above all — never begins with the drawer still
 * standing over the planes and faces it asks the user to tap.
 */
final class ProjectDrawerPolicy {

    private ProjectDrawerPolicy() {}

    /** The drawer's groups, top to bottom. */
    enum Group {
        /** Make something: a sketch in this project, or another project. */
        CREATE,
        /** Keep this project in the app's own slot, or get it back. */
        PROJECT,
        /** Move this project through the device's own storage. */
        TRANSFER,
        /** Bring somebody else's mesh in. */
        IMPORT,
        /** The application, not the project. */
        APP
    }

    /** One act a row performs. */
    enum Action {
        NEW_SKETCH,
        NEW_PROJECT,
        SAVE_PROJECT,
        OPEN_SAVED_PROJECT,
        SAVE_COPY,
        OPEN_FILE,
        SHARE_DIAGNOSTICS,
        IMPORT_GLB,
        SETTINGS
    }

    /** One row: its group, its act, its semantic id and its label. */
    static final class Row {
        final Group group;
        final Action action;
        final int id;
        final int label;

        Row(Group group, Action action, int id, int label) {
            this.group = group;
            this.action = action;
            this.id = id;
            this.label = label;
        }
    }

    /**
     * Every row, in drawing order. The ids are the ones the old project
     * popover's rows already had (verification names a control by what it
     * does), plus {@code project_new_sketch} for the one act that is new to
     * this surface.
     */
    static final Row[] ROWS = {
        new Row(Group.CREATE, Action.NEW_SKETCH, R.id.project_new_sketch, R.string.new_sketch),
        new Row(Group.CREATE, Action.NEW_PROJECT, R.id.project_new, R.string.project_new),
        new Row(Group.PROJECT, Action.SAVE_PROJECT, R.id.project_save, R.string.project_save),
        new Row(Group.PROJECT, Action.OPEN_SAVED_PROJECT, R.id.project_open, R.string.project_open),
        new Row(Group.TRANSFER, Action.SAVE_COPY, R.id.project_save_copy,
                R.string.project_save_copy),
        new Row(Group.TRANSFER, Action.OPEN_FILE, R.id.project_open_file,
                R.string.project_open_file),
        new Row(Group.TRANSFER, Action.SHARE_DIAGNOSTICS, R.id.project_share_diagnostics,
                R.string.project_share_diagnostics),
        new Row(Group.IMPORT, Action.IMPORT_GLB, R.id.import_glb, R.string.import_glb),
        new Row(Group.APP, Action.SETTINGS, R.id.project_settings, R.string.project_settings),
    };

    /** A group's heading. */
    static int groupLabel(Group group) {
        switch (group) {
            case CREATE:
                return R.string.project_drawer_create;
            case PROJECT:
                return R.string.project;
            case TRANSFER:
                return R.string.project_transfer;
            case IMPORT:
                return R.string.import_section;
            default:
                return R.string.application_section;
        }
    }

    /**
     * Whether the ForgeShape mark is drawn. It is the door to a PROJECT's
     * drawer, so it stands whenever a project is open -- in Construction, in
     * Sculpt, while sketching, Ready, extruding or revolving, over Freeform
     * and Surface bodies -- and is withdrawn only in the CAD bootstrap, where
     * no project exists yet and Back to Home is the way out.
     */
    static boolean markShown(boolean projectOpen, boolean bootstrap) {
        return projectOpen && !bootstrap;
    }

    /**
     * Whether New Sketch is offered: exactly when Add Primitive's New Sketch
     * could succeed -- a project is open, nothing is sculpted, no sketch is
     * open and the support chooser is not already up. A control that cannot
     * succeed is not drawn; native refuses the begin all the same.
     */
    static boolean newSketchShown(boolean projectOpen, boolean sculpting, boolean sketching,
                                  boolean chooserActive) {
        return projectOpen && !sculpting && !sketching && !chooserActive;
    }

    /** The drawer grows out of the mark, at the window's LEADING edge. */
    static boolean anchoredToLeadingEdge() {
        return true;
    }

    /** Dismisses the drawer, before any act. */
    interface Closer {
        void closeDrawer();
    }

    /**
     * Performs a row's act: the drawer is closed, then the ONE existing
     * handler for that act is called.
     */
    static void perform(Action action, Closer closer, ProjectActionsPopoverView.OnProjectAction listener) {
        closer.closeDrawer();
        switch (action) {
            case NEW_SKETCH:
                listener.onNewSketchRequested();
                break;
            case NEW_PROJECT:
                listener.onNewProjectRequested();
                break;
            case SAVE_PROJECT:
                listener.onSaveProjectRequested();
                break;
            case OPEN_SAVED_PROJECT:
                listener.onOpenProjectRequested();
                break;
            case SAVE_COPY:
                listener.onSaveCopyRequested();
                break;
            case OPEN_FILE:
                listener.onOpenFileRequested();
                break;
            case SHARE_DIAGNOSTICS:
                listener.onShareDiagnosticsRequested();
                break;
            case IMPORT_GLB:
                listener.onImportGlbRequested();
                break;
            default:
                listener.onSettingsRequested();
                break;
        }
    }
}
