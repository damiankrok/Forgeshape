package com.forgeshape.app;

/**
 * What the Freeform cage context surface shows, as pure rules over one native
 * read ({@code MODELING-FOUNDATIONS-R1} B).
 *
 * <p>No Android type and no state of its own: the session and the cage are
 * native truth, read in ONE locked call ({@link NativeViewport#freeformState}),
 * and every answer here is a function of that read. It exists so the rule "a
 * control that cannot succeed is not drawn" is a JVM test rather than a
 * screenshot: Push/Pull, Extrude and Delete need a FACE selection, Insert Loop
 * exactly one EDGE, Crease an edge selection, and the transform modes a
 * selection of any kind.
 */
final class FreeformPresentation {

    private FreeformPresentation() {
    }

    /** One read of {@code freeformState}, named. */
    static final class State {
        final boolean active;
        final boolean activeIsFreeform;
        final boolean editable;
        final int element;
        final boolean multiSelect;
        final int transformMode;
        final int selectionCount;
        final int level;
        final int symmetry;
        final int vertices;
        final int edges;
        final int faces;
        final boolean open;
        final int lastStatus;

        State(double[] slots) {
            active = slots[NativeViewport.FREEFORM_STATE_ACTIVE] != 0.0;
            activeIsFreeform = slots[NativeViewport.FREEFORM_STATE_ACTIVE_IS_FREEFORM] != 0.0;
            editable = slots[NativeViewport.FREEFORM_STATE_EDITABLE] != 0.0;
            element = (int) slots[NativeViewport.FREEFORM_STATE_ELEMENT];
            multiSelect = slots[NativeViewport.FREEFORM_STATE_MULTI] != 0.0;
            transformMode = (int) slots[NativeViewport.FREEFORM_STATE_MODE];
            selectionCount = (int) slots[NativeViewport.FREEFORM_STATE_SELECTION_COUNT];
            level = (int) slots[NativeViewport.FREEFORM_STATE_LEVEL];
            symmetry = (int) slots[NativeViewport.FREEFORM_STATE_SYMMETRY];
            vertices = (int) slots[NativeViewport.FREEFORM_STATE_VERTICES];
            edges = (int) slots[NativeViewport.FREEFORM_STATE_EDGES];
            faces = (int) slots[NativeViewport.FREEFORM_STATE_FACES];
            open = slots[NativeViewport.FREEFORM_STATE_OPEN] != 0.0;
            lastStatus = (int) slots[NativeViewport.FREEFORM_STATE_LAST_STATUS];
        }
    }

    /** Whether cage editing should be open: a visible, unlocked Freeform body under Shape. */
    static boolean editWanted(boolean projectOpen, boolean sculpting, boolean sketching,
                              boolean shapeToolHeld, State state) {
        return projectOpen && !sculpting && !sketching && shapeToolHeld && state.activeIsFreeform
                && state.editable;
    }

    static boolean faceSelection(State s) {
        return s.active && s.element == NativeViewport.FREEFORM_ELEMENT_FACE && s.selectionCount > 0;
    }

    static boolean showPushPull(State s) {
        return faceSelection(s);
    }

    static boolean showExtrude(State s) {
        return faceSelection(s);
    }

    static boolean showDeleteFaces(State s) {
        // The last faces of a cage cannot all go; native refuses by name, and
        // the control stays because which faces remain is not known here.
        return faceSelection(s) && s.selectionCount < s.faces;
    }

    static boolean showInsertLoop(State s) {
        return s.active && s.element == NativeViewport.FREEFORM_ELEMENT_EDGE && s.selectionCount == 1;
    }

    static boolean showCrease(State s) {
        return s.active && s.element == NativeViewport.FREEFORM_ELEMENT_EDGE && s.selectionCount > 0;
    }

    static boolean showTransformModes(State s) {
        return s.active && s.selectionCount > 0;
    }

    static boolean symmetryOn(State s, int plane) {
        return (s.symmetry & plane) != 0;
    }

    /** The symmetry flags after toggling one plane. */
    static int toggledSymmetry(State s, int plane) {
        return s.symmetry ^ plane;
    }

    /** The user-facing reason for a refusal code, as a string resource. */
    static int refusalMessage(int code) {
        switch (code) {
            case NativeViewport.FREEFORM_EMPTY_SELECTION:
                return R.string.freeform_refused_selection;
            case 23:  // InvalidCrease
                return R.string.freeform_refused_crease;
            case 24:  // InvalidSubdivisionLevel
            case 25:  // SubdivisionBudgetExceeded
                return R.string.freeform_refused_level;
            case 27:  // CageNotSymmetric
                return R.string.freeform_refused_not_symmetric;
            case 29:  // SymmetryRequiresMidpoint
                return R.string.freeform_refused_midpoint;
            case 31:  // EdgeLoopSelfCrossing
                return R.string.freeform_refused_loop;
            case 32:  // InvalidRatio
                return R.string.freeform_refused_ratio;
            case 33:  // ExtrudeRegionPinched
                return R.string.freeform_refused_pinched;
            case 34:  // InvalidDistance
                return R.string.freeform_refused_distance;
            case 35:  // DeleteWouldEmpty
            case 36:  // DeleteWouldBreakManifold
                return R.string.freeform_refused_delete;
            case 42:  // BodyLocked
                return R.string.freeform_refused_locked;
            case NativeViewport.FREEFORM_REFUSED_IN_SCULPT:
            case NativeViewport.FREEFORM_REFUSED_IN_SKETCH:
                return R.string.freeform_refused_mode;
            default:
                return R.string.freeform_refused_other;
        }
    }
}
