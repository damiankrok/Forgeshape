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
     * The instance handed to the next workspace, when the one before it was
     * destroyed by a theme change rather than by the user leaving.
     *
     * <p>A theme is applied by recreating the Activity, which destroys the whole
     * view tree and the state below it. Everything that MATTERS survives anyway,
     * because it is native and process-scoped — the scene, every body, the
     * active ObjectId, the mode, the Frozen Sculpt Mesh. But the session state
     * in this class is neither native nor static, and losing it would mean that
     * changing colour also snapped the display unit back to meters, closed the
     * Property Inspector and re-pointed the Tool Rail. That is not a theme
     * change; that is a theme change plus a small unasked-for reset.
     *
     * <p>So the outgoing instance is carried across and adopted by the incoming
     * workspace. Deliberately NOT a {@code Bundle} and not persistence: this
     * class holds no Android type, which is what keeps it unit-testable without
     * a device, and the carried instance dies with the process exactly as the
     * fields in it always have.
     */
    private static EditorUiState carriedAcrossRecreation;

    /**
     * Hands this state to whichever workspace is built next.
     *
     * <p>Called immediately before an intentional recreation. It is not a
     * general "save": a destroy that is not a configuration change leaves this
     * null, so leaving the app really does start clean.
     */
    static void carryAcrossRecreation(EditorUiState state) {
        carriedAcrossRecreation = state;
    }

    /**
     * The state a newly built workspace should adopt.
     *
     * <p>Returns the carried instance exactly once, so a later workspace built
     * for any other reason gets a fresh one and cannot inherit a session that
     * was never handed to it.
     */
    static EditorUiState forNewWorkspace() {
        final EditorUiState carried = carriedAcrossRecreation;
        carriedAcrossRecreation = null;
        return carried != null ? carried : new EditorUiState();
    }

    /**
     * Which Construction context the Tool Rail is pointing at.
     *
     * <p>These are not native tools and native code has never heard of them.
     * Construction has exactly two kinds of exact value — what the object
     * <i>is</i> and where it <i>sits</i> — with separate Apply boundaries
     * because they have different consequences, and this says which of the two
     * the precision surface shows when it is opened. It is rail selection as
     * drawn, which the UI architecture assigns to the UI as layout state.
     *
     * <p>{@code TRANSFORM} rather than {@code PLACE} because that is the term
     * the rail, the precision surface and the documentation all use, and one
     * concept gets one name. It is deliberately <b>not</b> a claim that direct
     * handles exist: everything behind it today is exact numeric, and the
     * surface it opens says so in its own title.
     */
    static final int CONSTRUCTION_TOOL_SHAPE = 0;
    static final int CONSTRUCTION_TOOL_TRANSFORM = 1;

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
     * Whether the precision surface is open, remembered per mode for the
     * process lifetime.
     *
     * <p><b>Both start closed, and that is the whole mobile workspace rule.</b>
     * The resting workspace is a viewport with its tools around the edges; the
     * exact-value surface is something the user asks for from the tool context
     * that owns it and dismisses when the numbers are set. A panel that opened
     * itself would be back to owning the layout it was just taken out of.
     *
     * <p>Per mode rather than globally because the two modes open different
     * bodies and the answer to "did I leave this open" belongs to each.
     */
    private boolean constructionPrecisionOpen;
    private boolean sculptPrecisionOpen;

    /**
     * Whether the New Project chooser is open.
     *
     * <p>Presentation: which question is on screen, carried across the
     * recreation a theme change performs exactly as an open panel is. Whether
     * HOME is on screen is deliberately NOT here — it is derived from native
     * truth ({@code projectOpen()}) on every refresh, because a Java copy of
     * "is there a project" would be a second answer that could disagree with
     * the scene.
     */
    private boolean newProjectChooserOpen;

    /**
     * Whether this process has already settled what to do about unsaved work.
     *
     * <p>Process-scoped for exactly the same reason the start choice is, and the
     * reason is sharper here: an Activity recreation must not put a decision
     * about the user's work back in front of them. Rotating the device after
     * pressing Discard and being asked to Discard again would read as the app
     * not having believed them — and pressing Recover twice would try to replace
     * a project with a candidate that has already been consumed.
     *
     * <p>Still losable, still writes nothing to disk, and still cannot change
     * the model: a genuine process kill correctly asks again, because after a
     * process kill the question is genuinely unanswered. Whether there is a
     * candidate at all is decided by what is on disk, never by this.
     */
    private static boolean recoveryResolved;

    /**
     * Whether the Settings page is on screen (`UI-PREF-R1` A).
     *
     * <p>Presentation: which page is up, carried across the recreation a
     * palette change performs exactly as the New Project chooser is — the user
     * chose a palette FROM this page and must come back to it, in the new
     * palette, rather than to wherever they were before opening it. The
     * appearance itself is no longer remembered here: since `UI-PREF-R1` it is
     * one field of the persisted {@link AppPreferences}, read through
     * {@link AppPreferencesStore}, and this class holds no copy of it.
     */
    private boolean settingsOpen;

    /** Whether every chrome surface is hidden, leaving the bare model. */
    private boolean chromeHidden;

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
        constructionTool = (tool == CONSTRUCTION_TOOL_TRANSFORM)
                ? CONSTRUCTION_TOOL_TRANSFORM : CONSTRUCTION_TOOL_SHAPE;
    }

    boolean precisionOpen(boolean sculpting) {
        return sculpting ? sculptPrecisionOpen : constructionPrecisionOpen;
    }

    void setPrecisionOpen(boolean sculpting, boolean open) {
        if (sculpting) {
            sculptPrecisionOpen = open;
        } else {
            constructionPrecisionOpen = open;
        }
    }

    /** Whether the Settings page is on screen. */
    boolean settingsOpen() {
        return settingsOpen;
    }

    void setSettingsOpen(boolean open) {
        settingsOpen = open;
    }

    /** Whether the New Project chooser is on screen. */
    boolean newProjectChooserOpen() {
        return newProjectChooserOpen;
    }

    /**
     * Records that the New Project chooser is open or closed.
     *
     * <p>What is chosen is deliberately NOT remembered: both answers end in the
     * ordinary product, and after that the answer is visible in native state,
     * which is the authority.
     */
    void setNewProjectChooserOpen(boolean open) {
        newProjectChooserOpen = open;
    }

    /** Whether the recovery question has been settled in this process. */
    boolean recoveryResolved() {
        return recoveryResolved;
    }

    /**
     * Records that the recovery question is settled, however it was settled.
     *
     * <p>Recover, Discard and "the candidate turned out to be unreadable" all
     * land here. What the user chose is deliberately not remembered: afterwards
     * the answer is visible in the live project and on disk, which are the
     * authorities, and a copy here would be a second one.
     */
    void recordRecoveryResolved() {
        recoveryResolved = true;
    }

    /** Puts the recovery question back, as a fresh process would. Verification. */
    void clearRecoveryResolved() {
        recoveryResolved = false;
    }

    boolean chromeHidden() {
        return chromeHidden;
    }

    void setChromeHidden(boolean hidden) {
        chromeHidden = hidden;
    }
}
