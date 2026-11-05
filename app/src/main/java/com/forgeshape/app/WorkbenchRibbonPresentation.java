package com.forgeshape.app;

/**
 * What the tablet's ribbon shows, as arithmetic on native facts
 * (`WORKBENCH-RIBBON-R2`).
 *
 * <p>The ribbon is the desktop's command row on a tablet workbench: tabs,
 * then captioned commands in named groups. Which groups and commands are
 * DRAWN is decided here, from a snapshot the workspace reads off native truth
 * on every refresh, so the rule "a control that cannot succeed is not drawn"
 * is one function that a JVM test can hold still rather than a scatter of
 * visibility calls. Nothing here is remembered, and nothing here is truth.
 *
 * <p>The desktop's own ribbon (`desktop_editor.cpp`, {@code buildModelRibbon})
 * is the reference: the same tabs, the same groups in the same order, the
 * same per-command conditions, so a user moving between the two shells
 * finds the same row.
 */
final class WorkbenchRibbonPresentation {

    private WorkbenchRibbonPresentation() {}

    /** The Model tab's index, the View tab's. The Sketch tab is the Tool Rail's on a tablet. */
    static final int TAB_MODEL = 0;
    static final int TAB_VIEW = 1;

    /** The face acts the ribbon's contextual group may carry, as bits. */
    static final int FACE_SKETCH_HERE = 1;
    static final int FACE_FINISH = 2;
    static final int FACE_HOLE = 4;
    static final int FACE_PATTERN = 8;

    /** The face act a command asks for. */
    static final int ACT_SKETCH_HERE = 0;
    static final int ACT_FILLET = 1;
    static final int ACT_CHAMFER = 2;
    static final int ACT_HOLE = 3;
    static final int ACT_PATTERN = 4;
    static final int ACT_MIRROR = 5;

    /** One snapshot of the facts the ribbon draws from; none is product truth. */
    static final class State {
        final boolean projectOpen;
        final boolean sketching;
        final boolean sculpting;
        /** Whether the active body is a primitive that can become a CAD part. */
        final boolean cadPartOffered;
        /** Whether the Transform tool is held and the gizmo is live. */
        final boolean transformOffered;
        final int transformMode;
        final boolean transformSpaceOffered;
        final int transformSpace;
        /** More than one body, so Delete can succeed. */
        final boolean deletable;
        /** The face acts offered on the touched face, {@code FACE_*} bits. */
        final int faceActs;
        final boolean gridVisible;
        final boolean outlineVisible;
        final boolean edgesVisible;

        State(boolean projectOpen, boolean sketching, boolean sculpting, boolean cadPartOffered,
              boolean transformOffered, int transformMode, boolean transformSpaceOffered,
              int transformSpace, boolean deletable, int faceActs, boolean gridVisible,
              boolean outlineVisible, boolean edgesVisible) {
            this.projectOpen = projectOpen;
            this.sketching = sketching;
            this.sculpting = sculpting;
            this.cadPartOffered = cadPartOffered;
            this.transformOffered = transformOffered;
            this.transformMode = transformMode;
            this.transformSpaceOffered = transformSpaceOffered;
            this.transformSpace = transformSpace;
            this.deletable = deletable;
            this.faceActs = faceActs;
            this.gridVisible = gridVisible;
            this.outlineVisible = outlineVisible;
            this.edgesVisible = edgesVisible;
        }
    }

    /**
     * Whether the ribbon is drawn at all: on a workbench, over a project,
     * outside a sketch and outside Sculpt. A sketch owns the viewport and its
     * tools live on the Tool Rail (the desktop's Sketch tab is that rail);
     * Sculpt's brushes do too, and the desktop has no Sculpt yet.
     */
    static boolean shown(boolean workbench, State state) {
        return workbench && state.projectOpen && !state.sketching && !state.sculpting;
    }

    /** The Part group: drawn only where the primitive can become a part. */
    static boolean partGroupShown(State state) {
        return state.cadPartOffered;
    }

    /** World / Local: only while a mode that has a space is held (Scale has none). */
    static boolean spaceShown(State state) {
        return state.transformOffered && state.transformSpaceOffered;
    }

    /** Delete: never the last body of a project. */
    static boolean deleteShown(State state) {
        return state.deletable;
    }

    /** The contextual Face group: drawn when the touched face offers anything. */
    static boolean faceGroupShown(State state) {
        return state.faceActs != 0;
    }

    static boolean faceActShown(State state, int act) {
        switch (act) {
            case ACT_SKETCH_HERE:
                return (state.faceActs & FACE_SKETCH_HERE) != 0;
            case ACT_FILLET:
            case ACT_CHAMFER:
                return (state.faceActs & FACE_FINISH) != 0;
            case ACT_HOLE:
                return (state.faceActs & FACE_HOLE) != 0;
            case ACT_PATTERN:
            case ACT_MIRROR:
                return (state.faceActs & FACE_PATTERN) != 0;
            default:
                return false;
        }
    }

    /** Which transform command is drawn held: the gizmo's mode, or none. */
    static boolean transformModeActive(State state, int mode) {
        return state.transformOffered && state.transformMode == mode;
    }

    static boolean transformSpaceActive(State state, int space) {
        return spaceShown(state) && state.transformSpace == space;
    }
}
