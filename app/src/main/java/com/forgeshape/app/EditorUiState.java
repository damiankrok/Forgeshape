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
     * Whether this PROCESS has already chosen how it started.
     *
     * <p>Static on purpose, and it is the one field here that is not per
     * instance. The start chooser must not come back when the Activity is
     * recreated — a theme change, a locale change, a low-memory restart — and
     * an instance field would re-ask on every one of them, because a recreated
     * Activity builds a fresh {@link EditorWorkspaceView} and therefore a fresh
     * {@code EditorUiState}. Process-scoped is exactly the lifetime the
     * question has.
     *
     * <p>It is still <b>losable</b>, which is what keeps it inside this class's
     * contract: nothing is written to disk, no {@code Bundle} carries it, and a
     * genuine process kill correctly asks again. It also cannot change the
     * model — the chooser's two paths both run through the ordinary product
     * entry points, and this flag only decides whether the question is put.
     */
    private static boolean startChoiceMade;

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
     * Which appearance this PROCESS is wearing.
     *
     * <p>Static for the same reason the start flag is, and more sharply: a theme
     * change is applied by <b>recreating the Activity</b>, which is the only
     * clean way to re-resolve themed resources for a UI built entirely in code.
     * An instance field would therefore be destroyed by the very act that
     * applies it, and the recreated workspace would come back in the theme the
     * user just left.
     *
     * <p>Still <b>losable</b>, and deliberately so: nothing is written to disk
     * and no {@code Bundle} carries it, so a genuine process kill returns to the
     * documented default. Persistence is not part of this stage.
     *
     * <p>It is presentation. It cannot change a Construction parameter, a
     * transform, an {@code ObjectId}, a revision, what is pickable or what is
     * selected — all of which are native truth that outlives the Activity.
     */
    private static AppTheme appTheme = AppTheme.defaultTheme();

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

    /** The appearance in force, which a fresh process reports as the default. */
    AppTheme appTheme() {
        return appTheme;
    }

    /**
     * The appearance in force, reachable before any workspace exists.
     *
     * <p>Static because the Activity must apply the theme <i>before</i> it
     * inflates anything, and at that moment there is no
     * {@link EditorWorkspaceView} and therefore no instance to ask.
     */
    static AppTheme currentAppTheme() {
        return appTheme;
    }

    /**
     * Records the appearance the user chose.
     *
     * @return whether this actually changed anything, so a caller can avoid
     *         recreating the Activity for a tap on the theme that is already on
     *         screen — which would be a visible flash for no result
     */
    static boolean setCurrentAppTheme(AppTheme theme) {
        if (theme == null || theme == appTheme) {
            return false;
        }
        appTheme = theme;
        return true;
    }

    /**
     * Returns the process to the documented default, as a fresh one starts.
     *
     * <p>Exists so verification can assert the default and then exercise both
     * appearances inside one instrumentation process. It destroys nothing: the
     * scene, the mode and every body are native state this does not touch.
     */
    static void resetAppTheme() {
        appTheme = AppTheme.defaultTheme();
    }

    /** Whether the start chooser still has a question to ask this process. */
    boolean startChoiceMade() {
        return startChoiceMade;
    }

    /**
     * Records that the user answered the start chooser.
     *
     * <p>What they chose is deliberately NOT remembered. Both paths end in the
     * ordinary product — one in Construction, one in Sculpt on a frozen mesh —
     * and after that the answer is already visible in native state, which is
     * the authority. Keeping a copy here would be a second truth about which
     * mode the product is in.
     */
    void recordStartChoice() {
        startChoiceMade = true;
    }

    /**
     * Puts the question back, as a fresh process would.
     *
     * <p>Exists so verification can exercise both sides of the flag inside one
     * instrumentation process, where every test after the first would otherwise
     * see an answered chooser. It destroys nothing: the model, the mode and the
     * scene are native state and this does not touch any of them.
     */
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

    void clearStartChoice() {
        startChoiceMade = false;
    }

    boolean chromeHidden() {
        return chromeHidden;
    }

    void setChromeHidden(boolean hidden) {
        chromeHidden = hidden;
    }
}
