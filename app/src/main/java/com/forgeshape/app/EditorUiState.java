package com.forgeshape.app;

/**
 * Everything the Editor Workspace is allowed to remember for itself.
 *
 * <p>The list is deliberately short and deliberately closed, because the line
 * it draws is the one architectural rule the UI can break silently. Native code
 * owns the product mode, the active sculpt tool, the brush, the primitive kind,
 * every parameter, the transform, the object id and the Frozen Sculpt Mesh. The
 * UI owns <i>drafts, presentation and layout</i> — state that has no meaning
 * below JNI and that no geometry depends on.
 *
 * <p>Concretely, each field here is safe to lose. Kill the process and the
 * object is exactly what it was; only which panel was open and which unit was
 * on screen resets. If a field ever appears here that would change the model if
 * it were wrong, it belongs in native code instead.
 *
 * <p>Holds no Android type, so the rules below are unit-testable on the JVM.
 */
final class EditorUiState {

    /**
     * Which Construction editor the Tool Rail is pointing at.
     *
     * <p>These are not native tools and native code has never heard of them.
     * Construction has exactly two kinds of exact value — what the object
     * <i>is</i> and where it <i>sits</i> — with separate Apply boundaries
     * because they have different consequences, and this says which of the two
     * the Property Inspector is showing. It is rail selection as drawn, which
     * §13 of the UI architecture assigns to the UI as layout state.
     */
    static final int CONSTRUCTION_TOOL_SHAPE = 0;
    static final int CONSTRUCTION_TOOL_PLACE = 1;

    /**
     * The unit every length on screen is written in.
     *
     * <p>Presentation only: choosing one makes no native call, converts by an
     * exact decimal point shift, and cannot change the object. Process-scoped —
     * it survives home/resume with the Activity and resets to meters only when
     * the process restarts.
     */
    private LengthUnit displayUnit = LengthUnit.METERS;

    /**
     * The primitive whose fields are on screen.
     *
     * <p><b>Draft only.</b> Not the object's kind: it decides which fields are
     * visible and which parameters Apply Shape will submit. The object becomes
     * that kind when Apply Shape says so and native code agrees, and never
     * before. Reset from native truth on every refresh.
     */
    private int draftPrimitiveKind = NativeViewport.PRIMITIVE_BOX;

    private int constructionTool = CONSTRUCTION_TOOL_SHAPE;

    /**
     * Inspector expansion, remembered per mode for the process lifetime.
     *
     * <p>Per mode rather than globally because the two modes ask different
     * things of it: Construction's inspector is where the work happens, Sculpt's
     * is a status surface the user will usually want out of the way.
     */
    private boolean constructionInspectorExpanded = true;
    private boolean sculptInspectorExpanded = true;

    /** Whether every chrome surface is hidden, leaving the bare model. */
    private boolean chromeHidden;

    /** Set once, from the first layout, so a rotation does not re-open a panel
     *  the user deliberately collapsed. */
    private boolean inspectorDefaultsApplied;

    LengthUnit displayUnit() {
        return displayUnit;
    }

    void setDisplayUnit(LengthUnit unit) {
        displayUnit = unit;
    }

    int draftPrimitiveKind() {
        return draftPrimitiveKind;
    }

    /** Accepts only a kind the product actually has; anything else falls back
     *  to a box rather than leaving the selector pointing at nothing. */
    void setDraftPrimitiveKind(int kind) {
        draftPrimitiveKind = (kind >= NativeViewport.PRIMITIVE_BOX
                && kind <= NativeViewport.PRIMITIVE_PLANE) ? kind : NativeViewport.PRIMITIVE_BOX;
    }

    int constructionTool() {
        return constructionTool;
    }

    void setConstructionTool(int tool) {
        constructionTool = (tool == CONSTRUCTION_TOOL_PLACE)
                ? CONSTRUCTION_TOOL_PLACE : CONSTRUCTION_TOOL_SHAPE;
    }

    boolean inspectorExpanded(boolean sculpting) {
        return sculpting ? sculptInspectorExpanded : constructionInspectorExpanded;
    }

    void setInspectorExpanded(boolean sculpting, boolean expanded) {
        if (sculpting) {
            sculptInspectorExpanded = expanded;
        } else {
            constructionInspectorExpanded = expanded;
        }
    }

    boolean chromeHidden() {
        return chromeHidden;
    }

    void setChromeHidden(boolean hidden) {
        chromeHidden = hidden;
    }

    /**
     * Applies the layout mode's opening detent, once.
     *
     * <p>A compact window opens with the inspector collapsed so the model is
     * the first thing on screen; anything wider opens it. After that the user's
     * choice stands, including across a rotation — re-opening a panel someone
     * just collapsed because the window changed shape is the kind of helpfulness
     * that reads as a bug.
     */
    void applyInitialDetents(WorkspaceLayoutMode mode, int windowHeightDp) {
        if (inspectorDefaultsApplied) {
            return;
        }
        inspectorDefaultsApplied = true;
        final boolean expanded = mode.inspectorStartsExpanded(windowHeightDp);
        constructionInspectorExpanded = expanded;
        sculptInspectorExpanded = expanded;
    }
}
