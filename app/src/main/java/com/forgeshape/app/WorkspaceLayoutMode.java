package com.forgeshape.app;

/**
 * The adaptive layout decision for the Editor Workspace, as a pure function of
 * the current <b>window</b> size.
 *
 * <p>Window size, never display size and never orientation: split-window,
 * free-form and a resized foldable are then the same rule as a rotation, and a
 * phone-landscape window and a small floating window get the same treatment
 * because they are the same problem.
 *
 * <p><b>Units are density-independent pixels throughout.</b> Nothing here takes
 * a physical pixel, so no layout decision can be accidentally calibrated to one
 * device's 1080x2400 panel — which is precisely how the previous shell came to
 * be verified in portrait only.
 *
 * <p>This class holds no Android type on purpose: the decision is arithmetic,
 * so it is unit-testable on the JVM without a device, and the numbers below can
 * be argued about in one place instead of being scattered through view code.
 */
enum WorkspaceLayoutMode {

    /** A phone in portrait, or any narrow window. */
    COMPACT,
    /** A large phone in landscape, a small tablet, or a half-screen split. */
    MEDIUM,
    /** A tablet: wide enough to dock a panel beside the model and tall enough
     *  that doing so is worth it. */
    EXPANDED;

    /** Below this window width the workspace is {@link #COMPACT}. */
    static final int MEDIUM_MIN_WIDTH_DP = 600;

    /** A window narrower than this can never be {@link #EXPANDED}. */
    static final int EXPANDED_MIN_WIDTH_DP = 840;

    /**
     * Below this window height a bottom sheet is not affordable.
     *
     * <p>This single threshold is what fixes the landscape failure. Phone
     * landscape measured 411 dp of height, and a bottom-anchored inspector in
     * that window either covers the model completely or pushes its own status
     * line off-screen — both of which happened. Under this height the inspector
     * moves to the side, where the cost is width, which a landscape window has
     * in abundance.
     *
     * <p>It also gates {@link #EXPANDED}: a 914 x 411 dp phone in landscape is
     * wide enough to look like a tablet by width alone, and it is not one.
     * Docking a permanent panel in a window that short would re-create the
     * defect in a new place.
     */
    static final int LOW_HEIGHT_MAX_DP = 480;

    /** Where the Property Inspector sits for a given window. */
    enum InspectorPlacement {
        /** Anchored to the bottom edge, overlaying the viewport. */
        BOTTOM_SHEET,
        /** Anchored to the trailing edge, overlaying the viewport. */
        SIDE_OVERLAY,
        /** Anchored to the trailing edge, laid out beside the model. */
        SIDE_DOCK
    }

    static WorkspaceLayoutMode forWindow(int widthDp, int heightDp) {
        if (widthDp >= EXPANDED_MIN_WIDTH_DP && heightDp >= LOW_HEIGHT_MAX_DP) {
            return EXPANDED;
        }
        if (widthDp >= MEDIUM_MIN_WIDTH_DP) {
            return MEDIUM;
        }
        return COMPACT;
    }

    /**
     * Chooses where the precision surface goes <b>when it is open</b>.
     *
     * <p>An expanded window docks it: there is enough width that a panel beside
     * the model costs the model nothing it needs. Otherwise the window's height
     * decides — a short window can only afford chrome at its sides.
     *
     * <p>None of the three is a resting state. Since the mobile workspace
     * redesign the surface is absent until the precision toggle asks for it, so
     * this answers where it appears rather than what the window permanently
     * gives up.
     */
    InspectorPlacement inspectorPlacement(int heightDp) {
        if (this == EXPANDED) {
            return InspectorPlacement.SIDE_DOCK;
        }
        return heightDp < LOW_HEIGHT_MAX_DP
                ? InspectorPlacement.SIDE_OVERLAY
                : InspectorPlacement.BOTTOM_SHEET;
    }

    /**
     * Whether the Tool Rail is laid out beside the viewport rather than over it.
     *
     * <p>Only an expanded window can pay for this. On a phone the rail overlays
     * the edge, which is what keeps the model full-bleed; the rail is narrow and
     * translucent precisely because it is standing on the picture.
     *
     * <p>"Docked" here means exactly what it already means for the Property
     * Inspector: the surface is drawn as <b>part of the layout</b> — flush,
     * with no elevation and no floating card — instead of as a raised panel
     * standing on the model. It is a claim about where the surface sits, and it
     * has to be true, which is why it is answered by the window and not by
     * taste. Note what it is <i>not</i>: it changes nothing about the
     * {@code SurfaceView}, which is full-bleed in every mode. See
     * {@link EditorWorkspaceView}.
     */
    boolean railDocked() {
        return this == EXPANDED;
    }

    // -----------------------------------------------------------------------
    // The Objects surface
    // -----------------------------------------------------------------------

    /**
     * How wide a dedicated Objects column is.
     *
     * <p>Fixed rather than proportional, because its content does not scale
     * with the window: a row reads "Body #7" and its `+` is one chip. A
     * proportional column would simply buy whitespace with viewport.
     */
    static final int OBJECTS_DOCK_WIDTH_DP = 180;

    /**
     * How wide the Tool Rail's column is once docked: its widest entry plus its
     * padding. Shared with {@link #objectsDocked} so the budget below is
     * computed from the same number the layout actually uses.
     */
    static final int RAIL_WIDTH_DP = 68;

    /**
     * The narrowest central viewport the shell will leave: as wide as a phone
     * screen.
     *
     * <p>This is the floor the expanded layout has always been held to — the
     * absolute half of it, kept, while the proportional half is deliberately
     * <b>not</b> applied to the three-column case. The reason is that the
     * proportional rule was written for two docked surfaces, and a scene list
     * is the third; 60 % of the window is unreachable at the bottom of the
     * expanded range once an inspector, a rail and an Objects column are all
     * subtracted, and pretending otherwise would either shrink the columns
     * below usefulness or quietly break the rule.
     */
    static final int MIN_CENTRAL_VIEWPORT_DP = 480;

    /**
     * Whether Objects gets a persistent surface of its own, beside the model.
     *
     * <p>Being {@link #EXPANDED} is necessary and not sufficient, and the
     * second condition is arithmetic rather than a fourth breakpoint: a window
     * earns the third column when it still leaves a central viewport at least
     * {@link #MIN_CENTRAL_VIEWPORT_DP} wide afterwards. Deriving it means a
     * later change to any column width moves this answer automatically instead
     * of silently violating the floor.
     *
     * <p>The bottom of the expanded range — 840 dp — therefore does not get
     * one, and that is the intended answer rather than a gap. A window that
     * wide is a large phone in landscape or a small tablet, and three permanent
     * chrome columns there is precisely the desktop-CAD clutter UI-OWNER-02
     * rules out. Objects stays where every window without a column keeps it: in
     * its own panel, one tap from the Objects capsule, standing on the model
     * only while it is open.
     */
    boolean objectsDocked(int widthDp) {
        if (this != EXPANDED) {
            return false;
        }
        final int remaining =
                widthDp - sideDockWidthDp(widthDp) - RAIL_WIDTH_DP - OBJECTS_DOCK_WIDTH_DP;
        return remaining >= MIN_CENTRAL_VIEWPORT_DP;
    }

    // -----------------------------------------------------------------------
    // Chrome sizing
    //
    // These exist as arithmetic rather than as fixed dimens because the chrome
    // budget is the thing this stage is accountable for. Every one of them is
    // both an absolute cap (so chrome never grows silly on a large window) and
    // a proportion (so chrome never eats a small one). The proportions are
    // chosen to keep the unoccluded viewport at or near 60 % of the window in
    // compact and medium windows even with the inspector fully open.
    // -----------------------------------------------------------------------

    /** Tallest a bottom-sheet inspector may be; its body scrolls beyond this. */
    static int bottomSheetMaxHeightDp(int windowHeightDp) {
        return clamp(Math.round(windowHeightDp * 0.30f), 160, 300);
    }

    /** Widest a side-overlay inspector may be; its body scrolls beyond this. */
    static int sideOverlayWidthDp(int windowWidthDp) {
        return clamp(Math.round(windowWidthDp * 0.33f), 240, 300);
    }

    /**
     * Widest a docked inspector may be.
     *
     * <p>30 % rather than 28 %, and capped at 340 rather than 320 dp, because
     * the panel has to <b>fit its own content</b> before it is allowed to be
     * narrow: at the old numbers a tablet's docked inspector gave the primitive
     * chooser about 85 dp a chip and clipped "Cylinder" to "Cyl" — a panel whose
     * controls cannot say their own names is worse than one that costs the model
     * a few more dp. The floor stays 260 dp, which is what keeps the 60 %
     * central-viewport rule true at the bottom of the expanded range.
     */
    static int sideDockWidthDp(int windowWidthDp) {
        return clamp(Math.round(windowWidthDp * 0.30f), 260, 340);
    }

    /**
     * The Global Toolbar's height.
     *
     * <p>A short window gets one row with the status message inline; a tall one
     * gets the message its own full-width line beneath the controls, where a
     * long rejection reason is readable. Either way the message is <b>always</b>
     * laid out inside the window — an off-screen status line was the reason
     * validation messages and the stale-source warning were unreachable in
     * landscape.
     */
    static boolean statusInlineWithControls(int heightDp) {
        return heightDp < LOW_HEIGHT_MAX_DP;
    }

    private static int clamp(int value, int min, int max) {
        return value < min ? min : (value > max ? max : value);
    }
}
