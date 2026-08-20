package com.forgeshape.app;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * The whole ForgeShape editor UI.
 *
 * <p>One shell, three regions — the Global Toolbar, the edge surfaces (Tool
 * Rail, and in Sculpt the direct brush controls), and the Property Inspector.
 * Mode and tool decide what those regions <i>contain</i>; they never decide
 * what regions there are. That is what makes the four sculpt brushes and a
 * numeric CAD inspector the same application rather than two.
 *
 * <p><b>The Vulkan viewport is full-bleed and stays that way.</b> The
 * {@code SurfaceView} is the first child, sized to the whole window, and no
 * layout decision in this class ever insets, pads or resizes it — window insets
 * are applied to the chrome container only. Nothing here can therefore cause a
 * swapchain rebuild, and the renderer's ownership of the surface is untouched.
 *
 * <p><b>It owns no product state.</b> Which mode is active, which sculpt tool
 * is held, what the brush is, what the object is and where it sits are all
 * native truth, read back after every request. What this class owns is in
 * {@link EditorUiState}: drafts, presentation and layout.
 *
 * <p><b>Every chrome surface consumes its own gestures.</b> The toolbar, the
 * rail, the brush sliders and the inspector all return true from
 * {@code onTouchEvent}, so a touch that lands on chrome is never delivered to
 * the {@code SurfaceView} beneath it — which in Construction would orbit the
 * camera and in Sculpt would deform the model.
 */
final class EditorWorkspaceView extends FrameLayout
        implements InspectorHost, GlobalToolbarView.OnGlobalAction,
        ToolRailView.OnToolSelected, PropertyInspectorView.OnExpandedChanged,
        BrushEdgeControlsView.OnBrushChanged,
        DisplaySettingsPopoverView.OnDisplaySettingChanged {

    private static final int[] SCULPT_TOOL_HINTS = {
            R.string.hint_grab, R.string.hint_clay, R.string.hint_smooth, R.string.hint_inflate
    };
    private static final int[] SCULPT_TOOL_NAMES = {
            R.string.tool_grab, R.string.tool_clay, R.string.tool_smooth, R.string.tool_inflate
    };

    private final EditorUiState uiState = new EditorUiState();

    private final View viewport;
    private final LinearLayout chromeRoot;
    private final LinearLayout middleRow;
    private final FrameLayout overlayRoot;

    private final GlobalToolbarView toolbar;
    private final ToolRailView toolRail;
    private final ScrollView toolRailScroll;
    private final BrushEdgeControlsView brushControls;
    private final PropertyInspectorView inspector;
    private final TextView restoreChip;
    private final DisplaySettingsPopoverView displayPopover;

    private final ConstructionShapeEditorView shapeEditor;
    private final ConstructionPlacementEditorView placementEditor;
    private final SculptContextView sculptContext;

    /** Reused across reads; native fills it with the authoritative state. */
    private final double[] nativeSculpt = new double[NativeViewport.SCULPT_STATE_SIZE];

    private WorkspaceLayoutMode layoutMode = WorkspaceLayoutMode.COMPACT;
    private WorkspaceLayoutMode.InspectorPlacement inspectorPlacement =
            WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET;

    /** Which entry set the rail is currently built from, so it is rebuilt on a
     *  mode change and not on every refresh. */
    private boolean railShowsSculptTools;

    /** The window the current arrangement was computed for, so the decision
     *  runs once per size rather than once per measure pass. */
    private int appliedWidthPx;
    private int appliedHeightPx;

    /** What the inspector is currently placed as, so it is re-parented only
     *  when the answer actually changed. */
    private WorkspaceLayoutMode.InspectorPlacement appliedPlacement;
    private int appliedInspectorWidthPx;

    EditorWorkspaceView(Context context, View viewport) {
        super(context);
        this.viewport = viewport;
        setId(R.id.editor_workspace);

        // Child 0: the viewport, at the whole window size, under everything.
        addView(viewport, new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));

        // Child 1: all interactive chrome. This container is transparent and
        // not clickable, so a touch in the gaps between chrome surfaces falls
        // through to the viewport exactly as it should; only the surfaces
        // themselves are opaque to touch.
        chromeRoot = new LinearLayout(context);
        chromeRoot.setOrientation(LinearLayout.VERTICAL);
        addView(chromeRoot, new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));

        toolbar = new GlobalToolbarView(context, this);
        chromeRoot.addView(toolbar, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        middleRow = new LinearLayout(context);
        middleRow.setOrientation(LinearLayout.HORIZONTAL);
        chromeRoot.addView(middleRow, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f));

        brushControls = new BrushEdgeControlsView(context, this);
        final LinearLayout.LayoutParams brushParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        brushParams.gravity = Gravity.CENTER_VERTICAL;
        brushParams.leftMargin = EditorControlStyles.dimen(context, R.dimen.brush_gap);
        middleRow.addView(brushControls, brushParams);

        // The empty middle is where the model lives. It is a weighted gap with
        // no background and no listener, so it costs the viewport nothing.
        middleRow.addView(EditorControlStyles.spacer(context));

        toolRail = new ToolRailView(context, this);
        // Scrolled rather than clipped: a window too short for every entry must
        // still be able to reach every entry. Dropping a tool in landscape
        // would be the same class of defect this stage exists to fix.
        toolRailScroll = new ScrollView(context);
        toolRailScroll.addView(toolRail, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        final LinearLayout.LayoutParams railParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        railParams.gravity = Gravity.CENTER_VERTICAL;
        railParams.rightMargin = EditorControlStyles.dimen(context, R.dimen.brush_gap);
        middleRow.addView(toolRailScroll, railParams);

        inspector = new PropertyInspectorView(context, this);

        // Child 2: surfaces that must survive the chrome being hidden.
        overlayRoot = new FrameLayout(context);
        addView(overlayRoot, new LayoutParams(LayoutParams.MATCH_PARENT,
                LayoutParams.MATCH_PARENT));
        restoreChip = EditorControlStyles.chip(context, R.id.restore_ui_chip, "⊞");
        restoreChip.setContentDescription(context.getString(R.string.show_ui));
        restoreChip.setVisibility(GONE);
        restoreChip.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                setChromeHidden(false);
            }
        });
        final LayoutParams restoreParams = new LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        restoreParams.gravity = Gravity.TOP | Gravity.END;
        restoreParams.topMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        restoreParams.rightMargin = EditorControlStyles.dimen(context, R.dimen.row_gap);
        overlayRoot.addView(restoreChip, restoreParams);

        // The display popover lives in the overlay rather than in the toolbar's
        // own row for one reason: the toolbar is a fixed-height strip inside a
        // vertical LinearLayout, so a panel added there would either be clipped
        // or would push the middle row down and resize the chrome. In the
        // overlay it simply hangs under the toolbar, and the adaptive layout
        // arithmetic in WorkspaceLayoutMode does not have to learn about it.
        displayPopover = new DisplaySettingsPopoverView(context, this, isDebuggableBuild(context));
        overlayRoot.addView(displayPopover, DisplaySettingsPopoverView.anchoredParams(context,
                EditorControlStyles.dimen(context, R.dimen.toolbar_height)));

        shapeEditor = new ConstructionShapeEditorView(context, this);
        placementEditor = new ConstructionPlacementEditorView(context, this);
        sculptContext = new SculptContextView(context, this);

        installInsetListener();
        syncFromNative();
    }

    // -----------------------------------------------------------------------
    // Window insets
    // -----------------------------------------------------------------------

    /**
     * Keeps every interactive control clear of the system bars, a display
     * cutout and the soft keyboard — and keeps the viewport clear of all three.
     *
     * <p>The padding goes on the chrome container, never on this view and never
     * on the {@code SurfaceView}. Padding the root would inset the Vulkan
     * surface, change the render target's size and rebuild the swapchain, for a
     * problem that is entirely about where buttons are drawn.
     */
    private void installInsetListener() {
        setOnApplyWindowInsetsListener(new OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                final Rect padding = chromeInsets(insets);
                chromeRoot.setPadding(padding.left, padding.top, padding.right, padding.bottom);
                overlayRoot.setPadding(padding.left, padding.top, padding.right, padding.bottom);
                // Returned unconsumed: this view has decided what chrome does
                // about them, and the viewport deliberately ignores them.
                return insets;
            }
        });
    }

    private Rect chromeInsets(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            final android.graphics.Insets bars = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            final android.graphics.Insets ime = insets.getInsets(WindowInsets.Type.ime());
            // The keyboard replaces the navigation bar rather than adding to
            // it: they occupy the same edge, and adding both would leave a
            // visible dead band above the keys.
            return new Rect(bars.left, bars.top, bars.right, Math.max(bars.bottom, ime.bottom));
        }
        // minSdk is 26, so the deprecated accessors are still the only ones
        // available on the oldest supported release. They report exactly the
        // same edges for the case that matters here.
        return new Rect(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
    }

    // -----------------------------------------------------------------------
    // Adaptive layout
    // -----------------------------------------------------------------------

    /**
     * Runs the adaptive decision <b>before</b> the children are measured.
     *
     * <p>Deliberately not in {@code onSizeChanged}: that runs during the layout
     * pass, and a surface added or re-parented there is measured against the
     * previous pass's numbers and laid out at zero height. Doing it at the top
     * of measure means whatever the decision changes is part of the same
     * traversal. It is idempotent, so the second measure pass of a traversal
     * changes nothing and it converges immediately.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        applyLayoutForWindow(MeasureSpec.getSize(widthMeasureSpec),
                MeasureSpec.getSize(heightMeasureSpec));
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // The Activity absorbs configuration changes so a rotation does not
        // destroy and recreate the Vulkan surface. That makes re-running the
        // layout decision here mandatory rather than optional: nothing else
        // will do it. A density change can alter the decision without altering
        // the pixel size, so the cached window is discarded rather than
        // compared.
        appliedWidthPx = 0;
        appliedHeightPx = 0;
        requestLayout();
    }

    /**
     * Re-runs the whole adaptive decision for a window of this size.
     *
     * <p>Driven by the window, not the display and not the orientation, so a
     * split-window resize, a free-form drag and a rotation all take the same
     * path.
     */
    void applyLayoutForWindow(int widthPx, int heightPx) {
        if (widthPx <= 0 || heightPx <= 0
                || (widthPx == appliedWidthPx && heightPx == appliedHeightPx)) {
            return;
        }
        appliedWidthPx = widthPx;
        appliedHeightPx = heightPx;
        final Context context = getContext();
        final int widthDp = EditorControlStyles.toDp(context, widthPx);
        final int heightDp = EditorControlStyles.toDp(context, heightPx);

        layoutMode = WorkspaceLayoutMode.forWindow(widthDp, heightDp);
        uiState.applyInitialDetents(layoutMode, heightDp);

        toolbar.setStatusInline(WorkspaceLayoutMode.statusInlineWithControls(heightDp));
        toolRail.setCompactEntries(heightDp < WorkspaceLayoutMode.LOW_HEIGHT_MAX_DP);
        // Roughly half the window's height for the two brush tracks, bounded by
        // the control's own sensible range, so they shrink with the window
        // instead of being clipped by it.
        brushControls.setTrackHeightPx(Math.round(heightPx * 0.45f));

        placeInspector(layoutMode.inspectorPlacement(heightDp), widthDp, heightDp);
        showInspectorDetent();
    }

    /**
     * Moves the Property Inspector between the bottom of the workspace and its
     * trailing edge.
     *
     * <p>A short window never gets a bottom sheet. That is the whole fix for
     * the landscape failure: at the bottom the panel measured to the full
     * window and pushed its own status line past the window edge, and at the
     * side the same content costs width, which a landscape window has.
     */
    private void placeInspector(WorkspaceLayoutMode.InspectorPlacement placement,
                                int widthDp, int heightDp) {
        final Context context = getContext();
        final boolean bottom =
                placement == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET;
        final int widthPx = bottom ? 0
                : dpToPx(placement == WorkspaceLayoutMode.InspectorPlacement.SIDE_DOCK
                        ? WorkspaceLayoutMode.sideDockWidthDp(widthDp)
                        : WorkspaceLayoutMode.sideOverlayWidthDp(widthDp));

        inspectorPlacement = placement;
        // A bottom sheet must be capped or it measures to whatever its content
        // wants, which is exactly how the previous panel came to fill the
        // window. A side placement is already bounded by the window's height,
        // so the cap is off and the body simply scrolls.
        inspector.setMaxHeightPx(bottom
                ? dpToPx(WorkspaceLayoutMode.bottomSheetMaxHeightDp(heightDp)) : 0);

        if (placement == appliedPlacement && widthPx == appliedInspectorWidthPx
                && inspector.getParent() != null) {
            return;
        }
        appliedPlacement = placement;
        appliedInspectorWidthPx = widthPx;

        final ViewGroup parent = (ViewGroup) inspector.getParent();
        if (parent != null) {
            parent.removeView(inspector);
        }
        if (bottom) {
            chromeRoot.addView(inspector, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            return;
        }
        final LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                widthPx, ViewGroup.LayoutParams.MATCH_PARENT);
        params.leftMargin = EditorControlStyles.dimen(context, R.dimen.row_gap_small);
        middleRow.addView(inspector, params);
        applyInspectorSideWidth();
    }

    /**
     * Narrows a side-placed inspector to its toggle when it is collapsed.
     *
     * <p>A bottom sheet gives back height by hiding its body; a side panel has
     * to give back width, or collapsing it buys the model nothing at all.
     */
    private void applyInspectorSideWidth() {
        if (inspectorPlacement == WorkspaceLayoutMode.InspectorPlacement.BOTTOM_SHEET) {
            return;
        }
        final ViewGroup.LayoutParams params = inspector.getLayoutParams();
        if (params == null) {
            return;
        }
        final int wanted = inspector.isExpanded() ? appliedInspectorWidthPx
                : dpToPx(WorkspaceLayoutMode.SIDE_COLLAPSED_WIDTH_DP);
        if (params.width != wanted) {
            params.width = wanted;
            inspector.setLayoutParams(params);
        }
    }

    private int dpToPx(int dp) {
        return Math.round(getResources().getDisplayMetrics().density * dp);
    }

    WorkspaceLayoutMode layoutMode() {
        return layoutMode;
    }

    WorkspaceLayoutMode.InspectorPlacement inspectorPlacement() {
        return inspectorPlacement;
    }

    // -----------------------------------------------------------------------
    // Chrome visibility
    // -----------------------------------------------------------------------

    /**
     * Collapses every chrome surface to a single restore chip, leaving the bare
     * model.
     *
     * <p>Nothing about the gesture model changes when chrome is hidden: the
     * viewport was always the whole window, so it simply stops having anything
     * standing on it.
     */
    void setChromeHidden(boolean hidden) {
        uiState.setChromeHidden(hidden);
        chromeRoot.setVisibility(hidden ? GONE : VISIBLE);
        restoreChip.setVisibility(hidden ? VISIBLE : GONE);
        toolbar.showChromeHidden(hidden);
        if (hidden) {
            // Hiding chrome means "show me the bare model". The popover lives in
            // the overlay, so it would otherwise survive the very act that was
            // meant to clear the viewport. Closed without animation: the chrome
            // around it is disappearing in the same frame, so animating this one
            // panel out would only draw attention to it.
            displayPopover.closeImmediately();
            toolbar.showDisplaySettingsOpen(false);
        }
    }

    boolean chromeHidden() {
        return uiState.chromeHidden();
    }

    // -----------------------------------------------------------------------
    // Reading native truth
    // -----------------------------------------------------------------------

    /**
     * Rebuilds every surface from authoritative native state.
     *
     * <p>The mode is <b>read back</b> rather than assumed, so a refused request
     * — entering Sculpt with nothing frozen, for instance — leaves the correct
     * surfaces on screen instead of ones that lie about what is being edited.
     * The same is true of the active sculpt tool and the brush.
     *
     * <p>This also rewrites the exact-value editors, which discards any
     * half-typed draft. That is deliberate and is why it is called only after
     * something below JNI actually changed, or on a resume — never for a
     * presentation-only action such as switching the display unit or the Tool
     * Rail entry.
     */
    void syncFromNative() {
        NativeViewport.sculptState(nativeSculpt);
        final boolean sculpting =
                NativeViewport.productMode() == NativeViewport.MODE_SCULPT;
        final boolean hasFrozenMesh = nativeSculpt[NativeViewport.SCULPT_HAS_MESH] != 0.0;

        toolbar.showContext(sculpting, hasFrozenMesh);
        buildRailFor(sculpting);
        // Display settings are native-owned and process-scoped, so on a resume
        // they are already whatever they were; this only makes the popover's
        // chips agree with them.
        refreshDisplaySettings();

        if (sculpting) {
            brushControls.setVisibility(VISIBLE);
            brushControls.refreshFromNative();
            sculptContext.refreshFromNative();
            toolRail.showActive((int) nativeSculpt[NativeViewport.SCULPT_TOOL]);
        } else {
            // In Construction the brush controls are not merely disabled but
            // absent: there is no brush to set, and an inert slider standing on
            // the model would be pure occlusion.
            brushControls.setVisibility(GONE);
            shapeEditor.refreshFromNative();
            placementEditor.refreshFromNative();
            toolRail.showActive(uiState.constructionTool());
        }
        showActiveInspectorBody(sculpting);
        showInspectorDetent();
        showDefaultStatus(sculpting);
    }

    /**
     * Writes what is currently being edited into the status line.
     *
     * <p>Always written on a re-read, so the line never sits empty and never
     * keeps describing text that is no longer in the fields. Callers with
     * something more specific to say — an applied value, a rejection reason —
     * overwrite it immediately afterwards.
     */
    private void showDefaultStatus(boolean sculpting) {
        final Context context = getContext();
        if (!sculpting) {
            showStatus(context.getString(R.string.status_showing,
                    shapeEditor.describeNativeKind(), uiState.displayUnit().label()),
                    R.color.text_secondary);
            return;
        }
        final int tool = (int) nativeSculpt[NativeViewport.SCULPT_TOOL];
        final int index = (tool >= 0 && tool < SCULPT_TOOL_HINTS.length)
                ? tool : NativeViewport.TOOL_GRAB;
        showStatus(context.getString(R.string.sculpt_gesture_rule,
                context.getString(SCULPT_TOOL_HINTS[index])), R.color.text_secondary);
    }

    private void buildRailFor(boolean sculpting) {
        if (railShowsSculptTools == sculpting && toolRail.getChildCount() > 0) {
            return;
        }
        railShowsSculptTools = sculpting;
        final Context context = getContext();
        if (sculpting) {
            toolRail.setEntries(new ToolRailView.Entry[]{
                    new ToolRailView.Entry(R.id.tool_rail_grab, "◈",
                            context.getString(R.string.tool_grab),
                            NativeViewport.TOOL_GRAB, false),
                    new ToolRailView.Entry(R.id.tool_rail_clay, "●",
                            context.getString(R.string.tool_clay),
                            NativeViewport.TOOL_CLAY, false),
                    new ToolRailView.Entry(R.id.tool_rail_smooth, "≈",
                            context.getString(R.string.tool_smooth),
                            NativeViewport.TOOL_SMOOTH, false),
                    new ToolRailView.Entry(R.id.tool_rail_inflate, "◎",
                            context.getString(R.string.tool_inflate),
                            NativeViewport.TOOL_INFLATE, false),
            });
        } else {
            // Sketch and Extrude are drawn from day one and are inert. Their
            // presence is the point: when they arrive, the shell's content
            // changes and its shape does not.
            toolRail.setEntries(new ToolRailView.Entry[]{
                    new ToolRailView.Entry(R.id.tool_rail_shape, "▣",
                            context.getString(R.string.tool_shape),
                            EditorUiState.CONSTRUCTION_TOOL_SHAPE, false),
                    new ToolRailView.Entry(R.id.tool_rail_place, "⊕",
                            context.getString(R.string.tool_place),
                            EditorUiState.CONSTRUCTION_TOOL_PLACE, false),
                    new ToolRailView.Entry(R.id.tool_rail_sketch, "▱",
                            context.getString(R.string.tool_sketch), -1, true),
                    new ToolRailView.Entry(R.id.tool_rail_extrude, "▤",
                            context.getString(R.string.tool_extrude), -1, true),
            });
        }
    }

    /**
     * Puts the body belonging to the active mode and Tool Rail entry into the
     * inspector.
     *
     * <p>Deliberately does not re-read native state: switching rail entry is a
     * presentation act, and a refresh here would throw away a number the user
     * had half typed in the other section.
     */
    private void showActiveInspectorBody(boolean sculpting) {
        final Context context = getContext();
        if (sculpting) {
            inspector.setBody(sculptContext, context.getString(R.string.inspector_sculpt_title));
        } else if (uiState.constructionTool() == EditorUiState.CONSTRUCTION_TOOL_PLACE) {
            inspector.setBody(placementEditor,
                    context.getString(R.string.inspector_place_title));
        } else {
            inspector.setBody(shapeEditor, context.getString(R.string.inspector_shape_title));
        }
    }

    private void showInspectorDetent() {
        inspector.showExpanded(uiState.inspectorExpanded(isSculpting()));
        applyInspectorSideWidth();
    }

    private boolean isSculpting() {
        return NativeViewport.productMode() == NativeViewport.MODE_SCULPT;
    }

    // -----------------------------------------------------------------------
    // Tool Rail
    // -----------------------------------------------------------------------

    @Override
    public void onToolSelected(int key) {
        if (isSculpting()) {
            // Ask, then read back: the rail draws the tool native code reports,
            // not the one that was tapped. Changing tool touches no geometry --
            // it publishes no revision, uploads nothing, does not re-freeze and
            // leaves Radius and Strength exactly as they were, because both are
            // shared by every tool.
            NativeViewport.setSculptTool(key);
            final int active = NativeViewport.sculptTool();
            toolRail.showActive(active);
            final int index = (active >= 0 && active < SCULPT_TOOL_NAMES.length)
                    ? active : NativeViewport.TOOL_GRAB;
            showStatus(getContext().getString(R.string.status_tool_selected,
                    getContext().getString(SCULPT_TOOL_NAMES[index]),
                    getContext().getString(SCULPT_TOOL_HINTS[index])), R.color.text_secondary);
            finishEditing();
            return;
        }
        // In Construction the rail chooses which exact-value editor is on
        // screen. Native code has no such concept, makes no call here, and
        // nothing about the object changes.
        uiState.setConstructionTool(key);
        toolRail.showActive(uiState.constructionTool());
        showActiveInspectorBody(false);
        showStatus(getContext().getString(R.string.status_construction_hint),
                R.color.text_secondary);
    }

    @Override
    public void onBrushChanged() {
        showStatus(getContext().getString(R.string.status_brush, brushControls.describeRadius(),
                brushControls.describeStrength()), R.color.text_secondary);
    }

    @Override
    public void onInspectorExpandedChanged(boolean expanded) {
        uiState.setInspectorExpanded(isSculpting(), expanded);
        applyInspectorSideWidth();
    }

    // -----------------------------------------------------------------------
    // Global actions
    // -----------------------------------------------------------------------

    /**
     * Copies the object's current Construction mesh into a Frozen Sculpt Mesh
     * and enters Sculpt Mode.
     *
     * <p>Unguarded on purpose. This control is on screen only while <b>no</b>
     * frozen mesh exists, so it can discard nothing: there is no sculpt work to
     * lose. The guarded path is the re-Freeze in the Sculpt inspector, which is
     * the one that replaces an edited mesh.
     *
     * <p>It changes no dimension, no primitive kind and no placement — the
     * Construction Source is only read, and it is still here, unchanged, when
     * Sculpt Mode is left.
     */
    @Override
    public void onFreezeToSculpt() {
        if (NativeViewport.freezeToSculpt() != NativeViewport.SCULPT_OK) {
            showStatus(getContext().getString(R.string.status_freeze_failed), R.color.text_error);
            return;
        }
        finishEditing();
        syncFromNative();
        showStatus(getContext().getString(R.string.status_frozen,
                shapeEditor.describeNativeKind()), R.color.text_success);
    }

    /**
     * Returns to the Frozen Sculpt Mesh exactly as it was left.
     *
     * <p>Never confirmed, because it destroys nothing: nothing that has been
     * sculpted is lost by having looked at the Construction Source. Guarding it
     * would train the user to dismiss the guard that matters.
     */
    @Override
    public void onResumeSculpt() {
        if (NativeViewport.enterSculptMode() != NativeViewport.SCULPT_OK) {
            showStatus(getContext().getString(R.string.status_nothing_frozen),
                    R.color.text_error);
            return;
        }
        finishEditing();
        syncFromNative();
    }

    @Override
    public void onBackToConstruction() {
        NativeViewport.enterConstructionMode();
        finishEditing();
        syncFromNative();
    }

    @Override
    public void onChromeHideRequested() {
        setChromeHidden(!uiState.chromeHidden());
    }

    // -----------------------------------------------------------------------
    // Display settings (presentation only)
    // -----------------------------------------------------------------------
    //
    // None of this publishes a mesh revision, changes a Construction parameter,
    // moves a sculpt vertex or affects what is pickable, so — unlike a mode or
    // tool change — none of it calls syncFromNative(). Doing so would rewrite
    // the exact-value editors and discard a half-typed dimension, which is
    // exactly the kind of surprise a display control must never cause.

    @Override
    public void onDisplaySettingsRequested() {
        final boolean opening = !displayPopover.isOpen();
        if (opening) {
            refreshDisplaySettings();
            // Hang the popover below the toolbar's ACTUAL height, not a nominal
            // one. The toolbar grows a second line when the status message
            // cannot share the control row, and a fixed offset would then put
            // the popover on top of the very message a rejected Apply writes.
            final ViewGroup.MarginLayoutParams params =
                    (ViewGroup.MarginLayoutParams) displayPopover.getLayoutParams();
            final int toolbarHeight = toolbar.getHeight();
            if (toolbarHeight > 0 && params.topMargin != toolbarHeight) {
                params.topMargin = toolbarHeight;
                displayPopover.setLayoutParams(params);
            }
        }
        displayPopover.setOpen(opening);
        toolbar.showDisplaySettingsOpen(opening);
    }

    @Override
    public void onShadingModelRequested(int model) {
        // The popover deliberately stays OPEN. Comparing Studio against MatCap
        // means switching back and forth, and a panel that dismissed itself on
        // every choice would make that four gestures instead of two.
        NativeViewport.setShadingModel(model);
        refreshDisplaySettings();
    }

    @Override
    public void onSurfaceShadingRequested(int shading) {
        NativeViewport.setSurfaceShading(shading);
        refreshDisplaySettings();
    }

    /** Repaints the popover from native truth, so a refused request shows. */
    private void refreshDisplaySettings() {
        displayPopover.showSettings(NativeViewport.shadingModel(),
                NativeViewport.surfaceShading());
    }

    /**
     * Whether this build is debuggable, which is what gates the debug
     * source-colour shading chip.
     *
     * <p>Read from the application info rather than from {@code BuildConfig}:
     * the project does not generate a {@code BuildConfig} class, and enabling
     * one for a single boolean would add a build feature to the product for a
     * diagnostic's benefit.
     */
    private static boolean isDebuggableBuild(Context context) {
        return (context.getApplicationInfo().flags
                & android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) != 0;
    }

    // -----------------------------------------------------------------------
    // InspectorHost
    // -----------------------------------------------------------------------

    @Override
    public void showStatus(CharSequence message, int colorRes) {
        toolbar.showStatus(message, colorRes);
    }

    @Override
    public void onNativeStateChanged() {
        syncFromNative();
    }

    /**
     * Re-expresses every length in the workspace in another unit.
     *
     * <p>Presentation only: <b>no native call is made</b>, so the authoritative
     * parameters, the authoritative transform, the mesh revision and the GPU
     * upload count are all untouched. Both Construction editors convert,
     * including the one that is off screen, so a draft does not silently change
     * meaning while it is away.
     */
    @Override
    public void onDisplayUnitRequested(LengthUnit unit) {
        final LengthUnit previous = uiState.displayUnit();
        if (unit == previous) {
            return;
        }
        uiState.setDisplayUnit(unit);
        boolean allConverted = shapeEditor.convertDisplayUnit(previous, unit);
        allConverted &= placementEditor.convertDisplayUnit(previous, unit);
        showStatus(getContext().getString(allConverted ? R.string.status_unit_display_only
                        : R.string.status_unit_unparsed, unit.label()),
                allConverted ? R.color.text_secondary : R.color.text_error);
    }

    @Override
    public void finishEditing() {
        shapeEditor.clearEditFocus();
        placementEditor.clearEditFocus();
        final InputMethodManager ime =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (ime != null) {
            ime.hideSoftInputFromWindow(getWindowToken(), 0);
        }
        if (viewport != null) {
            viewport.requestFocus();
        }
    }

    @Override
    public EditorUiState uiState() {
        return uiState;
    }

    // -----------------------------------------------------------------------
    // Verification support
    //
    // Package-private accessors, so a test can name a surface without reaching
    // through the view tree by position. They expose no state that is not
    // already on screen.
    // -----------------------------------------------------------------------

    PropertyInspectorView propertyInspector() {
        return inspector;
    }

    SculptContextView sculptContext() {
        return sculptContext;
    }

    /** The chrome rectangles, in this view's coordinates, that stand between
     *  the user and the model. Empty when chrome is hidden. */
    Rect[] chromeRects() {
        if (uiState.chromeHidden()) {
            return new Rect[0];
        }
        final View[] surfaces = {toolbar, brushControls, toolRailScroll, inspector};
        int count = 0;
        for (View surface : surfaces) {
            if (isOnScreen(surface)) {
                count++;
            }
        }
        final Rect[] rects = new Rect[count];
        int index = 0;
        for (View surface : surfaces) {
            if (!isOnScreen(surface)) {
                continue;
            }
            // Built from the view's own extent rather than its drawing rect,
            // because a scrolled container's drawing rect carries its scroll
            // offset and would misplace the rectangle.
            final Rect rect = new Rect(0, 0, surface.getWidth(), surface.getHeight());
            offsetDescendantRectToMyCoords(surface, rect);
            rects[index++] = rect;
        }
        return rects;
    }

    private boolean isOnScreen(View view) {
        if (view == null || view.getVisibility() != VISIBLE || view.getParent() == null) {
            return false;
        }
        return view.getWidth() > 0 && view.getHeight() > 0;
    }
}
